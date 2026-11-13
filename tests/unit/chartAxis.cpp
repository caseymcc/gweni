/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  controls::axis, the chart axes' ticks (gweni/controls/chartAxis.h): value
 *  steps at 1, 2 or 5 times a power of ten, an automatic range widened to
 *  whole steps (an empty one included), ticks that do not drift and their
 *  labels' decimals, a formatter in place of them; the time step chosen for
 *  several ranges and axis widths (the smallest whose labels leave the gap,
 *  Year with overlapping labels when none does); calendar day, month and
 *  year ticks at 00:00 in the zone, under UTC, a fixed +05:30 hook and a
 *  Central European one with daylight saving time, across 2024's leap day
 *  and both its DST changes (the hour 02:00 the clock skips left out, the one
 *  it passes twice there once, 6-hour ticks on the local clock, a day of 23
 *  hours), a midnight the clock skips put where its day begins, and the
 *  labels' date and year in place of a time; localUtcOffset following TZ
 *  (natively); category ticks and the stride that thins their labels; and
 *  the civil calendar's round trip.
 */
#include <gweni/controls/chartAxis.h>

#include <doctest/doctest.h>

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

using namespace gweni::controls::axis;

namespace
{

double utc(int64_t year, int month, int day, int hour=0, int minute=0, int second=0)
{
    return static_cast<double>(daysFromCivil(year, month, day))*86400.0+hour*3600.0+minute*60.0+second;
}

int64_t lastSunday(int64_t year, int month)
{
    const int64_t lastDay=(month == 12) ? daysFromCivil(year+1, 1, 1)-1 : daysFromCivil(year, month+1, 1)-1;
    const int64_t weekday=((lastDay+3)%7+7)%7; // Monday 0, Sunday 6

    return lastDay-(weekday+1)%7;
}

//! Central European Time: +1, +2 from the last Sunday of March 01:00 UTC to
//! the last Sunday of October 01:00 UTC.
int centralEurope(double t)
{
    const int64_t year=civilFromDays(static_cast<int64_t>(std::floor(t/86400.0))).year;
    const double start=static_cast<double>(lastSunday(year, 3))*86400.0+3600.0;
    const double end=static_cast<double>(lastSunday(year, 10))*86400.0+3600.0;

    return (t >= start && t < end) ? 7200 : 3600;
}

LabelMeasure sixPerChar()
{
    return [](const gweni::String &label) { return 6.0*static_cast<double>(label.size()); };
}

std::vector<gweni::String> labels(const std::vector<double> &ticks, TimeStep step, const UtcOffset &offset)
{
    std::vector<gweni::String> result;

    for(double t:ticks)
        result.push_back(formatTime(t, step, offset));
    return result;
}

}//namespace

TEST_CASE("Chart axis: value steps are 1, 2 or 5 times a power of ten")
{
    CHECK(niceStep(10.0, 5) == doctest::Approx(2.0));
    CHECK(niceStep(7.0, 5) == doctest::Approx(2.0));
    CHECK(niceStep(26.0, 5) == doctest::Approx(10.0));
    CHECK(niceStep(1.0, 10) == doctest::Approx(0.1));
    CHECK(niceStep(0.03, 6) == doctest::Approx(0.005));
    CHECK(niceStep(4.0e9, 4) == doctest::Approx(1.0e9));
    CHECK(niceStep(0.0, 5) == 1.0);
    CHECK(niceStep(-3.0, 5) == 1.0);

    // widened to whole steps, the step raised until they are few enough
    ValueRange range=niceRange(0.3, 9.7, 5);
    CHECK(range.min == doctest::Approx(0.0));
    CHECK(range.max == doctest::Approx(10.0));
    CHECK(range.step == doctest::Approx(2.0));

    range=niceRange(47.0, -3.0, 5);
    CHECK(range.min == doctest::Approx(-20.0));
    CHECK(range.max == doctest::Approx(60.0));
    CHECK(range.step == doctest::Approx(20.0));

    // an empty span widened by a tenth each way, [0, 1] at 0
    range=niceRange(5.0, 5.0, 5);
    CHECK(range.min == doctest::Approx(4.5));
    CHECK(range.max == doctest::Approx(5.5));
    CHECK(range.step == doctest::Approx(0.5));
    range=niceRange(0.0, 0.0, 5);
    CHECK(range.min == 0.0);
    CHECK(range.max == doctest::Approx(1.0));
    CHECK(range.step == doctest::Approx(0.2));
}

