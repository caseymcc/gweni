/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Knob's model and input on a real Canvas with the real (un-init()'d)
 *  textured skin on a StubRenderer, dragged and clicked through
 *  input::onMouseClicked and Canvas::inputMouseMoved, keyed through
 *  input::onKeyEvent (Shift and Control held through it too) and typed
 *  through Canvas::inputCharacter.
 *
 *  The arc runs from 225 to -45 degrees by default: angleForValue() gives the
 *  start at the minimum, the end at the maximum and 90 (the top) in the
 *  middle, a bipolar knob's arc starts at the range's 0, and valueForAngle()
 *  is its inverse, an angle in the gap the nearer end. A vertical drag moves
 *  the value by the motion, up raising it, the full range over DragRange
 *  units, summed from the press (one-unit moves add up, and back at the press
 *  is the value at the press), held at the range's ends (turning back moves it
 *  at once), a tenth as far with Shift; a press without motion changes
 *  nothing. An angular drag follows the mouse's angle round the dial, and
 *  turned on past the end through the gap stays at the end, never jumping to
 *  the start. Up/Right and Down/Left step by the speed (a hundredth of the
 *  range by default, a tenth of that with Shift, a whole number at least for
 *  an integer knob), PageUp/PageDown ten steps, Home/End the ends. A
 *  double-click, a Ctrl+click and Return open DragNumber's edit box over the
 *  dial, focused, its text selected; Return and a lost focus commit, Escape
 *  cancels, text that is no number reverts. onValueChanged fires once per
 *  real change and never for a value that stays. The value text shows the
 *  format, set only for a new string, and the knob is as wide as its widest
 *  part, the value text measured at the range's ends.
 */
#include "headlessFixture.h"

#include <gweni/controls/knob.h>
#include <gweni/controls/text.h>
#include <gweni/controls/textBox.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <doctest/doctest.h>

#include <cmath>

using namespace gweni;
using controls::Knob;

namespace
{

constexpr double Pi=3.14159265358979323846;

struct Scene
{
    test::StubRenderer renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    float clock=nextClock();

    //! The input handler keeps the last click's time and place across cases,
    //! so each case's clock starts well after the last one's.
    static float nextClock()
    {
        static float start=500.0f;

        start+=1000.0f;
        return start;
    }

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        input::onMouseClicked(canvas, 0, false);
        for(int key : {keys::Shift, keys::Alt, keys::Control})
            input::onKeyEvent(canvas, key, false);
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

    Knob *knob(int x=40, int y=40)
    {
        Knob *knob=canvas->newChild<Knob>();

        knob->setPos(x, y);
        frame();
        return knob;
    }

    //! The dial's centre on the canvas.
    PointF centreOf(Knob *knob) const
    {
        const Point at=knob->localPosToCanvas();
        const Rect dial=knob->getDialRect();

        return PointF(at.x+dial.x+dial.w*0.5f, at.y+dial.y+dial.h*0.5f);
    }

    Point dialCentre(Knob *knob) const
    {
        const PointF centre=centreOf(knob);

        return Point(int(centre.x), int(centre.y));
    }

    void move(int x, int y)
    {
        canvas->inputMouseMoved(x, y, 0, 0);
    }

    void move(const Point &at)
    {
        move(at.x, at.y);
    }

    //! The canvas point \p radius from the dial's centre at \p degrees (y up).
    Point onCircle(Knob *knob, double degrees, double radius=20.0) const
    {
        const PointF centre=centreOf(knob);

        return Point(int(std::lround(centre.x+radius*std::cos(degrees*Pi/180.0))),
            int(std::lround(centre.y-radius*std::sin(degrees*Pi/180.0))));
    }

    //! Presses at \p at, a second after the last click.
    void press(const Point &at)
    {
        clock+=1.0f;
        move(at);
        input::onMouseClicked(canvas, 0, true);
    }

    void release()
    {
        input::onMouseClicked(canvas, 0, false);
    }

    void click(controls::Base *control)
    {
        const Point at=control->localPosToCanvas();

        press(Point(at.x+control->getWidth()/2, at.y+control->getHeight()/2));
        release();
        frame();
    }

    void doubleClick(Knob *knob)
    {
        press(dialCentre(knob));
        release();
        input::onMouseClicked(canvas, 0, true);
        release();
        frame();
    }

    void key(int key)
    {
        input::onKeyEvent(canvas, key, true);
        input::onKeyEvent(canvas, key, false);
        frame();
    }

