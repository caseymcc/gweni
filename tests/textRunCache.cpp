/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  OpenGLCore keeps one glyph run per text id and skips the layout when the
 *  string has not changed. Everything else the run was built from (zIndex,
 *  draw colour, font) must still reach the glyphs: each case here changes one
 *  of them with the same string and compares the run with a fresh layout of
 *  the final inputs on a new text id. The snapshot holds the hidden flag and
 *  the glyph vertexes, not where the run sits, so the two compare byte for byte.
 *
 *  The inputs of a same-string renderText changed one at a time are zIndex,
 *  colour, font and hidden. Also: a Button's label takes the Disabled text
 *  colour and gives it back, and a Label brought to front over another (same
 *  string and position, new zIndex) has its glyphs re-depthed (#100).
 *
 *  A run that only moved is not laid out again: its vertexes get the move's
 *  delta added. After a series of odd moves away and back to the start, and on
 *  to a new place, the run must be a fresh layout where it ends each series,
 *  for SDF sizes (fractional corners, at scales 1, 1.25 and 2), coverage sizes
 *  (at 1 and 1.5) and LCD text. The SDF ones drifted by some ulps before the
 *  rebuild rounded each corner's offset to 1/256 px (#341).
 */
#include <doctest/doctest.h>

#include "capture.h"
#include "gpuRenderer.h"

#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/label.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <algorithm>
#include <cstring>
#include <set>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Font;
using gweni::Point;
using gweni::Rect;
using gweni::String;
using gweni::renderer::GLTextMode;
using gweni::renderer::OpenGLCore;
using gweni::renderer::TextAA;

const Color background(0, 0, 255, 255);
const Color colorA(255, 200, 100, 255);
const Color colorB(40, 220, 90, 255);

//! Everything one renderText call is made from.
struct TextDraw
{
    Font font;
    Point pos;
    int zIndex;
    Color color;
    String text;
};

TextDraw baseDraw()
{
    return TextDraw{ gweni::test::openSans(), Point(8, 40), 5, colorA, "Cached run" };
}

void clearFrame()
{
    glClearColor(background.r/255.0f, background.g/255.0f, background.b/255.0f, 1.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
}

//! One frame holding just this renderText.
void drawFrame(size_t textId, const TextDraw &draw)
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();

    renderer.begin();
    renderer.setDrawColor(draw.color);
    renderer.renderText(textId, draw.font, draw.pos, draw.zIndex, draw.text);
    renderer.end();
}

//! An empty frame, so the next case starts from a flushed mirror.
void emptyFrame()
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();

    renderer.begin();
    renderer.end();
}

std::vector<uint8_t> textBytes(size_t textId)
{
    const OpenGLCore::MirrorSnapshot snapshot=gweni::test::gpuRenderer().snapshotMirror();
    const auto it=snapshot.texts.find(textId);

    REQUIRE(it != snapshot.texts.end());
    return it->second;
}

//! The bytes `draw` gives when laid out on a text id that has never been drawn.
std::vector<uint8_t> freshLayout(const TextDraw &draw)
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    size_t textId=0;

    renderer.generateTextPrimitive(textId);
    drawFrame(textId, draw);

    const std::vector<uint8_t> bytes=textBytes(textId);

    renderer.releaseTextPrimitive(textId);
    emptyFrame();
    return bytes;
}

//! The glyph vertexes of a snapshot entry (past its hidden flag).
std::vector<ftgl::glyph_vertex_t> glyphVertexes(const std::vector<uint8_t> &bytes)
{
    REQUIRE(!bytes.empty());

    const size_t count=(bytes.size()-1)/sizeof(ftgl::glyph_vertex_t);
    std::vector<ftgl::glyph_vertex_t> vertexes(count);

    if(count > 0)
        memcpy(vertexes.data(), bytes.data()+1, count*sizeof(ftgl::glyph_vertex_t));
    return vertexes;
}

//! True when every glyph vertex carries `color`'s rgb, as renderText bakes it.
bool glyphsHaveColor(const std::vector<uint8_t> &bytes, const Color &color)
{
    const std::vector<ftgl::glyph_vertex_t> vertexes=glyphVertexes(bytes);

    if(vertexes.empty())
        return false;

    for(const ftgl::glyph_vertex_t &vertex:vertexes)
    {
        if((vertex.r != color.r/255.0f) || (vertex.g != color.g/255.0f) || (vertex.b != color.b/255.0f))
            return false;
    }
    return true;
}

