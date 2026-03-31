/*
 * Copyright (C) 2026 Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QPoint>
#include <cstdint>

// Raw matrices extracted from a cbuffer for stereo reprojection.
// All float4x4 values are row-major: M[row*4+col] = element (row, col).
struct VRFrameBufferMatrices
{
  float viewProj[2][16];
  float viewProjInverse[2][16];
  bool hasCameraPosAdjust = false;
  float cameraPosAdjust[2][4];
};

// Maps pixels between left and right eyes in a side-by-side (SBS) stereo texture.
//
// Phase 1 (simple mirror): pixel at x maps to x ± texWidth/2. No matrix data required.
//
// Phase 2 (matrix reproject): world-space reprojection via per-eye ViewProj /
//   ViewProjInverse matrices, detected generically from the current draw's shader
//   reflection. Ported from ConvertMonoUVToOtherEye in skyrim-community-shaders VR.hlsli.
class SBSMapper
{
public:
  bool enabled = false;

  // Returns 0 for the left eye (x < texWidth/2) or 1 for the right eye (x >= texWidth/2).
  uint32_t eyeIndexForPixel(QPoint px, uint32_t texWidth) const;

  // Phase 1 — simple horizontal mirror.
  // Returns {-1, -1} when enabled is false or coords are out of bounds.
  QPoint otherEyePixel(QPoint px, uint32_t texWidth, uint32_t texHeight) const;

  // Phase 2 — world-space reprojection.
  // monoUVx/monoUVy: per-eye UV in [0,1] (unflipped, not DR-adjusted).
  // depth: NDC depth in [0,1] from the depth buffer at this pixel.
  // eyeIndex: 0 (left) or 1 (right).
  // Returns true and sets otherMonoUVx/otherMonoUVy on success.
  static bool reproject(float monoUVx, float monoUVy, float depth, uint32_t eyeIndex,
                        const VRFrameBufferMatrices &mats, float &otherMonoUVx, float &otherMonoUVy);
};
