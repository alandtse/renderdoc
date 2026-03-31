# Fork Features

This document tracks features and changes in this fork
([alandtse/renderdoc](https://github.com/alandtse/renderdoc)) that diverge from
upstream ([baldurk/renderdoc](https://github.com/baldurk/renderdoc)).

Keep this file updated when adding, changing, or removing fork-specific features.
Include the branch or commit where the feature landed.

---

## Active Features

### VR SBS "Jump to Other Eye" Pixel Navigation
**Branch:** `vr_sbs_eye_mapping`
**Status:** Phase 1 — simple mirror (no matrix reprojection)

In VR captures that render both eyes side-by-side into a single texture (left eye in
`x ∈ [0, W/2)`, right eye in `x ∈ [W/2, W)`), it is useful to jump directly from a
picked pixel in one eye to the corresponding location in the other eye.

**Usage:**
1. Enable the **SBS** toggle button in the Texture Viewer's action toolbar.
2. Right-click a pixel in the SBS texture to pick it. The status bar shows `[L]` or `[R]`
   to indicate which eye the picked pixel belongs to.
3. Click **Other Eye** in the pixel context panel to jump the picked point to the
   corresponding position in the opposite eye's half of the texture.

**New components:**
- `qrenderdoc/Code/SBSMapper` — `SBSMapper` class encapsulating eye-index detection and
  pixel mapping. Phase 1 uses a simple horizontal mirror (no projection matrices needed).
  Designed to accept a Phase 2 matrix-reprojection path for geometrically accurate mapping.

**Known limitations (Phase 1):**
- The mapped pixel is the mirror position, not the world-projected position. For textures
  with dynamic resolution, the mirror is still accurate (both halves share the same scale).
- Full reprojection via `CameraViewProjInverse`/`CameraViewProj` cbuffer matrices is
  planned for Phase 2.

---

### Synchronized Pixel Shader Debugging
**Branch:** `synced_debuggers`
**Status:** In development

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
