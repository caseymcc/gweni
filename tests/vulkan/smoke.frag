#version 450
//
//  Gweni
//  Copyright (c) 2010 Facepunch Studios
//  MIT License, see LICENSE
//
//  vulkan_smoke's fragment shader: two outputs at location 0 with indices 0 and
//  1, the dual-source blending the LCD text path will use (SRC1_COLOR).
//

layout(location = 0, index = 0) out vec4 color;
layout(location = 0, index = 1) out vec4 coverage;

void main()
{
    color = vec4(1.0);
    coverage = vec4(0.5);
}
