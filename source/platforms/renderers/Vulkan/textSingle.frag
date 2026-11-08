#version 450
#extension GL_GOOGLE_include_directive : require
//
//  Gweni
//  Copyright (c) 2010 Facepunch Studios
//  MIT License, see LICENSE
//
//  The glyph slots' fragment shader for a device without the dualSrcBlend
//  feature: no second output, and LCD text falls back to grayscale
//  (SlotRenderer::lcdTextAvailable()).
//
#include "textFragment.glsl"
