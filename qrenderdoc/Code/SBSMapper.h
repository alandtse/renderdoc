/*
 * Copyright (C) 2026 Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QPoint>
#include <cstdint>

// Maps pixels between left and right eyes in a side-by-side (SBS) stereo texture.
//
// Phase 1: Simple mirror — assumes the texture is packed as [left | right] horizontally,
// each half occupying texWidth/2 pixels. No projection matrices required.
//
// Phase 2 (future): matrix-based reprojection through world space for geometrically
// accurate cross-eye correspondence when parallax matters.
class SBSMapper
{
public:
  bool enabled = false;

  // Returns 0 for the left eye (x < texWidth/2) or 1 for the right eye (x >= texWidth/2).
  uint32_t eyeIndexForPixel(QPoint px, uint32_t texWidth) const;

  // Returns the corresponding pixel in the other eye's half of the SBS texture.
  // Coordinates are in full-texture space (base mip level, unflipped).
  // Returns {-1, -1} when enabled is false or the input is out of bounds.
  QPoint otherEyePixel(QPoint px, uint32_t texWidth, uint32_t texHeight) const;
};
