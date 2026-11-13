/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#ifndef _gweni_controls_chartaxis_h_
#define _gweni_controls_chartaxis_h_

#include <gweni/export.h>
#include <gweni/platforms/platformTypes.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace gweni
{
namespace controls
{
//! The ticks and labels of a chart's axes, with no control behind them, so
//! Chart and Timeline share them as they are: value ticks at 1, 2 or 5 times a
//! power of ten, time ticks at one of a fixed list of steps (calendar days,
//! weeks, months and years in the axis's zone), category ticks, and the
//! thinning of labels that would overlap. A position along an axis is in the
//! axis's own units, 0 at its minimum and `length` at its maximum; what a
//! label's width is in those units is the caller's measure.
namespace axis
{

//! Seconds east of UTC at the instant t (epoch seconds), e.g. 3600 for CET
//! and 7200 for CEST. An empty one is UTC.
typedef std::function<int(double t)> UtcOffset;
//! A label's width, in the axis's units.
typedef std::function<double(const String &label)> LabelMeasure;
//! A value label in place of the axis's own.
typedef std::function<String(double value)> ValueFormatter;

//! The process's zone: localtime_r's tm_gmtoff, which follows TZ natively and
//! the browser's zone under Emscripten.
GWENI_EXPORT UtcOffset localUtcOffset();
//! A zone with no daylight saving time, seconds east of UTC.
GWENI_EXPORT UtcOffset fixedUtcOffset(int seconds);
//! offset(t), 0 for an empty hook.
GWENI_EXPORT int utcOffsetAt(const UtcOffset &offset, double t);

//! A tick: where it is (a value, an instant or a category index), its place
//! along the axis and its label.
struct Tick
{
    double value=0.0;
    double position=0.0;
    String label;
};

//! The axis position of value on an axis from min to max that is length long
//! (min at 0); 0 when max <= min.
GWENI_EXPORT double positionOf(double value, double min, double max, double length);

// ---- Value ticks -------------------------------------------------------

//! The smallest step of 1, 2 or 5 times a power of ten that cuts span into at
//! most maxTicks intervals (maxTicks below 1 counts as 1); 1 for a span that
//! is not positive or not finite.
GWENI_EXPORT double niceStep(double span, int maxTicks);

//! An automatic range: [min, max] widened to whole steps.
struct ValueRange
{
    double min=0.0;
    double max=1.0;
    double step=1.0;
};
//! min and max (in either order) widened to multiples of the smallest nice
//! step that gives at most maxTicks intervals once widened. An empty span
//! (min == max) is widened by a tenth of |min| each way, or to [0, 1] at 0.
GWENI_EXPORT ValueRange niceRange(double min, double max, int maxTicks);

//! The multiples of step in [min, max] (with a ten-thousandth of a step of
//! slack at each end), each k*step for a whole k so none drifts; empty for a
//! step that is not positive, min > max, or more than 100 000 ticks.
GWENI_EXPORT std::vector<double> valueTicks(double min, double max, double step);

//! value with as many decimals as writing step needs (0 for a whole step,
//! 1 for 0.5 or 2.5, 2 for 0.05 or 0.25 ...), "-0" written as "0".
GWENI_EXPORT String formatValue(double value, double step);

//! The ticks of a value axis from min to max, length long: valueTicks at
//! step, placed, labelled by formatter or, without one, formatValue.
GWENI_EXPORT std::vector<Tick> valueAxisTicks(double min, double max, double step, double length,
                                              const ValueFormatter &formatter=ValueFormatter());

// ---- Time ticks --------------------------------------------------------

//! The time axis's steps, smallest first. Day, Week (Mondays), Month (the
//! 1st) and Year (January 1st) ticks are at 00:00 in the axis's zone; the
//! steps under a day are at multiples of the step from the zone's midnight.
enum class TimeStep
{
    Second1,
    Second5,
    Second15,
    Minute1,
    Minute5,
    Minute15,
    Hour1,
    Hour6,
    Day1,
    Week1,
    Month1,
    Year1
};
constexpr int TimeStepCount=12;

//! The step's length in seconds (a month 30 days, a year 365).
GWENI_EXPORT double nominalSeconds(TimeStep step);

//! The instants of step's ticks in [min, max], ascending. A tick the zone's
//! clock skips (02:00 on a spring-forward night, for an hour step) is left
//! out, one it passes twice is there once; a calendar tick the clock skips
//! is put at the instant its day begins.
GWENI_EXPORT std::vector<double> timeTicks(double min, double max, TimeStep step,
                                           const UtcOffset &offset=UtcOffset());

//! The label of the tick at t for step, in the zone: "HH:MM:SS" (the second
//! steps) or "HH:MM" (minutes and hours), the date ("Nov 14") in their place
//! at the zone's midnight; "Nov 14" for Day and Week; "Nov" for Month; the
//! year ("2027") for Year, and for any other step's tick at 00:00 on January
//! 1st.
GWENI_EXPORT String formatTime(double t, TimeStep step, const UtcOffset &offset=UtcOffset());

//! A time axis's step and ticks.
struct TimeTicks
{
    TimeStep step=TimeStep::Year1;
    //! Whether no step's labels fit: step is then Year1, its labels overlap,
    //! and thinLabels picks the ones to show.
    bool overlapping=false;
    std::vector<Tick> ticks;
};
//! The smallest step whose labels, centred on their ticks in [min, max] on an
//! axis length long, leave at least gap between neighbours. A step that
//! would give more ticks than length/max(gap, 1) is not tried.
GWENI_EXPORT TimeTicks chooseTimeTicks(double min, double max, double length, const LabelMeasure &measure,
                                       double gap, const UtcOffset &offset=UtcOffset());

// ---- Category ticks and label thinning ----------------------------------

//! count ticks, one per category, each at the centre of its band of
//! length/count, value the index and label empty.
GWENI_EXPORT std::vector<Tick> categoryTicks(size_t count, double length);

//! Whether labels of these widths, centred on these positions (ascending),
//! taking every stride-th from the first, leave gap between neighbours.
GWENI_EXPORT bool labelsFit(const std::vector<double> &positions, const std::vector<double> &widths,
                            double gap, size_t stride=1);

//! The smallest stride at which every stride-th label from the first fits
//! (labelsFit); positions.size() when only one fits, 1 for none or one.
GWENI_EXPORT size_t thinLabels(const std::vector<double> &positions, const std::vector<double> &widths,
                               double gap);

//! The calendar of the proleptic Gregorian calendar, for the time ticks and
//! their tests: the days since 1970-01-01 of a date, and back.
struct CivilDate
{
    int64_t year=1970;
    int month=1; // 1..12
    int day=1;   // 1..31
};
GWENI_EXPORT int64_t daysFromCivil(int64_t year, int month, int day);
GWENI_EXPORT CivilDate civilFromDays(int64_t days);

}//namespace axis
}//namespace controls
}//namespace gweni

#endif//_gweni_controls_chartaxis_h_
