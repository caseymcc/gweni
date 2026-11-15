/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  controls::DateEdit on a real Canvas with the real (un-init()'d) textured
 *  skin on a StubRenderer, keyed through input::onKeyEvent and typed through
 *  Canvas::inputCharacter.
 *
 *  Its text in ISO ("2026-11-01", the default), D/M/Y ("01/11/2026") and
 *  M/D/Y ("11/1/2026") formats, and a time's format refused. Up and Down in
 *  the day (31 to 1 and back in a 31-day month, 30 to 1 in a 30-day one, 29
 *  to 1 in a leap February), the month (December to January in the same
 *  year, the 31st a month on clamped to November 30 and to February 28 or
 *  29) and the year sections (February 29 a year on is the 28th), the caret
 *  kept in its section; PageUp and PageDown taking a year in the day and
 *  month sections and ten in the year's. The lenient parse on Return
 *  ("2026-11-1", "11/1/2026" for M/D/YYYY, "1/11/2026" for DD/MM/YYYY,
 *  "20261101"), and text that is no date ("2026-02-30", "Nov 1", "") reverting
 *  on Return, on the focus leaving and on Escape. setRange clamping a typed
 *  and a stepped date. A weekend filter refusing a typed Saturday and a
 *  stepped day passing over the weekend (Friday to Monday, and back);
 *  isDateEnabled following the range and the filter. Empty allowed and not,
 *  a step from empty showing setToday's date. onValueChanged once per real
 *  change by the user, never from setDate, setRange, setFormat or the filter.
 *  The calendar button inside the field's padding, at its height; disabled
 *  with the field and while it is read-only, when nothing opens the calendar.
 *  A filtered month or year step going on from the day it began with (Jan 30
 *  to Mar 30, not Mar 28), the day section's PageUp too (Feb 29 on to the
 *  next Feb 29 when the 28th is refused), a range end that is no date
 *  refused, a step from empty searching on by day in the year section too,
 *  years held to 1..9999 (set, stepped and typed, a five-digit year
 *  reverting), and a new renderer text layout measuring the format's width
 *  again.
 */
#include "headlessFixture.h"

#include <gweni/controls/calendar.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/dateEdit.h>
#include <gweni/controls/popup.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <doctest/doctest.h>

using namespace gweni;
using controls::DateEdit;

namespace
{

//! Counts onValueChanged.
struct Changes: public event::Handler
{
    int count=0;

    void changed(event::Info) { ++count; }
};

//! \p date in ISO, "none" for no date.
String iso(const std::optional<Date> &date)
{
    return date?date->toIso():String("none");
}

bool weekday(const Date &date)
{
    return (date.weekday() != 0) && (date.weekday() != 6);
}

struct Scene
{
    test::StubRenderer renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    DateEdit *edit=nullptr;
    Changes events;
    double clock=10.0;

    Scene()
    {
        // a second apart per press, so two presses are never a double click
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);
        edit=canvas->newChild<DateEdit>();
        edit->setPos(20, 20);
        edit->onValueChanged.add(&events, &Changes::changed);
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

    void altDown()
    {
        input::onKeyEvent(canvas, keys::Alt, true);
        key(keys::Down);
        input::onKeyEvent(canvas, keys::Alt, false);
        frame();
    }

    void clickOn(controls::Base *control)
    {
        const Point at=control->localPosToCanvas();

        clock+=1.0;
        canvas->inputMouseMoved(at.x+control->getWidth()/2, at.y+control->getHeight()/2, 0, 0);
        input::onMouseClicked(canvas, 0, true);
        input::onMouseClicked(canvas, 0, false);
        frame();
    }

    //! The calendar's cell showing \p date, nullptr if none does.
    controls::internal::CalendarCell *cellFor(const Date &date)
    {
        controls::Calendar *calendar=edit->getCalendar();

        for(int i=0; i < controls::Calendar::CellCount; ++i)
        {
            controls::internal::CalendarCell *cell=calendar->getCell(i);

            if(!cell->hidden() && (cell->getDate() == date))
                return cell;
        }
        return nullptr;
    }

