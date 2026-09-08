"""Instance Sync - synchronizes the current event and picked pixel location between
exactly two RenderDoc instances.

Intended workflow: capture the same locked-camera scene into two separate .rdc files
(e.g. successive frames, or before/after a change), open one in each RenderDoc window,
enable this extension in both (Tools > Manage Extensions). From then on, changing the
current event or picking a pixel in either window mirrors it in the other, so you can
scrub both captures in lockstep and compare the same pixel frame to frame without
manually re-navigating both windows every time.

Not part of the compiled fork build - see FORK_FEATURES.md for install steps.

Implementation note: RenderDoc's embedded Python distribution does not bundle the
`_socket` extension module (confirmed by testing against a live instance - `socket`,
`ssl`, and `multiprocessing` all fail to import with "No module named '_socket'"), and
`PySide2.QtNetwork` isn't built either, so ordinary TCP/UDP IPC is not available here.
Instead this uses a named shared-memory mapping (`mmap.mmap(-1, size, tagname=...)`),
which Windows resolves by name across processes - whichever instance opens it first
creates it, the other just opens the same one, so there is no host/client negotiation
needed. Both sides poll it on a background thread; this doubles as the only way to
detect a local pixel pick too, since ITextureViewer has no push notification for that
(only the GetPickedLocation() getter). Event changes still get an immediate local push
via ICaptureViewer.OnEventChanged, so only the picked-pixel side is polling-latency
bound (~150ms); event sync itself is effectively instant once written.

This is a 2-node design: exactly two instances are expected to share the same shared
memory segment. A third instance enabling the extension will also attach to the same
segment and will interfere with the pairing - only run this with exactly two instances.

Reads/writes to each shared segment are not protected by a cross-process lock. Given
the tiny payload size and human-paced (or 150ms-polled) update frequency, a torn read
is exceedingly unlikely and self-corrects on the next poll if it ever happens - not
worth the added complexity of a named mutex for what this is used for.

Pixel-distance readout: each side reads back its own picked pixel's RGBA value (via
IReplayController.PickPixel on the texture currently shown in its Texture Viewer,
CompType.Typeless so no reinterpretation is applied) and publishes it alongside the
coordinates. Since the two instances have separate GPU devices/processes, one can't bind
the other's texture directly the way the SBS eye-compare heatmap samples both eyes from
a single device - so this stays a numeric readout (peer's RGBA and the distance to it)
rather than a live GPU-composited overlay like SBS. A full image-wide heatmap across
instances would need a new core API to upload a transferred frame as a sampleable
texture; out of scope here.
"""

import mmap
import os
import struct
import threading
import time

import qrenderdoc as qrd
import renderdoc as rd

# Separate segments per kind of state, so an event update and a pixel update can never
# race against each other via a shared read-modify-write.
SHM_TAG_EVENT = "RenderDocInstanceSync_Event_v1"
# v2: bumped when the pixel payload grew to include the picked color (RGBA), so an
# older-version instance never opens a same-named mapping sized for the old, smaller
# struct.
SHM_TAG_PIXEL = "RenderDocInstanceSync_Pixel_v2"

# seq, origin_pid, event_id
EVENT_FMT = "<iii"
# seq, origin_pid, x, y, r, g, b, a
PIXEL_FMT = "<iiiiffff"

POLL_INTERVAL_SECS = 0.15

# Severity tiers for the delta readout, matching the SBS eye-compare heatmap's
# language (10/255 is that feature's default mismatch threshold - see
# TextureViewer's m_SBSHeatmapThreshold): green below it, red at a clearly
# significant mismatch, yellow in between.
_SEVERITY_GREEN_MAX = 10.0 / 255.0
_SEVERITY_RED_MIN = 40.0 / 255.0
_SEVERITY_COLORS = {
    "green": "#2ecc71",
    "yellow": "#f1c40f",
    "red": "#e74c3c",
}


def _severity_color(dist):
    if dist < _SEVERITY_GREEN_MAX:
        return _SEVERITY_COLORS["green"]
    if dist < _SEVERITY_RED_MIN:
        return _SEVERITY_COLORS["yellow"]
    return _SEVERITY_COLORS["red"]


def _fmt_color(c):
    if c is None:
        return "no pick yet"
    return "RGBA(%.3f, %.3f, %.3f, %.3f)" % c


