/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_chart_h_
#define _gweni_controls_chart_h_

#include <gweni/controls/base.h>
#include <gweni/controls/chartAxis.h>

#include <unordered_map>
#include <vector>

namespace gweni
{
namespace controls
{
namespace internal
{
class Text;
}

//! How a Chart series is drawn.
enum class ChartSeriesType
{
    Line,       //!< its points joined by straight segments
    StepLine,   //!< each point's y held to the next point's x, then the step
    Bar,        //!< a bar from 0 per point, beside the other series' bars
    StackedBar  //!< a bar per point on the stack of the stacked series at its x
};

//! A value axis: left or right of the plot.
enum class ChartAxisSide
{
    Left=0,
    Right=1
};

//! What the x values are.
enum class ChartXAxisType
{
    Time,       //!< epoch seconds, ticked at axis::chooseTimeTicks's steps
    Value,      //!< plain numbers, ticked at 1, 2 or 5 times a power of ten
    Category    //!< category indices 0..n-1 (setCategories()), one band each
};

//! One sample of a series.
struct ChartPoint
{
    double x=0.0;
    double y=0.0;

    ChartPoint() {}
    ChartPoint(double x_, double y_): x(x_), y(y_) {}
    bool operator==(const ChartPoint &other) const { return x == other.x && y == other.y; }
};

//! A multi-series chart: line, step-line, bar and stacked-bar series over a
//! time, value or category x axis, with a value axis on the left and one on
//! the right.
//!
//! Each series has its own points (doubles, kept sorted by x: a point added
//! out of order is put in its place) and names the value axis it is drawn
//! against. Bars are centred on their x, getBarInterval() wide less a gap,
//! and the unstacked bar series and the stack (every stacked series, which
//! form one group) sit side by side at one x, in the order of their first
//! series. Stacked bars stack by equal x in series order, positive values up
//! from 0 and negative ones down, and a series with no point at an x adds
//! nothing there (it counts as 0); they are drawn against the axis of the
//! first stacked series. A NaN y is a gap in a line and no bar.
//!
//! The x range is setXRange()'s, or automatic: the visible series' x extent,
//! widened by half a bar interval each way when a bar series shows (and by
//! half a unit each way when the extent is a single x), and for a category
//! axis -0.5 to n-0.5. A value axis's range is setYRange()'s, or automatic:
//! the extent of the visible series drawn against it (with 0 unless
//! setYIncludesZero(false), and always with a bar series' 0), widened to
//! whole steps (axis::niceRange) of a step chosen for the plot's height. A
//! value axis shows its labels when a series names it, or the left one when
//! none names either.
//!
//! The plot is the control less its padding, the columns of the value axes'
//! labels, the row of the x labels and the axis titles' lines; its x runs
//! from the x range's min at its left edge to its max at its right edge, a
//! value axis's from its min at the bottom to its max at the top. Points
//! outside a range are where the range puts them, outside the plot: the
//! renderer clips them to it, never clamps them.
//!
//! The tick labels are pooled internal::Text children placed by the chart
//! (Placement::Owner), set only when their string changes. The chart lays out
//! its children in its Y arrange, once its width and height are final (the x
//! ticks need the plot's width, which needs the value labels' widths, which
//! need the plot's height: ScrollControl's pattern, docs/layout.md).
//!
//! A change of the data marks StateChange_Background, and lays the chart out
//! again only when an automatic range can move with it; a series on a fixed
//! value axis of a chart with a fixed x range draws a new point with no
//! layout.
class GWENI_EXPORT Chart: public Base
{
public:

    GWENI_CONTROL(Chart, Base);

public:

    ~Chart() override;

    //! Space between a tick label and the plot, and between two x labels.
    static constexpr int TickGap=4;
    static constexpr int XLabelGap=12;
    //! The value axes aim at a tick every this many units of the plot's
    //! height (at least one interval); a value x axis every XTickSpacing.
    static constexpr int YTickSpacing=32;
    static constexpr int XTickSpacing=80;
    //! The share of a bar interval the bars at one x take.
    static constexpr double BarFill=0.8;