//! The offscreen framebuffer, top-down RGBA8.
std::vector<uint8_t> readFrame()
{
    const Point size=gweni::test::viewSize();
    std::vector<uint8_t> pixels;

    gweni::capture::readFramebufferRgba(size.x, size.y, pixels);
    return pixels;
}

//! Pixels that are not the background colour.
size_t drawnPixels(const std::vector<uint8_t> &pixels)
{
    size_t drawn=0;

    for(size_t i=0; i+3 < pixels.size(); i+=4)
    {
        if((pixels[i] != background.r) || (pixels[i+1] != background.g) || (pixels[i+2] != background.b))
            ++drawn;
    }
    return drawn;
}

//! Pixels drawn by a frame holding just this renderText.
size_t drawnPixelsOf(size_t textId, const TextDraw &draw)
{
    clearFrame();
    drawFrame(textId, draw);
    return drawnPixels(readFrame());
}

}//namespace

TEST_CASE("Text run: a same-string zIndex change re-offsets the glyphs to a fresh layout's depth")
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    TextDraw draw=baseDraw();

    const std::vector<uint8_t> atFive=freshLayout(draw);
    draw.zIndex=9;
    const std::vector<uint8_t> atNine=freshLayout(draw);

    REQUIRE(atFive != atNine);

    size_t textId=0;

    renderer.generateTextPrimitive(textId);
    draw.zIndex=5;
    drawFrame(textId, draw);
    CHECK(textBytes(textId) == atFive);

    draw.zIndex=9;
    drawFrame(textId, draw);

    const std::vector<uint8_t> bytes=textBytes(textId);

    CHECK(bytes == atNine);

    // z = zIndex + the glyph's shift: every glyph in front of zIndex itself
    // (a rect drawn there is behind the whole run) and of the one before it,
    // less than one z unit away
    const std::vector<ftgl::glyph_vertex_t> vertexes=glyphVertexes(bytes);

    REQUIRE(!vertexes.empty());
    CHECK(vertexes.front().z > 9.0f);

    bool inFrontOfZIndex=true;

    for(const ftgl::glyph_vertex_t &vertex:vertexes)
        inFrontOfZIndex=inFrontOfZIndex && (vertex.z > 9.0f) && (vertex.z < 10.0f);
    CHECK(inFrontOfZIndex);

    renderer.releaseTextPrimitive(textId);
    emptyFrame();
}

TEST_CASE("Text run: a same-string colour change rebuilds the glyphs with the new colour")
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    TextDraw draw=baseDraw();

    const std::vector<uint8_t> withA=freshLayout(draw);
    draw.color=colorB;
    const std::vector<uint8_t> withB=freshLayout(draw);

    REQUIRE(glyphsHaveColor(withA, colorA));
    REQUIRE(glyphsHaveColor(withB, colorB));

    size_t textId=0;

    renderer.generateTextPrimitive(textId);
    draw.color=colorA;
    drawFrame(textId, draw);
    CHECK(textBytes(textId) == withA);

    draw.color=colorB;
    drawFrame(textId, draw);
    CHECK(textBytes(textId) == withB);
    CHECK(glyphsHaveColor(textBytes(textId), colorB));

    draw.color=colorA;
    drawFrame(textId, draw);
    CHECK(textBytes(textId) == withA);

    SUBCASE("alpha is not baked in, so an alpha-only change rebuilds nothing")
    {
        drawFrame(textId, draw);
        REQUIRE(renderer.getFrameStats().textSlotsWritten == 0);

        draw.color.a=40;
        drawFrame(textId, draw);
        CHECK(renderer.getFrameStats().textSlotsWritten == 0);
        CHECK(textBytes(textId) == withA);
    }

    renderer.releaseTextPrimitive(textId);
    emptyFrame();
}

TEST_CASE("Text run: a same-string font change rebuilds the glyphs in the new font")
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    Font larger=gweni::test::openSans();

    larger.size=17;
    REQUIRE((renderer.loadFont(larger) == Font::Status::Loaded));

    TextDraw draw=baseDraw();

    const std::vector<uint8_t> inBase=freshLayout(draw);
    draw.font=larger;
    const std::vector<uint8_t> inLarger=freshLayout(draw);

    REQUIRE(inBase != inLarger);

    size_t textId=0;

    renderer.generateTextPrimitive(textId);
    draw.font=gweni::test::openSans();
    drawFrame(textId, draw);
    CHECK(textBytes(textId) == inBase);

    draw.font=larger;
    drawFrame(textId, draw);
    CHECK(textBytes(textId) == inLarger);

    draw.font=gweni::test::openSans();
    drawFrame(textId, draw);
    CHECK(textBytes(textId) == inBase);

    renderer.releaseTextPrimitive(textId);
    emptyFrame();
    renderer.freeFont(larger);
}

