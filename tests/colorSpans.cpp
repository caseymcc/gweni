/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Colour spans in one text run (renderText's overload with ColorSpans), on every
 *  backend, in a coverage size (11 px) and an SDF one (24 px):
 *  - a run of three words, each in a span of its own, has each glyph's vertexes
 *    in its span's colour and the glyphs in no span (the spaces) in the draw
 *    colour, and draws each word's pixels in its colour alone;
 *  - a change of the spans alone (one span's colour, a span cut shorter, every
 *    span dropped by the plain call) writes only the glyph slots whose colour
 *    changed, uploads nothing to an atlas, moves no glyph, and leaves the run
 *    what a fresh layout with the final spans gives, byte for byte;
 *  - the plain call is the layout it always was: its bytes are those of the call
 *    with no spans and with one span over the whole string in the draw colour,
 *    and a run given spans and then the plain call again is a fresh plain layout.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <cstring>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Font;
using gweni::Point;
using gweni::String;
using gweni::renderer::ColorSpan;
using gweni::renderer::SlotRenderer;

const Color black(0, 0, 0, 255);
const Color white(255, 255, 255, 255);
const Color red(255, 0, 0, 255);
const Color green(0, 255, 0, 255);
const Color blue(0, 0, 255, 255);

const String text="RED GRN BLU";
const Point origin(8, 30);
const int zIndex=5;

Font fontOfSize(float size)
{
    Font font=gweni::test::openSans();

    font.size=size;
    return font;
}

std::vector<ColorSpan> threeSpans()
{
    return { ColorSpan{0, 3, red}, ColorSpan{4, 7, green}, ColorSpan{8, 11, blue} };
}

void drawFrame(size_t textId, const Font &font, const std::vector<ColorSpan> *spans)
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    renderer.begin();
    renderer.setDrawColor(white);
    if(spans)
        renderer.renderText(textId, font, origin, zIndex, text, *spans);
    else
        renderer.renderText(textId, font, origin, zIndex, text);
    renderer.end();
}

void emptyFrame()
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    renderer.begin();
    renderer.end();
}

std::vector<uint8_t> textBytes(size_t textId)
{
    const SlotRenderer::MirrorSnapshot snapshot=gweni::test::gpuRenderer().snapshotMirror();
    const auto it=snapshot.texts.find(textId);

    REQUIRE(it != snapshot.texts.end());
    return it->second;
}

std::vector<ftgl::glyph_vertex_t> glyphVertexes(const std::vector<uint8_t> &bytes)
{
    REQUIRE(!bytes.empty());

    const size_t count=(bytes.size()-1)/sizeof(ftgl::glyph_vertex_t);
    std::vector<ftgl::glyph_vertex_t> vertexes(count);

    if(count > 0)
        memcpy(vertexes.data(), bytes.data()+1, count*sizeof(ftgl::glyph_vertex_t));
    return vertexes;
}

//! The bytes a text id that has never been drawn gets from one frame of this call.
std::vector<uint8_t> freshLayout(const Font &font, const std::vector<ColorSpan> *spans)
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    size_t textId=0;

    renderer.generateTextPrimitive(textId);
    drawFrame(textId, font, spans);

    const std::vector<uint8_t> bytes=textBytes(textId);

    renderer.releaseTextPrimitive(textId);
    emptyFrame();
    return bytes;
}

//! Byte for byte, without doctest printing both vectors when they differ
bool sameBytes(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b)
{
    return a == b;
}

bool vertexHas(const ftgl::glyph_vertex_t &vertex, const Color &color)
{
    return (vertex.r == color.r/255.0f) && (vertex.g == color.g/255.0f) &&
        (vertex.b == color.b/255.0f) && (vertex.a == 1.0f);
}

//! The colour each glyph of "RED GRN BLU" is drawn in, glyph i being byte i
//! (one glyph per ASCII codepoint, the spaces included)
std::vector<Color> glyphColors(const std::vector<ColorSpan> &spans)
{
    std::vector<Color> colors(text.size(), white);

    for(const ColorSpan &span:spans)
    {
        for(size_t i=span.begin; i < span.end; ++i)
            colors[i]=span.color;
    }
    return colors;
}

//! How many glyphs have the colour `colors` gives them in all four vertexes
size_t glyphsInColor(const std::vector<uint8_t> &bytes, const std::vector<Color> &colors)
{
    const std::vector<ftgl::glyph_vertex_t> vertexes=glyphVertexes(bytes);
    size_t matching=0;

    REQUIRE(vertexes.size() == colors.size()*4);
    for(size_t glyph=0; glyph < colors.size(); ++glyph)
    {
        bool all=true;

        for(size_t j=0; j < 4; ++j)
            all=all && vertexHas(vertexes[glyph*4+j], colors[glyph]);
        if(all)
            ++matching;
    }
    return matching;
}

//! The x of every glyph's corners, to see that no glyph moved
std::vector<float> glyphPositions(const std::vector<uint8_t> &bytes)
{
    std::vector<float> positions;

    for(const ftgl::glyph_vertex_t &vertex:glyphVertexes(bytes))
    {
        positions.push_back(vertex.x);
        positions.push_back(vertex.y);
        positions.push_back(vertex.z);
    }
    return positions;
}

}//namespace

const float sizes[2]={ 11.0f, 24.0f };

