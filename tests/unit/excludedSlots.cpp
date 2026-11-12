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
 *    (the Inspector), and then 0;
 *  - the held ids follow, in the same update phase, a flag set or cleared,
 *    an app control moved into a flagged subtree and out again, and a
 *    flagged subtree taken off the canvas and put back;
 *  - the canvas's ToolTip box showing an overlay name's tooltip is every
 *    tool's: both read 0 over the frames that show it and hide it, and the
 *    overlay's ids row leaves its ids out; moved straight on to an app
 *    control's tooltip, it is the app's, counted in both, its ids too.
 */
#include "headlessFixture.h"

#include <gweni/controls/popup.h>
#include <gweni/controls/toolTip.h>
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

//! A control whose skin Control gives back its own slots (test::ShrinkingControl).
class Shrinking: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Shrinking, controls::Base)
    {}

    test::ShrinkingControl *shrinkingControl()
    {
        return static_cast<test::ShrinkingControl *>(const_cast<skin::Control *>(getSkinControl()));
    }
};

class DrawingSkin: public test::StubSkin
{
public:
    using StubSkin::StubSkin;

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == Shrinking::getTypeNameStatic())
            return std::make_unique<test::ShrinkingControl>(this, false, std::move(typeName));

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

//! The primitive ids the controls on canvas outside every flagged subtree
//! hold, sorted: the app's.
std::vector<size_t> appPrimitiveIds(const controls::Base *control)
{
    std::vector<size_t> primitives;
    std::vector<size_t> texts;

    if(control->isExcludedFromStats())
        return primitives;
    if(const skin::Control *skinControl=control->getSkinControl())
        skinControl->appendIds(primitives, texts);
    for(const controls::Base *child : control->getActualChildren())
    {
        const std::vector<size_t> childIds=appPrimitiveIds(child);

        primitives.insert(primitives.end(), childIds.begin(), childIds.end());
    }
    return sorted(primitives);
}

//! A control that deletes `victim` as it is deleted.
class DeletesOnDestruction: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(DeletesOnDestruction, controls::Base)
    {}

    ~DeletesOnDestruction() override
    {
        delete victim;
    }

    controls::Base *victim=nullptr;
};

//! How many of ids are in written.
size_t countWritten(const std::vector<size_t> &written, const std::vector<size_t> &ids)
{
    size_t count=0;

    for(size_t id : ids)
        count+=std::count(written.begin(), written.end(), id);
    return count;
}

//! Takes every primitive id the bank holds released, so the next ones it
//! hands out are those released after it was made; gives them back as it goes.
struct NoReleasedPrimitiveIds
{
    NoReleasedPrimitiveIds()
    {
        while(releasedPrimitiveIdCount() > 0)
            taken.push_back(getPrimitiveId(nullptr));
    }

    ~NoReleasedPrimitiveIds()
    {
        for(size_t id : taken)
            releasePrimitiveId(id);
    }

    std::vector<size_t> taken;
};

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
        // the last flush's record until the next flush
        CHECK(scene.canvas->excludedSlots().heldPrimitives() == all.size());
        CHECK(allExcluded(scene.canvas, all));
        scene.frame();
        CHECK(scene.renderer.frameStats().primitiveSlotsWritten == all.size());
        CHECK(scene.canvas->excludedSlots().heldPrimitives() == 0);
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

    // the live ids at the flush less the tools' (nothing changed since the
    // refresh read them)
    CHECK(primitives == scene.canvas->excludedSlots().appPrimitives());
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