    void hold(int key, bool down)
    {
        input::onKeyEvent(canvas, key, down);
    }

    void type(const char *text)
    {
        for(const char *c=text; *c; ++c)
            canvas->inputCharacter(static_cast<UnicodeChar>(*c));
    }
};

//! Counts onValueChanged.
struct Counter: public event::Handler
{
    int count=0;
    void changed(event::Info) { ++count; }
};

}//namespace

TEST_CASE("Knob: the angle and the value map at both ends, the middle and a bipolar 0")
{
    Scene scene;
    Knob *knob=scene.knob();

    CHECK(knob->getStartAngle() == 225.0);
    CHECK(knob->getEndAngle() == -45.0);
    CHECK(knob->getMin() == 0.0);
    CHECK(knob->getMax() == 1.0);

    CHECK(knob->angleForValue(0.0) == doctest::Approx(225.0));
    CHECK(knob->angleForValue(1.0) == doctest::Approx(-45.0));
    CHECK(knob->angleForValue(0.5) == doctest::Approx(90.0));
    // clamped to the range
    CHECK(knob->angleForValue(-3.0) == doctest::Approx(225.0));
    CHECK(knob->angleForValue(7.0) == doctest::Approx(-45.0));

    CHECK(knob->valueForAngle(225.0) == doctest::Approx(0.0));
    CHECK(knob->valueForAngle(-45.0) == doctest::Approx(1.0));
    CHECK(knob->valueForAngle(315.0) == doctest::Approx(1.0));
    CHECK(knob->valueForAngle(90.0) == doctest::Approx(0.5));
    CHECK(knob->valueForAngle(0.0) == doctest::Approx(225.0/270.0));
    // in the gap, at the bottom: the nearer end
    CHECK(knob->valueForAngle(260.0) == doctest::Approx(0.0));
    CHECK(knob->valueForAngle(300.0) == doctest::Approx(1.0));
    CHECK(knob->valueForAngle(-60.0) == doctest::Approx(1.0));

    // the range's 0 is where a bipolar arc starts
    CHECK(knob->getArcOrigin() == 0.0);
    knob->setRange(-1.0, 1.0);
    CHECK(knob->getNumber() == 0.0);
    CHECK(knob->getArcOrigin() == -1.0);
    knob->setBipolar(true);
    CHECK(knob->getArcOrigin() == 0.0);
    CHECK(knob->angleForValue(0.0) == doctest::Approx(90.0));
    CHECK(knob->valueForAngle(90.0) == doctest::Approx(0.0));
    CHECK(knob->angleForValue(-1.0) == doctest::Approx(225.0));
    CHECK(knob->angleForValue(1.0) == doctest::Approx(-45.0));
    // a range that does not hold 0 starts at its nearer end
    knob->setRange(2.0, 5.0);
    CHECK(knob->getArcOrigin() == 2.0);

    // other angles: a counter-clockwise sweep
    knob->setRange(0.0, 1.0);
    knob->setAngles(0.0, 180.0);
    CHECK(knob->angleForValue(0.5) == doctest::Approx(90.0));
    CHECK(knob->valueForAngle(45.0) == doctest::Approx(0.25));
    CHECK(knob->valueForAngle(200.0) == doctest::Approx(1.0));
    CHECK(knob->valueForAngle(-20.0) == doctest::Approx(0.0));
}

