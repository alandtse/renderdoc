# Fork Features

This document tracks features and changes in this fork
([alandtse/renderdoc](https://github.com/alandtse/renderdoc)) that diverge from
upstream ([baldurk/renderdoc](https://github.com/baldurk/renderdoc)).

Keep this file updated when adding, changing, or removing fork-specific features.
Include the branch or commit where the feature landed.

---

## Active Features

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
   position in the opposite eye.

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

**New components:**
- `qrenderdoc/Code/SBSMapper` — eye-index detection, simple mirror fallback, and static
  `reproject()` for world-space reprojection via `VRFrameBufferMatrices`.

**Known limitations:**
- Detection relies on variable names containing `viewproj`/`view_proj` (case-insensitive).
  Engines that use completely opaque names (e.g. `m0`, `data[0]`) fall back to the
  simple horizontal mirror.
- Dynamic resolution is not accounted for in the UV→pixel conversion; pixels in the
  unrendered border region fall back to the simple mirror.

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

## Policy Differences from Upstream

- LLM-assisted development is permitted (see [CONTRIBUTING.md](docs/CONTRIBUTING.md))
- Fork-specific contributions are GPL-3.0-or-later (see [COPYING](COPYING))
- Contributors pre-authorize copyright assignment to Baldur Karlsson for upstream
  reintegration (see [CONTRIBUTOR_LICENSE_AGREEMENT.md](CONTRIBUTOR_LICENSE_AGREEMENT.md))