TEST_CASE("Excluded slots: a flag or tree change takes effect at the next flush")
{
    Scene scene;
    controls::Base *tool=newTool(scene.canvas);
    controls::Base *moved=scene.app->getActualChildren().front();

    scene.frames(2);

    const std::vector<size_t> toolIds=primitiveIdsOf(tool);
    const std::vector<size_t> movedIds=primitiveIdsOf(moved);

    REQUIRE(allExcluded(scene.canvas, toolIds));
    REQUIRE(noneExcluded(scene.canvas, movedIds));

    // Each change below is made in an update phase: until that frame's flush
    // the record is the last flush's, read the same however often; after it,
    // the change's. A tool whose ids are no longer held gave none back, so
    // nothing of it lingers as cleared.
    auto updatePhase=[&]()
    {
        scene.clock+=Scene::Step;
        scene.canvas->updateCanvas();
    };
    auto heldNow=[&]() { return scene.canvas->excludedSlots().heldPrimitives(); };

    SUBCASE("a flag changed")
    {
        updatePhase();
        tool->setExcludedFromStats(false);
        moved->setExcludedFromStats(true);
        for(int read=0; read < 3; ++read)
        {
            CHECK(allExcluded(scene.canvas, toolIds));
            CHECK(noneExcluded(scene.canvas, movedIds));
            CHECK(heldNow() == toolIds.size());
        }
        scene.canvas->renderCanvas();
        CHECK(noneExcluded(scene.canvas, toolIds));
        CHECK(allExcluded(scene.canvas, movedIds));
        CHECK(heldNow() == movedIds.size());

        // and back
        updatePhase();
        tool->setExcludedFromStats(true);
        moved->setExcludedFromStats(false);
        CHECK(noneExcluded(scene.canvas, toolIds));
        CHECK(allExcluded(scene.canvas, movedIds));
        scene.canvas->renderCanvas();
        CHECK(allExcluded(scene.canvas, toolIds));
        CHECK(noneExcluded(scene.canvas, movedIds));
        CHECK(heldNow() == toolIds.size());
    }
    SUBCASE("an app control moved into a flagged subtree and out again")
    {
        updatePhase();
        moved->setParent(tool);
        for(int read=0; read < 3; ++read)
        {
            CHECK(noneExcluded(scene.canvas, movedIds));
            CHECK(heldNow() == toolIds.size());
        }
        scene.canvas->renderCanvas();
        CHECK(allExcluded(scene.canvas, movedIds));
        CHECK(heldNow() == toolIds.size()+movedIds.size());

        updatePhase();
        moved->setParent(scene.app);
        for(int read=0; read < 3; ++read)
        {
            CHECK(allExcluded(scene.canvas, movedIds));
            CHECK(heldNow() == toolIds.size()+movedIds.size());
        }
        scene.canvas->renderCanvas();
        CHECK(noneExcluded(scene.canvas, movedIds));
        CHECK(allExcluded(scene.canvas, toolIds));
        CHECK(heldNow() == toolIds.size());
    }
    SUBCASE("a flagged subtree taken off the canvas and put back")
    {
        updatePhase();
        tool->setParent(nullptr);
        for(int read=0; read < 3; ++read)
        {
            CHECK(allExcluded(scene.canvas, toolIds));
            CHECK(heldNow() == toolIds.size());
        }
        scene.canvas->renderCanvas();
        CHECK(noneExcluded(scene.canvas, toolIds));
        CHECK(heldNow() == 0);

        updatePhase();
        tool->setParent(scene.canvas);
        for(int read=0; read < 3; ++read)
        {
            CHECK(noneExcluded(scene.canvas, toolIds));
            CHECK(heldNow() == 0);
        }
        scene.canvas->renderCanvas();
        CHECK(allExcluded(scene.canvas, toolIds));
        CHECK(heldNow() == toolIds.size());
    }
}

