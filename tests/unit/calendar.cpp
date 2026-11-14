/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  controls::Calendar on a real Canvas with the real (un-init()'d) textured
 *  skin on a StubRenderer, clicked through input::onMouseClicked and
 *  Canvas::inputMouseMoved.
 *
 *  The month view: for a month starting on each weekday and each first day of
 *  the week, the grid's first cell is the first day of the week on or before
 *  the month's first day, so the month starts in the first row, its days
 *  follow in order and the rest of the 42 cells are the days either side,
 *  dimmed (inMonth false); the weekday row starts at the first day of the week
 *  (Monday by default) with the app's names; the title is "November 2026",
 *  from the app's month names. Showing another month (setDisplayedMonth, the
 *  header's buttons) makes and deletes no control, keeps the live primitive
 *  and text ids, and marks only the cells whose number or look changed; the
 *  calendar's size is the same for every month. The limits and the filter
 *  disable dates: a click on one selects nothing, and the header's buttons
 *  stop at the limits. Single selection: a click selects the date, firing
 *  onSelectionChanged once, a click on it again fires nothing, a click on a
 *  day of the next month selects it and shows that month, and
 *  setSelectedDate() and every other set*() fire nothing. None mode takes no
 *  selection. Today is ringed (the look's today flag) where the app says.
 *
 *  The keyboard, through input::onKeyEvent, once a click has given the
 *  calendar the focus: the cursor (ringed only while the calendar has the
 *  focus) moves a day with Left/Right and a week with Up/Down, to the month's
 *  first and last day with Home/End, a month with PageUp/PageDown and a year
 *  with Shift, crossing month and year ends (showing the month it lands in,
 *  the day clamped to a shorter month); Space and Return select at it. With
 *  limits and a weekend filter it passes over the disabled dates in its
 *  direction; an arrow that would leave the limits does nothing, and Home,
 *  End and a page past a limit stop at the nearest enabled date inside it, so
 *  PageDown at the limit does nothing.
 *
 *  Range: the first click sets the start (drawn as both ends), the hover then
 *  previews the range to the hovered date, the second click sets the end,
 *  swapped when earlier, and a third click starts a new range; each click
 *  fires onSelectionChanged once, setRange() nothing (swapping its ends too).
 *  Space selects at the cursor, which the preview follows.
 *
 *  The views: a click on the title shows the year's 12 months (the weekday
 *  row and the other 30 cells hidden, the calendar's size kept), a second the
 *  decade's 12 years (the years either side dimmed, the title "2020 – 2029");
 *  a click on a year shows its months, one on a month its days, the cells
 *  back where they were. In those views the arrows move over months and
 *  years, PageDown a year, Space zooms in, and months and years outside the
 *  limits are disabled.
 */
#include "headlessFixture.h"

#include <gweni/controls/calendar.h>
#include <gweni/controls/label.h>
#include <gweni/controls/scrollBarButton.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <vector>

using namespace gweni;
using controls::Calendar;
using controls::internal::CalendarCell;

namespace
{

size_t livePrimitives()
{
    return primitiveIdCount()-releasedPrimitiveIdCount();
}

size_t liveTexts()
{
    return textIdCount()-releasedTextIdCount();
}

//! The cell or its number marked for the next render.
bool marked(controls::Base *cell)
{
    if(cell->getStateChange() != controls::StateChange_Nothing)
        return true;
    for(controls::Base *child : cell->getChildren())
    {
        if(child->getStateChange() != controls::StateChange_Nothing)
            return true;
    }
    return false;
}

size_t subtreeSize(controls::Base *control)
{
    size_t count=1;

    for(controls::Base *child : control->getChildren())
        count+=subtreeSize(child);
    return count;
}

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
        static float start=0.0f;

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
        input::onKeyEvent(canvas, keys::Shift, false);
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

    Calendar *calendar(int year, int month)
    {
        Calendar *calendar=canvas->newChild<Calendar>();

        calendar->setPos(20, 20);
        calendar->setDisplayedMonth(year, month);
        frame();
        return calendar;
    }

    void click(controls::Base *control)
    {
        const Point at=control->localPosToCanvas();

        clock+=1.0f;
        canvas->inputMouseMoved(at.x+control->getWidth()/2, at.y+control->getHeight()/2, 0, 0);
        input::onMouseClicked(canvas, 0, true);
        input::onMouseClicked(canvas, 0, false);
        frame();
    }

    void hover(controls::Base *control)
    {
        const Point at=control->localPosToCanvas();

        canvas->inputMouseMoved(at.x+control->getWidth()/2, at.y+control->getHeight()/2, 0, 0);
        frame();
    }

    void key(int key)
    {
        input::onKeyEvent(canvas, key, true);
        input::onKeyEvent(canvas, key, false);
        frame();
    }

    void shiftKey(int key)
    {
        input::onKeyEvent(canvas, keys::Shift, true);
        this->key(key);
        input::onKeyEvent(canvas, keys::Shift, false);
    }
};

//! The cell showing \p date in the month view.
CalendarCell *cellOf(Calendar *calendar, const Date &date)
{
    return calendar->getCell(calendar->cellIndex(date));
}

//! The cells whose look has the cursor's ring.
int cursorCells(Calendar *calendar)
{
    int count=0;

    for(int i=0; i < Calendar::CellCount; ++i)
        count+=calendar->getCell(i)->getLook().cursor?1:0;
    return count;
}

//! Counts onSelectionChanged and onDisplayedMonthChanged.
struct Events: public event::Handler
{
    int selections=0;
    int months=0;

    void attach(Calendar *calendar)
    {
        calendar->onSelectionChanged.add(this, &Events::selection);
        calendar->onDisplayedMonthChanged.add(this, &Events::month);
    }

    void selection(event::Info) { ++selections; }
    void month(event::Info) { ++months; }
};

}//namespace

TEST_CASE("Calendar: the grid of a month starting on each weekday, with each first day of the week")
{
    Scene scene;
    Calendar *calendar=scene.calendar(2026, 11);

    CHECK(calendar->getFirstDayOfWeek() == 1);
    CHECK(calendar->getWeekdayLabel(0)->getText() == "Mo");
    CHECK(calendar->getWeekdayLabel(6)->getText() == "Su");
    CHECK(calendar->getTitle()->getText() == "November 2026");

    // 2026's months start on every weekday between them: Jan Thu, Feb Sun,
    // Mar Sun, Apr Wed, May Fri, Jun Mon, Jul Wed, Aug Sat, Sep Tue, Oct Thu,
    // Nov Sun, Dec Tue
    std::vector<bool> starts(7, false);

    for(int month=1; month <= 12; ++month)
        starts[static_cast<size_t>(Date(2026, month, 1).weekday())]=true;
    CHECK((std::find(starts.begin(), starts.end(), false) == starts.end()));

    for(int first=0; first < 7; ++first)
    {
        calendar->setFirstDayOfWeek(first);
        for(int column=0; column < 7; ++column)
            REQUIRE(calendar->getWeekdayLabel(column)->getText() == calendar->getWeekdayName((first+column)%7));

        for(int month=1; month <= 12; ++month)
        {
            CAPTURE(first);
            CAPTURE(month);
            calendar->setDisplayedMonth(2026, month);
            scene.frame();

            const Date start(2026, month, 1);
            const int lead=(start.weekday()-first+7)%7;

            for(int i=0; i < Calendar::CellCount; ++i)
            {
                CalendarCell *cell=calendar->getCell(i);
                const Date expected=start.addDays(i-lead);

                REQUIRE(cell->getDate().toIso() == expected.toIso());
                REQUIRE(cell->getText() == std::to_string(expected.day));
                REQUIRE(cell->getLook().inMonth == (expected.month == month));
                REQUIRE((cell->getDate().weekday() == (first+i%7)%7));
                REQUIRE(calendar->cellIndex(expected) == i);
            }
            // the month starts in the first row
            REQUIRE(lead < 7);
            REQUIRE((calendar->getCell(lead)->getDate() == start));
        }
    }
    CHECK((calendar->cellIndex(calendar->cellDate(0).addDays(-1)) == -1));
    CHECK((calendar->cellIndex(calendar->cellDate(41).addDays(1)) == -1));
}

TEST_CASE("Calendar: the names are the app's")
{
    Scene scene;
    Calendar *calendar=scene.calendar(2026, 3);

    calendar->setMonthNames({"Jan", "Feb", "Mär", "Apr", "Mai", "Jun", "Jul", "Aug", "Sep", "Okt", "Nov", "Dez"});
    calendar->setWeekdayNames({"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"});
    scene.frame();
    CHECK(calendar->getTitle()->getText() == "Mär 2026");
    CHECK(calendar->getWeekdayLabel(1)->getText() == "Di");

    // a list of the wrong length is ignored
    calendar->setMonthNames({"x"});
    calendar->setWeekdayNames({"a", "b"});
    CHECK(calendar->getTitle()->getText() == "Mär 2026");
    CHECK(calendar->getWeekdayLabel(0)->getText() == "Mo");
}

TEST_CASE("Calendar: a month change makes and deletes no control and keeps its size and ids")
{
    Scene scene;
    Calendar *calendar=scene.calendar(2026, 11);
    Events events;

    events.attach(calendar);

    const size_t controls=subtreeSize(scene.canvas);
    const size_t primitives=livePrimitives();
    const size_t texts=liveTexts();
    const Rect bounds=calendar->getBounds();
    std::vector<CalendarCell *> cells;

    for(int i=0; i < Calendar::CellCount; ++i)
        cells.push_back(calendar->getCell(i));

    for(int step=0; step < 30; ++step)
    {
        scene.click(calendar->getNextButton());
        REQUIRE(subtreeSize(scene.canvas) == controls);
        REQUIRE(livePrimitives() == primitives);
        REQUIRE(liveTexts() == texts);
        REQUIRE((calendar->getBounds() == bounds));
        for(int i=0; i < Calendar::CellCount; ++i)
            REQUIRE(calendar->getCell(i) == cells[static_cast<size_t>(i)]);
    }
    CHECK(events.months == 30);
    CHECK(calendar->getDisplayedYear() == 2029);
    CHECK(calendar->getDisplayedMonth() == 5);

    scene.click(calendar->getPreviousButton());
    CHECK(calendar->getDisplayedMonth() == 4);
    CHECK(events.months == 31);
    CHECK(events.selections == 0);

    // February and March 2026 both start on a Sunday: days 1..28 keep their
    // cells, numbers and looks, so only the cells after them are marked
    calendar->setDisplayedMonth(2026, 2);
    scene.frame();
    calendar->setDisplayedMonth(2026, 3);

    const int lead=calendar->cellIndex(Date(2026, 3, 1));

    for(int i=0; i < Calendar::CellCount; ++i)
    {
        CAPTURE(i);
        const bool kept=(i >= lead) && (i < lead+28);

        REQUIRE(marked(calendar->getCell(i)) == !kept);
    }
}

TEST_CASE("Calendar: a click selects a date once, a set fires nothing")
{
    Scene scene;
    Calendar *calendar=scene.calendar(2026, 11);
    Events events;

    events.attach(calendar);
    CHECK(calendar->getSelectionMode() == Calendar::SelectionMode::Single);
    CHECK_FALSE(calendar->getSelectedDate().has_value());

    const int index=calendar->cellIndex(Date(2026, 11, 11));

    scene.click(calendar->getCell(index));
    CHECK((calendar->getSelectedDate() == Date(2026, 11, 11)));
    CHECK(calendar->getCell(index)->getLook().selected);
    CHECK(events.selections == 1);

    // the same date again changes nothing
    scene.click(calendar->getCell(index));
    CHECK(events.selections == 1);

    // another date moves the selection
    const int other=calendar->cellIndex(Date(2026, 11, 20));

    scene.click(calendar->getCell(other));
    CHECK((calendar->getSelectedDate() == Date(2026, 11, 20)));
    CHECK_FALSE(calendar->getCell(index)->getLook().selected);
    CHECK(calendar->getCell(other)->getLook().selected);
    CHECK(events.selections == 2);

    // set*() fires nothing
    calendar->setSelectedDate(Date(2026, 11, 3));
    calendar->setSelectedDate(std::nullopt);
    calendar->setSelectedDate(Date(2026, 11, 4));
    calendar->setToday(Date(2026, 11, 5));
    calendar->setDateLimits(Date(2020, 1, 1), Date(2030, 1, 1));
    calendar->setDisplayedMonth(2026, 12);
    calendar->setDisplayedMonth(2026, 11);
    scene.frame();
    CHECK(events.selections == 2);
    CHECK((calendar->getSelectedDate() == Date(2026, 11, 4)));
    CHECK(calendar->getCell(calendar->cellIndex(Date(2026, 11, 4)))->getLook().selected);
    CHECK(calendar->getCell(calendar->cellIndex(Date(2026, 11, 5)))->getLook().today);

    // a day of the next month in the last rows: selected, its month shown
    const int trailing=calendar->cellIndex(Date(2026, 12, 2));

    REQUIRE(trailing > 0);
    scene.click(calendar->getCell(trailing));
    CHECK((calendar->getSelectedDate() == Date(2026, 12, 2)));
    CHECK(calendar->getDisplayedMonth() == 12);
    CHECK(events.selections == 3);
    CHECK(calendar->getCell(calendar->cellIndex(Date(2026, 12, 2)))->getLook().selected);
}

TEST_CASE("Calendar: None mode takes no selection")
{
    Scene scene;
    Calendar *calendar=scene.calendar(2026, 11);
    Events events;

    events.attach(calendar);
    calendar->setSelectionMode(Calendar::SelectionMode::None);
    scene.click(calendar->getCell(calendar->cellIndex(Date(2026, 11, 11))));
    CHECK_FALSE(calendar->getSelectedDate().has_value());
    CHECK(events.selections == 0);
}

TEST_CASE("Calendar: the limits and the filter refuse clicks, and the header stops at the limits")
{
    Scene scene;
    Calendar *calendar=scene.calendar(2026, 11);
    Events events;

    events.attach(calendar);
    calendar->setDateLimits(Date(2026, 10, 28), Date(2026, 12, 3));
    // weekends refused
    calendar->setDateFilter([](const Date &date) { return (date.weekday() != 0) && (date.weekday() != 6); });
    scene.frame();

    CHECK(calendar->isDateEnabled(Date(2026, 11, 11)));
    CHECK_FALSE(calendar->isDateEnabled(Date(2026, 11, 14))); // Saturday
    CHECK_FALSE(calendar->isDateEnabled(Date(2026, 10, 27)));
    CHECK_FALSE(calendar->isDateEnabled(Date(2026, 12, 4)));

    CalendarCell *saturday=calendar->getCell(calendar->cellIndex(Date(2026, 11, 14)));

    CHECK(saturday->getLook().disabled);
    CHECK(saturday->isDisabled());
    scene.click(saturday);
    CHECK_FALSE(calendar->getSelectedDate().has_value());

    scene.click(calendar->getCell(calendar->cellIndex(Date(2026, 11, 13))));
    CHECK((calendar->getSelectedDate() == Date(2026, 11, 13)));
    CHECK(events.selections == 1);

    // December 4 (a Friday) is past the maximum: in November's grid, refused
    CalendarCell *late=calendar->getCell(calendar->cellIndex(Date(2026, 12, 4)));

    REQUIRE(late);
    scene.click(late);
    CHECK((calendar->getSelectedDate() == Date(2026, 11, 13)));
    CHECK(calendar->getDisplayedMonth() == 11);
    CHECK(events.selections == 1);

    // the header's buttons go as far as the limits' months
    CHECK(calendar->showNext());
    CHECK_FALSE(calendar->canShowNext());
    scene.frame();
    CHECK(calendar->getNextButton()->isDisabled());
    scene.click(calendar->getNextButton());
    CHECK(calendar->getDisplayedMonth() == 12);
    CHECK_FALSE(calendar->showNext());

    CHECK(calendar->showPrevious());
    CHECK(calendar->showPrevious());
    CHECK(calendar->getDisplayedMonth() == 10);
    CHECK_FALSE(calendar->canShowPrevious());
    scene.frame();
    CHECK(calendar->getPreviousButton()->isDisabled());
    CHECK_FALSE(calendar->getNextButton()->isDisabled());
    CHECK_FALSE(calendar->showPrevious());
    CHECK(calendar->getDisplayedMonth() == 10);
}

TEST_CASE("Calendar: today is the app's")
{
    Scene scene;
    Calendar *calendar=scene.calendar(2026, 11);

    for(int i=0; i < Calendar::CellCount; ++i)
        REQUIRE_FALSE(calendar->getCell(i)->getLook().today);

    calendar->setToday(Date(2026, 11, 14));
    scene.frame();
    for(int i=0; i < Calendar::CellCount; ++i)
        REQUIRE((calendar->getCell(i)->getLook().today == (calendar->getCell(i)->getDate() == Date(2026, 11, 14))));
}

TEST_CASE("Calendar: the keys move the cursor across month and year ends")
{
    Scene scene;
    Calendar *calendar=scene.calendar(2026, 11);
    Events events;

    events.attach(calendar);
    CHECK(cursorCells(calendar) == 0);

    scene.click(cellOf(calendar, Date(2026, 11, 11)));
    REQUIRE(calendar->isFocussed());
    CHECK((calendar->getCursor() == Date(2026, 11, 11)));
    CHECK(cursorCells(calendar) == 1);
    CHECK(cellOf(calendar, Date(2026, 11, 11))->getLook().cursor);
    CHECK(events.selections == 1);

    struct Step
    {
        int key;
        bool shift;
        Date cursor;
        int month;
    };
    const Step steps[]={
        {keys::Left, false, Date(2026, 11, 10), 11},
        {keys::Right, false, Date(2026, 11, 11), 11},
        {keys::Up, false, Date(2026, 11, 4), 11},
        {keys::Down, false, Date(2026, 11, 11), 11},
        {keys::Home, false, Date(2026, 11, 1), 11},
        {keys::End, false, Date(2026, 11, 30), 11},
        {keys::Right, false, Date(2026, 12, 1), 12},
        {keys::Left, false, Date(2026, 11, 30), 11},
        {keys::Up, false, Date(2026, 11, 23), 11},
        {keys::PageDown, false, Date(2026, 12, 23), 12},
        {keys::PageUp, false, Date(2026, 11, 23), 11},
        {keys::PageDown, true, Date(2027, 11, 23), 11},
        {keys::PageUp, true, Date(2026, 11, 23), 11},
        {keys::Down, false, Date(2026, 11, 30), 11},
        {keys::Down, false, Date(2026, 12, 7), 12},
        {keys::End, false, Date(2026, 12, 31), 12},
        {keys::Right, false, Date(2027, 1, 1), 1},
        {keys::Up, false, Date(2026, 12, 25), 12},
        {keys::Down, false, Date(2027, 1, 1), 1},
        {keys::End, false, Date(2027, 1, 31), 1},
        {keys::PageDown, false, Date(2027, 2, 28), 2},
        {keys::PageUp, false, Date(2027, 1, 28), 1},
        {keys::Home, false, Date(2027, 1, 1), 1},
        {keys::Left, false, Date(2026, 12, 31), 12},
    };

    for(const Step &step : steps)
    {
        CAPTURE(step.key);
        CAPTURE(step.cursor.toIso());
        if(step.shift)
            scene.shiftKey(step.key);
        else
            scene.key(step.key);
        REQUIRE(calendar->getCursor().toIso() == step.cursor.toIso());
        REQUIRE(calendar->getDisplayedMonth() == step.month);
        REQUIRE(calendar->getDisplayedYear() == step.cursor.year);
        REQUIRE(cursorCells(calendar) == 1);
        REQUIRE(cellOf(calendar, step.cursor)->getLook().cursor);
    }
    // moving selects nothing
    CHECK(events.selections == 1);
    CHECK((calendar->getSelectedDate() == Date(2026, 11, 11)));

    scene.key(keys::Space);
    CHECK((calendar->getSelectedDate() == Date(2026, 12, 31)));
    CHECK(events.selections == 2);
    scene.key(keys::Return);
    CHECK(events.selections == 2);
    scene.key(keys::Left);
    scene.key(keys::Return);
    CHECK((calendar->getSelectedDate() == Date(2026, 12, 30)));
    CHECK(events.selections == 3);

    // the ring goes with the focus
    controls::Base *other=scene.canvas->newChild<controls::Label>();

    other->setKeyboardInputEnabled(true);
    other->focus();
    scene.frame();
    CHECK(cursorCells(calendar) == 0);
    calendar->focus();
    scene.frame();
    CHECK(cursorCells(calendar) == 1);
}

TEST_CASE("Calendar: the keys pass over disabled dates and stop at the limits")
{
    Scene scene;
    Calendar *calendar=scene.calendar(2026, 11);

    calendar->setDateLimits(Date(2026, 11, 5), Date(2026, 11, 25));
    calendar->setDateFilter([](const Date &date) { return (date.weekday() != 0) && (date.weekday() != 6); });
    scene.click(cellOf(calendar, Date(2026, 11, 13)));
    REQUIRE((calendar->getCursor() == Date(2026, 11, 13)));

    struct Step
    {
        int key;
        Date cursor;
    };
    const Step steps[]={
        {keys::Right, Date(2026, 11, 16)},    // Friday to Monday, over the weekend
        {keys::Left, Date(2026, 11, 13)},     // and back
        {keys::Down, Date(2026, 11, 20)},
        {keys::Down, Date(2026, 11, 20)},     // the 27th is past the maximum
        {keys::PageDown, Date(2026, 11, 25)}, // December is: the page stops at the maximum
        {keys::PageDown, Date(2026, 11, 25)}, // and at the limit does nothing
        {keys::Left, Date(2026, 11, 24)},
        {keys::End, Date(2026, 11, 25)},      // the last enabled day
        {keys::Right, Date(2026, 11, 25)},    // the 26th is past the maximum
        {keys::Home, Date(2026, 11, 5)},      // the first enabled day
        {keys::Left, Date(2026, 11, 5)},      // the 4th is before the minimum
        {keys::Up, Date(2026, 11, 5)},
        {keys::PageUp, Date(2026, 11, 5)},    // at the minimum
        {keys::Right, Date(2026, 11, 6)},
        {keys::PageUp, Date(2026, 11, 5)},    // October is before it: the page stops there
        {keys::Right, Date(2026, 11, 6)},
        {keys::Left, Date(2026, 11, 5)},
        {keys::Right, Date(2026, 11, 6)},
        {keys::Right, Date(2026, 11, 9)},
    };

    for(const Step &step : steps)
    {
        CAPTURE(step.key);
        CAPTURE(step.cursor.toIso());
        scene.key(step.key);
        REQUIRE(calendar->getCursor().toIso() == step.cursor.toIso());
        REQUIRE(calendar->getDisplayedMonth() == 11);
    }

    // a Saturday given by the app is moved off by the next key
    calendar->setCursor(Date(2026, 11, 14));
    scene.key(keys::Right);
    CHECK((calendar->getCursor() == Date(2026, 11, 16)));

    // Space on the cursor selects it; nothing a key reaches is disabled
    scene.key(keys::Space);
    CHECK((calendar->getSelectedDate() == Date(2026, 11, 16)));
}

TEST_CASE("Calendar: a range takes a start, a previewed end and swaps an earlier one")
{
    Scene scene;
    Calendar *calendar=scene.calendar(2026, 11);
    Events events;

    events.attach(calendar);
    calendar->setSelectionMode(Calendar::SelectionMode::Range);
    scene.frame();

    scene.click(cellOf(calendar, Date(2026, 11, 20)));
    CHECK((calendar->getRangeStart() == Date(2026, 11, 20)));
    CHECK_FALSE(calendar->getRangeEnd().has_value());
    CHECK(events.selections == 1);
    {
        const auto &look=cellOf(calendar, Date(2026, 11, 20))->getLook();

        CHECK(look.rangeStart);
        CHECK(look.rangeEnd);
        CHECK_FALSE(look.inRange);
    }

    // the hover previews the range, either side of the start
    scene.hover(cellOf(calendar, Date(2026, 11, 25)));
    for(int day=1; day <= 30; ++day)
    {
        CAPTURE(day);
        REQUIRE(cellOf(calendar, Date(2026, 11, day))->getLook().inRange == ((day >= 20) && (day <= 25)));
    }
    CHECK(cellOf(calendar, Date(2026, 11, 25))->getLook().rangeEnd);
    scene.hover(cellOf(calendar, Date(2026, 11, 17)));
    CHECK(cellOf(calendar, Date(2026, 11, 17))->getLook().rangeStart);
    CHECK(cellOf(calendar, Date(2026, 11, 20))->getLook().rangeEnd);
    CHECK_FALSE(cellOf(calendar, Date(2026, 11, 25))->getLook().inRange);
    CHECK(events.selections == 1);

    // the second click is the end, swapped when earlier
    scene.click(cellOf(calendar, Date(2026, 11, 10)));
    CHECK((calendar->getRangeStart() == Date(2026, 11, 10)));
    CHECK((calendar->getRangeEnd() == Date(2026, 11, 20)));
    CHECK(events.selections == 2);
    for(int day=1; day <= 30; ++day)
    {
        CAPTURE(day);
        const auto &look=cellOf(calendar, Date(2026, 11, day))->getLook();

        REQUIRE(look.inRange == ((day >= 10) && (day <= 20)));
        REQUIRE(look.rangeStart == (day == 10));
        REQUIRE(look.rangeEnd == (day == 20));
        REQUIRE_FALSE(look.selected);
    }
    // a whole range: the hover previews nothing
    scene.hover(cellOf(calendar, Date(2026, 11, 28)));
    CHECK_FALSE(cellOf(calendar, Date(2026, 11, 25))->getLook().inRange);

    // a third click starts a new range
    scene.click(cellOf(calendar, Date(2026, 11, 3)));
    CHECK((calendar->getRangeStart() == Date(2026, 11, 3)));
    CHECK_FALSE(calendar->getRangeEnd().has_value());
    CHECK(events.selections == 3);
    CHECK_FALSE(cellOf(calendar, Date(2026, 11, 15))->getLook().inRange);

    // the keys: the preview follows the cursor, Space sets the end
    scene.key(keys::Right);
    scene.key(keys::Right);
    CHECK(cellOf(calendar, Date(2026, 11, 4))->getLook().inRange);
    CHECK(cellOf(calendar, Date(2026, 11, 5))->getLook().rangeEnd);
    scene.key(keys::Space);
    CHECK((calendar->getRangeEnd() == Date(2026, 11, 5)));
    CHECK(events.selections == 4);

    // a one-day range
    scene.click(cellOf(calendar, Date(2026, 11, 12)));
    scene.click(cellOf(calendar, Date(2026, 11, 12)));
    CHECK((calendar->getRangeStart() == Date(2026, 11, 12)));
    CHECK((calendar->getRangeEnd() == Date(2026, 11, 12)));
    CHECK(events.selections == 6);

    // setRange fires nothing and swaps its ends
    calendar->setRange(Date(2026, 11, 28), Date(2026, 11, 22));
    CHECK((calendar->getRangeStart() == Date(2026, 11, 22)));
    CHECK((calendar->getRangeEnd() == Date(2026, 11, 28)));
    calendar->setRange(std::nullopt, Date(2026, 11, 28));
    CHECK_FALSE(calendar->getRangeStart().has_value());
    CHECK_FALSE(calendar->getRangeEnd().has_value());
    scene.frame();
    CHECK(events.selections == 6);
    CHECK_FALSE(calendar->getSelectedDate().has_value());
}

TEST_CASE("Calendar: the title zooms out to the year and the decade, a cell zooms back in")
{
    Scene scene;
    Calendar *calendar=scene.calendar(2026, 11);
    Events events;

    events.attach(calendar);

    const Rect bounds=calendar->getBounds();
    std::vector<Rect> cellBounds;

    for(int i=0; i < Calendar::CellCount; ++i)
        cellBounds.push_back(calendar->getCell(i)->getBounds());

    scene.click(calendar->getTitle());
    CHECK(calendar->getView() == Calendar::View::Year);
    CHECK(calendar->getTitle()->getText() == "2026");
    CHECK((calendar->getBounds() == bounds));
    for(int i=0; i < Calendar::CellCount; ++i)
    {
        CAPTURE(i);
        CalendarCell *cell=calendar->getCell(i);

        REQUIRE(cell->hidden() == (i >= Calendar::ZoomCount));
        if(i < Calendar::ZoomCount)
        {
            REQUIRE(cell->getText() == calendar->getShortMonthName(i+1));
            REQUIRE((cell->getDate() == Date(2026, i+1, 1)));
            // a quarter of the grid's width, the units left over spread
            const int quarter=Calendar::Columns*Calendar::CellWidth/Calendar::ZoomColumns;

            REQUIRE(cell->getWidth() >= quarter);
            REQUIRE(cell->getWidth() <= quarter+1);
        }
    }
    for(int column=0; column < Calendar::Columns; ++column)
        REQUIRE(calendar->getWeekdayLabel(column)->hidden());
    CHECK(calendar->getCell(0)->getText() == "Jan");

    scene.click(calendar->getTitle());
    CHECK(calendar->getView() == Calendar::View::Decade);
    CHECK(calendar->getTitle()->getText() == "2020 \xe2\x80\x93 2029");
    CHECK(calendar->getCell(0)->getText() == "2019");
    CHECK_FALSE(calendar->getCell(0)->getLook().inMonth);
    CHECK(calendar->getCell(1)->getLook().inMonth);
    CHECK(calendar->getCell(11)->getText() == "2030");
    CHECK_FALSE(calendar->getCell(11)->getLook().inMonth);
    // a third click does nothing
    scene.click(calendar->getTitle());
    CHECK(calendar->getView() == Calendar::View::Decade);

    scene.click(calendar->getCell(5));
    CHECK(calendar->getView() == Calendar::View::Year);
    CHECK(calendar->getDisplayedYear() == 2024);
    scene.click(calendar->getCell(2));
    CHECK(calendar->getView() == Calendar::View::Month);
    CHECK(calendar->getDisplayedYear() == 2024);
    CHECK(calendar->getDisplayedMonth() == 3);
    CHECK(calendar->getTitle()->getText() == "March 2024");
    // zooming selects nothing
    CHECK(events.selections == 0);
    CHECK((calendar->getBounds() == bounds));
    for(int i=0; i < Calendar::CellCount; ++i)
    {
        CAPTURE(i);
        REQUIRE_FALSE(calendar->getCell(i)->hidden());
        REQUIRE((calendar->getCell(i)->getBounds() == cellBounds[static_cast<size_t>(i)]));
    }
    for(int column=0; column < Calendar::Columns; ++column)
        REQUIRE_FALSE(calendar->getWeekdayLabel(column)->hidden());
}

TEST_CASE("Calendar: the keys and the limits in the year and decade views")
{
    Scene scene;
    Calendar *calendar=scene.calendar(2026, 11);

    calendar->setDateLimits(Date(2025, 3, 15), Date(2027, 8, 1));
    scene.click(cellOf(calendar, Date(2026, 11, 11)));
    scene.click(calendar->getTitle());
    REQUIRE(calendar->getView() == Calendar::View::Year);
    CHECK(calendar->getCell(10)->getLook().cursor);

    scene.key(keys::Right);
    CHECK(calendar->getCursor().month == 12);
    scene.key(keys::Right);
    CHECK(calendar->getCursor().year == 2027);
    CHECK(calendar->getDisplayedYear() == 2027);
    CHECK(calendar->getCell(0)->getLook().cursor);
    // August is the last month with a date inside the limits
    CHECK_FALSE(calendar->getCell(7)->getLook().disabled);
    CHECK(calendar->getCell(8)->getLook().disabled);
    scene.key(keys::Down);
    CHECK(calendar->getCursor().month == 5);
    scene.key(keys::Down);
    CHECK(calendar->getCursor().month == 5);
    scene.key(keys::PageDown);
    CHECK(calendar->getCursor().year == 2027);
    scene.key(keys::PageUp);
    CHECK(calendar->getCursor().year == 2026);
    CHECK(calendar->getDisplayedYear() == 2026);
    scene.key(keys::PageUp);
    CHECK(calendar->getCursor().year == 2025);
    scene.key(keys::PageUp);
    CHECK(calendar->getCursor().year == 2025);
    // a disabled month takes no click
    scene.click(calendar->getCell(0));
    CHECK(calendar->getView() == Calendar::View::Year);

    // the decade: 2024 before the minimum's year is disabled
    scene.click(calendar->getTitle());
    REQUIRE(calendar->getView() == Calendar::View::Decade);
    CHECK(calendar->getCell(5)->getLook().disabled);
    CHECK_FALSE(calendar->getCell(6)->getLook().disabled);
    CHECK(calendar->getCell(6)->getLook().cursor);
    scene.key(keys::Right);
    scene.key(keys::Right);
    CHECK(calendar->getCursor().year == 2027);
    scene.key(keys::Right);
    CHECK(calendar->getCursor().year == 2027);

    // Space zooms in at the cursor, to its month
    scene.key(keys::Space);
    CHECK(calendar->getView() == Calendar::View::Year);
    CHECK(calendar->getDisplayedYear() == 2027);
    scene.key(keys::Space);
    CHECK(calendar->getView() == Calendar::View::Month);
    CHECK(calendar->getDisplayedYear() == 2027);
    CHECK(calendar->getDisplayedMonth() == calendar->getCursor().month);
    CHECK(calendar->isDateEnabled(calendar->getCursor()));
}

TEST_CASE("Calendar: new limits or a filter put the cursor on an enabled date of the month shown")
{
    Scene scene;
    Calendar *calendar=scene.calendar(2026, 11);

    // CalendarDemo's limited calendar: the limits set after the month, the
    // cursor on the 1st before them
    calendar->setDateLimits(Date(2026, 11, 5), Date(2026, 12, 18));
    CHECK(calendar->getCursor().toIso() == "2026-11-05");
    calendar->focus();
    scene.frame();
    CHECK(cursorCells(calendar) == 1);
    CHECK(cellOf(calendar, Date(2026, 11, 5))->getLook().cursor);
    scene.key(keys::Right);
    CHECK(calendar->getCursor().toIso() == "2026-11-06");

    // a filter refusing the cursor's day moves it on to the next enabled one
    calendar->setCursor(Date(2026, 11, 14));
    calendar->setDateFilter([](const Date &date) { return (date.weekday() != 0) && (date.weekday() != 6); });
    CHECK(calendar->getCursor().toIso() == "2026-11-16");
    // past the month's last enabled day it goes back, never into the next month
    calendar->setCursor(Date(2026, 11, 28));
    calendar->setDateFilter([](const Date &date) { return date.day < 20; });
    CHECK(calendar->getCursor().toIso() == "2026-11-19");
    CHECK(calendar->getDisplayedMonth() == 11);
    scene.key(keys::Left);
    CHECK(calendar->getCursor().toIso() == "2026-11-18");
}

TEST_CASE("Calendar: zooming in puts the cursor on an enabled month and day of what is shown")
{
    Scene scene;
    Calendar *calendar=scene.calendar(2026, 11);

    calendar->setDateLimits(Date(2026, 11, 5), Date(2026, 12, 18));
    calendar->setCursor(Date(2026, 11, 25));
    scene.click(calendar->getTitle());
    REQUIRE(calendar->getView() == Calendar::View::Year);
    // December: the 25th is past the maximum, so the cursor is its last enabled day
    scene.click(calendar->getCell(11));
    REQUIRE(calendar->getView() == Calendar::View::Month);
    REQUIRE(calendar->getDisplayedMonth() == 12);
    CHECK(calendar->getCursor().toIso() == "2026-12-18");

    // a month whose days from the cursor's on are refused: back inside it,
    // not on into the next month
    calendar->setDateLimits(std::nullopt, std::nullopt);
    calendar->setDateFilter([](const Date &date) { return (date.month != 2) || (date.day < 20); });
    calendar->setDisplayedMonth(2027, 1);
    calendar->setCursor(Date(2027, 1, 31));
    scene.click(calendar->getTitle());
    REQUIRE(calendar->getView() == Calendar::View::Year);
    scene.click(calendar->getCell(1));
    REQUIRE(calendar->getView() == Calendar::View::Month);
    REQUIRE(calendar->getDisplayedMonth() == 2);
    CHECK(calendar->getCursor().toIso() == "2027-02-19");

    // the decade's 2027 in limits ending in August: the cursor's November is
    // past them, so it is on August, the last enabled month
    calendar->setDateFilter(nullptr);
    calendar->setDateLimits(Date(2025, 3, 15), Date(2027, 8, 1));
    calendar->setDisplayedMonth(2026, 11);
    calendar->setCursor(Date(2026, 11, 11));
    scene.click(calendar->getTitle());
    scene.click(calendar->getTitle());
    REQUIRE(calendar->getView() == Calendar::View::Decade);
    scene.click(calendar->getCell(2027-calendar->decadeFirstYear()));
    REQUIRE(calendar->getView() == Calendar::View::Year);
    REQUIRE(calendar->getDisplayedYear() == 2027);
    CHECK(calendar->getCursor().year == 2027);
    CHECK(calendar->getCursor().month == 8);
    CHECK_FALSE(calendar->getCell(calendar->getCursor().month-1)->getLook().disabled);
}
