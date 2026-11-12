/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  What a new size invalidates (#578), on a real Canvas with the StubSkin. A
 *  control given a new size invalidates the children it holds (its inner
 *  panel included), not its subtree: below them, each control whose own
 *  bounds the arrange changes invalidates its children in turn. A canvas
 *  resized so invalidates only its own children.
 *
 *  - A MenuStrip docked in a short panel is as high as its items whatever
 *    the canvas's height: it does not take the clamp to the canvas's height a
 *    Menu opened on the canvas has (Menu::calculateSize), which would read a
 *    size from beyond its parent that a canvas resize no longer invalidates
 *    it for. Shrunk below the strip and grown again, the canvas lays it out
 *    with no layout-verify difference.
 */
#include "headlessFixture.h"

#include <gweni/controls/menuStrip.h>

#include <doctest/doctest.h>

using namespace gweni;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;

    explicit Scene(const Point &size)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(size.x, size.y);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void settle()
    {
        for(int frame=0; frame<3; ++frame)
            this->frame();
    }
};

//! Layout verification on for the scope, and the failures it found in it.
struct Verify
{
    Verify(): m_outer(controls::layoutVerifyEnabled()), m_failures(controls::layoutVerifyFailures())
    {
        controls::setLayoutVerify(true);
    }
    ~Verify() { controls::setLayoutVerify(m_outer); }

    size_t failures() const { return controls::layoutVerifyFailures()-m_failures; }

private:
    bool m_outer;
    size_t m_failures;
};

}//namespace

TEST_CASE("Size invalidation: a MenuStrip in a short panel is as high as its items at any canvas height")
{
    Verify verify;
    Scene scene(Point(400, 300));

    controls::Base *panel=scene.canvas->newChild<controls::Base>();
    panel->setDock(DockPosition::Top);
    panel->setHeight(60);

    controls::MenuStrip *strip=panel->newChild<controls::MenuStrip>();
    strip->addItem("File");
    strip->addItem("Edit");
    scene.settle();

    const int height=strip->getHeight();

    REQUIRE(height > 12);
    CHECK(strip->getPreferredSize().height == height);

    // a canvas shorter than the strip, then as tall as before
    scene.canvas->setSize(400, height/2);
    scene.settle();
    CHECK(strip->getHeight() == height);
    CHECK(strip->getPreferredSize().height == height);

    scene.canvas->setSize(400, 300);
    scene.settle();
    CHECK(strip->getHeight() == height);
    CHECK(verify.failures() == 0);
}
