/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  renderer::Base::measureCarets on the shared renderer gives, for each caret
 *  position of a string (its utf8Next codepoint boundaries, 0 and the end
 *  included), exactly the x measureText gives the prefix ending there, 0 for
 *  the empty prefix: the definition, which the default is. The strings are the
 *  empty one, plain ASCII, kerning pairs, multi-byte UTF-8, glyphs OpenSans
 *  lacks, a '\n' mid-string and a stray continuation byte (whose boundaries
 *  are not freetype-gl's), each in grayscale coverage, in the SDF range, in
 *  LcdRGB (grayscale where the device cannot blend it) and at scale 1.5. A case
 *  that changes the text AA mode or the scale puts it back when it ends.
 *
 *  Text::getClosestCharacter, which measures through it, answers as the
 *  per-prefix walk it replaced, written out here: measureText of each prefix,
 *  the Manhattan distance to each caret at y 0, the last of equal distances,
 *  nothing beyond 4096. Each string is probed in an unwrapped Text with padding
 *  at every caret's x and one either side, the midpoints, before and past the
 *  line, on, above and below it; and a wrapped Text of them all on each row,
 *  the row found and the answer clamped as getClosestCharacter does. In the
 *  same four modes, the scale set by Canvas::setScale.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/text.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/utility.h>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <vector>

namespace
{

using gweni::Font;
using gweni::Point;
using gweni::String;
using gweni::controls::internal::Text;
using gweni::renderer::SlotRenderer;
using gweni::renderer::TextAA;
using gweni::renderer::TextMode;

const std::vector<String> &strings()
{
    static const std::vector<String> list={
        "",
        "x",
        "Hello, world",
        "AVAWAY To Ty. LT Yo",
        "na\xc3\xafve caf\xc3\xa9 \xe2\x80\x94 \xc3\xbc\xc3\x9f",  // naïve café — üß
        "a\xe4\xb8\xad\xe6\x96\x87" "b\xf0\x9f\x98\x80" "c",       // a中文b😀c, glyphs OpenSans lacks
        "two\nlines AV\nT",
        "a\x80" "b",                                               // a stray continuation byte
    };
    return list;
}

Font openSansAt(float size)
{
    Font font=gweni::test::openSans();

    font.size=size;
    return font;
}

const char *modeName(SlotRenderer &renderer, const Font &font)
{
    if(renderer.getTextMode(font) == TextMode::Sdf)
        return "sdf";
    return (renderer.textAA() == TextAA::Grayscale)?"gray":"lcd";
}

//! measureCarets against measureText of each codepoint prefix, for every string
void checkCarets(float size)
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const Font font=openSansAt(size);

    for(const String &text:strings())
    {
        CAPTURE(text);
        CAPTURE(size);
        CAPTURE(renderer.scale());
        INFO("mode ", modeName(renderer, font));

        std::vector<int> expected={0};

        for(size_t i=gweni::utf8Next(text, 0); !text.empty(); i=gweni::utf8Next(text, i))
        {
            expected.push_back(renderer.measureText(font, text.substr(0, i)).x);

            if(i >= text.size())
                break;
        }

        std::vector<int> xs={-1, -1};  // whatever was in it is replaced

        renderer.measureCarets(font, text, xs);
        CHECK(xs == expected);
    }
}

//! measureText of each codepoint prefix of text, 0 for the empty one: what
//! main's characterPositionIn gave each caret
std::vector<int> prefixWidths(const Font &font, const String &text)
{
    std::vector<int> xs={0};

    for(size_t i=gweni::utf8Next(text, 0); !text.empty(); i=gweni::utf8Next(text, i))
    {
        xs.push_back(gweni::test::gpuRenderer().measureText(font, text.substr(0, i)).x);

        if(i >= text.size())
            break;
    }
    return xs;
}

//! main's closestCharacterIn over the carets' prefix widths: the byte index
int referenceClosest(const String &text, const std::vector<int> &xs, Point p)
{
    int distance=4096;
    size_t value=0;
    size_t k=0;

    for(size_t i=0; ; i=gweni::utf8Next(text, i), ++k)
    {
        const int d=std::abs(xs[k]-p.x)+std::abs(0-p.y);

        if(d <= distance)
        {
            distance=d;
            value=i;
        }

        if(i >= text.size())
            break;
    }
    return static_cast<int>(value);
}

//! Every caret's x and one either side, the midpoints, before and past the
//! line; on, above and below a row of rowHeight at y 0. Relative to the row.
std::vector<Point> probes(const std::vector<int> &xs, int rowHeight)
{
    std::vector<int> px={-40, -1, xs.back()+1, xs.back()+40, 5000};

    for(size_t k=0; k < xs.size(); ++k)
    {
        px.insert(px.end(), {xs[k]-1, xs[k], xs[k]+1});
        if(k+1 < xs.size())
            px.push_back((xs[k]+xs[k+1])/2);
    }

    std::vector<Point> points;

    for(int y:{-rowHeight, -1, 0, rowHeight/2, rowHeight-1, 2*rowHeight, 5000})
    {
        for(int x:px)
            points.push_back(Point(x, y));
    }
    return points;
}

//! A textured-skin canvas over the view, its scale and the renderer's put back
//! to 1 when the case ends.
struct Scene
{
    Scene()
    {
        const Point viewSize=gweni::test::viewSize();

        skin=std::make_unique<gweni::skin::textured::TexturedSkin>(&gweni::test::gpuRenderer());
        REQUIRE(skin->init("DefaultSkin.json"));
        skin->setDefaultFont("OpenSans.ttf", 11);

        canvas=std::make_unique<gweni::controls::Canvas>(skin.get());
        canvas->setSize(viewSize.x, viewSize.y);
    }

    ~Scene()
    {
        canvas->setScale(1.0f);
        canvas.reset();
        skin.reset();

        SlotRenderer &renderer=gweni::test::gpuRenderer();

        renderer.setScale(1.0f);
        renderer.begin();
        renderer.end();
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    std::unique_ptr<gweni::skin::textured::TexturedSkin> skin;
    std::unique_ptr<gweni::controls::Canvas> canvas;
};

//! getClosestCharacter against main's walk, in every string unwrapped and in
//! all of them wrapped, at the canvas scale and text AA mode set
void checkClosest(float scale, float size)
{
    Scene scene;
    const Font font=openSansAt(size);
    const gweni::Padding padding(3, 2, 5, 4);

    scene.canvas->setScale(scale);
    CAPTURE(scale);
    CAPTURE(size);
    INFO("mode ", modeName(gweni::test::gpuRenderer(), font));

    String all;
    std::vector<Text *> texts;

    for(const String &str:strings())
    {
        Text *text=scene.canvas->newChild<Text>();

        text->setFont(font);
        text->setPadding(padding);
        text->setText(str);
        texts.push_back(text);
        all+=str+" ";
    }

    gweni::controls::Base *container=scene.canvas->newChild<gweni::controls::Base>();
    container->setBounds(0, 0, static_cast<int>(size*8), 2000);

    Text *wrapped=container->newChild<Text>();

    wrapped->setFont(font);
    wrapped->setWrap(true);
    wrapped->setText(all);
    scene.frame();

    const int rowHeight=gweni::test::gpuRenderer().measureText(font, " ").y;

    REQUIRE(rowHeight > 0);

    for(Text *text:texts)
    {
        const String &str=text->getText();
        const std::vector<int> xs=prefixWidths(font, str);

        CAPTURE(str);
        for(const Point &q:probes(xs, rowHeight))
        {
            const Point p(q.x+padding.left, q.y+padding.top);

            CAPTURE(p.x);
            CAPTURE(p.y);
            CHECK(text->getClosestCharacter(p) == referenceClosest(str, xs, q));
        }
    }

    const int lines=static_cast<int>(wrapped->numLines());

    REQUIRE(lines > 2);
    for(int line=0; line < lines; ++line)
    {
        const String &row=wrapped->getLine(line)->getText();
        const std::vector<int> xs=prefixWidths(font, row);
        CAPTURE(line);
        CAPTURE(row);
        for(const Point &q:probes(xs, rowHeight))
        {
            // getClosestCharacter's row: the last starting at or above the point
            const Point p(q.x, q.y+line*rowHeight);
            const int at=(p.y < 0)?0:std::min(lines-1, p.y/rowHeight);
            const int atStart=wrapped->getStartCharFromLine(at);
            const String &atRow=wrapped->getLine(at)->getText();
            const int expected=gweni::clamp(
                atStart+referenceClosest(atRow, prefixWidths(font, atRow), Point(p.x, p.y-at*rowHeight)),
                atStart, wrapped->getLastCaretOnLine(at));

            CAPTURE(p.x);
            CAPTURE(p.y);
            CHECK(wrapped->getClosestCharacter(p) == expected);
        }
    }
}

//! Puts the shared renderer's text AA mode back when a case ends.
struct RestoreTextAA
{
    RestoreTextAA(): aa(gweni::test::gpuRenderer().textAA()) {}
    ~RestoreTextAA() { gweni::test::gpuRenderer().setTextAA(aa); }

    TextAA aa;
};

//! Puts the scale back to 1 when a case ends.
struct ScaleGuard
{
    ~ScaleGuard() { gweni::test::gpuRenderer().setScale(1.0f); }
};

}//namespace

TEST_CASE("measureCarets: each caret is measureText of its prefix, grayscale coverage")
{
    RestoreTextAA restore;
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    renderer.setTextAA(TextAA::Grayscale);

    for(float size:{11.f, 12.f, 15.f})
        checkCarets(size);
}

TEST_CASE("measureCarets: each caret is measureText of its prefix, in the SDF range")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    for(float size:{18.f, 24.f, 37.f})
    {
        if(renderer.getTextMode(openSansAt(size)) != TextMode::Sdf)
            MESSAGE("GWENI_SDF_TEXT_MIN puts ", size, " px below the SDF threshold");
        checkCarets(size);
    }
}

