#version 450
#extension GL_GOOGLE_include_directive : require
//
//  Gweni
//  Copyright (c) 2010 Facepunch Studios
//  MIT License, see LICENSE
//
//  The glyph slots' fragment shader with the second output dual-source
//  blending reads (an LCD atlas's runs, VK_BLEND_FACTOR_SRC1_COLOR), for a
//  device with the dualSrcBlend feature; textSingle.frag is the one without.
//
#define GWENI_DUAL_SOURCE 1
#include "textFragment.glsl"