TEST_CASE("Chart axis: value ticks are whole multiples of the step, labelled with its decimals")
{
    const std::vector<double> ticks=valueTicks(0.0, 1.0, 0.1);

    REQUIRE(ticks.size() == 11);
    for(size_t i=0; i < ticks.size(); ++i)
        CHECK(ticks[i] == static_cast<double>(i)*0.1);
    CHECK(formatValue(ticks[3], 0.1) == "0.3");
    CHECK(formatValue(ticks[10], 0.1) == "1.0");

    CHECK(valueTicks(-25.0, 25.0, 10.0) == std::vector<double>{-20.0, -10.0, 0.0, 10.0, 20.0});
    CHECK(valueTicks(1.0, 0.0, 0.1).empty());
    CHECK(valueTicks(0.0, 1.0, 0.0).empty());

    CHECK(formatValue(1500.0, 500.0) == "1500");
    CHECK(formatValue(2.5, 0.5) == "2.5");
    CHECK(formatValue(0.05, 0.05) == "0.05");
    CHECK(formatValue(-0.0001, 0.1) == "0.0");
    CHECK(formatValue(-0.4, 0.2) == "-0.4");
    // a step that is no 1, 2 or 5 times a power of ten (an app's fixed one)
    CHECK(formatValue(0.75, 0.25) == "0.75");
    CHECK(formatValue(7.5, 2.5) == "7.5");

    const std::vector<Tick> placed=valueAxisTicks(0.0, 200.0, 50.0, 100.0);
    REQUIRE(placed.size() == 5);
    CHECK(placed[1].position == doctest::Approx(25.0));
    CHECK(placed[4].label == "200");

    const std::vector<Tick> formatted=valueAxisTicks(0.0, 200.0, 100.0, 100.0,
        [](double value) { return std::to_string(static_cast<int>(value))+" ms"; });
    REQUIRE(formatted.size() == 3);
    CHECK(formatted[2].label == "200 ms");
}

TEST_CASE("Chart axis: the time step is the smallest whose labels fit the width")
{
    const LabelMeasure measure=sixPerChar();
    const double hourStart=utc(2026, 11, 14, 10);
    const double hourEnd=utc(2026, 11, 14, 11);

    CHECK(chooseTimeTicks(hourStart, hourEnd, 800.0, measure, 8.0).step == TimeStep::Minute5);
    CHECK(chooseTimeTicks(hourStart, hourEnd, 200.0, measure, 8.0).step == TimeStep::Minute15);
    CHECK(chooseTimeTicks(hourStart, hourEnd, 100.0, measure, 8.0).step == TimeStep::Hour1);

    const TimeTicks minute=chooseTimeTicks(hourStart, hourStart+60.0, 800.0, measure, 8.0);
    CHECK(minute.step == TimeStep::Second5);
    REQUIRE(minute.ticks.size() == 13);
    CHECK(minute.ticks[1].label == "10:00:05");
    CHECK(minute.ticks[1].position == doctest::Approx(800.0/12.0));

    const TimeTicks month=chooseTimeTicks(utc(2026, 11, 1), utc(2026, 12, 1), 800.0, measure, 8.0);
    CHECK(month.step == TimeStep::Week1);
    REQUIRE(!month.ticks.empty());
    CHECK(month.ticks[0].label == "Nov 2"); // the first Monday

    const double threeYearsStart=utc(2023, 3, 10);
    const double threeYearsEnd=utc(2026, 3, 10);

    CHECK(chooseTimeTicks(threeYearsStart, threeYearsEnd, 800.0, measure, 8.0).step == TimeStep::Year1);
    CHECK(!chooseTimeTicks(threeYearsStart, threeYearsEnd, 800.0, measure, 8.0).overlapping);
    CHECK(chooseTimeTicks(threeYearsStart, threeYearsEnd, 2000.0, measure, 8.0).step == TimeStep::Month1);

    // nothing fits: Year, its labels thinned
    const TimeTicks centuries=chooseTimeTicks(utc(1900, 6, 1), utc(2100, 6, 1), 100.0, measure, 8.0);
    CHECK(centuries.step == TimeStep::Year1);
    CHECK(centuries.overlapping);
    CHECK(centuries.ticks.size() == 200);

    std::vector<double> positions;
    std::vector<double> widths;

    for(const Tick &tick:centuries.ticks)
    {
        positions.push_back(tick.position);
        widths.push_back(measure(tick.label));
    }

    const size_t stride=thinLabels(positions, widths, 8.0);
    CHECK(stride > 1);
    CHECK(labelsFit(positions, widths, 8.0, stride));
    CHECK(!labelsFit(positions, widths, 8.0, stride-1));
}

