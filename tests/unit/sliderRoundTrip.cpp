/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A slider's thumb position is a pure function of its value: updateBarFromValue()
 *  places it at lround(travel*v), or lround(travel*(1-v)) for a vertical slider,
 *  v the normalised value, kept within the travel (the control's length less the
 *  15-unit thumb). A drag or a click reads the value off where it put the thumb
 *  and places the thumb again from that value, so with rounding the thumb ends
 *  exactly where the mouse put it (issue #466: the truncating cast left it a unit
 *  short on some positions, and a one-unit step then did not move it).
 *
 *  For every travel from 1 to 400 units, horizontal and vertical, driven by real
 *  mouse input on the StubRenderer/StubSkin canvas, each with a frame after it:
 *  - a drag one unit at a time from one end to the other and back puts the thumb
 *    at each position p in turn (so every step moves it exactly one unit), and
 *    past the ends keeps it at the end; getFloatValue() is the value read off p;
 *  - a click on the track places the thumb at the click (the thumb centred on
 *    the mouse) wherever the thumb does not cover the click point;
 *  - setFloatValue() of k/(2*travel) for every k (every position and every half
 *    position between them) places the thumb at the rounded position, and a
 *    layout pass puts it back there;
 *  - clamped to 3, 4, 10 or 7 notches, a drag to each p snaps the thumb to the
 *    rounded position of the notch nearest p, and so does setFloatValue().
 *  Plus SliderDemo's 25 of 0..100 over its 135-unit travel: 33.75 rounds to 34.
 */
#include "headlessFixture.h"

#include <gweni/controls/horizontalSlider.h>
#include <gweni/controls/verticalSlider.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

using namespace gweni;

namespace
{

constexpr int ThumbLength=15;
constexpr int MaxTravel=400;
constexpr int Origin=20;

//! The double-click state is process-global: each scene starts its clock far
//! past the last one's, and every press is a second after the one before.
float nextClock()
{
    static float start=5000.0f;
    start+=100000.0f;
    return start;
}

//! The first few mismatches of a sweep, so one failing CHECK says which.
struct Mismatches
{
    size_t count=0;
    std::ostringstream first;

    void add(const std::string &what)
    {
        if(count++ < 8)
            first<<"\n  "<<what;
    }