    // ---- series ---------------------------------------------------------

    //! Adds a series, last, and returns its index. Its colour is the theme's
    //! ChartSeries<index % 8> unless setSeriesColorIndex() says otherwise.
    //! The renderer draws each series in a z layer of its own
    //! (skin::Control::depthLayersOf()), so adding or removing one renumbers
    //! the chart's range (Base::zOrderChanged()).
    size_t addSeries(const String &name, ChartSeriesType type,
                     ChartAxisSide axis=ChartAxisSide::Left);
    //! Removes series \p series; the ones after it move down an index.
    void removeSeries(size_t series);
    void clearSeries();
    size_t getSeriesCount() const { return m_series.size(); }

    void setSeriesName(size_t series, const String &name);
    String getSeriesName(size_t series) const;
    ChartSeriesType getSeriesType(size_t series) const;
    void setSeriesAxis(size_t series, ChartAxisSide axis);
    ChartAxisSide getSeriesAxis(size_t series) const;
    //! A hidden series is not drawn, and counts in no automatic range, stack
    //! or bar group.
    void setSeriesVisible(size_t series, bool visible);
    bool isSeriesVisible(size_t series) const;
    //! The theme colour a series is drawn in, ChartSeries<index % 8>.
    void setSeriesColorIndex(size_t series, int index);
    int getSeriesColorIndex(size_t series) const;
    //! A line's thickness in units (2 by default); one not above 0 is 1.
    void setSeriesLineThickness(size_t series, float thickness);
    float getSeriesLineThickness(size_t series) const;

    //! Replaces the series' points, sorted by x (equal x keep their order).
    void setSeriesPoints(size_t series, std::vector<ChartPoint> points);
    //! Adds a point after every point whose x is not above \p x: at the end
    //! for an x past the last one.
    void appendPoint(size_t series, double x, double y);
    //! Gives point \p index a new y; an index past the series is ignored.
    void setPointValue(size_t series, size_t index, double y);
    //! Removes the first \p count points (a sliding window's oldest).
    void removeFirstPoints(size_t series, size_t count);
    const std::vector<ChartPoint> &getSeriesPoints(size_t series) const;

    // ---- the x axis -----------------------------------------------------

    void setXAxisType(ChartXAxisType type);
    ChartXAxisType getXAxisType() const { return m_xType; }
    //! The category names, for a Category axis: category i at x = i.
    void setCategories(const std::vector<String> &categories);
    const std::vector<String> &getCategories() const { return m_categories; }

    //! A fixed x range (a max not above min is taken as min+1).
    void setXRange(double min, double max);
    //! Back to the automatic x range.
    void setXAutoRange();
    bool hasFixedXRange() const { return m_xFixed; }
    //! The x range drawn, fixed or automatic.
    double getXMin() const;
    double getXMax() const;

    //! The x distance a bar's interval spans: \p interval, or with 0 (the
    //! default) automatic, 1 on a category axis, else the smallest distance
    //! between two neighbouring x of a visible bar series (1 with none).
    void setBarInterval(double interval);
    double getBarInterval() const;

    //! The time axis's zone (axis::localUtcOffset() by default); an empty one
    //! is UTC.
    void setUtcOffset(const axis::UtcOffset &offset);
    const axis::UtcOffset &getUtcOffset() const { return m_utcOffset; }
    //! A Value x axis's labels in place of axis::formatValue's.
    void setXFormatter(const axis::ValueFormatter &formatter);
    //! Vertical grid lines at the x ticks (off by default).
    void setXGrid(bool grid);
    bool getXGrid() const { return m_xGrid; }
    //! A title centred under the x labels; empty for none.
    void setXTitle(const String &title);

    // ---- the value axes -------------------------------------------------

