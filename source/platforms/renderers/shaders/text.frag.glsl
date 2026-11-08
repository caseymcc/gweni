//
//  Gweni
//  Copyright (c) 2010 Facepunch Studios
//  MIT License, see LICENSE
//
//  The glyph slots' fragment shader body, shared by every backend, behind a
//  backend's prologue (OpenGLCore::shaderSource), which defines CLIP_COLUMNS,
//  CLIP_COLUMNS_LOG2, SDF_SPREAD (SlotRenderer::kSdfSpread, how far an SDF
//  glyph's field reaches past each edge, in atlas texels) and, with a second
//  output, GWENI_DUAL_SOURCE, and declares
//
//    uniform sampler2D texture1;    the atlas being drawn
//    uniform usampler2D clipBoxes;  the glyph slots' clip boxes
//    uniform int textMode;
//    uniform float textContrast;
//
// Dual-source: FragColor is the colour, Coverage what flush() blends an LCD
// atlas's runs with (GL_SRC1_COLOR), one weight per channel. The other atlases'
// SRC_ALPHA blend does not read Coverage. Without dual-source blending there is
// no second output, and Coverage goes nowhere.
#ifdef GWENI_DUAL_SOURCE
layout(location = 0, index = 0) out vec4 FragColor;
layout(location = 0, index = 1) out vec4 Coverage;
#else
layout(location = 0) out vec4 FragColor;
vec4 Coverage;
#endif

in vec2 texCoord;
in vec4 color;
in float gamma;
in vec2 pixel;
flat in uint slot;

// mix(), spelled out, and used in its place throughout: Mesa's GLSL compiler
// and lavapipe (from glslang's FMix) lower mix() to different arithmetic (mostly
// a + (b - a)*t and a*(1 - t) + b*t), which round a value that falls on an 8-bit
// tie a level apart (primitive.frag.glsl's gradients). Written out, every
// backend computes the same, and GL its pixels as before (docs/architecture.md ->
// "Vulkan's counters and pixels against GL's").
float lerp(float a, float b, float t) { return a + (b - a)*t; }

// texture1: the atlas being drawn.
// clipBoxes: one Box (x0, y0, x1, y1) per glyph slot, slot s at texel
// (s % CLIP_COLUMNS, s / CLIP_COLUMNS). Each atlas draws its own runs, some of
// the slots of each chunk, so the vertex shader gives the slot.

uvec4 clipBox(uint slot)
{
    return texelFetch(clipBoxes, ivec2(int(slot & (CLIP_COLUMNS - 1u)), int(slot >> CLIP_COLUMNS_LOG2)), 0);
}
// textMode: the TextMode of the atlas this draw samples: 0 coverage, 1 signed
// distance field, 2 subpixel coverage in r, g, b order, 3 in b, g, r order.
// textContrast: the LCD modes' contrast boost for white text
// (GWENI_TEXT_CONTRAST); 0 is none.
// the SDF edge's anti-aliasing half-width, in screen pixels
const float SDF_AA = 0.7;
void main()
{
    uvec4 clip = clipBox(slot);
    bool clipped = any(lessThan(pixel, vec2(clip.xy))) || any(greaterThanEqual(pixel, vec2(clip.zw)));

    float a = 0.0;

    if(textMode == 0)
    {
        a = texture(texture1, texCoord).r;

        // Straight alpha: the coverage scales alpha only, and the SRC_ALPHA blend
        // applies it to the colour once. color * a here would apply it twice.
        FragColor = vec4(color.rgb, color.a * pow( a, 1.0/gamma ));
        Coverage = vec4(0.0);
    }
    else if(textMode >= 2)
    {
        // the atlas texel's r, g, b cover the left, middle and right subpixels;
        // on a BGR panel the left one is blue
        vec3 c = texture(texture1, texCoord).rgb;

        if(textMode == 3)
            c = c.bgr;
        c = pow(c, vec3(1.0/gamma));

        // Blending sRGB values, as this framebuffer does, light text over a dark
        // background comes out thinner than dark over light at the same coverage,
        // and its colour fringes stand out from its thin stems. Light text's
        // coverage is raised through DirectWrite's enhanced-contrast curve
        // c*(k+1)/(c*k+1), which keeps 0 and 1 and lifts what is between, by k
        // growing with the text's luminance; black text is left as it is.
        float k = textContrast*dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));

        c = c*(k+1.0)/(c*k+1.0);
        a = (c.r+c.g+c.b)/3.0;

        // dst = colour*coverage + dst*(1-coverage) per channel; the alpha
        // channel as the grayscale path writes it, with the mean coverage
        FragColor = vec4(color.rgb, color.a*a);
        Coverage = vec4(color.a*c, color.a*a);
    }
    else
    {
        // The field is 0.5 on the glyph's edge and changes by 1/(2*SDF_SPREAD) per
        // atlas texel; w is how much it changes over one screen pixel here, so the
        // edge is anti-aliased over about a pixel at any size, as style 4's rounded
        // rect is. Both are computed so that every GL gives the same value, which
        // llvmpipe and the browser's SwiftShader did not, by up to 78/255 at an edge:
        // - the field is filtered here, bilinearly in float from the four texels
        //   around texCoord, as GL_LINEAR does: a texture unit keeps its filter's
        //   weights to a few bits of its own, and smoothstep's narrow w multiplies
        //   that difference by up to about ten at a large size;
        // - w is the field's change per texel times texCoord's screen gradient in
        //   texels, not d's own gradient: a derivative of the sampled field is
        //   implementation-defined (fine or coarse, per 2x2 block), while texCoord
        //   is linear over the glyph's quad, so its derivative is the same anywhere.
        vec2 texels = vec2(textureSize(texture1, 0));
        vec2 t = texCoord*texels - 0.5;
        vec2 f = fract(t);
        ivec2 last = ivec2(texels) - 1;
        ivec2 i0 = clamp(ivec2(floor(t)), ivec2(0), last);
        ivec2 i1 = clamp(ivec2(floor(t)) + 1, ivec2(0), last);
        float d = lerp(lerp(texelFetch(texture1, i0, 0).r, texelFetch(texture1, ivec2(i1.x, i0.y), 0).r, f.x),
                      lerp(texelFetch(texture1, ivec2(i0.x, i1.y), 0).r, texelFetch(texture1, i1, 0).r, f.x), f.y);
        float texelsPerPixel = 0.5*(length(dFdx(texCoord)*texels) + length(dFdy(texCoord)*texels));
        float w = max(SDF_AA*texelsPerPixel/(2.0*SDF_SPREAD), 1e-4);

        a = smoothstep(0.5-w, 0.5+w, d);
        FragColor = vec4(color.rgb, color.a*a);
        Coverage = vec4(0.0);
    }

    // after the sample, as in the primitive shader: its implicit derivatives want the quad
    if(clipped)
        discard;
    // Where a glyph draws nothing it must not write depth either: an SDF glyph's quad
    // reaches the spread past its ink, over its neighbours and the next row, and a
    // coverage glyph's box overlaps the next glyph's, which in a long run can sit at
    // the same depth (glyph i is at z+(i+1)/(glyphs+1)). a is 0 exactly where every
    // mode's colour and coverage are 0.
    if(a <= 0.0)
        discard;
}
