/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  gweni::TimeOfDay (gweni/date.h), the plain hour and minute a TimeEdit
 *  holds: validity at the ends of each field, addMinutes wrapping at midnight
 *  both ways (and over many days), the 12-hour hour (00:xx is 12 AM, 12:xx is
 *  12 PM) and hourFrom12 back, the order, toText in 24 h ("09:05") and 12 h
 *  ("9:05 AM", "12:00 AM" for midnight, "12:00 PM" for noon), and fromText's
 *  lenient reading: "1430", "930", "9", "14:30", "2:30 pm", "2:30PM",
 *  "2.30 p.m.", "12 am", "12:00 PM", with what it refuses (an hour or minute
 *  out of range, 0 or 13 with a marker, other letters, three groups, a
 *  three-digit minute, two markers, a marker before the digits, an empty or blank text).
 */
#include <gweni/date.h>

#include <doctest/doctest.h>

using gweni::TimeOfDay;

namespace doctest
{
template<> struct StringMaker<TimeOfDay>
{
    static String convert(const TimeOfDay &time) { return time.toText().c_str(); }
};
}

TEST_CASE("TimeOfDay: validity, minutes since midnight and addMinutes wrapping")
{
    CHECK(TimeOfDay(0, 0).isValid());
    CHECK(TimeOfDay(23, 59).isValid());
    CHECK_FALSE(TimeOfDay(24, 0).isValid());
    CHECK_FALSE(TimeOfDay(-1, 0).isValid());
    CHECK_FALSE(TimeOfDay(12, 60).isValid());
    CHECK_FALSE(TimeOfDay(12, -1).isValid());

    CHECK(TimeOfDay(14, 30).toMinutes() == 870);
    CHECK(TimeOfDay::fromMinutes(870) == TimeOfDay(14, 30));
    CHECK(TimeOfDay::fromMinutes(-1) == TimeOfDay(23, 59));

    CHECK(TimeOfDay(23, 50).addMinutes(20) == TimeOfDay(0, 10));
    CHECK(TimeOfDay(0, 10).addMinutes(-20) == TimeOfDay(23, 50));
    CHECK(TimeOfDay(10, 0).addMinutes(1440*3+5) == TimeOfDay(10, 5));
    CHECK(TimeOfDay(10, 0).addMinutes(-1440*2-60) == TimeOfDay(9, 0));
}

TEST_CASE("TimeOfDay: the 12-hour hour, AM and PM")
{
    CHECK(TimeOfDay(0, 30).hour12() == 12);
    CHECK_FALSE(TimeOfDay(0, 30).isPm());
    CHECK(TimeOfDay(12, 0).hour12() == 12);
    CHECK(TimeOfDay(12, 0).isPm());
    CHECK(TimeOfDay(13, 0).hour12() == 1);
    CHECK(TimeOfDay(11, 59).hour12() == 11);
    CHECK_FALSE(TimeOfDay(11, 59).isPm());

    CHECK(TimeOfDay::hourFrom12(12, false) == 0);
    CHECK(TimeOfDay::hourFrom12(12, true) == 12);
    CHECK(TimeOfDay::hourFrom12(1, true) == 13);
    CHECK(TimeOfDay::hourFrom12(11, false) == 11);
}

TEST_CASE("TimeOfDay: order")
{
    CHECK(TimeOfDay(9, 59) < TimeOfDay(10, 0));
    CHECK(TimeOfDay(10, 0) > TimeOfDay(9, 59));
    CHECK(TimeOfDay(10, 0) <= TimeOfDay(10, 0));
    CHECK(TimeOfDay(10, 0) >= TimeOfDay(10, 0));
    CHECK(TimeOfDay(10, 0) != TimeOfDay(10, 1));
}

TEST_CASE("TimeOfDay: toText in 24 and 12 hours")
{
    CHECK(TimeOfDay(9, 5).toText() == "09:05");
    CHECK(TimeOfDay(14, 30).toText(false) == "14:30");
    CHECK(TimeOfDay(9, 5).toText(true) == "9:05 AM");
    CHECK(TimeOfDay(14, 30).toText(true) == "2:30 PM");
    CHECK(TimeOfDay(0, 0).toText(true) == "12:00 AM");
    CHECK(TimeOfDay(12, 0).toText(true) == "12:00 PM");
    CHECK(TimeOfDay(23, 59).toText(true) == "11:59 PM");
}

TEST_CASE("TimeOfDay: fromText reads leniently")
{
    using Opt=std::optional<TimeOfDay>;

    CHECK(TimeOfDay::fromText("1430") == Opt(TimeOfDay(14, 30)));
    CHECK(TimeOfDay::fromText("930") == Opt(TimeOfDay(9, 30)));
    CHECK(TimeOfDay::fromText("9") == Opt(TimeOfDay(9, 0)));
    CHECK(TimeOfDay::fromText("0000") == Opt(TimeOfDay(0, 0)));
    CHECK(TimeOfDay::fromText("14:30") == Opt(TimeOfDay(14, 30)));
    CHECK(TimeOfDay::fromText(" 14:3 ") == Opt(TimeOfDay(14, 3)));
    CHECK(TimeOfDay::fromText("2:30 pm") == Opt(TimeOfDay(14, 30)));
    CHECK(TimeOfDay::fromText("2:30PM") == Opt(TimeOfDay(14, 30)));
    CHECK(TimeOfDay::fromText("2.30 p.m.") == Opt(TimeOfDay(14, 30)));
    CHECK(TimeOfDay::fromText("230p") == Opt(TimeOfDay(14, 30)));
    CHECK(TimeOfDay::fromText("12 am") == Opt(TimeOfDay(0, 0)));
    CHECK(TimeOfDay::fromText("12:00 AM") == Opt(TimeOfDay(0, 0)));
    CHECK(TimeOfDay::fromText("12:00 PM") == Opt(TimeOfDay(12, 0)));
    CHECK(TimeOfDay::fromText("11:59 a") == Opt(TimeOfDay(11, 59)));

    CHECK_FALSE(TimeOfDay::fromText("").has_value());
    CHECK_FALSE(TimeOfDay::fromText("  ").has_value());
    CHECK_FALSE(TimeOfDay::fromText("24:00").has_value());
    CHECK_FALSE(TimeOfDay::fromText("12:60").has_value());
    CHECK_FALSE(TimeOfDay::fromText("2460").has_value());
    CHECK_FALSE(TimeOfDay::fromText("0:30 am").has_value());
    CHECK_FALSE(TimeOfDay::fromText("13:00 pm").has_value());
    CHECK_FALSE(TimeOfDay::fromText("2:30 xm").has_value());
    CHECK_FALSE(TimeOfDay::fromText("noon").has_value());
    CHECK_FALSE(TimeOfDay::fromText("1:2:3").has_value());
    CHECK_FALSE(TimeOfDay::fromText("1:300").has_value());
    CHECK_FALSE(TimeOfDay::fromText("12345").has_value());
    CHECK_FALSE(TimeOfDay::fromText("2 am pm").has_value());
    CHECK_FALSE(TimeOfDay::fromText("pm").has_value());
    CHECK_FALSE(TimeOfDay::fromText("am 7:15").has_value());
}
