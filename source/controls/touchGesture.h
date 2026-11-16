/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  See license in gweni.h
 */

#ifndef _gweni_controls_touchgesture_h_
#define _gweni_controls_touchgesture_h_

#include <gweni/controls/canvas.h>

namespace gweni
{
namespace controls
{
namespace internal
{

//! The canvas's gesture for a finger that went down on nothing that keeps the
//! drag (Canvas::inputTouch): the canvas's input capture from the down to the
//! lift. Lifted within Canvas::TouchSlop of the down, it is a tap, given to
//! the canvas as a move to the down point and a left press and release there;
//! moved that far, it pans: each axis goes to the nearest control under the
//! down point (the touched one, then its actual parents) whose touchPan()
//! moves along it, which keeps the axis until the lift. The finger's motion
//! is summed per axis in canvas units, and what a control did not move (a
//! ScrollControl's fractions of a unit) is kept for the next move. Either
//! way the lift ends with inputMouseLeave(). Every other input is dropped
//! while it holds the canvas.
class TouchGesture: public InputCapture
{
public:

    explicit TouchGesture(Canvas *canvas): m_canvas(canvas) {}

    //! A finger went down at \p at (canvas units) on \p target (the control
    //! the canvas's hit test found there, or null).
    void start(Base *target, const PointF &at);

    bool active() const { return m_canvas && (m_canvas->inputCapture() == this); }
    bool panning() const { return m_panning; }

    void captureTouch(int id, TouchPhase phase, float x, float y) override;
    void captureCanvasDeleted(Canvas * /*canvas*/) override { m_canvas=nullptr; }

    //! Forgets the finger with nothing replayed, the capture given back if it
    //! is still this.
    void abandon();

private:

    //! An axis's pan: the control it went to (its id, 0 for none yet), its
    //! own units per canvas unit along the axis, and the motion it has not
    //! moved yet, in canvas units.
    struct Axis
    {
        size_t control=0;
        float scale=1.0f;
        float pending=0.0f;
    };

    void move(const PointF &at);
    void lift(bool cancelled);
    //! Gives each control an axis went to what is pending on it; an axis
    //! whose control is gone or hidden goes to none again.
    void panLatched();
    //! Finds a control for an axis that has none, from the target up.
    void latch(int axis);
    float unitsPerCanvasUnit(Base *control, int axis) const;

    Canvas *m_canvas;
    size_t m_target=0;
    PointF m_down;
    PointF m_last;
    bool m_panning=false;
    Axis m_axes[2];
};

}//namespace internal
}//namespace controls
}//namespace gweni

#endif//_gweni_controls_touchgesture_h_
