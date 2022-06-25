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

// Does copy, paste etc
bool GWENI_EXPORT doSpecialKeys(controls::Base *canvas, gweni::UnicodeChar chr);
bool GWENI_EXPORT handleAccelerator(controls::Base *canvas, gweni::UnicodeChar chr);

// Send input to canvas for study
void GWENI_EXPORT onMouseMoved(controls::Base *canvas, int x, int y,
    int deltaX, int deltaY);
bool GWENI_EXPORT onMouseClicked(controls::Base *canvas, int button, bool down);
bool GWENI_EXPORT onKeyEvent(controls::Base *canvas, int key, bool down);
void GWENI_EXPORT onCanvasThink(controls::Base *control);

}//namespace input
}//namespace gweni

#endif//_gweni_inputhandler_h_
