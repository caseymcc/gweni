/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_plotlines_h_
#define _gweni_controls_plotlines_h_

#include <gweni/controls/plotBase.h>

namespace gweni
{
namespace controls
{

//! A series of values drawn as a polyline in a frame (imgui's PlotLines),
//! with an optional caption at the top centre and a tooltip naming the
//! sample nearest the mouse.
//!
//! The series, its range, its caption and its tooltip are PlotBase's. The N
//! samples are spread evenly across the plot (the control less its padding
//! and the caption's line) and joined by N-1 segments, each one
//! renderer::Base::drawLine of the line's thickness (setLineThickness(),
//! 1 unit by default). A sample's point is at fractional units, centred so
//! the stroke stays inside the plot: sample i at x = T/2 + i*(W-T)/(N-1)
//! and y = T/2 + (1-share)*(H-T) from the plot's top-left, for a thickness
//! T, a plot W x H and the value's share of the scale range clamped to
//! 0..1. With T = 1 that is i*(W-1)/(N-1) plus the half unit that centres a
//! 1-unit stroke on its column. A value outside the range is drawn at the
//! plot's edge; a NaN value leaves a gap (the segments that touch it are not
//! drawn). A single sample is at the plot's middle and draws nothing.
//!
//! The textured skin draws the frame and one primitive slot per segment, and
//! redraws only the segments whose endpoints moved: a setValue() whose point
//! keeps its y marks nothing, one that moves it rewrites the two segments
//! next to it (one at either end), and a rescale, a new thickness or a
//! push() of a series of distinct values redraws every segment.
class GWENI_EXPORT PlotLines: public PlotBase
{
public:

    GWENI_CONTROL(PlotLines, PlotBase);

public:

    //! A scrolling series (a frame-time graph): drops the oldest value and
    //! appends \p value, so the count stays (an empty series takes it as its
    //! first value). Every value moves one sample left, so in a series of
    //! distinct values every segment is redrawn (in a run of equal values the
    //! segments keep their endpoints, and only the others are); nothing is
    //! laid out. Marks StateChange_Background unless no value changed.
    virtual void push(float value);

    //! The line's thickness in units (drawn as max(1, snap(thickness)) whole
    //! device px); one not above 0 is taken as 1. Marks
    //! StateChange_Background when it changes, which moves every point.
    virtual void setLineThickness(float thickness);
    float getLineThickness() const { return m_thickness; }

    //! Sample \p index's point in this control's own coordinates (its
    //! top-left at 0,0), as the segments are; its y is NaN for a NaN value.
    PointF getSamplePoint(size_t index) const;
    //! How many segments the series is drawn with: N-1, or 0 for fewer than
    //! two samples.
    size_t getSegmentCount() const { return (m_values.size() > 1)?m_values.size()-1:0; }
    //! Segment \p index, from sample \p index to the next, in this control's
    //! own coordinates. False when it is not drawn: past the series, or an
    //! endpoint is NaN.
    bool getSegment(size_t index, PointF &p0, PointF &p1) const;

    //! The sample nearest the point \p local (the control's own coordinates)
    //! along x, inside the plot, or -1.
    int sampleAt(const Point &local) const override;

protected:

    //! a sample is drawn as its point's y (x does not depend on the value)
    float drawnKey(size_t index) const override;

    //! Sample \p index's x and y from the plot's top-left.
    float sampleX(size_t index, const Rect &plot) const;
    float sampleY(size_t index, const Rect &plot) const;

    float m_thickness=1.0f;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_plotlines_h_
