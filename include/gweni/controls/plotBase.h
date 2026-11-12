/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_plotbase_h_
#define _gweni_controls_plotbase_h_

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

//! What PlotHistogram and PlotLines share: a series of values, the range they
//! are drawn in, an optional caption at the top centre (imgui's overlay_text),
//! the plot's bounds inside a frame, and a tooltip naming the hovered sample's
//! index and value. A subclass says how a value is drawn (drawnKey()), which
//! sample a point is over (sampleAt()) and what the hovered sample's look
//! changes (hoveredChanged()); its skin renderer draws the frame and the
//! samples.
//!
//! The plot is the control less its padding and, with a caption, less the
//! caption's line at the top, so nothing is drawn under its text
//! (getPlotBounds()).
class GWENI_EXPORT PlotBase: public Base
{
public:

    GWENI_CONTROL(PlotBase, Base);
    // on its own it has no series to draw; a subclass has its renderer
    GWENI_DRAWS_NOTHING

public:

    //! The series. Marks StateChange_Background unless the values are the
    //! ones it has.
    virtual void setValues(const std::vector<float> &values);
    //! One value; an index past the series is ignored. Marks
    //! StateChange_Background only when what is drawn for it changes
    //! (drawnKey()), or the automatic range does.
    virtual void setValue(size_t index, float value);
    const std::vector<float> &getValues() const { return m_values; }
    size_t getValueCount() const { return m_values.size(); }

    //! A fixed range: \p min at the plot's bottom, \p max at its top (a \p max
    //! not above \p min is taken as \p min+1).
    virtual void setScaleRange(float min, float max);
    //! Back to the automatic range: from min(0, the lowest value) to the
    //! highest value (or the bottom plus 1 when they are equal). It is worked
    //! out again on every change of the values, so a value past it, or the
    //! highest value lowered, rescales every sample.
    virtual void setAutoScale();
    bool hasFixedScale() const { return m_fixedScale; }
    //! The range the series is drawn in, fixed or automatic.
    float getScaleMin() const { return m_scaleMin; }
    float getScaleMax() const { return m_scaleMax; }

    //! A caption at the top centre, on a line of its own above the plot; an
    //! empty string removes it. Its line height is taken off the plot when
    //! it is laid out.
    virtual void setCaption(const String &caption);
    String getCaption() const;
    //! The caption's Text, or null without a caption.
    internal::Text *getCaptionControl() const { return m_caption; }

    //! The control less its padding and the caption's line, where the series
    //! is drawn, in the control's own coordinates (its top-left at 0,0).
    Rect getPlotBounds() const;

    //! The sample under the mouse, or -1.
    int getHoveredSample() const { return m_hovered; }
    //! The sample the point \p local (the control's own coordinates) is over,
    //! or -1.
    virtual int sampleAt(const Point & /*local*/) const { return -1; }
    //! The tooltip shown over sample \p index: "<index>: <value>".
    String sampleToolTip(size_t index) const;

    //! A plot asks for the size it was asked to be (its constructor's 200x80,
    //! or the app's): its series is drawn by its skin, and its caption is
    //! placed by it (Placement::Owner).
    bool sizedByRequest(Dim /*dim*/) const override { return true; }

    void onMouseMoved(int x, int y, int deltaX, int deltaY) override;
    void onMouseLeave() override;

protected:

    void layout(skin::Base *skin) override;
    void arrange(skin::Base *skin, Dim dim) override;
    //! Places the caption at the top centre from its preferred size, and
    //! takes its line off the plot.
    void placeCaption();

    //! What is drawn for sample \p index, as one number: setValue() marks the
    //! plot only when it changes (or the range does). Two NaNs are the same.
    virtual float drawnKey(size_t index) const { return m_values[index]; }
    //! The hovered sample changed (getHoveredSample()); the tooltip has been
    //! set already. A subclass whose hovered sample has a look of its own
    //! marks it here.
    virtual void hoveredChanged() {}

    //! Works the automatic range out again; true if the range changed.
    bool updateAutoScale();
    void setHoveredSample(int sample);

    std::vector<float> m_values;
    bool m_fixedScale=false;
    float m_scaleMin=0.0f;
    float m_scaleMax=1.0f;
    int m_hovered=-1;
    internal::Text *m_caption=nullptr;
    //! the caption's line height as last laid out, taken off the plot's top
    int m_captionHeight=0;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_plotbase_h_
