//
//  Gweni
//  Copyright (c) 2010 Facepunch Studios
//  MIT License, see LICENSE
//
//  The primitive slots' fragment shader body, shared by every backend, behind a
//  backend's prologue (OpenGLCore::shaderSource, Vulkan/primitive.frag), which
//  defines CLIP_COLUMNS and CLIP_COLUMNS_LOG2 and declares the samplers:
//
//    uniform sampler2DArray imagePages[5];  style 2's image pages
//    uniform usampler2D clipBoxes;  the slots' clip boxes
//
out vec4 FragColor;

in vec2 TexCoords;
in vec4 VertexColor;
flat in uint ShaderType;
in vec2 vLocal;
flat in vec2 vHalf;
flat in float vRadius;
flat in uint vEdges;
flat in float vBorder;
flat in vec4 vColor1;
flat in vec4 vBorderColor;
in vec4 vColor3;
flat in vec4 vSegment;
flat in vec4 vJoints;
flat in float vBehindDepth;
in vec2 vPixel;

// mix(), spelled out, and used in its place throughout: Mesa's GLSL compiler
// and lavapipe (from glslang's FMix) lower mix() to different arithmetic (mostly
// a + (b - a)*t and a*(1 - t) + b*t), which round a value that falls on an 8-bit
// tie a level apart (a row of every vertical gradient). Written out, every
// backend computes the same, and GL its pixels as before (docs/architecture.md ->
// "Vulkan's counters and pixels against GL's").
float lerp(float a, float b, float t) { return a + (b - a)*t; }
vec2 lerp(vec2 a, vec2 b, vec2 t) { return a + (b - a)*t; }
vec3 lerp(vec3 a, vec3 b, float t) { return a + (b - a)*t; }
vec4 lerp(vec4 a, vec4 b, float t) { return a + (b - a)*t; }

// imagePages[5]: style 2's images, a 2D texture array per size class, 128 << c
// texels square (SlotRenderer::ImageClasses; vRadius picks the class, vBorder the
// layer). GLSL 3.30 and ES 3.00 index a sampler array only with a constant, hence
// the switches.
// clipBoxes: one Box (x0, y0, x1, y1) per primitive slot, slot s at texel
// (s % CLIP_COLUMNS, s / CLIP_COLUMNS); the vertex shader gives the slot.
flat in uint vSlot;

uvec4 clipBox(uint slot)
{
    return texelFetch(clipBoxes, ivec2(int(slot & (CLIP_COLUMNS - 1u)), int(slot >> CLIP_COLUMNS_LOG2)), 0);
}

// The pixel's row of a rect and the rect's height, in device px: local y runs
// -half (top) .. +half (bottom), so at a pixel centre vLocal.y + vHalf.y is
// row + 0.5, and an interpolation error under half a pixel leaves the row as it is.
ivec2 gradientRow()
{
    int h = max(int(2.0*vHalf.y + 0.5), 1);
    int r = clamp(int(floor(vLocal.y + vHalf.y)), 0, h - 1);
    return ivec2(r, h);
}

// The row centre's place from the top edge to the bottom, (2r+1)/(2h), exactly.
float gradientT()
{
    ivec2 row = gradientRow();
    return float(2*row.x + 1)/float(2*row.y);
}

// Style 4's vertical gradient from top to bottom as the 8-bit level of each
// channel, alpha included, at the row's centre rounded half up:
// (top*2h + (bottom-top)*(2r+1) + h) / (2h), in ints. The colours are bytes/255
// from the vertex shader, so their bytes come back exactly, and every backend
// writes the same level. A float lerp landed exactly on x.5 of a level on some
// rows (an odd difference over an odd height, on the middle row), where one ulp
// of the interpolated position decided the level and llvmpipe and lavapipe
// rounded a level apart (#492; docs/architecture.md -> "Vulkan's counters and
// pixels against GL's"). The dividend is non-negative and under 2^24: h is at
// most 32767, so it is at most 255*2h + h.
vec4 gradientLevel(vec4 top, vec4 bottom)
{
    ivec2 row = gradientRow();
    ivec4 a = ivec4(top*255.0 + 0.5);
    ivec4 b = ivec4(bottom*255.0 + 0.5);
    int den = 2*row.y;
    ivec4 num = a*den + (b - a)*(2*row.x + 1) + row.y;
    // num/den as a float quotient of two exact floats, at most 1 off whatever the
    // division's precision, then set right in ints: an int division of each
    // channel cost llvmpipe 6.4 % more instructions in a frame of gweni_bench's
    // idle scene, this 2.9 %
    ivec4 level = ivec4(floor(vec4(num)/float(den)));
    level -= ivec4(lessThan(num, level*den));
    level += ivec4(greaterThanEqual(num, (level + 1)*den));
    return vec4(level)/255.0;
}

