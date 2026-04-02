/*
 * Copyright (C) 2026 Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QString>
#include "Code/Interface/QRDInterface.h"

// Describes the buffer locations of stereo VP matrices discovered from shader reflection.
// All matrix offsets are relative to the start of the constant buffer data
// (indices into the bytebuf returned by GetBufferData).
struct StereoMatrixConfig
{
  bool valid = false;
  ResourceId cbufId;
  uint64_t cbufByteOffset = 0;
  uint32_t viewProjOffset[2] = {};
  uint32_t viewProjInvOffset[2] = {};
  bool hasCameraPosAdjust = false;
  uint32_t cameraPosAdjustOffset[2] = {};
  uint32_t minBytesNeeded = 0;

  // When true the candidate was found by structural (name-independent) detection.
  // Caller must verify VP[0] * VPInv[0] ≈ I via SBSMapper::approxInverse before use.
  bool needsVerification = false;

  // Names of the shader variables that triggered detection (empty for structural candidates).
  QString viewProjVarName;
  QString viewProjInvVarName;
  QString cameraPosVarName;

  // Human-readable description for the settings dialog.
  QString description;
};

// Searches all pixel-shader constant blocks for stereo VP matrix array pairs using
// shader reflection. Returns all valid candidates ordered by confidence; iterate and
// use the first whose reprojection result lands in-bounds.
//
// Name-matched candidates (needsVerification=false) come first. Structural candidates
// (needsVerification=true) follow and must be validated before use.
rdcarray<StereoMatrixConfig> detectAllStereoMatrices(ICaptureContext &ctx);
