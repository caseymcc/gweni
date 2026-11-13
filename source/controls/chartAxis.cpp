/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/controls/chartAxis.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <limits>

namespace gweni
{
namespace controls
{
namespace axis
{

namespace
{

constexpr double SecondsPerDay=86400.0;

const char *const MonthNames[12]={"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

const double StepSeconds[TimeStepCount]={1.0, 5.0, 15.0, 60.0, 300.0, 900.0, 3600.0, 21600.0,
                                           SecondsPerDay, 7.0*SecondsPerDay, 30.0*SecondsPerDay,
                                           365.0*SecondsPerDay};

int64_t floorDiv(int64_t a, int64_t b)
{
    int64_t q=a/b;

    if((a%b != 0) && ((a < 0) != (b < 0)))
        --q;
    return q;
}

bool isCalendarStep(TimeStep step)
{
    return step >= TimeStep::Day1;
}

//! The step after step among 1, 2, 5 times a power of ten.
double nextNiceStep(double step)
{
    const double magnitude=std::pow(10.0, std::floor(std::log10(step)));
    const double mantissa=step/magnitude;

    if(mantissa < 1.5)
        return 2.0*magnitude;
    if(mantissa < 3.5)
        return 5.0*magnitude;
    return 10.0*magnitude;
}

//! The first instant at or after x whose local time (x+offset) is a multiple
//! of step.
double alignUp(double x, int offset, double step)
{
    return std::ceil((x+offset)/step)*step-offset;
}

//! The smallest whole second in (lo, hi] whose offset is offset(hi): where
//! the zone's offset changed, for a lo and hi whose offsets differ.
double findTransition(const UtcOffset &offset, double lo, double hi)
{
    const int after=utcOffsetAt(offset, hi);
    double low=std::floor(lo);  // offset(low) != after, or low <= lo
    double high=std::ceil(hi);

    if(utcOffsetAt(offset, high) != after)
        high=hi;
    while(high-low > 1.0)
    {
        const double middle=std::floor((low+high)/2.0);

        if(utcOffsetAt(offset, middle) == after)
            high=middle;
        else
            low=middle;
    }
    return std::max(high, std::nextafter(lo, std::numeric_limits<double>::infinity()));
}

//! The instant at which the local time local begins in the zone: the instant
//! whose local time it is, or, when the clock skips it, the instant the skip
//! ends. exists says which.
double localToUtc(const UtcOffset &offset, double local, bool *exists)
{
    const double first=local-utcOffsetAt(offset, local);
    double t=first;

    for(int i=0; i < 4; ++i)
    {
        const double next=local-utcOffsetAt(offset, t);

        if(next == t)
            break;
        t=next;
    }

    if(t+utcOffsetAt(offset, t) == local)
    {
        if(exists)
            *exists=true;
        return t;
    }

    // skipped: the offsets before and after the jump bound the instant it
    // happened at, and the first instant past it is where this time would be
    const double a=local-utcOffsetAt(offset, first);
    const double b=first;
    const double lo=std::min(a, b);
    const double hi=std::max(a, b);

    if(exists)
        *exists=false;
    if(utcOffsetAt(offset, lo) == utcOffsetAt(offset, hi))
        return hi;
    return findTransition(offset, lo, hi);
}

int64_t localDay(const UtcOffset &offset, double t)
{
    return static_cast<int64_t>(std::floor((t+utcOffsetAt(offset, t))/SecondsPerDay));
}

std::vector<double> subDayTicks(double min, double max, double step, const UtcOffset &offset)
{
    std::vector<double> ticks;
    int current=utcOffsetAt(offset, min);
    double lo=min;
    double t=alignUp(min, current, step);
    double lastLocal=-std::numeric_limits<double>::infinity();

    while(t <= max)
    {
        const int at=utcOffsetAt(offset, t);

        if(at != current)
        {
            // the offset changed in (lo, t]: the ticks go on from there,
            // aligned to the new one
            const double transition=findTransition(offset, lo, t);

            current=utcOffsetAt(offset, transition);
            lo=transition;
            t=alignUp(transition, current, step);
            continue;
        }

        const double local=t+current;

        // a time the clock passes twice is a tick once
        if(local > lastLocal)
        {
            ticks.push_back(t);
            lastLocal=local;
        }
        lo=t;
        t+=step;
    }
    return ticks;
}

std::vector<double> calendarTicks(double min, double max, TimeStep step, const UtcOffset &offset)
{
    std::vector<double> ticks;
    const int64_t firstDay=localDay(offset, min)-1;
    auto add=[&](int64_t day) -> bool
        {
            const double t=localToUtc(offset, static_cast<double>(day)*SecondsPerDay, nullptr);

            if(t > max)
                return false;
            if(t >= min && (ticks.empty() || t > ticks.back()))
                ticks.push_back(t);
            return true;
        };

    switch(step)
    {
    case TimeStep::Day1:
        for(int64_t day=firstDay; add(day); ++day)
            ;
        break;
    case TimeStep::Week1:
    {
        // 1970-01-01 was a Thursday; Monday is 0
        const int64_t weekday=((firstDay+3)%7+7)%7;

        for(int64_t day=firstDay-weekday; add(day); day+=7)
            ;
        break;
    }
    case TimeStep::Month1:
    {
        const CivilDate date=civilFromDays(firstDay);
        int64_t year=date.year;
        int month=date.month;

        while(add(daysFromCivil(year, month, 1)))
        {
            if(++month > 12)
            {
                month=1;
                ++year;
            }
        }
        break;
    }
    case TimeStep::Year1:
    default:
        for(int64_t year=civilFromDays(firstDay).year; add(daysFromCivil(year, 1, 1)); ++year)
            ;
        break;
    }
    return ticks;
}

String formatDate(const CivilDate &date)
{
    char text[32];

    std::snprintf(text, sizeof(text), "%s %d", MonthNames[date.month-1], date.day);
    return text;
}

String formatYear(int64_t year)
{
    char text[32];

    std::snprintf(text, sizeof(text), "%lld", static_cast<long long>(year));
    return text;
}

}//namespace

UtcOffset localUtcOffset()
{
    return [](double t) -> int
        {
            if(!std::isfinite(t))
                return 0;

            const std::time_t seconds=static_cast<std::time_t>(std::floor(t));
            std::tm local{};

#if defined(_WIN32)
            if(localtime_s(&local, &seconds) != 0)
                return 0;
            return static_cast<int>(_mkgmtime(&local)-seconds);
#else
            if(!localtime_r(&seconds, &local))
                return 0;
            return static_cast<int>(local.tm_gmtoff);
#endif
        };
}

UtcOffset fixedUtcOffset(int seconds)
{
    return [seconds](double) { return seconds; };
}

int utcOffsetAt(const UtcOffset &offset, double t)
{
    return offset ? offset(t) : 0;
}

double positionOf(double value, double min, double max, double length)
{
    if(!(max > min))
        return 0.0;
    return (value-min)/(max-min)*length;
}

double niceStep(double span, int maxTicks)
{
    if(!(span > 0.0) || !std::isfinite(span))
        return 1.0;

    const double raw=span/std::max(maxTicks, 1);
    const double magnitude=std::pow(10.0, std::floor(std::log10(raw)));
    const double norm=raw/magnitude;
    const double slack=1.0+1e-9;

    if(norm <= 1.0*slack)
        return magnitude;
    if(norm <= 2.0*slack)
        return 2.0*magnitude;
    if(norm <= 5.0*slack)
        return 5.0*magnitude;
    return 10.0*magnitude;
}

ValueRange niceRange(double min, double max, int maxTicks)
{
    ValueRange range;

    if(!std::isfinite(min) || !std::isfinite(max))
        return range;
    if(min > max)
        std::swap(min, max);
    if(min == max)
    {
        if(min == 0.0)
        {
            max=1.0;
        }
        else
        {
            const double widen=std::fabs(min)*0.1;

            min-=widen;
            max+=widen;
        }
    }

    const int ticks=std::max(maxTicks, 1);
    double step=niceStep(max-min, ticks);

    for(int i=0; i < 8; ++i)
    {
        const double lo=std::floor(min/step+1e-9)*step;
        const double hi=std::ceil(max/step-1e-9)*step;

        if((hi-lo)/step <= ticks+1e-9 || i == 7)
        {
            range.min=lo;
            range.max=hi;
            range.step=step;
            break;
        }
        step=nextNiceStep(step);
    }
    return range;
}

std::vector<double> valueTicks(double min, double max, double step)
{
    std::vector<double> ticks;

    if(!(step > 0.0) || !(max >= min) || !std::isfinite(min) || !std::isfinite(max))
        return ticks;

    const double first=std::ceil(min/step-1e-4);
    const double last=std::floor(max/step+1e-4);

    if(last-first > 100000.0)
        return ticks;
    for(double k=first; k <= last; k+=1.0)
        ticks.push_back(k*step);
    return ticks;
}

String formatValue(double value, double step)
{
    int decimals=0;

    if(step > 0.0 && step < 1.0)
        decimals=std::min(15, static_cast<int>(std::ceil(-std::log10(step)-1e-9)));

    char text[64];

    std::snprintf(text, sizeof(text), "%.*f", decimals, value);

    String label=text;

    if(!label.empty() && label[0] == '-' && label.find_first_not_of("-0.") == String::npos)
        label.erase(0, 1);
    return label;
}

std::vector<Tick> valueAxisTicks(double min, double max, double step, double length,
                                 const ValueFormatter &formatter)
{
    std::vector<Tick> ticks;

    for(double value:valueTicks(min, max, step))
    {
        Tick tick;

        tick.value=value;
        tick.position=positionOf(value, min, max, length);
        tick.label=formatter ? formatter(value) : formatValue(value, step);
        ticks.push_back(std::move(tick));
    }
    return ticks;
}

double nominalSeconds(TimeStep step)
{
    return StepSeconds[static_cast<int>(step)];
}

std::vector<double> timeTicks(double min, double max, TimeStep step, const UtcOffset &offset)
{
    if(!std::isfinite(min) || !std::isfinite(max) || min > max)
        return {};
    if(isCalendarStep(step))
        return calendarTicks(min, max, step, offset);
    return subDayTicks(min, max, nominalSeconds(step), offset);
}

String formatTime(double t, TimeStep step, const UtcOffset &offset)
{
    const double local=std::floor(t+utcOffsetAt(offset, t)+1e-6);
    const int64_t day=static_cast<int64_t>(std::floor(local/SecondsPerDay));
    const int64_t second=static_cast<int64_t>(local)-day*static_cast<int64_t>(SecondsPerDay);
    const CivilDate date=civilFromDays(day);
    const bool newYear=(date.month == 1 && date.day == 1);
    char text[32];

    switch(step)
    {
    case TimeStep::Year1:
        return formatYear(date.year);
    case TimeStep::Month1:
        return newYear ? formatYear(date.year) : String(MonthNames[date.month-1]);
    case TimeStep::Day1:
    case TimeStep::Week1:
        return newYear ? formatYear(date.year) : formatDate(date);
    default:
        break;
    }

    if(second == 0)
        return newYear ? formatYear(date.year) : formatDate(date);

    const int hours=static_cast<int>(second/3600);
    const int minutes=static_cast<int>((second/60)%60);
    const int seconds=static_cast<int>(second%60);

    if(step <= TimeStep::Second15)
        std::snprintf(text, sizeof(text), "%02d:%02d:%02d", hours, minutes, seconds);
    else
        std::snprintf(text, sizeof(text), "%02d:%02d", hours, minutes);
    return text;
}

TimeTicks chooseTimeTicks(double min, double max, double length, const LabelMeasure &measure,
                          double gap, const UtcOffset &offset)
{
    TimeTicks result;

    if(!std::isfinite(min) || !std::isfinite(max) || !(max > min))
        return result;

    const double mostTicks=length/std::max(gap, 1.0)+1.0;
    std::vector<double> positions;
    std::vector<double> widths;

    for(int i=0; i < TimeStepCount; ++i)
    {
        const TimeStep step=static_cast<TimeStep>(i);
        const bool last=(i == TimeStepCount-1);

        if(!last && (max-min)/nominalSeconds(step) > mostTicks)
            continue;

        std::vector<Tick> ticks;

        positions.clear();
        widths.clear();
        for(double t:timeTicks(min, max, step, offset))
        {
            Tick tick;

            tick.value=t;
            tick.position=positionOf(t, min, max, length);
            tick.label=formatTime(t, step, offset);
            positions.push_back(tick.position);
            widths.push_back(measure ? measure(tick.label) : 0.0);
            ticks.push_back(std::move(tick));
        }

        const bool fits=labelsFit(positions, widths, gap);

        if(fits || last)
        {
            result.step=step;
            result.overlapping=!fits;
            result.ticks=std::move(ticks);
            break;
        }
    }
    return result;
}

std::vector<Tick> categoryTicks(size_t count, double length)
{
    std::vector<Tick> ticks(count);

    for(size_t i=0; i < count; ++i)
    {
        ticks[i].value=static_cast<double>(i);
        ticks[i].position=(static_cast<double>(i)+0.5)*length/static_cast<double>(count);
    }
    return ticks;
}

bool labelsFit(const std::vector<double> &positions, const std::vector<double> &widths, double gap,
               size_t stride)
{
    const size_t count=std::min(positions.size(), widths.size());

    stride=std::max<size_t>(stride, 1);
    for(size_t i=0; i+stride < count; i+=stride)
    {
        const size_t j=i+stride;
        const double right=positions[i]+widths[i]/2.0;
        const double left=positions[j]-widths[j]/2.0;

        if(left-right < gap)
            return false;
    }
    return true;
}

size_t thinLabels(const std::vector<double> &positions, const std::vector<double> &widths, double gap)
{
    const size_t count=std::min(positions.size(), widths.size());

    for(size_t stride=1; stride < count; ++stride)
    {
        if(labelsFit(positions, widths, gap, stride))
            return stride;
    }
    return std::max<size_t>(count, 1);
}

int64_t daysFromCivil(int64_t year, int month, int day)
{
    // Howard Hinnant's days_from_civil
    year-=(month <= 2) ? 1 : 0;

    const int64_t era=floorDiv(year, 400);
    const int64_t yearOfEra=year-era*400;
    const int64_t dayOfYear=(153*(month+(month > 2 ? -3 : 9))+2)/5+day-1;
    const int64_t dayOfEra=yearOfEra*365+yearOfEra/4-yearOfEra/100+dayOfYear;

    return era*146097+dayOfEra-719468;
}

CivilDate civilFromDays(int64_t days)
{
    days+=719468;

    const int64_t era=floorDiv(days, 146097);
    const int64_t dayOfEra=days-era*146097;
    const int64_t yearOfEra=(dayOfEra-dayOfEra/1460+dayOfEra/36524-dayOfEra/146096)/365;
    const int64_t dayOfYear=dayOfEra-(365*yearOfEra+yearOfEra/4-yearOfEra/100);
    const int64_t monthIndex=(5*dayOfYear+2)/153;
    CivilDate date;

    date.day=static_cast<int>(dayOfYear-(153*monthIndex+2)/5+1);
    date.month=static_cast<int>(monthIndex < 10 ? monthIndex+3 : monthIndex-9);
    date.year=yearOfEra+era*400+(date.month <= 2 ? 1 : 0);
    return date;
}

}//namespace axis
}//namespace controls
}//namespace gweni
