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
//                      (y up, z in [-w, w]; w = 1 but under a view transform,
//                      SlotRenderer::setViewTransform), the same matrix for every API
//    slotBase          int: the first slot of the chunk being drawn
//    VERTEX_ID         the vertex's index from the chunk's first vertex
//                      (gl_VertexID; gl_VertexIndex with the chunk's vertex
//                      buffer bound at its first slot)
//    CLIP_SPACE_TO_API(p)
//                      a statement turning GL's clip-space position into the
//                      API's (nothing for GL; Vulkan maps z to [0, w] as GL's
//                      viewport transform does, z*0.5 + w*0.5, so every depth is
//                      computed as on GL)
//
layout (location = 0) in uint style;
layout (location = 1) in ivec3 inPosition;   // device px (signed) and z (16 bits unsigned, read as GL_SHORT with x and y)
layout (location = 2) in uvec2 inTexCoords;    // color0 (top fill) or UV
layout (location = 3) in ivec2 inLocal;        // SDF local position (rel to rect center)
layout (location = 4) in uvec2 inRadiusBorder; // SDF .x=radius (styles 4, 6, 7: the edge mask in its high byte) .y=border (px)
layout (location = 5) in uvec2 inColor1;       // SDF gradient bottom color
layout (location = 6) in uvec2 inBorderColor;  // SDF border color
layout (location = 7) in uvec2 inColor3;       // bilinear bottom edge (6) / checker dark (7)

out vec2 TexCoords;
out vec4 VertexColor;
flat out uint ShaderType;
out vec2 vLocal;          // interpolated local position (px)
flat out vec2 vHalf;      // rect half-size (px)
flat out float vRadius;
flat out uint vEdges;     // styles 4, 6, 7: the edge mask (renderer::RoundedEdge), else 0
flat out float vBorder;
flat out vec4 vColor1;    // gradient bottom color
flat out vec4 vBorderColor;
out vec4 vColor3;         // smooth: style 6 interpolates it along x
flat out vec4 vSegment;   // style 3: the line's endpoints (xy, zw), px from the quad centre;
                          // style 8: its phases (xy), spacing (z) and reciprocal (w), px
flat out vec4 vJoints;    // style 3: its previous (xy) and next (zw) neighbour points, as vSegment;
                          // style 8: its major count (x) and stroke px (y)
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
    // the rounded styles keep their radius in the low byte and the edge mask in
    // the high one; the others (an image unit, an icon kind, a line's neighbour
    // bits) the whole field
    bool rounded = style == 4u || style == 6u || style == 7u;
    vRadius = float(rounded ? (inRadiusBorder.x & 0xffu) : inRadiusBorder.x);
    vEdges = rounded ? (inRadiusBorder.x >> 8) & 0xffu : 0u;
    vBorder = float(inRadiusBorder.y);
    vColor1 = unpackColor(inColor1);
    vBorderColor = unpackColor(inBorderColor);
    vColor3 = unpackColor(inColor3);
    // style 3 keeps its first endpoint in color1, int16 bits in half-pixel units; the
    // quad is centred on the segment, so the second is its negation (color3 holds
    // it, or the next neighbour), negated as an integer so a 0 stays +0.0
    ivec2 lineStart = (ivec2(inColor1) << 16) >> 16;
    vSegment = vec4(vec2(lineStart), vec2(-lineStart))*0.5;
    // and the neighbour points drawLine was given, in border colour (previous) and
    // color3 (next), which radius's bits say it holds (vRadius)
    vJoints = vec4((ivec4(uvec4(inBorderColor, inColor3)) << 16) >> 16)*0.5;
    // style 8 (drawGrid) keeps neither: its two phases (1/16 px in the border
    // colour's two fields), its spacing (a float's bits in tx/ty) and its
    // reciprocal, its major count and its stroke, worked out here once a vertex
    if(style == 8u)
    {
        float spacing = uintBitsToFloat((inTexCoords.x << 16) | inTexCoords.y);

        vSegment = vec4(vec2(inBorderColor)/16.0, spacing, 1.0/spacing);
        vJoints = vec4(float(inRadiusBorder.y & 0xffu), float((inRadiusBorder.y >> 8) & 0xffu), 0.0, 0.0);
    }
    vPixel = vec2(inPosition.xy);
    // GL_SHORT sign-extends z; its low 16 bits are Vertex::z, 0..65535
    float z = float(inPosition.z & 0xffff);

    if(style == 0u)//item has been hidden, move all coords to 0.0f
        gl_Position = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    else
        gl_Position = projection * vec4(float(inPosition.x), float(inPosition.y), z, 1.0f);
    CLIP_SPACE_TO_API(gl_Position);

    // GL's window depth of a point half a layer behind; w is exactly 1.0 in the
    // front view, and a view transform's camera (SlotRenderer::setViewTransform)
    // can make it anything
    vec4 behind = projection * vec4(float(inPosition.x), float(inPosition.y), z - 0.5, 1.0);
    vBehindDepth = behind.z/behind.w*0.5 + 0.5;
}
