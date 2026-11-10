/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  DragNumber on a real Canvas with the real (un-init()'d) textured skin on a
 *  recording StubRenderer, dragged and clicked through input::onMouseClicked
 *  and Canvas::inputMouseMoved, keyed through input::onKeyEvent (Shift, Alt
 *  and Control held through it too) and typed through Canvas::inputCharacter.
 *
 *  A drag changes the value by the motion times the speed (0.1 by default, 1
 *  for an integer field), ten times that with Shift and a tenth with Alt; the
 *  motion is summed, so steps worth less than one whole unit of an integer
 *  field add up to one, and a drag back to its start gives the value at the
 *  press. The value is clamped to the range, and the drag held there, so
 *  turning back moves it at once; an integer field rounds. onValueChanged
 *  fires once per real change: none for a press and release without motion,
 *  a drag clamped at the range's end, a sub-unit integer step, or the same
 *  value set again. A drag frame whose formatted string stays marks nothing on
 *  the field or its Text; one that changes it marks the Text alone.
 *
 *  A double-click, a Ctrl+click and Return on the focused field each show the
 *  edit box over it, focused, its text (the value at the format's precision,
 *  trailing zeros dropped) selected, and the field's slot hidden; Return
 *  commits and gives the focus back to the field, a click on another control
 *  (the focus lost) commits, Escape cancels, and text that is no number leaves
 *  the value and the shown text as they were; a field whose page is hidden
 *  while it is typed into, or whose box stops taking keyboard input, commits
 *  too (the box's onLostKeyboardFocus() runs once), laid out in that frame's
 *  update phase in one pass, and shown again it is an unedited field at rest.
 *  The box is made once: its ids are taken by the first edit only. Opening
 *  and closing the box each way moves no control and keeps the field's
 *  preferred size, in one update-phase pass, whatever its size flags: docked
 *  Top and Elastic, Elastic in a HorizontalLayout cell, Elastic in X docked
 *  Left; the box covers the field, 0,0,w,h; and a value the app sets while it
 *  is open still widens an Elastic field (#521). Up/Right
 *  and Down/Left step the focused field. The format is refused unless it
 *  holds one conversion of a double. The textured renderer draws the field
 *  DragNumber at rest, DragNumberHover hovered, DragNumberDown pressed,
 *  DragNumberFocus focused and DragNumberDisabled disabled, one slot, and a
 *  disabled field takes no drag.
 */
#include "headlessFixture.h"

#include <gweni/controls/dragNumber.h>
#include <gweni/controls/layout/layout.h>
#include <gweni/controls/text.h>
#include <gweni/controls/textBox.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <doctest/doctest.h>

#include <vector>

using namespace gweni;
using controls::DragNumber;
namespace tokyonight=skin::textured::tokyonight;

namespace
{

//! Records each slot's last rounded-rect fill and border, and hides.
class FieldRecorder: public test::StubRenderer
{
public:
    struct Draw
    {
        size_t id;
        bool hidden;
        Color fill;
        Color border;
    };

    void drawRoundedRect(size_t primitiveId, Rect, float, float, Color top, Color, Color border, int) override
    {
        draws.push_back({primitiveId, false, top, border});
    }

    void hidePrimitive(size_t primitiveId) override
    {
        draws.push_back({primitiveId, true, Color(), Color()});
    }

    const Draw *last(size_t id) const
    {
        for(auto it=draws.rbegin(); it != draws.rend(); ++it)
        {
            if(it->id == id)
                return &*it;
        }
        return nullptr;
    }

    std::vector<Draw> draws;
};

struct Scene
{
    FieldRecorder renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    float clock=nextClock();

    //! The input handler keeps the last click's time and place across cases,
    //! so each case's clock starts well after the last one's.
    static float nextClock()
    {
        static float start=0.0f;

        start+=1000.0f;
        return start;
    }

    Scene()
    {
        // a second apart per click, so two clicks are a double click only
        // when a test makes them one
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        // the hovered control, the focus and the button and key state are
        // process-global
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
        renderer.draws.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    DragNumber *field(int x=20, int y=20)
    {
        DragNumber *field=canvas->newChild<DragNumber>();

        field->setPos(x, y);
        frame();
        return field;
    }

    Point centreOf(controls::Base *control) const
    {
        const Point at=control->localPosToCanvas();

        return Point(at.x+control->getWidth()/2, at.y+control->getHeight()/2);
    }

    void move(int x, int y)
    {
        canvas->inputMouseMoved(x, y, 0, 0);
    }

    //! Presses on \p control's centre (a second after the last click).
    Point press(controls::Base *control)
    {
        const Point at=centreOf(control);

        clock+=1.0f;
        move(at.x, at.y);
        input::onMouseClicked(canvas, 0, true);
        return at;
    }

    void release()
    {
        input::onMouseClicked(canvas, 0, false);
    }

    void click(controls::Base *control)
    {
        press(control);
        release();
        frame();
    }

    //! Two presses at one place and time: the second is a double click.
    void doubleClick(controls::Base *control)
    {
        press(control);
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

controls::internal::Text *textOf(DragNumber *field)
{
    for(controls::Base *child : field->getActualChildren())
    {
        if(auto *text=gweni_cast<controls::internal::Text>(child))
            return text;
    }
    return nullptr;
}

bool sameColor(const Color &a, const Color &b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

}//namespace

TEST_CASE("DragNumber: a drag moves the value by the motion times the speed, Shift ten times and Alt a tenth")
{
    Scene scene;
    DragNumber *field=scene.field();

    CHECK(field->getSpeed() == doctest::Approx(0.1));
    CHECK(field->getFormat() == "%.3f");
    CHECK(field->getText() == "0.000");

    const Point at=scene.press(field);
    CHECK(field->isDepressed());
    CHECK(MouseFocus == field);

    scene.move(at.x+10, at.y);
    CHECK(field->getNumber() == doctest::Approx(1.0));
    scene.move(at.x+15, at.y+7);    // only the horizontal motion counts
    CHECK(field->getNumber() == doctest::Approx(1.5));

    scene.hold(keys::Shift, true);
    scene.move(at.x+16, at.y);
    CHECK(field->getNumber() == doctest::Approx(2.5));
    scene.hold(keys::Shift, false);

    scene.hold(keys::Alt, true);
    scene.move(at.x+26, at.y);
    CHECK(field->getNumber() == doctest::Approx(2.6));
    scene.hold(keys::Alt, false);

    // leftwards, past where it started: no range, so below zero
    scene.move(at.x-10, at.y);
    CHECK(field->getNumber() == doctest::Approx(-1.0));
    CHECK(field->getText() == "-1.000");

    scene.release();
    scene.frame();
    CHECK_FALSE(field->isDepressed());
    CHECK(MouseFocus == nullptr);

    // a speed of its own
    field->setSpeed(2.5);
    field->setNumber(0.0);
    const Point again=scene.press(field);
    scene.move(again.x+4, again.y);
    CHECK(field->getNumber() == doctest::Approx(10.0));
    scene.release();
}

TEST_CASE("DragNumber: motion is summed, so sub-unit integer steps add up and a drag back gives the start")
{
    Scene scene;
    DragNumber *field=scene.field();

    field->setInteger(true);
    CHECK(field->getSpeed() == doctest::Approx(1.0));
    CHECK(field->getFormat() == "%.0f");
    field->setNumber(5.0);
    CHECK(field->getText() == "5");

    Counter counter;
    field->onValueChanged.add(&counter, &Counter::changed);

    // Alt: a tenth of a unit per unit of motion, so ten moves of one make one
    const Point at=scene.press(field);
    scene.hold(keys::Alt, true);
    std::vector<double> values;
    for(int i=1; i <= 10; ++i)
    {
        scene.move(at.x+i, at.y);
        values.push_back(field->getNumber());
    }
    // 0.1..0.4 round to 5, 0.5 and up to 6
    CHECK(values == std::vector<double>{5, 5, 5, 5, 6, 6, 6, 6, 6, 6});
    CHECK(counter.count == 1);

    // back to where the press was, Alt still held: the value at the press
    scene.move(at.x, at.y);
    CHECK(field->getNumber() == 5.0);
    CHECK(counter.count == 2);
    scene.hold(keys::Alt, false);
    // the same way without Alt: ten units of motion are ten units of value
    scene.move(at.x-10, at.y);
    CHECK(field->getNumber() == -5.0);
    scene.release();
    scene.frame();

    // a float field at a low speed: each unit of motion moves it
    DragNumber *fine=scene.field(20, 60);
    fine->setSpeed(0.001);
    const Point onFine=scene.press(fine);
    for(int i=1; i <= 5; ++i)
        scene.move(onFine.x+i, onFine.y);
    CHECK(fine->getNumber() == doctest::Approx(0.005));
    CHECK(fine->getText() == "0.005");
    scene.release();
}

TEST_CASE("DragNumber: the range clamps, the drag turns back at once at its end, and an integer field rounds")
{
    Scene scene;
    DragNumber *field=scene.field();

    field->setInteger(true);
    field->setRange(10.0, 0.0);     // swapped
    CHECK(field->hasRange());
    CHECK(field->getMin() == 0.0);
    CHECK(field->getMax() == 10.0);

    Counter counter;
    field->onValueChanged.add(&counter, &Counter::changed);

    const Point at=scene.press(field);
    scene.move(at.x+50, at.y);
    CHECK(field->getNumber() == 10.0);
    CHECK(counter.count == 1);
    // further past the end: held there, no event
    scene.move(at.x+80, at.y);
    CHECK(field->getNumber() == 10.0);
    CHECK(counter.count == 1);
    // turning back moves it at once
    scene.move(at.x+77, at.y);
    CHECK(field->getNumber() == 7.0);
    CHECK(counter.count == 2);
    scene.release();

    field->setNumber(20.0);
    CHECK(field->getNumber() == 10.0);
    field->setNumber(-3.0);
    CHECK(field->getNumber() == 0.0);
    field->setNumber(1.4);
    CHECK(field->getNumber() == 1.0);
    field->setNumber(1.6);
    CHECK(field->getNumber() == 2.0);

    // a float field made integer rounds its value
    DragNumber *other=scene.field(20, 60);
    other->setNumber(2.6);
    other->setInteger(true);
    CHECK(other->getNumber() == 3.0);
    CHECK(other->getText() == "3");

    // a range whose ends are not whole keeps a rounded value inside it
    other->setRange(0.5, 2.5);
    other->setNumber(2.5);
    CHECK(other->getNumber() == 2.0);
    other->clearRange();
    other->setNumber(-1e9);
    CHECK(other->getNumber() == -1e9);
}

TEST_CASE("DragNumber: onValueChanged fires once per real change, never for a no-op")
{
    Scene scene;
    DragNumber *field=scene.field();
    Counter counter;

    field->onValueChanged.add(&counter, &Counter::changed);

    // a press and release without motion
    scene.click(field);
    CHECK(counter.count == 0);
    CHECK(field->getNumber() == 0.0);

    // a drag that only moves vertically
    const Point at=scene.press(field);
    scene.move(at.x, at.y+20);
    scene.release();
    CHECK(counter.count == 0);

    // the same value again, from the app or with doEvents off
    field->setNumber(0.0);
    CHECK(counter.count == 0);
    field->setNumber(3.0, false);
    CHECK(counter.count == 0);
    CHECK(field->getNumber() == 3.0);
    field->setNumber(4.0);
    CHECK(counter.count == 1);
    field->setNumber(4.0);
    CHECK(counter.count == 1);

    // an integer step of less than one
    field->setInteger(true);
    field->setSpeed(0.2);
    const int before=counter.count;
    const Point onIt=scene.press(field);
    scene.move(onIt.x+2, onIt.y);     // 4.4 rounds to 4
    CHECK(counter.count == before);
    scene.release();
}

TEST_CASE("DragNumber: a drag frame that keeps the shown string marks nothing, one that changes it the Text alone")
{
    Scene scene;
    DragNumber *field=scene.field();
    controls::internal::Text *text=textOf(field);
    REQUIRE(text != nullptr);

    field->setSpeed(0.0001);
    const Point at=scene.press(field);
    scene.frame();      // the down look

    // 0.0001: "0.000" stays
    scene.move(at.x+1, at.y);
    CHECK(field->getNumber() == doctest::Approx(0.0001));
    CHECK(field->getText() == "0.000");
    CHECK(field->getStateChange() == controls::StateChange_Nothing);
    CHECK(text->getStateChange() == controls::StateChange_Nothing);
    scene.frame();
    CHECK(scene.renderer.draws.empty());

    // 0.0010: "0.001", the Text's text alone
    scene.move(at.x+10, at.y);
    CHECK(field->getText() == "0.001");
    CHECK(field->getStateChange() == controls::StateChange_Nothing);
    CHECK((text->getStateChange() & controls::StateChange_Text) != 0);
    scene.frame();
    scene.release();
}

TEST_CASE("DragNumber: a double-click, a Ctrl+click and Return each open the edit box, its value selected")
{
    Scene scene;
    DragNumber *field=scene.field();

    field->setNumber(1.25);
    scene.frame();

    scene.doubleClick(field);
    REQUIRE(field->isEditing());
    controls::internal::DragNumberTextBox *box=field->getEditBox();
    REQUIRE(box != nullptr);
    CHECK_FALSE(box->hidden());
    CHECK(KeyboardFocus == box);
    CHECK(box->getText() == "1.25");
    CHECK(box->getSelection() == "1.25");
    CHECK((box->getBounds() == Rect(0, 0, field->getWidth(), field->getHeight())));
    CHECK(textOf(field)->hidden());
    // the field's own slot is hidden under the box
    const size_t slot=field->getSkinControl()->getPrimitives()[0];
    REQUIRE(scene.renderer.last(slot) != nullptr);
    CHECK(scene.renderer.last(slot)->hidden);
    // the double click's press did not start a drag
    CHECK_FALSE(field->isDepressed());

    scene.key(keys::Escape);
    CHECK_FALSE(field->isEditing());
    CHECK(box->hidden());
    CHECK_FALSE(textOf(field)->hidden());
    CHECK(KeyboardFocus == field);

    // Ctrl+click
    scene.hold(keys::Control, true);
    scene.click(field);
    scene.hold(keys::Control, false);
    CHECK(field->isEditing());
    CHECK(KeyboardFocus == box);
    CHECK(field->getEditBox() == box);
    scene.key(keys::Escape);
    CHECK_FALSE(field->isEditing());

    // Return on the focused field
    REQUIRE(KeyboardFocus == field);
    scene.key(keys::Return);
    CHECK(field->isEditing());
    scene.key(keys::Escape);

    // a plain click edits nothing
    scene.click(field);
    CHECK_FALSE(field->isEditing());
}

TEST_CASE("DragNumber: Return and a lost focus commit, Escape cancels, text that is no number reverts")
{
    Scene scene;
    DragNumber *field=scene.field();
    controls::TextBox *elsewhere=scene.canvas->newChild<controls::TextBox>();
    elsewhere->setPos(20, 100);
    field->setRange(0.0, 100.0);
    field->setNumber(5.0);
    scene.frame();

    Counter counter;
    field->onValueChanged.add(&counter, &Counter::changed);

    // Return
    scene.doubleClick(field);
    scene.type("42.5");
    scene.key(keys::Return);
    CHECK_FALSE(field->isEditing());
    CHECK(field->getNumber() == 42.5);
    CHECK(field->getText() == "42.500");
    CHECK(counter.count == 1);
    CHECK(KeyboardFocus == field);

    // the same value committed again: no event
    scene.doubleClick(field);
    scene.key(keys::Return);
    CHECK(field->getNumber() == 42.5);
    CHECK(counter.count == 1);

    // past the range: clamped
    scene.doubleClick(field);
    scene.type("250");
    scene.key(keys::Return);
    CHECK(field->getNumber() == 100.0);
    CHECK(counter.count == 2);

    // the focus going to another control commits
    scene.doubleClick(field);
    scene.type("7");
    scene.click(elsewhere);
    CHECK_FALSE(field->isEditing());
    CHECK(field->getNumber() == 7.0);
    CHECK(counter.count == 3);
    CHECK(KeyboardFocus == elsewhere);

    // Escape
    scene.doubleClick(field);
    scene.type("99");
    scene.key(keys::Escape);
    CHECK_FALSE(field->isEditing());
    CHECK(field->getNumber() == 7.0);
    CHECK(field->getText() == "7.000");
    CHECK(counter.count == 3);

    // text that is no number: the value and its text as they were
    for(const char *bad : {"", "-", ".", "1.2.3", "--4"})
    {
        CAPTURE(bad);
        scene.doubleClick(field);
        field->getEditBox()->setText(bad);
        scene.key(keys::Return);
        CHECK_FALSE(field->isEditing());
        CHECK(field->getNumber() == 7.0);
        CHECK(field->getText() == "7.000");
    }
    CHECK(counter.count == 3);
}

TEST_CASE("DragNumber: hidden while being typed into, the edit ends and commits in that frame's update phase")
{
    Scene scene;
    controls::Base *host=scene.canvas->newChild<controls::Base>();
    host->setBounds(0, 0, 640, 480);
    DragNumber *field=host->newChild<DragNumber>();
    field->setPos(20, 20);
    field->setRange(0.0, 100.0);
    field->setNumber(5.0);
    scene.frame();

    Counter counter;
    field->onValueChanged.add(&counter, &Counter::changed);

    scene.doubleClick(field);
    REQUIRE(field->isEditing());
    controls::internal::DragNumberTextBox *box=field->getEditBox();
    Counter lostFocus;
    box->onLostFocusCaller.add(&lostFocus, &Counter::changed);
    scene.type("7");

    // the page hidden, as a tab switch does
    host->setHidden(true);
    scene.frame();

    const controls::LayoutStats &stats=scene.canvas->getLayoutStats();

    CHECK(KeyboardFocus == nullptr);
    CHECK_FALSE(field->isEditing());
    CHECK(field->getNumber() == 7.0);
    CHECK(counter.count == 1);
    CHECK(box->hidden());
    CHECK_FALSE(textOf(field)->hidden());
    // laid out in the update phase, in one pass
    CHECK(stats.passes == 1);
    CHECK_FALSE(stats.renderLayout);
    // the box's onLostKeyboardFocus() ran, once
    CHECK(lostFocus.count == 1);

    // shown again, away from the mouse: an unedited field, at rest
    scene.move(400, 400);
    host->setHidden(false);
    scene.frame();
    CHECK_FALSE(field->isEditing());
    CHECK(box->hidden());
    CHECK_FALSE(textOf(field)->hidden());
    CHECK(textOf(field)->getText() == "7.000");
    const size_t slot=field->getSkinControl()->getPrimitives()[0];
    const FieldRecorder::Draw *draw=scene.renderer.last(slot);
    REQUIRE(draw != nullptr);
    CHECK_FALSE(draw->hidden);
    CHECK(sameColor(draw->fill, tokyonight::DragNumber.top));
    CHECK(sameColor(draw->border, tokyonight::DragNumber.border));
    CHECK(scene.canvas->getLayoutStats().passes == 1);
    CHECK_FALSE(scene.canvas->getLayoutStats().renderLayout);
    CHECK(lostFocus.count == 1);
    CHECK(counter.count == 1);

    // a settled frame draws nothing
    scene.frame();
    CHECK(scene.renderer.draws.empty());
}

TEST_CASE("DragNumber: its edit box made input-disabled while typed into loses the focus and commits")
{
    Scene scene;
    DragNumber *field=scene.field();
    field->setRange(0.0, 100.0);
    field->setNumber(5.0);
    scene.frame();

    scene.doubleClick(field);
    REQUIRE(field->isEditing());
    controls::internal::DragNumberTextBox *box=field->getEditBox();
    Counter lostFocus;
    box->onLostFocusCaller.add(&lostFocus, &Counter::changed);
    scene.type("12");

    box->setKeyboardInputEnabled(false);
    scene.frame();

    const controls::LayoutStats &stats=scene.canvas->getLayoutStats();

    CHECK(KeyboardFocus == nullptr);
    CHECK(lostFocus.count == 1);
    CHECK_FALSE(field->isEditing());
    CHECK(field->getNumber() == 12.0);
    CHECK(box->hidden());
    CHECK_FALSE(textOf(field)->hidden());
    CHECK(stats.passes == 1);
    CHECK_FALSE(stats.renderLayout);

    // once
    scene.frame();
    CHECK(lostFocus.count == 1);
    CHECK(scene.renderer.draws.empty());
}

TEST_CASE("DragNumber: the edit box's ids are taken by the first edit only")
{
    Scene scene;
    const test::IdBankCounts before=test::IdBankCounts::now();
    DragNumber *field=scene.field();
    const test::IdBankCounts withField=test::IdBankCounts::now();

    scene.doubleClick(field);
    REQUIRE(field->isEditing());
    scene.key(keys::Escape);
    controls::internal::DragNumberTextBox *box=field->getEditBox();
    const test::IdBankCounts edited=test::IdBankCounts::now();

    CHECK(edited.livePrimitives() > withField.livePrimitives());
    CHECK(edited.liveTexts() > withField.liveTexts());

    for(int i=0; i < 3; ++i)
    {
        scene.doubleClick(field);
        CHECK(field->getEditBox() == box);
        scene.key(keys::Escape);

        const test::IdBankCounts again=test::IdBankCounts::now();

        CHECK(again.livePrimitives() == edited.livePrimitives());
        CHECK(again.liveTexts() == edited.liveTexts());
    }

    // deleting the field gives them all back
    field->delayedDelete();
    scene.frame();
    scene.frame();

    const test::IdBankCounts after=test::IdBankCounts::now();

    CHECK(after.livePrimitives() == before.livePrimitives());
    CHECK(after.liveTexts() == before.liveTexts());
}

TEST_CASE("DragNumber: Up/Right and Down/Left step the focused field, Shift and Alt as for a drag")
{
    Scene scene;
    DragNumber *field=scene.field();

    field->focus();
    scene.key(keys::Up);
    CHECK(field->getNumber() == doctest::Approx(0.1));
    scene.key(keys::Right);
    CHECK(field->getNumber() == doctest::Approx(0.2));
    scene.key(keys::Down);
    scene.key(keys::Left);
    CHECK(field->getNumber() == doctest::Approx(0.0));

    scene.hold(keys::Shift, true);
    scene.key(keys::Up);
    scene.hold(keys::Shift, false);
    CHECK(field->getNumber() == doctest::Approx(1.0));

    // an integer field steps a whole unit at least
    field->setInteger(true);
    field->setSpeed(0.25);
    scene.key(keys::Up);
    CHECK(field->getNumber() == 2.0);

    // while editing, the keys stay in the box
    scene.doubleClick(field);
    scene.key(keys::Up);
    scene.key(keys::Down);
    CHECK(field->getNumber() == 2.0);
    scene.key(keys::Escape);
}

TEST_CASE("DragNumber: the format must hold one conversion of a double")
{
    Scene scene;
    DragNumber *field=scene.field();

    field->setNumber(1.5);
    CHECK(field->setFormat("%.1f ms"));
    CHECK(field->getText() == "1.5 ms");
    CHECK(field->getValue() == "1.5");
    for(const char *bad : {"%s", "%d", "%.1f %.1f", "no conversion", "%n", "%.123f"})
    {
        CAPTURE(bad);
        CHECK_FALSE(field->setFormat(bad));
        CHECK(field->getFormat() == "%.1f ms");
    }
    CHECK(field->setFormat("%e"));
    CHECK(field->getText() == "1.500000e+00");
    CHECK(field->setFormat(""));
    CHECK(field->getFormat() == "%.3f");

    // the edit text: the format's precision, trailing zeros dropped
    field->setFormat("%.4f");
    field->setNumber(0.125);
    CHECK(field->editText() == "0.125");
    field->setNumber(-0.00001);
    CHECK(field->editText() == "0");

    double value=0.0;
    CHECK(DragNumber::parseNumber(" 2.5 ", value));
    CHECK(value == 2.5);
    CHECK(DragNumber::parseNumber("-1e3", value));
    CHECK(value == -1000.0);
    for(const char *bad : {"", " ", "abc", "1x", "1e999", "nan", "inf"})
    {
        CAPTURE(bad);
        value=4.0;
        CHECK_FALSE(DragNumber::parseNumber(bad, value));
        CHECK(value == 4.0);
    }
    field->setValue("3.25");
    CHECK(field->getNumber() == 3.25);
    field->setValue("nope");
    CHECK(field->getNumber() == 3.25);
}

TEST_CASE("DragNumber: the textured renderer draws one slot by state, and a disabled field takes no drag")
{
    Scene scene;
    DragNumber *field=scene.field();
    const auto &primitives=field->getSkinControl()->getPrimitives();
    REQUIRE(primitives.size() == 1);
    const size_t slot=primitives[0];

    auto look=[&](const tokyonight::Surface &surface)
    {
        const FieldRecorder::Draw *draw=scene.renderer.last(slot);

        return draw && !draw->hidden && sameColor(draw->fill, surface.top) && sameColor(draw->border, surface.border);
    };

    field->addStateChange(controls::StateChange_Background);
    scene.frame();
    CHECK(look(tokyonight::DragNumber));

    const Point at=scene.centreOf(field);
    scene.move(at.x, at.y);
    scene.frame();
    CHECK(look(tokyonight::DragNumberHover));
    // a hover draws the field's slot and nothing else
    for(const FieldRecorder::Draw &draw : scene.renderer.draws)
        CHECK(draw.id == slot);

    scene.press(field);
    scene.frame();
    CHECK(look(tokyonight::DragNumberDown));
    scene.release();
    scene.move(400, 400);
    scene.frame();
    CHECK(look(tokyonight::DragNumberFocus));

    // a settled frame draws nothing
    scene.frame();
    CHECK(scene.renderer.draws.empty());

    field->setDisabled(true);
    scene.frame();
    CHECK(look(tokyonight::DragNumberDisabled));
    const Point onIt=scene.press(field);
    scene.move(onIt.x+30, onIt.y);
    scene.release();
    CHECK(field->getNumber() == 0.0);
    CHECK_FALSE(field->isDepressed());
    scene.doubleClick(field);
    CHECK_FALSE(field->isEditing());
}

namespace
{

using BoundsList=std::vector<std::pair<const controls::Base *, Rect>>;

//! Every control's bounds under \p control, but \p skip's subtree.
void collectBounds(const controls::Base *control, const controls::Base *skip, BoundsList &out)
{
    if(control == skip)
        return;
    out.push_back({control, control->getBounds()});
    for(const controls::Base *child : control->getActualChildren())
        collectBounds(child, skip, out);
}

//! Every control's bounds and the field's preferred size, as they were when
//! taken: the edit box opening or closing must leave them so.
struct LayoutSnapshot
{
    Scene &scene;
    DragNumber *field;
    BoundsList bounds;
    Size preferred;

    LayoutSnapshot(Scene &scene, DragNumber *field):
        scene(scene),
        field(field)
    {
        collectBounds(scene.canvas, field->getEditBox(), bounds);
        preferred=field->getPreferredSize();
    }

    void check(const char *step) const
    {
        const String when=step;

        CAPTURE(when);

        BoundsList now;

        collectBounds(scene.canvas, field->getEditBox(), now);
        REQUIRE(now.size() == bounds.size());
        for(size_t i=0; i < now.size(); ++i)
        {
            const String type=now[i].first->getTypeName();
            const Rect &was=bounds[i].second;
            const Rect &is=now[i].second;

            CAPTURE(type);
            CAPTURE(i);
            CHECK(now[i].first == bounds[i].first);
            CHECK(is.x == was.x);
            CHECK(is.y == was.y);
            CHECK(is.w == was.w);
            CHECK(is.h == was.h);
        }
        CHECK(field->getPreferredSize().width == preferred.width);
        CHECK(field->getPreferredSize().height == preferred.height);
        // one pass, in the update phase
        CHECK(scene.canvas->getLayoutStats().passes == 1);
        CHECK_FALSE(scene.canvas->getLayoutStats().renderLayout);
        if(field->isEditing())
        {
            const Rect box=field->getEditBox()->getBounds();

            CHECK(box.x == 0);
            CHECK(box.y == 0);
            CHECK(box.w == field->getWidth());
            CHECK(box.h == field->getHeight());
        }
    }
};

//! Lays the scene out until a frame runs no pass.
void settle(Scene &scene)
{
    for(int i=0; i < 10; ++i)
    {
        scene.frame();
        if(scene.canvas->getLayoutStats().passes == 0)
            break;
    }
}

//! Opens \p field's edit box each way (a double-click, a Ctrl+click, Return)
//! and closes it each way (Return, Escape, a click on \p elsewhere), the value
//! left as it was, and checks the layout after every open and close frame.
void checkEditKeepsLayout(Scene &scene, DragNumber *field, controls::Base *elsewhere)
{
    settle(scene);
    const double value=field->getNumber();
    const LayoutSnapshot before(scene, field);
    REQUIRE(field->getWidth() > 0);
    REQUIRE(field->getHeight() > 0);

    // a double-click, Return committing the value it shows
    scene.doubleClick(field);
    REQUIRE(field->isEditing());
    before.check("opened by a double-click");
    scene.key(keys::Return);
    REQUIRE_FALSE(field->isEditing());
    before.check("committed by Return");

    // a Ctrl+click, typed into, Escape
    scene.hold(keys::Control, true);
    scene.click(field);
    scene.hold(keys::Control, false);
    REQUIRE(field->isEditing());
    before.check("opened by a Ctrl+click");
    scene.type("42");
    scene.frame();
    before.check("typed into");
    scene.key(keys::Escape);
    REQUIRE_FALSE(field->isEditing());
    before.check("cancelled by Escape");

    // Return on the focused field, the focus then lost
    REQUIRE(KeyboardFocus == field);
    scene.key(keys::Return);
    REQUIRE(field->isEditing());
    before.check("opened by Return");
    scene.click(elsewhere);
    REQUIRE_FALSE(field->isEditing());
    before.check("committed by a lost focus");

    CHECK(field->getNumber() == value);
}

}//namespace

TEST_CASE("DragNumber: docked Top, Elastic, the edit box opening and closing moves nothing and keeps its size")
{
    Scene scene;
    controls::Base *parent=scene.canvas->newChild<controls::Base>();
    parent->setBounds(0, 0, 300, 200);
    DragNumber *field=parent->newChild<DragNumber>();
    field->setDock(DockPosition::Top);
    field->setSizeFlags({SizeFlag::Elastic, SizeFlag::Elastic});
    field->setNumber(128.0);
    controls::Base *below=parent->newChild<controls::Base>();
    below->setDock(DockPosition::Top);
    below->setHeight(30);
    controls::TextBox *elsewhere=scene.canvas->newChild<controls::TextBox>();
    elsewhere->setPos(20, 300);

    checkEditKeepsLayout(scene, field, elsewhere);
}

TEST_CASE("DragNumber: Elastic in a HorizontalLayout cell, the edit box opening and closing moves nothing")
{
    Scene scene;
    // a row as high as its cells, above a block docked Top
    controls::Base *row=scene.canvas->newChild<controls::Base>();
    row->setDock(DockPosition::Top);
    controls::HorizontalLayout *layout=new controls::HorizontalLayout();
    row->setLayout(layout);
    DragNumber *field=row->newChild<DragNumber>();
    field->setSizeFlags({SizeFlag::Elastic, SizeFlag::Elastic});
    field->setNumber(128.0);
    layout->addControl(field);
    controls::Base *next=row->newChild<controls::Base>();
    next->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    next->setSize(10, 4);
    layout->addControl(next);
    controls::Base *below=scene.canvas->newChild<controls::Base>();
    below->setDock(DockPosition::Top);
    below->setHeight(30);
    controls::TextBox *elsewhere=scene.canvas->newChild<controls::TextBox>();
    elsewhere->setPos(20, 300);

    checkEditKeepsLayout(scene, field, elsewhere);
}

TEST_CASE("DragNumber: Elastic in X, docked Left, the edit box keeps its width, and a new value still widens it")
{
    Scene scene;
    controls::Base *parent=scene.canvas->newChild<controls::Base>();
    parent->setBounds(0, 0, 400, 40);
    DragNumber *field=parent->newChild<DragNumber>();
    field->setDock(DockPosition::Left);
    field->setSizeFlags({SizeFlag::Elastic, SizeFlag::Fixed});
    field->setNumber(5.0);
    controls::Base *next=parent->newChild<controls::Base>();
    next->setDock(DockPosition::Left);
    next->setWidth(30);
    controls::TextBox *elsewhere=scene.canvas->newChild<controls::TextBox>();
    elsewhere->setPos(20, 300);

    checkEditKeepsLayout(scene, field, elsewhere);

    // the app sets a longer value while the box is open: the field widens to
    // it, the box with it, and the block after it moves
    scene.doubleClick(field);
    REQUIRE(field->isEditing());
    settle(scene);
    const int width=field->getWidth();
    const int nextX=next->getX();
    field->setNumber(123456789.0);
    settle(scene);
    CHECK(field->getWidth() > width);
    CHECK(next->getX() > nextX);
    CHECK((field->getEditBox()->getBounds() == Rect(0, 0, field->getWidth(), field->getHeight())));
    const int wide=field->getWidth();
    scene.key(keys::Escape);
    CHECK(field->getWidth() == wide);
    CHECK(field->getText() == "123456789.000");
}
