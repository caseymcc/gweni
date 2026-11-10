#version 450
#extension GL_GOOGLE_include_directive : require
//
//  Gweni
//  Copyright (c) 2010 Facepunch Studios
//  MIT License, see LICENSE
//
//  renderer::Vulkan's prologue to the glyph slots' vertex shader body
//  (../shaders/text.vert.glsl), which OpenGLCore compiles behind its own.
//
//  As primitive.vert: projection and slotBase are push constants (the whole
//  block, which text.frag reads textMode and textContrast from), each chunk's
//  vertex buffer is bound at its first slot so gl_VertexIndex counts from the
//  chunk's first vertex, and CLIP_SPACE_TO_API maps GL's clip-space z to
//  Vulkan's [0, 1] with no y flip.
//
#include "textPush.glsl"

#define projection push.projection
#define slotBase push.slotBase
#define VERTEX_ID gl_VertexIndex
#define CLIP_SPACE_TO_API(p) p = vec4((p).x, (p).y, (p).z*0.5 + (p).w*0.5, (p).w)

#include "../shaders/text.vert.glsl"