class InstanceSync(qrd.CaptureViewer):
    # qrd.CaptureViewer is a SWIG director class - it must be subclassed (not just
    # duck-typed) for AddCaptureViewer/RemoveCaptureViewer to accept the instance as a
    # real ICaptureViewer*; a plain object raises a SWIG type error.
    def __init__(self, ctx):
        qrd.CaptureViewer.__init__(self)
        self.ctx = ctx
        self.mqh = ctx.Extensions().GetMiniQtHelper()
        self.pid = os.getpid()

        self._event_shm = mmap.mmap(-1, struct.calcsize(EVENT_FMT), tagname=SHM_TAG_EVENT)
        self._pixel_shm = mmap.mmap(-1, struct.calcsize(PIXEL_FMT), tagname=SHM_TAG_PIXEL)

        self.running = True

        # Last event/pixel this instance itself applied (whether from a local user action
        # or from a remote update) - lets both the OnEventChanged callback and the pixel
        # poll below tell "the peer just told us this" apart from "the user just did this
        # locally", so applying a remote update never gets echoed straight back.
        self._last_event = -1
        self._last_pixel = (-1, -1)
        self._last_event_seq_seen = -1
        self._last_pixel_seq_seen = -1

        # For the pixel-distance readout panel.
        self._last_local_color = None
        self._last_peer_color = None
        self._last_peer_pid = None

        self._toplevel = self.mqh.CreateToplevelWidget("Instance Sync", self._on_panel_closed)
        container = self.mqh.CreateVerticalContainer()
        self.mqh.AddWidget(self._toplevel, container)
        self._label = self.mqh.CreateLabel()
        self.mqh.AddWidget(container, self._label)
        self.mqh.SetWidgetText(self._label, "Instance Sync: waiting for a pixel pick...")
        ctx.AddDockWindow(self._toplevel, qrd.DockReference.NewFloatingArea, None)

        ctx.AddCaptureViewer(self)

        self._poll_thread = threading.Thread(target=self._poll_loop, daemon=True)
        self._poll_thread.start()

    def _on_panel_closed(self, ctx, widget, text):
        self._toplevel = None
        self._label = None

    # -- ICaptureViewer overrides (see qrenderdoc.CaptureViewer) --

    def OnCaptureLoaded(self):
        pass

    def OnCaptureClosed(self):
        pass

    def OnSelectedEventChanged(self, eventId):
        pass

    def OnEventChanged(self, eventId):
        if eventId == self._last_event:
            return
        self._last_event = eventId
        self._write(self._event_shm, EVENT_FMT, eventId)

    # -- shared memory helpers --

    def _write(self, shm, fmt, *values):
        # Bump the sequence number so readers (including ourselves, harmlessly) can tell
        # this write apart from whatever was there before. Sequence numbers start at 1 -
        # a segment nobody has ever written to reads back as all zeros (seq 0), which
        # readers treat as "no message yet" rather than a real update from pid 0.
        shm.seek(0)
        prev = shm.read(struct.calcsize(fmt))
        prev_seq = struct.unpack(fmt, prev)[0] if len(prev) == struct.calcsize(fmt) else 0
        shm.seek(0)
        shm.write(struct.pack(fmt, prev_seq + 1, self.pid, *values))
        shm.flush()
        return prev_seq + 1

    def _pick_color(self, tex, xy, sub):
        # Reads back the RGBA value at the given pixel of the texture currently shown in
        # the Texture Viewer. CompType.Typeless means no reinterpretation is applied - the
        # value is read as whatever the texture's native format is.
        if tex is None or sub is None:
            return (0.0, 0.0, 0.0, 0.0)
        out = {}

        def cb(controller):
            val = controller.PickPixel(tex, xy[0], xy[1], sub, rd.CompType.Typeless)
            out["v"] = tuple(val.floatValue)

        try:
            self.ctx.Replay().BlockInvoke(cb)
        except Exception:
            pass
        return out.get("v", (0.0, 0.0, 0.0, 0.0))

    def _update_label(self):
        # Rich-text HTML so the delta line can be color-coded by severity, the
        # same green/yellow/red language the SBS eye-compare heatmap uses to
        # flag mismatches (see _SEVERITY_GREEN_MAX/_SEVERITY_RED_MIN above) -
        # a plain monochrome number doesn't make a big discrepancy jump out the
        # way the heatmap's red tint does.
        def do_update():
            if self._label is None:
                return
            lc = self._last_local_color
            pc = self._last_peer_color
            lines = ["Local &nbsp;%s: %s" % (self._last_pixel, _fmt_color(lc))]
            if pc is not None:
                lines.append("Peer &nbsp;&nbsp;(pid %s): %s" % (self._last_peer_pid, _fmt_color(pc)))
                if lc is not None:
                    diffs = [abs(a - b) for a, b in zip(lc, pc)]
                    dist = sum(d * d for d in diffs[:3]) ** 0.5
                    color = _severity_color(dist)
                    lines.append(
                        '<span style="color:%s; font-weight:bold;">'
                        "Delta RGB dist: %.4f &nbsp;(max channel: %.4f)</span>"
                        % (color, dist, max(diffs[:3]))
                    )
            else:
                lines.append("Peer: no pixel received yet")
            self.mqh.SetWidgetText(self._label, "<br>".join(lines))

        try:
            self.mqh.InvokeOntoUIThread(do_update)
        except Exception:
            pass

    def _poll_loop(self):
        last_local_pixel = (-1, -1)
        # The xy this instance has already published its own color for - separate from
        # _last_pixel (which also gets set by applying a *remote* move) so that after
        # applying a peer's move we still publish our own color back once, instead of
        # treating it as an echo of a coordinate we already know about.
        color_published_for = (-1, -1)

        while self.running:
            time.sleep(POLL_INTERVAL_SECS)

            # Remote updates are checked *before* this instance publishes its own local
            # state below. Both directions live in the same shared segment, last-write-
            # wins - if local-publish ran first, this instance's own routine republish of
            # its current pixel (see the "echo our own color back" comment below) could
            # overwrite a peer message that arrived in between, before this loop ever got
            # a chance to read it, permanently losing that update.

            # 1. Check for a remote event update.
            self._event_shm.seek(0)
            edata = self._event_shm.read(struct.calcsize(EVENT_FMT))
            if len(edata) == struct.calcsize(EVENT_FMT):
                eseq, eorigin, eid = struct.unpack(EVENT_FMT, edata)
                if eseq > 0 and eseq > self._last_event_seq_seen:
                    self._last_event_seq_seen = eseq
                    if eorigin != self.pid and eid != self._last_event:
                        self._last_event = eid

                        def apply_event(eid=eid):
                            if self.ctx.IsCaptureLoaded():
                                self.ctx.SetEventID([self], eid, eid)

                        self.mqh.InvokeOntoUIThread(apply_event)

            # 2. Check for a remote pixel update. Always record the peer's color/pixel
            #    for the readout, but only re-navigate if the coordinates actually moved
            #    (a peer echoing our own color back at the same pixel shouldn't re-trigger
            #    GotoLocation).
            self._pixel_shm.seek(0)
            pdata = self._pixel_shm.read(struct.calcsize(PIXEL_FMT))
            if len(pdata) == struct.calcsize(PIXEL_FMT):
                pseq, porigin, px, py, pr, pg, pb, pa = struct.unpack(PIXEL_FMT, pdata)
                if pseq > 0 and pseq > self._last_pixel_seq_seen:
                    self._last_pixel_seq_seen = pseq
                    if porigin != self.pid:
                        self._last_peer_color = (pr, pg, pb, pa)
                        self._last_peer_pid = porigin
                        self._update_label()
                        if (px, py) != self._last_pixel:
                            self._last_pixel = (px, py)

                            def apply_pixel(px=px, py=py):
                                if self.ctx.HasTextureViewer():
                                    self.ctx.GetTextureViewer().GotoLocation(px, py)

                            self.mqh.InvokeOntoUIThread(apply_pixel)

            # 3. Detect a local pixel pick (no push notification exists for this),
            #    read back its color, and publish both - including echoing our own color
            #    back after applying a peer's move above, so the peer's readout gets our
            #    side of the comparison too.
            picked = {}
            # InvokeOntoUIThread queues the callback and returns immediately - it does not
            # block until the callback has actually run - so without this event, the read
            # below would race the callback and see stale/empty data almost every time.
            done = threading.Event()

            def read_picked():
                if self.ctx.HasTextureViewer():
                    tv = self.ctx.GetTextureViewer()
                    picked["xy"] = tuple(tv.GetPickedLocation())
                    picked["tex"] = tv.GetCurrentResource()
                    picked["sub"] = tv.GetSelectedSubresource()
                done.set()

            try:
                self.mqh.InvokeOntoUIThread(read_picked)
                done.wait(POLL_INTERVAL_SECS)
            except Exception:
                pass

            xy = picked.get("xy")
            if xy is not None and xy != last_local_pixel:
                last_local_pixel = xy
                if xy[0] >= 0 and xy[1] >= 0 and xy != color_published_for:
                    color_published_for = xy
                    color = self._pick_color(picked.get("tex"), xy, picked.get("sub"))
                    self._last_pixel = xy
                    self._last_local_color = color
                    self._write(self._pixel_shm, PIXEL_FMT, xy[0], xy[1], *color)
                    self._update_label()

    def shutdown(self):
        self.running = False
        for shm in (self._event_shm, self._pixel_shm):
            try:
                shm.close()
            except Exception:
                pass


_sync = None


def register(version, ctx):
    global _sync
    _sync = InstanceSync(ctx)
    print("Instance Sync loaded (pid %d)" % _sync.pid)


def unregister():
    global _sync
    if _sync is not None:
        _sync.ctx.RemoveCaptureViewer(_sync)
        _sync.shutdown()
        _sync = None
