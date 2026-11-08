/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_plothistogram_h_
#define _gweni_controls_plothistogram_h_

#include <gweni/controls/base.h>

#include <vector>

namespace gweni
{
namespace controls
{
namespace internal
{
class Text;
}

//! A series of values drawn as bars from the bottom of a frame (imgui's
//! PlotHistogram), with an optional caption at the top centre (imgui's
//! overlay_text) and a tooltip naming the hovered bar's index and value.
//!
//! The plot is the control less its padding (getPlotBounds()). Bar i
//! takes the columns [i*W/N, (i+1)*W/N) of its W units, rounded down, so the
//! remainder is spread one unit at a time across the row and no bar is more
//! than one unit wider than another; a bar leaves its column's last unit
//! empty as the gap to the next when the column is 2 units wide or more. A
//! bar's height is its value's share of the scale range times the plot's
//! height, rounded to a whole unit and clamped to the plot: a value at or
//! below the range's minimum (or not a number) draws nothing, one past its
//! maximum fills the column. A bar with no height or no width is hidden.
//!
//! The textured skin draws the frame and one primitive slot per bar, and
//! redraws only the bars whose rect or look changed: a setValue() whose bar
//! keeps its height marks nothing, one that changes it rewrites that bar's
//! slot alone, and a rescale redraws every bar that moved.
class GWENI_EXPORT PlotHistogram: public Base
{
public:

    GWENI_CONTROL(PlotHistogram, Base);

public:

    //! The series, one bar per value. Marks StateChange_Background unless the
    //! values are the ones it has.
    virtual void setValues(const std::vector<float> &values);
    //! One value; an index past the series is ignored. Marks
    //! StateChange_Background only when the bar's height changes, or the
    //! automatic range does.
    virtual void setValue(size_t index, float value);
    const std::vector<float> &getValues() const { return m_values; }
    size_t getValueCount() const { return m_values.size(); }

    //! A fixed range: \p min at the plot's bottom, \p max at its top (a \p max
    //! not above \p min is taken as \p min+1).
    virtual void setScaleRange(float min, float max);
    //! Back to the automatic range: from min(0, the lowest value) to the
    //! highest value (or the bottom plus 1 when they are equal). It is worked
    //! out again on every change of the values, so a value past it, or the
    //! highest value lowered, rescales every bar.
    virtual void setAutoScale();
    bool hasFixedScale() const { return m_fixedScale; }
    //! The range the bars are drawn in, fixed or automatic.
    float getScaleMin() const { return m_scaleMin; }
    float getScaleMax() const { return m_scaleMax; }

    //! A caption over the plot at its top centre; an empty string removes it.
    virtual void setCaption(const String &caption);
    String getCaption() const;
    //! The caption's Text, or null without a caption.
    internal::Text *getCaptionControl() const { return m_caption; }

    //! The control less its padding, where the bars are, in the control's
    //! own coordinates (its top-left at 0,0), as the bar rects are.
    Rect getPlotBounds() const;
    //! Bar \p index's rect in this control's own coordinates: its column
    //! less the gap, as high as its value makes it, on the plot's bottom.
    //! Empty (no width or no height) when the bar draws nothing.
    Rect getBarRect(size_t index) const;
    //! The bar whose column holds the point \p local (the control's own
    //! coordinates) inside the plot, or -1.
    int getBarAt(const Point &local) const;
    //! The bar under the mouse, or -1. The skin draws it in its hover look.
    int getHoveredBar() const { return m_hoveredBar; }

    //! The tooltip shown over bar \p index: "<index>: <value>".
    String barToolTip(size_t index) const;

    void onMouseMoved(int x, int y, int deltaX, int deltaY) override;
    void onMouseLeave() override;

protected:

    void layout(skin::Base *skin) override;

    //! Works the automatic range out again; true if the range changed.
    bool updateAutoScale();
    //! The height bar \p index has in a plot \p plotHeight high.
    int barHeight(size_t index, int plotHeight) const;
    void setHoveredBar(int bar);

    std::vector<float> m_values;
    bool m_fixedScale=false;
    float m_scaleMin=0.0f;
    float m_scaleMax=1.0f;
    int m_hoveredBar=-1;
    internal::Text *m_caption=nullptr;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_plothistogram_h_