    //! A fixed range for the axis on \p side (a max not above min is taken as
    //! min+1).
    void setYRange(ChartAxisSide side, double min, double max);
    void setYAutoRange(ChartAxisSide side);
    bool hasFixedYRange(ChartAxisSide side) const { return axisOf(side).fixed; }
    //! The range drawn, as last laid out (the fixed one at once).
    double getYMin(ChartAxisSide side) const;
    double getYMax(ChartAxisSide side) const;
    //! The tick step as last laid out.
    double getYStep(ChartAxisSide side) const { return axisOf(side).range.step; }
    //! Whether an automatic range reaches 0 (true by default; a bar always
    //! does).
    void setYIncludesZero(ChartAxisSide side, bool include);
    //! Horizontal grid lines at the axis's ticks: on for the left axis, off
    //! for the right one, by default.
    void setYGrid(ChartAxisSide side, bool grid);
    bool getYGrid(ChartAxisSide side) const { return axisOf(side).grid; }
    //! The axis's labels in place of axis::formatValue's.
    void setYFormatter(ChartAxisSide side, const axis::ValueFormatter &formatter);
    //! A title above the axis's labels; empty for none.
    void setYTitle(ChartAxisSide side, const String &title);
    //! Whether the axis's labels show (see the class comment).
    bool isYAxisShown(ChartAxisSide side) const;

    // ---- geometry, in this control's own coordinates (top-left at 0,0) ---

    //! The plot, as last laid out.
    Rect getPlotBounds() const { return m_plot; }
    //! Where x and a value on the axis of \p side are, as last laid out.
    double xPosition(double x) const;
    double yPosition(ChartAxisSide side, double y) const;
    //! Point \p index of a series; its y is NaN for a NaN value. For a
    //! stacked bar, the top of its segment.
    PointF getPointPosition(size_t series, size_t index) const;
    //! A line's polyline: its points, or for a step line each point and then
    //! (x of the next point, its own y), 2n-1 of them. False (and NaN) for a
    //! vertex of a NaN value.
    size_t getVertexCount(size_t series) const;
    bool getVertex(size_t series, size_t vertex, PointF &point) const;
    //! The same in doubles, which a point far out of range still holds
    //! exactly enough to be cut to the plot.
    bool getVertex(size_t series, size_t vertex, double &x, double &y) const;
    //! A bar's rect (a bar or stacked bar series' point), whole units, cut to
    //! the plot: false for a point of no other type, a hidden series, a NaN
    //! or 0 value, or a bar wholly outside the plot.
    bool getBarRect(size_t series, size_t index, Rect &rect) const;
    //! The value a stacked bar's segment starts at (0 for any other point).
    double getStackBase(size_t series, size_t index) const;
    //! The axis on which stacked bars are drawn: the first stacked series'.
    ChartAxisSide getStackAxis() const;
    //! The groups of bars side by side at one x, and series' group (-1 for a
    //! hidden series or one that is no bar).
    int getBarGroupCount() const;
    int getBarGroup(size_t series) const;

    //! The ticks, as last laid out, their positions in this control's
    //! coordinates (x across for the x axis, y down for a value axis); only
    //! the ticks whose labels show (thinned so they do not overlap).
    const std::vector<axis::Tick> &getXTicks() const { return m_xTicks; }
    const std::vector<axis::Tick> &getYTicks(ChartAxisSide side) const { return axisOf(side).ticks; }
    //! The time axis's step as last laid out.
    axis::TimeStep getTimeStep() const { return m_timeStep; }
    //! The pooled labels, as last laid out (one per tick, in order).
    const std::vector<internal::Text *> &getXLabels() const { return m_xLabels; }
    const std::vector<internal::Text *> &getYLabels(ChartAxisSide side) const { return axisOf(side).labels; }

