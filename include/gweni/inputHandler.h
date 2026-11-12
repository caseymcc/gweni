/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_inputhandler_h_
#define _gweni_inputhandler_h_

#include <gweni/platforms/inputEvent.h>
#include <queue>

namespace gweni
{
namespace controls
{

class Base;

}//namespace controls

namespace input
{

namespace message
{

enum
{
    Copy,
    Paste,
    Cut,
    Undo,
    Redo,
    SelectAll
};

}//namespace message

// For use in panels
bool GWENI_EXPORT isKeyDown(int key);
bool GWENI_EXPORT isLeftMouseDown();
bool GWENI_EXPORT isRightMouseDown();
Point GWENI_EXPORT getMousePosition();

inline bool isShiftDown()
{
    return isKeyDown(keys::Shift);
}

inline bool isControlDown()
{
    return isKeyDown(keys::Control);
}

bool GWENI_EXPORT handleAccelerator(controls::Base *canvas, gweni::UnicodeChar chr);

// Send input to canvas for study
void GWENI_EXPORT onMouseMoved(controls::Base *canvas, int x, int y,
    int deltaX, int deltaY);
bool GWENI_EXPORT onMouseClicked(controls::Base *canvas, int button, bool down);
//! The cursor left the canvas's window: the hovered control on it gets its
//! onMouseLeave() and nothing is hovered until the cursor moves again. A drag
//! (a control holding the mouse focus) keeps its control.
void GWENI_EXPORT onMouseLeft(controls::Base *canvas);
bool GWENI_EXPORT onKeyEvent(controls::Base *canvas, int key, bool down);
void GWENI_EXPORT onCanvasThink(controls::Base *control);
//! When the next key repeat onCanvasThink() would deliver to \p canvas is due:
//! the first time past the earliest held key's next repeat while the keyboard
//! focus is on the canvas and no input capture holds it, else +infinity (no
//! key repeats there). For Canvas::nextWorkTime().
double GWENI_EXPORT nextKeyRepeatTime(controls::Base *canvas);
//! Unfocuses (Base::unfocus(), so onLostKeyboardFocus() runs) a keyboard
//! focus that is no longer visible() or no longer takes keyboard input.
//! Returns whether it did. The canvas calls it before its update-phase layout,
//! so what the hook changes is laid out in the frame's pass, and after it, for
//! a holder that layout hid.
bool GWENI_EXPORT dropLostKeyboardFocus();

//! A running count of hit-test visits: one per controls::Base::getControlAt
//! call, on every control a hit test enters, whether it finds the point or
//! rejects it. Never reset; diff it around what you measure (gweni_bench's
//! hit_tests=). Input arrives between frames, so it is in no per-frame stats.
size_t GWENI_EXPORT hitTestVisits();

}//namespace input
}//namespace gweni

#endif//_gweni_inputhandler_h_
