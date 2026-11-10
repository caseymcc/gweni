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
 *  - closed, it gives back every primitive and text id it took;
 *  - onOpenChangedCaller is called once for each open() that makes an
 *    overlay and each close() that closes one (toggle() included), with the
 *    canvas and 1 or 0, after find() gives the new answer; not for an overlay
 *    made as another control's child, nor for a handler deleted;
 *  - setBottomReserve() raises the canvas's overlay by that many units, laying
 *    out the overlay alone, kept across a close and an open and at a new
 *    canvas size; an overlay on a page ignores it, and a canvas made after a
 *    deleted one with a reserve starts at 0;
 *  - a reserve set from a control's Y arrange (as the demo shell sets it from
 *    its status bar) is in effect in that same pass, with the overlay
 *    arranged before the control or settled and clean: one pass, and what a
 *    full layout pass gives (layout verification).
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

namespace
{

//! What StatsOverlay::onOpenChangedCaller told it, in order.
class OpenListener: public event::Handler
{
public:
    struct Call
    {
        controls::Base *canvas;
        size_t open;
        controls::Base *overlay;
    };

    OpenListener()
    {
        StatsOverlay::onOpenChangedCaller.add(this, &OpenListener::onOpenChanged);
    }

    void onOpenChanged(event::Info info)
    {
        calls.push_back({info.control, info.integer, info.controlCaller});
    }

    std::vector<Call> calls;
};

}//namespace

TEST_CASE("StatsOverlay: onOpenChangedCaller tells of every open and close")
{
    Scene scene;
    OpenListener listener;

    StatsOverlay *overlay=StatsOverlay::open(scene.canvas);

    REQUIRE(listener.calls.size() == 1);
    CHECK((listener.calls[0].canvas == scene.canvas));
    CHECK(listener.calls[0].open == 1);
    CHECK((listener.calls[0].overlay == overlay));
    // open already: nothing to tell
    StatsOverlay::open(scene.canvas);
    CHECK(listener.calls.size() == 1);

    StatsOverlay::close(scene.canvas);
    REQUIRE(listener.calls.size() == 2);
    CHECK((listener.calls[1].canvas == scene.canvas));
    CHECK(listener.calls[1].open == 0);
    CHECK((listener.calls[1].overlay == overlay));
    StatsOverlay::close(scene.canvas);
    CHECK(listener.calls.size() == 2);

    // toggle() tells too, each time, and find() already gives the new answer
    // when a handler is called
    bool foundOnOpen=false;
    bool foundOnClose=true;
    struct Checker: public event::Handler {} checker;

    StatsOverlay::onOpenChangedCaller.addCallback(&checker, [&](event::Handler &, event::Info info)
    {
        controls::Canvas *canvas=info.control->getCanvas();

        (info.integer ? foundOnOpen : foundOnClose)=(StatsOverlay::find(canvas) != nullptr);
    });
    CHECK(StatsOverlay::toggle(scene.canvas));
    CHECK_FALSE(StatsOverlay::toggle(scene.canvas));
    CHECK(listener.calls.size() == 4);
    CHECK(listener.calls[2].open == 1);
    CHECK(listener.calls[3].open == 0);
    CHECK(foundOnOpen);
    CHECK_FALSE(foundOnClose);
    scene.frames(2);

    // an overlay made as another control's child is not the canvas's
    scene.other->newChild<StatsOverlay>();
    scene.frames(2);
    CHECK(listener.calls.size() == 4);

    // another canvas's carry that canvas
    {
        controls::Canvas *second=new controls::Canvas(&scene.skin);

        second->setSize(320, 240);
        StatsOverlay::open(second);
        REQUIRE(listener.calls.size() == 5);
        CHECK((listener.calls[4].canvas == second));
        StatsOverlay::close(second);
        CHECK((listener.calls[5].canvas == second));
        delete second;
    }
    CHECK(listener.calls.size() == 6);

    // a handler deleted is called no more
    {
        OpenListener gone;

        StatsOverlay::open(scene.canvas);
        CHECK(gone.calls.size() == 1);
    }
    StatsOverlay::close(scene.canvas);
    CHECK(listener.calls.size() == 8);
    scene.frames(2);
}

