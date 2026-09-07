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
"""

import mmap
import os
import struct
import threading
import time

import qrenderdoc as qrd

# Separate segments per kind of state, so an event update and a pixel update can never
# race against each other via a shared read-modify-write.
SHM_TAG_EVENT = "RenderDocInstanceSync_Event_v1"
SHM_TAG_PIXEL = "RenderDocInstanceSync_Pixel_v1"

# seq, origin_pid, event_id
EVENT_FMT = "<iii"
# seq, origin_pid, x, y
PIXEL_FMT = "<iiii"

POLL_INTERVAL_SECS = 0.15


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

        ctx.AddCaptureViewer(self)

        self._poll_thread = threading.Thread(target=self._poll_loop, daemon=True)
        self._poll_thread.start()

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
        # this write apart from whatever was there before.
        shm.seek(0)
        prev = shm.read(struct.calcsize(fmt))
        prev_seq = struct.unpack(fmt, prev)[0] if len(prev) == struct.calcsize(fmt) else 0
        shm.seek(0)
        shm.write(struct.pack(fmt, prev_seq + 1, self.pid, *values))
        shm.flush()
        return prev_seq + 1

    def _poll_loop(self):
        last_local_pixel = (-1, -1)
        while self.running:
            time.sleep(POLL_INTERVAL_SECS)

            # 1. Detect a local pixel pick (no push notification exists for this) and
            #    publish it.
            picked = {}
            # InvokeOntoUIThread queues the callback and returns immediately - it does not
            # block until the callback has actually run - so without this event, the read
            # below would race the callback and see stale/empty data almost every time.
            done = threading.Event()

            def read_picked():
                if self.ctx.HasTextureViewer():
                    picked["xy"] = tuple(self.ctx.GetTextureViewer().GetPickedLocation())
                done.set()

            try:
                self.mqh.InvokeOntoUIThread(read_picked)
                done.wait(POLL_INTERVAL_SECS)
            except Exception:
                pass

            xy = picked.get("xy")
            if xy is not None and xy != last_local_pixel:
                last_local_pixel = xy
                if xy != self._last_pixel and xy[0] >= 0 and xy[1] >= 0:
                    self._last_pixel = xy
                    self._write(self._pixel_shm, PIXEL_FMT, xy[0], xy[1])

            # 2. Check for a remote event update.
            self._event_shm.seek(0)
            edata = self._event_shm.read(struct.calcsize(EVENT_FMT))
            if len(edata) == struct.calcsize(EVENT_FMT):
                eseq, eorigin, eid = struct.unpack(EVENT_FMT, edata)
                if eseq > self._last_event_seq_seen:
                    self._last_event_seq_seen = eseq
                    if eorigin != self.pid and eid != self._last_event:
                        self._last_event = eid

                        def apply_event(eid=eid):
                            if self.ctx.IsCaptureLoaded():
                                self.ctx.SetEventID([self], eid, eid)

                        self.mqh.InvokeOntoUIThread(apply_event)

            # 3. Check for a remote pixel update.
            self._pixel_shm.seek(0)
            pdata = self._pixel_shm.read(struct.calcsize(PIXEL_FMT))
            if len(pdata) == struct.calcsize(PIXEL_FMT):
                pseq, porigin, px, py = struct.unpack(PIXEL_FMT, pdata)
                if pseq > self._last_pixel_seq_seen:
                    self._last_pixel_seq_seen = pseq
                    if porigin != self.pid and (px, py) != self._last_pixel:
                        self._last_pixel = (px, py)

                        def apply_pixel(px=px, py=py):
                            if self.ctx.HasTextureViewer():
                                self.ctx.GetTextureViewer().GotoLocation(px, py)

                        self.mqh.InvokeOntoUIThread(apply_pixel)

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