// Signed distance to a rounded box centered at the origin (negative inside).
float sdRoundBox(vec2 p, vec2 b, float r)
{
    vec2 q = abs(p) - b + vec2(r);
    return min(max(q.x, q.y), 0.0) + length(max(q, vec2(0.0))) - r;
}

// The edge mask's bits (renderer::RoundedEdge, Vertex::radius's high byte):
// bit 0..3 the top-left, top-right, bottom-left and bottom-right corner square,
// bit 4..7 the top, right, bottom and left side without a border.

// The radius of p's corner, p from the box's centre (y down): r, or 0 where the
// mask makes that corner square. Every corner's arc is within r of it, so the
// quadrant p is in names the corner whose arc it can be near.
float cornerRadius(vec2 p, float r)
{
    uint corner = (p.y < 0.0 ? 0u : 2u) + (p.x < 0.0 ? 0u : 1u);
    return ((vEdges >> corner) & 1u) != 0u ? 0.0 : r;
}

// Rounded-rect mask of styles 4, 6 and 7: applies the border to fill and the
// anti-aliased outer edge to its alpha. Both are a 1 px box filter: the part of
// the pixel on the inner side of the edge, with aa the size of a pixel in units
// of d. A pixel-aligned straight edge is then exactly 1 inside and 0 outside, so
// the outermost pixel is fully covered, two rects that abut meet without a seam
// and an N px border is N whole rows; only a curved edge (a corner arc) gets
// partial coverage. The edge mask (vEdges) makes a corner square (its radius 0)
// and keeps the border off a side; with none of its bits set every step is the
// one it was without a mask, so an unmasked rect draws the same bytes.
vec4 roundedRect(vec4 fill)
{
    float r = clamp(vRadius, 0.0, min(vHalf.x, vHalf.y));
    float d = sdRoundBox(vLocal, vHalf, cornerRadius(vLocal, r));
    // d is a true distance in vLocal's units, so a pixel is aa of it: vLocal's
    // screen gradient (1 while vLocal is in device px). Not d's own gradient:
    // fwidth(d) is sqrt(2) too wide across a corner diagonal, and at a square
    // corner, where d=max(qx,qy) has a kink, the 2x2 derivative of the corner
    // pixel can span both axes and come out sqrt(2) too.
    float aa = max(0.5*(length(dFdx(vLocal)) + length(dFdy(vLocal))), 1e-4);
    float shapeA = clamp(0.5 - d/aa, 0.0, 1.0);   // outer edge coverage

    vec3 rgb = fill.rgb;
    if(vBorder > 0.0)
    {
        // -d is the inward distance from the edge; border occupies [0, vBorder].
        // A side without a border (the mask's high nibble) is pushed out past the
        // border's reach (vBorder and a pixel) in the quadrants it bounds, and
        // there the border is measured to the straight box with it pushed, the
        // nearer of that and d: the borders of the sides next to it run straight
        // to its edge, and no pixel is within vBorder of it. Selects, not a
        // branch around a second sdRoundBox(): that branch cost llvmpipe 4.6 %
        // of gweni_bench idle's flush with no rect masked. A quadrant with no
        // such side keeps d, so an unmasked rect draws the same bytes.
        float sideX = float((vEdges >> (vLocal.x < 0.0 ? 7u : 5u)) & 1u);
        float sideY = float((vEdges >> (vLocal.y < 0.0 ? 4u : 6u)) & 1u);
        vec2 qb = abs(vLocal) - vHalf - vec2(sideX, sideY)*(vBorder + aa);
        float db = (sideX + sideY > 0.0) ? min(d, max(qb.x, qb.y)) : d;
        float bMix = clamp(0.5 + (vBorder + db)/aa, 0.0, 1.0);
        rgb = lerp(fill.rgb, vBorderColor.rgb, bMix);
    }
    return vec4(rgb, fill.a * shapeA);
}

// Signed distance to the axis-aligned box [lo, hi] (negative inside).
float sdBox(vec2 p, vec2 lo, vec2 hi)
{
    vec2 v = abs(p - 0.5*(lo + hi)) - 0.5*(hi - lo);
    return length(max(v, vec2(0.0))) + min(max(v.x, v.y), 0.0);
}