TEST_CASE("StatsOverlay: setBottomReserve keeps the canvas's overlay above a bar")
{
    Scene scene;
    constexpr int Bar=22;

    CHECK(StatsOverlay::bottomReserve(scene.canvas) == 0);

    StatsOverlay *overlay=StatsOverlay::open(scene.canvas);

    scene.frames(2);

    const Rect atRest=overlay->getBounds();
    const Rect corner=scene.corner->getBounds();
    const Rect other=scene.other->getBounds();

    CHECK(atRest.y+atRest.h == scene.canvas->getHeight()-StatsOverlay::Inset);

    StatsOverlay::setBottomReserve(scene.canvas, Bar);
    CHECK(StatsOverlay::bottomReserve(scene.canvas) == Bar);
    // the overlay alone is laid out again
    CHECK(overlay->needsLayout());
    CHECK_FALSE(scene.corner->needsLayout());
    CHECK_FALSE(scene.other->needsLayout());
    scene.frame();
    CHECK(scene.canvas->getLayoutStats().visited == 0);
    CHECK(scene.canvas->previousFrameStats().layout.visited == 0);
    CHECK(overlay->getX() == atRest.x);
    CHECK(overlay->getY()+overlay->getHeight() == scene.canvas->getHeight()-StatsOverlay::Inset-Bar);
    CHECK(overlay->getWidth() == atRest.w);
    CHECK(overlay->getHeight() == atRest.h);
    CHECK((scene.corner->getBounds() == corner));
    CHECK((scene.other->getBounds() == other));
    // the same again changes nothing
    StatsOverlay::setBottomReserve(scene.canvas, Bar);
    CHECK_FALSE(overlay->needsLayout());

    // kept for the canvas: an overlay opened later takes it, at a new size too
    StatsOverlay::close(scene.canvas);
    scene.frames(2);
    overlay=StatsOverlay::open(scene.canvas);
    scene.canvas->setSize(640, 480);
    scene.frames(2);
    CHECK(overlay->getY()+overlay->getHeight() == 480-StatsOverlay::Inset-Bar);
    CHECK(overlay->getX()+overlay->getWidth() == 640-StatsOverlay::Inset);

    // an overlay on a page of the app's keeps to its parent's corner
    controls::Base *page=scene.canvas->newChild<controls::Base>("page");

    page->setBounds(0, 0, 500, 400);

    StatsOverlay *onPage=page->newChild<StatsOverlay>();

    scene.frames(2);
    CHECK(onPage->getY()+onPage->getHeight() == 400-StatsOverlay::Inset);

    // a negative reserve is none, and 0 forgets it
    StatsOverlay::setBottomReserve(scene.canvas, -5);
    CHECK(StatsOverlay::bottomReserve(scene.canvas) == 0);
    scene.frames(2);
    CHECK(overlay->getY()+overlay->getHeight() == 480-StatsOverlay::Inset);
    StatsOverlay::close(scene.canvas);
    scene.frames(2);
}

TEST_CASE("StatsOverlay: a canvas made after one with a reserve starts at 0")
{
    for(int round=0; round < 4; ++round)
    {
        Scene scene;

        CAPTURE(round);
        // the last round's canvas may be at this address: its reserve is not this one's
        CHECK(StatsOverlay::bottomReserve(scene.canvas) == 0);
        StatsOverlay::setBottomReserve(scene.canvas, 30);
        CHECK(StatsOverlay::bottomReserve(scene.canvas) == 30);
    }
    CHECK(StatsOverlay::bottomReserve(nullptr) == 0);
}

namespace
{

//! A bar along its parent's bottom that, as the demo shell does, keeps the
//! canvas's overlay clear of it from its parent's Y arrange.
class ReservingPanel: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(ReservingPanel, controls::Base)
    {
        setDock(DockPosition::Center);
        m_bar=newChild<controls::Base>("bar");
        m_bar->setDock(DockPosition::Bottom);
        m_bar->setSizeFlags({SizeFlag::Expand, SizeFlag::Fixed});
        m_bar->setHeight(BarHeight);
    }

    static constexpr int BarHeight=19;

protected:
    void arrange(skin::Base *skin, Dim dim) override
    {
        ParentClass::arrange(skin, dim);
        if(dim == Dim::Y)
        {
            controls::Canvas *canvas=getCanvas();

            StatsOverlay::setBottomReserve(canvas, canvas->getHeight()-m_bar->localPosToCanvas().y);
        }
    }

private:
    controls::Base *m_bar=nullptr;
};

}//namespace

TEST_CASE("StatsOverlay: a reserve set from a layout pass is in effect in that pass")
{
    controls::setLayoutVerify(true);

    const size_t verifyFailures=controls::layoutVerifyFailures();

    // opened before the panel, so arranged before it in the first pass; then
    // a panel made over a settled overlay; then the reserve given back
    for(bool settledFirst : {false, true})
    {
        CAPTURE(settledFirst);
        Scene scene;
        StatsOverlay *overlay=StatsOverlay::open(scene.canvas);

        if(settledFirst)
        {
            scene.frames(3);
            REQUIRE(overlay->getY()+overlay->getHeight() == scene.canvas->getHeight()-StatsOverlay::Inset);
        }

        ReservingPanel *panel=scene.canvas->newChild<ReservingPanel>();

        scene.frame();
        CHECK(scene.canvas->getLayoutStats().passes == 1);
        CHECK(StatsOverlay::bottomReserve(scene.canvas) == ReservingPanel::BarHeight);
        CHECK(overlay->getY()+overlay->getHeight()
            == scene.canvas->getHeight()-StatsOverlay::Inset-ReservingPanel::BarHeight);
        scene.frames(2);
        CHECK(scene.canvas->getLayoutStats().passes == 0);

        delete panel;
        StatsOverlay::setBottomReserve(scene.canvas, 0);
        scene.frame();
        CHECK(overlay->getY()+overlay->getHeight() == scene.canvas->getHeight()-StatsOverlay::Inset);
        StatsOverlay::close(scene.canvas);
        scene.frames(2);
    }
    CHECK(controls::layoutVerifyFailures() == verifyFailures);
    controls::setLayoutVerify(false);
}
