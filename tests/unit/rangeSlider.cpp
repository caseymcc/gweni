/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  controls::RangeSlider on a StubRenderer/StubSkin canvas, driven through
 *  the canvas's input, a frame after each step:
 *  - setValues clamps to the range, orders the two values, keeps the gap (the
 *    high value moved up, the low one down at the maximum), snaps to the step,
 *    and fires nothing, marking nothing when the values are the ones it has;
 *  - a drag of either thumb one unit at a time moves it by exactly that unit
 *    (its position rounded from the value read off it), stops it at the other
 *    thumb's value less or plus the gap, never moving the other, and at the
 *    range's ends;
 *  - a press on the track moves the nearer thumb there, centred on the mouse,
 *    and drags it on; two thumbs at one value are told apart by the drag's
 *    first move (toward the minimum the low one), on the track or on a thumb;
 *  - a step and notches keep the values on their grid, in a drag and a key;
 *  - each thumb's keys: Left/Down and Right/Up a step, PageUp/PageDown ten,
 *    Home/End to its limits; Tab goes low then high;
 *  - onValueChanged fires once per real change with the thumb in `integer`,
 *    then onLowChanged or onHighChanged, and never from set*;
 *  - the per-thumb and joined value texts are set only when their strings
 *    change (a drag that keeps them sets none);
 *  - the geometry, horizontal and vertical: the thumbs' bounds, the travel,
 *    the fill between their centres, a vertical text column;
 *  - disabled, a press, a drag and keys change nothing and take no focus;
 *  - with the textured RangeSlider and RangeSliderThumb renderers: the
 *    thumbs are drawn at the slider's z+2 and z+3, the front one (dragged or
 *    focused last) at z+3 and hit first where they overlap, the focused one
 *    alone with RangeSliderThumbFocus's border; a hover writes the hovered
 *    thumb's slot alone, a drag step the thumb's and the slider's (its track,
 *    which the stub counts as written, and its fill), never the other thumb's.
 */
#include "headlessFixture.h"

#include <gweni/controls/rangeSlider.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/inputEvent.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace gweni;

namespace
{

constexpr int Origin=20;
constexpr int Thumb=controls::RangeSlider::ThumbLength;

float nextClock()
{
    static float start=900000.0f;
    start+=100000.0f;
    return start;
}

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::RangeSlider *slider=nullptr;
    float clock=nextClock();
    bool horizontal;
    //! Each onValueChanged: the thumb and the low and high values then.
    struct Change
    {
        size_t thumb;
        double low, high;
    };
    std::vector<Change> changes;
    int lowChanged=0;
    int highChanged=0;

    struct Listener: public event::Handler
    {
        Scene *scene=nullptr;
        void valueChanged(event::Info info)
        {
            scene->changes.push_back({info.integer, scene->slider->getLow(), scene->slider->getHigh()});
        }
        void lowChange(event::Info) { ++scene->lowChanged; }
        void highChange(event::Info) { ++scene->highChanged; }
    } listener;

    explicit Scene(bool horizontal_=true, int travel=100): horizontal(horizontal_)
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 400);
        slider=canvas->newChild<controls::RangeSlider>();
        slider->setHorizontal(horizontal);
        if(horizontal)
            slider->setBounds(Origin, Origin, travel+Thumb, 20);
        else
            slider->setBounds(Origin, Origin, 20, travel+Thumb);
        slider->setRange(0, travel);
        slider->setValues(0, travel);
        listener.scene=this;
        slider->onValueChanged.add(&listener, &Listener::valueChanged);
        slider->onLowChanged.add(&listener, &Listener::lowChange);
        slider->onHighChanged.add(&listener, &Listener::highChange);
        frame();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        canvas->inputMouseButton(0, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    //! The thumb's position along the axis, in the slider's coordinates.
    int thumb(int i) const
    {
        const Rect rect=slider->getThumbRect(i);
        return horizontal?rect.x:rect.y;
    }

    //! The canvas point `along` units down the slider's axis, mid-way across.
    Point at(int along) const
    {
        return horizontal?Point(Origin+along, Origin+10):Point(Origin+10, Origin+along);
    }

    void moveTo(Point p)
    {
        canvas->inputMouseMoved(p.x, p.y, 0, 0);
        frame();
    }

    void press()
    {
        clock+=1.0f;
        canvas->inputMouseButton(0, true);
        frame();
    }

    void release()
    {
        canvas->inputMouseButton(0, false);
        frame();
    }

    void key(int k)
    {
        canvas->inputModifierKey(k, true);
        canvas->inputModifierKey(k, false);
        frame();
    }

    using Base=controls::Base;
};

