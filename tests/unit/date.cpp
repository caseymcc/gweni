/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  gweni::Date (gweni/date.h), the plain proleptic-Gregorian date the
 *  Calendar works on: leap years (every 4th, not every 100th, every 400th),
 *  the days in each month, validity, the weekday (0 Sunday) of known dates on
 *  both sides of 1970 and of year 0, addDays across month and year ends both
 *  ways, addMonths/addYears clamping the day to the new month's last (Jan 31 +
 *  1 month is Feb 28 or 29, Feb 29 + 1 year Feb 28) with negative steps
 *  crossing years, the day number (days since 1970-01-01) round trip and its
 *  step of exactly one per day over years -2000..3000, the order, and ISO text
 *  both ways with what fromIso refuses.
 */
#include <gweni/date.h>

#include <doctest/doctest.h>

using gweni::Date;

namespace doctest
{
template<> struct StringMaker<Date>
{
    static String convert(const Date &date) { return date.toIso().c_str(); }
};
}

TEST_CASE("Date: leap years and the days in a month")
{
    CHECK(Date::isLeapYear(2024));
    CHECK(Date::isLeapYear(2000));
    CHECK(Date::isLeapYear(0));
    CHECK(Date::isLeapYear(-4));
    CHECK_FALSE(Date::isLeapYear(1900));
    CHECK_FALSE(Date::isLeapYear(2100));
    CHECK_FALSE(Date::isLeapYear(2026));
    CHECK_FALSE(Date::isLeapYear(-1));

    const int days2026[12]={31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for(int m=1; m <= 12; ++m)
        CHECK(Date::daysInMonth(2026, m) == days2026[m-1]);
    CHECK(Date::daysInMonth(2024, 2) == 29);
    CHECK(Date::daysInMonth(1900, 2) == 28);
    CHECK(Date::daysInMonth(2000, 2) == 29);
    CHECK(Date::daysInMonth(2026, 0) == 0);
    CHECK(Date::daysInMonth(2026, 13) == 0);
}

TEST_CASE("Date: validity")
{
    CHECK(Date(2026, 11, 11).isValid());
    CHECK(Date(2024, 2, 29).isValid());
    CHECK_FALSE(Date(2026, 2, 29).isValid());
    CHECK_FALSE(Date(2026, 4, 31).isValid());
    CHECK_FALSE(Date(2026, 0, 1).isValid());
    CHECK_FALSE(Date(2026, 13, 1).isValid());
    CHECK_FALSE(Date(2026, 1, 0).isValid());
    CHECK(Date().isValid());
    CHECK(Date() == Date(1970, 1, 1));
}

TEST_CASE("Date: the weekday")
{
    CHECK(Date(1970, 1, 1).weekday() == 4);   // Thursday
    CHECK(Date(2026, 11, 11).weekday() == 3); // Wednesday
    CHECK(Date(2026, 11, 1).weekday() == 0);  // Sunday
    CHECK(Date(2000, 1, 1).weekday() == 6);   // Saturday
    CHECK(Date(1969, 12, 31).weekday() == 3); // Wednesday
    CHECK(Date(1969, 12, 28).weekday() == 0); // Sunday
    CHECK(Date(1969, 12, 27).weekday() == 6); // Saturday
    CHECK(Date(1900, 1, 1).weekday() == 1);   // Monday
    CHECK(Date(1, 1, 1).weekday() == 1);      // Monday (proleptic)
    CHECK(Date(0, 1, 1).weekday() == 6);      // Saturday

    // Each day is the next weekday, on both sides of the day number's 0.
    Date d(1969, 6, 1);
    int w=d.weekday();
    for(int i=0; i < 800; ++i)
    {
        d=d.addDays(1);
        w=(w+1)%7;
        REQUIRE(d.weekday() == w);
    }
}

TEST_CASE("Date: addDays across month and year ends")
{
    CHECK(Date(2026, 1, 31).addDays(1) == Date(2026, 2, 1));
    CHECK(Date(2026, 2, 28).addDays(1) == Date(2026, 3, 1));
    CHECK(Date(2024, 2, 28).addDays(1) == Date(2024, 2, 29));
    CHECK(Date(2024, 2, 29).addDays(1) == Date(2024, 3, 1));
    CHECK(Date(2026, 12, 31).addDays(1) == Date(2027, 1, 1));
    CHECK(Date(2027, 1, 1).addDays(-1) == Date(2026, 12, 31));
    CHECK(Date(2024, 3, 1).addDays(-1) == Date(2024, 2, 29));
    CHECK(Date(2026, 11, 11).addDays(7) == Date(2026, 11, 18));
    CHECK(Date(2026, 11, 11).addDays(-14) == Date(2026, 10, 28));
    CHECK(Date(2026, 11, 11).addDays(365) == Date(2027, 11, 11));
    CHECK(Date(2024, 1, 1).addDays(366) == Date(2025, 1, 1));
    CHECK(Date(2026, 11, 11).addDays(0) == Date(2026, 11, 11));
    CHECK(Date(1970, 1, 1).addDays(-1) == Date(1969, 12, 31));
}

TEST_CASE("Date: addMonths and addYears clamp the day")
{
    CHECK(Date(2026, 1, 31).addMonths(1) == Date(2026, 2, 28));
    CHECK(Date(2024, 1, 31).addMonths(1) == Date(2024, 2, 29));
    CHECK(Date(2026, 3, 31).addMonths(1) == Date(2026, 4, 30));
    CHECK(Date(2026, 3, 31).addMonths(-1) == Date(2026, 2, 28));
    CHECK(Date(2026, 11, 15).addMonths(2) == Date(2027, 1, 15));
    CHECK(Date(2026, 1, 15).addMonths(-1) == Date(2025, 12, 15));
    CHECK(Date(2026, 1, 15).addMonths(-13) == Date(2024, 12, 15));
    CHECK(Date(2026, 1, 15).addMonths(-25) == Date(2023, 12, 15));
    CHECK(Date(2026, 5, 31).addMonths(24) == Date(2028, 5, 31));
    CHECK(Date(0, 1, 10).addMonths(-1) == Date(-1, 12, 10));
    CHECK(Date(-1, 12, 10).addMonths(1) == Date(0, 1, 10));
    CHECK(Date(2026, 6, 6).addMonths(0) == Date(2026, 6, 6));

    CHECK(Date(2024, 2, 29).addYears(1) == Date(2025, 2, 28));
    CHECK(Date(2024, 2, 29).addYears(4) == Date(2028, 2, 29));
    CHECK(Date(2024, 2, 29).addYears(-1) == Date(2023, 2, 28));
    CHECK(Date(2026, 11, 11).addYears(-30) == Date(1996, 11, 11));

    CHECK(Date(2026, 11, 11).firstOfMonth() == Date(2026, 11, 1));
    CHECK(Date(2026, 11, 11).lastOfMonth() == Date(2026, 11, 30));
    CHECK(Date(2024, 2, 3).lastOfMonth() == Date(2024, 2, 29));
}

TEST_CASE("Date: the day number round trip over years -2000..3000")
{
    CHECK(Date(1970, 1, 1).toDayNumber() == 0);
    CHECK(Date(1970, 1, 2).toDayNumber() == 1);
    CHECK(Date(1969, 12, 31).toDayNumber() == -1);
    CHECK(Date(2000, 1, 1).toDayNumber() == 10957);
    CHECK(Date(0, 3, 1).toDayNumber() == -719468);
    CHECK(Date::fromDayNumber(0) == Date(1970, 1, 1));
    CHECK(Date::fromDayNumber(-719468) == Date(0, 3, 1));

    const int64_t first=Date(-2000, 1, 1).toDayNumber();
    const int64_t last=Date(3000, 12, 31).toDayNumber();
    Date expected(-2000, 1, 1);
    int64_t steps=0;
    for(int64_t n=first; n <= last; ++n)
    {
        const Date d=Date::fromDayNumber(n);
        REQUIRE(d == expected);
        REQUIRE(d.isValid());
        REQUIRE(d.toDayNumber() == n);
        // The next date by hand, without the day number.
        if(expected.day < Date::daysInMonth(expected.year, expected.month))
            ++expected.day;
        else if(expected.month < 12)
            expected=Date(expected.year, expected.month+1, 1);
        else
            expected=Date(expected.year+1, 1, 1);
        ++steps;
    }
    // 5001 years of 365 days, plus the leap days: 1213 of them in -2000..3000.
    CHECK(steps == 5001*365+1213);
}

TEST_CASE("Date: the order")
{
    CHECK(Date(2026, 11, 11) < Date(2026, 11, 12));
    CHECK(Date(2026, 10, 31) < Date(2026, 11, 1));
    CHECK(Date(2025, 12, 31) < Date(2026, 1, 1));
    CHECK(Date(-1, 12, 31) < Date(0, 1, 1));
    CHECK(Date(2026, 11, 11) <= Date(2026, 11, 11));
    CHECK(Date(2026, 11, 11) >= Date(2026, 11, 11));
    CHECK(Date(2026, 11, 12) > Date(2026, 11, 11));
    CHECK_FALSE(Date(2026, 11, 12) < Date(2026, 11, 11));
    CHECK(Date(2026, 11, 11) != Date(2026, 11, 12));
    CHECK_FALSE(Date(2026, 11, 11) != Date(2026, 11, 11));
}

TEST_CASE("Date: ISO text")
{
    CHECK(Date(2026, 11, 11).toIso() == "2026-11-11");
    CHECK(Date(2026, 1, 2).toIso() == "2026-01-02");
    CHECK(Date(5, 3, 4).toIso() == "0005-03-04");
    CHECK(Date(-1, 12, 31).toIso() == "-0001-12-31");
    CHECK(Date(10000, 1, 1).toIso() == "+10000-01-01");

    CHECK(Date::fromIso("2026-11-11") == Date(2026, 11, 11));
    CHECK(Date::fromIso("2024-02-29") == Date(2024, 2, 29));
    CHECK(Date::fromIso("0005-03-04") == Date(5, 3, 4));
    CHECK(Date::fromIso("-0001-12-31") == Date(-1, 12, 31));
    CHECK(Date::fromIso("+10000-01-01") == Date(10000, 1, 1));
    CHECK(Date::fromIso("+2026-11-11") == Date(2026, 11, 11));

    CHECK_FALSE(Date::fromIso("2026-02-29").has_value());
    CHECK_FALSE(Date::fromIso("2026-13-01").has_value());
    CHECK_FALSE(Date::fromIso("2026-00-10").has_value());
    CHECK_FALSE(Date::fromIso("2026-1-01").has_value());
    CHECK_FALSE(Date::fromIso("2026-01-1").has_value());
    CHECK_FALSE(Date::fromIso("26-01-01").has_value());
    CHECK_FALSE(Date::fromIso("20260-01-01").has_value());
    CHECK_FALSE(Date::fromIso("2026/01/01").has_value());
    CHECK_FALSE(Date::fromIso("2026-01-01 ").has_value());
    CHECK_FALSE(Date::fromIso("2026-01-01T00:00").has_value());
    CHECK_FALSE(Date::fromIso("").has_value());
    CHECK_FALSE(Date::fromIso("-").has_value());

    for(int y=-50; y <= 2100; y+=7)
        for(int m=1; m <= 12; ++m)
        {
            const Date d(y, m, Date::daysInMonth(y, m));
            REQUIRE(Date::fromIso(d.toIso()) == d);
        }
}
