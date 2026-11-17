/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  See license in gweni.h
 */

#ifndef _gweni_controls_touchgesture_h_
#define _gweni_controls_touchgesture_h_

#include <gweni/controls/canvas.h>

#include <vector>

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
//! ScrollControl's fractions of a unit) is kept for the next move. Lifted
//! while moving (Canvas::MinFlingSpeed or faster over its last
//! Canvas::FlingWindowSeconds), the controls it panned fling on: an
//! animation each, slowing exponentially (Canvas::FlingTimeConstant) on the
//! canvas clock, until under Canvas::StopFlingSpeed or at the end of their
//! range; any finger put down on the canvas stops every fling there. Held
//! still for Canvas::LongPressSeconds (within the slop), it is a long press:
//! the context menu of the control under it (Canvas::contextMenuOwnerFor)
//! opens at the finger, and the lift does nothing more. Either way the lift
//! ends with inputMouseLeave(). Every other input is dropped while it holds
//! the canvas.
class TouchGesture: public InputCapture
{
public:

    explicit TouchGesture(Canvas *canvas): m_canvas(canvas) {}

    //! A finger went down at \p at (canvas units) on \p target (the control
    //! the canvas's hit test found there, or null); \p stoppedFling: the
    //! down stopped a fling, so its lift is no tap.
    void start(Base *target, const PointF &at, bool stoppedFling);

    //! Stops every fling on \p canvas; returns how many there were.
    static size_t stopFlings(Canvas *canvas);

    bool active() const { return m_canvas && (m_canvas->inputCapture() == this); }

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
    //! The long press's timer: opens the context menu under the finger.
    void longPress();
    //! Starts the flings of the axes' controls from the finger's speed.
    void fling();
    //! Gives each control an axis went to what is pending on it; an axis
    //! whose control is gone or hidden goes to none again.
    void panLatched();
    //! Finds a control for an axis that has none, from the target up.
    void latch(int axis);
    //! Drops what an axis's control refused at the end of its range.
    static void dropRefused(Axis &state);
    float unitsPerCanvasUnit(Base *control, int axis) const;

    Canvas *m_canvas;
    size_t m_target=0;
    PointF m_down;
    PointF m_last;
    bool m_panning=false;
    //! the down stopped a fling, or the long press opened a menu: no tap
    bool m_noTap=false;
    bool m_longPressed=false;
    TimerHandle m_longPressTimer;
    //! The finger's recent points (Canvas::FlingWindowSeconds and one more),
    //! on platform::getTimeInSeconds(), for the fling's speed.
    struct Sample
    {
        double time;
        PointF at;
    };
    std::vector<Sample> m_samples;
    //! Per axis, the way the finger goes (1, -1, or 0 before it has gone a
    //! unit) and the furthest it went that way: turning back a unit from
    //! there restarts the samples, so the fling's speed is the new way's.
    int m_direction[2]={0, 0};
    float m_extreme[2]={0.0f, 0.0f};
    Axis m_axes[2];
};

}//namespace internal
}//namespace controls
}//namespace gweni

#endif//_gweni_controls_touchgesture_h_