std::string str(const Rect &r)
{
    return std::to_string(r.x)+","+std::to_string(r.y)+" "+std::to_string(r.w)+"x"+std::to_string(r.h);
}

}//namespace

TEST_CASE("RangeSlider: setValues clamps, orders, keeps the gap and fires nothing")
{
    Scene s;
    controls::RangeSlider *r=s.slider;

    r->setValues(-10, 50);
    CHECK(r->getLow() == 0);
    CHECK(r->getHigh() == 50);
    r->setValues(80, 20);
    CHECK(r->getLow() == 20);
    CHECK(r->getHigh() == 80);
    r->setValues(30, 200);
    CHECK(r->getHigh() == 100);

    r->setMinimumGap(10);
    r->setValues(40, 42);
    CHECK(r->getLow() == 40);
    CHECK(r->getHigh() == 50);
    // at the maximum the low one goes down
    r->setValues(97, 100);
    CHECK(r->getLow() == 90);
    CHECK(r->getHigh() == 100);
    // a gap past the range is the range
    r->setMinimumGap(500);
    CHECK(r->getMinimumGap() == 100);
    CHECK(r->getLow() == 0);
    CHECK(r->getHigh() == 100);
    r->setMinimumGap(0);

    r->setStep(25);
    r->setValues(30, 60);
    CHECK(r->getLow() == 25);
    CHECK(r->getHigh() == 50);
    r->setStep(0);
    CHECK(s.changes.empty());
    CHECK(s.lowChanged == 0);
    CHECK(s.highChanged == 0);

    // the same values again mark nothing
    s.frame();
    r->setValues(25, 50);
    CHECK(r->getStateChange() == controls::StateChange_Nothing);
    CHECK(s.slider->getThumb(0)->getStateChange() == controls::StateChange_Nothing);
}

static void dragEach(bool horizontal)
{
    Scene s(horizontal);
    controls::RangeSlider *r=s.slider;
    const int travel=100;

    r->setMinimumGap(20);
    r->setValues(30, 70);
    s.frame();

    // the low thumb toward the maximum (a vertical slider's minimum is at the bottom)
    const int lowStart=s.thumb(0);
    const int dir=horizontal?1:-1;
    const int grab=Thumb/2;

    CHECK(lowStart == (horizontal?30:travel-30));
    s.moveTo(s.at(lowStart+grab));
    s.press();
    CHECK(r->getDraggedThumb() == 0);
    CHECK(r->getThumb(0)->isDepressed());
    for(int i=1; i<=10; ++i)
    {
        s.moveTo(s.at(lowStart+grab+dir*i));
        CHECK(s.thumb(0) == lowStart+dir*i);
        CHECK(r->getLow() == doctest::Approx(30+i));
    }
    CHECK(s.changes.size() == 10);
    // on toward the high thumb: it stops at 70-20
    s.moveTo(s.at(lowStart+grab+dir*40));
    CHECK(r->getLow() == 50);
    CHECK(r->getHigh() == 70);
    CHECK(s.thumb(0) == (horizontal?50:travel-50));
    // past the minimum: 0
    s.moveTo(s.at(lowStart+grab-dir*200));
    CHECK(r->getLow() == 0);
    s.release();
    CHECK(!r->getThumb(0)->isDepressed());
    CHECK(r->getDraggedThumb() == -1);
    for(const Scene::Change &c:s.changes)
        CHECK(c.thumb == 0);
    CHECK(s.lowChanged == int(s.changes.size()));
    CHECK(s.highChanged == 0);

    // the high thumb down to the low one plus the gap, then past the maximum
    s.changes.clear();
    const int highStart=s.thumb(1);

    s.moveTo(s.at(highStart+grab));
    s.press();
    CHECK(r->getDraggedThumb() == 1);
    s.moveTo(s.at(highStart+grab-dir*1));
    CHECK(s.thumb(1) == highStart-dir);
    CHECK(r->getHigh() == doctest::Approx(69));
    s.moveTo(s.at(highStart+grab-dir*90));
    CHECK(r->getHigh() == 20);
    CHECK(r->getLow() == 0);
    s.moveTo(s.at(highStart+grab+dir*200));
    CHECK(r->getHigh() == 100);
    s.release();
    for(const Scene::Change &c:s.changes)
        CHECK(c.thumb == 1);
    CHECK(s.highChanged == int(s.changes.size()));
}

