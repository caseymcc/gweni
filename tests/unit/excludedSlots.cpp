/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Canvas::excludedSlots(), the one record of the tools' renderer ids that
 *  every tool on a canvas reads, on a real Canvas with a StubSkin whose
 *  Controls draw a rect into each of their slots whenever their control
 *  changes (and whose renderer counts a released slot as written, as
 *  SlotRenderer's flush clears it), under a stepped canvas clock:
 *  - it holds the ids every setExcludedFromStats() subtree on the canvas holds
 *    (a tool's on a page too), and none of the app's; with no flagged control
 *    it is empty;
 *  - an id a flagged subtree gives back (a control of it deleted, between
 *    frames or in the update phase) stays in it through the update phase of
 *    the frame after the flush that cleared it, whoever reads it and however
 *    often, and is gone the frame after; a tool deleted whole counts so too;
 *  - with a StatsOverlay and an Inspector both open, a frame only one of them
 *    wrote in reads 0 slots in the other: the overlay's slots row over a
 *    window of frames the Inspector wrote in, the Inspector's slots line on
 *    the overlay's refresh frames, and both over the frames a flagged
 *    control's deletion clears its slots in;
 *  - closing either tool, the other reads 0 over the frames its slots are
 *    hidden and cleared in;
 *  - the overlay's ids row is the same with the Inspector open as without:
 *    the live ids less every tool's;
 *  - an app change shows in both, on the next refresh (the overlay) or frame
 *    (the Inspector), and then 0.
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/platforms/platform.h>
#include <gweni/tools/inspector.h>
#include <gweni/tools/statsOverlay.h>
#include <gweni/tools/toolSupport.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using namespace gweni;
using tools::Inspector;
using tools::StatsOverlay;

namespace
{

//! A StubControl that draws a rect into each of its slots when its control
//! has a state change, so the StubRenderer counts them as written.
class DrawingControl: public test::StubControl
{
public:
    using StubControl::StubControl;

    void update(renderer::Base *renderer, controls::Base *control) override
    {
        if(control->getStateChange() == controls::StateChange_Nothing)
            return;
        for(size_t id : getPrimitives())
            renderer->drawFilledRect(id, control->getRenderBounds(), control->getZIndex());
    }
};

class DrawingSkin: public test::StubSkin
{
public:
    using StubSkin::StubSkin;

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        const bool takesText=typeName == test::StubText::getTypeNameStatic();

        return std::make_unique<DrawingControl>(this, takesText, std::move(typeName));
    }
};

// The overlay's rows, as StatsOverlay::lines() lists them.
enum Line
{
    HeaderLine, FrameLine, CpuLine, UpdateLine, RenderLine, FlushLine,
    FpsLine, IdsLine, MaxHeaderLine, RenderedLine, LayoutLine, ZLine, SlotsLine, UploadsLine
};

const String NoSlots="prim slots | 0 | glyphs | 0";
const String InspectorNoSlots="slots written: 0 primitive, 0 text";

struct Scene
{
    test::StubRenderer renderer;
    DrawingSkin skin{&renderer};
    controls::Canvas *canvas;
    controls::Base *app;
    float clock=10.0f;

    static constexpr float Step=1.0f/64.0f;

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(1024, 768);
        app=canvas->newChild<controls::Base>("app");
        app->setBounds(10, 10, 300, 200);
        app->newChild<controls::Base>()->setBounds(0, 0, 50, 20);
        frames(3);
    }

    ~Scene()
    {
        HoveredControl=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    void frame()
    {
        clock+=Step;
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void frames(int count)
    {
        for(int i=0; i < count; ++i)
            frame();
    }

    //! Frames until the overlay's next refresh has run, and how many.
    int untilRefresh(StatsOverlay *overlay)
    {
        const size_t refreshes=overlay->refreshes();
        int count=0;

        while((overlay->refreshes() == refreshes) && (count < 100))
        {
            frame();
            ++count;
        }
        return count;
    }
};

std::vector<size_t> sorted(std::vector<size_t> ids)
{
    std::sort(ids.begin(), ids.end());
    return ids;
}

//! The primitive ids control's subtree holds, sorted.
std::vector<size_t> primitiveIdsOf(const controls::Base *control)
{
    std::vector<size_t> primitives;
    std::vector<size_t> texts;

    tools::appendSubtreeIds(control, primitives, texts);
    return sorted(primitives);
}

bool allExcluded(controls::Canvas *canvas, const std::vector<size_t> &ids)
{
    for(size_t id : ids)
    {
        if(!canvas->excludedSlots().primitive(id))
            return false;
    }
    return true;
}

bool noneExcluded(controls::Canvas *canvas, const std::vector<size_t> &ids)
{
    for(size_t id : ids)
    {
        if(canvas->excludedSlots().primitive(id))
            return false;
    }
    return true;
}

//! A plain flagged subtree: a tool of the test's own, with two children.
controls::Base *newTool(controls::Base *parent)
{
    controls::Base *tool=parent->newChild<controls::Base>("tool");

    tool->setExcludedFromStats(true);
    tool->setBounds(400, 10, 200, 100);
    tool->newChild<controls::Base>("first")->setBounds(0, 0, 50, 20);
    tool->newChild<controls::Base>("second")->setBounds(0, 30, 50, 20);
    return tool;
}

}//namespace

TEST_CASE("Excluded slots: the ids every flagged subtree on the canvas holds")
{
    Scene scene;

    CHECK(scene.canvas->excludedSlots().heldPrimitives() == 0);
    CHECK(scene.canvas->excludedSlots().heldTexts() == 0);

    controls::Base *tool=newTool(scene.canvas);
    // a flagged subtree on a page of the app's is the canvas's too
    controls::Base *pageTool=newTool(scene.app);

    scene.frames(2);

    const std::vector<size_t> toolIds=primitiveIdsOf(tool);
    const std::vector<size_t> pageToolIds=primitiveIdsOf(pageTool);

    // a StubControl takes two slots
    REQUIRE(toolIds.size() == 6);
    REQUIRE(pageToolIds.size() == 6);
    CHECK(scene.canvas->excludedSlots().heldPrimitives() == 12);
    CHECK(allExcluded(scene.canvas, toolIds));
    CHECK(allExcluded(scene.canvas, pageToolIds));
    // the app's are not
    CHECK(noneExcluded(scene.canvas, primitiveIdsOf(scene.app->getActualChildren().front())));
    CHECK_FALSE(scene.canvas->excludedSlots().primitive(primitiveIdsOf(scene.app).front()));

    // another canvas's are not this one's
    controls::Canvas other(&scene.skin);
    controls::Base *otherTool=newTool(&other);

    other.updateCanvas();
    other.renderCanvas();
    CHECK(noneExcluded(scene.canvas, primitiveIdsOf(otherTool)));
    CHECK(allExcluded(&other, primitiveIdsOf(otherTool)));
}

TEST_CASE("Excluded slots: a released id lasts to the frame after the flush that cleared it")
{
    Scene scene;
    controls::Base *tool=newTool(scene.canvas);

    scene.frames(2);

    controls::Base *first=tool->getActualChildren().front();
    const std::vector<size_t> ids=primitiveIdsOf(first);

    REQUIRE(ids.size() == 2);
    SUBCASE("deleted between frames")
    {
        delete first;
        // frame N's flush clears it...
        CHECK(allExcluded(scene.canvas, ids));
        scene.frame();
        CHECK(sorted(scene.renderer.writtenPrimitiveIds()) == ids);
        // ...and frame N+1's update phase reads it, as often as it likes
        CHECK(allExcluded(scene.canvas, ids));
        CHECK(allExcluded(scene.canvas, ids));
        CHECK(scene.canvas->excludedSlots().heldPrimitives() == 4);
        scene.frame();
        CHECK(noneExcluded(scene.canvas, ids));
    }
    SUBCASE("deleted in the update phase, by a delayed delete")
    {
        first->delayedDelete();
        // frame N deletes it in its update phase and clears it in its flush
        scene.frame();
        CHECK(sorted(scene.renderer.writtenPrimitiveIds()) == ids);
        CHECK(allExcluded(scene.canvas, ids));
        scene.frame();
        CHECK(noneExcluded(scene.canvas, ids));
    }
    SUBCASE("the whole tool deleted")
    {
        const std::vector<size_t> all=primitiveIdsOf(tool);

        delete tool;
        CHECK(scene.canvas->excludedSlots().heldPrimitives() == 0);
        CHECK(allExcluded(scene.canvas, all));
        scene.frame();
        CHECK(scene.renderer.frameStats().primitiveSlotsWritten == all.size());
        CHECK(allExcluded(scene.canvas, all));
        scene.frame();
        CHECK(noneExcluded(scene.canvas, all));
    }
    // the app's deleted controls never are
    controls::Base *appChild=scene.app->getActualChildren().front();
    const std::vector<size_t> appIds=primitiveIdsOf(appChild);

    delete appChild;
    CHECK(noneExcluded(scene.canvas, appIds));
    scene.frame();
    CHECK(noneExcluded(scene.canvas, appIds));
}

TEST_CASE("Excluded slots: the overlay and the Inspector open together read the app's slots alone")
{
    Scene scene;
    StatsOverlay *overlay=StatsOverlay::open(scene.canvas);
    Inspector *inspector=Inspector::open(scene.canvas);

    scene.untilRefresh(overlay);
    scene.untilRefresh(overlay);
    CHECK(overlay->lines()[SlotsLine] == NoSlots);
    CHECK(inspector->counterLines()[Inspector::SlotsLine] == InspectorNoSlots);

    SUBCASE("the Inspector writes on every frame of the overlay's window")
    {
        // to the end of the window shown now, then a whole window
        for(int window=0; window < 2; ++window)
        {
            const size_t refreshes=overlay->refreshes();

            while(overlay->refreshes() == refreshes)
            {
                inspector->addStateChange(controls::StateChange_Background);
                scene.frame();
                REQUIRE(scene.renderer.frameStats().primitiveSlotsWritten > 0);
            }
        }
        CHECK(overlay->shownWindow().primitiveSlots == 0);
        CHECK(overlay->lines()[SlotsLine] == NoSlots);
    }
    SUBCASE("the overlay's refresh frames, in the Inspector")
    {
        // the frame after each refresh reads the refresh frame's writes
        for(int window=0; window < 3; ++window)
        {
            scene.untilRefresh(overlay);
            scene.frame();
            CHECK(inspector->counterLines()[Inspector::SlotsLine] == InspectorNoSlots);
        }
    }
    SUBCASE("a flagged control's deletion, in both")
    {
        controls::Base *tool=newTool(scene.canvas);

        scene.untilRefresh(overlay);
        scene.untilRefresh(overlay);
        delete tool->getActualChildren().front();
        scene.frame();
        REQUIRE(scene.renderer.frameStats().primitiveSlotsWritten > 0);
        scene.frame();
        CHECK(inspector->counterLines()[Inspector::SlotsLine] == InspectorNoSlots);
        scene.untilRefresh(overlay);
        CHECK(overlay->shownWindow().primitiveSlots == 0);
    }
    SUBCASE("the Inspector closed, in the overlay")
    {
        Inspector::close(scene.canvas);
        for(int i=0; i < 4; ++i)
            scene.frame();
        CHECK(Inspector::find(scene.canvas) == nullptr);
        scene.untilRefresh(overlay);
        CHECK(overlay->shownWindow().primitiveSlots == 0);
        CHECK(overlay->shownWindow().glyphSlots == 0);
        scene.untilRefresh(overlay);
        CHECK(overlay->shownWindow().primitiveSlots == 0);
    }
    SUBCASE("the overlay closed, in the Inspector")
    {
        StatsOverlay::close(scene.canvas);
        for(int i=0; i < 4; ++i)
        {
            scene.frame();
            CHECK(inspector->counterLines()[Inspector::SlotsLine] == InspectorNoSlots);
        }
    }
    SUBCASE("an app change, in both, then 0")
    {
        scene.frames(3);
        scene.app->addStateChange(controls::StateChange_Background);
        scene.frame();

        const size_t appWritten=scene.renderer.frameStats().primitiveSlotsWritten;

        REQUIRE(appWritten > 0);
        scene.frame();
        CHECK(inspector->counterLines()[Inspector::SlotsLine]
            == format("slots written: %zu primitive, 0 text", appWritten));
        scene.frame();
        CHECK(inspector->counterLines()[Inspector::SlotsLine] == InspectorNoSlots);
        scene.untilRefresh(overlay);
        CHECK(overlay->shownWindow().primitiveSlots == appWritten);
        scene.untilRefresh(overlay);
        CHECK(overlay->shownWindow().primitiveSlots == 0);
        CHECK(inspector->counterLines()[Inspector::SlotsLine] == InspectorNoSlots);
    }
}

TEST_CASE("Excluded slots: the overlay's live ids are the same with the Inspector open")
{
    Scene scene;
    StatsOverlay *overlay=StatsOverlay::open(scene.canvas);

    scene.untilRefresh(overlay);
    scene.untilRefresh(overlay);

    const size_t primitives=overlay->shownPrimitiveIds();
    const size_t texts=overlay->shownTextIds();

    CHECK(primitives == primitiveIdCount()-releasedPrimitiveIdCount()
        -scene.canvas->excludedSlots().heldPrimitives());

    Inspector *inspector=Inspector::open(scene.canvas);

    inspector->nodeFor(scene.app)->open();
    scene.untilRefresh(overlay);
    scene.untilRefresh(overlay);
    CHECK(scene.canvas->excludedSlots().heldPrimitives() > 0);
    CHECK(overlay->shownPrimitiveIds() == primitives);
    CHECK(overlay->shownTextIds() == texts);

    Inspector::close(scene.canvas);
    scene.frames(3);
    scene.untilRefresh(overlay);
    CHECK(overlay->shownPrimitiveIds() == primitives);
    CHECK(overlay->shownTextIds() == texts);
}
