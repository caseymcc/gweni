#version 450
//
//  Gweni
//  Copyright (c) 2010 Facepunch Studios
//  MIT License, see LICENSE
//
//  vulkan_smoke's vertex shader: a triangle from the vertex index alone, so the
//  pipeline needs no vertex input. Never drawn; only compiled and built into a
//  pipeline.
//

void main()
{
    vec2 corner = vec2(float(gl_VertexIndex & 1), float(gl_VertexIndex >> 1));
    gl_Position = vec4(corner*2.0 - 1.0, 0.0, 1.0);
}