TEST_CASE("Chart axis: month and year ticks are 00:00 on the 1st in the zone, across a leap year")
{
    // UTC: February 2024 has 29 days
    const std::vector<double> months=timeTicks(utc(2024, 1, 15), utc(2024, 4, 15), TimeStep::Month1);
    CHECK(months == std::vector<double>{utc(2024, 2, 1), utc(2024, 3, 1), utc(2024, 4, 1)});
    CHECK(months[1]-months[0] == 29.0*86400.0);
    CHECK(labels(months, TimeStep::Month1, UtcOffset()) == std::vector<gweni::String>{"Feb", "Mar", "Apr"});

    // a fixed +05:30: local midnight is 18:30 UTC the day before
    const UtcOffset india=fixedUtcOffset(5*3600+1800);
    const std::vector<double> indiaMonths=timeTicks(utc(2024, 1, 15), utc(2024, 3, 15), TimeStep::Month1, india);
    CHECK(indiaMonths == std::vector<double>{utc(2024, 1, 31, 18, 30), utc(2024, 2, 29, 18, 30)});
    CHECK(labels(indiaMonths, TimeStep::Month1, india) == std::vector<gweni::String>{"Feb", "Mar"});

    // Central Europe: March 1st is CET, April and May 1st CEST
    const UtcOffset cet=centralEurope;
    const std::vector<double> cetMonths=timeTicks(utc(2024, 2, 15), utc(2024, 5, 15), TimeStep::Month1, cet);
    CHECK(cetMonths == std::vector<double>{utc(2024, 2, 29, 23), utc(2024, 3, 31, 22), utc(2024, 4, 30, 22)});
    CHECK(labels(cetMonths, TimeStep::Month1, cet) == std::vector<gweni::String>{"Mar", "Apr", "May"});
    CHECK(cetMonths[1]-cetMonths[0] == 31.0*86400.0-3600.0);

    // years: 2024 is 366 days
    const std::vector<double> years=timeTicks(utc(2023, 6, 1), utc(2025, 6, 1), TimeStep::Year1, cet);
    CHECK(years == std::vector<double>{utc(2023, 12, 31, 23), utc(2024, 12, 31, 23)});
    CHECK(years[1]-years[0] == 366.0*86400.0);
    CHECK(labels(years, TimeStep::Year1, cet) == std::vector<gweni::String>{"2024", "2025"});

    // a month tick on January 1st is labelled with the year
    const std::vector<double> newYear=timeTicks(utc(2024, 11, 15), utc(2025, 2, 15), TimeStep::Month1, cet);
    CHECK(labels(newYear, TimeStep::Month1, cet) == std::vector<gweni::String>{"Dec", "2025", "Feb"});
}