    std::string text() const
    {
        return std::to_string(count)+" mismatches:"+first.str();
    }
};

template<typename SliderType>
struct Scene
{
    static constexpr bool Horizontal=std::is_same<SliderType, controls::HorizontalSlider>::value;

    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::Slider *slider=nullptr;
    float clock=nextClock();
    int travel=0;

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(MaxTravel+ThumbLength+2*Origin, MaxTravel+ThumbLength+2*Origin);
        slider=canvas->newChild<SliderType>();
        setTravel(1);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        // the hovered control, the focus and the button state are process-global
        input::onMouseClicked(canvas, 0, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    void frame()
    {
        renderer.events.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    //! The slider's length along its axis is the travel plus the thumb.
    void setTravel(int units)
    {
        travel=units;
        if(Horizontal)
            slider->setBounds(Origin, Origin, travel+ThumbLength, 20);
        else
            slider->setBounds(Origin, Origin, 20, travel+ThumbLength);
        frame();
        REQUIRE(along(slider)-along(slider->getSliderBar()) == travel);
    }

    static int along(controls::Base *control)
    {
        return Horizontal?control->getWidth():control->getHeight();
    }

    //! The thumb's position along the axis, in the slider's coordinates.
    int thumb() const
    {
        return Horizontal?slider->getSliderBar()->getX():slider->getSliderBar()->getY();
    }

    //! The canvas point `along` units down the slider's axis, mid-way across it.
    Point canvasAt(int along) const
    {
        return Horizontal?Point(Origin+along, Origin+10):Point(Origin+10, Origin+along);
    }

    //! Where the thumb is placed from normalised value v: what #466 pins.
    int placed(float v) const
    {
        const float fraction=Horizontal?v:1.f-v;
        const long position=std::lround(float(travel)*fraction);

        return int(std::max(0L, std::min(long(travel), position)));
    }

    //! The normalised value read off thumb position p (calculateValue()).
    float readOff(int p) const
    {
        const float fraction=float(p)/float(travel);

        return Horizontal?fraction:1.f-fraction;
    }

    //! The value a slider clamped to `notches` snaps v to (setValueInternal).
    static float snapped(float v, int notches)
    {
        return std::floor(v*float(notches)+0.5f)/float(notches);
    }

    void moveTo(Point at)
    {
        canvas->inputMouseMoved(at.x, at.y, 0, 0);
    }

    void press()
    {
        clock+=1.0f;
        canvas->inputMouseButton(0, true);
    }

    void release()
    {
        canvas->inputMouseButton(0, false);
    }

    //! Drags the thumb from position 0 to the travel's end and back, one unit
    //! per frame, then past each end; `expected(p)` is where it must end up
    //! with the mouse at p.
    template<typename Expected>
    void dragSweep(Mismatches &bad, int notches, Expected expected)
    {
        // the thumb at 0 (a vertical slider's value 1), pressed at its centre
        slider->setFloatValue(Horizontal?slider->getMin():slider->getMax());
        frame();
        if(thumb() != 0)
        {
            bad.add("travel "+std::to_string(travel)+": setFloatValue did not put the thumb at 0");
            return;
        }

        const int grab=ThumbLength/2;

        moveTo(canvasAt(grab));
        frame();
        press();
        frame();

        auto step=[&](int p, int mouse) {
            const int before=thumb();

            moveTo(canvasAt(mouse+grab));
            frame();

            const int want=expected(p);

            if(thumb() != want)
                bad.add("travel "+std::to_string(travel)+(notches?" notches "+std::to_string(notches):"")+
                    ": mouse at "+std::to_string(mouse)+" (from thumb "+std::to_string(before)+
                    "): thumb at "+std::to_string(thumb())+", expected "+std::to_string(want));
            else if(!notches && mouse == p && slider->getFloatValue() != readOff(p))
                bad.add("travel "+std::to_string(travel)+": value at "+std::to_string(p)+" is "+
                    std::to_string(slider->getFloatValue())+", expected "+std::to_string(readOff(p)));
        };

        for(int p=1; p <= travel; ++p)
            step(p, p);
        // past the end the thumb stays there
        step(travel, travel+5);
        for(int p=travel; p >= 0; --p)
            step(p, p);
        step(0, -5);

        release();
        frame();
    }
};

template<typename SliderType>
void checkDrags()
{
    Scene<SliderType> scene;
    Mismatches bad;

    for(int travel=1; travel <= MaxTravel; ++travel)
    {
        scene.setTravel(travel);
        scene.dragSweep(bad, 0, [](int p) { return p; });
    }
    CHECK_MESSAGE(bad.count == 0, bad.text());
}

template<typename SliderType>
void checkClicks()
{
    Scene<SliderType> scene;
    Mismatches bad;
    // the click centres the thumb on the mouse: a horizontal slider subtracts
    // half the thumb's width in whole units (7), a vertical one 7.5, truncated
    const int centre=Scene<SliderType>::Horizontal?ThumbLength/2:ThumbLength/2+1;

    for(int travel=1; travel <= MaxTravel; ++travel)
    {
        scene.setTravel(travel);

        for(int p=0; p <= travel; ++p)
        {
            const int click=p+centre;

            // put the thumb at whichever end leaves the click point on the track
            int from=-1;

            for(int end : {0, travel})
            {
                if(click < end || click >= end+ThumbLength)
                    from=end;
            }
            if(from < 0)
                continue;

            const float v=scene.readOff(from);

            scene.slider->setFloatValue(v);
            scene.frame();
            scene.moveTo(scene.canvasAt(click));
            scene.frame();
            scene.press();
            scene.frame();
            scene.release();
            scene.frame();

            if(scene.thumb() != p)
                bad.add("travel "+std::to_string(travel)+": click at "+std::to_string(click)+
                    " (thumb from "+std::to_string(from)+"): thumb at "+std::to_string(scene.thumb())+
                    ", expected "+std::to_string(p));
        }
    }
    CHECK_MESSAGE(bad.count == 0, bad.text());
}

template<typename SliderType>
void checkSetValue()
{
    Scene<SliderType> scene;
    Mismatches bad;

    for(int travel=1; travel <= MaxTravel; ++travel)
    {
        scene.setTravel(travel);

        for(int k=0; k <= 2*travel; ++k)
        {
            const float v=float(k)/float(2*travel);
            const int want=scene.placed(v);

            scene.slider->setFloatValue(v);
            if(scene.thumb() != want)
                bad.add("travel "+std::to_string(travel)+": setFloatValue("+std::to_string(v)+
                    "): thumb at "+std::to_string(scene.thumb())+", expected "+std::to_string(want));

            // a layout pass puts it back at the same place
            scene.slider->invalidate();
            scene.frame();
            if(scene.thumb() != want)
                bad.add("travel "+std::to_string(travel)+": setFloatValue("+std::to_string(v)+
                    ") then a layout: thumb at "+std::to_string(scene.thumb())+", expected "+
                    std::to_string(want));
        }
    }
    CHECK_MESSAGE(bad.count == 0, bad.text());
}

template<typename SliderType>
void checkNotches()
{
    Scene<SliderType> scene;
    Mismatches bad;

    for(int notches : {3, 4, 10, 7})
    {
        scene.slider->setNotchCount(notches);
        scene.slider->setClampToNotches(true);

        for(int travel=1; travel <= MaxTravel; ++travel)
        {
            scene.setTravel(travel);
            scene.dragSweep(bad, notches, [&](int p) {
                return scene.placed(Scene<SliderType>::snapped(scene.readOff(p), notches));
            });

            for(int k=0; k <= 4*notches; ++k)
            {
                const float v=float(k)/float(4*notches);
                const int want=scene.placed(Scene<SliderType>::snapped(v, notches));

                scene.slider->setFloatValue(v);
                scene.frame();
                if(scene.thumb() != want)
                    bad.add("travel "+std::to_string(travel)+" notches "+std::to_string(notches)+
                        ": setFloatValue("+std::to_string(v)+"): thumb at "+
                        std::to_string(scene.thumb())+", expected "+std::to_string(want));
            }
        }
    }
    CHECK_MESSAGE(bad.count == 0, bad.text());
}

}//namespace

TEST_CASE("a HorizontalSlider's dragged thumb ends at the mouse, one unit per one-unit step")
{
    checkDrags<controls::HorizontalSlider>();
}

TEST_CASE("a VerticalSlider's dragged thumb ends at the mouse, one unit per one-unit step")
{
    checkDrags<controls::VerticalSlider>();
}

TEST_CASE("a HorizontalSlider's thumb clicked to a track position ends there")
{
    checkClicks<controls::HorizontalSlider>();
}

TEST_CASE("a VerticalSlider's thumb clicked to a track position ends there")
{
    checkClicks<controls::VerticalSlider>();
}

TEST_CASE("a HorizontalSlider's thumb is placed at the rounded position of its value")
{
    checkSetValue<controls::HorizontalSlider>();
}

TEST_CASE("a VerticalSlider's thumb is placed at the rounded position of its value")
{
    checkSetValue<controls::VerticalSlider>();
}

TEST_CASE("a notched HorizontalSlider snaps its thumb to the rounded notch position")
{
    checkNotches<controls::HorizontalSlider>();
}

TEST_CASE("a notched VerticalSlider snaps its thumb to the rounded notch position")
{
    checkNotches<controls::VerticalSlider>();
}

TEST_CASE("SliderDemo's 25 of 0..100 over a 135-unit travel puts the thumb at 34")
{
    Scene<controls::HorizontalSlider> scene;

    scene.setTravel(135);
    scene.slider->setRange(0, 100);
    scene.slider->setFloatValue(25);
    scene.frame();
    CHECK(scene.thumb() == 34);
}