TEST_CASE("RangeSlider: a drag of each thumb, horizontal")
{
    dragEach(true);
}

TEST_CASE("RangeSlider: a drag of each thumb, vertical")
{
    dragEach(false);
}

TEST_CASE("RangeSlider: a press on the track moves the nearer thumb and drags it")
{
    for(bool horizontal:{true, false})
    {
        CAPTURE(horizontal);
        Scene s(horizontal);
        controls::RangeSlider *r=s.slider;
        auto pos=[&](int value) { return horizontal?value:100-value; };

        r->setValues(20, 80);
        s.frame();
        // nearer the low one: it moves there, centred on the mouse
        s.moveTo(s.at(pos(35)+Thumb/2));
        s.press();
        CHECK(r->getLow() == doctest::Approx(35));
        CHECK(r->getHigh() == doctest::Approx(80));
        CHECK(r->getDraggedThumb() == 0);
        CHECK(KeyboardFocus == r->getThumb(0));
        s.moveTo(s.at(pos(36)+Thumb/2));
        CHECK(r->getLow() == doctest::Approx(36));
        s.release();
        REQUIRE(s.changes.size() == 2);
        CHECK(s.changes[0].thumb == 0);

        // nearer the high one
        s.moveTo(s.at(pos(70)+Thumb/2));
        s.press();
        CHECK(r->getHigh() == doctest::Approx(70));
        CHECK(r->getDraggedThumb() == 1);
        CHECK(KeyboardFocus == r->getThumb(1));
        s.release();

        // two thumbs at one value: a press on them waits for the drag's way
        for(bool towardMin:{true, false})
        {
            CAPTURE(towardMin);
            r->setValues(50, 50);
            s.frame();
            s.changes.clear();
            s.moveTo(s.at(pos(50)+Thumb/2));
            s.press();
            CHECK(r->getLow() == doctest::Approx(50));
            CHECK(r->getHigh() == doctest::Approx(50));
            CHECK(s.changes.empty());
            s.moveTo(s.at(pos(towardMin?47:53)+Thumb/2));
            CHECK(r->getDraggedThumb() == (towardMin?0:1));
            CHECK(KeyboardFocus == r->getThumb(towardMin?0:1));
            CHECK(r->getLow() == doctest::Approx((towardMin?47:50)));
            CHECK(r->getHigh() == doctest::Approx((towardMin?50:53)));
            s.release();
            // and on the track beside them, on the side it goes
            r->setValues(50, 50);
            s.frame();
            s.moveTo(s.at(pos(towardMin?10:90)+Thumb/2));
            s.press();
            CHECK(r->getLow() == doctest::Approx((towardMin?10:50)));
            CHECK(r->getHigh() == doctest::Approx((towardMin?50:90)));
            s.release();
        }
    }
}

