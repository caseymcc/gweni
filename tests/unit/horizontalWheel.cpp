/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Canvas maps Shift+wheel to horizontal for every input translator, while
 *  an explicit horizontal wheel stays horizontal. Base bubbles that axis to
 *  the nearest ScrollControl whose shown, enabled horizontal bar can move;
 *  a vertical-only, fitting or exhausted one passes it to its parent.
 *
 *  Existing IInputEvent implementers need no horizontal override (the
 *  default returns false). GLFW sends both nonzero axes, rounded independently
 *  at 60 units a notch, even when the first axis was consumed.
 */
#include "headlessFixture.h"

#include <gweni/controls/scrollControl.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/input/glfw3.h>

#include <doctest/doctest.h>

#include <utility>
#include <vector>

using namespace gweni;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas canvas{&skin};
    controls::ScrollControl *scroll;
    controls::Base *content;

    Scene()
    {
        canvas.setSize(400, 300);
        scroll=canvas.newChild<controls::ScrollControl>();
        scroll->setDock(DockPosition::Center);
        content=scroll->newChild<controls::Base>();
        content->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        content->setSize(800, 600);
        frame();
        frame();
    }

    ~Scene()
    {
        canvas.inputModifierKey(keys::Shift, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
    }

    void frame()
    {
        canvas.updateCanvas();
        canvas.renderCanvas();
    }
};

//! Implements every required hook from before the horizontal wheel existed.
struct LegacyReceiver: public IInputEvent
{
    bool inputMouseMoved(int, int, int, int) override { return false; }
    bool inputMouseButton(int, bool) override { return false; }
    bool inputModifierKey(int, bool) override { return false; }
    bool inputCharacter(UnicodeChar) override { return false; }
    bool inputMouseWheel(int) override { return false; }
    bool inputQuit() override { return false; }
};

struct WheelReceiver: public LegacyReceiver
{
    std::vector<std::pair<char, int>> events;
    bool takeVertical=false;
    bool takeHorizontal=false;

    bool inputMouseWheel(int val) override
    {
        events.emplace_back('Y', val);
        return takeVertical;
    }

    bool inputMouseWheelH(int val) override
    {
        events.emplace_back('X', val);
        return takeHorizontal;
    }
};

}//namespace

TEST_CASE("Horizontal wheel: Canvas maps Shift+wheel to horizontal")
{
    Scene scene;
    REQUIRE(scene.scroll->getMaxHorizontalScrollOffset() > 0);
    REQUIRE(scene.scroll->getMaxVerticalScrollOffset() > 0);
    scene.canvas.inputMouseMoved(30, 30, 0, 0);
    REQUIRE(HoveredControl == scene.content);
    scene.canvas.inputModifierKey(keys::Shift, true);
    REQUIRE(input::isShiftDown());

    CHECK(scene.canvas.inputMouseWheel(-60));
    scene.frame();
    CHECK(scene.scroll->getHorizontalScrollOffset() > 0);
    CHECK(scene.scroll->getVerticalScrollOffset() == 0);

    // A native horizontal event stays horizontal with Shift still held.
    const int shifted=scene.scroll->getHorizontalScrollOffset();
    CHECK(scene.canvas.inputMouseWheelH(-60));
    scene.frame();
    CHECK(scene.scroll->getHorizontalScrollOffset() > shifted);
    CHECK(scene.scroll->getVerticalScrollOffset() == 0);

    scene.canvas.inputModifierKey(keys::Shift, false);
    const int horizontal=scene.scroll->getHorizontalScrollOffset();
    CHECK(scene.canvas.inputMouseWheel(-60));
    scene.frame();
    CHECK(scene.scroll->getHorizontalScrollOffset() == horizontal);
    CHECK(scene.scroll->getVerticalScrollOffset() > 0);

    // An explicit horizontal event also reaches the control without Shift.
    const int vertical=scene.scroll->getVerticalScrollOffset();
    CHECK(scene.canvas.inputMouseWheelH(60));
    scene.frame();
    CHECK(scene.scroll->getHorizontalScrollOffset() < horizontal);
    CHECK(scene.scroll->getVerticalScrollOffset() == vertical);
}

