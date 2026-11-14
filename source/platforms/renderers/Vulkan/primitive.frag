#version 450
#extension GL_GOOGLE_include_directive : require
//
//  Gweni
//  Copyright (c) 2010 Facepunch Studios
//  MIT License, see LICENSE
//
//  renderer::Vulkan's prologue to the primitive slots' fragment shader body
//  (../shaders/primitive.frag.glsl): the clip-box table and the 5 image pages
//  in descriptor set 0. CLIP_COLUMNS is SlotRenderer::kClipColumns, which
//  vulkan.cpp checks.
//
#define CLIP_COLUMNS_LOG2 11u
#define CLIP_COLUMNS 2048u

layout(set = 0, binding = 0) uniform usampler2D clipBoxes;
layout(set = 0, binding = 1) uniform sampler2DArray imagePages[5];

#include "../shaders/primitive.frag.glsl"
