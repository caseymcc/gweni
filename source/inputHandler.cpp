/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/inputHandler.h>
#include <gweni/controls/base.h>
#include <gweni/dragAndDrop.h>
#include <gweni/hook.h>
#include <gweni/platforms/platform.h>
#include <gweni/controls/canvas.h>
#include <gweni/gweni.h>

namespace gweni
{
namespace input
{

static const float      c_DoubleClickSpeed=0.5f;
static const float      c_KeyRepeatRate=0.03f;
static const float      c_KeyRepeatDelay=0.3f;
static const unsigned   c_MaxMouseButtons=5;

struct Action
{
    unsigned char type;
    int x, y;
    UnicodeChar chr;
};

struct KeyData
{
    KeyData()
    {
        for(int i=0; i < keys::KeysCount; i++)
        {
            KeyState[i]=false;
            NextRepeat[i]=0;
        }

        Target=nullptr;
        LeftMouseDown=false;
        RightMouseDown=false;
    }

    bool KeyState[keys::KeysCount];
    float NextRepeat[keys::KeysCount];
    controls::Base *Target;
    bool LeftMouseDown;
    bool RightMouseDown;

};

static KeyData g_keyData;
static Point g_mousePosition;

static float g_fLastClickTime[c_MaxMouseButtons];
static Point g_pntLastClickPos;


static void UpdateHoveredControl(controls::Base *inCanvas)
{
    controls::Base *hovered=inCanvas->getControlAt(g_mousePosition.x, g_mousePosition.y);

    if(hovered != HoveredControl)
    {
        if(HoveredControl)
        {
            controls::Base *OldHover=HoveredControl;
            HoveredControl=nullptr;
            OldHover->onMouseLeave();
        }

        HoveredControl=hovered;

        if(HoveredControl)
            HoveredControl->onMouseEnter();
    }

    if(MouseFocus && MouseFocus->getCanvas() == inCanvas)
    {
        if(HoveredControl)
        {
            controls::Base *OldHover=HoveredControl;
            HoveredControl=nullptr;
            OldHover->redraw();
        }

        HoveredControl=MouseFocus;
    }
}

static bool FindKeyboardFocus(controls::Base *control)
{
    if(!control)
        return false;

    if(control->getKeyboardInputEnabled())
    {
        // Make sure none of our children have keyboard focus first
        //  todo recursive
        for(auto &&child : control->getChildren())
        {
            if(child == KeyboardFocus)
                return false;
        }

        control->focus();
        return true;
    }

    return FindKeyboardFocus(control->getParent());
}

Point getMousePosition()
{
    return g_mousePosition;
}

void onCanvasThink(controls::Base *control)
{
    if(MouseFocus && !MouseFocus->visible())
        MouseFocus=nullptr;

    if(KeyboardFocus
        && (!KeyboardFocus->visible() || !KeyboardFocus->getKeyboardInputEnabled()))
    {
        KeyboardFocus=nullptr;
    }

    if(!KeyboardFocus)
        return;

    if(KeyboardFocus->getCanvas() != control)
        return;

    float fTime=platform::getTimeInSeconds();

    // Simulate Key-Repeats
    for(int i=0; i < keys::KeysCount; i++)
    {
        if(g_keyData.KeyState[i] && g_keyData.Target != KeyboardFocus)
        {
            g_keyData.KeyState[i]=false;
            continue;
        }

        if(g_keyData.KeyState[i] && fTime > g_keyData.NextRepeat[i])
        {
            g_keyData.NextRepeat[i]=platform::getTimeInSeconds()+c_KeyRepeatRate;

            if(KeyboardFocus)
                KeyboardFocus->onKeyPress(i);
        }
    }
}

bool isKeyDown(int key)
{
    return g_keyData.KeyState[key];
}

bool isLeftMouseDown()
{
    return g_keyData.LeftMouseDown;
}

bool isRightMouseDown()
{
    return g_keyData.RightMouseDown;
}

void onMouseMoved(controls::Base *canvas, int x, int y,
    int /*deltaX*/, int /*deltaY*/)
{
    g_mousePosition.x=x;
    g_mousePosition.y=y;
    UpdateHoveredControl(canvas);
}

bool onMouseClicked(controls::Base *canvas, int iMouseButton, bool down)
{
    // If we click on a control that isn't a menu we want to close
    // all the open menus. Menus are children of the canvas.
    if(down && (!HoveredControl || !HoveredControl->isMenuComponent()))
        canvas->closeMenus();

    if(!HoveredControl)
        return false;

    if(HoveredControl->getCanvas() != canvas)
        return false;

    if(!HoveredControl->visible())
        return false;

    if(HoveredControl == canvas)
        return false;

    if(iMouseButton > static_cast<int>(c_MaxMouseButtons))
        return false;

    if(iMouseButton == 0)
        g_keyData.LeftMouseDown=down;
    else if(iMouseButton == 1)
        g_keyData.RightMouseDown=down;

    // Double click.
    // TODO - Shouldn't double click if mouse has moved significantly
    bool bIsDoubleClick=false;

    if(down
        && g_pntLastClickPos.x == g_mousePosition.x
        && g_pntLastClickPos.y == g_mousePosition.y
        && (platform::getTimeInSeconds()-g_fLastClickTime[iMouseButton]) < c_DoubleClickSpeed)
    {
        bIsDoubleClick=true;
    }

    if(down && !bIsDoubleClick)
    {
        g_fLastClickTime[iMouseButton]=platform::getTimeInSeconds();
        g_pntLastClickPos=g_mousePosition;
    }

    if(down)
    {
        if(!FindKeyboardFocus(HoveredControl))
        {
            if(KeyboardFocus)
                KeyboardFocus->unfocus();
        }
    }

    HoveredControl->updateCursor();

    // This tells the child it has been touched, which
    // in turn tells its parents, who tell their parents.
    // This is basically so that Windows can pop themselves
    // to the top when one of their children has been clicked.
    if(down)
        HoveredControl->touch();

#if GWENI_HOOKSYSTEM

    if(down)
    {
        if(hook::callHook(&hook::Base::onControlClicked,
            HoveredControl,
            g_mousePosition.x,
            g_mousePosition.y))
            return true;
    }

#endif

    switch(iMouseButton)
    {
    case 0:
        if(draganddrop::nnMouseButton(HoveredControl,
            g_mousePosition.x, g_mousePosition.y,
            down))
        {
            return true;
        }

        if(bIsDoubleClick)
            HoveredControl->onMouseDoubleClickLeft(g_mousePosition.x, g_mousePosition.y);
        else
            HoveredControl->onMouseClickLeft(g_mousePosition.x, g_mousePosition.y, down);

        return true;

    case 1:
        if(bIsDoubleClick)
            HoveredControl->onMouseDoubleClickRight(g_mousePosition.x, g_mousePosition.y);
        else
            HoveredControl->onMouseClickRight(g_mousePosition.x, g_mousePosition.y, down);

        return true;
    }

    return false;
}

bool handleAccelerator(controls::Base *canvas, UnicodeChar chr)
{
    // Build the accelerator search string
    String accelString;

    if(isControlDown())
        accelString+="CTRL+";

    if(isShiftDown())
        accelString+="SHIFT+";

    chr=towupper(chr);
    accelString+=chr;

    if(KeyboardFocus && KeyboardFocus->handleAccelerator(accelString))
        return true;

    if(MouseFocus && MouseFocus->handleAccelerator(accelString))
        return true;

    if(canvas->handleAccelerator(accelString))
        return true;

    return false;
}

bool doSpecialKeys(controls::Base *canvas, UnicodeChar chr)
{
    if(!KeyboardFocus)
        return false;

    if(KeyboardFocus->getCanvas() != canvas)
        return false;

    if(!KeyboardFocus->visible())
        return false;

    if(!isControlDown())
        return false;

    if(chr == 'C' || chr == 'c')
    {
        KeyboardFocus->onCopy(nullptr);
        return true;
    }

    if(chr == 'V' || chr == 'v')
    {
        KeyboardFocus->onPaste(nullptr);
        return true;
    }

    if(chr == 'X' || chr == 'x')
    {
        KeyboardFocus->onCut(nullptr);
        return true;
    }

    if(chr == 'A' || chr == 'a')
    {
        KeyboardFocus->onSelectAll(nullptr);
        return true;
    }

    return false;
}

bool onKeyEvent(controls::Base *canvas, int key, bool down)
{
    controls::Base *target=KeyboardFocus;

    if(target && target->getCanvas() != canvas)
        target=nullptr;

    if(target && !target->visible())
        target=nullptr;

    if(down)
    {
        if(!g_keyData.KeyState[key])
        {
            g_keyData.KeyState[key]=true;
            g_keyData.NextRepeat[key]=platform::getTimeInSeconds()+c_KeyRepeatDelay;
            g_keyData.Target=target;

            if(target)
                return target->onKeyPress(key);
        }
    }
    else
    {
        if(g_keyData.KeyState[key])
        {
            g_keyData.KeyState[key]=false;

            //! @bug This causes shift left arrow in textboxes
            //! to not work. What is disabling it here breaking?
            //! `g_keyData.Target = nullptr;`

            if(target)
                return target->onKeyRelease(key);
        }
    }

    return false;
}

}//namespace input
}//namespace gweni
