/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  See license in Gweni.h
 */


#ifndef _gweni_inputevent_h_
#define _gweni_inputevent_h_

#include <gweni/platforms/platformTypes.h>

namespace gweni
{
namespace keys
{

enum: char
{
    Invalid=0,
    Return=1,
    Backspace=2,
    Delete=3,
    Left=4,
    Right=5,
    Shift=6,
    Tab=7,
    Space=8,
    Home=9,
    End=10,
    Control=11,
    Up=12,
    Down=13,
    Escape=14,
    Alt=15,
    Command=16,   // Mac command/clover
    KeysCount //!< number of keys
};

}//namespace keys

class IInputEvent
{
public:
    virtual ~IInputEvent() {}

    virtual bool inputMouseMoved(int x, int y, int deltaX, int deltaY)=0;
    virtual bool inputMouseButton(int button, bool down)=0;
    virtual bool inputModifierKey(int key, bool down)=0;
    virtual bool inputCharacter(gweni::UnicodeChar chr)=0;
    virtual bool inputMouseWheel(int val)=0;
    virtual bool inputQuit()=0;
};

}

#endif //_gweni_inputevent_h_
