/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_plothistogram_h_
#define _gweni_controls_plothistogram_h_

#include <gweni/controls/plotBase.h>

namespace gweni
{
namespace controls
{

//! A series of values drawn as bars from the bottom of a frame (imgui's
//! PlotHistogram), with an optional caption at the top centre (imgui's
//! overlay_text) and a tooltip naming the hovered bar's index and value.
//!
//! The series, its range, its caption and its tooltip are PlotBase's. The
//! plot is the control less its padding and, with a caption, less the
//! caption's line at the top, so no bar runs under its text
//! (getPlotBounds()). Bar i takes the columns [i*W/N, (i+1)*W/N) of its W
//! units, rounded down, so the remainder is spread one unit at a time across
//! the row and no bar is more than one unit wider than another; a bar leaves
//! its column's last unit empty as the gap to the next when the column is 2
//! units wide or more. A bar's height is its value's share of the scale
//! range times the plot's height, rounded to a whole unit and clamped to the
//! plot: a value at or below the range's minimum (or not a number) draws
//! nothing, one past its maximum fills the column. A bar with no height or no
//! width is hidden.
//!
//! The textured skin draws the frame and one primitive slot per bar, and
//! redraws only the bars whose rect or look changed: a setValue() whose bar
//! keeps its height marks nothing, one that changes it rewrites that bar's
//! slot alone, and a rescale redraws every bar that moved.
class GWENI_EXPORT PlotHistogram: public PlotBase
{
public:

    GWENI_CONTROL(PlotHistogram, PlotBase);

public:

    //! Bar \p index's rect in this control's own coordinates: its column
    //! less the gap, as high as its value makes it, on the plot's bottom.
    //! Empty (no width or no height) when the bar draws nothing.
    Rect getBarRect(size_t index) const;
    //! The bar whose column holds the point \p local (the control's own
    //! coordinates) inside the plot, or -1.
    int getBarAt(const Point &local) const;
    int sampleAt(const Point &local) const override { return getBarAt(local); }
    //! The bar under the mouse, or -1. The skin draws it in its hover look.
    int getHoveredBar() const { return getHoveredSample(); }

    //! The tooltip shown over bar \p index: "<index>: <value>".
    String barToolTip(size_t index) const { return sampleToolTip(index); }

protected:

    //! a bar is drawn as its height
    float drawnKey(size_t index) const override;
    //! the two bars' looks; the renderer redraws only those
    void hoveredChanged() override { addStateChange(StateChange_Background); }

    //! The height bar \p index has in a plot \p plotHeight high.
    int barHeight(size_t index, int plotHeight) const;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_plothistogram_h_