TEST_CASE("Horizontal wheel: bubbles to the nearest horizontal ScrollControl that can move")
{
    Scene scene;
    auto *nested=scene.content->newChild<controls::ScrollControl>();
    nested->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    nested->setSize(300, 180);
    auto *child=nested->newChild<controls::Base>();
    child->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    child->setSize(600, 400);

    bool useOuter=false;

    SUBCASE("the nearest horizontal bar moves") {}
    SUBCASE("a vertical-only ScrollControl passes horizontal on")
    {
        nested->setScroll(false, true);
        useOuter=true;
    }
    SUBCASE("a fitting horizontal bar is hidden")
    {
        child->setWidth(100);
        useOuter=true;
    }
    SUBCASE("a permanent fitting horizontal bar is disabled")
    {
        child->setWidth(100);
        nested->setAutoHideBars(false);
        useOuter=true;
    }
    SUBCASE("the nearest bar at its right end passes horizontal on")
    {
        scene.frame();
        nested->setHorizontalScrollOffset(nested->getMaxHorizontalScrollOffset());
        useOuter=true;
    }
    scene.frame();
    REQUIRE(nested->getMaxVerticalScrollOffset() > 0);
    const int before=nested->getHorizontalScrollOffset();

    CHECK(child->onMouseWheeledH(-60));
    scene.frame();
    if(useOuter)
    {
        CHECK(nested->getHorizontalScrollOffset() == before);
        CHECK(scene.scroll->getHorizontalScrollOffset() > 0);
    }
    else
    {
        CHECK(nested->getHorizontalScrollOffset() > before);
        CHECK(scene.scroll->getHorizontalScrollOffset() == 0);
    }
    CHECK(nested->getVerticalScrollOffset() == 0);
    CHECK(scene.scroll->getVerticalScrollOffset() == 0);

    // At the left end of every ancestor, neither can use a leftward wheel.
    nested->setHorizontalScrollOffset(0);
    scene.scroll->setHorizontalScrollOffset(0);
    scene.frame();
    CHECK_FALSE(child->onMouseWheeledH(60));
    scene.frame();
    CHECK(nested->getHorizontalScrollOffset() == 0);
    CHECK(scene.scroll->getHorizontalScrollOffset() == 0);
}

TEST_CASE("Horizontal wheel: IInputEvent remains compatible with a vertical-only implementer")
{
    LegacyReceiver receiver;
    IInputEvent &interface=receiver;

    CHECK_FALSE(interface.inputMouseWheelH(-60));
    CHECK_FALSE(interface.inputMouseWheelH(60));
    CHECK_FALSE(interface.inputMouseWheelH(0));
}

TEST_CASE("Horizontal wheel: GLFW delivers both rounded axes independently")
{
    WheelReceiver receiver;
    input::GLFW3 glfw;
    glfw.initialize(&receiver);

    SUBCASE("both axes are delivered when vertical was consumed")
    {
        receiver.takeVertical=true;
        CHECK(glfw.processScroll(-0.26f, 0.26f));
        REQUIRE(receiver.events.size() == 2);
        CHECK(receiver.events[0].first == 'Y');
        CHECK(receiver.events[0].second == 16);
        CHECK(receiver.events[1].first == 'X');
        CHECK(receiver.events[1].second == -16);
    }
    SUBCASE("a horizontal-only event is consumed by its receiver")
    {
        receiver.takeHorizontal=true;
        CHECK(glfw.processScroll(-1.0f, 0.0f));
        REQUIRE(receiver.events.size() == 1);
        CHECK(receiver.events[0].first == 'X');
        CHECK(receiver.events[0].second == -60);
    }
    SUBCASE("both axes can be unconsumed")
    {
        CHECK_FALSE(glfw.processScroll(0.26f, -0.26f));
        REQUIRE(receiver.events.size() == 2);
        CHECK(receiver.events[0].first == 'Y');
        CHECK(receiver.events[0].second == -16);
        CHECK(receiver.events[1].first == 'X');
        CHECK(receiver.events[1].second == 16);
    }
    SUBCASE("a zero or sub-unit wheel emits nothing")
    {
        CHECK_FALSE(glfw.processScroll(0.0f, 0.0f));
        CHECK_FALSE(glfw.processScroll(0.001f, -0.001f));
        CHECK(receiver.events.empty());
    }
    SUBCASE("an old receiver still consumes no horizontal event")
    {
        LegacyReceiver legacy;
        glfw.initialize(&legacy);
        CHECK_FALSE(glfw.processScroll(1.0f, 0.0f));
    }
    SUBCASE("before initialization there is no receiver")
    {
        input::GLFW3 uninitialized;
        CHECK_FALSE(uninitialized.processScroll(1.0f, 1.0f));
    }
}