TEST_CASE("Chart axis: ticks across the DST changes follow the local clock")
{
    const UtcOffset cet=centralEurope;

    // spring forward, 2024-03-31 01:00 UTC: 02:00 CET becomes 03:00 CEST
    const std::vector<double> days=timeTicks(utc(2024, 3, 30, 12), utc(2024, 4, 1, 12), TimeStep::Day1, cet);
    CHECK(days == std::vector<double>{utc(2024, 3, 30, 23), utc(2024, 3, 31, 22)});
    CHECK(days[1]-days[0] == 23.0*3600.0);
    CHECK(labels(days, TimeStep::Day1, cet) == std::vector<gweni::String>{"Mar 31", "Apr 1"});

    const std::vector<double> springHours=timeTicks(utc(2024, 3, 30, 23), utc(2024, 3, 31, 3), TimeStep::Hour1, cet);
    CHECK(springHours == std::vector<double>{utc(2024, 3, 30, 23), utc(2024, 3, 31, 0), utc(2024, 3, 31, 1),
                                             utc(2024, 3, 31, 2), utc(2024, 3, 31, 3)});
    CHECK(labels(springHours, TimeStep::Hour1, cet)
          == std::vector<gweni::String>{"Mar 31", "01:00", "03:00", "04:00", "05:00"});

    const std::vector<double> sixHours=timeTicks(utc(2024, 3, 30, 23), utc(2024, 3, 31, 22), TimeStep::Hour6, cet);
    CHECK(labels(sixHours, TimeStep::Hour6, cet) == std::vector<gweni::String>{"Mar 31", "06:00", "12:00", "18:00", "Apr 1"});
    CHECK(sixHours[1] == utc(2024, 3, 31, 4));

    // fall back, 2024-10-27 01:00 UTC: 03:00 CEST becomes 02:00 CET, and the
    // 02:00 it passes twice is a tick once
    const std::vector<double> fallHours=timeTicks(utc(2024, 10, 26, 22), utc(2024, 10, 27, 4), TimeStep::Hour1, cet);
    CHECK(fallHours == std::vector<double>{utc(2024, 10, 26, 22), utc(2024, 10, 26, 23), utc(2024, 10, 27, 0),
                                           utc(2024, 10, 27, 2), utc(2024, 10, 27, 3), utc(2024, 10, 27, 4)});
    CHECK(labels(fallHours, TimeStep::Hour1, cet)
          == std::vector<gweni::String>{"Oct 27", "01:00", "02:00", "03:00", "04:00", "05:00"});

    const std::vector<double> fallDays=timeTicks(utc(2024, 10, 26, 12), utc(2024, 10, 28, 12), TimeStep::Day1, cet);
    CHECK(fallDays == std::vector<double>{utc(2024, 10, 26, 22), utc(2024, 10, 27, 23)});

    // a zone whose clock skips midnight (00:00 -> 01:00 on 2024-03-31): that
    // day's tick is where the day begins, the jump, and is labelled with it
    const double jump=utc(2024, 3, 31);
    const UtcOffset midnightJump=[jump](double t) { return t >= jump ? 3600 : 0; };
    const std::vector<double> jumpDays=timeTicks(utc(2024, 3, 30, 12), utc(2024, 4, 1, 12), TimeStep::Day1, midnightJump);
    CHECK(jumpDays == std::vector<double>{utc(2024, 3, 31), utc(2024, 3, 31, 23)});
    CHECK(labels(jumpDays, TimeStep::Day1, midnightJump) == std::vector<gweni::String>{"Mar 31", "Apr 1"});

    // the minutes under it are on the local clock too
    const std::vector<double> quarter=timeTicks(utc(2024, 7, 1, 9, 50), utc(2024, 7, 1, 10, 20), TimeStep::Minute15, cet);
    CHECK(labels(quarter, TimeStep::Minute15, cet) == std::vector<gweni::String>{"12:00", "12:15"});
    CHECK(quarter[0] == utc(2024, 7, 1, 10));
}

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
TEST_CASE("Chart axis: the default hook is the process's zone, following TZ")
{
    const char *previous=std::getenv("TZ");
    const std::string saved=previous ? previous : "";

    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();

    const UtcOffset local=localUtcOffset();

    for(double t:{utc(2024, 1, 10), utc(2024, 3, 31, 0, 59, 59), utc(2024, 3, 31, 1), utc(2024, 7, 1),
                  utc(2024, 10, 27, 0, 59, 59), utc(2024, 10, 27, 1), utc(2024, 12, 31, 23, 30)})
        CHECK(local(t) == centralEurope(t));
    CHECK(timeTicks(utc(2024, 2, 15), utc(2024, 5, 15), TimeStep::Month1, local)
          == timeTicks(utc(2024, 2, 15), utc(2024, 5, 15), TimeStep::Month1, centralEurope));

    setenv("TZ", "UTC0", 1);
    tzset();
    CHECK(localUtcOffset()(utc(2024, 7, 1)) == 0);

    if(previous)
        setenv("TZ", saved.c_str(), 1);
    else
        unsetenv("TZ");
    tzset();
}
#endif

TEST_CASE("Chart axis: category labels are thinned to a stride that fits")
{
    const std::vector<Tick> ticks=categoryTicks(10, 100.0);

    REQUIRE(ticks.size() == 10);
    CHECK(ticks[0].position == doctest::Approx(5.0));
    CHECK(ticks[9].position == doctest::Approx(95.0));
    CHECK(ticks[9].value == 9.0);

    std::vector<double> positions;

    for(const Tick &tick:ticks)
        positions.push_back(tick.position);

    // spacing 10: 8 wide with a gap of 2 fits as they are
    CHECK(thinLabels(positions, std::vector<double>(10, 8.0), 2.0) == 1);
    // 20 wide: every 2nd touches, every 3rd leaves 10
    CHECK(thinLabels(positions, std::vector<double>(10, 20.0), 2.0) == 3);
    // a wide first label (always shown) makes the stride larger
    std::vector<double> widths(10, 8.0);
    widths[0]=40.0;
    CHECK(thinLabels(positions, widths, 2.0) == 3);
    // only one fits
    CHECK(thinLabels(positions, std::vector<double>(10, 1000.0), 2.0) == 10);
    CHECK(thinLabels({}, {}, 2.0) == 1);
    CHECK(thinLabels({50.0}, {1000.0}, 2.0) == 1);
}

TEST_CASE("Chart axis: the civil calendar round trip")
{
    CHECK(daysFromCivil(1970, 1, 1) == 0);
    CHECK(daysFromCivil(1969, 12, 31) == -1);
    CHECK(daysFromCivil(2000, 3, 1) == 11017);
    CHECK(daysFromCivil(2024, 3, 1)-daysFromCivil(2024, 2, 28) == 2);
    CHECK(daysFromCivil(2100, 3, 1)-daysFromCivil(2100, 2, 28) == 1);

    for(int64_t day=-800000; day <= 800000; day+=997)
    {
        const CivilDate date=civilFromDays(day);

        REQUIRE(daysFromCivil(date.year, date.month, date.day) == day);
    }
}
