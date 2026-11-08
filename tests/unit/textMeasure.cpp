/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  internal::Text keeps what it measured: the string's size, the font's line
 *  height and, wrapped, its longest word, each until one of its inputs
 *  changes (issue #324). Counted on a StubRenderer that records every string
 *  it measures, on a real Canvas:
 *  - a Label is measured once, however many passes and calculateSize calls
 *    ask (both axes, a forced re-layout, three nested ScrollControls each
 *    measuring its children again);
 *  - setText with a new string, setFont with a new Font, the same Font
 *    mutated in place, setWrap there and back, textChanged() and the
 *    renderer's textLayoutChanged() each cost exactly one new measure, and
 *    setText with the same string none;
 *  - Canvas::setScale to 2 and back measures nothing and gives the same
 *    bounds: measureText() is in canvas units at every scale, so the scale is
 *    no part of the key;
 *  - a skin with another renderer measures again, one with the same renderer
 *    does not;
 *  - an empty Text asks for the line height once and is that plus padding
 *    high, also once a character is typed and deleted (#268);
 *  - a wrapped Text whose parent has no width request measures its longest
 *    word once,
 *    not on every X pass, and again for a new string;
 *  - a settled frame measures nothing.
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/controls/label.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/text.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <vector>

using namespace gweni;
using controls::internal::Text;

namespace
{

//! A StubRenderer recording the strings it measures, whose
//! textLayoutChanged() a test can raise.
struct MeasureRenderer: public test::StubRenderer
{
    using gweni::renderer::Base::textLayoutChanged;

    gweni::Point measureText(const gweni::Font &font, const gweni::String &text) override
    {
        measured.push_back(text);
        return StubRenderer::measureText(font, text);
    }

    //! How many times `text` was measured since the last clear().
    size_t count(const gweni::String &text) const
    {
        return static_cast<size_t>(std::count(measured.begin(), measured.end(), text));
    }

    void clear() { measured.clear(); }

    std::vector<gweni::String> measured;
};

struct Scene
{
    MeasureRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
    }

    ~Scene()
    {
        delete canvas;
    }

    //! One frame; how many measureText() calls the renderer saw in it.
    size_t frame()
    {
        const size_t before=renderer.measures;

        canvas->updateCanvas();
        canvas->renderCanvas();
        return renderer.measures-before;
    }

    const controls::LayoutStats &stats() const { return canvas->getLayoutStats(); }
};

bool sameRect(const Rect &a, const Rect &b)
{
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

//! A Label's own Text: its only internal::Text child.
Text *textOf(controls::Base *label)
{
    for(controls::Base *child : label->getChildren())
    {
        if(Text *text=dynamic_cast<Text *>(child))
            return text;
    }
    return nullptr;
}

}//namespace

TEST_CASE("Text measure: a Label is measured once over passes and in both axes")
{
    Scene scene;
    controls::Label *label=scene.canvas->newChild<controls::Label>();

    label->setText("measured once");
    label->setPos(10, 10);

    CHECK(scene.frame() == 1);
    CHECK(scene.renderer.count("measured once") == 1);
    CHECK(scene.stats().measureCalls == 1);

    Text *text=textOf(label);
    REQUIRE(text != nullptr);

    const Rect bounds=label->getBounds();

    // a pass that enters the Text again (calculateSize in X and Y) measures nothing
    text->invalidate();
    CHECK(scene.frame() == 0);
    CHECK(scene.stats().passes >= 1);
    CHECK(scene.stats().visited > 0);
    CHECK(sameRect(label->getBounds(), bounds));

    // a settled frame neither
    CHECK(scene.frame() == 0);
    CHECK(scene.stats().measureCalls == 0);
}

TEST_CASE("Text measure: a Label under three nested ScrollControls is measured once")
{
    Scene scene;
    controls::ScrollControl *outer=scene.canvas->newChild<controls::ScrollControl>("outer");

    outer->setBounds(0, 0, 300, 250);

    controls::ScrollControl *middle=outer->newChild<controls::ScrollControl>("middle");

    middle->setBounds(0, 0, 250, 200);

    controls::ScrollControl *inner=middle->newChild<controls::ScrollControl>("inner");

    inner->setBounds(0, 0, 200, 150);

    controls::Label *label=inner->newChild<controls::Label>();

    label->setText("deep label");
    label->setPos(5, 5);

    scene.frame();
    CHECK(scene.renderer.count("deep label") == 1);

    // relaid out from the top, through every ScrollControl's own re-measure
    scene.renderer.clear();
    textOf(label)->invalidate();
    outer->invalidate();
    middle->invalidate();
    inner->invalidate();
    scene.frame();
    CHECK(scene.renderer.count("deep label") == 0);
}

TEST_CASE("Text measure: each change of an input costs one measure, the same string none")
{
    Scene scene;
    Text *text=scene.canvas->newChild<Text>();

    text->setText("first");
    text->setPos(10, 10);
    CHECK(scene.frame() == 1);

    SUBCASE("setText with a new string")
    {
        text->setText("second string");
        CHECK(scene.frame() == 1);
        CHECK(scene.renderer.count("second string") == 1);
        CHECK(text->getWidth() == scene.renderer.StubRenderer::measureText(text->getFont(), "second string").x);
    }

    SUBCASE("setText with the same string")
    {
        text->setText("first");
        CHECK(scene.frame() == 0);
    }

    SUBCASE("setFont with a new Font")
    {
        Font font=text->getFont();

        font.size*=2;
        text->setFont(font);
        CHECK(scene.frame() == 1);
        CHECK(text->getHeight() == scene.renderer.StubRenderer::measureText(font, "first").y);
        // the Font must outlive the Text's use of it
        text->setFont(scene.skin.getDefaultFont());
        scene.frame();
    }

    SUBCASE("the same Font mutated in place")
    {
        Font font=text->getFont();

        text->setFont(font);
        CHECK(scene.frame() == 1);

        const int width=text->getWidth();

        font.size*=2;
        text->invalidate();
        CHECK(scene.frame() == 1);
        CHECK(text->getWidth() != width);
        CHECK(text->getWidth() == scene.renderer.StubRenderer::measureText(font, "first").x);
        text->setFont(scene.skin.getDefaultFont());
        scene.frame();
    }

    SUBCASE("setWrap there and back")
    {
        text->setWrap(true);
        text->setWrap(false);
        CHECK(scene.frame() == 1);
    }

    SUBCASE("textChanged()")
    {
        text->textChanged();
        text->invalidate();
        CHECK(scene.frame() == 1);
    }

    SUBCASE("the renderer's textLayoutChanged()")
    {
        scene.renderer.textLayoutChanged();
        CHECK(scene.frame() == 1);
        CHECK(scene.renderer.count("first") == 2);
    }

    // and then it is kept again
    CHECK(scene.frame() == 0);
}

TEST_CASE("Text measure: Canvas::setScale to 2 and back measures nothing")
{
    Scene scene;
    controls::Label *label=scene.canvas->newChild<controls::Label>();

    label->setText("scaled label");
    label->setPos(20, 30);

    controls::internal::Text *empty=scene.canvas->newChild<Text>();

    empty->setPos(20, 80);
    scene.frame();

    const Rect labelBounds=label->getBounds();
    const Rect textBounds=textOf(label)->getBounds();
    const Rect emptyBounds=empty->getBounds();

    scene.canvas->setScale(2.0f);
    CHECK(scene.frame() == 0);
    CHECK(scene.stats().passes >= 1);
    CHECK(sameRect(label->getBounds(), labelBounds));
    CHECK(sameRect(textOf(label)->getBounds(), textBounds));
    CHECK(sameRect(empty->getBounds(), emptyBounds));

    scene.canvas->setScale(1.0f);
    CHECK(scene.frame() == 0);
    CHECK(sameRect(label->getBounds(), labelBounds));
    CHECK(sameRect(empty->getBounds(), emptyBounds));
}

TEST_CASE("Text measure: a skin with another renderer measures again, the same renderer does not")
{
    Scene scene;
    Text *text=scene.canvas->newChild<Text>();

    text->setText("skinned");
    text->setPos(10, 10);
    scene.frame();

    SUBCASE("another renderer")
    {
        MeasureRenderer other;
        test::StubSkin otherSkin{&other};

        scene.canvas->setSkin(&otherSkin);
        scene.frame();
        CHECK(other.count("skinned") == 1);

        // kept by the new renderer too
        text->invalidate();
        scene.frame();
        CHECK(other.count("skinned") == 1);

        // give the first skin back before the second goes
        scene.canvas->setSkin(&scene.skin);
        scene.frame();
    }

    SUBCASE("the same renderer")
    {
        test::StubSkin sameRenderer{&scene.renderer};

        scene.canvas->setSkin(&sameRenderer);
        scene.renderer.clear();
        scene.frame();
        CHECK(scene.renderer.count("skinned") == 0);

        scene.canvas->setSkin(&scene.skin);
        scene.frame();
    }
}

TEST_CASE("Text measure: an empty Text asks for the line height once and is that plus padding high")
{
    Scene scene;
    Text *text=scene.canvas->newChild<Text>();

    text->setPadding(Padding(2, 3, 4, 5));
    text->setPos(10, 10);

    const size_t lineHeights=scene.renderer.lineHeights;

    scene.frame();
    CHECK(scene.renderer.lineHeights-lineHeights == 1);

    const int lineHeight=scene.renderer.StubRenderer::lineHeight(text->getFont());

    CHECK(text->getWidth() == 1+2+4);
    CHECK(text->getHeight() == lineHeight+3+5);

    text->invalidate();
    CHECK(scene.frame() == 0);

    text->setText("x");
    CHECK(scene.frame() == 1);
    CHECK(text->getHeight() == lineHeight+3+5);

    // the line height is the font's, still kept: the empty string measures nothing
    text->setText("");
    CHECK(scene.frame() == 0);
    CHECK(text->getWidth() == 1+2+4);
    CHECK(text->getHeight() == lineHeight+3+5);
}

TEST_CASE("Text measure: a wrapped Text measures its longest word once, and again for a new string")
{
    Scene scene;
    // placed, but no width asked of it: the Text asks for its longest word
    controls::Base *box=scene.canvas->newChild<controls::Base>();

    box->setPos(10, 10);

    Text *text=box->newChild<Text>();

    text->setWrap(true);
    text->setText("one three fifteen");
    scene.frame();
    scene.frame();

    REQUIRE_FALSE(box->hasRequestedSize(Dim::X));
    CHECK(text->getPreferredSize().width == scene.renderer.StubRenderer::measureText(text->getFont(), "fifteen").x);

    // an X pass asks for the longest word again: kept, as the wrap is
    scene.renderer.clear();
    text->invalidate();
    box->invalidate();
    CHECK(scene.frame() == 0);
    CHECK(scene.frame() == 0);

    // a new string with a longer word
    text->setText("one three seventeenth");
    scene.frame();
    CHECK(scene.renderer.count("seventeenth") >= 1);
    CHECK(text->getPreferredSize().width
        == scene.renderer.StubRenderer::measureText(text->getFont(), "seventeenth").x);

    scene.renderer.clear();
    text->invalidate();
    CHECK(scene.frame() == 0);
}