TEST_CASE("Excluded slots: a tool's tooltip is every tool's, the app's tooltip the app's")
{
    Scene scene;
    // an app control with a tooltip, clear of both tools
    controls::Base *tipped=scene.canvas->newChild<controls::Base>("tipped");

    tipped->setBounds(10, 400, 100, 40);
    tipped->setToolTipText("the app's");

    StatsOverlay *overlay=StatsOverlay::open(scene.canvas);
    Inspector *inspector=Inspector::open(scene.canvas);

    scene.untilRefresh(overlay);
    scene.untilRefresh(overlay);

    controls::Label *interval=overlay->nameCells()[4];

    REQUIRE(interval->getText() == "interval");

    const Point onName=overlay->getPos()+interval->getPos()
        +Point(interval->getWidth()/2, interval->getHeight()/2);
    const size_t idsBefore=overlay->shownPrimitiveIds();
    const size_t textIdsBefore=overlay->shownTextIds();

    // the slots of the frame before, in the Inspector, and of the window shown
    // last, in the overlay, are 0
    auto bothZero=[&]()
    {
        return inspector->counterLines()[Inspector::SlotsLine] == InspectorNoSlots
            && overlay->shownWindow().primitiveSlots == 0 && overlay->shownWindow().glyphSlots == 0;
    };
    // every frame until the overlay's next refresh reads 0 in the Inspector,
    // and so does that refresh
    auto zeroUntilRefresh=[&]()
    {
        const size_t refreshes=overlay->refreshes();
        bool zero=true;

        for(int i=0; (i < 100) && (overlay->refreshes() == refreshes); ++i)
        {
            scene.frame();
            zero=zero && (inspector->counterLines()[Inspector::SlotsLine] == InspectorNoSlots);
        }
        return zero && bothZero();
    };

    // onto the name, until its tooltip shows
    scene.canvas->inputMouseMoved(onName.x, onName.y, 0, 0);
    for(int i=0; (i < 100) && !scene.canvas->toolTipShown(); ++i)
        scene.frame();
    REQUIRE(scene.canvas->toolTipOwner() == interval);

    controls::ToolTip *box=scene.canvas->toolTipControl();

    REQUIRE(box);
    REQUIRE(box->isExcludedFromStats());

    const std::vector<size_t> boxIds=primitiveIdsOf(box);

    REQUIRE(!boxIds.empty());
    // the box is a tool's for every tool: the frames that showed it and the
    // ones after
    CHECK(allExcluded(scene.canvas, boxIds));
    CHECK(zeroUntilRefresh());
    CHECK(zeroUntilRefresh());
    // and its ids are not the app's
    CHECK(overlay->shownPrimitiveIds() == idsBefore);
    CHECK(overlay->shownTextIds() == textIdsBefore);

    // a press hides it: that frame is the tools' too
    scene.canvas->inputMouseButton(0, true);
    scene.frame();
    REQUIRE(!scene.canvas->toolTipShown());
    scene.canvas->inputMouseButton(0, false);
    CHECK(zeroUntilRefresh());
    CHECK(zeroUntilRefresh());
    CHECK(box->isExcludedFromStats());
    CHECK(overlay->shownPrimitiveIds() == idsBefore);

    // straight to the app control's tooltip: the box is the app's, counted
    // in both, and its ids are the app's
    scene.canvas->inputMouseMoved(60, 420, 0, 0);
    for(int i=0; (i < 100) && !scene.canvas->toolTipShown(); ++i)
        scene.frame();
    REQUIRE(scene.canvas->toolTipOwner() == tipped);
    CHECK_FALSE(box->isExcludedFromStats());
    CHECK(noneExcluded(scene.canvas, boxIds));
    // the frame after the show reads the show's writes
    scene.frame();
    CHECK(inspector->counterLines()[Inspector::SlotsLine] != InspectorNoSlots);
    scene.untilRefresh(overlay);
    CHECK(overlay->shownWindow().primitiveSlots > 0);
    scene.untilRefresh(overlay);
    CHECK(overlay->shownPrimitiveIds() == idsBefore+boxIds.size());
}

TEST_CASE("Excluded slots: a tool's id taken again by the app in the same frame is the app's")
{
    Scene scene;
    StatsOverlay *overlay=StatsOverlay::open(scene.canvas);
    Inspector *inspector=Inspector::open(scene.canvas);
    controls::Base *tool=newTool(scene.canvas);

    scene.untilRefresh(overlay);
    scene.untilRefresh(overlay);
    REQUIRE(overlay->lines()[SlotsLine] == NoSlots);

    controls::Base *first=tool->getActualChildren().front();
    const std::vector<size_t> freed=primitiveIdsOf(first);

    REQUIRE(freed.size() == 2);

    // so the ids the deletion frees are the first ones handed out again
    NoReleasedPrimitiveIds noReleased;
    controls::Base *created=nullptr;

    // the tool's control deleted in the update phase, and an app control made
    // in the same frame, which takes its ids as its render phase generates it
    first->delayedDelete();
    scene.clock+=Scene::Step;
    scene.canvas->updateCanvas();
    created=scene.app->newChild<controls::Base>("created");
    created->setBounds(0, 40, 50, 20);
    scene.canvas->renderCanvas();

    const std::vector<size_t> createdIds=primitiveIdsOf(created);

    // the reuse happened
    REQUIRE(createdIds == freed);

    const std::vector<size_t> &written=scene.renderer.writtenPrimitiveIds();
    const size_t appWritten=countWritten(written, appPrimitiveIds(scene.canvas));

    REQUIRE(countWritten(written, createdIds) == createdIds.size());
    CHECK(noneExcluded(scene.canvas, createdIds));
    CHECK(tools::appFrameWrites(scene.canvas, &scene.renderer).primitiveSlots == appWritten);
    // the frame after reads it, in the Inspector and then in the overlay
    scene.frame();
    CHECK(inspector->counterLines()[Inspector::SlotsLine]
        == format("slots written: %zu primitive, 0 text", appWritten));
    scene.untilRefresh(overlay);
    CHECK(overlay->shownWindow().primitiveSlots == appWritten);
}