TEST_CASE("Knob: a vertical drag is summed from the press, clamped at the ends, and fine with Shift")
{
    Scene scene;
    Knob *knob=scene.knob();
    Counter counter;
    knob->onValueChanged.add(&counter, &Counter::changed);

    knob->setNumber(0.5);
    counter.count=0;

    const Point start=scene.dialCentre(knob);

    // a press without motion changes nothing
    scene.press(start);
    CHECK(knob->isDepressed());
    scene.release();
    CHECK_FALSE(knob->isDepressed());
    CHECK(knob->getNumber() == 0.5);
    CHECK(counter.count == 0);

    // a hundredth of the range per two units: the range over DragRange
    scene.press(start);
    for(int i=1; i <= 20; ++i)
        scene.move(start.x, start.y-i);
    CHECK(knob->getNumber() == doctest::Approx(0.5+20.0/Knob::DragRange));
    CHECK(counter.count == 20);
    // sideways motion does nothing
    scene.move(start.x+30, start.y-20);
    CHECK(knob->getNumber() == doctest::Approx(0.5+20.0/Knob::DragRange));
    // down lowers it, and back at the press is the value at the press
    scene.move(start.x, start.y+10);
    CHECK(knob->getNumber() == doctest::Approx(0.5-10.0/Knob::DragRange));
    scene.move(start.x, start.y);
    CHECK(knob->getNumber() == 0.5);
    scene.release();

    // held at the top end: turning back moves it at once
    counter.count=0;
    scene.press(start);
    scene.move(start.x, start.y-400);
    CHECK(knob->getNumber() == 1.0);
    scene.move(start.x, start.y-500);
    CHECK(knob->getNumber() == 1.0);
    CHECK(counter.count == 1);
    scene.move(start.x, start.y-490);
    CHECK(knob->getNumber() == doctest::Approx(1.0-10.0/Knob::DragRange));
    scene.release();

    // Shift: a tenth as far
    knob->setNumber(0.5);
    scene.press(start);
    scene.hold(keys::Shift, true);
    scene.move(start.x, start.y-40);
    CHECK(knob->getNumber() == doctest::Approx(0.5+4.0/Knob::DragRange));
    scene.hold(keys::Shift, false);
    scene.release();

    // an integer knob rounds, the motion still summed
    knob->setRange(0.0, 10.0);
    knob->setInteger(true);
    knob->setNumber(5.0);
    scene.press(start);
    for(int i=1; i <= 19; ++i)
        scene.move(start.x, start.y-i);
    // 19 units are 0.95 of a whole number at 10 over DragRange
    CHECK(knob->getNumber() == 6.0);
    scene.move(start.x, start.y);
    CHECK(knob->getNumber() == 5.0);
    scene.release();
}

TEST_CASE("Knob: an angular drag follows the mouse round the arc and never jumps across the gap")
{
    Scene scene;
    Knob *knob=scene.knob();
    knob->setDragMode(Knob::DragMode::Angular);
    knob->setNumber(0.5);

    Counter counter;
    knob->onValueChanged.add(&counter, &Counter::changed);

    // pressed at the top: the value stays until the mouse turns
    scene.press(scene.onCircle(knob, 90.0));
    CHECK(knob->getNumber() == 0.5);
    // clockwise to the right: a third of the sweep up
    for(double a=75.0; a >= 0.0; a-=15.0)
        scene.move(scene.onCircle(knob, a));
    CHECK(knob->getNumber() == doctest::Approx(0.5+90.0/270.0).epsilon(0.01));
    // on through the end and the gap to the start's side: held at the end
    for(double a=-15.0; a >= -150.0; a-=15.0)
        scene.move(scene.onCircle(knob, a));
    CHECK(knob->getNumber() == 1.0);
    // back the other way: down at once
    for(double a=-135.0; a <= -45.0; a+=15.0)
        scene.move(scene.onCircle(knob, a));
    CHECK(knob->getNumber() == doctest::Approx(1.0-105.0/270.0).epsilon(0.01));
    scene.release();

    // from the start, counter-clockwise into the gap: held at the minimum
    knob->setNumber(0.0);
    counter.count=0;
    scene.press(scene.onCircle(knob, 225.0));
    for(double a=240.0; a <= 330.0; a+=15.0)
        scene.move(scene.onCircle(knob, a));
    CHECK(knob->getNumber() == 0.0);
    CHECK(counter.count == 0);
    scene.release();

    // Shift: a tenth of the angle
    knob->setNumber(0.5);
    scene.press(scene.onCircle(knob, 90.0));
    scene.hold(keys::Shift, true);
    for(double a=75.0; a >= 0.0; a-=15.0)
        scene.move(scene.onCircle(knob, a));
    CHECK(knob->getNumber() == doctest::Approx(0.5+9.0/270.0).epsilon(0.01));
    scene.hold(keys::Shift, false);
    scene.release();

    // the centre, where the angle means nothing, moves nothing
    knob->setNumber(0.5);
    scene.press(scene.onCircle(knob, 90.0));
    scene.move(scene.dialCentre(knob));
    CHECK(knob->getNumber() == 0.5);
    scene.release();
}

