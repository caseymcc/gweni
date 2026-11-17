//
//  Gweni
//  Copyright (c) 2010 Facepunch Studios
//  MIT License, see LICENSE
//
//  The glyph slots' vertex shader body, shared by every backend, behind a
//  backend's prologue (OpenGLCore::shaderSource, Vulkan/text.vert), which
//  declares projection and slotBase and defines VERTEX_ID and
//  CLIP_SPACE_TO_API, LAYER_REMAP and LAYER_FETCH, as for primitive.vert.glsl.
//
layout (location = 0) in vec3 vertPosition;
layout (location = 1) in vec2 vertTexCoord;
layout (location = 2) in vec4 vertColor;
layout (location = 3) in float vertGamma;

out vec2 texCoord;
out vec4 color;
out float gamma;
out vec2 pixel;    // pixel-space position, the clip boxes' space
// The glyph slot. The text is drawn in chunks, each with its attributes pointed at
// its first slot (slotBase) and indexed from there, so VERTEX_ID is a vertex of
// the chunk: slot (VERTEX_ID >> 2) of it, however its runs are split between
// draws.
flat out uint slot;

// As primitive.vert.glsl's: glyph i of a run, at its label plus (i+1)/(n+1),
// stays between the run's shown layer and the next
float shownLayerZ(float z)
{
    float label = floor(z);
    int i = clamp(int(label), 0, 65535);
    float t = float(LAYER_FETCH(i));

    return t + (z - label)*(float(LAYER_FETCH(min(i + 1, 65535))) - t);
}

void main()
{
    texCoord=vertTexCoord;
    color=vertColor;
    gamma=vertGamma;
    pixel=vertPosition.xy;
    slot=uint(slotBase)+(uint(VERTEX_ID) >> 2);

    // On a 1/16 px grid, the coarsest sub-pixel grid GL guarantees (GL_SUBPIXEL_BITS
    // is at least 4), so no rasteriser snaps it further: an SDF glyph's pen is
    // fractional, and llvmpipe and the browser's SwiftShader snapped its quad to
    // grids of their own, which moved the field under the pixels by up to 1/32 px
    // and its anti-aliased edge by up to 10/255. Coverage and LCD glyphs are on
    // whole pixels already.
    float z = vertPosition.z;

    if(LAYER_REMAP)
        z = shownLayerZ(z);
    gl_Position = projection * vec4(floor(vertPosition.xy*16.0 + 0.5)/16.0, z, 1.0f);
    CLIP_SPACE_TO_API(gl_Position);
}
