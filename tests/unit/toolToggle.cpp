/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The tools' open/close/toggle/find over a closing instance (#502), for each
 *  of tools::Inspector, tools::ThemeEditor and tools::StatsOverlay, on a real
 *  Canvas with a StubSkin under a stepped canvas clock. A closed tool is
 *  hidden and left for its delayed delete, and from then on it is not the
 *  canvas's (Canvas::isPendingDelete()):
 *  - find() is null right after close();
 *  - two toggles in one frame leave one shown instance, a new one, and the
 *    closed one is deleted in the next frame;
 *  - closed by its close button (setDeleteOnClose, the Inspector and the
 *    ThemeEditor), a toggle() in the same frame opens a new, shown instance
 *    rather than showing the doomed one again;
 *  - once everything has settled with the tool closed, every primitive and
 *    text id it took is given back.
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/controls/canvas.h>
#include <gweni/platforms/platform.h>
#include <gweni/tools/inspector.h>
#include <gweni/tools/statsOverlay.h>
#include <gweni/tools/themeEditor.h>

#include <doctest/doctest.h>

#include <vector>

using namespace gweni;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    float clock=10.0f;

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(1024, 768);
        canvas->newChild<controls::Base>("app")->setBounds(10, 10, 300, 200);
        frames(3);
    }

    ~Scene()
    {
        HoveredControl=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    void frames(int count)
    {
        for(int i=0; i < count; ++i)
        {
            clock+=1.0f/64.0f;
            canvas->updateCanvas();
            canvas->renderCanvas();
        }
    }

    //! The canvas's children of type Tool, the closing ones included.
    template<class Tool>
    std::vector<Tool *> instances() const
    {
        std::vector<Tool *> found;

        for(controls::Base *child : canvas->getActualChildren())
        {
            if(Tool *tool=gweni_cast<Tool>(child))
                found.push_back(tool);
        }
        return found;
    }

    //! A click on tool's close button, through the canvas's input.
    void clickCloseButton(controls::Base *tool)
    {
        controls::Base *button=tool->findChildByName("closeButton", true);

        REQUIRE(button);

        const Point at=button->localPosToCanvas();
        const int x=at.x+button->getWidth()/2;
        const int y=at.y+button->getHeight()/2;

        canvas->inputMouseMoved(x, y, 0, 0);
        canvas->inputMouseButton(0, true);
        canvas->inputMouseButton(0, false);
        canvas->inputMouseMoved(-1, -1, 0, 0);
    }
};

template<class Tool>
void checkToggles(bool closeButton)
{
    Scene scene;
    const test::IdBankCounts before=test::IdBankCounts::now();

    SUBCASE("find() is null right after close()")
    {
        Tool *tool=Tool::open(scene.canvas);

        REQUIRE(tool);
        CHECK(Tool::find(scene.canvas) == tool);
        scene.frames(2);
        Tool::close(scene.canvas);
        CHECK(Tool::find(scene.canvas) == nullptr);
        CHECK(scene.canvas->isPendingDelete(tool));
        // closing again does nothing more
        Tool::close(scene.canvas);
        CHECK(Tool::find(scene.canvas) == nullptr);
        scene.frames(2);
        CHECK(scene.instances<Tool>().empty());
    }
    SUBCASE("two toggles in one frame leave one shown instance, a new one")
    {
        REQUIRE(Tool::toggle(scene.canvas));
        scene.frames(2);

        Tool *first=Tool::find(scene.canvas);

        REQUIRE(first);

        const unsigned firstId=first->getControlId();

        CHECK_FALSE(Tool::toggle(scene.canvas));
        CHECK(Tool::find(scene.canvas) == nullptr);
        CHECK(Tool::toggle(scene.canvas));

        Tool *second=Tool::find(scene.canvas);

        REQUIRE(second);
        CHECK(second != first);
        CHECK(second->visible());
        scene.frames(2);

        const std::vector<Tool *> left=scene.instances<Tool>();

        REQUIRE(left.size() == 1);
        CHECK(left.front() == second);
        CHECK(left.front()->getControlId() != firstId);
        CHECK(Tool::find(scene.canvas) == second);
        CHECK(second->visible());
        Tool::close(scene.canvas);
        scene.frames(2);
    }
    if(closeButton)
    {
        SUBCASE("closed by its close button, a toggle opens a new shown one")
        {
            Tool *first=Tool::open(scene.canvas);

            REQUIRE(first);
            scene.frames(2);
            scene.clickCloseButton(first);
            CHECK_FALSE(first->visible());
            CHECK(scene.canvas->isPendingDelete(first));
            CHECK(Tool::find(scene.canvas) == nullptr);
            CHECK(Tool::toggle(scene.canvas));

            Tool *second=Tool::find(scene.canvas);

            REQUIRE(second);
            CHECK(second != first);
            CHECK(second->visible());
            scene.frames(2);
            REQUIRE(scene.instances<Tool>().size() == 1);
            CHECK(Tool::find(scene.canvas) == second);
            CHECK(second->visible());
            CHECK_FALSE(Tool::toggle(scene.canvas));
            scene.frames(2);
        }
    }

    // everything closed and settled: every id back
    CHECK(Tool::find(scene.canvas) == nullptr);
    CHECK(scene.instances<Tool>().empty());
    scene.frames(2);

    const test::IdBankCounts after=test::IdBankCounts::now();

    CHECK(after.livePrimitives() == before.livePrimitives());
    CHECK(after.liveTexts() == before.liveTexts());
    CHECK(scene.renderer.doubleReleased == 0);
}

}//namespace

TEST_CASE("Tool toggles skip a closing instance: Inspector")
{
    checkToggles<tools::Inspector>(true);
}

TEST_CASE("Tool toggles skip a closing instance: ThemeEditor")
{
    checkToggles<tools::ThemeEditor>(true);
}

TEST_CASE("Tool toggles skip a closing instance: StatsOverlay")
{
    checkToggles<tools::StatsOverlay>(false);
}