TEST_CASE("Excluded slots: the slots a tool's skin Control gives back itself are the tool's")
{
    Scene scene;
    StatsOverlay *overlay=StatsOverlay::open(scene.canvas);
    Inspector *inspector=Inspector::open(scene.canvas);
    controls::Base *tool=newTool(scene.canvas);
    Shrinking *shrinking=tool->newChild<Shrinking>("shrinking");

    shrinking->setBounds(0, 60, 50, 20);
    scene.untilRefresh(overlay);
    scene.untilRefresh(overlay);
    REQUIRE(overlay->lines()[SlotsLine] == NoSlots);
    REQUIRE(primitiveIdsOf(shrinking).size() == 2);

    // a slot given back a frame, then the other, each cleared by that frame's
    // flush
    for(size_t keep : {size_t(1), size_t(0)})
    {
        const std::vector<size_t> before=primitiveIdsOf(shrinking);

        shrinking->shrinkingControl()->keep=keep;
        shrinking->addStateChange(controls::StateChange_Background);
        scene.frame();
        REQUIRE(primitiveIdsOf(shrinking).size() == keep);
        REQUIRE(countWritten(scene.renderer.writtenPrimitiveIds(), before) == before.size());
        CHECK(allExcluded(scene.canvas, before));
        CHECK(tools::appFrameWrites(scene.canvas, &scene.renderer).primitiveSlots == 0);
        scene.frame();
        CHECK(inspector->counterLines()[Inspector::SlotsLine] == InspectorNoSlots);
    }
    scene.untilRefresh(overlay);
    CHECK(overlay->shownWindow().primitiveSlots == 0);
    CHECK(overlay->lines()[SlotsLine] == NoSlots);
}

TEST_CASE("Excluded slots: an app control deleted as a tool is torn down is the app's")
{
    Scene scene;
    controls::Base *tool=newTool(scene.canvas);
    controls::Base *owner=tool->getActualChildren().front();
    // on the canvas, outside the tool, deleted as it goes
    controls::Base *victim=nullptr;

    SUBCASE("deleted by a destructor of the tool's")
    {
        victim=scene.app->newChild<controls::Base>("victim");
        victim->setBounds(0, 60, 50, 20);
        owner->newChild<DeletesOnDestruction>()->victim=victim;
    }
    SUBCASE("a popup a control of the tool's owns")
    {
        // closed and delayed-deleted, so it goes in the next update phase
        controls::Popup *popup=owner->createPopup();

        REQUIRE(popup);
        popup->setBounds(10, 300, 100, 60);
        popup->open(scene.app);
        victim=popup;
    }
    REQUIRE_FALSE(victim->statsExcluded());
    scene.frames(2);

    const std::vector<size_t> toolIds=primitiveIdsOf(tool);
    const std::vector<size_t> victimIds=primitiveIdsOf(victim);

    REQUIRE(!victimIds.empty());
    REQUIRE(noneExcluded(scene.canvas, victimIds));

    delete tool;
    scene.frame();

    const std::vector<size_t> &written=scene.renderer.writtenPrimitiveIds();

    REQUIRE(countWritten(written, victimIds) == victimIds.size());
    // its cleared slots are the app's, the tool's the tool's
    CHECK(noneExcluded(scene.canvas, victimIds));
    CHECK(allExcluded(scene.canvas, toolIds));
    CHECK(tools::appFrameWrites(scene.canvas, &scene.renderer).primitiveSlots
        == written.size()-countWritten(written, toolIds));
}

TEST_CASE("Excluded slots: a tool closed whole owns the slots the next flush clears")
{
    Scene scene;
    controls::Base *tool=nullptr;
    bool inspector=false;

    SUBCASE("the Inspector")
    {
        tool=Inspector::open(scene.canvas);
        inspector=true;
    }
    SUBCASE("the overlay")
    {
        tool=StatsOverlay::open(scene.canvas);
    }
    scene.frames(4);

    const std::vector<size_t> toolIds=primitiveIdsOf(tool);

    REQUIRE(!toolIds.empty());
    REQUIRE(allExcluded(scene.canvas, toolIds));
    if(inspector)
        Inspector::close(scene.canvas);
    else
        StatsOverlay::close(scene.canvas);
    // the close deletes it in the update phase, and the flush clears its slots
    scene.frame();
    REQUIRE(Inspector::find(scene.canvas) == nullptr);
    REQUIRE(StatsOverlay::find(scene.canvas) == nullptr);
    REQUIRE(countWritten(scene.renderer.writtenPrimitiveIds(), toolIds) == toolIds.size());
    CHECK(allExcluded(scene.canvas, toolIds));

    const tools::AppFrameWrites writes=tools::appFrameWrites(scene.canvas, &scene.renderer);

    CHECK(writes.primitiveSlots == 0);
    CHECK(writes.glyphSlots == 0);
    // and no longer once a flush has not cleared them
    scene.frame();
    CHECK(noneExcluded(scene.canvas, toolIds));
}
