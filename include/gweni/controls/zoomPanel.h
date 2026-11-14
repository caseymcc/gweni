/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_zoomPanel_h_
#define _gweni_controls_zoomPanel_h_

#include <gweni/controls/base.h>
#include <gweni/controls/canvas.h>

#include <memory>

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
//!
//! The wheel zooms about the cursor (zoomAt()), WheelStep a notch, unless a
//! control under the cursor takes it (a ScrollControl that can scroll). A
//! middle or right press, or a left one with Space held, anywhere on the panel
//! starts a pan (pressGesture()): the panel has the canvas's input until the
//! button is let go, and once the mouse is PanThreshold units from the press
//! the content follows it. Let go before that, the press and the release go
//! to the canvas as if they had just happened, so a click (a right click's
//! context menu) still reaches the control under the mouse. A pan's press
//! closes the open menus and popup as any other press there does. A
//! disabled panel neither zooms nor pans.
class GWENI_EXPORT ZoomPanel: public Base
{
public:

    GWENI_CONTROL(ZoomPanel, Base);

    GWENI_DRAWS_NOTHING

public:

    //! How far, in canvas units along either axis, the mouse moves from a
    //! press before the press is a pan
    static constexpr int PanThreshold=3;
    //! The zoom factor of one wheel notch (60), towards the user zooming out
    static constexpr float WheelStep=1.1f;
    //! The lowest zoom setZoomRange() takes
    static constexpr float MinZoomLimit=1.0f/1024.0f;

    ~ZoomPanel() override;

    //! The zoom, clamped to zoomRange(); 1 by default.
    void setZoom(float zoom);
    float zoom() const { return m_zoom; }

    //! Where the content's origin is drawn from the panel's origin, in canvas
    //! units (the panel's own, not zoomed); 0,0 by default.
    void setPan(const gweni::PointF &pan);
    const gweni::PointF &pan() const { return m_pan; }

    //! The zooms setZoom() keeps to, 0.25..4 by default; the zoom is clamped
    //! to a new range at once, and \p lowest to MinZoomLimit.
    void setZoomRange(float lowest, float highest);

    //! Zooms to \p zoom (clamped to the range) about \p at, a point in the
    //! panel's own units: the content point drawn there stays there.
    void zoomAt(float zoom, const gweni::PointF &at);

    //! Whether a pan gesture has the mouse and has moved the content.
    bool panning() const;
    float minZoom() const { return m_minZoom; }
    float maxZoom() const { return m_maxZoom; }

    //! The inner panel's size in content units; a negative axis (the default)
    //! follows the panel's own size on that axis.
    void setContentSize(const gweni::Point &size);
    gweni::Point contentSize() const;

    void childRenderTransform(const renderer::RenderTransform &outer, const Point &at,
        renderer::RenderTransform &inner) const override;
    //! A content point p is at pan()+p*zoom() in the panel's units: hit-tests,
    //! localPosToCanvas() and canvasPosToLocal() go through the zoom as the
    //! drawing does.
    gweni::PointF toChildSpace(const gweni::PointF &local) const override;
    gweni::PointF fromChildSpace(const gweni::PointF &child) const override;

    void arrange(skin::Base *skin, Dim dim) override;

    bool onMouseWheeled(int delta) override;
    //! Taken and ignored: the horizontal wheel and Shift+wheel zoom nothing.
    bool onMouseWheeledH(int delta) override;
    InputCapture *pressGesture(int button) override;

protected:

    //! The panel asks for the size it was given, not its content's.
    bool sizedByRequest(Dim dim) const override { return hasRequestedSize(dim); }

    //! Zooms by a wheel \p delta (60 a notch) about the canvas point \p mouse.
    void wheelAt(int delta, const gweni::Point &mouse);

    //! The content draws in a new space: every control under the inner
    //! panel draws again (its text at a new size too when \p zoomed), and
    //! nothing is laid out. A subclass that draws something of its own that
    //! follows the zoom or the pan (a NodeGraph's grid) marks it here too.
    virtual void spaceChanged(bool zoomed);

    float m_zoom=1.0f;
    float m_minZoom=0.25f;
    float m_maxZoom=4.0f;
    gweni::PointF m_pan;
    gweni::Point m_contentSize=gweni::Point(-1, -1);

    class PanGesture;
    std::unique_ptr<PanGesture> m_gesture;
};

}
}
#endif//_gweni_controls_zoomPanel_h_
