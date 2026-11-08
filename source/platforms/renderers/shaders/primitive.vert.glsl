//
//  Gweni
//  Copyright (c) 2010 Facepunch Studios
//  MIT License, see LICENSE
//
//  The primitive slots' vertex shader body, shared by every backend. A backend's
//  prologue goes in front of it (OpenGLCore::shaderSource, Vulkan/primitive.vert)
//  and declares what differs between the APIs:
//
//    projection        mat4: device px, y down, z index -> GL's clip space
//                      (y up, z in [-1, 1], w = 1), the same matrix for every API
//    slotBase          int: the first slot of the chunk being drawn
//    VERTEX_ID         the vertex's index from the chunk's first vertex
//                      (gl_VertexID; gl_VertexIndex with the chunk's vertex
//                      buffer bound at its first slot)
//    CLIP_SPACE_TO_API(p)
//                      a statement turning GL's clip-space position into the
//                      API's (nothing for GL; Vulkan maps z to [0, 1] as GL's
//                      viewport transform does, z*0.5 + 0.5, so every depth is
//                      computed as on GL)
//
layout (location = 0) in uint style;
layout (location = 1) in ivec3 inPosition;   // device px (signed) and z (16 bits unsigned, read as GL_SHORT with x and y)
layout (location = 2) in uvec2 inTexCoords;    // color0 (top fill) or UV
layout (location = 3) in ivec2 inLocal;        // SDF local position (rel to rect center)
layout (location = 4) in uvec2 inRadiusBorder; // SDF .x=radius .y=border (px)
layout (location = 5) in uvec2 inColor1;       // SDF gradient bottom color
layout (location = 6) in uvec2 inBorderColor;  // SDF border color
layout (location = 7) in uvec2 inColor3;       // bilinear bottom edge (6) / checker dark (7)

out vec2 TexCoords;
out vec4 VertexColor;
flat out uint ShaderType;
out vec2 vLocal;          // interpolated local position (px)
flat out vec2 vHalf;      // rect half-size (px)
flat out float vRadius;
flat out float vBorder;
flat out vec4 vColor1;    // gradient bottom color
flat out vec4 vBorderColor;
out vec4 vColor3;         // smooth: style 6 interpolates it along x
flat out vec4 vSegment;   // style 3: the line's endpoints (xy, zw), px from the quad centre
flat out float vBehindDepth; // style 3: the depth half a layer behind this slot's z
out vec2 vPixel;          // pixel-space position, the clip boxes' space

// The slot this vertex's quad is, for the fragment shader's clip box. A chunk is
// drawn from index 0 of its own slots, 4 vertexes apiece, with its attributes
// pointed at its first slot (slotBase), so VERTEX_ID >> 2 is the slot within the
// chunk. Flat: the four vertexes of a slot carry the same value, so whichever is
// the provoking vertex gives it.
flat out uint vSlot;

vec4 unpackColor(uvec2 c)
{
    return vec4(
        float((c.x >> 8) & 0xffu)/255.0,
        float(c.x & 0xffu)/255.0,
        float((c.y >> 8) & 0xffu)/255.0,
        float(c.y & 0xffu)/255.0);
}

void main()
{
    ShaderType = style;
    vSlot = uint(slotBase) + (uint(VERTEX_ID) >> 2);

    TexCoords = vec2(inTexCoords)/65535.0;
    VertexColor = unpackColor(inTexCoords);

    // inLocal is in half-pixel units, so an odd-sized rect's half-size (3.5) is exact
    vLocal = vec2(inLocal)*0.5;
    vHalf = abs(vLocal);
    vRadius = float(inRadiusBorder.x);
    vBorder = float(inRadiusBorder.y);
    vColor1 = unpackColor(inColor1);
    vBorderColor = unpackColor(inBorderColor);
    vColor3 = unpackColor(inColor3);
    // style 3 keeps its endpoints in color1 and color3, int16 bits in half-pixel units
    vSegment = vec4((ivec4(uvec4(inColor1, inColor3)) << 16) >> 16)*0.5;
    vPixel = vec2(inPosition.xy);
    // GL_SHORT sign-extends z; its low 16 bits are Vertex::z, 0..65535
    float z = float(inPosition.z & 0xffff);

    if(style == 0u)//item has been hidden, move all coords to 0.0f
        gl_Position = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    else
        gl_Position = projection * vec4(float(inPosition.x), float(inPosition.y), z, 1.0f);
    CLIP_SPACE_TO_API(gl_Position);

    vec4 behind = projection * vec4(float(inPosition.x), float(inPosition.y), z - 0.5, 1.0);
    vBehindDepth = behind.z*0.5 + 0.5;
}
