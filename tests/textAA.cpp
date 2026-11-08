/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  SlotRenderer's text AA modes (setTextAA, GWENI_TEXT_AA): Grayscale draws one
 *  coverage per pixel from an R8 atlas, LcdRGB/LcdBGR one per subpixel from a
 *  depth-3 atlas, blended per channel through the text shader's second output.
 *
 *  White "l" and "I" at 12 px on black are read back in each mode: on an RGB
 *  panel a stem's left edge pixel is covered on its right, blue, side and its
 *  right edge pixel on its red side, so blue leads on the left and red on the
 *  right; BGR is the mirror; grayscale has red equal to blue everywhere. A
 *  switch of mode at runtime must draw what a renderer started in that mode
 *  draws, and nothing laid out in the old mode.
 *
 *  This is the coverage path: when GWENI_SDF_TEXT_MIN puts 12 px in the SDF
 *  range, which stays grayscale, the cases are skipped.
 *
 *  The LCD modes' atlas is depth 3 / RGB8. The runtime switch is checked as
 *  gray -> rgb -> gray on the shared renderer and on a canvas switched at
 *  runtime, byte for byte against fresh renderers started in each mode. A case
 *  that changes the mode puts it back when it ends.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/label.h>
#include <gweni/platforms/commonPlatform.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <memory>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Font;
using gweni::Point;
using gweni::String;
using gweni::renderer::TextMode;
using gweni::renderer::SlotRenderer;
using gweni::renderer::TextAA;

const Color white(255, 255, 255, 255);

//! Where each glyph is drawn, a column band of the view to itself
const int glyphLeft[2]={20, 60};
const char *const glyphText[2]={"l", "I"};
const int bandWidth=30;

Font at12()
{
    Font font=gweni::test::openSans();

    font.size=12;
    return font;
}

const char *name(TextAA aa)
{
    return (aa == TextAA::LcdRGB)?"rgb":(aa == TextAA::LcdBGR)?"bgr":"gray";
}

//! Puts the shared renderer's mode back when a case ends.
struct RestoreTextAA
{
    RestoreTextAA(): aa(gweni::test::gpuRenderer().textAA()) {}
    ~RestoreTextAA() { gweni::test::gpuRenderer().setTextAA(aa); }

    TextAA aa;
};

std::vector<uint8_t> readFrame()
{
    std::vector<uint8_t> pixels;

    gweni::test::readView(pixels);
    return pixels;
}

const uint8_t *pixelAt(const std::vector<uint8_t> &frame, int x, int y)
{
    return &frame[(static_cast<size_t>(y)*gweni::test::viewSize().x+x)*4];
}

bool inked(const uint8_t *p)
{
    return (p[0] != 0) || (p[1] != 0) || (p[2] != 0);
}

size_t inkedPixels(const std::vector<uint8_t> &frame)
{
    size_t count=0;

    for(size_t i=0; i < frame.size(); i+=4)
        count+=inked(&frame[i])?1:0;
    return count;
}

void clearFrame()
{
    gweni::test::clearView(gweni::Color(0, 0, 0, 255));
}

//! The two glyphs' text ids on one renderer, released (and that flushed) at the end.
struct Glyphs
{
    explicit Glyphs(SlotRenderer &renderer): renderer(renderer)
    {
        for(size_t &id:ids)
            renderer.generateTextPrimitive(id);
    }

    ~Glyphs()
    {
        for(size_t id:ids)
            renderer.releaseTextPrimitive(id);
        renderer.begin();
        renderer.end();
    }

    //! One frame of both glyphs in white on black, read back
    std::vector<uint8_t> draw()
    {
        clearFrame();
        renderer.begin();
        renderer.setDrawColor(white);
        for(int i=0; i < 2; ++i)
            renderer.renderText(ids[i], at12(), Point(glyphLeft[i], 40), 1, glyphText[i]);
        renderer.end();
        return readFrame();
    }

    SlotRenderer &renderer;
    size_t ids[2]={0, 0};
};

//! A renderer of its own, over the same view, started in one mode.
struct FreshRenderer
{
    explicit FreshRenderer(TextAA aa):
        paths("resources"),
        owned(gweni::test::newRenderer(paths, gweni::Rect(Point(0, 0), gweni::test::viewSize()))),
        renderer(*owned)
    {
        renderer.init();
        // explicitly, so GWENI_TEXT_AA in the environment takes no part
        renderer.setTextAA(aa);
    }

    gweni::platform::CurrentPath paths;
    std::unique_ptr<SlotRenderer> owned;
    SlotRenderer &renderer;
};

bool coverageRange()
{
    if(gweni::test::gpuRenderer().getTextMode(at12()) != TextMode::Sdf)
        return true;
    MESSAGE("skipped: GWENI_SDF_TEXT_MIN draws 12 px from the SDF atlas, which is grayscale");
    return false;
}

}//namespace

