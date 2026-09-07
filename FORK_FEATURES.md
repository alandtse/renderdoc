# Fork Features

This document tracks features and changes in this fork
([alandtse/renderdoc](https://github.com/alandtse/renderdoc)) that diverge from
upstream ([baldurk/renderdoc](https://github.com/baldurk/renderdoc)).

Keep this file updated when adding, changing, or removing fork-specific features.
Include the branch or commit where the feature landed.

---

## Active Features

### Callstack: Function Offset and RVA in API Inspector

Resolved callstack frames in the API Inspector now include the intra-function byte
offset and a module-relative RVA (or absolute VA fallback), making frames directly
usable in RE tools such as Ghidra without manual arithmetic.

**Display format:**
- PDB-resolved: `MyFunc+0x18 line 42  [RVA:0x12ab34]`
- Unresolved (module only): `d3d11.dll+0x0012ab34` (unchanged — already RVA-relative)
- No module info: `0x00007fff1234abcd  [VA:0x00007fff1234abcd]`

The RVA is `addr − module_load_base`; in Ghidra (which loads at the PE's preferred
`ImageBase`) the Go To address is `ImageBase + RVA`.

**Callstack panel UX:**
- **Ctrl+C** on a selected frame copies the `0x…` address suffix (RVA when available,
  VA otherwise); falls back to the full frame text when no suffix is present.
- **Right-click** → *Copy frame* / *Copy all frames* / *Copy RVA* (or *Copy address*).

**Changes:**
- `Callstack::AddressDetails` gains `addr` and `moduleBase` fields (backend only; flows
  into `ICaptureAccess::GetResolve()` strings already accessible from Python).
- `DIA2::GetAddr` queries `IDiaSymbol::get_virtualAddress` to compute the intra-function
  offset; `Win32CallstackResolver::GetAddr` appends it and stores the module base.
- Linux resolver stores `addr` and `moduleBase` from the matched module entry.
- `GL_Callstacks` test updated to parse just the numeric portion of the line-number field.

---

### VR SBS "Jump to Other Eye" Pixel Navigation

In VR captures that render both eyes side-by-side into a single texture (left eye in
`x ∈ [0, W/2)`, right eye in `x ∈ [W/2, W)`), it is useful to jump directly from a
picked pixel in one eye to the corresponding location in the other eye.

**Usage:**
1. Enable the **SBS** toggle button in the Texture Viewer's action toolbar (auto-enables
   when stereo matrices are detected in the current draw's pixel shader).
2. Right-click a pixel in the SBS texture to pick it. The status bar shows `[L]` or `[R]`
   to indicate which eye the picked pixel belongs to. The comparison panel updates
   automatically showing both eye values and per-channel deltas.
3. Click **Other Eye** in the pixel context panel to navigate to the corresponding
   position in the opposite eye. The button label shows the computed destination
   coordinates for the current pick, updated live as you move the pick around.
4. Enable **Heatmap** (next to the SBS toggle) to flag, across the whole SBS texture at
   once, every pixel where left/right eye reprojection disagrees — useful for spotting a
   discrepancy visually before hunting for it pixel by pixel. A translucent red tint marks
   a colour mismatch above the heatmap match threshold (the underlying image stays visible
   underneath — this highlights, it doesn't replace, the original pixels); translucent blue
   marks a pixel where reprojection is unreliable (see round-trip verification below).
   Rendered by a generated custom shader directly into the displayed image (the same
   mechanism as RenderDoc's built-in debug overlays), so it composites correctly with the
   real render output instead of being layered on top as a separate UI element. Recompiled
   on demand (toggling it on, changing SBS matrix/dynres settings, or navigating to a
   different event) — the match threshold, channel selection, and round-trip settings
   (right-click the SBS button) also recompile the shader, debounced so dragging a slider
   doesn't recompile on every intermediate tick. Defaults to 10/255 on "Any channel" (max of
   R/G/B), a rough "noticeable on casual viewing" cutoff that ignores per-eye
   dithering/quantization noise. The channel selector can instead isolate Red/Green/Blue
   (catches a post-effect, LUT, or fog tint applied to only one eye, which often shows as a
   hue shift more than a brightness change) or Luma (perceptual brightness difference only,
   ignoring pure colour/hue shifts, to focus on geometry or reprojection divergence).
   **D3D11 captures only for now** — the toggle is disabled with an explanatory tooltip on
   other APIs.

**How it works:**
When the pixel shader at the current EID has a constant buffer containing `float4x4[>=2]`
arrays named like `ViewProj` and `ViewProjInverse` (detected via shader reflection),
the jump uses world-space reprojection for a geometrically accurate result:
unproject the source pixel to world space using the source eye's ViewProjInverse,
then reproject into the other eye via the other eye's ViewProj.
Falls back to a simple horizontal mirror when no stereo matrices are found,
the depth buffer is unavailable, or the reprojected UV lands outside [0,1].

Detection uses `ShaderConstant::byteOffset` and `ShaderConstantType::arrayByteStride`
from `ShaderReflection` — no engine-specific hardcoded offsets. A `float4[>=2]` array
whose name suggests a per-eye camera position (`CameraPosAdjust`, `EyePos`, etc.) is
also extracted when present and used to correct IPD offset.

The mismatch heatmap ports the same reprojection math to HLSL and runs it as a generated
custom shader (`IReplayController::BuildCustomShader`, the same mechanism used for
user-authored visualisation shaders), resolving a single matrix candidate for the whole
image up front (rather than retrying candidates per pixel like the single-point jump) and
embedding it, along with the current threshold/channel/round-trip settings, as literal
shader constants. The shader samples the SBS texture at both the source and reprojected
UV directly (both eyes are already in the one texture being displayed) and blends the
mismatch/unreliable tint over the real sampled colour, so it is a true overlay rather than
a separate compositing layer. Depth comes from a second texture bound alongside the
display texture via a new `TextureDisplay::customShaderDepthId` field (D3D11-only for
now — `renderdoc/driver/d3d11/d3d11_rendertexture.cpp` binds it at a spare SRV slot using
the same `GetShaderDetails` helper the overlay/pixel-history paths already use for
format-correct depth views). Optional round-trip (back-projection) verification
reprojects each match back to its source eye and flags pixels whose round-trip drift
exceeds a pixel threshold as unreliable rather than mismatched, to avoid flagging
occlusion/disocclusion edges as false positives.

An earlier version of this feature computed the diff on the CPU and displayed it via a
semi-transparent Qt widget layered over the texture view. That doesn't work: the texture
view paints directly to the screen outside Qt's compositing pipeline
(`Qt::WA_PaintOnScreen`), so a translucent sibling widget blends against whatever Qt
itself last painted there (effectively blank) rather than the live rendered image — it
looked like the heatmap was replacing the picture instead of highlighting it. Rendering
the heatmap as part of the actual GPU output sidesteps this entirely.

**New components:**
- `qrenderdoc/Code/SBSMapper` — eye-index detection, simple mirror fallback, and static
  `reproject()` for world-space reprojection via `VRFrameBufferMatrices`.
- `TextureDisplay::customShaderDepthId` (`renderdoc/api/replay/control_types.h`) — optional
  second resource bound alongside a custom shader's primary display texture. Local-only
  (never serialized over remote replay), so no API/wire-protocol version bump was needed.

**Known limitations:**
- Detection relies on variable names containing `viewproj`/`view_proj` (case-insensitive).
  Engines that use completely opaque names (e.g. `m0`, `data[0]`) fall back to the
  simple horizontal mirror.
- Dynamic resolution is not accounted for in the UV→pixel conversion; pixels in the
  unrendered border region fall back to the simple mirror.
- The heatmap is D3D11-only for now — the toggle is disabled on other APIs. Extending it to
  D3D12/OpenGL/Vulkan means implementing the same second-resource binding in each backend's
  custom-shader resource setup (`d3d12_rendertexture.cpp`, `gl_rendertexture.cpp`,
  `vk_rendertexture.cpp`), not a fundamental blocker, just not done yet.
- The heatmap requires a bound depth target and at least one detected (or manually entered)
  stereo matrix; without either it leaves the display unchanged rather than guessing.

---

### Synchronized Pixel Shader Debugging

Steps multiple pixel shader debugger instances in lockstep, allowing divergences
between shaders to be identified interactively. Useful for VR cross-eye divergence
checks and for comparing why adjacent pixels produce different results.

New components:
- `PixelDebugSyncManager` — singleton coordinating step synchronization across
  `ShaderViewer` instances
- `PixelDebugSyncPanel` — UI panel showing per-step variable diffs between synced
  debuggers
- `IPixelDebugSyncPanel` / `IPixelDebugSyncManager` interfaces in `QRDInterface.h`

Python/agent API (`qrenderdoc` module, via `ctx.GetPixelDebugSyncManager()`):
- `SyncGroupInfo` — group metadata (id, viewer count, threshold, break settings)
- `SyncVarDiff` — per-variable divergence data (values per viewer, presence flags,
  per-component divergence mask)
- `IPixelDebugSyncManager.GetAllGroupIds()` — list active group IDs
- `IPixelDebugSyncManager.GetGroupInfo(groupId)` — query group metadata
- `IPixelDebugSyncManager.ComputeDiffs(groupId)` — get full variable diff list
- `IPixelDebugSyncManager.HasBranchDivergence(groupId)` — detect control-flow split
- `IPixelDebugSyncManager.SetThreshold/SetIgnoreIntDivergence` — configure group
- `IPixelDebugSyncManager.FormatVarValue/VarsAreDivergent` — formatting utilities

---

### PDB Symbol Manager

Exposes why each DLL's PDB symbols succeeded or failed to load, and allows
force-loading a PDB after the initial symbol resolution pass.

**Usage:**
1. Open a capture that has callstacks (the **Tools → Resolve Symbols** menu
   item must be enabled).
2. Run **Tools → Resolve Symbols** as usual. When it finishes, the
   **Tools → Symbol Manager…** item becomes active.
3. The Symbol Manager dialog shows every module seen in the capture with
   columns: *Module*, *Status*, *PDB Path*, and *Details*.
4. Rows are colour-coded: green = Loaded, orange-red = Not Found,
   dark red = Failed, grey = Ignored/Skipped.
5. Select any non-loaded row and click **Load PDB…** to browse for a `.pdb`
   file. The PDB is loaded immediately (GUID/age validation is bypassed for
   force loads), the table refreshes, and the API Inspector re-resolves its
   displayed callstack.
6. **Refresh** re-queries the resolver (useful if symbols were loaded via
   other means while the dialog is open).

**UI features:**
- **Tooltips**: hovering any cell shows the full status reason string.
- **Sorting**: click any column header to sort ascending/descending.
- **Filter**: a search box above the table live-filters rows by any column text
  (case-insensitive).
- **Remove from Ignore List**: enabled when an *Ignored* row is selected.
  Removes the module from the persistent ignore list, marks it *Not Found*,
  and saves the updated list to config. The module can then be loaded via
  **Load PDB…**.
- **Force Loaded** status (blue): modules loaded via **Load PDB…** show a
  distinct *Force Loaded* status so it is clear GUID/age validation was
  bypassed.

**New/modified components:**
- `renderdoc/api/replay/callstack_types.h` — new public header defining
  `PDBStatus` enum (including `ForceLoaded` value) and `ModuleStatus` struct.
- `renderdoc/os/os_specific.h` — `Callstack::StackResolver` gains virtual
  `GetModuleStatuses()`, `ForceLoadPDB()`, `AddIgnore()`, and `RemoveIgnore()`
  with default no-op implementations.
- `renderdoc/os/win32/win32_callstack.cpp` — `Win32CallstackResolver` tracks
  `pdbStatus`/`pdbPath`/`statusReason` per module; implements all four virtual
  methods. `ForceLoadPDB` sets `ForceLoaded` status and removes the module from
  the ignore list if it was previously ignored. `AddIgnore`/`RemoveIgnore`
  persist the ignore list via `PersistIgnoreList()`.
- `renderdoc/api/replay/renderdoc_replay.h` — `ICaptureAccess` exposes
  `GetModuleStatuses()`, `ForceLoadPDB()`, `AddIgnore()`, and `RemoveIgnore()`.
- `renderdoc/replay/capture_file.cpp` — delegates all four new methods to the
  resolver.
- `renderdoc/core/remote_server.h` — empty stubs (remote PDB management is
  not supported).
- `qrenderdoc/Windows/Dialogs/SymbolManagerDialog` — new Qt dialog.
- `qrenderdoc/Windows/MainWindow` — **Tools → Symbol Manager…** menu action,
  enabled after Resolve Symbols completes.

---

### Instance Sync (Python extension, not compiled into the build)

Synchronizes the current event and picked pixel location between exactly two RenderDoc
instances, so a locked-camera scene captured into two separate `.rdc` files (successive
frames, before/after a change, etc.) can be scrubbed and pixel-inspected in lockstep
instead of manually re-navigating both windows for every comparison.

**This is a RenderDoc Python extension, not something compiled into `renderdoc.dll`/
`qrenderdoc.exe`.** RenderDoc only discovers extensions from the user's own config
folder (`%APPDATA%\qrenderdoc\extensions` on Windows), never from the repo or install
directory, so it can't be "built in" the normal sense — the source lives in this repo
for version control, but each user installs it themselves.

**Install:**
1. Copy `util/qrenderdoc-extensions/instance_sync/` into
   `%APPDATA%\qrenderdoc\extensions\instance_sync\` (Linux:
   `~/.local/share/qrenderdoc/extensions/instance_sync/`).
2. In **both** RenderDoc windows: **Tools → Manage Extensions**, tick **Enabled** for
   "Instance Sync", restart if prompted.

**Usage:** open one capture per window. Both instances attach to the same named
shared-memory mapping (no host/client roles, no configuration) — whichever opens it
first creates it, the other just opens the existing one. From then on, changing the
current event (Event Browser navigation, step next/prev, Find) or picking a pixel in the
Texture Viewer in either window mirrors it in the other.

**How it works:**
- Event sync is push-based on the local side: the extension registers as an
  `ICaptureViewer` (`ICaptureContext.AddCaptureViewer`) to receive `OnEventChanged`, and
  writes the new event ID into a shared-memory segment
  (`mmap.mmap(-1, size, tagname=...)`). Applying an incoming update calls
  `ICaptureContext.SetEventID` excluding itself from that call, so applying a remote
  event doesn't re-trigger a write back to the peer.
- Pixel sync has no push notification in the API (`ITextureViewer` only exposes a
  `GetPickedLocation()` getter) — the extension polls it on a background thread (~150ms),
  same thread that also polls the shared-memory segments for updates from the peer, and
  calls `ITextureViewer.GotoLocation()` when an incoming pixel differs from the last one
  applied.
- The embedded RenderDoc Python interpreter does not bundle the `_socket` extension
  module in this build (`socket`/`ssl`/`multiprocessing` all fail to import — confirmed
  live; `PySide2.QtNetwork` isn't built either), so ordinary TCP/UDP IPC isn't available.
  A named shared-memory mapping is used instead — it's resolved by name across processes
  by the OS, so there's no listen/connect/bind step at all. Each shared segment carries a
  sequence number and the writer's PID so a reader can tell "the peer just wrote this"
  apart from its own most recent write. All calls back into RenderDoc are marshaled onto
  the Qt UI thread via `MiniQtHelper.InvokeOntoUIThread`.

**Known limitations:**
- Two-instance design only: exactly two instances are expected to share the same shared
  memory segments. A third instance enabling the extension will also attach to the same
  segments and will interfere with the pairing - only run this with exactly two
  instances.
- Pixel sync latency is bounded by the ~150ms poll interval (event sync is pushed
  immediately but still observed by the peer's poll loop, so it shares the same bound).
- Shared-memory reads/writes aren't protected by a cross-process lock; given the tiny
  payload and human/150ms-poll-paced update rate a torn read is exceedingly unlikely and
  self-corrects on the next poll if it ever happens.
- No UI indicator of sync/connection state yet - check the Python scripting console's
  output for the "Instance Sync loaded (pid ...)" message logged at startup.

**New components:**
- `util/qrenderdoc-extensions/instance_sync/` — `extension.json` manifest and
  `__init__.py` implementation.

---

### Shader Source Names as First-Class Support

Exposes full shader source filenames natively to RenderDoc's UI components, allowing easy filtering and discovery of API events by matching real source code filenames instead of just numerical IDs or pipeline states.

**Features:**
- Event Browser: Use `$shader(filename)` or `$shader("partial match")` filters to instantly find all draw/dispatch calls using a specific shader.
- Resource Inspector: Filter shader objects directly by their debug info source name.

**Changes:**
- Extends the D3D11 driver (`WrappedID3D11DeviceContext::AddUsage`) to capture shader device child bindings into the `m_ResourceUses` array at capture chunk load time.
- Modifies `ResourceUsage` enums and UI formatters (`QRDUtils`) to gracefully handle explicit shader stages (e.g. `VS_Shader`, `PS_Shader`).
- `CaptureContext` caches shader filenames asynchronously via ReplayController debug info parsing upon capture load.

## Policy Differences from Upstream

- LLM-assisted development is permitted (see [CONTRIBUTING.md](docs/CONTRIBUTING.md))
- Fork-specific contributions are GPL-3.0-or-later (see [COPYING](COPYING))
- Contributors pre-authorize copyright assignment to Baldur Karlsson for upstream
  reintegration (see [CONTRIBUTOR_LICENSE_AGREEMENT.md](CONTRIBUTOR_LICENSE_AGREEMENT.md))
