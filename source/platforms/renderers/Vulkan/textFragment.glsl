//
//  Gweni
//  Copyright (c) 2010 Facepunch Studios
//  MIT License, see LICENSE
//
//  renderer::Vulkan's prologue to the glyph slots' fragment shader body
//  (../shaders/text.frag.glsl), behind text.frag (with a second output, for
//  a device with dualSrcBlend) and textSingle.frag (without): the atlas being
//  drawn and the glyph slots' clip-box table in descriptor set 0, textMode and
//  textContrast push constants. CLIP_COLUMNS is SlotRenderer::kClipColumns and
//  SDF_SPREAD SlotRenderer::kSdfSpread, which vulkan.cpp checks.
//
#define CLIP_COLUMNS_LOG2 11u
#define CLIP_COLUMNS 2048u
#define SDF_SPREAD 6.0

#include "textPush.glsl"

#define textMode push.textMode
#define textContrast push.textContrast

layout(set = 0, binding = 0) uniform sampler2D texture1;
layout(set = 0, binding = 1) uniform usampler2D clipBoxes;

#include "../shaders/text.frag.glsl"
