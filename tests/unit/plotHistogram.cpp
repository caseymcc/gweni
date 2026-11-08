/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  PlotHistogram: a series drawn as bars from the bottom of a frame, one
 *  primitive slot per bar, redrawn only where a bar changed.
 *
 *  It runs the textured skin's real PlotHistogram renderer on a recording
 *  StubRenderer (a StubControl for every other type) and checks:
 *  - the bar rects for several widths, counts and ranges: bar i in the
 *    columns [i*W/N, (i+1)*W/N) of the plot, less a 1-unit gap where the
 *    column is 2 units wide or more, no column more than a unit wider than
 *    another, on the plot's bottom and inside it, as high as the value's share
 *    of the range rounded (nothing at or below the minimum, the whole height
 *    past the maximum), the automatic range from min(0, lowest) to the
 *    highest value, and getBarAt() finding the column of every x;
 *  - a setValue() that changes a bar's height drawing that bar's slot alone,
 *    one that keeps it marking and drawing nothing, one to no height hiding
 *    the slot alone, and a settled frame drawing nothing;
 *  - a new range (fixed, or the automatic one moved by a value) redrawing
 *    every bar and not the frame;
 *  - a new bar count resizing the slot list and drawing into no slot it does
 *    not own, the slots given back when it shrinks and when the plot goes;
 *  - the hovered bar's tooltip ("<index>: <value>") shown after the canvas's
 *    delay, nothing marked or drawn by a move inside the bar, a move to the
 *    next bar redrawing just the two bars and changing the shown text at
 *    once, the text following the hovered bar's value, and no tooltip outside
 *    the plot;
 *  - hiding and showing the plot, and its caption, a Text child numbered
 *    after the bars and placed at the plot's top centre.
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/controls/plotHistogram.h>
#include <gweni/controls/text.h>
#include <gweni/controls/toolTip.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedControls.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <vector>

using namespace gweni;

namespace
{

//! Records every slot drawn into and every slot hidden.
class DrawRecorder: public test::StubRenderer
{
public:
    void drawRoundedRect(size_t primitiveId, Rect, float, float, Color top, Color, Color,
        int zIndex) override
    {
        drawn.push_back(primitiveId);
        colors[primitiveId]=top;
        zIndices[primitiveId]=zIndex;
    }

    void hidePrimitive(size_t primitiveId) override
    {
        hidden.push_back(primitiveId);
    }

    void clear()
    {
        drawn.clear();
        hidden.clear();
        events.clear();
    }

    std::vector<size_t> drawn;
    std::vector<size_t> hidden;
    //! the fill and z each slot was last drawn with
    std::map<size_t, Color> colors;
    std::map<size_t, int> zIndices;
};

//! The textured skin's PlotHistogram renderer, a StubControl for the rest.
class PlotSkin: public test::StubSkin
{
public:
    explicit PlotSkin(test::StubRenderer *renderer): StubSkin(renderer) {}

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == "PlotHistogram")
            return std::make_unique<skin::textured::PlotHistogram>();
        return StubSkin::getControlRenderer(typeName);
    }
};

