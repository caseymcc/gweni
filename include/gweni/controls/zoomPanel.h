/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_zoomPanel_h_
#define _gweni_controls_zoomPanel_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{

//! A container that shows its content at a continuous zoom and a pan offset:
//! everything in it draws and clips as it would on a canvas at the canvas's
//! scale times zoom(), its rects' edges snapped to device px at that scale and
//! its text drawn at that size, while it lays out in its own units as always.
//! Its children go into an inner panel (getInnerPanel()) at the panel's top
//! left, contentSize() units large, which the zoom and pan only move on screen:
//! a new zoom or pan lays nothing out.
//!
//! A content point p is drawn at the panel's origin (on its whole device px)
//! plus pan()+p*zoom() canvas units.
class GWENI_EXPORT ZoomPanel: public Base
{
public:

    GWENI_CONTROL(ZoomPanel, Base);

    GWENI_DRAWS_NOTHING

public:

    //! The zoom, clamped to zoomRange(); 1 by default.
    void setZoom(float zoom);
    float zoom() const { return m_zoom; }

    //! Where the content's origin is drawn from the panel's origin, in canvas
    //! units (the panel's own, not zoomed); 0,0 by default.
    void setPan(const gweni::PointF &pan);
    const gweni::PointF &pan() const { return m_pan; }

    //! The zooms setZoom() keeps to, 0.25..4 by default; the zoom is clamped
    //! to a new range at once.
    void setZoomRange(float lowest, float highest);
    float minZoom() const { return m_minZoom; }
    float maxZoom() const { return m_maxZoom; }

    //! The inner panel's size in content units; a negative axis (the default)
    //! follows the panel's own size on that axis.
    void setContentSize(const gweni::Point &size);
    gweni::Point contentSize() const;

    void childRenderTransform(const renderer::RenderTransform &outer, const Point &at,
        renderer::RenderTransform &inner) const override;

    void arrange(skin::Base *skin, Dim dim) override;

protected:

    //! The panel asks for the size it was given, not its content's.
    bool sizedByRequest(Dim dim) const override { return hasRequestedSize(dim); }

    //! The content draws in a new space: every control under the inner
    //! panel draws again, and nothing is laid out.
    void spaceChanged();

    float m_zoom=1.0f;
    float m_minZoom=0.25f;
    float m_maxZoom=4.0f;
    gweni::PointF m_pan;
    gweni::Point m_contentSize=gweni::Point(-1, -1);
};

}
}
#endif//_gweni_controls_zoomPanel_h_