    //! A chart is the size it was asked to be (its constructor's 400x240,
    //! or the app's): its series are drawn by its skin, its labels placed by
    //! it.
    bool sizedByRequest(Dim /*dim*/) const override { return true; }

    //! The labels in the skin's Colors.Chart.AxisText.
    void updateColors() override;

protected:

    //! The X arrange leaves the children alone; the Y arrange computes the
    //! layout (layout()) and lays them out, X then Y.
    void arrange(skin::Base *skin, Dim dim) override;
    void layout(skin::Base *skin) override;

    struct Series
    {
        String name;
        ChartSeriesType type=ChartSeriesType::Line;
        ChartAxisSide axis=ChartAxisSide::Left;
        bool visible=true;
        int colorIndex=0;
        float thickness=2.0f;
        std::vector<ChartPoint> points;
        //! a stacked series' segment starts, one per point
        std::vector<double> stackBase;
        int barGroup=-1;
    };

    struct ValueAxis
    {
        bool fixed=false;
        double fixedMin=0.0;
        double fixedMax=1.0;
        bool grid=false;
        bool includeZero=true;
        axis::ValueFormatter formatter;
        String title;

        //! from the data (ensureData)
        bool used=false;
        bool hasData=false;
        double dataMin=0.0;
        double dataMax=0.0;

        //! as last laid out
        axis::ValueRange range;
        std::vector<axis::Tick> ticks;
        std::vector<internal::Text *> labels;
        internal::Text *titleLabel=nullptr;
    };

    ValueAxis &axisOf(ChartAxisSide side) { return m_yAxes[static_cast<int>(side)]; }
    const ValueAxis &axisOf(ChartAxisSide side) const { return m_yAxes[static_cast<int>(side)]; }
    const Series *seriesAt(size_t series) const { return series < m_series.size() ? &m_series[series] : nullptr; }

    //! Series \p series's data changed: marks the chart, and lays it out
    //! again when the x range or the series' value axis is automatic.
    void dataChanged(size_t series);
    //! A change of the axes' settings: lays the chart out again.
    void axesChanged();
    //! Stacks, bar groups and the data's extents, from the points.
    void ensureData() const;
    void computeData();

    //! A label's width and the labels' line height, measured once per string
    //! for the font, renderer and text layout they were measured with.
    int labelWidth(const String &text);
    int labelLineHeight();
    void renewMeasureKey();

    //! The pool grown or shrunk to \p count labels.
    void resizeLabels(std::vector<internal::Text *> &labels, size_t count);
    internal::Text *makeLabel();
    void setTitleLabel(internal::Text *&label, const String &title);

    void computeXTicks();

    std::vector<Series> m_series;
    ValueAxis m_yAxes[2];

    ChartXAxisType m_xType=ChartXAxisType::Time;
    std::vector<String> m_categories;
    bool m_xFixed=false;
    double m_xFixedMin=0.0;
    double m_xFixedMax=1.0;
    double m_barInterval=0.0;
    axis::UtcOffset m_utcOffset;
    axis::ValueFormatter m_xFormatter;
    bool m_xGrid=false;
    String m_xTitle;
    internal::Text *m_xTitleLabel=nullptr;

    //! from the data (ensureData)
    mutable bool m_dataDirty=true;
    double m_dataXMin=0.0;
    double m_dataXMax=1.0;
    double m_effectiveBarInterval=1.0;
    int m_barGroups=0;
    int m_stackSeries=-1;

    //! as last laid out
    Rect m_plot;
    std::vector<axis::Tick> m_xTicks;
    axis::TimeStep m_timeStep=axis::TimeStep::Day1;
    std::vector<internal::Text *> m_xLabels;

    //! the measures' key and cache
    const Font *m_measureFont=nullptr;
    Font m_measureFontValue;
    const renderer::Base *m_measureRenderer=nullptr;
    unsigned m_measureSerial=0;
    int m_lineHeight=-1;
    std::unordered_map<String, int> m_widths;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_chart_h_