// Signed distance to the outline of [lo, hi], s thick on its inner side.
float sdOutline(vec2 p, vec2 lo, vec2 hi, float s)
{
    return max(sdBox(p, lo, hi), -sdBox(p, lo + vec2(s), hi - vec2(s)));
}

// Distance from p to the segment a-b (for stroked icon glyphs).
float sdSegment(vec2 p, vec2 a, vec2 b)
{
    vec2 pa = p - a;
    vec2 ba = b - a;
    float h = clamp(dot(pa, ba)/dot(ba, ba), 0.0, 1.0);
    return length(pa - ba*h);
}

// The part of a pixel (a unit square of device px) on the inner side of a straight
// edge e px from its centre, n the edge's unit normal: exact for a half-plane. Its
// projection on n is |n.x|+|n.y| wide: the corners give the quadratic ends, the
// middle is linear, and an axis-aligned edge (n = (1,0) or (0,1)) is the plain
// clamp(0.5 + e, 0, 1) of style 4's box filter.
float pixelArea(float e, vec2 n)
{
    vec2 m = abs(n);
    float a = max(m.x, m.y);
    float b = min(m.x, m.y);
    float h1 = 0.5*(a - b);
    float h2 = 0.5*(a + b);

    if(e <= -h2)
        return 0.0;
    if(e >= h2)
        return 1.0;
    if(e < -h1)
        return (e + h2)*(e + h2)/(2.0*a*b);
    if(e > h1)
        return 1.0 - (h2 - e)*(h2 - e)/(2.0*a*b);
    return 0.5 + e/a;
}

// Style 9: the part of the pixel centred at c (px from the quad's top-left) on the
// inner side of a trapezoid's edge, less a half, from the vertex shader's
// trapezoidEdge(): pixelArea() of the centre's distance e to it, written without
// a branch or a division. pixelArea() is odd about e = 0 less its 0.5: its linear
// part e/a with |e| clamped to h1, and past h1 its quadratic end, by t = clamp(|e|
// - h1, 0, b), t(2b - t)/(2ab), which is 0 at h1 and b/(2a) (to 1) at h2. Exact
// for a pixel the edge crosses anywhere in its column.
float trapezoidEdge(vec2 c, vec4 line, vec3 area)
{
    float e = line.x*c.y + line.y*c.x + line.z;
    float d = abs(e);
    float b = area.x;
    float t = clamp(d - line.w, 0.0, b);

    return sign(e)*(min(d, line.w)*area.y + t*(2.0*b - t)*area.z);
}

// Style 3: the vector from the nearest point of the segment from s to e to a pixel's
// centre (pa the centre less s, ba e less s), and h, where that point is along it
// (0 at s, 1 at e).
vec2 lineOffset(vec2 pa, vec2 ba, out float h)
{
    h = clamp(dot(pa, ba)/max(dot(ba, ba), 1e-8), 0.0, 1.0);
    return pa - ba*h;
}

// Style 3's coverage of a pixel by the capsule radius px around a segment (ba its
// end less its start), v the pixel's offset from its nearest point (lineOffset): the
// part of the pixel between the stroke's two edges, each taken as a straight edge
// across the pixel normal to n, the direction of v (radial in a cap).
float lineOffsetCoverage(vec2 v, vec2 ba, float radius, out vec2 n)
{
    float d = length(v);

    n = d > 1e-4 ? v/d
        : (dot(ba, ba) > 1e-8 ? normalize(vec2(-ba.y, ba.x)) : vec2(0.0, 1.0));
    return clamp(pixelArea(radius - d, n) + pixelArea(radius + d, n) - 1.0, 0.0, 1.0);
}