TEST_CASE("Knob: the keys step by the speed, Shift fine, PageUp/PageDown ten steps, Home and End the ends")
{
    Scene scene;
    Knob *knob=scene.knob();
    Counter counter;
    knob->onValueChanged.add(&counter, &Counter::changed);
    knob->setNumber(0.5);
    knob->focus();
    counter.count=0;

    CHECK(knob->getSpeed() == doctest::Approx(0.01));
    scene.key(keys::Up);
    CHECK(knob->getNumber() == doctest::Approx(0.51));
    scene.key(keys::Right);
    CHECK(knob->getNumber() == doctest::Approx(0.52));
    scene.key(keys::Down);
    scene.key(keys::Left);
    CHECK(knob->getNumber() == doctest::Approx(0.5));
    scene.hold(keys::Shift, true);
    scene.key(keys::Up);
    CHECK(knob->getNumber() == doctest::Approx(0.501));
    scene.hold(keys::Shift, false);
    scene.key(keys::PageUp);
    CHECK(knob->getNumber() == doctest::Approx(0.601));
    scene.key(keys::PageDown);
    scene.key(keys::PageDown);
    CHECK(knob->getNumber() == doctest::Approx(0.401));
    scene.key(keys::Home);
    CHECK(knob->getNumber() == 0.0);
    CHECK(counter.count == 9);
    // at the end: no change, no event
    scene.key(keys::Home);
    scene.key(keys::Down);
    CHECK(counter.count == 9);
    scene.key(keys::End);
    CHECK(knob->getNumber() == 1.0);
    scene.key(keys::PageUp);
    CHECK(counter.count == 10);

    // a speed of its own
    knob->setSpeed(0.25);
    scene.key(keys::Home);
    scene.key(keys::Up);
    CHECK(knob->getNumber() == doctest::Approx(0.25));

    // an integer knob steps by a whole number at least, Shift included
    knob->setSpeed(0.0);
    knob->setRange(0.0, 10.0);
    knob->setInteger(true);
    knob->setNumber(5.0);
    scene.key(keys::Up);
    CHECK(knob->getNumber() == 6.0);
    scene.hold(keys::Shift, true);
    scene.key(keys::Up);
    CHECK(knob->getNumber() == 7.0);
    scene.hold(keys::Shift, false);
    // ten whole steps
    scene.key(keys::PageDown);
    CHECK(knob->getNumber() == 0.0);
    scene.key(keys::Up);
    scene.key(keys::PageUp);
    CHECK(knob->getNumber() == 10.0);

    // disabled: nothing
    knob->setDisabled(true);
    scene.key(keys::Home);
    CHECK(knob->getNumber() == 10.0);
}

TEST_CASE("Knob: typed entry opens on a double-click or Ctrl+click or Return and commits or cancels or reverts")
{
    Scene scene;
    Knob *knob=scene.knob();
    controls::TextBox *elsewhere=scene.canvas->newChild<controls::TextBox>();
    elsewhere->setPos(300, 300);
    knob->setRange(0.0, 100.0);
    knob->setNumber(5.0);
    scene.frame();

    Counter counter;
    knob->onValueChanged.add(&counter, &Counter::changed);

    // a double-click: the box over the dial, focused, the value selected
    scene.doubleClick(knob);
    REQUIRE(knob->isEditing());
    controls::internal::DragNumberTextBox *box=knob->getEditBox();
    REQUIRE(box);
    CHECK(box->getOwner() == knob);
    CHECK(KeyboardFocus == box);
    CHECK(box->getText() == "5");
    CHECK(box->getSelection() == "5");
    const Rect dial=knob->getDialRect();
    CHECK(box->getY() == dial.y+(dial.h-Knob::EditHeight)/2);
    CHECK(box->getWidth() == knob->getWidth());

    // Return commits, the focus back on the knob
    scene.type("7");
    scene.key(keys::Return);
    CHECK_FALSE(knob->isEditing());
    CHECK(box->hidden());
    CHECK(knob->getNumber() == 7.0);
    CHECK(knob->getValueText() == "7.00");
    CHECK(counter.count == 1);
    CHECK(KeyboardFocus == knob);

    // Return on the focused knob opens it; the same value again: no event
    scene.key(keys::Return);
    CHECK(knob->isEditing());
    scene.key(keys::Return);
    CHECK_FALSE(knob->isEditing());
    CHECK(counter.count == 1);

    // past the range: clamped
    scene.doubleClick(knob);
    scene.type("250");
    scene.key(keys::Return);
    CHECK(knob->getNumber() == 100.0);
    CHECK(counter.count == 2);

    // a Ctrl+click opens it, and the focus going elsewhere commits
    scene.hold(keys::Control, true);
    scene.press(scene.dialCentre(knob));
    scene.release();
    scene.hold(keys::Control, false);
    scene.frame();
    CHECK(knob->isEditing());
    scene.type("12.5");
    scene.click(elsewhere);
    CHECK_FALSE(knob->isEditing());
    CHECK(knob->getNumber() == 12.5);
    CHECK(counter.count == 3);
    CHECK(KeyboardFocus == elsewhere);

    // Escape cancels
    scene.doubleClick(knob);
    scene.type("99");
    scene.key(keys::Escape);
    CHECK_FALSE(knob->isEditing());
    CHECK(knob->getNumber() == 12.5);
    CHECK(counter.count == 3);
    CHECK(KeyboardFocus == knob);

    // text that is no number: the value as it was
    for(const char *bad : {"", "-", "abc", "1.2.3"})
    {
        CAPTURE(bad);
        scene.doubleClick(knob);
        box->setText(bad);
        scene.key(keys::Return);
        CHECK_FALSE(knob->isEditing());
        CHECK(knob->getNumber() == 12.5);
        CHECK(knob->getValueText() == "12.50");
    }
    CHECK(counter.count == 3);

    // the box is made once
    CHECK(knob->getEditBox() == box);

    // a disabled knob opens nothing
    knob->setDisabled(true);
    scene.doubleClick(knob);
    CHECK_FALSE(knob->isEditing());
}