TEST_CASE("RangeSlider: a step and notches keep the values on their grid")
{
    Scene s;
    controls::RangeSlider *r=s.slider;

    r->setStep(10);
    r->setValues(20, 80);
    s.frame();
    const int start=s.thumb(0);

    s.moveTo(s.at(start+Thumb/2));
    s.press();
    s.moveTo(s.at(start+Thumb/2+4));
    CHECK(r->getLow() == 20);
    CHECK(s.thumb(0) == 20);
    s.moveTo(s.at(start+Thumb/2+6));
    CHECK(r->getLow() == 30);
    CHECK(s.thumb(0) == 30);
    s.release();
    s.key(keys::Right);
    CHECK(r->getLow() == 40);
    s.key(keys::Left);
    s.key(keys::Left);
    CHECK(r->getLow() == 20);

    r->setStep(0);
    r->setNotchCount(4);
    r->setClampToNotches(true);
    CHECK(r->getGrid() == 25);
    CHECK(r->getLow() == 25);
    CHECK(r->getHigh() == 75);
    s.key(keys::Right);
    CHECK(r->getLow() == 50);
}

TEST_CASE("RangeSlider: each thumb's keys")
{
    Scene s;
    controls::RangeSlider *r=s.slider;

    r->setMinimumGap(10);
    r->setValues(30, 60);
    s.frame();
    r->getThumb(0)->focus();
    s.frame();

    s.key(keys::Right);
    CHECK(r->getLow() == doctest::Approx(31));
    s.key(keys::Up);
    CHECK(r->getLow() == doctest::Approx(32));
    s.key(keys::Left);
    s.key(keys::Down);
    CHECK(r->getLow() == doctest::Approx(30));
    s.key(keys::PageUp);
    CHECK(r->getLow() == doctest::Approx(40));
    s.key(keys::PageDown);
    CHECK(r->getLow() == doctest::Approx(30));
    s.key(keys::End);
    CHECK(r->getLow() == 50);
    s.key(keys::Home);
    CHECK(r->getLow() == 0);
    CHECK(r->getHigh() == 60);
    CHECK(s.highChanged == 0);

    // Tab goes on to the high thumb
    s.key(keys::Tab);
    CHECK(KeyboardFocus == r->getThumb(1));
    CHECK(r->getFrontThumb() == 1);
    s.key(keys::Home);
    CHECK(r->getHigh() == 10);
    s.key(keys::End);
    CHECK(r->getHigh() == 100);
    s.key(keys::Right);
    CHECK(r->getHigh() == 100);
    for(const Scene::Change &c:s.changes)
        CHECK(c.thumb == (&c < &s.changes[8]?0u:1u));
    CHECK(s.changes.size() == 10);
}

TEST_CASE("RangeSlider: the value texts are set only when their strings change")
{
    for(controls::RangeSlider::ValueTextMode mode:
        {controls::RangeSlider::ValueTextMode::PerThumb, controls::RangeSlider::ValueTextMode::Joined})
    {
        Scene s(true, 200);
        controls::RangeSlider *r=s.slider;
        const bool joined=(mode == controls::RangeSlider::ValueTextMode::Joined);

        CHECK(!r->getValueTextControl(0));
        r->setValueTextMode(mode);
        CHECK(r->setValueFormat("%.0f"));
        CHECK(!r->setValueFormat("%d"));
        r->setRange(0, 100);
        r->setValues(20, 80);
        s.frame();
        if(joined)
        {
            CHECK(r->getValueText(0) == "20 \xE2\x80\x93 80");
            CHECK(r->getValueTextControl(0) == r->getValueTextControl(1));
        }
        else
        {
            CHECK(r->getValueText(0) == "20");
            CHECK(r->getValueText(1) == "80");
        }

        // 200 units over 0..100: two units a value step; one unit keeps "20"
        r->setValues(20.0, 80);
        s.frame();
        const int start=s.thumb(0);

        s.moveTo(s.at(start+Thumb/2));
        s.press();
        controls::resetChangeStats();
        s.moveTo(s.at(start+Thumb/2+1));
        CHECK(r->getLow() == doctest::Approx(20.5));
        // the frame rendered no text change (no glyph slot to write)
        CHECK(controls::getChangeStats().text == 0);
        controls::resetChangeStats();
        s.moveTo(s.at(start+Thumb/2+3));
        CHECK(r->getValueText(0) == (joined?"22 \xE2\x80\x93 80":"22"));
        CHECK(controls::getChangeStats().text == 1);
        s.release();
    }
}

