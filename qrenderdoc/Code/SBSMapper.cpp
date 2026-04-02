/*
 * Copyright (C) 2026 Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "SBSMapper.h"

#include <QtGlobal>

uint32_t SBSMapper::eyeIndexForPixel(QPoint px, uint32_t texWidth) const
{
  return ((uint32_t)px.x() >= texWidth / 2) ? 1u : 0u;
}

QPoint SBSMapper::otherEyePixel(QPoint px, uint32_t texWidth, uint32_t texHeight) const
{
  if(!enabled)
    return {-1, -1};

  if(px.x() < 0 || px.y() < 0 || (uint32_t)px.x() >= texWidth || (uint32_t)px.y() >= texHeight)
    return {-1, -1};

  int halfWidth = (int)(texWidth / 2);
  int otherX = (px.x() < halfWidth) ? px.x() + halfWidth : px.x() - halfWidth;

  return {otherX, px.y()};
}

// Multiplies a row-major 4x4 matrix M[16] by a column vector (vx,vy,vz,vw).
// Implements the HLSL mul(M, v) convention: result[i] = dot(row_i(M), v).
static void mulMat4Vec4(const float M[16], float vx, float vy, float vz, float vw, float out[4])
{
  out[0] = M[0] * vx + M[1] * vy + M[2] * vz + M[3] * vw;
  out[1] = M[4] * vx + M[5] * vy + M[6] * vz + M[7] * vw;
  out[2] = M[8] * vx + M[9] * vy + M[10] * vz + M[11] * vw;
  out[3] = M[12] * vx + M[13] * vy + M[14] * vz + M[15] * vw;
}

// World-space reprojection: unproject the source pixel to world space using
// ViewProjInverse[eyeIndex], then project into the other eye via ViewProj[otherEye].
//
// Input:  per-eye UV (x,y) in [0,1], NDC depth in [0,1], source eyeIndex.
// Output: otherMonoUVx/Y in [0,1] per-eye UV for the other eye.
// Returns false if the reprojection is degenerate or the result is outside [0,1].
bool SBSMapper::approxInverse(const float VP[16], const float VPInv[16])
{
  float prod[16] = {};
  for(int r = 0; r < 4; r++)
    for(int c = 0; c < 4; c++)
      for(int k = 0; k < 4; k++)
        prod[r * 4 + c] += VP[r * 4 + k] * VPInv[k * 4 + c];

  float err = 0.0f;
  for(int i = 0; i < 4; i++)
    for(int j = 0; j < 4; j++)
      err += qAbs(prod[i * 4 + j] - (i == j ? 1.0f : 0.0f));

  return err < 0.5f;
}

bool SBSMapper::reproject(float monoUVx, float monoUVy, float depth, uint32_t eyeIndex,
                          const VRFrameBufferMatrices &mats, float &otherMonoUVx, float &otherMonoUVy)
{
  // Step 1: UV → clip space.
  // D3D convention: UV (0,0) = top-left, clip (+1,+1) = top-right.
  //   monoUV * float2(2,-2) - float2(1,-1)
  float clipX = monoUVx * 2.0f - 1.0f;
  float clipY = 1.0f - monoUVy * 2.0f;
  float clipZ = depth;
  float clipW = 1.0f;

  // Step 2: Clip → world via CameraViewProjInverse[eyeIndex].
  float world[4];
  mulMat4Vec4(mats.viewProjInverse[eyeIndex], clipX, clipY, clipZ, clipW, world);

  if(qAbs(world[3]) < 1e-7f)
    return false;

  world[0] /= world[3];
  world[1] /= world[3];
  world[2] /= world[3];
  // world[3] is now 1.0

  // Step 3: Optional eye-offset correction (CameraPosAdjust / IPD offset).
  // Needed when the view matrices do not fully encode the interpupillary offset —
  // i.e. both eyes share a single world origin and the IPD is in a separate field.
  uint32_t otherEye = 1u - eyeIndex;
  if(mats.hasCameraPosAdjust)
  {
    world[0] += mats.cameraPosAdjust[eyeIndex][0] - mats.cameraPosAdjust[otherEye][0];
    world[1] += mats.cameraPosAdjust[eyeIndex][1] - mats.cameraPosAdjust[otherEye][1];
    world[2] += mats.cameraPosAdjust[eyeIndex][2] - mats.cameraPosAdjust[otherEye][2];
  }

  // Step 4: World → other eye clip via CameraViewProj[otherEye].
  // world[3] == 1.0 after the divide, so fold it into the constant column.
  const float *V = mats.viewProj[otherEye];
  float cX = V[0] * world[0] + V[1] * world[1] + V[2] * world[2] + V[3];
  float cY = V[4] * world[0] + V[5] * world[1] + V[6] * world[2] + V[7];
  float cW = V[12] * world[0] + V[13] * world[1] + V[14] * world[2] + V[15];

  if(qAbs(cW) < 1e-7f)
    return false;

  cX /= cW;
  cY /= cW;

  // Step 5: Clip → UV (Y-flipped back).
  //   clipPosOther.xy * float2(0.5, -0.5) + 0.5
  otherMonoUVx = cX * 0.5f + 0.5f;
  otherMonoUVy = -cY * 0.5f + 0.5f;

  return otherMonoUVx >= 0.0f && otherMonoUVx <= 1.0f && otherMonoUVy >= 0.0f && otherMonoUVy <= 1.0f;
}
