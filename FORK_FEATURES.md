# Fork Features

This document tracks features and changes in this fork
([alandtse/renderdoc](https://github.com/alandtse/renderdoc)) that diverge from
upstream ([baldurk/renderdoc](https://github.com/baldurk/renderdoc)).

Keep this file updated when adding, changing, or removing fork-specific features.
Include the branch or commit where the feature landed.

---

## Active Features

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
