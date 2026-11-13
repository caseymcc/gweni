/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/chart.h>
#include <gweni/gweni.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/text.h>
#include <gweni/skin.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace gweni
{
namespace controls
{

namespace
{

bool isBar(ChartSeriesType type)
{
    return type == ChartSeriesType::Bar || type == ChartSeriesType::StackedBar;
}

bool isLine(ChartSeriesType type)
{
    return type == ChartSeriesType::Line || type == ChartSeriesType::StepLine;
}

bool byX(const ChartPoint &a, const ChartPoint &b)
{
    return a.x < b.x;
}

constexpr float NaNf=std::numeric_limits<float>::quiet_NaN();

}//namespace

GWENI_CONTROL_CONSTRUCTOR(Chart)
{
    setBounds(gweni::Rect(0, 0, 400, 240));
    // inside the frame's 1-unit border, with room clear of it
    setPadding(Padding(4, 4, 4, 4));
    m_yAxes[0].grid=true;
    m_utcOffset=axis::localUtcOffset();

    // the legend's rows scroll up and down, never sideways
    m_legend=newChild<ScrollControl>();
    m_legend->setPlacement(Placement::Owner);
    m_legend->setScroll(false, true);
    m_legend->setAutoHideBars(true);
    m_legend->setHidden(true);
}

// the labels are children: ~Base deletes them
Chart::~Chart()
{
}

// ---- series -----------------------------------------------------------------

size_t Chart::addSeries(const String &name, ChartSeriesType type, ChartAxisSide axis)
{
    Series series;

    series.name=name;
    series.type=type;
    series.axis=axis;
    series.colorIndex=static_cast<int>(m_series.size()%8);
    m_series.push_back(std::move(series));

    // one more layer for the renderer (depthLayersOf()), read only on a
    // renumber: this control's range is numbered again
    zOrderChanged();
    axesChanged();
    syncLegend();
    return m_series.size()-1;
}

void Chart::removeSeries(size_t series)
{
    if(series >= m_series.size())
        return;

    m_series.erase(m_series.begin()+static_cast<std::ptrdiff_t>(series));
    // a press on a point is no click once the series it was on moved
    m_pressed=false;

    zOrderChanged();
    axesChanged();
    syncLegend();
}

void Chart::clearSeries()
{
    if(m_series.empty())
        return;

    m_series.clear();
    m_pressed=false;

    zOrderChanged();
    axesChanged();
    syncLegend();
}

void Chart::setSeriesName(size_t series, const String &name)
{
    if(series >= m_series.size() || m_series[series].name == name)
        return;

    m_series[series].name=name;
    syncLegend();
}

String Chart::getSeriesName(size_t series) const
{
    const Series *s=seriesAt(series);

    return s ? s->name : String();
}

ChartSeriesType Chart::getSeriesType(size_t series) const
{
    const Series *s=seriesAt(series);

    return s ? s->type : ChartSeriesType::Line;
}

void Chart::setSeriesAxis(size_t series, ChartAxisSide axis)
{
    if(series >= m_series.size() || m_series[series].axis == axis)
        return;

    m_series[series].axis=axis;
    axesChanged();
}

ChartAxisSide Chart::getSeriesAxis(size_t series) const
{
    const Series *s=seriesAt(series);

    return s ? s->axis : ChartAxisSide::Left;
}

void Chart::setSeriesVisible(size_t series, bool visible)
{
    if(series >= m_series.size() || m_series[series].visible == visible)
        return;

    m_series[series].visible=visible;
    dataChanged(series);
    // the row's swatch and name show it
    if(internal::ChartLegendRow *row=getLegendRow(series))
        row->redraw();
}

bool Chart::isSeriesVisible(size_t series) const
{
    const Series *s=seriesAt(series);

    return s && s->visible;
}

void Chart::setSeriesColorIndex(size_t series, int index)
{
    if(series >= m_series.size() || m_series[series].colorIndex == index)
        return;

    m_series[series].colorIndex=index;
    addStateChange(StateChange_Background);
    if(internal::ChartLegendRow *row=getLegendRow(series))
        row->redraw();
}

int Chart::getSeriesColorIndex(size_t series) const
{
    const Series *s=seriesAt(series);

    return s ? s->colorIndex : 0;
}

void Chart::setSeriesLineThickness(size_t series, float thickness)
{
    if(!(thickness > 0.0f))
        thickness=1.0f;
    if(series >= m_series.size() || m_series[series].thickness == thickness)
        return;

    m_series[series].thickness=thickness;
    addStateChange(StateChange_Background);
}

float Chart::getSeriesLineThickness(size_t series) const
{
    const Series *s=seriesAt(series);

    return s ? s->thickness : 1.0f;
}

void Chart::setSeriesPoints(size_t series, std::vector<ChartPoint> points)
{
    if(series >= m_series.size())
        return;

    points.erase(std::remove_if(points.begin(), points.end(),
        [](const ChartPoint &p) { return !std::isfinite(p.x); }), points.end());
    if(!std::is_sorted(points.begin(), points.end(), byX))
        std::stable_sort(points.begin(), points.end(), byX);
    if(points == m_series[series].points)
        return;

    m_series[series].points=std::move(points);
    dataChanged(series);
}

void Chart::appendPoint(size_t series, double x, double y)
{
    if(series >= m_series.size() || !std::isfinite(x))
        return;

    std::vector<ChartPoint> &points=m_series[series].points;
    const ChartPoint point(x, y);

    if(points.empty() || !(x < points.back().x))
        points.push_back(point);
    else
        points.insert(std::upper_bound(points.begin(), points.end(), point, byX), point);
    dataChanged(series);
}

void Chart::setPointValue(size_t series, size_t index, double y)
{
    if(series >= m_series.size() || index >= m_series[series].points.size())
        return;

    double &value=m_series[series].points[index].y;

    // a NaN replacing a NaN is no change
    if(value == y || (std::isnan(value) && std::isnan(y)))
        return;

    value=y;
    dataChanged(series);
}

void Chart::removeFirstPoints(size_t series, size_t count)
{
    if(series >= m_series.size() || count == 0)
        return;

    std::vector<ChartPoint> &points=m_series[series].points;

    count=std::min(count, points.size());
    if(count == 0)
        return;

    points.erase(points.begin(), points.begin()+static_cast<std::ptrdiff_t>(count));
    dataChanged(series);
}

const std::vector<ChartPoint> &Chart::getSeriesPoints(size_t series) const
{
    static const std::vector<ChartPoint> none;
    const Series *s=seriesAt(series);

    return s ? s->points : none;
}

// ---- the axes ---------------------------------------------------------------

void Chart::setXAxisType(ChartXAxisType type)
{
    if(m_xType == type)
        return;

    m_xType=type;
    axesChanged();
}

void Chart::setCategories(const std::vector<String> &categories)
{
    if(m_categories == categories)
        return;

    m_categories=categories;
    axesChanged();
}

void Chart::setXRange(double min, double max)
{
    if(!(max > min))
        max=min+1.0;
    if(m_xFixed && m_xFixedMin == min && m_xFixedMax == max)
        return;

    m_xFixed=true;
    m_xFixedMin=min;
    m_xFixedMax=max;
    axesChanged();
}

void Chart::setXAutoRange()
{
    if(!m_xFixed)
        return;

    m_xFixed=false;
    axesChanged();
}

double Chart::getXMin() const
{
    ensureData();
    return m_xFixed ? m_xFixedMin : m_dataXMin;
}

double Chart::getXMax() const
{
    ensureData();
    return m_xFixed ? m_xFixedMax : m_dataXMax;
}

void Chart::setBarInterval(double interval)
{
    if(!(interval > 0.0))
        interval=0.0;
    if(m_barInterval == interval)
        return;

    m_barInterval=interval;
    axesChanged();
}

double Chart::getBarInterval() const
{
    ensureData();
    return m_effectiveBarInterval;
}

void Chart::setUtcOffset(const axis::UtcOffset &offset)
{
    m_utcOffset=offset;
    axesChanged();
}

void Chart::setXFormatter(const axis::ValueFormatter &formatter)
{
    m_xFormatter=formatter;
    axesChanged();
}

void Chart::setXGrid(bool grid)
{
    if(m_xGrid == grid)
        return;

    m_xGrid=grid;
    addStateChange(StateChange_Background);
}

void Chart::setXTitle(const String &title)
{
    if(m_xTitle == title)
        return;

    m_xTitle=title;
    invalidate();
}

void Chart::setYRange(ChartAxisSide side, double min, double max)
{
    ValueAxis &yAxis=axisOf(side);

    if(!(max > min))
        max=min+1.0;
    if(yAxis.fixed && yAxis.fixedMin == min && yAxis.fixedMax == max)
        return;

    yAxis.fixed=true;
    yAxis.fixedMin=min;
    yAxis.fixedMax=max;
    axesChanged();
}

void Chart::setYAutoRange(ChartAxisSide side)
{
    ValueAxis &yAxis=axisOf(side);

    if(!yAxis.fixed)
        return;

    yAxis.fixed=false;
    axesChanged();
}

double Chart::getYMin(ChartAxisSide side) const
{
    const ValueAxis &yAxis=axisOf(side);

    return yAxis.fixed ? yAxis.fixedMin : yAxis.range.min;
}

double Chart::getYMax(ChartAxisSide side) const
{
    const ValueAxis &yAxis=axisOf(side);

    return yAxis.fixed ? yAxis.fixedMax : yAxis.range.max;
}

void Chart::setYIncludesZero(ChartAxisSide side, bool include)
{
    ValueAxis &yAxis=axisOf(side);

    if(yAxis.includeZero == include)
        return;

    yAxis.includeZero=include;
    axesChanged();
}

void Chart::setYGrid(ChartAxisSide side, bool grid)
{
    ValueAxis &yAxis=axisOf(side);

    if(yAxis.grid == grid)
        return;

    yAxis.grid=grid;
    addStateChange(StateChange_Background);
}

void Chart::setYFormatter(ChartAxisSide side, const axis::ValueFormatter &formatter)
{
    axisOf(side).formatter=formatter;
    axesChanged();
}

void Chart::setYTitle(ChartAxisSide side, const String &title)
{
    ValueAxis &yAxis=axisOf(side);

    if(yAxis.title == title)
        return;

    yAxis.title=title;
    invalidate();
}

bool Chart::isYAxisShown(ChartAxisSide side) const
{
    ensureData();

    if(axisOf(side).used)
        return true;
    // the left axis of a chart with no series on either
    return side == ChartAxisSide::Left && !m_yAxes[1].used;
}

// ---- markers and spans ------------------------------------------------------

size_t Chart::addMarker(double x, const String &caption)
{
    Marker marker;

    marker.x=x;
    marker.caption=caption;
    m_markers.push_back(std::move(marker));
    addStateChange(StateChange_Background);
    // its caption is made and placed by the layout
    if(!caption.empty())
        invalidate();
    return m_markers.size()-1;
}

void Chart::setMarkerX(size_t marker, double x)
{
    if(marker >= m_markers.size() || m_markers[marker].x == x)
        return;

    m_markers[marker].x=x;
    addStateChange(StateChange_Background);
    if(!m_markers[marker].caption.empty())
        invalidate();
}

void Chart::setMarkerCaption(size_t marker, const String &caption)
{
    if(marker >= m_markers.size() || m_markers[marker].caption == caption)
        return;

    m_markers[marker].caption=caption;
    invalidate();
}

void Chart::removeMarker(size_t marker)
{
    if(marker >= m_markers.size())
        return;

    delete m_markers[marker].label;  // ~Base takes it out of the children
    m_markers.erase(m_markers.begin()+static_cast<std::ptrdiff_t>(marker));
    addStateChange(StateChange_Background);
    invalidate();
}

void Chart::clearMarkers()
{
    if(m_markers.empty())
        return;

    for(Marker &marker : m_markers)
        delete marker.label;
    m_markers.clear();
    addStateChange(StateChange_Background);
    invalidate();
}

double Chart::getMarkerX(size_t marker) const
{
    return (marker < m_markers.size()) ? m_markers[marker].x : 0.0;
}

String Chart::getMarkerCaption(size_t marker) const
{
    return (marker < m_markers.size()) ? m_markers[marker].caption : String();
}

internal::Text *Chart::getMarkerLabel(size_t marker) const
{
    return (marker < m_markers.size()) ? m_markers[marker].label : nullptr;
}

bool Chart::getMarkerLine(size_t marker, PointF &top, PointF &bottom) const
{
    if(marker >= m_markers.size() || m_plot.w <= 0 || m_plot.h <= 0)
        return false;

    const double x=xPosition(m_markers[marker].x);

    if(!std::isfinite(x) || x < m_plot.x || x > m_plot.x+m_plot.w)
        return false;

    top=PointF(static_cast<float>(x), static_cast<float>(m_plot.y));
    bottom=PointF(static_cast<float>(x), static_cast<float>(m_plot.y+m_plot.h));
    return true;
}

size_t Chart::addSpan(double x0, double x1)
{
    Span span;

    span.x0=std::min(x0, x1);
    span.x1=std::max(x0, x1);
    m_spans.push_back(span);
    addStateChange(StateChange_Background);
    return m_spans.size()-1;
}

void Chart::setSpan(size_t span, double x0, double x1)
{
    if(span >= m_spans.size())
        return;

    const double low=std::min(x0, x1);
    const double high=std::max(x0, x1);

    if(m_spans[span].x0 == low && m_spans[span].x1 == high)
        return;

    m_spans[span].x0=low;
    m_spans[span].x1=high;
    addStateChange(StateChange_Background);
}

void Chart::removeSpan(size_t span)
{
    if(span >= m_spans.size())
        return;

    m_spans.erase(m_spans.begin()+static_cast<std::ptrdiff_t>(span));
    addStateChange(StateChange_Background);
}

void Chart::clearSpans()
{
    if(m_spans.empty())
        return;

    m_spans.clear();
    addStateChange(StateChange_Background);
}

double Chart::getSpanStart(size_t span) const
{
    return (span < m_spans.size()) ? m_spans[span].x0 : 0.0;
}

double Chart::getSpanEnd(size_t span) const
{
    return (span < m_spans.size()) ? m_spans[span].x1 : 0.0;
}

bool Chart::getSpanRect(size_t span, Rect &rect) const
{
    if(span >= m_spans.size() || m_plot.w <= 0 || m_plot.h <= 0)
        return false;

    const double x0=xPosition(m_spans[span].x0);
    const double x1=xPosition(m_spans[span].x1);

    if(!std::isfinite(x0) || !std::isfinite(x1))
        return false;

    // whole units, cut to the plot: a span from far out of range is cut
    // in doubles before it is rounded
    const double left=std::max(x0, double(m_plot.x));
    const double right=std::min(x1, double(m_plot.x+m_plot.w));

    if(!(right > left))
        return false;

    const int l=static_cast<int>(std::lround(left));
    const int r=static_cast<int>(std::lround(right));

    if(r <= l)
        return false;

    rect=Rect(l, m_plot.y, r-l, m_plot.h);
    return true;
}

bool Chart::isMarkerLabel(const Base *child) const
{
    for(const Marker &marker : m_markers)
    {
        if(marker.label == child)
            return true;
    }
    return false;
}

const Rect *Chart::getChildClipRect(const Base *child) const
{
    return isMarkerLabel(child) ? &m_plot : nullptr;
}

void Chart::placeMarkerCaptions()
{
    const int lineHeight=labelLineHeight();

    for(Marker &marker : m_markers)
    {
        if(marker.caption.empty())
        {
            delete marker.label;
            marker.label=nullptr;
            continue;
        }

        if(!marker.label)
        {
            marker.label=makeLabel();
            marker.label->setTextColor(getSkin()->Colors.Chart.MarkerText);
        }
        marker.label->setText(marker.caption);

        PointF top;
        PointF bottom;

        if(!getMarkerLine(size_t(&marker-m_markers.data()), top, bottom))
        {
            marker.label->setHidden(true);
            continue;
        }

        // right of the line, or left of it where the plot would cut it
        const int width=labelWidth(marker.caption);
        const int lineX=static_cast<int>(std::floor(top.x));
        int x=lineX+CaptionGap;

        if(x+width > m_plot.x+m_plot.w)
            x=lineX-CaptionGap-width;
        marker.label->setHidden(false);
        placeChild(marker.label, x, m_plot.y+CaptionInset, width, lineHeight);
    }
}

// ---- the legend ---------------------------------------------------------------

namespace internal
{

GWENI_CONTROL_CONSTRUCTOR(ChartLegendRow)
{
    setTextPadding(Padding(PaddingLeft, PaddingY, PaddingRight, PaddingY));
}

bool ChartLegendRow::isSeriesShown() const
{
    return m_chart && m_chart->isSeriesVisible(m_series);
}

int ChartLegendRow::getColorIndex() const
{
    return m_chart ? m_chart->getSeriesColorIndex(m_series) : 0;
}

Rect ChartLegendRow::getSwatchRect() const
{
    return Rect(SwatchInset, (getHeight()-SwatchSize)/2, SwatchSize, SwatchSize);
}

// A click toggles the series, and never selects the row.
void ChartLegendRow::onPress(event::Info info)
{
    if(isDisabled())
        return;

    if(m_chart)
        m_chart->legendRowPressed(*this);
    Button::onPress(info);
}

void ChartLegendRow::updateColors()
{
    if(!isSeriesShown() && !isDisabled())
    {
        setTextColor(getSkin()->Colors.Selectable.Disabled);
        return;
    }
    ParentClass::updateColors();
}

}//namespace internal

void Chart::setLegendVisible(bool visible)
{
    if(m_legendVisible == visible)
        return;

    m_legendVisible=visible;
    syncLegend();
}

internal::ChartLegendRow *Chart::getLegendRow(size_t series) const
{
    return (series < m_legendRows.size()) ? m_legendRows[series] : nullptr;
}

void Chart::syncLegend()
{
    if(!m_legend)
        return;

    // Hidden and deleted between frames: the row may be the one whose click
    // removed its series, still inside its own press.
    while(m_legendRows.size() > m_series.size())
    {
        internal::ChartLegendRow *row=m_legendRows.back();

        row->setSeries(nullptr, 0);
        row->setHidden(true);
        row->delayedDelete();
        m_legendRows.pop_back();
    }
    while(m_legendRows.size() < m_series.size())
        m_legendRows.push_back(m_legend->newChild<internal::ChartLegendRow>());

    for(size_t i=0; i < m_legendRows.size(); ++i)
    {
        internal::ChartLegendRow *row=m_legendRows[i];

        // a series removed before it moves the rows after it down an index
        const bool moved=(row->getSeries() != i);

        row->setSeries(this, i);
        row->setText(m_series[i].name);
        if(moved)
            row->redraw();
    }

    m_legend->setHidden(!m_legendVisible || m_series.empty());
    invalidate();
}

int Chart::placeLegend(int innerLeft, int innerTop, int innerRight, int innerBottom)
{
    if(!m_legend || m_legend->hidden())
        return 0;

    // As its content asks, measured by this pass (the rows' names, their
    // padding, the scroll control's insets), and no taller than the chart.
    const Size &content=m_legend->getPreferredSize();
    const int height=std::min(content.height, innerBottom-innerTop);
    int width=content.width;

    // a bar that shows takes its thickness from the rows
    if(content.height > height)
        width+=getSkin()->Metrics.ScrollBarThickness;
    width=std::min(width, (innerRight-innerLeft)/3);

    placeChild(m_legend, innerRight-width, innerTop, width, std::max(0, height));
    return width+LegendGap;
}

void Chart::legendRowPressed(internal::ChartLegendRow &row)
{
    const size_t series=row.getSeries();

    if(series >= m_series.size())
        return;

    setSeriesVisible(series, !m_series[series].visible);

    event::Info info(&row);

    info.integer=series;
    onSeriesToggled.call(this, info);
}

// ---- hover and click -----------------------------------------------------------

namespace
{

//! The points of a series sorted by x: the first at or past x.
std::vector<ChartPoint>::const_iterator firstAtOrPast(const std::vector<ChartPoint> &points, double x)
{
    return std::lower_bound(points.begin(), points.end(), x,
        [](const ChartPoint &point, double at) { return point.x < at; });
}

}//namespace

bool Chart::hoverAt(const Point &local, ChartHover &hover) const
{
    ensureData();

    if(m_plot.w <= 0 || m_plot.h <= 0 || local.x < m_plot.x || local.y < m_plot.y
        || local.x >= m_plot.x+m_plot.w || local.y >= m_plot.y+m_plot.h)
        return false;

    const double min=getXMin();
    const double max=getXMax();

    if(!(max > min))
        return false;

    // the unit's middle, in x
    const double at=min+(local.x+0.5-m_plot.x)/m_plot.w*(max-min);
    bool found=false;
    double nearest=0.0;

    for(const Series &series : m_series)
    {
        if(!series.visible || series.points.empty())
            continue;

        const auto next=firstAtOrPast(series.points, at);

        for(auto it : {next, (next == series.points.begin()) ? series.points.end() : next-1})
        {
            if(it == series.points.end())
                continue;
            if(!found || std::fabs(it->x-at) < std::fabs(nearest-at))
            {
                nearest=it->x;
                found=true;
            }
        }
    }

    if(!found || nearest < min || nearest > max)
        return false;

    hover.x=nearest;
    hover.points.assign(m_series.size(), -1);
    for(size_t i=0; i < m_series.size(); ++i)
    {
        const Series &series=m_series[i];

        if(!series.visible)
            continue;

        const auto it=firstAtOrPast(series.points, nearest);

        if(it != series.points.end() && it->x == nearest)
            hover.points[i]=it-series.points.begin();
    }
    return true;
}

bool Chart::pointAt(const Point &local, size_t &series, size_t &index) const
{
    ensureData();

    if(m_plot.w <= 0 || m_plot.h <= 0 || local.x < m_plot.x || local.y < m_plot.y
        || local.x >= m_plot.x+m_plot.w || local.y >= m_plot.y+m_plot.h)
        return false;

    const double min=getXMin();
    const double max=getXMax();

    if(!(max > min))
        return false;

    const double at=min+(local.x+0.5-m_plot.x)/m_plot.w*(max-min);

    // a bar or stacked segment holding the point: the last series is drawn
    // in front, so it is asked first; a bar is at most an interval wide, so
    // only the points either side of the x can hold it
    for(size_t s=m_series.size(); s-- > 0;)
    {
        const Series &one=m_series[s];

        if(!one.visible || !isBar(one.type) || one.points.empty())
            continue;

        const size_t next=size_t(firstAtOrPast(one.points, at)-one.points.begin());

        for(size_t i=(next > 0 ? next-1 : 0); i <= next+1 && i < one.points.size(); ++i)
        {
            Rect rect;

            if(getBarRect(s, i, rect) && local.x >= rect.x && local.x < rect.x+rect.w
                && local.y >= rect.y && local.y < rect.y+rect.h)
            {
                series=s;
                index=i;
                return true;
            }
        }
    }

    // else the nearest line point within the radius
    const double radius=PointHitRadius;
    double best=radius*radius;
    bool found=false;

    for(size_t s=0; s < m_series.size(); ++s)
    {
        const Series &one=m_series[s];

        if(!one.visible || !isLine(one.type) || one.points.empty())
            continue;

        const size_t next=size_t(firstAtOrPast(one.points, at)-one.points.begin());

        for(size_t i=(next > 0 ? next-1 : 0); i <= next && i < one.points.size(); ++i)
        {
            const PointF point=getPointPosition(s, i);

            if(!std::isfinite(point.x) || !std::isfinite(point.y))
                continue;

            const double dx=point.x-(local.x+0.5);
            const double dy=point.y-(local.y+0.5);
            const double distance=dx*dx+dy*dy;

            if(distance <= best)
            {
                best=distance;
                series=s;
                index=i;
                found=true;
            }
        }
    }
    return found;
}

bool Chart::getCrosshairLine(PointF &top, PointF &bottom) const
{
    if(!m_hovering || m_plot.w <= 0 || m_plot.h <= 0)
        return false;

    const double x=xPosition(m_hover.x);

    if(!std::isfinite(x) || x < m_plot.x || x > m_plot.x+m_plot.w)
        return false;

    top=PointF(static_cast<float>(x), static_cast<float>(m_plot.y));
    bottom=PointF(static_cast<float>(x), static_cast<float>(m_plot.y+m_plot.h));
    return true;
}

void Chart::setHoverText(const HoverText &text)
{
    m_hoverText=text;
    if(m_hovering && !m_hoverControl)
        setToolTipText(m_hoverText ? m_hoverText(*this, m_hover) : defaultHoverText(m_hover));
}

void Chart::setHoverControl(Base *control, const HoverFill &fill)
{
    if(m_hoverControl && m_hoverControl != control)
    {
        if(getToolTip() == m_hoverControl)
            setToolTip(nullptr);
        delete m_hoverControl;
    }

    m_hoverControl=control;
    m_hoverFill=fill;
    if(control)
    {
        // kept as the tooltip's control is, a hidden child
        control->setParent(this);
        control->setHidden(true);
    }

    // shown as the next hover asks
    const ChartHover hover=m_hover;
    const bool hovering=m_hovering;

    m_hovering=false;
    setHover(hovering, hover);
}

String Chart::defaultHoverText(const ChartHover &hover) const
{
    String text;

    switch(m_xType)
    {
    case ChartXAxisType::Time:
    {
        // the date, and the time of day where it is not midnight
        const String date=axis::formatTime(hover.x, axis::TimeStep::Day1, m_utcOffset);
        String time=axis::formatTime(hover.x, axis::TimeStep::Second1, m_utcOffset);

        text=date;
        if(time != date && time.size() == 8)
        {
            if(time.compare(5, 3, ":00") == 0)
                time.resize(5);
            text+=" "+time;
        }
        break;
    }
    case ChartXAxisType::Category:
    {
        const double index=std::floor(hover.x+0.5);

        if(index >= 0.0 && index < double(m_categories.size()))
            text=m_categories[size_t(index)];
        break;
    }
    case ChartXAxisType::Value:
    default:
    {
        char value[64];

        std::snprintf(value, sizeof(value), "%g", hover.x);
        text=m_xFormatter ? m_xFormatter(hover.x) : String(value);
        break;
    }
    }

    for(size_t i=0; i < m_series.size() && i < hover.points.size(); ++i)
    {
        if(hover.points[i] < 0)
            continue;

        const Series &series=m_series[i];
        const double y=series.points[size_t(hover.points[i])].y;
        const ValueAxis &yAxis=axisOf(series.type == ChartSeriesType::StackedBar ? getStackAxis() : series.axis);
        char value[64];

        std::snprintf(value, sizeof(value), "%g", y);
        text+="  "+series.name+": "+(yAxis.formatter ? yAxis.formatter(y) : String(value));
    }
    return text;
}

void Chart::setHover(bool hovering, const ChartHover &hover, bool refill)
{
    if(hovering == m_hovering && (!hovering || hover == m_hover) && !(hovering && refill))
        return;

    m_hovering=hovering;
    m_hover=hovering ? hover : ChartHover();
    // the crosshair moved, came or went
    addStateChange(StateChange_Background);

    if(!hovering)
    {
        setToolTipText(String());
        if(m_hoverControl && getToolTip() == m_hoverControl)
            setToolTip(nullptr);
        return;
    }

    if(m_hoverControl)
    {
        if(m_hoverFill)
            m_hoverFill(*this, m_hover, m_hoverControl);
        if(getToolTip() != m_hoverControl)
            setToolTip(m_hoverControl);
        else if(Canvas *canvas=getCanvas())
            canvas->toolTipChanged(this);
        return;
    }
    setToolTipText(m_hoverText ? m_hoverText(*this, m_hover) : defaultHoverText(m_hover));
}

void Chart::refreshHover(bool refill)
{
    if(!m_mouseInside)
        return;

    ChartHover hover;
    const bool hovering=hoverAt(m_mouse, hover);

    setHover(hovering, hover, refill);
}

Rect Chart::getToolTipAnchor() const
{
    PointF top;
    PointF bottom;

    if(!getCrosshairLine(top, bottom))
        return ParentClass::getToolTipAnchor();

    // right of the crosshair, from the plot's top
    const Point at=const_cast<Chart *>(this)->localPosToCanvas(Point(static_cast<int>(std::floor(top.x))+TickGap*2, m_plot.y));

    return Rect(at.x, at.y, 0, 0);
}

void Chart::onMouseMoved(int x, int y, int deltaX, int deltaY)
{
    m_mouse=canvasPosToLocal(Point(x, y));
    m_mouseInside=true;
    refreshHover();
    ParentClass::onMouseMoved(x, y, deltaX, deltaY);
}

void Chart::onMouseLeave()
{
    m_mouseInside=false;
    setHover(false, ChartHover());
    ParentClass::onMouseLeave();
}

void Chart::onMouseClickLeft(int x, int y, bool down)
{
    const Point local=canvasPosToLocal(Point(x, y));
    size_t series=0;
    size_t index=0;

    if(down)
    {
        m_pressed=pointAt(local, series, index);
        m_pressSeries=series;
        m_pressIndex=index;
        // the release comes here wherever the mouse goes
        if(m_pressed)
            MouseFocus=this;
        return;
    }

    if(!m_pressed)
        return;

    m_pressed=false;
    if(MouseFocus == this)
        MouseFocus=nullptr;

    if(!pointAt(local, series, index) || series != m_pressSeries || index != m_pressIndex)
        return;

    m_clickedSeries=series;
    m_clickedIndex=index;

    event::Info info(this);

    info.integer=series;
    info.point=Point(static_cast<int>(index), 0);
    onPointClicked.call(this, info);
}

// ---- data -------------------------------------------------------------------

void Chart::dataChanged(size_t series)
{
    m_dataDirty=true;
    // the renderer redraws what moved
    addStateChange(StateChange_Background);

    // An automatic range, and the ticks and labels from it, are the layout's:
    // with the x range and the series' value axis fixed, nothing it lays out
    // can move.
    ChartAxisSide side=m_series[series].axis;

    // a stacked series is drawn on the first stacked series' axis
    if(m_series[series].type == ChartSeriesType::StackedBar)
    {
        for(const Series &other : m_series)
        {
            if(other.type == ChartSeriesType::StackedBar)
            {
                side=other.axis;
                break;
            }
        }
    }

    if(!m_xFixed || !axisOf(side).fixed)
        invalidate();
    // what the mouse is over may be another point now, or a new value
    refreshHover(true);
}

void Chart::axesChanged()
{
    m_dataDirty=true;
    addStateChange(StateChange_Background);
    invalidate();
    refreshHover(true);
}

void Chart::ensureData() const
{
    if(m_dataDirty)
        const_cast<Chart *>(this)->computeData();
}

void Chart::computeData()
{
    m_dataDirty=false;

    // the bar groups side by side at one x: each unstacked bar series, and
    // the stack in the place of its first series
    int groups=0;
    int stackGroup=-1;

    m_stackSeries=-1;
    for(size_t i=0; i < m_series.size(); ++i)
    {
        Series &series=m_series[i];

        series.barGroup=-1;
        if(series.type == ChartSeriesType::StackedBar && m_stackSeries < 0)
            m_stackSeries=static_cast<int>(i);
        if(!series.visible)
            continue;
        if(series.type == ChartSeriesType::Bar)
            series.barGroup=groups++;
        else if(series.type == ChartSeriesType::StackedBar)
        {
            if(stackGroup < 0)
                stackGroup=groups++;
            series.barGroup=stackGroup;
        }
    }
    m_barGroups=groups;

    const ChartAxisSide stackAxis=(m_stackSeries >= 0) ? m_series[size_t(m_stackSeries)].axis :
        ChartAxisSide::Left;

    // the bar interval: given, a category's, or the smallest step between
    // two neighbouring x of a visible bar series
    if(m_barInterval > 0.0)
        m_effectiveBarInterval=m_barInterval;
    else if(m_xType == ChartXAxisType::Category)
        m_effectiveBarInterval=1.0;
    else
    {
        double smallest=std::numeric_limits<double>::infinity();

        for(const Series &series : m_series)
        {
            if(!series.visible || !isBar(series.type))
                continue;
            for(size_t i=1; i < series.points.size(); ++i)
            {
                const double step=series.points[i].x-series.points[i-1].x;

                if(step > 0.0 && step < smallest)
                    smallest=step;
            }
        }
        m_effectiveBarInterval=std::isfinite(smallest) ? smallest : 1.0;
    }

    // The stacks: at each x of a visible stacked series, in series order,
    // positive values up from 0 and negative ones down; a series with no
    // point at an x adds nothing there.
    std::vector<double> xs;

    for(const Series &series : m_series)
    {
        if(series.visible && series.type == ChartSeriesType::StackedBar)
        {
            for(const ChartPoint &point : series.points)
                xs.push_back(point.x);
        }
    }
    std::sort(xs.begin(), xs.end());
    xs.erase(std::unique(xs.begin(), xs.end()), xs.end());

    std::vector<double> up(xs.size(), 0.0);
    std::vector<double> down(xs.size(), 0.0);

    for(Series &series : m_series)
    {
        if(series.type != ChartSeriesType::StackedBar)
        {
            series.stackBase.clear();
            continue;
        }

        series.stackBase.assign(series.points.size(), 0.0);
        if(!series.visible)
            continue;

        for(size_t i=0; i < series.points.size(); ++i)
        {
            const ChartPoint &point=series.points[i];
            const size_t at=size_t(std::lower_bound(xs.begin(), xs.end(), point.x)-xs.begin());

            if(!std::isfinite(point.y) || point.y >= 0.0)
            {
                series.stackBase[i]=up[at];
                if(std::isfinite(point.y))
                    up[at]+=point.y;
            }
            else
            {
                series.stackBase[i]=down[at];
                down[at]+=point.y;
            }
        }
    }

    // the extents: x over every visible series, a value axis's over the
    // visible series drawn against it
    for(ValueAxis &yAxis : m_yAxes)
    {
        yAxis.used=false;
        yAxis.hasData=false;
    }

    bool anyX=false;
    bool bars=false;
    double xMin=0.0;
    double xMax=0.0;

    for(const Series &series : m_series)
    {
        const ChartAxisSide side=(series.type == ChartSeriesType::StackedBar) ? stackAxis : series.axis;
        ValueAxis &yAxis=axisOf(side);

        yAxis.used=true;
        if(!series.visible || series.points.empty())
            continue;

        if(!anyX)
        {
            xMin=series.points.front().x;
            xMax=series.points.back().x;
            anyX=true;
        }
        else
        {
            xMin=std::min(xMin, series.points.front().x);
            xMax=std::max(xMax, series.points.back().x);
        }

        auto extend=[&yAxis](double value)
        {
            if(!yAxis.hasData)
            {
                yAxis.dataMin=value;
                yAxis.dataMax=value;
                yAxis.hasData=true;
                return;
            }
            yAxis.dataMin=std::min(yAxis.dataMin, value);
            yAxis.dataMax=std::max(yAxis.dataMax, value);
        };

        if(isBar(series.type))
        {
            bars=true;
            // a bar grows from 0, or from its stack
            extend(0.0);
        }

        for(size_t i=0; i < series.points.size(); ++i)
        {
            const double y=series.points[i].y;

            if(!std::isfinite(y))
                continue;
            if(series.type == ChartSeriesType::StackedBar)
            {
                extend(series.stackBase[i]);
                extend(series.stackBase[i]+y);
            }
            else
                extend(y);
        }
    }

    if(m_xType == ChartXAxisType::Category)
    {
        double count=static_cast<double>(m_categories.size());

        if(anyX)
            count=std::max(count, std::floor(xMax)+1.0);
        m_dataXMin=-0.5;
        m_dataXMax=std::max(count, 1.0)-0.5;
        return;
    }

    if(!anyX)
    {
        m_dataXMin=0.0;
        m_dataXMax=1.0;
        return;
    }

    if(bars)
    {
        xMin-=m_effectiveBarInterval/2.0;
        xMax+=m_effectiveBarInterval/2.0;
    }
    if(!(xMax > xMin))
    {
        xMin-=0.5;
        xMax+=0.5;
    }
    m_dataXMin=xMin;
    m_dataXMax=xMax;
}

// ---- geometry ---------------------------------------------------------------

double Chart::xPosition(double x) const
{
    const double min=getXMin();
    const double max=getXMax();

    if(!(max > min))
        return m_plot.x;
    return m_plot.x+(x-min)/(max-min)*m_plot.w;
}

double Chart::yPosition(ChartAxisSide side, double y) const
{
    const double min=getYMin(side);
    const double max=getYMax(side);
    const double bottom=m_plot.y+m_plot.h;

    if(!(max > min))
        return bottom;
    return bottom-(y-min)/(max-min)*m_plot.h;
}

ChartAxisSide Chart::getStackAxis() const
{
    ensureData();
    return (m_stackSeries >= 0) ? m_series[size_t(m_stackSeries)].axis : ChartAxisSide::Left;
}

int Chart::getBarGroupCount() const
{
    ensureData();
    return m_barGroups;
}

int Chart::getBarGroup(size_t series) const
{
    ensureData();

    const Series *s=seriesAt(series);

    return s ? s->barGroup : -1;
}

double Chart::getStackBase(size_t series, size_t index) const
{
    ensureData();

    const Series *s=seriesAt(series);

    if(!s || index >= s->stackBase.size())
        return 0.0;
    return s->stackBase[index];
}

PointF Chart::getPointPosition(size_t series, size_t index) const
{
    ensureData();

    const Series *s=seriesAt(series);

    if(!s || index >= s->points.size())
        return PointF();

    const ChartPoint &point=s->points[index];
    const float x=static_cast<float>(xPosition(point.x));

    if(!std::isfinite(point.y))
        return PointF(x, NaNf);
    if(s->type == ChartSeriesType::StackedBar)
        return PointF(x, static_cast<float>(yPosition(getStackAxis(), s->stackBase[index]+point.y)));
    return PointF(x, static_cast<float>(yPosition(s->axis, point.y)));
}

size_t Chart::getVertexCount(size_t series) const
{
    const Series *s=seriesAt(series);

    if(!s || !isLine(s->type) || s->points.empty())
        return 0;
    if(s->type == ChartSeriesType::StepLine)
        return 2*s->points.size()-1;
    return s->points.size();
}

bool Chart::getVertex(size_t series, size_t vertex, double &x, double &y) const
{
    ensureData();

    const Series *s=seriesAt(series);

    x=std::numeric_limits<double>::quiet_NaN();
    y=x;
    if(!s || vertex >= getVertexCount(series))
        return false;

    // a step line: point i at 2i, and at 2i+1 the next point's x at its y
    const size_t index=(s->type == ChartSeriesType::StepLine) ? vertex/2 : vertex;
    const bool riser=(s->type == ChartSeriesType::StepLine) && (vertex%2 == 1);
    const ChartPoint &point=s->points[index];

    x=xPosition(riser ? s->points[index+1].x : point.x);
    if(!std::isfinite(point.y))
        return false;
    y=yPosition(s->axis, point.y);
    return std::isfinite(y);
}

bool Chart::getVertex(size_t series, size_t vertex, PointF &point) const
{
    double x;
    double y;
    const bool shown=getVertex(series, vertex, x, y);

    point=PointF(static_cast<float>(x), shown ? static_cast<float>(y) : NaNf);
    return shown;
}

bool Chart::getBarRect(size_t series, size_t index, Rect &rect) const
{
    ensureData();

    const Series *s=seriesAt(series);

    if(!s || !s->visible || !isBar(s->type) || index >= s->points.size() || s->barGroup < 0)
        return false;

    const ChartPoint &point=s->points[index];

    if(!std::isfinite(point.y) || point.y == 0.0)
        return false;

    const bool stacked=(s->type == ChartSeriesType::StackedBar);
    const ChartAxisSide side=stacked ? getStackAxis() : s->axis;
    const double base=stacked ? s->stackBase[index] : 0.0;
    const double min=getXMin();
    const double max=getXMax();

    if(!(max > min) || m_barGroups <= 0)
        return false;

    // the groups share BarFill of the interval, centred on x
    const double band=m_effectiveBarInterval/(max-min)*m_plot.w;
    const double groupWidth=band*BarFill;
    const double slot=groupWidth/m_barGroups;
    const double left=xPosition(point.x)-groupWidth/2.0+s->barGroup*slot;
    // bars side by side keep a unit between them where they have room
    const int gap=(m_barGroups > 1 && slot >= 4.0) ? 1 : 0;
    const int x0=static_cast<int>(std::lround(left));
    int x1=static_cast<int>(std::lround(left+slot))-gap;

    if(x1 <= x0)
        x1=x0+1;

    const double yBase=yPosition(side, base);
    const double yTop=yPosition(side, base+point.y);

    if(!std::isfinite(yBase) || !std::isfinite(yTop))
        return false;

    int y0=static_cast<int>(std::lround(std::min(yBase, yTop)));
    int y1=static_cast<int>(std::lround(std::max(yBase, yTop)));

    // a value too small to reach a unit still shows one
    if(y1 == y0)
    {
        if(point.y > 0.0)
            y0=y1-1;
        else
            y1=y0+1;
    }

    // cut to the plot
    const int left0=std::max(x0, m_plot.x);
    const int right0=std::min(x1, m_plot.x+m_plot.w);
    const int top0=std::max(y0, m_plot.y);
    const int bottom0=std::min(y1, m_plot.y+m_plot.h);

    if(right0 <= left0 || bottom0 <= top0)
        return false;

    rect=Rect(left0, top0, right0-left0, bottom0-top0);
    return true;
}

// ---- measures ---------------------------------------------------------------

void Chart::renewMeasureKey()
{
    const Font &font=getSkin()->getDefaultFont();
    const renderer::Base *renderer=getSkin()->getRenderer();
    const unsigned serial=renderer ? renderer->textLayoutSerial() : 0;

    if(m_measureFont == &font && m_measureFontValue == font && m_measureRenderer == renderer
        && m_measureSerial == serial)
        return;

    m_measureFont=&font;
    m_measureFontValue=font;
    m_measureRenderer=renderer;
    m_measureSerial=serial;
    m_widths.clear();
    m_lineHeight=-1;
}

int Chart::labelWidth(const String &text)
{
    const auto found=m_widths.find(text);

    if(found != m_widths.end())
        return found->second;

    // the time axis tries many strings as its range moves: keep it bounded
    if(m_widths.size() >= 1024)
        m_widths.clear();

    const int width=measureText(getSkin()->getDefaultFont(), text).x;

    m_widths.emplace(text, width);
    return width;
}

int Chart::labelLineHeight()
{
    if(m_lineHeight < 0)
        m_lineHeight=measureLineHeight(getSkin()->getDefaultFont());
    return m_lineHeight;
}

// ---- layout -----------------------------------------------------------------

internal::Text *Chart::makeLabel()
{
    internal::Text *label=nullptr;

    {
        PartScope part;

        label=newChild<internal::Text>();
    }
    label->setPlacement(Placement::Owner);
    label->setTextColor(getSkin()->Colors.Chart.AxisText);
    return label;
}

void Chart::updateColors()
{
    const Color &color=getSkin()->Colors.Chart.AxisText;

    for(internal::Text *label : m_xLabels)
        label->setTextColor(color);
    for(ValueAxis &yAxis : m_yAxes)
    {
        for(internal::Text *label : yAxis.labels)
            label->setTextColor(color);
        if(yAxis.titleLabel)
            yAxis.titleLabel->setTextColor(color);
    }
    if(m_xTitleLabel)
        m_xTitleLabel->setTextColor(color);
    for(Marker &marker : m_markers)
    {
        if(marker.label)
            marker.label->setTextColor(getSkin()->Colors.Chart.MarkerText);
    }
}

void Chart::resizeLabels(std::vector<internal::Text *> &labels, size_t count)
{
    while(labels.size() > count)
    {
        delete labels.back();   // ~Base takes it out of the children
        labels.pop_back();
    }
    while(labels.size() < count)
        labels.push_back(makeLabel());
}

void Chart::setTitleLabel(internal::Text *&label, const String &title)
{
    if(title.empty())
    {
        delete label;
        label=nullptr;
        return;
    }

    if(!label)
        label=makeLabel();
    label->setText(title);
}

void Chart::computeXTicks()
{
    const double min=getXMin();
    const double max=getXMax();
    const double length=m_plot.w;
    std::vector<axis::Tick> ticks;

    m_xTicks.clear();
    if(!(max > min) || length <= 0.0)
        return;

    switch(m_xType)
    {
    case ChartXAxisType::Time:
    {
        axis::TimeTicks time=axis::chooseTimeTicks(min, max, length,
            [this](const String &label) { return double(labelWidth(label)); }, XLabelGap, m_utcOffset);

        m_timeStep=time.step;
        ticks=std::move(time.ticks);
        break;
    }
    case ChartXAxisType::Value:
    {
        const double step=axis::niceStep(max-min, std::max(1, m_plot.w/XTickSpacing));

        ticks=axis::valueAxisTicks(min, max, step, length, m_xFormatter);
        break;
    }
    case ChartXAxisType::Category:
    {
        const double first=std::max(0.0, std::ceil(min));
        const double last=std::floor(max);

        for(double i=first; i <= last; i+=1.0)
        {
            axis::Tick tick;
            const size_t index=static_cast<size_t>(i);

            tick.value=i;
            tick.position=axis::positionOf(i, min, max, length);
            if(index < m_categories.size())
                tick.label=m_categories[index];
            ticks.push_back(std::move(tick));
        }
        break;
    }
    }

    // every stride-th label, so none overlaps the next
    std::vector<double> positions;
    std::vector<double> widths;

    positions.reserve(ticks.size());
    widths.reserve(ticks.size());
    for(const axis::Tick &tick : ticks)
    {
        positions.push_back(tick.position);
        widths.push_back(labelWidth(tick.label));
    }

    const size_t stride=axis::thinLabels(positions, widths, XLabelGap);

    for(size_t i=0; i < ticks.size(); i+=stride)
    {
        ticks[i].position+=m_plot.x;
        m_xTicks.push_back(std::move(ticks[i]));
    }
}

void Chart::layout(skin::Base *skin)
{
    ParentClass::layout(skin);
    ensureData();
    renewMeasureKey();

    // the labels are children laid out next: what is written to them is
    // placement
    PlacementScope placement;

    const int lineHeight=labelLineHeight();
    const Padding &padding=getPadding();
    const int innerLeft=padding.left;
    const int innerTop=padding.top;
    const int innerBottom=std::max(innerTop, getHeight()-padding.bottom);
    // the legend's column comes off the right
    const int controlRight=std::max(innerLeft, getWidth()-padding.right);
    const int innerRight=std::max(innerLeft,
        controlRight-placeLegend(innerLeft, innerTop, controlRight, innerBottom));
    const bool shown[2]={isYAxisShown(ChartAxisSide::Left), isYAxisShown(ChartAxisSide::Right)};
    const bool titles=(shown[0] && !m_yAxes[0].title.empty()) || (shown[1] && !m_yAxes[1].title.empty());

    // the y titles' line above, half a line for the top tick's label, the x
    // labels' row and the x title's line below
    const int top=innerTop+(titles ? lineHeight : 0)+lineHeight/2;
    const int bottom=innerBottom-lineHeight-TickGap-(m_xTitle.empty() ? 0 : lineHeight);
    const int plotHeight=std::max(0, bottom-top);
    const int maxTicks=std::max(1, plotHeight/YTickSpacing);
    int widths[2]={0, 0};

    for(int i=0; i < 2; ++i)
    {
        ValueAxis &yAxis=m_yAxes[i];

        if(yAxis.fixed)
        {
            yAxis.range.min=yAxis.fixedMin;
            yAxis.range.max=yAxis.fixedMax;
            yAxis.range.step=axis::niceStep(yAxis.fixedMax-yAxis.fixedMin, maxTicks);
        }
        else
        {
            double low=yAxis.hasData ? yAxis.dataMin : 0.0;
            double high=yAxis.hasData ? yAxis.dataMax : 0.0;

            if(yAxis.includeZero)
            {
                low=std::min(low, 0.0);
                high=std::max(high, 0.0);
            }
            yAxis.range=axis::niceRange(low, high, maxTicks);
        }

        yAxis.ticks.clear();
        if(!shown[i])
            continue;

        yAxis.ticks=axis::valueAxisTicks(yAxis.range.min, yAxis.range.max, yAxis.range.step, plotHeight,
            yAxis.formatter);
        for(axis::Tick &tick : yAxis.ticks)
        {
            tick.position=top+plotHeight-tick.position;
            widths[i]=std::max(widths[i], labelWidth(tick.label));
        }
    }

    const int left=innerLeft+(shown[0] ? widths[0]+TickGap : 0);
    const int right=std::max(left, innerRight-(shown[1] ? widths[1]+TickGap : 0));

    const Rect plot(left, top, right-left, plotHeight);

    // a new plot is a new clip for the captions
    if(!(plot == m_plot))
    {
        for(Marker &marker : m_markers)
        {
            if(marker.label)
                marker.label->addStateChangeRecursive(StateChange_Geometry);
        }
    }
    m_plot=plot;
    computeXTicks();

    // the value labels, right of the left column and left of the right one,
    // centred on their ticks inside the control
    auto clampY=[&](int y) { return std::max(innerTop, std::min(y, innerBottom-lineHeight)); };

    for(int i=0; i < 2; ++i)
    {
        ValueAxis &yAxis=m_yAxes[i];

        resizeLabels(yAxis.labels, yAxis.ticks.size());
        for(size_t t=0; t < yAxis.ticks.size(); ++t)
        {
            internal::Text *label=yAxis.labels[t];
            const axis::Tick &tick=yAxis.ticks[t];
            const int width=labelWidth(tick.label);
            const int x=(i == 0) ? m_plot.x-TickGap-width : m_plot.x+m_plot.w+TickGap;
            const int y=clampY(static_cast<int>(std::lround(tick.position))-lineHeight/2);

            label->setText(tick.label);
            placeChild(label, x, y, width, lineHeight);
        }
    }

    // the x labels under the plot, centred on their ticks inside the control
    resizeLabels(m_xLabels, m_xTicks.size());
    for(size_t t=0; t < m_xTicks.size(); ++t)
    {
        internal::Text *label=m_xLabels[t];
        const axis::Tick &tick=m_xTicks[t];
        const int width=labelWidth(tick.label);
        int x=static_cast<int>(std::lround(tick.position))-width/2;

        x=std::max(innerLeft, std::min(x, innerRight-width));
        label->setText(tick.label);
        placeChild(label, x, m_plot.y+m_plot.h+TickGap, width, lineHeight);
    }

    // the titles: the value axes' above their columns, the x axis's centred
    // under its labels
    for(int i=0; i < 2; ++i)
    {
        ValueAxis &yAxis=m_yAxes[i];

        setTitleLabel(yAxis.titleLabel, shown[i] ? yAxis.title : String());
        if(!yAxis.titleLabel)
            continue;

        const int width=labelWidth(yAxis.title);
        const int x=(i == 0) ? innerLeft : std::max(innerLeft, innerRight-width);

        placeChild(yAxis.titleLabel, x, innerTop, width, lineHeight);
    }

    setTitleLabel(m_xTitleLabel, m_xTitle);
    if(m_xTitleLabel)
    {
        const int width=labelWidth(m_xTitle);
        const int x=std::max(innerLeft, m_plot.x+(m_plot.w-width)/2);

        placeChild(m_xTitleLabel, x, innerBottom-lineHeight, width, lineHeight);
    }

    placeMarkerCaptions();
    // the plot may have moved under the mouse
    refreshHover();

    // the renderer draws from what this computed
    addStateChange(StateChange_Background);
}

// The x ticks need the plot's width, which needs the value labels' widths,
// which need the plot's height: everything is computed once both are final,
// at the Y arrange, and the labels are laid out then, X and Y, as a
// ScrollControl lays out its content (docs/layout.md, "Children whose X
// layout needs the parent's height").
void Chart::arrange(skin::Base *skin, Dim dim)
{
    if(dim == Dim::X)
        return;

    stampLayoutPass();

    if(needsLayout())
    {
        m_needsLayout=false;
        layout(skin);
    }

    countLayoutChildrenScanned(m_children);
    for(auto &&child : m_children)
    {
        if(child->hidden())
            continue;
        // a label made by layout() was not measured this pass
        child->doCalculateSize(skin, Dim::X);
        child->doArrange(skin, Dim::X);
    }

    countLayoutChildrenScanned(m_children);
    for(auto &&child : m_children)
    {
        if(!child->hidden())
            child->doCalculateSize(skin, Dim::Y);
    }

    countLayoutChildrenScanned(m_children);
    for(auto &&child : m_children)
    {
        if(!child->hidden())
            child->doArrange(skin, Dim::Y);
    }

    postLayout(skin);
}

}//namespace controls
}//namespace gweni