TEST_CASE("RangeSlider: the geometry, horizontal and vertical")
{
    {
        Scene s(true, 100);
        controls::RangeSlider *r=s.slider;

        r->setValues(20, 60);
        s.frame();
        CHECK(r->getTravel() == 100);
        CHECK(str(r->getThumbRect(0)) == str(Rect(20, 0, Thumb, 20)));
        CHECK(str(r->getThumbRect(1)) == str(Rect(60, 0, Thumb, 20)));
        CHECK(str(r->getFillBounds()) == str(Rect(Origin+20+Thumb/2, Origin, 40, 20)));
    }
    {
        Scene s(false, 100);
        controls::RangeSlider *r=s.slider;

        r->setValues(20, 60);
        s.frame();
        CHECK(str(r->getThumbRect(0)) == str(Rect(0, 80, 20, Thumb)));
        CHECK(str(r->getThumbRect(1)) == str(Rect(0, 40, 20, Thumb)));
        CHECK(str(r->getFillBounds()) == str(Rect(Origin, Origin+40+Thumb/2, 20, 40)));
        // a text column right of the track
        r->setValueFormat("%.0f");
        s.frame();
        CHECK(r->getValueTextColumn() > 0);
        CHECK(r->getWidth() == 20+controls::RangeSlider::ValueTextGap+r->getValueTextColumn());
        CHECK(r->getTrackBounds().w == 20);
        CHECK(r->getValueTextControl(1)->getY() == 0);
        CHECK(r->getValueTextControl(0)->getBounds().bottom() == r->getHeight());
    }
}

TEST_CASE("RangeSlider: disabled takes no mouse or keys")
{
    Scene s;
    controls::RangeSlider *r=s.slider;

    r->setValues(30, 60);
    r->setDisabled(true);
    s.frame();
    CHECK(r->getThumb(0)->isDisabled());
    s.moveTo(s.at(30+Thumb/2));
    s.press();
    s.moveTo(s.at(40+Thumb/2));
    s.release();
    s.moveTo(s.at(5));
    s.press();
    s.release();
    CHECK(r->getLow() == 30);
    CHECK(KeyboardFocus != r->getThumb(0));
    r->getThumb(0)->focus();
    s.key(keys::Right);
    CHECK(r->getLow() == 30);
    CHECK(s.changes.empty());
}

namespace
{

//! Records the id, z and border width of each rounded rect drawn.
class RangeRecorder: public test::StubRenderer
{
public:
    struct Draw
    {
        size_t id;
        int z;
        float borderWidth;
    };

    void drawRoundedRect(size_t primitiveId, Rect rect, float radius, float borderWidth, Color top,
        Color bottom, Color border, int zIndex) override
    {
        draws.push_back({primitiveId, zIndex, borderWidth});
        StubRenderer::drawRoundedRect(primitiveId, rect, radius, borderWidth, top, bottom, border, zIndex);
    }

    //! The last draw of `id`.
    const Draw *last(size_t id) const
    {
        for(auto it=draws.rbegin(); it != draws.rend(); ++it)
            if(it->id == id)
                return &*it;
        return nullptr;
    }

    std::vector<Draw> draws;
};

class RangeSkin: public test::StubSkin
{
public:
    explicit RangeSkin(test::StubRenderer *renderer): StubSkin(renderer) {}

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == "RangeSlider")
            return std::make_unique<skin::textured::RangeSlider>();
        if(typeName == "RangeSliderThumb")
            return std::make_unique<skin::textured::RangeSliderThumb>();
        return StubSkin::getControlRenderer(typeName);
    }
};

}//namespace

