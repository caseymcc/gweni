//
//  Gweni
//  Copyright (c) 2010 Facepunch Studios
//  MIT License, see LICENSE
//
//  The glyph slots' push constants, one block for both stages
//  (renderer::Vulkan::TextPush): the vertex shader reads projection and
//  slotBase, the fragment shader textMode and textContrast.
//
layout(push_constant) uniform TextPush
{
    mat4 projection;
    int slotBase;
    int textMode;
    float textContrast;
} push;