struct Scene
{
    explicit Scene(int width=200, int height=80)
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
        plot=canvas->newChild<controls::PlotHistogram>();
        plot->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        plot->setBounds(20, 30, width, height);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        // the hovered control is process-global
        HoveredControl=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    void frame()
    {
        renderer.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    //! Moves the mouse to \p local (the plot's coordinates) and renders a frame.
    void moveTo(int x, int y)
    {
        const Point at=plot->localPosToCanvas(Point(x, y));
        canvas->inputMouseMoved(at.x, at.y, 0, 0);
        frame();
    }

    void waitDelay()
    {
        clock+=canvas->getToolTipDelay();
        frame();
    }

    const std::vector<size_t> &ids() const
    {
        return plot->getSkinControl()->getPrimitives();
    }

    size_t barId(size_t bar) const
    {
        return ids().at(1+bar);
    }

    bool drewOnlyOwnSlots() const
    {
        const std::set<size_t> own(ids().begin(), ids().end());

        for(size_t id : renderer.drawn)
        {
            if(own.count(id) == 0)
                return false;
        }
        for(size_t id : renderer.hidden)
        {
            if(own.count(id) == 0)
                return false;
        }
        return true;
    }

    //! The slots of the plot drawn into this frame (the caption's Text and
    //! the tooltip box are StubControls, which draw nothing).
    std::set<size_t> drawnSet() const
    {
        return std::set<size_t>(renderer.drawn.begin(), renderer.drawn.end());
    }

    DrawRecorder renderer;
    PlotSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::PlotHistogram *plot=nullptr;
    float clock=10.0f;
};

std::vector<float> ramp(size_t count, float top)
{
    std::vector<float> values(count);

    for(size_t i=0; i < count; ++i)
        values[i]=top*float(i+1)/float(count);
    return values;
}

//! Every bar's rect against the documented columns, gap and heights.
void checkBars(const controls::PlotHistogram *plot)
{
    const Rect area=plot->getPlotBounds();
    const size_t count=plot->getValueCount();
    const float min=plot->getScaleMin();
    const float max=plot->getScaleMax();
    int narrowest=area.w, widest=0;

    for(size_t i=0; i < count; ++i)
    {
        CAPTURE(i);
        const int x0=int(i*size_t(area.w)/count);
        const int x1=int((i+1)*size_t(area.w)/count);
        const int column=x1-x0;
        const int width=(column >= 2)?column-1:column;
        const float value=plot->getValues()[i];
        int height=0;

        if(value >= max)
            height=area.h;
        else if(value > min)
            height=int(std::lround((value-min)/(max-min)*float(area.h)));

        const Rect bar=plot->getBarRect(i);

        narrowest=std::min(narrowest, column);
        widest=std::max(widest, column);
        CHECK(bar.x == area.x+x0);

        if((width == 0) || (height == 0))
        {
            CHECK(((bar.w == 0) || (bar.h == 0)));
            continue;
        }

        CHECK(bar.w == width);
        CHECK(bar.h == height);
        // on the bottom, inside the plot
        CHECK(bar.y+bar.h == area.y+area.h);
        CHECK(bar.y >= area.y);
        CHECK(bar.x+bar.w <= area.x+area.w);

        // the next bar starts after this one's gap: no overlap
        if(i+1 < count)
            CHECK(plot->getBarRect(i+1).x == bar.x+column);
    }

    // the remainder is spread a unit at a time
    if(count > 0)
        CHECK(widest-narrowest <= 1);

    // every x of the plot is in the column getBarAt() names
    for(int x=area.x; x < area.x+area.w; ++x)
    {
        CAPTURE(x);
        const int bar=plot->getBarAt(Point(x, area.y));

        REQUIRE(bar >= 0);
        const int x0=int(size_t(bar)*size_t(area.w)/count);
        const int x1=int(size_t(bar+1)*size_t(area.w)/count);

        CHECK(x-area.x >= x0);
        CHECK(x-area.x < x1);
    }
    CHECK(plot->getBarAt(Point(area.x-1, area.y)) == -1);
    CHECK(plot->getBarAt(Point(area.x+area.w, area.y)) == -1);
    CHECK(plot->getBarAt(Point(area.x, area.y+area.h)) == -1);
}

}//namespace

TEST_CASE("PlotHistogram: bar rects split the plot in whole units, on its bottom and inside it")
{
    for(int width : {200, 101, 37, 12})
    {
        for(size_t count : {size_t(1), size_t(3), size_t(7), size_t(32), size_t(50)})
        {
            CAPTURE(width);
            CAPTURE(count);
            Scene scene(width, 60);

            scene.plot->setScaleRange(0.0f, 10.0f);
            scene.plot->setValues(ramp(count, 12.0f));
            scene.frame();

            const Rect area=scene.plot->getPlotBounds();

            // the frame's padding all round
            CHECK((area == Rect(2, 2, width-4, 56)));
            checkBars(scene.plot);
        }
    }

    Scene scene;

    // a fixed range: at or below the minimum nothing, past the maximum all
    scene.plot->setScaleRange(1.0f, 5.0f);
    scene.plot->setValues({0.0f, 1.0f, 3.0f, 5.0f, 9.0f, -4.0f});
    scene.frame();
    checkBars(scene.plot);
    CHECK(scene.plot->getBarRect(0).h == 0);
    CHECK(scene.plot->getBarRect(1).h == 0);
    CHECK(scene.plot->getBarRect(2).h == 38);
    CHECK(scene.plot->getBarRect(3).h == 76);
    CHECK(scene.plot->getBarRect(4).h == 76);
    CHECK(scene.plot->getBarRect(5).h == 0);

    // the automatic range: from 0 to the highest value
    scene.plot->setAutoScale();
    scene.plot->setValues({2.0f, 4.0f, 8.0f});
    CHECK(scene.plot->getScaleMin() == 0.0f);
    CHECK(scene.plot->getScaleMax() == 8.0f);
    scene.frame();
    checkBars(scene.plot);
    CHECK(scene.plot->getBarRect(2).h == 76);
    CHECK(scene.plot->getBarRect(1).h == 38);

    // ... and from a negative lowest value
    scene.plot->setValues({-2.0f, 0.0f, 2.0f});
    CHECK(scene.plot->getScaleMin() == -2.0f);
    CHECK(scene.plot->getScaleMax() == 2.0f);
    checkBars(scene.plot);
    CHECK(scene.plot->getBarRect(0).h == 0);
    CHECK(scene.plot->getBarRect(1).h == 38);

    // all equal: a range of 1 above them
    scene.plot->setValues({0.0f, 0.0f});
    CHECK(scene.plot->getScaleMax() == 1.0f);
    checkBars(scene.plot);
}