TEST_CASE("RangeSlider: the textured thumbs' layers, focus look and written slots")
{
    RangeRecorder renderer;
    RangeSkin skin(&renderer);
    float clock=nextClock();

    platform::setTimeSource([&clock]() { return clock; });

    controls::Canvas *canvas=new controls::Canvas(&skin);
    canvas->setSize(400, 400);

    controls::RangeSlider *slider=canvas->newChild<controls::RangeSlider>();
    slider->setBounds(Origin, Origin, 100+Thumb, 20);
    slider->setRange(0, 100);
    slider->setValues(40, 60);

    auto frame=[&]() {
        renderer.draws.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    };
    auto slotOf=[](controls::Base *control, size_t i) { return control->getSkinControl()->getPrimitives().at(i); };

    frame();
    frame();

    const size_t low=slotOf(slider->getThumb(0), 0);
    const size_t high=slotOf(slider->getThumb(1), 0);
    const size_t fill=slotOf(slider, 1);
    const int z=slider->getZIndex();

    // drawn once, the high thumb in front
    frame();
    slider->getThumb(0)->redraw();
    slider->getThumb(1)->redraw();
    frame();
    REQUIRE(renderer.last(low));
    REQUIRE(renderer.last(high));
    CHECK(slider->getFrontThumb() == 1);
    CHECK(renderer.last(low)->z == z+2);
    CHECK(renderer.last(high)->z == z+3);

    // the low thumb focused: in front, alone with the focus border
    slider->getThumb(0)->focus();
    frame();
    REQUIRE(renderer.last(low));
    REQUIRE(renderer.last(high));
    CHECK(slider->getFrontThumb() == 0);
    CHECK(renderer.last(low)->z == z+3);
    CHECK(renderer.last(high)->z == z+2);
    CHECK(renderer.last(low)->borderWidth == skin::textured::tokyonight::RangeSliderThumbFocus.borderWidth);
    CHECK(renderer.last(high)->borderWidth == skin::textured::tokyonight::SliderThumb.borderWidth);

    // overlapping thumbs: the front one is hit
    slider->setValues(50, 50);
    frame();
    const Point both=slider->localPosToCanvas(Point(50+Thumb/2, 10));
    CHECK(canvas->getControlAt(both.x, both.y) == slider->getThumb(0));
    slider->getThumb(1)->focus();
    frame();
    CHECK(canvas->getControlAt(both.x, both.y) == slider->getThumb(1));
    slider->getThumb(1)->unfocus();
    slider->setValues(40, 60);
    frame();
    frame();

    // a hover writes the hovered thumb's slot alone
    const Point onHigh=slider->localPosToCanvas(Point(60+Thumb/2, 10));

    canvas->inputMouseMoved(onHigh.x, onHigh.y, 0, 0);
    frame();
    CHECK(std::set<size_t>(renderer.writtenPrimitiveIds().begin(), renderer.writtenPrimitiveIds().end())
        == std::set<size_t>{high});
    frame();
    CHECK(renderer.writtenPrimitiveIds().empty());

    // a drag step writes the thumb's and the fill's
    clock+=1.0f;
    canvas->inputMouseButton(0, true);
    frame();
    canvas->inputMouseMoved(onHigh.x+1, onHigh.y, 1, 0);
    frame();
    CHECK(slider->getHigh() == doctest::Approx(61));
    {
        // the stub counts every draw: the slider's update draws its track too,
        // whose bytes a SlotRenderer finds unchanged
        const std::set<size_t> written(renderer.writtenPrimitiveIds().begin(), renderer.writtenPrimitiveIds().end());

        CHECK(written.count(high) == 1);
        CHECK(written.count(fill) == 1);
        CHECK(written.count(low) == 0);
        CHECK(written.size() == 3);
    }
    canvas->inputMouseButton(0, false);
    frame();

    HoveredControl=nullptr;
    KeyboardFocus=nullptr;
    MouseFocus=nullptr;
    delete canvas;
    platform::setTimeSource(nullptr);
}
