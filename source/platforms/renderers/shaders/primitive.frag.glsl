//
//  Gweni
//  Copyright (c) 2010 Facepunch Studios
//  MIT License, see LICENSE
//
//  The primitive slots' fragment shader body, shared by every backend, behind a
//  backend's prologue (OpenGLCore::shaderSource, Vulkan/primitive.frag), which
//  defines CLIP_COLUMNS and CLIP_COLUMNS_LOG2 and declares the samplers:
//
//    uniform sampler2D images[8];   style 2's image units
//    uniform usampler2D clipBoxes;  the slots' clip boxes
//
out vec4 FragColor;

in vec2 TexCoords;
in vec4 VertexColor;
flat in uint ShaderType;
in vec2 vLocal;
flat in vec2 vHalf;
flat in float vRadius;
flat in float vBorder;
flat in vec4 vColor1;
flat in vec4 vBorderColor;
in vec4 vColor3;
flat in vec4 vSegment;
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

// images[8]: style 2's textures, one per image unit (vRadius picks it). GLSL 3.30
// and ES 3.00 index a sampler array only with a constant, hence the switch.
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

// Rounded-rect mask of styles 4, 6 and 7: applies the border to fill and the
// anti-aliased outer edge to its alpha. Both are a 1 px box filter: the part of
// the pixel on the inner side of the edge, with aa the size of a pixel in units
// of d. A pixel-aligned straight edge is then exactly 1 inside and 0 outside, so
// the outermost pixel is fully covered, two rects that abut meet without a seam
// and an N px border is N whole rows; only a curved edge (a corner arc) gets
// partial coverage.
vec4 roundedRect(vec4 fill)
{
    float r = clamp(vRadius, 0.0, min(vHalf.x, vHalf.y));
    float d = sdRoundBox(vLocal, vHalf, r);
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
        float bMix = clamp(0.5 + (vBorder + d)/aa, 0.0, 1.0);
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

// Image unit `unit` at uv as GL_LINEAR_MIPMAP_LINEAR with GL_CLAMP_TO_EDGE samples
// it, with uv's screen gradients dx and dy (taken in uniform control flow),
// computed so that every GL gives the same value, which textureGrad did not
// (llvmpipe against the browser's SwiftShader, by up to 4/255 in ImagePanelDemo):
// the levels are made on the CPU (imageMipChain), and the LOD and the weights are
// the GL spec's, computed in float here:
// - the LOD is log2 of the longer of the two gradients in level 0's texels,
//   rounded to 1/256: uv is linear over the image's quad, so its gradients are
//   the same anywhere and on any GL up to a float error the rounding removes;
// - at or below 0 (magnification) it is level 0, bilinear: the 4 texels around
//   uv, clamped to the level's edge, weighted by uv's place between their
//   centres to 1/256; above, the 4 of each of the 2 levels around it, the levels
//   mixed by its fraction; past the last level (1x1) that level's 4.
// Each tap reads one texel exactly: textureLod at its centre and its level's
// whole LOD, the image's filter nearest (OpenGLCore::createImageTexture). Not
// texelFetch, which reads the same texel: llvmpipe runs every branch of a shader
// for every pixel, masked, and skips a sample whose pixels are all masked off but
// not a texelFetch, so with 8 of them here a page without an image took 5.7 %
// more instructions a frame than with textureGrad (ButtonDemo), and 2.4 % now.
// GLSL 3.30 and ES 3.00 index a sampler array only with a constant, hence the
// switches; the taps are one loop whose count is not a constant, so that no
// compiler unrolls it into 8 switches.
vec4 imageTexel(int unit, vec2 uv, vec2 dx, vec2 dy)
{
    ivec2 size0 = ivec2(1);

    switch(unit)
    {
    case 0: size0 = textureSize(images[0], 0); break;
    case 1: size0 = textureSize(images[1], 0); break;
    case 2: size0 = textureSize(images[2], 0); break;
    case 3: size0 = textureSize(images[3], 0); break;
    case 4: size0 = textureSize(images[4], 0); break;
    case 5: size0 = textureSize(images[5], 0); break;
    case 6: size0 = textureSize(images[6], 0); break;
    case 7: size0 = textureSize(images[7], 0); break;
    }

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
        vec2 centre = (vec2(p) + 0.5)/vec2(size);
        float texelLod = float(level);
        vec4 texel = vec4(0.0);

        switch(unit)
        {
        case 0: texel = textureLod(images[0], centre, texelLod); break;
        case 1: texel = textureLod(images[1], centre, texelLod); break;
        case 2: texel = textureLod(images[2], centre, texelLod); break;
        case 3: texel = textureLod(images[3], centre, texelLod); break;
        case 4: texel = textureLod(images[4], centre, texelLod); break;
        case 5: texel = textureLod(images[5], centre, texelLod); break;
        case 6: texel = textureLod(images[6], centre, texelLod); break;
        case 7: texel = textureLod(images[7], centre, texelLod); break;
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
    bool clipped = any(lessThan(vPixel, vec2(clip.xy))) || any(greaterThanEqual(vPixel, vec2(clip.zw)));
    vec2 uvDx = dFdx(TexCoords);
    vec2 uvDy = dFdy(TexCoords);

    if(ShaderType == 0u)
        FragColor = vec4(0.0, 0.0, 0.0, 0.0);
    else if(ShaderType == 1u)
        FragColor = VertexColor;
    else if (ShaderType == 2u)
    {
        // Image: its texture's unit is in vRadius, the draw colour (tint) in vColor1.
        FragColor = imageTexel(int(vRadius + 0.5), TexCoords, uvDx, uvDy) * vColor1;
    }
    else if (ShaderType == 3u)
    {
        // Line: a capsule vBorder px thick around the segment vSegment.xy-vSegment.zw
        // (vLocal is in device px). A pixel is covered by the part of it between the
        // stroke's two edges, each taken as a straight edge across the pixel along
        // the normal from the nearest point of the segment (radial in a cap): the
        // pixel's exact area for the body of the stroke, so a thin slanted line is
        // as bright on every row. drawLine puts the stroke's edges of a horizontal
        // or vertical segment on pixel boundaries, where this is style 4's box
        // filter: whole fully covered rows (columns) and nothing beside them.
        vec2 pa = vLocal - vSegment.xy;
        vec2 ba = vSegment.zw - vSegment.xy;
        float h = clamp(dot(pa, ba)/max(dot(ba, ba), 1e-8), 0.0, 1.0);
        vec2 v = pa - ba*h;
        float d = length(v);
        vec2 n = d > 1e-4 ? v/d
            : (dot(ba, ba) > 1e-8 ? normalize(vec2(-ba.y, ba.x)) : vec2(0.0, 1.0));
        float radius = 0.5*vBorder;
        float a = clamp(pixelArea(radius - d, n) + pixelArea(radius + d, n) - 1.0, 0.0, 1.0);
        FragColor = vec4(VertexColor.rgb, VertexColor.a * a);
        // A partly covered pixel's depth is moved toward the layer behind, up to half
        // a layer as its coverage goes to 0. The segments of a polyline share a z, and
        // with GL_LESS the first one's anti-aliased cap rim would otherwise keep the
        // next one off pixels it covers fully; this way a later segment draws over the
        // pixels an earlier one covers less, still in front of every lower z, while
        // a fully covered pixel keeps its z, so a translucent polyline is blended once
        // there however many of its segments cover it.
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
    else if (ShaderType == 5u)
    {
        // Icon glyph: vRadius=kind, vBorder=dir (the chevron only).
        int kind = int(vRadius + 0.5);
        float a;

        if(kind <= 1)
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
            else
            {
                // chevron "v" pointing down (rotated by dir above).
                vec2 A = vec2(-0.5, -0.2);
                vec2 B = vec2(0.0, 0.28);
                vec2 C = vec2(0.5, -0.2);
                d = min(sdSegment(p, A, B), sdSegment(p, B, C)) - 0.13;
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
    // nor does a pixel an icon or a line leaves uncovered (a rule's quad reaches past
    // its control, over its neighbours, and a line's quad is its bounding box), whose
    // coverage beside a stroke's edge is 0 give or take rounding: less than half a
    // step of 8-bit alpha changes no pixel
    if(clipped || (ShaderType == 2u && FragColor.a <= 0.0)
        || ((ShaderType == 3u || ShaderType == 5u) && FragColor.a < 0.5/255.0))
        discard;
    // every other style writes the depth the fixed function would
    gl_FragDepth = depth;
}
