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

#include <algorithm>
#include <cmath>
#include <limits>

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
            PressTarget[i]=nullptr;
        }

        Target=nullptr;
        LeftMouseDown=false;
        RightMouseDown=false;
    }

    bool KeyState[keys::KeysCount];
    double NextRepeat[keys::KeysCount];
    //! The control each held key's press went to: its release goes there only
    //! (compared, never followed: it may be gone).
    controls::Base *PressTarget[keys::KeysCount];
    controls::Base *Target;
    bool LeftMouseDown;
    bool RightMouseDown;

};

static KeyData g_keyData;
static Point g_mousePosition;
// The canvas the mouse last moved on, until it leaves it (onMouseLeft): the
// one a stale hover may be hit-tested again on.
static controls::Base *g_mouseCanvas=nullptr;

static double g_fLastClickTime[c_MaxMouseButtons];
static Point g_pntLastClickPos;


// While a control on this canvas holds the mouse (MouseFocus), it is the
// hovered control: a move hit-tests nothing and sends no leave or enter, so a
// drag repaints only what the captured control changes itself. Hover is
// hit-tested again once the capture ends (onMouseClicked, onCanvasThink).
//
// The hover moves on before the old control hears its leave, as the keyboard
// focus does in Base::focus(): an ancestor told of the leave
// (onDescendantInteraction) then sees where the hover went, so a row whose
// look covers both the old and the new control changes nothing.
static void UpdateHoveredControl(controls::Base *inCanvas)
{
    if(MouseFocus && MouseFocus->getCanvas() == inCanvas)
    {
        if(HoveredControl != MouseFocus)
        {
            controls::Base *OldHover=HoveredControl;
            HoveredControl=MouseFocus;

            if(OldHover)
                OldHover->onMouseLeave();

            // no enter for the captured control, but its ancestors still learn
            // that the hover is now in it (unless the leave deleted it: ~Base
            // clears both pointers)
            if(MouseFocus && HoveredControl == MouseFocus)
                MouseFocus->notifyAncestorsOfInteraction();
        }

        return;
    }

    controls::Base *hovered=inCanvas->getControlAt(g_mousePosition.x, g_mousePosition.y);

    if(hovered != HoveredControl)
    {
        controls::Base *OldHover=HoveredControl;
        HoveredControl=hovered;

        if(OldHover)
            OldHover->onMouseLeave();

        // unless the leave deleted it (~Base clears HoveredControl)
        if(hovered && HoveredControl == hovered)
            hovered->onMouseEnter();
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

bool dropLostKeyboardFocus()
{
    if(!KeyboardFocus
        || (KeyboardFocus->visible() && KeyboardFocus->getKeyboardInputEnabled()))
        return false;

    // Like any other unfocus: its onLostKeyboardFocus() runs (a TextBox's
    // onLostFocusCaller, so a DragNumber's edit ends), and it is repainted, so
    // it does not keep the focused look.
    KeyboardFocus->unfocus();
    return true;
}

void onCanvasThink(controls::Base *control)
{
    if(MouseFocus && !MouseFocus->visible())
    {
        const bool captured=(MouseFocus->getCanvas() == control);
        MouseFocus=nullptr;

        // its holder hidden, the hover goes to what is under the mouse
        if(captured)
            UpdateHoveredControl(control);
    }

    controls::Canvas *canvas=control->getCanvas();

    // While an input capture has the canvas's input (a tools::DepthView), the
    // hover stays and no key repeats: a stale hover waits for the capture to
    // end, and a key held from before it goes up when it is released
    // (Canvas::inputModifierKey).
    const bool inputCaptured=(canvas && canvas->inputCapture());

    // Content moved under the resting mouse this frame, and the layout that
    // moved it is done: the hover goes to what is under the mouse now. A
    // capture keeps its holder hovered, as for a move.
    if(canvas && !inputCaptured)
    {
        if(canvas->takeHoverStale() && g_mouseCanvas == control)
            UpdateHoveredControl(control);
    }

    dropLostKeyboardFocus();

    if(inputCaptured)
        return;

    if(!KeyboardFocus)
        return;

    if(KeyboardFocus->getCanvas() != control)
        return;

    const double fTime=platform::getTimeInSeconds();

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

double nextKeyRepeatTime(controls::Base *control)
{
    double next=std::numeric_limits<double>::infinity();
    controls::Canvas *canvas=control?control->getCanvas():nullptr;

    if(!canvas || canvas->inputCapture() || !KeyboardFocus || (KeyboardFocus->getCanvas() != canvas))
        return next;

    for(int i=0; i < keys::KeysCount; i++)
    {
        // onCanvasThink repeats once the clock is past NextRepeat
        if(g_keyData.KeyState[i] && (g_keyData.Target == KeyboardFocus))
            next=std::min(next, std::nextafter(g_keyData.NextRepeat[i], std::numeric_limits<double>::infinity()));
    }
    return next;
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
    g_mouseCanvas=canvas;
    UpdateHoveredControl(canvas);
}

void onMouseLeft(controls::Base *canvas)
{
    if(g_mouseCanvas == canvas)
        g_mouseCanvas=nullptr;

    if(MouseFocus && MouseFocus->getCanvas() == canvas)
        return;

    if(!HoveredControl || HoveredControl->getCanvas() != canvas)
        return;

    controls::Base *oldHover=HoveredControl;
    HoveredControl=nullptr;
    oldHover->onMouseLeave();
}

// The click's delivery to the hovered control, once the open menus are closed.
static bool deliverMouseClick(controls::Base *canvas, int iMouseButton, bool down);

bool onMouseClicked(controls::Base *canvas, int iMouseButton, bool down)
{
    // If we click on a control that isn't a menu we want to close
    // all the open menus. Menus are children of the canvas.
    if(down && (!HoveredControl || !HoveredControl->isMenuComponent()))
        canvas->closeMenus();

    // A press off the shown Popup and its anchor closes it, and still goes
    // where it was going.
    if(down)
    {
        if(controls::Canvas *target=canvas->getCanvas())
            target->popupPressed(HoveredControl);
    }

    const bool captured=(MouseFocus && MouseFocus->getCanvas() == canvas);
    const bool handled=deliverMouseClick(canvas, iMouseButton, down);

    // The click ended the capture (a release, usually): the hover was pinned
    // to the holder, so it goes to what is under the mouse now.
    if(captured && !MouseFocus)
        UpdateHoveredControl(canvas);

    // A right press opens the context menu of the control under the mouse (or
    // of its nearest ancestor with one), once the press has closed the menus
    // that were open and reached the control.
    if(down && (iMouseButton == 1))
    {
        controls::Canvas *target=canvas->getCanvas();

        if(target)
        {
            if(controls::Base *owner=target->contextMenuOwnerFor(HoveredControl))
            {
                target->showContextMenu(owner, g_mousePosition);
                return true;
            }
        }
    }

    return handled;
}

static bool deliverMouseClick(controls::Base *canvas, int iMouseButton, bool down)
{
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

    // A press under a modal (a stale hover, before the end of the update
    // phase hit-tests it again) goes nowhere; a release still ends the press
    // of a control that had it before the modal came up.
    if(down)
    {
        if(controls::Canvas *target=canvas->getCanvas())
        {
            if(!target->modalAllows(HoveredControl))
                return false;
        }
    }

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
    // Accelerators are ASCII ("CTRL+C"), so nothing else can match one.
    if(chr >= 0x80)
        return false;

    // Build the accelerator search string
    String accelString;

    if(isControlDown())
        accelString+="CTRL+";

    if(isShiftDown())
        accelString+="SHIFT+";

    if(chr >= 'a' && chr <= 'z')
        chr-='a'-'A';
    accelString+=static_cast<char>(chr);

    if(KeyboardFocus && KeyboardFocus->handleAccelerator(accelString))
        return true;

    controls::Canvas *target=canvas->getCanvas();

    if(MouseFocus && (!target || target->modalAllows(MouseFocus))
        && MouseFocus->handleAccelerator(accelString))
        return true;

    // under a modal, the app's accelerators are out of reach: the top modal's
    // are looked up instead of the whole tree's
    controls::Base *root=canvas;

    if(target && target->topModal())
        root=target->topModal();

    if(root->handleAccelerator(accelString))
        return true;

    return false;
}

bool onKeyEvent(controls::Base *canvas, int key, bool down)
{
    // Escape closes the shown popup (a Popup or a context menu), or the Menus
    // open over a Popup first, whatever has the keyboard focus; its release
    // then finds the key up and goes nowhere.
    if(down && (key == keys::Escape) && !g_keyData.KeyState[key])
    {
        controls::Canvas *target=canvas->getCanvas();

        if(target && target->escapePopup())
            return true;
    }

    controls::Base *target=KeyboardFocus;

    if(target && target->getCanvas() != canvas)
        target=nullptr;

    if(target && !target->visible())
        target=nullptr;

    // Under a modal, a key with no focus it allows goes to the top modal
    // itself (its Escape, its Tab, which cycles inside it).
    if(controls::Canvas *onCanvas=canvas->getCanvas())
    {
        if(controls::Base *top=onCanvas->topModal())
        {
            if(!target || !onCanvas->modalAllows(target))
                target=top->visible() ? top : nullptr;
        }
    }

    if(down)
    {
        if(!g_keyData.KeyState[key])
        {
            g_keyData.KeyState[key]=true;
            g_keyData.NextRepeat[key]=platform::getTimeInSeconds()+c_KeyRepeatDelay;
            g_keyData.Target=target;
            g_keyData.PressTarget[key]=target;

            if(target)
                return target->onKeyPress(key);
        }
    }
    else
    {
        if(g_keyData.KeyState[key])
        {
            g_keyData.KeyState[key]=false;

            // The press moved the focus (Enter choosing a dialog's button,
            // which gives the focus back to the app): the release is not the
            // new holder's, which would act on a key it never saw go down.
            if(target != g_keyData.PressTarget[key])
                target=nullptr;
            g_keyData.PressTarget[key]=nullptr;

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