TEST_CASE("measureCarets: each caret is measureText of its prefix, LCD")
{
    RestoreTextAA restore;
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    renderer.setTextAA(TextAA::LcdRGB);
    if(!renderer.lcdTextAvailable())
        MESSAGE("no LCD text on this device (", renderer.lcdTextUnavailable(), "): grayscale");

    for(float size:{11.f, 12.f, 15.f})
        checkCarets(size);
}

TEST_CASE("measureCarets: each caret is measureText of its prefix, at scale 1.5")
{
    ScaleGuard guard;
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    renderer.setScale(1.5f);

    // 11 and 12 px are coverage and SDF at 1.5 (16.5 and 18 device px), 24 SDF
    for(float size:{11.f, 12.f, 24.f})
        checkCarets(size);
}

TEST_CASE("getClosestCharacter: main's per-prefix answers, grayscale coverage")
{
    RestoreTextAA restore;

    gweni::test::gpuRenderer().setTextAA(TextAA::Grayscale);
    checkClosest(1.0f, 12.0f);
}

TEST_CASE("getClosestCharacter: main's per-prefix answers, in the SDF range")
{
    checkClosest(1.0f, 24.0f);
}

TEST_CASE("getClosestCharacter: main's per-prefix answers, LCD")
{
    RestoreTextAA restore;

    gweni::test::gpuRenderer().setTextAA(TextAA::LcdRGB);
    checkClosest(1.0f, 12.0f);
}

TEST_CASE("getClosestCharacter: main's per-prefix answers, at Canvas::setScale(1.5)")
{
    // 16.5 device px, coverage, and 18, SDF
    checkClosest(1.5f, 11.0f);
    checkClosest(1.5f, 12.0f);
}