TEST_CASE("Text AA: an LCD mode loads a depth-3 RGB8 atlas of its own, grayscale an R8 one")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    RestoreTextAA restore;

    if(!coverageRange())
        return;

    for(TextAA aa:{TextAA::Grayscale, TextAA::LcdRGB, TextAA::LcdBGR, TextAA::Grayscale})
    {
        CAPTURE(name(aa));
        renderer.setTextAA(aa);
        REQUIRE(renderer.textAA() == aa);
        REQUIRE(renderer.ensureFont(at12()));

        const bool lcd=(aa != TextAA::Grayscale);
        int bits[4]={-1, -1, -1, -1};

        CHECK(renderer.getTextMode(at12()) == ((aa == TextAA::LcdRGB)?TextMode::LcdRGB:
            (aa == TextAA::LcdBGR)?TextMode::LcdBGR:TextMode::Coverage));
        CHECK(renderer.getFontAtlasDepth(at12()) == (lcd?3u:1u));

        // RGB8 or R8, as the framebuffer it is attached to reports its channels
        REQUIRE(renderer.textureChannelSizes(renderer.getFontAtlasHandle(at12()), bits));
        CHECK(bits[0] == 8);
        CHECK(bits[1] == (lcd?8:0));
        CHECK(bits[2] == (lcd?8:0));
        CHECK(bits[3] == 0);
        // the font loaded before the switch is loaded again in the new mode
        CHECK(renderer.getFontAtlasHandle(gweni::test::openSans()) != 0);
    }
    renderer.freeFont(at12());
}

TEST_CASE("Text AA: a switch keeps each font's owners, so it takes as many freeFonts to go as before")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    RestoreTextAA restore;

    if(!coverageRange())
        return;

    renderer.setTextAA(TextAA::Grayscale);

    const size_t fontsBefore=renderer.getLoadedFontCount();

    // two owners in gray
    REQUIRE((renderer.loadFont(at12()) == Font::Status::Loaded));
    REQUIRE((renderer.loadFont(at12()) == Font::Status::Loaded));

    for(TextAA aa:{TextAA::LcdRGB, TextAA::Grayscale, TextAA::LcdBGR})
    {
        CAPTURE(name(aa));
        renderer.setTextAA(aa);
        CHECK(renderer.getFontAtlasHandle(at12()) != 0);
        CHECK(renderer.getLoadedFontCount() == fontsBefore+1);
    }

    // the first free leaves it to the second owner, in the new mode
    renderer.freeFont(at12());

    const unsigned int atlas=renderer.getFontAtlasHandle(at12());

    CHECK(atlas != 0);
    CHECK(renderer.getFontAtlasDepth(at12()) == 3u);

    // a switch with one owner left keeps that one
    renderer.setTextAA(TextAA::Grayscale);
    CHECK(renderer.getFontAtlasHandle(at12()) != 0);
    CHECK(renderer.getFontAtlasDepth(at12()) == 1u);

    renderer.freeFont(at12());
    CHECK(renderer.getFontAtlasHandle(at12()) == 0);
    CHECK(renderer.getLoadedFontCount() == fontsBefore);

    // a font only used, never loaded, has no owner to keep: one free takes it
    REQUIRE(renderer.ensureFont(at12()));
    renderer.setTextAA(TextAA::LcdRGB);
    CHECK(renderer.getFontAtlasHandle(at12()) != 0);
    renderer.freeFont(at12());
    CHECK(renderer.getFontAtlasHandle(at12()) == 0);
}

TEST_CASE("Text AA: a stem's edges fringe blue-left/red-right in rgb, mirrored in bgr, and not in gray")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    RestoreTextAA restore;

    if(!coverageRange())
        return;

    for(TextAA aa:{TextAA::Grayscale, TextAA::LcdRGB, TextAA::LcdBGR})
    {
        CAPTURE(name(aa));
        renderer.setTextAA(aa);

        Glyphs glyphs(renderer);
        const std::vector<uint8_t> frame=glyphs.draw();
        const Point size=gweni::test::viewSize();

        REQUIRE(inkedPixels(frame) > 20); // the premise: something was drawn

        if(aa == TextAA::Grayscale)
        {
            int coloured=0;

            for(int y=0; y < size.y; ++y)
                for(int x=0; x < size.x; ++x)
                {
                    const uint8_t *p=pixelAt(frame, x, y);

                    coloured+=(p[0] != p[2])?1:0;
                }
            CHECK(coloured == 0);
            continue;
        }

        // the channel the left subpixel of a pixel is, and the right one's
        const int leftChannel=(aa == TextAA::LcdRGB)?0:2;
        const int rightChannel=2-leftChannel;

        for(int i=0; i < 2; ++i)
        {
            CAPTURE(glyphText[i]);

            int rows=0;
            int wrong=0;

            for(int y=0; y < size.y; ++y)
            {
                int left=-1, right=-1;

                for(int x=glyphLeft[i]-5; x < glyphLeft[i]+bandWidth; ++x)
                {
                    if(!inked(pixelAt(frame, x, y)))
                        continue;
                    if(left < 0)
                        left=x;
                    right=x;
                }
                if(left < 0)
                    continue;

                const uint8_t *l=pixelAt(frame, left, y);
                const uint8_t *r=pixelAt(frame, right, y);

                ++rows;
                // the left edge pixel is covered on its right side, the right one on its left
                if(!(l[rightChannel] > l[leftChannel]) || !(r[leftChannel] > r[rightChannel]))
                {
                    if(wrong < 4)
                        MESSAGE("row ", y, ": left (", left, ") rgb ", (int)l[0], ",", (int)l[1], ",", (int)l[2],
                            ", right (", right, ") rgb ", (int)r[0], ",", (int)r[1], ",", (int)r[2]);
                    ++wrong;
                }
            }

            CHECK(rows >= 8); // the premise: a stem the height of a 12 px letter
            CHECK(wrong == 0);
        }
    }
}

