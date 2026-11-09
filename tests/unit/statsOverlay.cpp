/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  gweni::tools::StatsOverlay on a real Canvas, with a StubSkin whose
 *  Controls draw a rect into each of their slots whenever their control
 *  changes, under a stepped canvas clock (1/64 s a frame, so a refresh is
 *  every 16 frames exactly) and a profile clock that advances 100 us per read:
 *  - open() makes one overlay, the canvas's overlay, which find() finds;
 *    toggle() closes and opens it, close() deletes it; two toggles in one
 *    frame leave it open, a new one, the closed one deleted;
 *  - it sits StatsOverlay::Inset units in from the canvas's bottom-right
 *    corner, and follows the canvas to a new size and a new scale;
 *  - a hit test passes through it: the control under it is found, with the
 *    same hit-test visits as without it;
 *  - it refreshes every RefreshSeconds: the frames between two refreshes set
 *    no label and write no slot, and a refresh sets only labels whose text
 *    changed, in a frame with no layout pass counted and no animation run;
 *  - it shows the window's times (each phase one profile-clock step, the
 *    frame interval every read between two starts) and fps;
 *  - an app change shows on the next refresh as the most one frame did
 *    (rendered, changed, the app's slots alone), and 0 on the one after;
 *  - it counts the app's controls, the tools' subtrees left out, and the
 *    app's live ids;
 *  - closed, it gives back every primitive and text id it took.
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/tools/inspector.h>
#include <gweni/tools/statsOverlay.h>

#include <doctest/doctest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace gweni;
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

// The rows, as StatsOverlay::lines() lists them.
enum Line
{
    HeaderLine, FrameLine, CpuLine, UpdateLine, RenderLine, FlushLine,
    FpsLine, IdsLine, MaxHeaderLine, RenderedLine, LayoutLine, ZLine, SlotsLine, UploadsLine
};

struct Scene
{
    test::StubRenderer renderer;
    DrawingSkin skin{&renderer};
    controls::Canvas *canvas;
    //! a control in the canvas's bottom-right corner, under the overlay
    controls::Base *corner;
    controls::Base *other;
    float clock=10.0f;
    uint64_t profileReads=0;

    static constexpr float Step=1.0f/64.0f;

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        platform::setProfileClock([this]() { return (++profileReads)*100; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(1024, 768);
        corner=canvas->newChild<controls::Base>("corner");
        corner->setBounds(624, 368, 400, 400);
        other=canvas->newChild<controls::Base>("other");
        other->setBounds(10, 10, 300, 200);
        frames(3);
    }

    ~Scene()
    {
        HoveredControl=nullptr;
        delete canvas;
        platform::setProfileClock(nullptr);
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

}//namespace

TEST_CASE("StatsOverlay: open, find, toggle and close")
{
    Scene scene;

    CHECK(StatsOverlay::find(scene.canvas) == nullptr);

    StatsOverlay *overlay=StatsOverlay::open(scene.canvas);

    REQUIRE(overlay);
    CHECK(StatsOverlay::open(scene.canvas) == overlay);
    CHECK(StatsOverlay::find(scene.canvas) == overlay);
    CHECK(overlay->isExcludedFromStats());
    scene.frames(2);
    // an overlay: after the app's children, one added later included
    scene.canvas->newChild<controls::Base>("late");
    scene.frames(2);
    CHECK(scene.canvas->getActualChildren().back() == overlay);

    CHECK_FALSE(StatsOverlay::toggle(scene.canvas));
    CHECK_FALSE(overlay->visible());
    scene.frames(2);
    CHECK(StatsOverlay::find(scene.canvas) == nullptr);
    CHECK(StatsOverlay::toggle(scene.canvas));
    REQUIRE(StatsOverlay::find(scene.canvas));
    StatsOverlay::close(scene.canvas);
    scene.frames(2);
    CHECK(StatsOverlay::find(scene.canvas) == nullptr);

    // closed and opened again before the closed one's delayed delete
    REQUIRE(StatsOverlay::toggle(scene.canvas));
    CHECK_FALSE(StatsOverlay::toggle(scene.canvas));
    CHECK(StatsOverlay::find(scene.canvas) == nullptr);
    CHECK(StatsOverlay::toggle(scene.canvas));
    scene.frames(2);

    StatsOverlay *reopened=StatsOverlay::find(scene.canvas);

    REQUIRE(reopened);
    CHECK(reopened->visible());
    size_t overlays=0;

    for(controls::Base *child : scene.canvas->getActualChildren())
        overlays+=gweni_cast<StatsOverlay>(child) ? 1 : 0;
    CHECK(overlays == 1);
    StatsOverlay::close(scene.canvas);
    scene.frames(2);
    CHECK(StatsOverlay::find(scene.canvas) == nullptr);
}

TEST_CASE("StatsOverlay: in the canvas's bottom-right corner, at any size and scale")
{
    Scene scene;
    StatsOverlay *overlay=StatsOverlay::open(scene.canvas);

    scene.frames(2);

    auto inCorner=[&]()
    {
        const Rect &bounds=overlay->getBounds();

        CHECK(bounds.w > 0);
        CHECK(bounds.h > 0);
        CHECK(bounds.x+bounds.w == scene.canvas->getWidth()-StatsOverlay::Inset);
        CHECK(bounds.y+bounds.h == scene.canvas->getHeight()-StatsOverlay::Inset);
    };

    inCorner();

    const Point size(overlay->getWidth(), overlay->getHeight());

    scene.canvas->setSize(640, 480);
    scene.frames(2);
    inCorner();
    scene.canvas->setScale(2.0f);
    scene.frames(2);
    inCorner();
    // the same logical size at every canvas size and scale
    CHECK(overlay->getWidth() == size.x);
    CHECK(overlay->getHeight() == size.y);
}

TEST_CASE("StatsOverlay: hit tests pass through it")
{
    Scene scene;
    const Point at(scene.canvas->getWidth()-20, scene.canvas->getHeight()-20);

    const size_t visitsBefore=input::hitTestVisits();

    CHECK(scene.canvas->getControlAt(at.x, at.y, false) == scene.corner);

    const size_t visitsWithout=input::hitTestVisits()-visitsBefore;
    StatsOverlay *overlay=StatsOverlay::open(scene.canvas);

    scene.frames(2);
    REQUIRE(overlay->getBounds().x < at.x);
    REQUIRE(overlay->getBounds().y < at.y);

    const size_t visitsOpen=input::hitTestVisits();

    CHECK(scene.canvas->getControlAt(at.x, at.y) == scene.corner);
    CHECK(scene.canvas->getControlAt(at.x, at.y, false) == scene.corner);
    CHECK((input::hitTestVisits()-visitsOpen) == 2*visitsWithout);
}

TEST_CASE("StatsOverlay: it refreshes every RefreshSeconds and settles in between")
{
    Scene scene;
    StatsOverlay *overlay=StatsOverlay::open(scene.canvas);

    scene.untilRefresh(overlay);
    scene.untilRefresh(overlay);
    // a refresh every 16 frames of 1/64 s
    CHECK(scene.untilRefresh(overlay) == 16);

    for(int refresh=0; refresh < 2; ++refresh)
    {
        const size_t sets=overlay->textSets();
        const size_t refreshes=overlay->refreshes();

        for(int i=0; i < 15; ++i)
        {
            scene.frame();
            CAPTURE(i);
            CHECK(scene.renderer.frameStats().primitiveSlotsWritten == 0);
            CHECK(scene.renderer.frameStats().textSlotsWritten == 0);
            CHECK(overlay->textSets() == sets);
            CHECK(overlay->refreshes() == refreshes);
        }

        // the refresh frame: it is counted in neither the layout nor the animations
        scene.frame();
        CHECK(overlay->refreshes() == refreshes+1);
        CHECK(scene.canvas->getLayoutStats().passes == 0);
        CHECK(scene.canvas->getUpdateStats().animsRun == 0);
    }
}

TEST_CASE("StatsOverlay: it shows the window's times and fps")
{
    Scene scene;
    StatsOverlay *overlay=StatsOverlay::open(scene.canvas);

    for(int i=0; i < 6; ++i)
        scene.untilRefresh(overlay);

    const StatsOverlay::Window &shown=overlay->shownWindow();

    CHECK(shown.frames == 16);
    // two reads each in updateCanvas() and renderCanvas(); the stub times no flush
    CHECK(shown.update.min == 100);
    CHECK(shown.update.max == 100);
    CHECK(shown.render.avg() == 100);
    CHECK(shown.flush.max == 0);
    CHECK(shown.cpu.max == 200);
    CHECK(shown.frame.min == 400);
    CHECK(shown.frame.max == 400);
    CHECK(overlay->shownFps() == doctest::Approx(64.0f));

    const std::vector<String> lines=overlay->lines();

    CHECK(lines[HeaderLine] == "ms | min | avg | max");
    CHECK(lines[FrameLine] == "frame | 0.4 | 0.4 | 0.4");
    CHECK(lines[CpuLine] == "cpu | 0.2 | 0.2 | 0.2");
    CHECK(lines[UpdateLine] == "update | 0.1 | 0.1 | 0.1");
    CHECK(lines[RenderLine] == "render | 0.1 | 0.1 | 0.1");
    CHECK(lines[FlushLine] == "flush | 0.0 | 0.0 | 0.0");
    CHECK(lines[FpsLine] == format("fps | 64.0 | controls | %zu", overlay->shownControls()));
    // the plot's last point is the window's most cpu time
    CHECK(overlay->cpuPlot()->getValues().back() == doctest::Approx(0.2f));
    CHECK(overlay->cpuPlot()->getValueCount() == StatsOverlay::PlotSamples);
}

TEST_CASE("StatsOverlay: an app change shows on the next refresh, then 0")
{
    Scene scene;
    StatsOverlay *overlay=StatsOverlay::open(scene.canvas);

    scene.untilRefresh(overlay);
    scene.untilRefresh(overlay);
    CHECK(overlay->lines()[RenderedLine] == "rendered | 0 | changed | 0");
    CHECK(overlay->lines()[SlotsLine] == "prim slots | 0 | glyphs | 0");

    // a frame in the middle of the window
    scene.frames(5);
    scene.other->addStateChange(controls::StateChange_Background);
    scene.frame();

    const size_t appWritten=scene.renderer.frameStats().primitiveSlotsWritten;

    REQUIRE(appWritten > 0);
    scene.untilRefresh(overlay);
    CHECK(overlay->shownWindow().rendered == 1);
    CHECK(overlay->shownWindow().changed == 1);
    CHECK(overlay->shownWindow().primitiveSlots == appWritten);
    CHECK(overlay->lines()[RenderedLine] == "rendered | 1 | changed | 1");
    CHECK(overlay->lines()[SlotsLine] == format("prim slots | %zu | glyphs | 0", appWritten));
    CHECK(overlay->slotsPlot()->getValues().back() == doctest::Approx(float(appWritten)));

    // its own refresh wrote slots; they are not the app's
    scene.untilRefresh(overlay);
    CHECK(overlay->lines()[RenderedLine] == "rendered | 0 | changed | 0");
    CHECK(overlay->lines()[SlotsLine] == "prim slots | 0 | glyphs | 0");
}

TEST_CASE("StatsOverlay: the tree's controls and ids are the app's")
{
    Scene scene;
    const size_t livePrimitives=primitiveIdCount()-releasedPrimitiveIdCount();
    const size_t liveTexts=textIdCount()-releasedTextIdCount();
    StatsOverlay *overlay=StatsOverlay::open(scene.canvas);

    // an inspector is a tool too: left out of the count
    tools::Inspector::open(scene.canvas);
    scene.untilRefresh(overlay);
    scene.untilRefresh(overlay);
    CHECK(overlay->shownControls() == 2);
    CHECK(overlay->shownPrimitiveIds() >= livePrimitives);
    CHECK(overlay->shownTextIds() >= liveTexts);
    tools::Inspector::close(scene.canvas);
    scene.frames(2);
    scene.untilRefresh(overlay);
    CHECK(overlay->shownPrimitiveIds() == livePrimitives);
    CHECK(overlay->shownTextIds() == liveTexts);

    // a new control is counted on the next refresh
    scene.canvas->newChild<controls::Base>("late")->newChild<controls::Base>();
    scene.untilRefresh(overlay);
    CHECK(overlay->shownControls() == 4);
}

TEST_CASE("StatsOverlay: closed, it gives back every id it took")
{
    Scene scene;
    const test::IdBankCounts before=test::IdBankCounts::now();
    const size_t livePrimitives=scene.renderer.livePrimitives.size();
    const size_t liveTexts=scene.renderer.liveTexts.size();

    StatsOverlay::open(scene.canvas);
    scene.frames(20);
    CHECK(scene.renderer.livePrimitives.size() > livePrimitives);
    StatsOverlay::close(scene.canvas);
    scene.frames(2);
    CHECK(scene.renderer.livePrimitives.size() == livePrimitives);
    CHECK(scene.renderer.liveTexts.size() == liveTexts);
    CHECK(test::IdBankCounts::now().livePrimitives() == before.livePrimitives());
    CHECK(test::IdBankCounts::now().liveTexts() == before.liveTexts());
}