TEST_CASE("Knob: onValueChanged fires once per real change, never for a value that stays")
{
    Scene scene;
    Knob *knob=scene.knob();
    Counter counter;
    knob->onValueChanged.add(&counter, &Counter::changed);

    knob->setNumber(0.25);
    CHECK(counter.count == 1);
    knob->setNumber(0.25);
    CHECK(counter.count == 1);
    // clamped to where it is
    knob->setNumber(1.0);
    knob->setNumber(3.0);
    CHECK(knob->getNumber() == 1.0);
    CHECK(counter.count == 2);
    // without events
    knob->setNumber(0.5, false);
    CHECK(knob->getNumber() == 0.5);
    CHECK(counter.count == 2);
    // a range that keeps the value: no event; one that clamps it: one
    knob->setRange(0.0, 2.0);
    CHECK(counter.count == 2);
    knob->setRange(0.75, 2.0);
    CHECK(knob->getNumber() == 0.75);
    CHECK(counter.count == 3);
    // an integer knob rounds once
    knob->setInteger(true);
    CHECK(knob->getNumber() == 1.0);
    CHECK(counter.count == 4);
    knob->setNumber(1.2);
    CHECK(counter.count == 4);
    // a value that is no number
    knob->setNumber(std::nan(""));
    CHECK(knob->getNumber() == 1.0);
    CHECK(counter.count == 4);
}

TEST_CASE("Knob: the value text follows the format, and the knob is as wide as its widest part")
{
    Scene scene;
    Knob *knob=scene.knob();

    REQUIRE(knob->getValueTextControl());
    CHECK(knob->getValueText() == "0.00");
    CHECK(knob->getFormat() == "%.2f");
    CHECK(knob->setFormat("%.1f dB"));
    CHECK(knob->getValueText() == "0.0 dB");
    CHECK_FALSE(knob->setFormat("%d"));
    CHECK(knob->getFormat() == "%.1f dB");
    knob->setFormat("");
    knob->setInteger(true);
    CHECK(knob->getValueText() == "0");

    // the dial alone is the diameter square
    knob->setShowValue(false);
    CHECK_FALSE(knob->getValueTextControl());
    scene.frame();
    CHECK(knob->getWidth() == Knob::DefaultDiameter);
    CHECK(knob->getHeight() == Knob::DefaultDiameter);
    CHECK((knob->getDialRect() == Rect(0, 0, Knob::DefaultDiameter, Knob::DefaultDiameter)));

    // a caption above and the value text under it
    knob->setShowValue(true);
    knob->setCaption("Gain");
    scene.frame();
    const Size caption=knob->getCaptionControl()->getPreferredSize();
    const Size value=knob->getValueTextControl()->getPreferredSize();
    CHECK(knob->getHeight() == caption.height+value.height+2*Knob::TextGap+Knob::DefaultDiameter);
    CHECK(knob->getDialRect().y == caption.height+Knob::TextGap);
    CHECK(knob->getValueTextControl()->getY() == caption.height+2*Knob::TextGap+Knob::DefaultDiameter);

    // a caption wider than the dial widens it, the dial centred
    knob->setCaption("A caption much wider than the dial");
    scene.frame();
    CHECK(knob->getWidth() == knob->getCaptionControl()->getPreferredSize().width);
    CHECK(knob->getDialRect().x == (knob->getWidth()-Knob::DefaultDiameter)/2);
    knob->setCaption("");
    CHECK_FALSE(knob->getCaptionControl());
}