TEST_CASE("PlotHistogram: a setValue draws only its bar's slot, and nothing when the height stays")
{
    Scene scene;

    scene.plot->setScaleRange(0.0f, 1.0f);
    scene.plot->setValues(ramp(32, 1.0f));

    // the first frame draws the frame and every bar
    scene.frame();
    REQUIRE(scene.ids().size() == 1+32);
    CHECK(scene.drawnSet().size() == 1+32);
    CHECK(scene.drewOnlyOwnSlots());

    // settled: nothing
    scene.frame();
    CHECK(scene.renderer.drawn.empty());
    CHECK(scene.renderer.hidden.empty());

    // a new height: that bar alone
    scene.plot->setValue(5, 0.9f);
    CHECK((scene.plot->getStateChange() & controls::StateChange_Background) != 0);
    scene.frame();
    CHECK(scene.renderer.drawn == std::vector<size_t>{scene.barId(5)});
    CHECK(scene.renderer.hidden.empty());

    // a value whose bar keeps its height (76*0.9 and 76*0.895 both round to 68):
    // nothing marked, nothing drawn
    CHECK(scene.plot->getBarRect(5).h == 68);
    scene.plot->setValue(5, 0.895f);
    CHECK(scene.plot->getBarRect(5).h == 68);
    CHECK(scene.plot->getStateChange() == controls::StateChange_Nothing);
    scene.frame();
    CHECK(scene.renderer.drawn.empty());
    CHECK(scene.renderer.hidden.empty());

    // the same value: nothing; an index past the series: ignored
    scene.plot->setValue(5, 0.895f);
    scene.plot->setValue(32, 0.5f);
    CHECK(scene.plot->getStateChange() == controls::StateChange_Nothing);
    CHECK(scene.plot->getValueCount() == 32);

    // down to no height: that bar's slot hidden, nothing drawn
    scene.plot->setValue(7, 0.0f);
    scene.frame();
    CHECK(scene.renderer.drawn.empty());
    CHECK(scene.renderer.hidden == std::vector<size_t>{scene.barId(7)});

    // and back: drawn again
    scene.plot->setValue(7, 0.5f);
    scene.frame();
    CHECK(scene.renderer.drawn == std::vector<size_t>{scene.barId(7)});
    CHECK(scene.renderer.hidden.empty());

    // the bars are in front of the frame, which is at the plot's z
    CHECK(scene.renderer.zIndices[scene.ids()[0]] == scene.plot->getZIndex());
    CHECK(scene.renderer.zIndices[scene.barId(7)] == scene.plot->getZIndex()+1);
}

TEST_CASE("PlotHistogram: a new range redraws every bar, not the frame")
{
    Scene scene;

    scene.plot->setScaleRange(0.0f, 1.0f);
    scene.plot->setValues(ramp(32, 1.0f));
    scene.frame();

    std::set<size_t> bars;

    for(size_t i=0; i < 32; ++i)
        bars.insert(scene.barId(i));

    // a fixed range twice as high: every bar is half as high
    scene.plot->setScaleRange(0.0f, 2.0f);
    scene.frame();
    CHECK(scene.drawnSet() == bars);

    // the same range again: nothing
    scene.plot->setScaleRange(0.0f, 2.0f);
    CHECK(scene.plot->getStateChange() == controls::StateChange_Nothing);

    // automatic: 0 to the highest value, 1.0, so every bar grows back
    scene.plot->setAutoScale();
    CHECK_FALSE(scene.plot->hasFixedScale());
    CHECK(scene.plot->getScaleMax() == 1.0f);
    scene.frame();
    CHECK(scene.drawnSet() == bars);

    // a value past the automatic range rescales every bar
    scene.plot->setValue(3, 4.0f);
    CHECK(scene.plot->getScaleMax() == 4.0f);
    scene.frame();
    CHECK(scene.drawnSet() == bars);

    // and lowering the highest value again does too
    scene.plot->setValue(3, 0.1f);
    CHECK(scene.plot->getScaleMax() == 1.0f);
    scene.frame();
    CHECK(scene.drawnSet() == bars);

    // the automatic range it already has, made fixed: nothing moves
    scene.plot->setScaleRange(0.0f, 1.0f);
    CHECK(scene.plot->hasFixedScale());
    CHECK(scene.plot->getStateChange() == controls::StateChange_Nothing);
}