TEST_CASE("Text run: a hidden run stays hidden through a re-offset and a rebuild")
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    TextDraw draw=baseDraw();
    size_t textId=0;

    renderer.generateTextPrimitive(textId);
    REQUIRE(drawnPixelsOf(textId, draw) > 20);

    renderer.hideText(textId);
    CHECK(drawnPixelsOf(textId, draw) == 0);

    SUBCASE("zIndex re-offset")
    {
        draw.zIndex=9;
    }

    SUBCASE("colour rebuild")
    {
        draw.color=colorB;
    }

    SUBCASE("zIndex and colour together")
    {
        draw.zIndex=9;
        draw.color=colorB;
    }

    CHECK(drawnPixelsOf(textId, draw) == 0);

    const std::vector<uint8_t> hidden=textBytes(textId);

    REQUIRE(!hidden.empty());
    CHECK(hidden[0] == 1);

    renderer.showText(textId);
    CHECK(drawnPixelsOf(textId, draw) > 20);
    CHECK(textBytes(textId) == freshLayout(draw));

    renderer.releaseTextPrimitive(textId);
    emptyFrame();
}

TEST_CASE("Text run: the same call in the next frame writes and uploads nothing")
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    const TextDraw draw=baseDraw();
    size_t textId=0;

    renderer.generateTextPrimitive(textId);
    drawFrame(textId, draw);
    REQUIRE(renderer.getFrameStats().textSlotsWritten > 0);

    drawFrame(textId, draw);
    CHECK(renderer.getFrameStats().textSlotsWritten == 0);
    CHECK(renderer.getFrameStats().primitiveSlotsWritten == 0);
    CHECK(renderer.getFrameStats().uploadBytes == 0);
    CHECK(renderer.getFrameStats().atlasUploadBytes == 0);

    renderer.releaseTextPrimitive(textId);
    emptyFrame();
}

TEST_CASE("Text run: a Button's label takes the Disabled text colour and gives it back")
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    const Point viewSize=gweni::test::viewSize();

    std::set<size_t> textIdsBefore;

    for(const auto &entry:renderer.snapshotMirror().texts)
        textIdsBefore.insert(entry.first);

    gweni::skin::textured::TexturedSkin *skin=new gweni::skin::textured::TexturedSkin(&renderer);

    // No setDefaultFont("OpenSans.ttf", 11) here: the label draws in the
    // skin's own default ("OpenSans", 11, a separate entry), and the skin
    // frees its default font when it is deleted, which would free the shared
    // test::openSans() with it (the renderer keeps one entry per equal Font,
    // with no reference count).
    REQUIRE(skin->init("DefaultSkin.json"));

    gweni::controls::Canvas *canvas=new gweni::controls::Canvas(skin);

    canvas->setSize(viewSize.x, viewSize.y);

    gweni::controls::Button *button=canvas->newChild<gweni::controls::Button>();

    button->setText("Disable me");
    button->setBounds(10, 10, 160, 30);

    const auto frame=[canvas]()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    };

    // the label's run is the one text id this canvas made
    const auto labelBytes=[&]()
    {
        std::vector<std::vector<uint8_t>> runs;

        for(const auto &entry:renderer.snapshotMirror().texts)
        {
            if(textIdsBefore.count(entry.first) == 0)
                runs.push_back(entry.second);
        }
        REQUIRE(runs.size() == 1);
        return runs.front();
    };

    const Color normal=skin->Colors.Button.Normal;
    const Color disabled=skin->Colors.Button.Disabled;

    REQUIRE(!(normal == disabled));

    frame();
    frame();
    CHECK(glyphsHaveColor(labelBytes(), normal));

    button->setDisabled(true);
    frame();
    CHECK(glyphsHaveColor(labelBytes(), disabled));

    button->setDisabled(false);
    frame();
    CHECK(glyphsHaveColor(labelBytes(), normal));

    delete canvas;
    delete skin;
    emptyFrame();
}

