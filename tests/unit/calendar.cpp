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
};

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
    CHECK(calendar->showNextMonth());
    CHECK_FALSE(calendar->canShowNextMonth());
    scene.frame();
    CHECK(calendar->getNextButton()->isDisabled());
    scene.click(calendar->getNextButton());
    CHECK(calendar->getDisplayedMonth() == 12);
    CHECK_FALSE(calendar->showNextMonth());

    CHECK(calendar->showPreviousMonth());
    CHECK(calendar->showPreviousMonth());
    CHECK(calendar->getDisplayedMonth() == 10);
    CHECK_FALSE(calendar->canShowPreviousMonth());
    scene.frame();
    CHECK(calendar->getPreviousButton()->isDisabled());
    CHECK_FALSE(calendar->getNextButton()->isDisabled());
    CHECK_FALSE(calendar->showPreviousMonth());
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