TEST_CASE("Text AA: gray -> rgb -> gray at runtime draws what fresh renderers in each mode draw")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    RestoreTextAA restore;

    if(!coverageRange())
        return;

    std::vector<uint8_t> freshGray, freshRgb;

    {
        FreshRenderer fresh(TextAA::Grayscale);
        Glyphs glyphs(fresh.renderer);

        freshGray=glyphs.draw();
    }
    {
        FreshRenderer fresh(TextAA::LcdRGB);
        Glyphs glyphs(fresh.renderer);

        freshRgb=glyphs.draw();
    }

    REQUIRE(inkedPixels(freshGray) > 20);
    REQUIRE(freshRgb != freshGray);

    renderer.setTextAA(TextAA::Grayscale);

    Glyphs glyphs(renderer);
    const std::vector<uint8_t> gray0=glyphs.draw();

    renderer.setTextAA(TextAA::LcdRGB);

    // nothing laid out in the old mode is drawn: a frame without renderText is empty
    clearFrame();
    renderer.begin();
    renderer.end();
    CHECK(inkedPixels(readFrame()) == 0);

    const std::vector<uint8_t> rgb=glyphs.draw();

    renderer.setTextAA(TextAA::Grayscale);

    const std::vector<uint8_t> gray1=glyphs.draw();

    CHECK(gray0 == freshGray);
    CHECK(rgb == freshRgb);
    CHECK(gray1 == freshGray);
}

namespace
{

//! A textured-skin canvas over the whole view: a Label sized to its text and a Button.
struct CanvasScene
{
    explicit CanvasScene(SlotRenderer &renderer)
    {
        skin=new gweni::skin::textured::TexturedSkin(&renderer);
        REQUIRE(skin->init("DefaultSkin.json"));

        canvas=new gweni::controls::Canvas(skin);
        canvas->setSize(gweni::test::viewSize().x, gweni::test::viewSize().y);
        canvas->setPadding(gweni::Padding(10, 10, 10, 10));

        gweni::controls::Label *label=canvas->newChild<gweni::controls::Label>();

        label->setText("Illumination Tester 0123 minimum");
        label->sizeToContents();
        label->setPos(10, 10);

        gweni::controls::Button *button=canvas->newChild<gweni::controls::Button>();

        button->setText("Button");
        button->setBounds(10, 50, 120, 30);
    }

    ~CanvasScene()
    {
        delete canvas;
        delete skin;
    }

    std::vector<uint8_t> frame()
    {
        clearFrame();
        canvas->updateCanvas();
        canvas->renderCanvas();
        return readFrame();
    }

    gweni::skin::textured::TexturedSkin *skin;
    gweni::controls::Canvas *canvas;
};

std::vector<uint8_t> freshCanvasFrame(TextAA aa)
{
    FreshRenderer fresh(aa);
    CanvasScene scene(fresh.renderer);

    scene.frame();
    return scene.frame();
}

}//namespace

TEST_CASE("Text AA: a switch at runtime re-lays a canvas's whole tree, its next frame a fresh start's")
{
    if(!coverageRange())
        return;

    const std::vector<uint8_t> freshGray=freshCanvasFrame(TextAA::Grayscale);
    const std::vector<uint8_t> freshRgb=freshCanvasFrame(TextAA::LcdRGB);
    const std::vector<uint8_t> freshBgr=freshCanvasFrame(TextAA::LcdBGR);

    REQUIRE(freshRgb != freshGray);
    REQUIRE(freshBgr != freshRgb);

    FreshRenderer switched(TextAA::Grayscale);
    CanvasScene scene(switched.renderer);

    scene.frame();
    CHECK(scene.frame() == freshGray);

    for(TextAA aa:{TextAA::LcdRGB, TextAA::LcdBGR, TextAA::Grayscale})
    {
        CAPTURE(name(aa));
        switched.renderer.setTextAA(aa);

        const std::vector<uint8_t> &fresh=(aa == TextAA::LcdRGB)?freshRgb:(aa == TextAA::LcdBGR)?freshBgr:freshGray;

        CHECK(scene.frame() == fresh);
        // and it stays so, with nothing left to do
        CHECK(scene.frame() == fresh);
        CHECK(switched.renderer.getFrameStats().textSlotsWritten == 0);
    }
}