// An image at uv as GL_LINEAR_MIPMAP_LINEAR with GL_CLAMP_TO_EDGE samples it, with
// uv's screen gradients dx and dy (taken in uniform control flow), computed so
// that every GL gives the same value, which textureGrad did not (llvmpipe against
// the browser's SwiftShader, by up to 4/255 in ImagePanelDemo). The image is
// size0 texels at the origin of layer `layer` of class `page`'s page, each of its
// levels at the origin of that level of the layer, so its own edges are clamped
// to here, not the layer's, and nothing of the rest of the layer is read. The
// levels are made on the CPU (imageMipChain), and the LOD and the weights are the
// GL spec's, computed in float here:
// - the LOD is log2 of the longer of the two gradients in level 0's texels,
//   rounded to 1/256: uv is linear over the image's quad, so its gradients are
//   the same anywhere and on any GL up to a float error the rounding removes;
// - at or below 0 (magnification) it is level 0, bilinear: the 4 texels around
//   uv, clamped to the level's edge, weighted by uv's place between their
//   centres to 1/256; above, the 4 of each of the 2 levels around it, the levels
//   mixed by its fraction; past the last level (1x1) that level's 4.
// Each tap reads one texel exactly: textureLod at its centre in the page's level
// (a power of two texels square, so the centre is exact) and the level's whole
// LOD, the page's filter nearest (OpenGLCore::allocateImagePage). Not texelFetch,
// which reads the same texel: llvmpipe runs every branch of a shader for every
// pixel, masked, and skips a sample whose pixels are all masked off but not a
// texelFetch, so with 8 of them here a page without an image took 5.7 % more
// instructions a frame than with textureGrad (ButtonDemo), and 2.4 % now. The taps
// are one loop whose count is not a constant, so that no compiler unrolls it into
// 8 switches.
vec4 imageTexel(int page, float layer, ivec2 size0, vec2 uv, vec2 dx, vec2 dy)
{
    size0 = max(size0, ivec2(1));

    vec2 texels = vec2(size0);
    float rho = max(length(dx*texels), length(dy*texels));
    float lod = floor(log2(max(rho, 1e-8))*256.0 + 0.5)/256.0;

    // the last level, floor(log2(the longer side)), log2's float error corrected
    int longer = max(size0.x, size0.y);
    int last = int(log2(float(longer)));
    if((longer >> last) == 0)
        last -= 1;
    else if((longer >> (last + 1)) != 0)
        last += 1;

    lod = clamp(lod, 0.0, float(last));
    int level0 = int(floor(lod));
    int level1 = min(level0 + 1, last);
    float levelWeight = lod - float(level0);
    int taps = levelWeight > 0.0 ? 8 : 4;
    int side = 128 << page;
    vec4 sum = vec4(0.0);

    for(int tap = 0; tap < taps; ++tap)
    {
        bool first = tap < 4;
        int level = first ? level0 : level1;
        // level n's size, as GL and Vulkan size a mip level
        ivec2 size = max(size0 >> level, ivec2(1));
        vec2 t = uv*vec2(size) - 0.5;
        // the weights to 1/256 of a texel, as a texture unit's bilinear filter
        // keeps them, so that uv's float error (a quad moved, or another GL's
        // interpolation) does not move a level
        vec2 f = floor(fract(t)*256.0 + 0.5)/256.0;
        ivec2 corner = ivec2(tap & 1, (tap >> 1) & 1);
        ivec2 p = clamp(ivec2(floor(t)) + corner, ivec2(0), size - 1);
        vec2 w = lerp(vec2(1.0) - f, f, vec2(corner));
        vec3 centre = vec3((vec2(p) + 0.5)/float(side >> level), layer);
        float texelLod = float(level);
        vec4 texel = vec4(0.0);

        switch(page)
        {
        case 0: texel = textureLod(imagePages[0], centre, texelLod); break;
        case 1: texel = textureLod(imagePages[1], centre, texelLod); break;
        case 2: texel = textureLod(imagePages[2], centre, texelLod); break;
        case 3: texel = textureLod(imagePages[3], centre, texelLod); break;
        case 4: texel = textureLod(imagePages[4], centre, texelLod); break;
        }
        sum += texel*(w.x*w.y*(first ? 1.0 - levelWeight : levelWeight));
    }
    return sum;
}