TEST_CASE("PlotHistogram: a new bar count resizes the slot list, drawing only into its own slots")
{
    const test::IdBankCounts baseline=test::IdBankCounts::now();

    {
        Scene scene;

        scene.plot->setScaleRange(0.0f, 1.0f);
        scene.plot->setValues(ramp(32, 1.0f));
        scene.frame();
        CHECK(scene.ids().size() == 1+32);
        CHECK(scene.drewOnlyOwnSlots());

        const size_t frame=scene.ids()[0];

        // more bars: the list grows, the frame's slot kept, every bar drawn
        scene.plot->setValues(ramp(40, 1.0f));
        scene.frame();
        CHECK(scene.ids().size() == 1+40);
        CHECK(scene.ids()[0] == frame);
        CHECK(scene.drewOnlyOwnSlots());
        CHECK_FALSE(test::hasDuplicates(scene.ids()));
        for(size_t i=0; i < 40; ++i)
            CHECK(scene.drawnSet().count(scene.barId(i)) == 1);

        // fewer: the extra slots go back to the bank
        scene.plot->setValues(ramp(10, 1.0f));
        scene.frame();
        CHECK(scene.ids().size() == 1+10);
        CHECK(scene.ids()[0] == frame);
        CHECK(scene.drewOnlyOwnSlots());

        // none: the frame alone
        scene.plot->setValues({});
        scene.frame();
        CHECK(scene.ids().size() == 1);
        CHECK(scene.drewOnlyOwnSlots());

        // the same (empty) series again: not a change
        scene.frame();
        scene.plot->setValues({});
        CHECK(scene.plot->getStateChange() == controls::StateChange_Nothing);

        CHECK(scene.renderer.doubleAcquired == 0);
        CHECK(scene.renderer.doubleReleased == 0);
    }

    // the plot gone with its canvas, every id it took is back
    CHECK(test::IdBankCounts::now().livePrimitives() == baseline.livePrimitives());
}