TEST_CASE("Text run: a label brought to front re-depths its glyphs with the same string and position")
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    const Point viewSize=gweni::test::viewSize();

    std::set<size_t> textIdsBefore;

    for(const auto &entry:renderer.snapshotMirror().texts)
        textIdsBefore.insert(entry.first);

    gweni::skin::textured::TexturedSkin *skin=new gweni::skin::textured::TexturedSkin(&renderer);

    // the skin's own default font, as in the Button case above
    REQUIRE(skin->init("DefaultSkin.json"));

    gweni::controls::Canvas *canvas=new gweni::controls::Canvas(skin);

    canvas->setSize(viewSize.x, viewSize.y);

    // Overlapping, like two windows: only their order changes below.
    gweni::controls::Label *back=canvas->newChild<gweni::controls::Label>();
    gweni::controls::Label *front=canvas->newChild<gweni::controls::Label>();

    back->setText("AAAA");
    front->setText("BBBBBBBB");
    for(gweni::controls::Label *label:{back, front})
    {
        label->setAlignment(gweni::Alignment::Absolute);
        label->setBounds(10, 10, 160, 30);
    }

    const auto frame=[canvas]()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    };

    // the run of this canvas's text ids that has `glyphs` glyphs
    const auto runOf=[&](size_t glyphs)
    {
        std::vector<std::vector<ftgl::glyph_vertex_t>> runs;

        for(const auto &entry:renderer.snapshotMirror().texts)
        {
            if(textIdsBefore.count(entry.first) != 0)
                continue;

            std::vector<ftgl::glyph_vertex_t> vertexes=glyphVertexes(entry.second);

            if(vertexes.size() == glyphs*4)
                runs.push_back(vertexes);
        }
        REQUIRE(runs.size() == 1);
        return runs.front();
    };

    gweni::controls::Base *backText=back->getChildren().front();
    gweni::controls::Base *frontText=front->getChildren().front();

    frame();
    frame();

    const int backZ=backText->getZIndex();
    const Rect backBounds=backText->getBounds();
    const std::vector<ftgl::glyph_vertex_t> before=runOf(4);

    REQUIRE(backZ < frontText->getZIndex());
    // each run just in front of its label's z
    auto atZ=[](const std::vector<ftgl::glyph_vertex_t> &run, int z)
        {
            return !run.empty() && run.front().z > float(z) && run.back().z < float(z+1);
        };

    CHECK(atZ(before, backZ));

    back->bringToFront();
    frame();

    // same string, same place; only the depth moved, in front of the other label
    REQUIRE((backText->getBounds() == backBounds));
    REQUIRE(backText->getZIndex() > frontText->getZIndex());

    const std::vector<ftgl::glyph_vertex_t> after=runOf(4);

    REQUIRE(after.size() == before.size());
    CHECK(atZ(after, backText->getZIndex()));
    CHECK(atZ(runOf(8), frontText->getZIndex()));

    bool samePlace=true;

    for(size_t i=0; i < after.size(); ++i)
        samePlace=samePlace && (after[i].x == before[i].x) && (after[i].y == before[i].y);
    CHECK(samePlace);

    // and a frame after it writes nothing
    frame();
    CHECK(renderer.getFrameStats().textSlotsWritten == 0);

    delete canvas;
    delete skin;
    emptyFrame();
}

namespace
{

//! Puts the shared renderer's scale and text AA mode back when a case ends.
struct RestoreScaleAndAA
{
    RestoreScaleAndAA(): aa(gweni::test::gpuRenderer().textAA()) {}
    ~RestoreScaleAndAA()
    {
        OpenGLCore &renderer=gweni::test::gpuRenderer();

        renderer.setScale(1.0f);
        renderer.setTextAA(aa);
    }

