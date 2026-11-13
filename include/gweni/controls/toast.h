/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_toast_h_
#define _gweni_controls_toast_h_

#include <gweni/controls/base.h>
#include <gweni/gweni.h>

namespace gweni
{
namespace controls
{

class Label;
class WindowCloseButton;

//! What a toast reports: its accent (a theme surface and its title's colour)
//! and its default timeout (Canvas::setToastTimeout).
enum class ToastKind: int
{
    Info,
    Positive,
    Warning,
    Negative
};

//! Canvas::notify's options.
struct ToastOptions
{
    //! Seconds it shows (on the canvas clock, not counting the time the stack
    //! was paused under the mouse): below 0 the canvas's default for its kind,
    //! 0 until it is dismissed (a sticky one).
    double timeout=-1.0;
    //! A line above the text in the kind's colour; none when empty.
    String title;
};

//! A short notice the canvas shows in its top-right corner (Canvas::notify):
//! an optional title, a text wrapped to Width units (so it may be several
//! lines) and a close button, on a panel with its kind's accent. It goes when
//! its time is up, on a click on its close button and on dismiss(); a click on
//! its body does nothing. It takes no keyboard focus, and a click on it leaves
//! the app's focus where it was. Removing it gives back every id it took.
class GWENI_EXPORT Toast: public Base
{
public:

    GWENI_CONTROL(Toast, Base);

public:
    //! Its width in units, and the text's inside it.
    static constexpr int Width=320;
    //! Units between two toasts, and between the stack and the canvas's edges.
    static constexpr int Gap=8;
    //! The accent strip's width, inside its left edge.
    static constexpr int AccentWidth=4;

    ToastKind getKind() const { return m_kind; }
    String getText() const;
    String getTitle() const;
    //! The seconds it shows for, 0 for a sticky one.
    double getTimeout() const { return m_timeout; }
    //! When it goes, on the canvas's toast clock (Canvas::toastClock()):
    //! infinity for a sticky one.
    double getDeadline() const { return m_deadline; }

    //! Takes it off the stack: hidden at once (the others close the gap in
    //! this frame's layout) and deleted with the canvas's next delayed deletes.
    //! A second call does nothing.
    void dismiss();
    bool isDismissed() const { return m_dismissed; }

    controls::Label *getTextLabel() const { return m_text; }
    controls::Label *getTitleLabel() const { return m_title; }
    controls::WindowCloseButton *getCloseButton() const { return m_close; }

    //! The title in its kind's colour, the text in the skin's toast colour.
    void updateColors() override;

protected:

    friend class Canvas;
    //! Canvas::notify: its kind, text, title, timeout and deadline.
    void setup(ToastKind kind, const String &text, const String &title, double timeout, double deadline);

    void onClosePressed(event::Info);

    ToastKind m_kind=ToastKind::Info;
    double m_timeout=0.0;
    double m_deadline=0.0;
    bool m_dismissed=false;

    controls::WindowCloseButton *m_close=nullptr;
    controls::Label *m_title=nullptr;
    controls::Label *m_text=nullptr;
};

namespace internal
{

//! The canvas's toast stack (Canvas::toastHost()): owner-placed in the
//! canvas's top-right corner, Toast::Gap in from both edges, Toast::Width
//! wide and as high as its toasts, newest at the top. It draws nothing and
//! takes no mouse, so the mouse reaches what is under it between the toasts.
//! It is a canvas overlay (Canvas::addOverlay), above every modal, popup and
//! menu, below the ToolTip box.
class GWENI_EXPORT ToastHost: public Base
{
public:

    GWENI_CONTROL(ToastHost, Base);

public:
    GWENI_DRAWS_NOTHING

    //! Places itself, one axis per arrange (Placement::Owner).
    void arrange(skin::Base *skin, Dim dim) override;
};

}//namespace internal

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_toast_h_
