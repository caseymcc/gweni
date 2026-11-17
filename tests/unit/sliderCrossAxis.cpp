/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A slider's thumb stays on its track across the slider's axis (issue #727):
 *  a HorizontalSlider's thumb at y 0, a VerticalSlider's at x 0 (the track
 *  column's left, a value text's column right of it), whatever the drag or the
 *  click does across the axis and whatever margin the slider has. A margin is
 *  the slider's outer spacing in its parent's layout, not an inset of its
 *  track: Base::restrictedPos clamped the thumb to the parent's bounds less the
 *  parent's margin, so a slider docked Top with a bottom margin of 4 put its
 *  20-unit thumb at y -4 on the first drag move, one docked Left with a right
 *  margin of 12 at x -12, and a margin along the axis kept the thumb short of
 *  the slider's ends (value min and max out of reach). And the slider kept
 *  the thumb's own cross coordinate in updateBarFromValue(), so a thumb the
 *  drag had moved across (a vertical slider's narrower thumb into its value
 *  text) stayed there.
 *
 *  For a HorizontalSlider docked Top with a bottom margin, a VerticalSlider
 *  docked Left with a right margin, a vertical one with a value text, a
 *  horizontal one with left and right margins, a vertical one with top and
 *  bottom margins and a horizontal and a vertical one clamped to notches,
 *  driven by real mouse input on the StubRenderer/StubSkin canvas: right after
 *  every input event and after every frame of a drag along the axis, across
 *  it, off the slider and past both ends, of a click on the track, the
 *  release, a layout pass and setFloatValue(), the thumb's cross coordinate is
 *  0 and its axis coordinate where the mouse (or the value) put it; dragged
 *  past each end the value is the slider's min and max.
 *
 *  And Base::restrictedPos itself: a restrictToParent child of a parent with a
 *  margin is kept inside the parent's own bounds, [0, parent size - size], and
 *  a padding of the child's own still keeps it that far off the parent's
 *  edges (a scroll bar's thumb kept between its arrow buttons).
 */
#include "headlessFixture.h"

#include <gweni/controls/horizontalSlider.h>
#include <gweni/controls/verticalSlider.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <sstream>
#include <string>

using namespace gweni;

namespace
{

//! The double-click state is process-global: each scene starts its clock far
//! past the last one's, and every press is a second after the one before.
float nextClock()
{
    static float start=9000000.0f;
    start+=100000.0f;
    return start;
}

//! One slider in a host container, as a demo page places one.
struct Setup
{
    const char *name;
    bool horizontal;
    Margin margin;
    DockPosition dock;
    //! empty: no value text
    const char *valueFormat;
    //! 0: not clamped to notches
    int notches;
};

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::Base *host=nullptr;
    controls::Slider *slider=nullptr;
    const Setup setup;
    float clock=nextClock();
    std::ostringstream bad;
    size_t badCount=0;
    String step="built";

    explicit Scene(const Setup &s):
        setup(s)
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(500, 500);
        host=canvas->newChild<controls::Base>();
        host->setBounds(40, 40, 300, 300);
        if(setup.horizontal)
            slider=host->newChild<controls::HorizontalSlider>();
        else
            slider=host->newChild<controls::VerticalSlider>();
        slider->setRange(0.f, 100.f);
        slider->setMargin(setup.margin);
        if(setup.dock != DockPosition::None)
            slider->setDock(setup.dock);
        else if(setup.horizontal)
            slider->setBounds(20, 20, 200, 20);
        else
            slider->setBounds(20, 20, 80, 200);
        if(setup.valueFormat[0])
            REQUIRE(slider->setValueFormat(setup.valueFormat));
        if(setup.notches)
        {
            slider->setNotchCount(setup.notches);
            slider->setClampToNotches(true);
        }
        slider->setFloatValue(35.f);
        frame();
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

    controls::Base *thumb() const { return slider->getSliderBar(); }

    int length() const { return setup.horizontal?slider->getWidth():slider->getHeight(); }
    int thumbLength() const { return setup.horizontal?thumb()->getWidth():thumb()->getHeight(); }
    int travel() const { return length()-thumbLength(); }

    int along() const { return setup.horizontal?thumb()->getX():thumb()->getY(); }
    int across() const { return setup.horizontal?thumb()->getY():thumb()->getX(); }

    void fail(const String &what)
    {
        if(badCount++ < 12)
            bad<<"\n  "<<setup.name<<", "<<step<<": "<<what;
    }

    //! The thumb on its track across the axis, and inside the slider.
    void checkAcross(const char *when)
    {
        if(across() != 0)
            fail(String(when)+": thumb "+(setup.horizontal?"y ":"x ")+std::to_string(across())+
                ", expected 0 (thumb at "+std::to_string(thumb()->getX())+", "+
                std::to_string(thumb()->getY())+")");
    }

    void checkAlong(int want)
    {
        if(along() != want)
            fail("thumb along the axis at "+std::to_string(along())+", expected "+std::to_string(want));
    }

    void frame()
    {
        checkAcross("before the frame");
        renderer.events.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
        checkAcross("after the frame");
    }

    //! A canvas point: `a` units down the slider's axis, `c` across it, both
    //! in the slider's own coordinates.
    Point canvasAt(int a, int c)
    {
        return slider->localPosToCanvas(setup.horizontal?Point(a, c):Point(c, a));
    }

    void moveTo(Point at)
    {
        canvas->inputMouseMoved(at.x, at.y, 0, 0);
        checkAcross("after the move");
    }

    void press()
    {
        clock+=1.0f;
        canvas->inputMouseButton(0, true);
        checkAcross("after the press");
    }

    void release()
    {
        canvas->inputMouseButton(0, false);
        checkAcross("after the release");
    }

    //! The normalised value read off thumb position p (calculateValue()).
    float readOff(int p) const
    {
        const float fraction=float(p)/float(travel());

        return setup.horizontal?fraction:1.f-fraction;
    }

    //! Where the thumb is placed from normalised value v (updateBarFromValue()).
    int placed(float v) const
    {
        if(setup.notches)
            v=std::floor(v*float(setup.notches)+0.5f)/float(setup.notches);

        const float fraction=setup.horizontal?v:1.f-v;
        const long position=std::lround(float(travel())*fraction);

        return int(std::max(0L, std::min(long(travel()), position)));
    }

    //! Where a thumb moved to p (by a drag or a click) ends: p kept on the
    //! track, read as a value and placed from it.
    int ends(int p) const
    {
        p=std::max(0, std::min(travel(), p));
        return placed(readOff(p));
    }

    //! The slider's value at the end of the track the thumb's position p is at.
    float valueAt(int p) const
    {
        const bool minEnd=setup.horizontal?p == 0:p == travel();

        return minEnd?slider->getMin():slider->getMax();
    }

    void run()
    {
        REQUIRE(travel() > 40);
        if(setup.valueFormat[0] && !setup.horizontal)
            REQUIRE(thumb()->getWidth() < slider->getWidth());

        const int crossSize=setup.horizontal?thumb()->getHeight():thumb()->getWidth();
        const int sliderCross=setup.horizontal?slider->getHeight():slider->getWidth();
        const int grab=thumbLength()/2;
        const int mid=crossSize/2;

        // the drag: pressed at the thumb's centre
        step="the press on the thumb";
        int start=along();

        moveTo(canvasAt(start+grab, mid));
        frame();
        press();
        frame();
        checkAlong(start);

        // `a` the thumb's wanted position along the axis, `c` the mouse across
        auto dragTo=[&](const char *what, int a, int c) {
            step=what;
            moveTo(canvasAt(a+grab, c));
            frame();
            checkAlong(ends(a));
        };

        dragTo("a 5-unit move along", start+5, mid);
        dragTo("a move along and across", start+12, mid+7);
        dragTo("a move across, off the slider", start+12, mid+3*sliderCross);
        dragTo("a move across, off the other side", start+20, mid-3*sliderCross);
        dragTo("a move into the value text's column", start+25, sliderCross-2);
        dragTo("a move past the track's start", -40, mid+9);
        if(slider->getFloatValue() != valueAt(0))
            fail("past the track's start the value is "+std::to_string(slider->getFloatValue())+
                ", expected "+std::to_string(valueAt(0)));
        dragTo("a move past the track's end", travel()+40, mid-9);
        if(slider->getFloatValue() != valueAt(travel()))
            fail("past the track's end the value is "+std::to_string(slider->getFloatValue())+
                ", expected "+std::to_string(valueAt(travel())));
        dragTo("a move back along, across", travel()/2, mid+5);

        step="the release";
        const int released=along();

        release();
        frame();
        checkAlong(released);
        moveTo(canvasAt(length()+30, sliderCross+30));
        frame();

        // a click on the track, off the thumb and off its middle across
        for(const int click : {length()/5, length()-length()/5})
        {
            step="a click on the track at "+std::to_string(click);
            // a horizontal slider centres the thumb on the click less 7 (its
            // half width, whole units), a vertical one less 7.5, truncated
            const int centre=setup.horizontal?thumbLength()/2:thumbLength()/2+1;

            moveTo(canvasAt(click, mid+3));
            frame();
            press();
            frame();
            checkAlong(ends(click-centre));
            release();
            frame();
            checkAlong(ends(click-centre));
        }

        step="a layout pass";
        const int laidOut=along();

        slider->invalidate();
        frame();
        checkAlong(laidOut);

        for(const float value : {0.f, 27.f, 64.f, 100.f})
        {
            step="setFloatValue("+std::to_string(value)+")";
            slider->setFloatValue(value);
            checkAcross("after the set");
            frame();
            checkAlong(placed(value/100.f));
        }
    }
};

void checkSlider(const Setup &setup)
{
    Scene scene(setup);

    scene.run();

    const std::string mismatches=std::to_string(scene.badCount)+" mismatches:"+scene.bad.str();

    CHECK_MESSAGE(scene.badCount == 0, mismatches);
}

//! "x,y", so a failing CHECK shows both.
std::string pointText(const Point &p)
{
    return std::to_string(p.x)+","+std::to_string(p.y);
}

}//namespace

TEST_CASE("a HorizontalSlider docked Top with a bottom margin keeps its thumb at y 0")
{
    checkSlider({"Top, Margin(0,0,0,4)", true, Margin(0, 0, 0, 4), DockPosition::Top, "", 0});
}

TEST_CASE("a VerticalSlider docked Left with a right margin keeps its thumb at x 0")
{
    checkSlider({"Left, Margin(0,0,12,0)", false, Margin(0, 0, 12, 0), DockPosition::Left, "", 0});
}

TEST_CASE("a VerticalSlider with a value text keeps its thumb in the track column")
{
    checkSlider({"value text %+.1f", false, Margin(0, 0, 0, 0), DockPosition::None, "%+.1f", 0});
}

TEST_CASE("a HorizontalSlider with left and right margins reaches both ends, its thumb at y 0")
{
    checkSlider({"Top, Margin(10,0,6,0)", true, Margin(10, 0, 6, 0), DockPosition::Top, "", 0});
}

TEST_CASE("a VerticalSlider with top and bottom margins reaches both ends, its thumb at x 0")
{
    checkSlider({"Left, Margin(0,8,0,5)", false, Margin(0, 8, 0, 5), DockPosition::Left, "", 0});
}

TEST_CASE("a notched HorizontalSlider with a margin keeps its thumb at y 0")
{
    checkSlider({"notched, Top, Margin(0,3,0,4)", true, Margin(0, 3, 0, 4), DockPosition::Top, "", 5});
}

TEST_CASE("a notched VerticalSlider with a margin keeps its thumb at x 0")
{
    checkSlider({"notched, Left, Margin(4,0,12,0)", false, Margin(4, 0, 12, 0), DockPosition::Left, "", 4});
}

TEST_CASE("restrictedPos keeps a child inside its parent's own bounds whatever the parent's margin")
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas canvas(&skin);

    canvas.setSize(400, 400);

    auto *parent=canvas.newChild<controls::Base>();

    parent->setBounds(10, 10, 100, 80);
    parent->setMargin(Margin(5, 6, 7, 8));

    auto *child=parent->newChild<controls::Base>();

    child->setBounds(0, 0, 20, 10);
    child->restrictToParent(true);

    CHECK(pointText(child->restrictedPos(-50, -50)) == pointText(Point(0, 0)));
    CHECK(pointText(child->restrictedPos(500, 500)) == pointText(Point(80, 70)));
    CHECK(pointText(child->restrictedPos(0, 0)) == pointText(Point(0, 0)));
    CHECK(pointText(child->restrictedPos(80, 70)) == pointText(Point(80, 70)));
    CHECK(pointText(child->restrictedPos(30, 40)) == pointText(Point(30, 40)));

    // not restricted: anywhere
    child->restrictToParent(false);
    CHECK(pointText(child->restrictedPos(-50, 500)) == pointText(Point(-50, 500)));
}

TEST_CASE("restrictedPos keeps a child its own padding off the parent's edges")
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas canvas(&skin);

    canvas.setSize(400, 400);

    for(const Margin parentMargin : {Margin(0, 0, 0, 0), Margin(5, 6, 7, 8)})
    {
        CAPTURE(parentMargin.left);

        auto *parent=canvas.newChild<controls::Base>();

        parent->setBounds(10, 10, 100, 80);
        parent->setMargin(parentMargin);

        auto *child=parent->newChild<controls::Base>();

        child->setBounds(0, 0, 20, 10);
        child->restrictToParent(true);
        // a scroll bar's thumb: kept between the arrow buttons at each end
        child->setPadding(Padding(3, 2, 4, 6));

        CHECK(pointText(child->restrictedPos(-50, -50)) == pointText(Point(3, 2)));
        CHECK(pointText(child->restrictedPos(500, 500)) == pointText(Point(100-20-4, 80-10-6)));
        CHECK(pointText(child->restrictedPos(30, 40)) == pointText(Point(30, 40)));

        delete parent;
    }
}
