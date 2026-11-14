/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#ifndef _gweni_date_h_
#define _gweni_date_h_

#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>

namespace gweni
{

//! A calendar date: a year, a month (1-12) and a day of the month, on the
//! proleptic Gregorian calendar, with no time zone and no clock. Nothing here
//! reads the system's time: "today" is whatever date the app says it is, since
//! which day it is depends on a time zone, and that is the app's concern.
//!
//! The year is astronomical (year 0 is 1 BC, -1 is 2 BC). The arithmetic goes
//! through the day number, the days since 1970-01-01 (Howard Hinnant's
//! days_from_civil and civil_from_days), exact for any int year whose day
//! number fits in an int64.
struct Date
{
    int year=1970;
    int month=1;
    int day=1;

    constexpr Date()=default;
    constexpr Date(int y, int m, int d):year(y), month(m), day(d) {}

    static constexpr bool isLeapYear(int y)
    {
        return (y%4 == 0) && ((y%100 != 0) || (y%400 == 0));
    }

    //! The days in month m (1-12) of year y; 0 for a month outside 1-12.
    static constexpr int daysInMonth(int y, int m)
    {
        if((m < 1) || (m > 12))
            return 0;
        if(m == 2)
            return isLeapYear(y)?29:28;
        if((m == 4) || (m == 6) || (m == 9) || (m == 11))
            return 30;
        return 31;
    }

    //! The month exists and the day is in it.
    constexpr bool isValid() const
    {
        return (month >= 1) && (month <= 12) && (day >= 1) && (day <= daysInMonth(year, month));
    }

    //! Days since 1970-01-01 (negative before it). Meaningful for a valid date.
    constexpr int64_t toDayNumber() const
    {
        const int64_t y=int64_t(year)-(month <= 2?1:0);
        const int64_t era=(y >= 0?y:y-399)/400;
        const int64_t yoe=y-era*400;                                         // [0, 399]
        const int64_t mp=(month+9)%12;                                       // March is 0
        const int64_t doy=(153*mp+2)/5+day-1;                                // [0, 365]
        const int64_t doe=yoe*365+yoe/4-yoe/100+doy;                         // [0, 146096]
        return era*146097+doe-719468;
    }

    static constexpr Date fromDayNumber(int64_t n)
    {
        n+=719468;
        const int64_t era=(n >= 0?n:n-146096)/146097;
        const int64_t doe=n-era*146097;                                      // [0, 146096]
        const int64_t yoe=(doe-doe/1460+doe/36524-doe/146096)/365;           // [0, 399]
        const int64_t doy=doe-(365*yoe+yoe/4-yoe/100);                       // [0, 365]
        const int64_t mp=(5*doy+2)/153;                                      // [0, 11]
        const int d=int(doy-(153*mp+2)/5+1);
        const int m=int(mp < 10?mp+3:mp-9);
        const int y=int(yoe+era*400+(m <= 2?1:0));
        return Date(y, m, d);
    }

    //! 0 is Sunday, 6 Saturday.
    constexpr int weekday() const
    {
        const int64_t n=toDayNumber();
        // 1970-01-01 was a Thursday (4).
        const int64_t w=(n+4)%7;
        return int(w < 0?w+7:w);
    }

    constexpr Date addDays(int64_t n) const
    {
        return fromDayNumber(toDayNumber()+n);
    }

    //! n months on (back, for a negative n), the day clamped to the new month's
    //! last: January 31 plus one month is February 28 or 29.
    constexpr Date addMonths(int64_t n) const
    {
        int64_t total=int64_t(year)*12+(month-1)+n;
        int64_t y=(total >= 0?total:total-11)/12;
        const int m=int(total-y*12)+1;
        const int last=daysInMonth(int(y), m);
        return Date(int(y), m, day > last?last:day);
    }

    //! n years on, the day clamped: February 29 plus one year is February 28.
    constexpr Date addYears(int64_t n) const
    {
        return addMonths(n*12);
    }

    //! The first day of this date's month.
    constexpr Date firstOfMonth() const { return Date(year, month, 1); }
    //! The last day of this date's month.
    constexpr Date lastOfMonth() const { return Date(year, month, daysInMonth(year, month)); }

    //! "2026-11-11": the year as at least 4 digits, with a sign outside
    //! 0..9999 ("-0001", "+10000", ISO 8601's expanded form).
    std::string toIso() const
    {
        char buffer[32];
        if((year >= 0) && (year <= 9999))
            std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", year, month, day);
        else
            std::snprintf(buffer, sizeof(buffer), "%c%04lld-%02d-%02d", year < 0?'-':'+',
                (long long)(year < 0?-int64_t(year):int64_t(year)), month, day);
        return buffer;
    }

    //! Reads toIso()'s form: an optional sign, 4 or more year digits (a sign
    //! for more than 4), '-', 2 month digits, '-', 2 day digits, nothing else.
    //! Empty for anything else, an invalid date included ("2026-02-30").
    static std::optional<Date> fromIso(const std::string &text)
    {
        size_t i=0;
        bool negative=false;
        bool sign=false;

        if((i < text.size()) && ((text[i] == '-') || (text[i] == '+')))
        {
            negative=(text[i] == '-');
            sign=true;
            ++i;
        }

        int64_t y=0;
        size_t digits=0;
        while((i < text.size()) && (text[i] >= '0') && (text[i] <= '9'))
        {
            y=y*10+(text[i]-'0');
            if(y > 100000000)
                return std::nullopt;
            ++i;
            ++digits;
        }
        if((digits < 4) || ((digits > 4) && !sign))
            return std::nullopt;

        auto twoDigits=[&](int &value) -> bool
        {
            if((i+3 > text.size()) || (text[i] != '-'))
                return false;
            const char a=text[i+1];
            const char b=text[i+2];
            if((a < '0') || (a > '9') || (b < '0') || (b > '9'))
                return false;
            value=(a-'0')*10+(b-'0');
            i+=3;
            return true;
        };

        int m=0;
        int d=0;
        if(!twoDigits(m) || !twoDigits(d) || (i != text.size()))
            return std::nullopt;

        const Date date(int(negative?-y:y), m, d);
        if(!date.isValid())
            return std::nullopt;
        return date;
    }

    constexpr bool operator==(const Date &other) const
    {
        return (year == other.year) && (month == other.month) && (day == other.day);
    }
    constexpr bool operator!=(const Date &other) const { return !(*this == other); }
    constexpr bool operator<(const Date &other) const
    {
        if(year != other.year)
            return year < other.year;
        if(month != other.month)
            return month < other.month;
        return day < other.day;
    }
    constexpr bool operator>(const Date &other) const { return other < *this; }
    constexpr bool operator<=(const Date &other) const { return !(other < *this); }
    constexpr bool operator>=(const Date &other) const { return !(*this < other); }
};

}//namespace gweni

#endif//_gweni_date_h_