TEST_CASE("PlotHistogram: the hovered bar's tooltip, unmoved by a move inside the bar")
{
    Scene scene;

    scene.plot->setScaleRange(0.0f, 1.0f);
    scene.plot->setValues({0.25f, 0.5f, 0.75f, 1.0f});
    scene.frame();

    // the plot is 196 units wide, 49 per column
    const Rect area=scene.plot->getPlotBounds();
    const int column=area.w/4;

    // onto bar 1: its look changes, no tooltip until the delay has passed
    scene.moveTo(area.x+column+10, area.y+area.h-5);
    CHECK(scene.plot->getHoveredBar() == 1);
    CHECK(scene.plot->getToolTipText() == "1: 0.5");
    CHECK(scene.renderer.drawn == std::vector<size_t>{scene.barId(1)});
    CHECK((scene.renderer.colors[scene.barId(1)] == skin::textured::tokyonight::PlotBarHover.top));
    CHECK_FALSE(scene.canvas->toolTipShown());

    scene.waitDelay();
    REQUIRE(scene.canvas->toolTipShown());
    controls::ToolTip *box=scene.canvas->toolTipControl();
    REQUIRE(box);
    CHECK(box->getText() == "1: 0.5");

    const Rect boxBounds=box->getBounds();

    // a move inside the same bar's column, above the bar too: nothing marked,
    // nothing drawn, the box where it was
    scene.renderer.clear();
    const Point inside=scene.plot->localPosToCanvas(Point(area.x+column+30, area.y+2));
    scene.canvas->inputMouseMoved(inside.x, inside.y, 0, 0);
    CHECK(scene.plot->getStateChange() == controls::StateChange_Nothing);
    scene.frame();
    CHECK(scene.renderer.drawn.empty());
    CHECK(scene.plot->getHoveredBar() == 1);
    CHECK(box->getText() == "1: 0.5");
    CHECK((box->getBounds() == boxBounds));

    // to bar 2: the two bars redrawn, the shown text changed at once
    scene.moveTo(area.x+2*column+5, area.y+area.h-5);
    CHECK(scene.plot->getHoveredBar() == 2);
    CHECK(scene.drawnSet() == std::set<size_t>{scene.barId(1), scene.barId(2)});
    CHECK((scene.renderer.colors[scene.barId(1)] == skin::textured::tokyonight::PlotBar.top));
    REQUIRE(scene.canvas->toolTipShown());
    CHECK(scene.canvas->toolTipControl()->getText() == "2: 0.75");

    // the hovered bar's value changes: so does its tooltip
    scene.plot->setValue(2, 0.125f);
    CHECK(scene.plot->getToolTipText() == "2: 0.125");
    scene.frame();
    CHECK(scene.canvas->toolTipControl()->getText() == "2: 0.125");

    // into the padding, out of the plot: no bar, no tooltip
    scene.moveTo(1, 1);
    CHECK(scene.plot->getHoveredBar() == -1);
    CHECK(scene.plot->getToolTipText().empty());
    CHECK(scene.drawnSet() == std::set<size_t>{scene.barId(2)});
    CHECK_FALSE(scene.canvas->toolTipShown());

    // back onto a bar, then off the plot altogether
    scene.moveTo(area.x+5, area.y+area.h-5);
    CHECK(scene.plot->getHoveredBar() == 0);
    scene.canvas->inputMouseMoved(380, 280, 0, 0);
    scene.frame();
    CHECK(scene.plot->getHoveredBar() == -1);
    CHECK(scene.drawnSet() == std::set<size_t>{scene.barId(0)});

    // the hovered bar dropped with the values it was in
    scene.moveTo(area.x+3*column+5, area.y+area.h-5);
    CHECK(scene.plot->getHoveredBar() == 3);
    scene.plot->setValues({0.5f, 0.5f});
    CHECK(scene.plot->getHoveredBar() == -1);
    CHECK(scene.plot->getToolTipText().empty());
}

TEST_CASE("PlotHistogram: hidden and shown, and its caption at the top centre over the bars")
{
    Scene scene;

    scene.plot->setScaleRange(0.0f, 1.0f);
    scene.plot->setValues({0.0f, 0.5f, 1.0f});
    scene.frame();

    // hidden: every slot hidden, nothing drawn
    scene.plot->setHidden(true);
    scene.frame();
    CHECK(scene.renderer.drawn.empty());
    CHECK(std::set<size_t>(scene.renderer.hidden.begin(), scene.renderer.hidden.end())
        == std::set<size_t>(scene.ids().begin(), scene.ids().end()));

    // a change while hidden draws nothing
    scene.plot->setValue(1, 0.25f);
    scene.frame();
    CHECK(scene.renderer.drawn.empty());

    // shown: the frame and the two bars with a height drawn, the empty one hidden
    scene.plot->setHidden(false);
    scene.frame();
    CHECK(scene.drawnSet() == std::set<size_t>{scene.ids()[0], scene.barId(1), scene.barId(2)});
    CHECK(scene.renderer.hidden == std::vector<size_t>{scene.barId(0)});

    // a caption: a Text child, numbered after the bars' z
    CHECK(scene.plot->getCaptionControl() == nullptr);
    scene.plot->setCaption("frame time");
    scene.frame();

    controls::internal::Text *caption=scene.plot->getCaptionControl();

    REQUIRE(caption);
    CHECK(scene.plot->getCaption() == "frame time");
    CHECK(caption->getParent() == scene.plot);
    CHECK(caption->getZIndex() >= scene.plot->getZIndex()+2);

    // at the plot's top centre
    const Rect area=scene.plot->getPlotBounds();
    const Rect bounds=caption->getBounds();

    CHECK(bounds.y == area.y);
    CHECK(std::abs((bounds.x+bounds.w/2)-(area.x+area.w/2)) <= 1);

    // a new caption reuses the Text; an empty one removes it
    scene.plot->setCaption("other");
    CHECK(scene.plot->getCaptionControl() == caption);
    scene.plot->setCaption("");
    CHECK(scene.plot->getCaptionControl() == nullptr);
    scene.frame();
    CHECK(scene.plot->getCaption().empty());
}