TEST_CASE("Colour spans: a run of three spans draws each glyph in its span's colour")
{
    for(float size:sizes)
    {
        SlotRenderer &renderer=gweni::test::gpuRenderer();
        const Font font=fontOfSize(size);
        const std::vector<ColorSpan> spans=threeSpans();

        CAPTURE(size);

        size_t textId=0;

        renderer.generateTextPrimitive(textId);
        gweni::test::clearView(black);
        drawFrame(textId, font, &spans);

        // the vertexes: each word's glyphs in its span's colour, the spaces white
        CHECK(glyphsInColor(textBytes(textId), glyphColors(spans)) == text.size());

        // the pixels: inside each word's columns, every lit pixel is a share of that
        // word's colour alone (the glyphs blend over black by their coverage)
        std::vector<uint8_t> pixels;

        gweni::test::readView(pixels);

        const Point view=gweni::test::viewSize();
        const int wordEnd[3]={ renderer.measureText(font, "RED").x, renderer.measureText(font, "RED GRN").x,
            renderer.measureText(font, text).x };
        const int wordStart[3]={ 0, renderer.measureText(font, "RED ").x, renderer.measureText(font, "RED GRN ").x };
        const Color colors[3]={ red, green, blue };
        const int lineHeight=renderer.lineHeight(font);

        for(int word=0; word < 3; ++word)
        {
            size_t lit=0, other=0;

            CAPTURE(word);
            for(int y=origin.y; y < origin.y+lineHeight; ++y)
            {
                for(int x=origin.x+wordStart[word]+1; x < origin.x+wordEnd[word]-1; ++x)
                {
                    const uint8_t *p=&pixels[(size_t(y)*view.x+x)*4];

                    if((p[0] == 0) && (p[1] == 0) && (p[2] == 0))
                        continue;
                    ++lit;
                    // a channel the word's colour does not have stays 0
                    if(((colors[word].r == 0) && (p[0] != 0)) || ((colors[word].g == 0) && (p[1] != 0)) ||
                        ((colors[word].b == 0) && (p[2] != 0)))
                        ++other;
                }
            }
            CHECK(lit > 20);
            CHECK(other == 0);
        }

        renderer.releaseTextPrimitive(textId);
        emptyFrame();
    }
}

//! The span changes of the case below, one at a time
enum class SpanChange
{
    OneColor,   //!< one span's colour: that word's 3 glyphs
    CutShorter, //!< a span cut shorter: the one glyph it left, now in the draw colour
    OverSpace,  //!< a span over a space: that glyph alone
    Plain       //!< the plain call: the 9 glyphs that were in a span
};

TEST_CASE("Colour spans: a change of the spans alone writes only the glyphs whose colour changed")
{
    for(float size:sizes)
    {
        for(SpanChange change:{ SpanChange::OneColor, SpanChange::CutShorter, SpanChange::OverSpace,
            SpanChange::Plain })
        {
            SlotRenderer &renderer=gweni::test::gpuRenderer();
            const Font font=fontOfSize(size);
            std::vector<ColorSpan> spans=threeSpans();
            size_t written=0;

            CAPTURE(size);
            CAPTURE(static_cast<int>(change));

            size_t textId=0;

            renderer.generateTextPrimitive(textId);
            drawFrame(textId, font, &spans);

            const std::vector<float> positions=glyphPositions(textBytes(textId));

            // a frame with the same spans writes nothing
            drawFrame(textId, font, &spans);
            REQUIRE(renderer.getFrameStats().textSlotsWritten == 0);

            switch(change)
            {
            case SpanChange::OneColor:
                spans[1].color=Color(200, 100, 50, 255);
                written=3;
                break;
            case SpanChange::CutShorter:
                spans[2].end=10;
                written=1;
                break;
            case SpanChange::OverSpace:
                spans.insert(spans.begin()+1, ColorSpan{3, 4, blue});
                written=1;
                break;
            case SpanChange::Plain:
                spans.clear();
                written=9;
                break;
            }
            drawFrame(textId, font, (change == SpanChange::Plain)?nullptr:&spans);

            const SlotRenderer::FrameStats stats=renderer.getFrameStats();
            const std::vector<uint8_t> bytes=textBytes(textId);

            CHECK(stats.textSlotsWritten == written);
            CHECK(stats.atlasUploadCalls == 0);
            CHECK(glyphPositions(bytes) == positions);
            CHECK(glyphsInColor(bytes, glyphColors(spans)) == text.size());
            // what a run laid out with the final spans holds
            CHECK(sameBytes(bytes, freshLayout(font, spans.empty()?nullptr:&spans)));

            renderer.releaseTextPrimitive(textId);
            emptyFrame();
        }
    }
}

TEST_CASE("Colour spans: the plain call lays the run out as it always did")
{
    for(float size:sizes)
    {
        SlotRenderer &renderer=gweni::test::gpuRenderer();
        const Font font=fontOfSize(size);
        const std::vector<ColorSpan> none;
        const std::vector<ColorSpan> whole={ ColorSpan{0, text.size(), white} };

        CAPTURE(size);

        const std::vector<uint8_t> plain=freshLayout(font, nullptr);

        CHECK(glyphsInColor(plain, glyphColors(none)) == text.size());
        CHECK(sameBytes(freshLayout(font, &none), plain));
        CHECK(sameBytes(freshLayout(font, &whole), plain));

        // spans and back: the plain run again
        size_t textId=0;
        const std::vector<ColorSpan> spans=threeSpans();

        renderer.generateTextPrimitive(textId);
        drawFrame(textId, font, &spans);
        REQUIRE(!sameBytes(textBytes(textId), plain));
        drawFrame(textId, font, nullptr);
        CHECK(sameBytes(textBytes(textId), plain));

        renderer.releaseTextPrimitive(textId);
        emptyFrame();
    }
}
