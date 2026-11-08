#version 450
#extension GL_GOOGLE_include_directive : require
//
//  Gweni
//  Copyright (c) 2010 Facepunch Studios
//  MIT License, see LICENSE
//
//  renderer::Vulkan's prologue to the primitive slots' vertex shader body
//  (../shaders/primitive.vert.glsl), which OpenGLCore compiles behind its own.
//
//  projection and slotBase are push constants. Each chunk's vertex buffer is
//  bound at its first slot (vkCmdBindVertexBuffers' offset) and drawn with a
//  vertexOffset of 0, so gl_VertexIndex counts from the chunk's first vertex as
//  GL's gl_VertexID does from its vertex array's.
//
//  The projection is OpenGLCore's matrix, into GL's clip space, and
//  CLIP_SPACE_TO_API maps its z from [-1, 1] to Vulkan's [0, 1] as GL's
//  viewport transform does (w is 1), so each fragment's depth is computed as
//  on GL. y is not flipped: the target holds its rows bottom-up, as GL's
//  framebuffer does (renderer::Vulkan::readPixels turns them over, as
//  OpenGLCore's does), so every triangle lands on the same window coordinates
//  as on GL, and its coverage, interpolation and derivatives are GL's.
//
layout(push_constant) uniform PrimitivePush
{
    mat4 projection;
    int slotBase;
} push;

#define projection push.projection
#define slotBase push.slotBase
#define VERTEX_ID gl_VertexIndex
#define CLIP_SPACE_TO_API(p) p = vec4((p).x, (p).y, (p).z*0.5 + 0.5, (p).w)

#include "../shaders/primitive.vert.glsl"