    //! Every control under the canvas, counted.
    static size_t countControls(controls::Base *control)
    {
        size_t count=1;

        for(controls::Base *child : control->getChildren())
            count+=countControls(child);
        return count;
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

    String date() const
    {
        return edit->getDate()?edit->getDate()->toIso():String("none");
    }
};

}//namespace

TEST_CASE("DateEdit: each format's text")
{
    Scene scene;
    DateEdit *edit=scene.edit;

    CHECK(edit->getFormat() == "YYYY-MM-DD");
    CHECK(edit->getText() == "1970-01-01");
    edit->setDate(Date(2026, 11, 1));
    CHECK(edit->getText() == "2026-11-01");
    CHECK(edit->setFormat("DD/MM/YYYY"));
    CHECK(edit->getText() == "01/11/2026");
    CHECK(edit->setFormat("M/D/YYYY"));
    CHECK(edit->getText() == "11/1/2026");
    CHECK(edit->setFormat("D.M.YYYY"));
    CHECK(edit->getText() == "1.11.2026");

    // a time's format is refused, an invalid date ignored
    CHECK_FALSE(edit->setFormat("HH:mm"));
    CHECK(edit->getFormat() == "D.M.YYYY");
    edit->setDate(Date(2026, 2, 30));
    CHECK(scene.date() == "2026-11-01");
    CHECK(scene.events.count == 0);
}

TEST_CASE("DateEdit: Up and Down in each section, wrapping, and the 31st in a 30-day month")
{
    Scene scene;
    DateEdit *edit=scene.edit;

    edit->setDate(Date(2026, 10, 31));
    // the day: 31 to 1 in October, and back
    CHECK(scene.step(9, keys::Up) == "2026-10-01");
    CHECK(scene.step(9, keys::Down) == "2026-10-31");
    CHECK(edit->getCursorPos() == 9);
    // the month: the 31st into November is the 30th
    CHECK(scene.step(6, keys::Up) == "2026-11-30");
    CHECK(edit->getCursorPos() == 6);
    // and the 30th wraps to the 1st in November
    CHECK(scene.step(9, keys::Up) == "2026-11-01");
    CHECK(scene.step(9, keys::Down) == "2026-11-30");
    // December to January in the same year
    edit->setDate(Date(2026, 12, 15));
    CHECK(scene.step(6, keys::Up) == "2026-01-15");
    CHECK(scene.step(6, keys::Down) == "2026-12-15");
    // the year, a leap day a year on the 28th
    CHECK(scene.step(2, keys::Up) == "2027-12-15");
    edit->setDate(Date(2024, 2, 29));
    CHECK(scene.step(9, keys::Up) == "2024-02-01");
    CHECK(scene.step(9, keys::Down) == "2024-02-29");
    CHECK(scene.step(2, keys::Up) == "2025-02-28");
    edit->setDate(Date(2026, 1, 31));
    CHECK(scene.step(6, keys::Up) == "2026-02-28");
    CHECK(scene.events.count == 12);

    // the sections follow the format: D/M/Y's day is first
    edit->setFormat("DD/MM/YYYY");
    edit->setDate(Date(2026, 4, 30));
    CHECK(scene.step(1, keys::Up) == "01/04/2026");
    CHECK(scene.step(4, keys::Up) == "01/05/2026");
    CHECK(scene.step(8, keys::Down) == "01/05/2025");
}

TEST_CASE("DateEdit: PageUp and PageDown")
{
    Scene scene;
    DateEdit *edit=scene.edit;

    edit->setDate(Date(2026, 11, 15));
    CHECK(scene.step(9, keys::PageUp) == "2027-11-15");
    CHECK(scene.step(9, keys::PageDown) == "2026-11-15");
    CHECK(scene.step(6, keys::PageUp) == "2027-11-15");
    CHECK(scene.step(6, keys::PageDown) == "2026-11-15");
    CHECK(scene.step(2, keys::PageUp) == "2036-11-15");
    CHECK(scene.step(2, keys::PageDown) == "2026-11-15");
    CHECK(scene.events.count == 6);
}

TEST_CASE("DateEdit: the lenient parse, and text that is no date reverting")
{
    Scene scene;
    DateEdit *edit=scene.edit;

    edit->setDate(Date(2026, 1, 1));
    CHECK(scene.enter("2026-11-1") == "2026-11-01");
    CHECK(scene.enter("20261102") == "2026-11-02");
    CHECK(scene.enter("2026/11/3") == "2026-11-03");
    edit->setFormat("M/D/YYYY");
    CHECK(scene.enter("11/1/2026") == "11/1/2026");
    CHECK(scene.date() == "2026-11-01");
    edit->setFormat("DD/MM/YYYY");
    CHECK(scene.enter("2/11/2026") == "02/11/2026");
    CHECK(scene.date() == "2026-11-02");
    CHECK(scene.events.count == 5);

    // no date: reverted on Return, on the focus leaving, on Escape
    CHECK(scene.enter("30/02/2026") == "02/11/2026");
    CHECK(scene.enter("Nov 1") == "02/11/2026");
    CHECK(scene.enter("") == "02/11/2026");
    scene.type("31/04/2026");
    edit->unfocus();
    CHECK(edit->getText() == "02/11/2026");
    scene.type("24/12/2026");
    scene.key(keys::Escape);
    CHECK(edit->getText() == "02/11/2026");
    CHECK(scene.events.count == 5);
    // a date typed and the focus leaving takes it
    scene.type("24/12/2026");
    edit->unfocus();
    CHECK(scene.date() == "2026-12-24");
    CHECK(scene.events.count == 6);
}

TEST_CASE("DateEdit: the range clamps a typed and a stepped date")
{
    Scene scene;
    DateEdit *edit=scene.edit;

    edit->setDate(Date(2026, 11, 15));
    edit->setRange(Date(2026, 11, 5), Date(2026, 12, 18));
    CHECK(edit->getMinDate()->toIso() == "2026-11-05");
    CHECK(edit->getMaxDate()->toIso() == "2026-12-18");
    CHECK(scene.enter("2027-01-01") == "2026-12-18");
    CHECK(scene.enter("2026-01-01") == "2026-11-05");
    CHECK(scene.step(2, keys::Up) == "2026-12-18");
    CHECK(scene.step(6, keys::Down) == "2026-11-18");
    CHECK(scene.events.count == 4);
    CHECK(edit->isDateEnabled(Date(2026, 11, 5)));
    CHECK_FALSE(edit->isDateEnabled(Date(2026, 11, 4)));
    CHECK_FALSE(edit->isDateEnabled(Date(2026, 12, 19)));

    // the setters clamp, with no event
    edit->setDate(Date(2020, 1, 1));
    CHECK(scene.date() == "2026-11-05");
    edit->setRange(Date(2026, 11, 20), std::nullopt);
    CHECK(scene.date() == "2026-11-20");
    CHECK(scene.events.count == 4);
}

TEST_CASE("DateEdit: the filter refuses a typed date and is passed over by a step")
{
    Scene scene;
    DateEdit *edit=scene.edit;

    // 2026-11-13 is a Friday
    edit->setDate(Date(2026, 11, 13));
    edit->setDateFilter(weekday);
    CHECK(edit->isDateEnabled(Date(2026, 11, 13)));
    CHECK_FALSE(edit->isDateEnabled(Date(2026, 11, 14)));

    // a Saturday typed reverts; a Monday is taken
    CHECK(scene.enter("2026-11-14") == "2026-11-13");
    CHECK(scene.events.count == 0);
    CHECK(scene.enter("2026-11-16") == "2026-11-16");
    CHECK(scene.events.count == 1);
    // the day back from Monday is Friday, and on again Monday
    CHECK(scene.step(9, keys::Down) == "2026-11-13");
    CHECK(scene.step(9, keys::Up) == "2026-11-16");
    CHECK(scene.events.count == 3);

    // the setter does not ask it; no filter takes everything
    edit->setDate(Date(2026, 11, 15));
    CHECK(scene.date() == "2026-11-15");
    edit->setDateFilter(nullptr);
    CHECK(scene.enter("2026-11-14") == "2026-11-14");
    CHECK(scene.events.count == 4);
}

TEST_CASE("DateEdit: empty allowed and not")
{
    Scene scene;
    DateEdit *edit=scene.edit;

    edit->setDate(Date(2026, 11, 15));
    edit->setDate(std::nullopt);
    CHECK(scene.date() == "2026-11-15");
    CHECK(scene.enter("") == "2026-11-15");
    CHECK(scene.events.count == 0);

    edit->setAllowEmpty(true);
    CHECK(edit->allowsEmpty());
    CHECK(scene.enter("") == "");
    CHECK(scene.date() == "none");
    CHECK(scene.events.count == 1);
    // a step from empty shows today
    edit->setToday(Date(2026, 11, 15));
    CHECK(edit->getToday().toIso() == "2026-11-15");
    CHECK(scene.step(0, keys::Up) == "2026-11-15");
    CHECK(scene.events.count == 2);
    edit->setDate(std::nullopt);
    CHECK(edit->getText() == "");
    // not allowed any more: today
    edit->setAllowEmpty(false);
    CHECK(scene.date() == "2026-11-15");
    CHECK(scene.events.count == 2);
}

TEST_CASE("DateEdit: the calendar opens in a popup made once, with the field's date, limits and filter")
{
    Scene scene;
    DateEdit *edit=scene.edit;

    edit->setDate(Date(2026, 11, 13));
    edit->setRange(Date(2026, 11, 5), Date(2026, 12, 18));
    edit->setDateFilter(weekday);
    edit->setToday(Date(2026, 11, 10));
    scene.frame();
    CHECK(edit->getPopup() == nullptr);
    CHECK_FALSE(edit->isCalendarOpen());

    // Alt+Down from the focused field
    scene.caretAt(3);
    scene.altDown();
    REQUIRE(edit->isCalendarOpen());
    controls::Popup *popup=edit->getPopup();
    controls::Calendar *calendar=edit->getCalendar();
    REQUIRE(popup != nullptr);
    REQUIRE(calendar != nullptr);
    CHECK(calendar->getSelectionMode() == controls::Calendar::SelectionMode::Single);
    CHECK(calendar->getSelectedDate()->toIso() == "2026-11-13");
    CHECK(calendar->getCursor().toIso() == "2026-11-13");
    CHECK(calendar->getDisplayedMonth() == 11);
    CHECK(calendar->getMinDate()->toIso() == "2026-11-05");
    CHECK(calendar->getMaxDate()->toIso() == "2026-12-18");
    CHECK(calendar->getToday()->toIso() == "2026-11-10");
    CHECK_FALSE(calendar->isDateEnabled(Date(2026, 11, 14)));
    CHECK_FALSE(calendar->isDateEnabled(Date(2026, 11, 4)));
    CHECK(KeyboardFocus == calendar);
    // below the field
    CHECK(popup->localPosToCanvas().y >= edit->localPosToCanvas().y+edit->getHeight());

    // Escape: closed, nothing changed, the focus back
    scene.key(keys::Escape);
    CHECK_FALSE(edit->isCalendarOpen());
    CHECK(KeyboardFocus == edit);
    CHECK(scene.date() == "2026-11-13");
    CHECK(scene.events.count == 0);

    // F4 opens it again with no new control, on the date the field has now
    const size_t controls=Scene::countControls(scene.canvas);
    edit->setDate(Date(2026, 12, 1));
    scene.key(keys::F4);
    REQUIRE(edit->isCalendarOpen());
    CHECK(edit->getPopup() == popup);
    CHECK(edit->getCalendar() == calendar);
    CHECK(Scene::countControls(scene.canvas) == controls);
    CHECK(calendar->getSelectedDate()->toIso() == "2026-12-01");
    CHECK(calendar->getDisplayedMonth() == 12);
    scene.key(keys::Escape);

    // Down without Alt steps the section
    scene.caretAt(9);
    scene.key(keys::Down);
    CHECK_FALSE(edit->isCalendarOpen());
    CHECK(scene.date() == "2026-12-18");
}

TEST_CASE("DateEdit: a day picked sets the field, closes the calendar and gives the focus back")
{
    Scene scene;
    DateEdit *edit=scene.edit;

    edit->setDate(Date(2026, 11, 13));
    scene.frame();

    // the button opens it, the field taking the focus from the press
    scene.clickOn(edit->getButton());
    REQUIRE(edit->isCalendarOpen());
    CHECK(KeyboardFocus == edit->getCalendar());
    scene.frame();

    controls::internal::CalendarCell *cell=scene.cellFor(Date(2026, 11, 20));
    REQUIRE(cell != nullptr);
    scene.clickOn(cell);
    CHECK_FALSE(edit->isCalendarOpen());
    CHECK(scene.date() == "2026-11-20");
    CHECK(edit->getText() == "2026-11-20");
    CHECK(KeyboardFocus == edit);
    CHECK(scene.events.count == 1);

    // the day already selected closes it too, with no event; Return picks
    // the cursor's day
    scene.key(keys::F4);
    REQUIRE(edit->isCalendarOpen());
    scene.frame();
    scene.clickOn(scene.cellFor(Date(2026, 11, 20)));
    CHECK_FALSE(edit->isCalendarOpen());
    CHECK(scene.events.count == 1);
    scene.key(keys::F4);
    scene.key(keys::Right);
    scene.key(keys::Return);
    CHECK_FALSE(edit->isCalendarOpen());
    CHECK(scene.date() == "2026-11-21");
    CHECK(scene.events.count == 2);

    // a second click on the button closes it with nothing chosen
    scene.clickOn(edit->getButton());
    REQUIRE(edit->isCalendarOpen());
    scene.frame();
    scene.clickOn(edit->getButton());
    CHECK_FALSE(edit->isCalendarOpen());
    CHECK(scene.date() == "2026-11-21");
    CHECK(scene.events.count == 2);
}

TEST_CASE("DateEdit: typed text is read before the calendar opens, and an empty field opens on today")
{
    Scene scene;
    DateEdit *edit=scene.edit;

    edit->setDate(Date(2026, 11, 13));
    scene.type("2026-11-25");
    scene.key(keys::F4);
    REQUIRE(edit->isCalendarOpen());
    CHECK(scene.date() == "2026-11-25");
    CHECK(edit->getCalendar()->getSelectedDate()->toIso() == "2026-11-25");
    CHECK(scene.events.count == 1);
    scene.key(keys::Escape);

    edit->setAllowEmpty(true);
    edit->setToday(Date(2027, 3, 4));
    edit->setDate(std::nullopt);
    scene.caretAt(0);
    scene.key(keys::F4);
    REQUIRE(edit->isCalendarOpen());
    CHECK_FALSE(edit->getCalendar()->getSelectedDate().has_value());
    CHECK(edit->getCalendar()->getCursor().toIso() == "2027-03-04");
    scene.key(keys::Return);
    CHECK(scene.date() == "2027-03-04");
    CHECK(scene.events.count == 2);

    // disabled, nothing opens
    edit->setDisabled(true);
    scene.key(keys::F4);
    CHECK_FALSE(edit->isCalendarOpen());
}

TEST_CASE("DateEdit: the button is in the field's width")
{
    Scene scene;
    DateEdit *edit=scene.edit;

    scene.frame();
    controls::internal::DateEditButton *button=edit->getButton();
    CHECK(button->getWidth() == DateEdit::ButtonWidth);
    CHECK((button->getDock() == DockPosition::Right));
    // right of the text, inside the field
    CHECK(button->getBounds().x > edit->getWidth()/2);
    CHECK(button->getBounds().x+button->getWidth() <= edit->getWidth());
    // the field's height inside its padding, not Button's own
    const Padding &padding=edit->getPadding();
    CHECK(button->getBounds().y == padding.top);
    CHECK(button->getBounds().y+button->getHeight() == edit->getFieldHeight()-padding.bottom);
    CHECK_FALSE(button->isTabable());
}

TEST_CASE("DateEdit: disabled, the button is disabled and nothing opens the calendar")
{
    Scene scene;
    DateEdit *edit=scene.edit;
    controls::internal::DateEditButton *button=edit->getButton();

    edit->setDate(Date(2026, 11, 13));
    scene.caretAt(3);
    scene.key(keys::F4);
    REQUIRE(edit->isCalendarOpen());

    // disabling closes it and disables the button with the field
    edit->setDisabled(true);
    scene.frame();
    CHECK_FALSE(edit->isCalendarOpen());
    CHECK(button->isDisabled());
    scene.clickOn(button);
    CHECK_FALSE(edit->isCalendarOpen());
    edit->openCalendar();
    CHECK_FALSE(edit->isCalendarOpen());

    edit->setDisabled(false);
    scene.frame();
    CHECK_FALSE(button->isDisabled());
    scene.clickOn(button);
    CHECK(edit->isCalendarOpen());
    scene.key(keys::Escape);
    CHECK(scene.date() == "2026-11-13");
}

TEST_CASE("DateEdit: read-only, the calendar does not open and the date stays")
{
    Scene scene;
    DateEdit *edit=scene.edit;
    controls::internal::DateEditButton *button=edit->getButton();

    edit->setDate(Date(2026, 11, 13));
    edit->setEditable(false);
    scene.frame();
    CHECK(button->isDisabled());
    scene.caretAt(3);
    scene.key(keys::F4);
    CHECK_FALSE(edit->isCalendarOpen());
    scene.altDown();
    CHECK_FALSE(edit->isCalendarOpen());
    scene.clickOn(button);
    CHECK_FALSE(edit->isCalendarOpen());
    CHECK(scene.date() == "2026-11-13");
    CHECK(scene.events.count == 0);

    edit->setEditable(true);
    scene.frame();
    CHECK_FALSE(button->isDisabled());
    scene.caretAt(3);
    scene.key(keys::F4);
    CHECK(edit->isCalendarOpen());
}

TEST_CASE("DateEdit: a filtered month step goes on from the day it began with")
{
    Scene scene;
    DateEdit *edit=scene.edit;

    // 2026-01-30 is a Friday; a month on is Feb 28 (a Saturday, refused),
    // then March 30 (a Monday), not March 28 (a Saturday too)
    edit->setDate(Date(2026, 1, 30));
    edit->setDateFilter(weekday);
    CHECK(scene.step(5, keys::Up) == "2026-03-30");
    CHECK(scene.events.count == 1);
    // and a year step: 2027-01-30 is a Saturday, 2028-01-30 a Sunday,
    // 2029-01-30 a Tuesday
    edit->setDate(Date(2026, 1, 30));
    CHECK(scene.step(2, keys::Up) == "2029-01-30");
}

TEST_CASE("DateEdit: the day section's PageUp is a year step from the day it began with")
{
    Scene scene;
    DateEdit *edit=scene.edit;

    // 2024-02-29 a year on is February 28, which the filter refuses, and so
    // in 2026 and 2027; the next a year step takes is 2028-02-29
    edit->setDateFilter([](const Date &date) { return !((date.month == 2) && (date.day == 28)); });
    edit->setDate(Date(2024, 2, 29));
    CHECK(scene.step(9, keys::PageUp) == "2028-02-29");
    CHECK(scene.events.count == 1);
    // as the year section's Up does
    edit->setDate(Date(2024, 2, 29));
    CHECK(scene.step(2, keys::Up) == "2028-02-29");
}

TEST_CASE("DateEdit: an invalid range end is refused")
{
    Scene scene;
    DateEdit *edit=scene.edit;

    edit->setDate(Date(2026, 11, 15));
    edit->setRange(Date(2026, 11, 5), Date(2026, 12, 18));
    // a month past December (an app's Date(y, m+1, 1) in December) is no
    // date: the call is refused, the range and the date as they were
    edit->setRange(Date(2026, 13, 1), std::nullopt);
    CHECK(iso(edit->getMinDate()) == "2026-11-05");
    CHECK(iso(edit->getMaxDate()) == "2026-12-18");
    edit->setRange(std::nullopt, Date(2026, 2, 30));
    CHECK(iso(edit->getMinDate()) == "2026-11-05");
    CHECK(iso(edit->getMaxDate()) == "2026-12-18");
    REQUIRE(edit->getText() == "2026-11-15");
    // and the day steps as before
    CHECK(scene.step(9, keys::Up) == "2026-11-16");
    CHECK(scene.events.count == 1);
}

TEST_CASE("DateEdit: a step from empty searches on by day")
{
    Scene scene;
    DateEdit *edit=scene.edit;

    edit->setAllowEmpty(true);
    edit->setDate(std::nullopt);
    edit->setDateFilter(weekday);
    // 2026-11-14 is a Saturday: the step shows the Monday after it, whichever
    // section the caret is in (an ISO field's first is the year)
    edit->setToday(Date(2026, 11, 14));
    CHECK(scene.step(0, keys::Up) == "2026-11-16");
    CHECK(scene.events.count == 1);
}

TEST_CASE("DateEdit: a year is 1..9999")
{
    Scene scene;
    DateEdit *edit=scene.edit;

    edit->setDate(Date(-1, 6, 1));
    CHECK(edit->getText() == "0001-01-01");
    edit->setDate(Date(10000, 1, 1));
    CHECK(edit->getText() == "9999-12-31");
    // past the last year a step changes nothing
    CHECK(scene.step(2, keys::Up) == "9999-12-31");
    // a five-digit year is no date: it reverts, not clamped to 9999-12-31
    edit->setDate(Date(2026, 1, 1));
    CHECK(scene.enter("20261-01-01") == "2026-01-01");
    edit->setDate(Date(1, 1, 1));
    CHECK(scene.step(2, keys::Down) == "0001-01-01");
    CHECK(scene.events.count == 0);
}

TEST_CASE("DateEdit: a new text layout measures the format's width again")
{
    Scene scene;
    DateEdit *edit=scene.edit;

    // the same font and format: the cached width, no measure of the format
    edit->invalidate();
    size_t before=scene.renderer.measures;
    scene.frame();
    const size_t cached=scene.renderer.measures-before;

    edit->invalidate();
    scene.renderer.changeTextLayout();
    before=scene.renderer.measures;
    scene.frame();
    // the format's ten samples and AM/PM's two measured again
    CHECK(scene.renderer.measures-before >= cached+10);
}
