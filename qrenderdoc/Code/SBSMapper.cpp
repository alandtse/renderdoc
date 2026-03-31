/*
 * Copyright (C) 2026 Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "SBSMapper.h"

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