    TextAA aa;
};

//! A run drawn at a font size and canvas scale, in the mode that gives.
struct MoveSetup
{
    float size;
    float scale;
    TextAA aa;
    GLTextMode mode;
};

const char *modeName(GLTextMode mode)
{
    switch(mode)
    {
    case GLTextMode::Sdf: return "sdf";
    case GLTextMode::LcdRGB: return "rgb";
    case GLTextMode::LcdBGR: return "bgr";
    default: return "coverage";
    }
}

//! Odd steps in canvas units, both signs on both axes, ending where they began.
const std::vector<Point> awayAndBack={ Point(37, -23), Point(-11, 5), Point(3, 29), Point(-53, -7),
    Point(19, 13), Point(5, -17) };

//! Odd steps to somewhere new.
const std::vector<Point> toNewPlace={ Point(13, 7), Point(-5, 21), Point(31, -3), Point(-9, -15),
    Point(7, 11), Point(23, 1), Point(-1, 9) };

//! The vertex fields that differ between two snapshot entries (every one if
//! their sizes differ), so a failure prints a count, not two byte dumps.
size_t differingFields(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b)
{
    if(a.size() != b.size())
        return std::max(a.size(), b.size())/sizeof(float);

    size_t differing=(a.empty() || (a[0] == b[0]))?0:1;

    for(size_t i=1; i+sizeof(float) <= a.size(); i+=sizeof(float))
    {
        if(memcmp(&a[i], &b[i], sizeof(float)) != 0)
            ++differing;
    }
    return differing;
}

//! Moves the run of `draw` on one text id by each step and returns its bytes
//! where it ends. Each renderText of the same string takes the move path and
//! adds its delta to the mirror, so the steps share one frame: the arithmetic
//! is the same as a frame each, without a frame's cost per step under ASAN.
std::vector<uint8_t> moveThrough(size_t textId, TextDraw &draw, const std::vector<Point> &steps)
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();

    renderer.begin();
    renderer.setDrawColor(draw.color);
    for(const Point &step:steps)
    {
        draw.pos=draw.pos+step;
        renderer.renderText(textId, draw.font, draw.pos, draw.zIndex, draw.text);
    }
    renderer.end();
    return textBytes(textId);
}

}//namespace

TEST_CASE("Text run: a moved run is the bytes of a fresh layout where it ends, after any number of moves")
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    const RestoreScaleAndAA restore;

    // An SDF run is scaled by px/48 from the 48 px atlas, so its quad corners
    // are fractions of a pixel that are not dyadic at 22 px, 25 px (20 at scale
    // 1.25) and 22 px (11 at scale 2, the skin's default font in ControlTest's
    // clip scenario at --scale 2); at 24 px they are (s = 1/2). 11 px at 1 and 1.5 is
    // coverage text, on whole pixels, and the LCD modes shift its glyphs by
    // thirds of a pixel. Each move must land exactly where a fresh layout does.
    const MoveSetup setups[]={
        { 22.0f, 1.0f, TextAA::Grayscale, GLTextMode::Sdf },
        { 24.0f, 1.0f, TextAA::Grayscale, GLTextMode::Sdf },
        { 20.0f, 1.25f, TextAA::Grayscale, GLTextMode::Sdf },
        { 11.0f, 2.0f, TextAA::Grayscale, GLTextMode::Sdf },
        { 11.0f, 1.0f, TextAA::Grayscale, GLTextMode::Coverage },
        { 11.0f, 1.5f, TextAA::Grayscale, GLTextMode::Coverage },
        { 11.0f, 1.0f, TextAA::LcdRGB, GLTextMode::LcdRGB },
        { 11.0f, 1.5f, TextAA::LcdRGB, GLTextMode::LcdRGB },
    };

    for(const MoveSetup &setup:setups)
    {
        renderer.setTextAA(setup.aa);
        renderer.setScale(setup.scale);

        TextDraw draw=baseDraw();

        draw.font.size=setup.size;
        draw.text="Inside the Window, moved";

        const String mode=modeName(setup.mode);

        CAPTURE(setup.size);
        CAPTURE(setup.scale);
        CAPTURE(mode);

        // GWENI_SDF_TEXT_MIN or GWENI_TEXT_AA may put this size in another mode
        if(renderer.getTextMode(draw.font) != setup.mode)
        {
            MESSAGE("skipped: ", draw.font.size, " px at scale ", setup.scale, " is not ",
                modeName(setup.mode), " here");
            continue;
        }

        size_t textId=0;
        const Point start=draw.pos;

        renderer.generateTextPrimitive(textId);
        drawFrame(textId, draw);

        const std::vector<uint8_t> atStart=textBytes(textId);

        REQUIRE(differingFields(atStart, freshLayout(draw)) == 0);

        const std::vector<uint8_t> back=moveThrough(textId, draw, awayAndBack);

        REQUIRE(((draw.pos.x == start.x) && (draw.pos.y == start.y)));
        CHECK(differingFields(back, atStart) == 0);

        const std::vector<uint8_t> moved=moveThrough(textId, draw, toNewPlace);

        CAPTURE(draw.pos.x);
        CAPTURE(draw.pos.y);
        CHECK(differingFields(moved, atStart) > 0);
        CHECK(differingFields(moved, freshLayout(draw)) == 0);

        renderer.releaseTextPrimitive(textId);
        emptyFrame();
    }
}