void main()
{
    // Tested here, discarded at the end: the derivatives below (dFdx/dFdy, fwidth)
    // want the whole quad alive.
    uvec4 clip = clipBox(vSlot);
    float depth = gl_FragCoord.z;
    bool lineDeferred = false;  // style 3: an adjacent segment of its polyline draws this pixel
    bool offGrid = false;       // style 8: the pixel is on no grid line
    bool clipped = any(lessThan(vPixel, vec2(clip.xy))) || any(greaterThanEqual(vPixel, vec2(clip.zw)));
    vec2 uvDx = dFdx(TexCoords);
    vec2 uvDy = dFdy(TexCoords);

    if(ShaderType == 0u)
        FragColor = vec4(0.0, 0.0, 0.0, 0.0);
    else if(ShaderType == 1u)
        FragColor = VertexColor;
    else if (ShaderType == 2u)
    {
        // Image: its page is in vRadius, its layer in vBorder, its size in
        // vJoints.xy, the draw colour (tint) in vColor1.
        FragColor = imageTexel(int(vRadius + 0.5), vBorder, ivec2(vJoints.xy + 0.5), TexCoords, uvDx, uvDy)
            * vColor1;
    }
    else if (ShaderType == 3u)
    {
        // Line: a capsule vBorder px thick around the segment vSegment.xy-vSegment.zw
        // (px from the quad's centre). A pixel is covered by the part of it between
        // the stroke's two edges, each taken as a straight edge across the pixel along
        // the normal from the nearest point of the segment (radial in a cap): the
        // pixel's exact area for the body of the stroke, so a thin slanted line is
        // as bright on every row. drawLine puts the stroke's edges of a horizontal
        // or vertical segment on pixel boundaries, where this is style 4's box
        // filter: whole fully covered rows (columns) and nothing beside them.
        float radius = 0.5*vBorder;
        float h;
        vec2 n;
        vec2 ba = vSegment.zw - vSegment.xy;
        float a = lineOffsetCoverage(lineOffset(vLocal - vSegment.xy, ba, h), ba, radius, n);

        // The neighbours drawLine was given (the bits of vRadius, the points in
        // vJoints) are the polyline's segments before this one (k = 0, its previous
        // point to this one's start) and after it (k = 2, this one's end to its next
        // point), drawn at the same thickness and z in slots of their own. Each pixel
        // two of them reach is drawn by one, so it is blended once: the segment
        // nearest to its centre, ties (the disc at a joint, which both end at) going
        // to the earlier one. Every slot reaching the pixel must agree on that to the
        // bit, so the centre is taken exactly (at a pixel centre vLocal is a multiple
        // of 0.5, from the quad's whole-px corners) and each distance is a function of
        // the centre less the segment's start and its end less its start, whole half
        // px whichever slot's centre they are taken from, through the one lineOffset
        // call in the loop. (Ownership by distance rather than by coverage: llvmpipe
        // runs every branch of the shader for every pixel, masked, so what is added
        // here every primitive's pixels pay for.)
        int joints = int(vRadius + 0.5);
        bool present[3];
        vec2 starts[3];
        vec2 ends[3];

        present[0] = (joints & 1) != 0;
        present[1] = true;
        present[2] = (joints & 2) != 0;
        starts[0] = vJoints.xy;
        ends[0] = vSegment.xy;
        starts[1] = vSegment.xy;
        ends[1] = vSegment.zw;
        starts[2] = vSegment.zw;
        ends[2] = vJoints.zw;

        vec2 p = floor(vLocal*2.0 + 0.5)*0.5;
        vec2 offset[3];
        float dist[3];
        float along[3];

        for(int k = 0; k < 3; ++k)
        {
            along[k] = 0.0;
            offset[k] = lineOffset(p - starts[k], ends[k] - starts[k], along[k]);
            dist[k] = present[k] ? dot(offset[k], offset[k]) : 1e30;
        }
        if(dist[0] <= dist[1] || dist[2] < dist[1])
            lineDeferred = true;

        // This segment draws the pixel at the part of it that its stroke and its
        // nearer neighbour's cover together. Where both nearest points are the
        // joint, the strokes there are the same disc: the larger of the two (equal
        // but for vLocal's rounding). Elsewhere, the two coverages a and ak less
        // their overlap: the product a*ak where their edges cross at right angles,
        // the smaller where they are parallel and cover the same side of the pixel,
        // and a + ak - 1 (or nothing) where they cover opposite sides, mixed by the
        // cosine between the two normals. That is the union of the two strips over
        // the pixel to within a few levels (a cap's strip against the other's body,
        // a bend's inner crotch, a sharp bend's tip), and continuous: at the line
        // where one nearest point leaves the joint the two normals are one, and both
        // owners of a boundary compute it alike.
        bool before = dist[0] <= dist[2];
        vec2 nk;
        float ak = lineOffsetCoverage(before ? offset[0] : offset[2],
            before ? ends[0] - starts[0] : ends[2] - starts[2], radius, nk);
        bool disc = before ? (along[0] >= 1.0 && along[1] <= 0.0) : (along[1] >= 1.0 && along[2] <= 0.0);

        if(!(before ? present[0] : present[2]))
            ak = 0.0;
        if(ak > 0.0)
        {
            if(disc)
                a = max(a, ak);
            else
            {
                float both = a*ak;
                float c = dot(n, nk);
                float overlap = c >= 0.0 ? lerp(both, min(a, ak), c)
                    : lerp(both, max(a + ak - 1.0, 0.0), -c);

                a = clamp(a + ak - overlap, max(a, ak), 1.0);
            }
        }
        FragColor = vec4(VertexColor.rgb, VertexColor.a * a);
        // A partly covered pixel's depth is moved toward the layer behind, up to half
        // a layer as its coverage goes to 0. Adjacent segments no longer both draw a
        // pixel, but segments farther apart along a polyline at one z can (one shorter
        // than the stroke, or a zigzag crossing itself), and with GL_LESS the first
        // one's anti-aliased rim would otherwise keep a later one off pixels it covers
        // fully; this way a later segment draws over the pixels an earlier one covers
        // less, still in front of every lower z, while a fully covered pixel keeps its
        // z, so it is blended once there however many of them cover it.
        depth = lerp(vBehindDepth, gl_FragCoord.z, a);
    }
    else if (ShaderType == 4u)
    {
        // SDF rounded rect: vertical gradient fill + optional border, analytic AA.
        FragColor = roundedRect(gradientLevel(VertexColor, vColor1));
    }
    else if (ShaderType == 6u)
    {
        // Bilinear fill: VertexColor and vColor3 are the top and bottom edge colours
        // at this x (each linear in x, so exact across the triangle split), blended
        // along y at the row's exact place, as style 4's gradient is.
        FragColor = roundedRect(lerp(VertexColor, vColor3, gradientT()));
    }
    else if (ShaderType == 7u)
    {
        // Checker composite: VertexColor (with alpha) over 4 px cells of vColor1
        // (light) and vColor3 (dark), counted from the rect's top-left; opaque.
        ivec2 cell = ivec2(floor((vLocal + vHalf)/4.0));
        vec3 checker = ((cell.x + cell.y) & 1) == 0 ? vColor1.rgb : vColor3.rgb;
        FragColor = roundedRect(vec4(lerp(checker, VertexColor.rgb, VertexColor.a), 1.0));
    }
    else if (ShaderType == 8u)
    {
        // Grid lines (drawGrid): one every spacing px from a phase on each axis,
        // stroke whole px wide where drawLine would put a stroke across that
        // coordinate, those a whole number of majorEvery spacings from the phase in
        // vColor3 and the rest in vColor1; the vertex shader unpacked them into
        // vSegment (phases, spacing, its reciprocal) and vJoints (major count,
        // stroke). The pixel is found from vLocal rounded to its half px, as the
        // icons' is.
        float stroke = vJoints.y;
        vec2 q = floor(floor(vLocal*2.0 + 0.5)*0.5 + vHalf);
        // the nearest line on each axis, and where its stroke starts
        vec2 k = floor((q + 0.5 - vSegment.xy)*vSegment.w + 0.5);
        vec2 start = floor(vSegment.xy + k*vSegment.z + 0.5 - 0.5*stroke);
        bool onX = q.x >= start.x && q.x < start.x + stroke;
        bool onY = q.y >= start.y && q.y < start.y + stroke;
        bool major = (onX && mod(k.x, vJoints.x) == 0.0) || (onY && mod(k.y, vJoints.x) == 0.0);

        offGrid = !(onX || onY);
        FragColor = major ? vColor3 : vColor1;
    }
    else if (ShaderType == 9u)
    {
        // Trapezoid (drawTrapezoid): the quad is its whole columns, from the highest
        // edge to the lowest, and a pixel is covered by its exact area between the
        // two edges (vSegment and vColor1 the top's terms, vJoints and vBorderColor
        // the bottom's): each edge's part, less the 1 their union is (where the top
        // is above the bottom every point is inside one of them): the sum of the
        // two less their halves. Where the edges have crossed, the part between
        // them is that sum negated, so it is the sum's magnitude, exact but in a
        // pixel the crossing is in, which is covered by the difference of its two
        // sides. The pixel's column and row
        // are those vLocal is in: at the pixel's centre it is half a px from their
        // edges, farther than an interpolation error moves it, so the coverage is
        // a function of the flat edges and the column and row alone.
        vec2 c = floor(vLocal + vHalf) + 0.5;
        float a = min(abs(trapezoidEdge(c, vSegment, vColor1.xyz)
            + trapezoidEdge(c, vJoints, vBorderColor.xyz)), 1.0);

        FragColor = vec4(VertexColor.rgb, VertexColor.a * a);
    }
    else if (ShaderType == 5u)
    {
        // Icon glyph: vRadius=kind, vBorder=dir (the chevron and the triangles).
        int kind = int(vRadius + 0.5);
        float a;

        if(kind == 9)
        {
            // Arc (drawArc): on the largest circle centred in the quad, its stroke,
            // the whole px drawArc packs into the border colour's red byte, inside
            // the circle, round caps at both ends. Its start and sweep are color1's
            // two fields, 1/65536 of a turn from the top clockwise (y is down), a
            // sweep of 65535 the whole ring. In device px with the box filter and
            // the constant aa of kinds 2-6, at the pixel centre rounded to the
            // half px it is at (as kinds 0 and 1), so the arc drawn here is the
            // arc moved, or drawn by another backend.
            float s = floor(vBorderColor.r*255.0 + 0.5);
            float startBits = floor(vColor1.r*255.0 + 0.5)*256.0 + floor(vColor1.g*255.0 + 0.5);
            float sweepBits = floor(vColor1.b*255.0 + 0.5)*256.0 + floor(vColor1.a*255.0 + 0.5);
            float aa = max(0.5*(length(dFdx(vLocal)) + length(dFdy(vLocal))), 1e-4);
            vec2 q = floor(vLocal*2.0 + 0.5)*0.5;
            float outer = min(vHalf.x, vHalf.y);
            float w = min(s, outer);
            float r = outer - 0.5*w;    // the stroke's middle
            float d = abs(length(q) - r) - 0.5*w;

            if(sweepBits < 65535.0)
            {
                // where q is from the start, in turns clockwise: inside the sweep
                // the ring's distance is the arc's, past it the nearer cap's
                float start = startBits/65536.0;
                float sweep = sweepBits/65535.0;
                float t = dot(q, q) > 0.0 ? fract(atan(q.x, -q.y)*0.15915494 - start) : 0.0;

                if(t > sweep)
                {
                    float a0 = start*6.2831853;
                    float a1 = (start + sweep)*6.2831853;
                    vec2 p0 = r*vec2(sin(a0), -cos(a0));
                    vec2 p1 = r*vec2(sin(a1), -cos(a1));

                    d = min(length(q - p0), length(q - p1)) - 0.5*w;
                }
            }
            a = clamp(0.5 - d/aa, 0.0, 1.0);
        }
        else if(kind <= 1 || kind >= 7)
        {
            // Normalized local coords in [-1,1], so the stroke scales with the rect.
            // At a pixel's centre vLocal is a multiple of 0.5 exactly (the quad's
            // corners are whole px, vLocal is from its centre), so it is rounded to
            // that: interpolated, it carries a float error that depends on where the
            // quad is and on the rasteriser, which the stroke's anti-aliased edge
            // turned into a level of difference between the glyph drawn here and the
            // same glyph moved, or drawn by another GL.
            vec2 local = floor(vLocal*2.0 + 0.5)*0.5;
            vec2 p = local / max(vHalf, vec2(1.0));
            int dir = int(vBorder + 0.5);

            // p is turned into the down-pointing shape's frame: its apex,
            // at +y there, lands at -x for left and +x for right.
            if(dir == 1) p = vec2(p.x, -p.y);       // up
            else if(dir == 2) p = vec2(p.y, -p.x);  // left
            else if(dir == 3) p = vec2(-p.y, p.x);  // right
            // dir 0 == down / as-authored

            float d;
            if(kind == 0)
            {
                // checkmark (two strokes); y is down, so B is the low point.
                vec2 A = vec2(-0.55, 0.0);
                vec2 B = vec2(-0.18, 0.38);
                vec2 C = vec2(0.55, -0.45);
                d = min(sdSegment(p, A, B), sdSegment(p, B, C)) - 0.14;
            }
            else if(kind == 1)
            {
                // chevron "v" pointing down (rotated by dir above).
                vec2 A = vec2(-0.5, -0.2);
                vec2 B = vec2(0.0, 0.28);
                vec2 C = vec2(0.5, -0.2);
                d = min(sdSegment(p, A, B), sdSegment(p, B, C)) - 0.13;
            }
            else
            {
                // triangle pointing down (rotated by dir above), its base along the
                // top of the rect and its apex at the bottom: filled (7), or its
                // outline (8), the stroke drawIcon packs into the border colour's
                // red byte thick, inside its edge. Its distance is the largest of
                // its three sides' (each a unit normal from the centre), exact
                // inside and along the sides, where the edge's coverage reads it.
                d = max(-p.y - 0.8, max(dot(p, vec2(0.88379, 0.46789)), dot(p, vec2(-0.88379, 0.46789))) - 0.42110);
                if(kind == 8)
                {
                    vec2 hp = max(vHalf, vec2(1.0));
                    float t = 0.5*floor(vBorderColor.r*255.0 + 0.5)*(1.0/hp.x + 1.0/hp.y);

                    d = abs(d + 0.5*t) - 0.5*t;
                }
            }

            // aa is one device pixel in p's units: vLocal is in device px, so a
            // pixel is 1 over the half size p is divided by. Not fwidth(d): a
            // derivative of d is implementation-defined (fine or coarse, per 2x2
            // block), so the stroke's edge differed by up to 82/255 between llvmpipe
            // and the browser's SwiftShader; nor vLocal's own derivative, whose float
            // rounding depends on where the quad is, which moved a pixel of a glyph
            // drawn across the view's edge.
            vec2 h = max(vHalf, vec2(1.0));
            float aa = 0.5*(1.0/h.x + 1.0/h.y);
            a = 1.0 - smoothstep(0.0, aa, d);
        }
        else
        {
            // Kinds 2-6 (the window buttons' glyphs and a separator's rule) are
            // measured in device px, with
            // a stroke of the whole px drawIcon packs into the border colour's red
            // byte and roundedRect()'s 1 px box filter. Its aa is constant over the
            // quad (fwidth(d) is not: it changes per 2x2 block), so the glyph is
            // mirror-symmetric pixel for pixel. Every edge of the dash, squares and rule is
            // a whole px from the quad's top-left, so each stroke covers whole rows
            // and columns fully and nothing beside them.
            float s = floor(vBorderColor.r*255.0 + 0.5);
            float aa = max(0.5*(length(dFdx(vLocal)) + length(dFdy(vLocal))), 1e-4);
            vec2 size = 2.0*vHalf;
            vec2 q = vLocal + vHalf;    // px from the quad's top-left
            float side = min(size.x, size.y);
            vec2 o = floor((size - vec2(side))*0.5);  // the glyph's square, centred

            float d;
            if(kind == 2)
            {
                // cross: both diagonals of the centred square, their round caps
                // reaching its edges
                float e = 0.5*side - 0.5*s;
                d = min(sdSegment(vLocal, vec2(-e), vec2(e)),
                        sdSegment(vLocal, vec2(-e, e), vec2(e, -e))) - 0.5*s;
            }
            else if(kind == 3)
            {
                // dash: the square's width, on the row(s) at (the floor of) its middle
                float y0 = o.y + floor((side - s)*0.5);
                d = sdBox(q, vec2(o.x, y0), vec2(o.x + side, y0 + s));
            }
            else if(kind == 4)
            {
                // square outline
                d = sdOutline(q, o, o + vec2(side), s);
            }
            else if(kind == 6)
            {
                // rule: the quad's whole length, on the row(s) (column(s) for dir 1)
                // at (the floor of) its middle across
                if(int(vBorder + 0.5) == 0)
                {
                    float y0 = floor((size.y - s)*0.5);
                    d = sdBox(q, vec2(0.0, y0), vec2(size.x, y0 + s));
                }
                else
                {
                    float x0 = floor((size.x - s)*0.5);
                    d = sdBox(q, vec2(x0, 0.0), vec2(x0 + s, size.y));
                }
            }
            else
            {
                // restore: a front square at the bottom left, and the outline of one
                // offset up and right, only where it is outside the front square
                float off = max(floor(side/5.0 + 0.5), s + 1.0);
                vec2 frontLo = o + vec2(0.0, off);
                vec2 frontHi = o + vec2(side - off, side);
                vec2 backLo = o + vec2(off, 0.0);
                vec2 backHi = o + vec2(side, side - off);
                d = min(sdOutline(q, frontLo, frontHi, s),
                        max(sdOutline(q, backLo, backHi, s), -sdBox(q, frontLo, frontHi)));
            }
            a = clamp(0.5 - d/aa, 0.0, 1.0);
        }
        FragColor = vec4(VertexColor.rgb, VertexColor.a * a);
    }

    // a transparent image texel writes no depth, so what lies behind it still draws;
    // nor does a pixel an icon, an arc or a line leaves uncovered (a rule's quad reaches past
    // its control, over its neighbours, and a line's quad is its bounding box), whose
    // coverage beside a stroke's edge is 0 give or take rounding: less than half a
    // step of 8-bit alpha changes no pixel; nor one a rounded rect (styles 4, 6, 7)
    // leaves uncovered, outside its corner arcs, so a slot behind it drawn after it (a
    // node graph's link made after the pin it ends on) still shows around the arcs;
    // nor one a trapezoid (style 9) leaves uncovered, above its top or below its
    // bottom (its alpha, not a coverage flag of its own, which cost llvmpipe 0.4 %
    // of gweni_bench idle's flush, on every primitive's pixels)
    if(clipped || lineDeferred || offGrid || (ShaderType == 2u && FragColor.a <= 0.0)
        || (ShaderType >= 3u && ShaderType != 8u && FragColor.a < 0.5/255.0))
        discard;
    // every other style writes the depth the fixed function would
    gl_FragDepth = depth;
}
