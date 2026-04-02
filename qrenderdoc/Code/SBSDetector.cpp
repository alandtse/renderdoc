/*
 * Copyright (C) 2026 Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "SBSDetector.h"

#include <QtGlobal>

// Classifies a shader variable name for stereo matrix detection.
//
// Returns:
//   1 — ViewProj (world → clip): name suggests a combined view-projection matrix
//   2 — ViewProjInverse (clip → world): name suggests the inverse
//   3 — CameraPosAdjust: per-eye world-space camera position offset
//   0 — unrecognised
//
// Matching is case-insensitive. Both combined ("worldtoclip") and
// underscore-separated ("world_to_clip") forms are accepted.
static int classifyStereoVarName(const rdcstr &name)
{
  QString n = QString::fromUtf8(name.c_str(), (int)name.size()).toLower();

  // World → clip (ViewProj direction).
  // Covers: ViewProj, ViewProjection, WorldToClip, WorldToProj, g_matWorldToProj, etc.
  bool isWorldToClip = n.contains(lit("viewproj")) || n.contains(lit("view_proj")) ||
                       n.contains(lit("worldtoclip")) || n.contains(lit("world_to_clip")) ||
                       n.contains(lit("worldtoproj")) || n.contains(lit("world_to_proj"));

  // Explicit clip → world names — classified as inverse regardless of "inv" suffix.
  // Covers: ClipToWorld, WorldFromClip, ProjToWorld, ProjectionToWorld, etc.
  bool isClipToWorld = n.contains(lit("cliptoworld")) || n.contains(lit("clip_to_world")) ||
                       n.contains(lit("worldfromclip")) || n.contains(lit("world_from_clip")) ||
                       n.contains(lit("projtoworld")) || n.contains(lit("proj_to_world")) ||
                       n.contains(lit("projectiontoworld")) ||
                       n.contains(lit("projection_to_world"));

  bool hasInv = n.contains(lit("inv")) || n.contains(lit("inverse"));

  if(isClipToWorld)
    return 2;
  if(isWorldToClip && hasInv)
    return 2;
  if(isWorldToClip)
    return 1;

  // Per-eye world-space camera position (optional, used for IPD correction).
  if(n.contains(lit("camerapos")) || n.contains(lit("eyepos")) || n.contains(lit("eyeoffset")) ||
     n.contains(lit("posadjust")) || n.contains(lit("eyeadjust")) || n.contains(lit("hmdpos")) ||
     n.contains(lit("eyeorigin")))
    return 3;

  return 0;
}

rdcarray<StereoMatrixConfig> detectAllStereoMatrices(ICaptureContext &ctx)
{
  rdcarray<StereoMatrixConfig> results;
  const ShaderReflection *refl = ctx.CurPipelineState().GetShaderReflection(ShaderStage::Pixel);
  if(!refl)
    return results;

  for(int bi = 0; bi < (int)refl->constantBlocks.size(); bi++)
  {
    const ConstantBlock &block = refl->constantBlocks[bi];

    // --- Pass 1: name-based detection ---
    // First-match-wins prevents a later array (e.g. CameraPreviousViewProj) from
    // overwriting a correctly identified earlier one (e.g. CameraViewProj).
    bool foundVP = false, foundVPInv = false;
    StereoMatrixConfig named;

    // Structural fallback: collect all float4x4[>=2] arrays regardless of name.
    rdcarray<const ShaderConstant *> mat4Arrays;

    for(int vi = 0; vi < (int)block.variables.size(); vi++)
    {
      const ShaderConstant &var = block.variables[vi];
      const ShaderConstantType &t = var.type;

      if(t.elements >= 2 && t.rows == 4 && t.columns == 4 && t.baseType == VarType::Float)
      {
        mat4Arrays.push_back(&var);

        int cls = classifyStereoVarName(var.name);
        uint32_t stride = (t.arrayByteStride > 0) ? t.arrayByteStride : 64u;

        if(cls == 1 && !foundVP)
        {
          named.viewProjOffset[0] = var.byteOffset;
          named.viewProjOffset[1] = var.byteOffset + stride;
          named.viewProjVarName = QString::fromUtf8(var.name.c_str(), (int)var.name.size());
          foundVP = true;
        }
        else if(cls == 2 && !foundVPInv)
        {
          named.viewProjInvOffset[0] = var.byteOffset;
          named.viewProjInvOffset[1] = var.byteOffset + stride;
          named.viewProjInvVarName = QString::fromUtf8(var.name.c_str(), (int)var.name.size());
          foundVPInv = true;
        }
      }
      // float4[>=2] — per-eye world-space camera position offset (optional).
      else if(t.elements >= 2 && t.rows == 1 && t.columns == 4 && t.baseType == VarType::Float &&
              classifyStereoVarName(var.name) == 3)
      {
        uint32_t stride = (t.arrayByteStride > 0) ? t.arrayByteStride : 16u;
        named.cameraPosAdjustOffset[0] = var.byteOffset;
        named.cameraPosAdjustOffset[1] = var.byteOffset + stride;
        named.cameraPosVarName = QString::fromUtf8(var.name.c_str(), (int)var.name.size());
        named.hasCameraPosAdjust = true;
      }
    }

    // Helper: fill the cbuffer location fields and push a candidate.
    auto pushCandidate = [&](StereoMatrixConfig &cfg) {
      // Use fixedBindNumber (shader binding slot), not bi (reflection array index).
      UsedDescriptor cbufDesc =
          ctx.CurPipelineState().GetConstantBlock(ShaderStage::Pixel, block.fixedBindNumber, 0);
      if(cbufDesc.descriptor.resource == ResourceId())
        return;
      cfg.cbufId = cbufDesc.descriptor.resource;
      cfg.cbufByteOffset = cbufDesc.descriptor.byteOffset;
      uint32_t maxEnd = qMax(cfg.viewProjOffset[1], cfg.viewProjInvOffset[1]) + 64u;
      if(cfg.hasCameraPosAdjust)
        maxEnd = qMax(maxEnd, cfg.cameraPosAdjustOffset[1] + 16u);
      cfg.minBytesNeeded = maxEnd;
      cfg.valid = true;
      cfg.description = QFormatStr("b%1 (%2)%3%4")
                            .arg(block.fixedBindNumber)
                            .arg(QString::fromUtf8(block.name.c_str(), (int)block.name.size()))
                            .arg(cfg.hasCameraPosAdjust ? lit(" + CamPos") : QString())
                            .arg(cfg.needsVerification ? lit(" [unverified]") : QString());
      results.push_back(cfg);
    };

    if(foundVP && foundVPInv)
    {
      pushCandidate(named);
    }
    else if(mat4Arrays.size() >= 2)
    {
      // --- Pass 2: structural fallback ---
      // No name match but there are 2+ float4x4[>=2] arrays. Try the first pair
      // as a potential VP/VPInv candidate. The reprojection code will verify the
      // inverse relationship before using this candidate.
      const ShaderConstant &a0 = *mat4Arrays[0];
      const ShaderConstant &a1 = *mat4Arrays[1];
      uint32_t s0 = (a0.type.arrayByteStride > 0) ? a0.type.arrayByteStride : 64u;
      uint32_t s1 = (a1.type.arrayByteStride > 0) ? a1.type.arrayByteStride : 64u;

      StereoMatrixConfig structural;
      structural.viewProjOffset[0] = a0.byteOffset;
      structural.viewProjOffset[1] = a0.byteOffset + s0;
      structural.viewProjInvOffset[0] = a1.byteOffset;
      structural.viewProjInvOffset[1] = a1.byteOffset + s1;
      structural.needsVerification = true;
      pushCandidate(structural);

      // Also try the reverse ordering in case the arrays are listed VPInv, VP.
      StereoMatrixConfig structural2;
      structural2.viewProjOffset[0] = a1.byteOffset;
      structural2.viewProjOffset[1] = a1.byteOffset + s1;
      structural2.viewProjInvOffset[0] = a0.byteOffset;
      structural2.viewProjInvOffset[1] = a0.byteOffset + s0;
      structural2.needsVerification = true;
      pushCandidate(structural2);
    }
  }
  return results;
}
