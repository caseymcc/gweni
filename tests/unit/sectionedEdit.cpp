/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  internal::SectionedEdit, the field DateEdit and TimeEdit are, on a real
 *  Canvas with the real (un-init()'d) textured skin on a StubRenderer, keyed
 *  through input::onKeyEvent and typed through Canvas::inputCharacter.
 *
 *  setFormat takes the vocabulary's date and time patterns and refuses one
 *  missing a date part, mixing a date and a time, repeating a section,
 *  joining two sections with no separator, a 12-hour hour without AP (or AP
 *  with a 24-hour one) and an unknown letter, keeping the format it had; the
 *  text is each format's ("2026-11-01", "01/11/2026", "11/1/2026", "14:05",
 *  "2:05 PM", "02:05 PM"). The section under the caret is the run it is in or
 *  at the end of, the next after a separator. Up and Down step that section
 *  by one, wrapping inside it with no carry (day 31 to 1 and 1 to 31 in the
 *  same month, 30 to 1 in a 30-day month, December to January in the same
 *  year, minute 59 to 0 in the same hour, hour 23 to 0, AP switching), a
 *  month step from the 31st clamping the day (to the 30th, to February 28
 *  or 29), and keep the caret in its section; PageUp and PageDown take a
 *  year in the day and month sections, ten in the year's, ten minutes in
 *  the minute's and twelve hours in the hour's. The minute step steps to its
 *  next multiple either way and keeps a typed minute as typed. The parse is
 *  lenient ("2026-11-1", "11/1/2026" for M/D/YYYY, "20261101", "26-1-2",
 *  "1430", "2:30 pm") and text that is no value reverts on Return, on the
 *  focus leaving and on Escape (which, with nothing typed, goes on to the
 *  parent); a typed and a stepped value are clamped to the range, a step at
 *  the limit wrapping round to the range's other end; an empty text is no
 *  value only with setAllowEmpty, and a step from empty shows the empty-step
 *  value. A value, a range end or an empty-step value that is no date or
 *  time (a 13th month, 24:00) is refused, keeping what the field had.
 *  onValueChanged fires once per real change by the user and never from a
 *  setter. The width is the format's at the font, whatever the text
 *  (an empty field's too) and whatever size the app asks for, and a step
 *  that keeps the string's width lays nothing out.
 */
#include "headlessFixture.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/sectionedEdit.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <doctest/doctest.h>

using namespace gweni;
using controls::internal::SectionedEdit;
using Value=SectionedEdit::Value;

namespace
{

Value dateValue(int y, int m, int d)
{
    Value value;
    value.date=Date(y, m, d);
    return value;
}

Value timeValue(int h, int m)
{
    Value value;
    value.time=TimeOfDay(h, m);
    return value;
}

//! Counts onValueChanged.
struct Changes: public event::Handler
{
    int count=0;

    void changed(event::Info) { ++count; }
};

struct Scene
{
    test::StubRenderer renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::Base *parent=nullptr;
    SectionedEdit *edit=nullptr;
    Changes events;
    int &changes=events.count;
    int parentEscapes=0;

    struct Parent: public controls::Base
    {
        GWENI_CONTROL_INLINE(Parent, controls::Base)
        {
        }
        GWENI_DRAWS_NOTHING

        bool onKeyEscape(bool down) override
        {
            if(down && escapes)
                ++*escapes;
            return true;
        }

        int *escapes=nullptr;
    };

    Scene()
    {
        platform::setTimeSource([]() { return 10.0; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);

        Parent *holder=canvas->newChild<Parent>();
        holder->escapes=&parentEscapes;
        holder->setBounds(0, 0, 600, 400);
        parent=holder;

        edit=holder->newChild<SectionedEdit>();
        edit->setPos(20, 20);
        edit->onValueChanged.add(&events, &Changes::changed);
        frame();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
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

    void key(int key)
    {
        input::onKeyEvent(canvas, key, true);
        input::onKeyEvent(canvas, key, false);
        frame();
    }

    //! The caret at \p pos, the field focused.
    void caretAt(int pos)
    {
        edit->focus();
        edit->setCursorPos(pos);
        edit->setCursorend(pos);
    }

    //! Focuses the field and types \p text over all of it.
    void type(const char *text)
    {
        edit->focus();
        edit->onSelectAll(edit);
        for(const char *c=text; *c; ++c)
            canvas->inputCharacter(static_cast<UnicodeChar>(*c));
    }

    //! Steps the section at \p caret with \p key and returns the text.
    String step(int caret, int key)
    {
        caretAt(caret);
        this->key(key);
        return edit->getText();
    }
};

}//namespace

TEST_CASE("SectionedEdit: formats and their text")
{
    Scene scene;
    SectionedEdit *edit=scene.edit;

    edit->setFieldValue(dateValue(2026, 11, 1));
    CHECK(edit->getText() == "2026-11-01");
    CHECK(edit->isDateFormat());

    CHECK(edit->setFormat("DD/MM/YYYY"));
    CHECK(edit->getText() == "01/11/2026");
    CHECK(edit->setFormat("M/D/YYYY"));
    CHECK(edit->getText() == "11/1/2026");
    CHECK(edit->getSections().size() == 3);

    // refused, keeping M/D/YYYY
    CHECK_FALSE(edit->setFormat("YYYY-MM"));
    CHECK_FALSE(edit->setFormat("YYYY-MM-DD HH:mm"));
    CHECK_FALSE(edit->setFormat("YYYY-MM-DD-DD"));
    CHECK_FALSE(edit->setFormat("YYYYMMDD"));
    CHECK_FALSE(edit->setFormat("h:mm"));
    CHECK_FALSE(edit->setFormat("HH:mm AP"));
    CHECK_FALSE(edit->setFormat("YYYY-MM-DDx"));
    CHECK_FALSE(edit->setFormat(""));
    CHECK(edit->getFormat() == "M/D/YYYY");
    CHECK(edit->getText() == "11/1/2026");

    edit->setFieldValue(timeValue(14, 5));
    CHECK(edit->setFormat("HH:mm"));
    CHECK_FALSE(edit->isDateFormat());
    CHECK(edit->getText() == "14:05");
    CHECK(edit->setFormat("h:mm AP"));
    CHECK(edit->getText() == "2:05 PM");
    CHECK(edit->setFormat("hh:mm AP"));
    CHECK(edit->getText() == "02:05 PM");
    edit->setFieldValue(timeValue(0, 0));
    CHECK(edit->setFormat("h:mm AP"));
    CHECK(edit->getText() == "12:00 AM");
    edit->setFieldValue(timeValue(12, 0));
    CHECK(edit->getText() == "12:00 PM");
    CHECK(edit->setFormat("H:mm"));
    CHECK(edit->getText() == "12:00");
}

TEST_CASE("SectionedEdit: the section under the caret")
{
    Scene scene;
    SectionedEdit *edit=scene.edit;

    edit->setFieldValue(dateValue(2026, 11, 1));
    // "2026-11-01"
    CHECK(edit->sectionAt(0) == 0);
    CHECK(edit->sectionAt(4) == 0);
    CHECK(edit->sectionAt(5) == 1);
    CHECK(edit->sectionAt(7) == 1);
    CHECK(edit->sectionAt(8) == 2);
    CHECK(edit->sectionAt(10) == 2);

    edit->setFieldValue(timeValue(14, 5));
    edit->setFormat("h:mm AP");
    // "2:05 PM"
    CHECK(edit->sectionAt(1) == 0);
    CHECK(edit->sectionAt(2) == 1);
    CHECK(edit->sectionAt(5) == 2);
    CHECK(edit->sectionAt(7) == 2);
}

TEST_CASE("SectionedEdit: Up and Down step a date's sections, wrapping with no carry")
{
    Scene scene;
    SectionedEdit *edit=scene.edit;

    edit->setFieldValue(dateValue(2026, 12, 31));
    // the day: 31 to 1 in December, and back
    CHECK(scene.step(9, keys::Up) == "2026-12-01");
    CHECK(scene.step(9, keys::Down) == "2026-12-31");
    // the caret stays in the day
    CHECK(edit->getCursorPos() == 9);
    // the month: December to January in the same year
    CHECK(scene.step(6, keys::Up) == "2026-01-31");
    CHECK(scene.step(6, keys::Down) == "2026-12-31");
    // the year
    CHECK(scene.step(2, keys::Up) == "2027-12-31");
    CHECK(scene.step(2, keys::Down) == "2026-12-31");
    CHECK(scene.changes == 6);

    // the 31st a month on, into a 30-day month and into February
    CHECK(scene.step(6, keys::Down) == "2026-11-30");
    edit->setFieldValue(dateValue(2026, 1, 31));
    CHECK(scene.step(6, keys::Up) == "2026-02-28");
    edit->setFieldValue(dateValue(2024, 1, 31));
    CHECK(scene.step(6, keys::Up) == "2024-02-29");
    // the day wraps at the month's own length
    edit->setFieldValue(dateValue(2026, 11, 30));
    CHECK(scene.step(9, keys::Up) == "2026-11-01");
    CHECK(scene.step(9, keys::Down) == "2026-11-30");
    edit->setFieldValue(dateValue(2024, 2, 29));
    CHECK(scene.step(9, keys::Up) == "2024-02-01");
    // a leap day a year on is the 28th
    edit->setFieldValue(dateValue(2024, 2, 29));
    CHECK(scene.step(2, keys::Up) == "2025-02-28");
}

TEST_CASE("SectionedEdit: PageUp and PageDown take a year, ten years, ten minutes, twelve hours")
{
    Scene scene;
    SectionedEdit *edit=scene.edit;

    edit->setFieldValue(dateValue(2026, 11, 15));
    CHECK(scene.step(9, keys::PageUp) == "2027-11-15");
    CHECK(scene.step(6, keys::PageUp) == "2028-11-15");
    CHECK(scene.step(6, keys::PageDown) == "2027-11-15");
    CHECK(scene.step(2, keys::PageUp) == "2037-11-15");
    CHECK(scene.step(2, keys::PageDown) == "2027-11-15");

    edit->setFieldValue(timeValue(14, 55));
    edit->setFormat("HH:mm");
    CHECK(scene.step(4, keys::PageUp) == "14:05");
    CHECK(scene.step(4, keys::PageDown) == "14:55");
    CHECK(scene.step(1, keys::PageUp) == "02:55");
    CHECK(scene.step(1, keys::PageDown) == "14:55");
}

TEST_CASE("SectionedEdit: a time's sections, AM and PM")
{
    Scene scene;
    SectionedEdit *edit=scene.edit;

    edit->setFieldValue(timeValue(23, 59));
    edit->setFormat("HH:mm");
    // the minute: 59 to 0 in the same hour
    CHECK(scene.step(4, keys::Up) == "23:00");
    CHECK(scene.step(4, keys::Down) == "23:59");
    // the hour: 23 to 0
    CHECK(scene.step(1, keys::Up) == "00:59");
    CHECK(scene.step(1, keys::Down) == "23:59");

    edit->setFormat("h:mm AP");
    edit->setFieldValue(timeValue(11, 30));
    CHECK(edit->getText() == "11:30 AM");
    // 11 AM an hour on is 12 PM
    CHECK(scene.step(1, keys::Up) == "12:30 PM");
    CHECK(scene.step(1, keys::Up) == "1:30 PM");
    CHECK(scene.step(0, keys::Down) == "12:30 PM");
    // AP steps switch it
    CHECK(scene.step(7, keys::Up) == "12:30 AM");
    CHECK(edit->getFieldValue()->time.toText() == "00:30");
    CHECK(scene.step(7, keys::Down) == "12:30 PM");
    CHECK(edit->getFieldValue()->time.toText() == "12:30");

    // A and P typed in the AP section, any other character typed
    const int before=scene.changes;
    scene.caretAt(7);
    scene.canvas->inputCharacter('a');
    CHECK(edit->getText() == "12:30 AM");
    scene.canvas->inputCharacter('A');
    CHECK(edit->getText() == "12:30 AM");
    scene.canvas->inputCharacter('P');
    CHECK(edit->getText() == "12:30 PM");
    CHECK(scene.changes == before+2);
    scene.caretAt(2);
    scene.canvas->inputCharacter('p');
    CHECK(edit->getText() == "12p:30 PM");
}

TEST_CASE("SectionedEdit: the minute step")
{
    Scene scene;
    SectionedEdit *edit=scene.edit;

    edit->setFormat("HH:mm");
    edit->setMinuteStep(15);
    edit->setFieldValue(timeValue(9, 0));
    CHECK(scene.step(4, keys::Up) == "09:15");
    CHECK(scene.step(4, keys::Up) == "09:30");
    CHECK(scene.step(4, keys::Up) == "09:45");
    CHECK(scene.step(4, keys::Up) == "09:00");
    CHECK(scene.step(4, keys::Down) == "09:45");

    // a typed minute is kept, and steps to the step's next multiple
    scene.type("9:07");
    scene.key(keys::Return);
    CHECK(edit->getText() == "09:07");
    CHECK(scene.step(4, keys::Up) == "09:15");
    scene.type("9:07");
    scene.key(keys::Return);
    CHECK(scene.step(4, keys::Down) == "09:00");
    // PageUp with a step of 15 is 15 minutes; with 5, ten
    CHECK(scene.step(4, keys::PageUp) == "09:15");
    edit->setMinuteStep(5);
    CHECK(scene.step(4, keys::PageUp) == "09:25");
    CHECK(scene.step(4, keys::Up) == "09:30");
    // refused steps
    edit->setMinuteStep(0);
    edit->setMinuteStep(61);
    CHECK(edit->getMinuteStep() == 5);
}

TEST_CASE("SectionedEdit: the lenient parse, and text that is no value reverting")
{
    Scene scene;
    SectionedEdit *edit=scene.edit;

    CHECK(edit->parse("2026-11-1")->date.toIso() == "2026-11-01");
    CHECK(edit->parse("2026 / 11 / 01")->date.toIso() == "2026-11-01");
    CHECK(edit->parse("20261101")->date.toIso() == "2026-11-01");
    CHECK(edit->parse("26-1-2")->date.toIso() == "2026-01-02");
    CHECK_FALSE(edit->parse("2026-02-30").has_value());
    CHECK_FALSE(edit->parse("2026-11").has_value());
    CHECK_FALSE(edit->parse("Nov 1 2026").has_value());
    CHECK_FALSE(edit->parse("2026-111-1").has_value());
    edit->setFormat("M/D/YYYY");
    CHECK(edit->parse("11/1/2026")->date.toIso() == "2026-11-01");
    edit->setFormat("DD/MM/YYYY");
    CHECK(edit->parse("1/11/2026")->date.toIso() == "2026-11-01");
    CHECK(edit->parse("01112026")->date.toIso() == "2026-11-01");
    edit->setFormat("HH:mm");
    CHECK(edit->parse("1430")->time.toText() == "14:30");
    CHECK(edit->parse("2:30 pm")->time.toText() == "14:30");
    edit->setFormat("h:mm AP");
    CHECK(edit->parse("1430")->time.toText() == "14:30");
    CHECK(edit->parse("2:30 pm")->time.toText() == "14:30");

    edit->setFormat("YYYY-MM-DD");
    edit->setFieldValue(dateValue(2026, 1, 2));
    scene.changes=0;

    // typed and taken on Return
    scene.type("2026-11-1");
    CHECK(edit->getText() == "2026-11-1");
    scene.key(keys::Return);
    CHECK(edit->getText() == "2026-11-01");
    CHECK(edit->getFieldValue()->date.toIso() == "2026-11-01");
    CHECK(scene.changes == 1);
    // the same value typed again changes nothing
    scene.type("20261101");
    scene.key(keys::Return);
    CHECK(edit->getText() == "2026-11-01");
    CHECK(scene.changes == 1);

    // no value: reverted on Return
    scene.type("2026-13-01");
    scene.key(keys::Return);
    CHECK(edit->getText() == "2026-11-01");
    // on the focus leaving
    scene.type("hello");
    edit->unfocus();
    CHECK(edit->getText() == "2026-11-01");
    // taken on the focus leaving
    scene.type("2026-12-24");
    edit->unfocus();
    CHECK(edit->getFieldValue()->date.toIso() == "2026-12-24");
    CHECK(scene.changes == 2);
    // on Escape, which the field takes; with nothing typed it goes on up
    scene.type("2026-12-2");
    scene.key(keys::Escape);
    CHECK(edit->getText() == "2026-12-24");
    CHECK(scene.parentEscapes == 0);
    scene.key(keys::Escape);
    CHECK(scene.parentEscapes == 1);
    CHECK(scene.changes == 2);
    // an empty text where none is allowed
    scene.type(" ");
    scene.key(keys::Return);
    CHECK(edit->getText() == "2026-12-24");
    CHECK(scene.changes == 2);
}

TEST_CASE("SectionedEdit: the range clamps a typed and a stepped value")
{
    Scene scene;
    SectionedEdit *edit=scene.edit;

    edit->setFieldValue(dateValue(2026, 11, 15));
    edit->setFieldRange(dateValue(2026, 11, 5), dateValue(2026, 12, 18));
    scene.changes=0;

    scene.type("2027-01-01");
    scene.key(keys::Return);
    CHECK(edit->getText() == "2026-12-18");
    scene.type("2026-01-01");
    scene.key(keys::Return);
    CHECK(edit->getText() == "2026-11-05");
    CHECK(scene.changes == 2);

    // a year on is past the limit: clamped to it
    CHECK(scene.step(2, keys::Up) == "2026-12-18");
    // at the limit the day wraps round to the range's start
    CHECK(scene.step(9, keys::Up) == "2026-12-01");
    edit->setFieldValue(dateValue(2026, 11, 5));
    CHECK(scene.step(9, keys::Down) == "2026-11-30");
    // the setter clamps, with no event
    scene.changes=0;
    edit->setFieldValue(dateValue(2030, 1, 1));
    CHECK(edit->getText() == "2026-12-18");
    edit->setFieldRange(std::nullopt, dateValue(2026, 12, 1));
    CHECK(edit->getText() == "2026-12-01");
    CHECK(scene.changes == 0);
}

TEST_CASE("SectionedEdit: empty allowed and not")
{
    Scene scene;
    SectionedEdit *edit=scene.edit;

    edit->setFieldValue(dateValue(2026, 11, 15));
    edit->setFieldValue(std::nullopt);
    CHECK(edit->getFieldValue().has_value());

    edit->setAllowEmpty(true);
    scene.changes=0;
    scene.type("");
    edit->onKeyBackspace(true);
    CHECK(edit->getText().empty());
    scene.key(keys::Return);
    CHECK_FALSE(edit->getFieldValue().has_value());
    CHECK(scene.changes == 1);

    // a step from empty shows the empty-step value
    edit->setEmptyStepValue(dateValue(2026, 6, 1));
    CHECK(scene.step(0, keys::Up) == "2026-06-01");
    CHECK(scene.changes == 2);

    edit->setFieldValue(std::nullopt);
    CHECK(edit->getText().empty());
    CHECK(scene.changes == 2);
    // not allowed any more: the empty-step value
    edit->setAllowEmpty(false);
    CHECK(edit->getText() == "2026-06-01");
}

TEST_CASE("SectionedEdit: a value, a range end or an empty-step value that is none is refused")
{
    Scene scene;
    SectionedEdit *edit=scene.edit;

    edit->setFieldValue(dateValue(2026, 11, 15));
    edit->setFieldRange(dateValue(2026, 11, 5), dateValue(2026, 12, 18));
    edit->setEmptyStepValue(dateValue(2026, 11, 20));
    scene.changes=0;

    // a 13th month and a 30th of February are no dates, 24:00 no time
    edit->setFieldValue(dateValue(2026, 13, 1));
    CHECK(edit->getText() == "2026-11-15");
    edit->setFieldRange(dateValue(2026, 2, 30), std::nullopt);
    REQUIRE(edit->getFieldMin().has_value());
    CHECK(edit->getFieldMin()->date.toIso() == "2026-11-05");
    edit->setEmptyStepValue(dateValue(2026, 11, 31));
    CHECK(edit->getEmptyStepValue().date.toIso() == "2026-11-20");
    edit->setEmptyStepValue(timeValue(24, 0));
    CHECK(edit->getEmptyStepValue().time.toText() == "00:00");
    // so a step from empty shows the last good one
    edit->setAllowEmpty(true);
    edit->setFieldValue(std::nullopt);
    CHECK(scene.step(9, keys::Up) == "2026-11-20");
    CHECK(scene.changes == 1);
}

TEST_CASE("SectionedEdit: the width is the format's, and a step of the same width lays nothing out")
{
    Scene scene;
    SectionedEdit *edit=scene.edit;

    edit->setFieldValue(dateValue(2026, 11, 15));
    scene.frame();
    const int width=edit->getWidth();
    CHECK(width > 0);
    CHECK(width < 200);

    // the app's size is not used
    edit->setWidth(300);
    scene.frame();
    CHECK(edit->getWidth() == width);

    // nor the text's
    scene.type("2026-11-15 and more text");
    scene.frame();
    CHECK(edit->getWidth() == width);
    scene.key(keys::Return);
    edit->setAllowEmpty(true);
    edit->setFieldValue(std::nullopt);
    scene.frame();
    CHECK(edit->getWidth() == width);
    edit->setFieldValue(dateValue(2026, 11, 15));
    scene.frame();

    // a longer format is wider
    edit->setFormat("DD/MM/YYYY");
    scene.frame();
    CHECK(edit->getWidth() == width);
    edit->setFormat("h:mm AP");
    scene.frame();
    CHECK(edit->getWidth() < width);

    edit->setFormat("YYYY-MM-DD");
    scene.frame();
    scene.caretAt(9);
    scene.frame();
    input::onKeyEvent(scene.canvas, keys::Up, true);
    input::onKeyEvent(scene.canvas, keys::Up, false);
    scene.canvas->updateCanvas();
    CHECK(edit->getText() == "2026-11-16");
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    scene.canvas->renderCanvas();
}
