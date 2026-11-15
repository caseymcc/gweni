/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  controls::TimeEdit on a real Canvas with the real (un-init()'d) textured
 *  skin on a StubRenderer, keyed through input::onKeyEvent and typed through
 *  Canvas::inputCharacter.
 *
 *  Its text in 24 h ("09:05", "00:00") and 12 h ("9:05 AM", midnight "12:00
 *  AM", noon "12:00 PM", "11:59 PM"), setTwelveHour switching keeping the
 *  time, and a date format refused. Up and Down in the hour (23 to 0, 11 AM
 *  to 12 PM on a 12-hour clock), the minute (59 to 0 with no carry into the
 *  hour) and AP sections (switching, 12:30 AM being 00:30), A and P typed in
 *  the AP section; PageUp and PageDown taking twelve hours and ten minutes.
 *  The lenient parse on Return in both clocks ("1430", "2:30 pm", "12 am" is
 *  00:00, "12 pm" 12:00, "0930"), and text that is no time ("25:00",
 *  "2:30 xm", "0:30 am", "") reverting on Return, on the focus leaving and on
 *  Escape. setRange clamping a typed and a stepped time. Empty allowed and
 *  not. The minute step: Up and Down by it to its next multiple, a typed
 *  minute kept. onValueChanged once per real change by the user, never from
 *  setTime, setRange, setTwelveHour or setMinuteStep. A format the parse
 *  cannot read back (the minute before the hour, AP not last) refused.
 */
#include "headlessFixture.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/timeEdit.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <doctest/doctest.h>

using namespace gweni;
using controls::TimeEdit;

namespace
{

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
    TimeEdit *edit=nullptr;
    Changes events;

    Scene()
    {
        platform::setTimeSource([]() { return 10.0; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);
        edit=canvas->newChild<TimeEdit>();
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

    void caretAt(int pos)
    {
        edit->focus();
        edit->setCursorPos(pos);
        edit->setCursorend(pos);
    }

    //! Types \p text over the whole field.
    void type(const char *text)
    {
        edit->focus();
        edit->onSelectAll(edit);
        if(!*text)
            edit->onKeyBackspace(true);
        for(const char *c=text; *c; ++c)
            canvas->inputCharacter(static_cast<UnicodeChar>(*c));
    }

    //! Types \p text and Return; the text then.
    String enter(const char *text)
    {
        type(text);
        key(keys::Return);
        return edit->getText();
    }

    String step(int caret, int key)
    {
        caretAt(caret);
        this->key(key);
        return edit->getText();
    }

    String time() const
    {
        return edit->getTime()?edit->getTime()->toText():String("none");
    }
};

}//namespace

TEST_CASE("TimeEdit: 24-hour and 12-hour text")
{
    Scene scene;
    TimeEdit *edit=scene.edit;

    CHECK(edit->getText() == "00:00");
    CHECK_FALSE(edit->isTwelveHour());
    edit->setTime(TimeOfDay(9, 5));
    CHECK(edit->getText() == "09:05");

    edit->setTwelveHour(true);
    CHECK(edit->isTwelveHour());
    CHECK(edit->getFormat() == "h:mm AP");
    CHECK(edit->getText() == "9:05 AM");
    edit->setTime(TimeOfDay(0, 0));
    CHECK(edit->getText() == "12:00 AM");
    edit->setTime(TimeOfDay(12, 0));
    CHECK(edit->getText() == "12:00 PM");
    edit->setTime(TimeOfDay(23, 59));
    CHECK(edit->getText() == "11:59 PM");

    // back to 24 hours, the time kept
    edit->setTwelveHour(false);
    CHECK(edit->getText() == "23:59");
    CHECK(scene.time() == "23:59");

    // a date's format is refused, an invalid time ignored
    CHECK_FALSE(edit->setFormat("YYYY-MM-DD"));
    CHECK(edit->setFormat("H:mm"));
    CHECK(edit->getText() == "23:59");
    edit->setTime(TimeOfDay(24, 0));
    CHECK(edit->getText() == "23:59");
    CHECK(scene.events.count == 0);
}

TEST_CASE("TimeEdit: Up, Down, PageUp and PageDown in each section")
{
    Scene scene;
    TimeEdit *edit=scene.edit;

    edit->setTime(TimeOfDay(23, 59));
    // "23:59": the hour 23 to 0 and back, the minute 59 to 0 in the same hour
    CHECK(scene.step(1, keys::Up) == "00:59");
    CHECK(scene.step(1, keys::Down) == "23:59");
    CHECK(scene.step(4, keys::Up) == "23:00");
    CHECK(scene.step(4, keys::Down) == "23:59");
    CHECK(scene.events.count == 4);
    // ten minutes, twelve hours
    CHECK(scene.step(4, keys::PageUp) == "23:09");
    CHECK(scene.step(4, keys::PageDown) == "23:59");
    CHECK(scene.step(0, keys::PageUp) == "11:59");
    CHECK(scene.step(0, keys::PageDown) == "23:59");

    edit->setTwelveHour(true);
    edit->setTime(TimeOfDay(11, 30));
    // "11:30 AM": 11 AM an hour on is 12 PM, 12 AM an hour back is 11 PM
    CHECK(scene.step(1, keys::Up) == "12:30 PM");
    CHECK(scene.time() == "12:30");
    edit->setTime(TimeOfDay(0, 30));
    CHECK(scene.step(1, keys::Down) == "11:30 PM");
    CHECK(scene.time() == "23:30");
    // the AP section: Up and Down switch, PageUp too
    CHECK(scene.step(7, keys::Up) == "11:30 AM");
    CHECK(scene.step(7, keys::Down) == "11:30 PM");
    CHECK(scene.step(7, keys::PageUp) == "11:30 AM");
    edit->setTime(TimeOfDay(12, 15));
    CHECK(scene.step(7, keys::Down) == "12:15 AM");
    CHECK(scene.time() == "00:15");
    // A and P typed there
    scene.caretAt(7);
    scene.canvas->inputCharacter('p');
    CHECK(scene.time() == "12:15");
    scene.canvas->inputCharacter('A');
    CHECK(scene.time() == "00:15");
}

TEST_CASE("TimeEdit: the lenient parse, and text that is no time reverting")
{
    Scene scene;
    TimeEdit *edit=scene.edit;

    edit->setTime(TimeOfDay(8, 0));
    CHECK(scene.enter("1430") == "14:30");
    CHECK(scene.enter("2:30 pm") == "14:30");
    CHECK(scene.enter("12 am") == "00:00");
    CHECK(scene.enter("12 pm") == "12:00");
    CHECK(scene.enter("0930") == "09:30");
    // "2:30 pm" was the time already
    CHECK(scene.events.count == 4);

    edit->setTwelveHour(true);
    CHECK(scene.enter("1430") == "2:30 PM");
    CHECK(scene.enter("12:00 am") == "12:00 AM");
    CHECK(scene.time() == "00:00");
    CHECK(scene.enter("12:00 PM") == "12:00 PM");
    CHECK(scene.time() == "12:00");
    CHECK(scene.events.count == 7);

    // no time: reverted on Return, on the focus leaving, on Escape
    CHECK(scene.enter("25:00") == "12:00 PM");
    CHECK(scene.enter("2:30 xm") == "12:00 PM");
    CHECK(scene.enter("0:30 am") == "12:00 PM");
    CHECK(scene.enter("") == "12:00 PM");
    scene.type("99");
    edit->unfocus();
    CHECK(edit->getText() == "12:00 PM");
    scene.type("3:15 pm");
    scene.key(keys::Escape);
    CHECK(edit->getText() == "12:00 PM");
    CHECK(scene.time() == "12:00");
    CHECK(scene.events.count == 7);
    // a time typed and the focus leaving takes it
    scene.type("3:15 pm");
    edit->unfocus();
    CHECK(scene.time() == "15:15");
    CHECK(scene.events.count == 8);
}

TEST_CASE("TimeEdit: the range clamps a typed and a stepped time")
{
    Scene scene;
    TimeEdit *edit=scene.edit;

    edit->setTime(TimeOfDay(12, 0));
    edit->setRange(TimeOfDay(9, 0), TimeOfDay(17, 30));
    CHECK(edit->getMinTime()->toText() == "09:00");
    CHECK(edit->getMaxTime()->toText() == "17:30");
    CHECK(scene.enter("18:00") == "17:30");
    CHECK(scene.enter("7:45") == "09:00");
    // 09:00 an hour back is 08:00, clamped back to the limit: on down to
    // 00:00 and round to 23:00, clamped to the range's end
    CHECK(scene.step(1, keys::Down) == "17:30");
    // twelve hours on is 05:30, clamped to the start
    CHECK(scene.step(1, keys::PageUp) == "09:00");
    CHECK(scene.events.count == 4);

    // the setters clamp, with no event
    edit->setTime(TimeOfDay(3, 0));
    CHECK(edit->getText() == "09:00");
    edit->setRange(TimeOfDay(10, 0), std::nullopt);
    CHECK(edit->getText() == "10:00");
    CHECK(scene.events.count == 4);
}

TEST_CASE("TimeEdit: empty allowed and not")
{
    Scene scene;
    TimeEdit *edit=scene.edit;

    edit->setTime(TimeOfDay(10, 0));
    edit->setTime(std::nullopt);
    CHECK(scene.time() == "10:00");
    CHECK(scene.enter("") == "10:00");
    CHECK(scene.events.count == 0);

    edit->setAllowEmpty(true);
    CHECK(scene.enter("") == "");
    CHECK(scene.time() == "none");
    CHECK(scene.events.count == 1);
    // a step from empty shows 00:00
    CHECK(scene.step(0, keys::Up) == "00:00");
    CHECK(scene.events.count == 2);
    edit->setTime(std::nullopt);
    CHECK(edit->getText() == "");
    CHECK(scene.events.count == 2);
}

TEST_CASE("TimeEdit: the minute step")
{
    Scene scene;
    TimeEdit *edit=scene.edit;

    edit->setMinuteStep(15);
    edit->setTime(TimeOfDay(9, 50));
    CHECK(scene.step(4, keys::Up) == "09:00");
    CHECK(scene.step(4, keys::Up) == "09:15");
    CHECK(scene.step(4, keys::Down) == "09:00");
    CHECK(scene.step(4, keys::Down) == "09:45");
    // a typed minute is kept, and a step goes to the next multiple
    CHECK(scene.enter("9:07") == "09:07");
    CHECK(scene.step(4, keys::Up) == "09:15");
    CHECK(scene.enter("9:07") == "09:07");
    CHECK(scene.step(4, keys::Down) == "09:00");
    edit->setMinuteStep(5);
    CHECK(scene.step(4, keys::Up) == "09:05");
    CHECK(scene.events.count == 9);
    edit->setMinuteStep(7);
    CHECK(edit->getMinuteStep() == 7);
    CHECK(scene.events.count == 9);
}

TEST_CASE("TimeEdit: a format the parse cannot read back is refused")
{
    Scene scene;
    TimeEdit *edit=scene.edit;

    edit->setTime(TimeOfDay(14, 30));
    // the minute before the hour, AP before the time
    CHECK_FALSE(edit->setFormat("mm:HH"));
    CHECK_FALSE(edit->setFormat("AP h:mm"));
    CHECK_FALSE(edit->setFormat("h AP mm"));
    CHECK(edit->getFormat() == "HH:mm");
    CHECK(edit->setFormat("h:mm AP"));
    CHECK(edit->setFormat("h.mm AP"));
    CHECK(edit->getText() == "2.30 PM");
    CHECK(edit->setFormat("H:mm"));
    CHECK(scene.events.count == 0);
}
