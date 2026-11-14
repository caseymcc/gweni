/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/dragAndDrop.h>
#include <gweni/utility.h>
#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/platforms/platform.h>

namespace gweni
{
namespace draganddrop
{

Package *CurrentPackage=nullptr;
gweni::controls::Base *HoveredControl=nullptr;
gweni::controls::Base *SourceControl=nullptr;

// the live item drag's list (its package's sourceList), kept apart from the
// package, which a control may free while the drag is told of it
static gweni::controls::Base *SourceList=nullptr;

static gweni::controls::Base *LastPressedControl=nullptr;
static gweni::controls::Base *NewHoveredControl=nullptr;
static gweni::Point LastPressedPos;
// the live drag's canvas, and the last point the cursor was at in it
static gweni::controls::Canvas *DragCanvas=nullptr;
static gweni::Point LastDragPos;
static int Cursor=cursor::Normal;
static bool DropRefused=false;
// the canvas whose next left release a cancel left pending: the button that
// held the drag is still down, and its release must reach no control
static gweni::controls::Canvas *ReleaseCanvas=nullptr;
// the drag's canvas, when a wheel during the drag may have moved what is under
// the resting cursor, and whether a layout has placed what it scrolled since:
// the target is found again in the update phase after that layout
static gweni::controls::Canvas *RehoverCanvas=nullptr;
static bool RehoverLaidOut=false;

static void setCursor(int type)
{
    Cursor=type;
    platform::setCursor(static_cast<unsigned char>(type));
}

// Forgets the live drag, calling nothing.
static void clear()
{
    CurrentPackage=nullptr;
    SourceControl=nullptr;
    SourceList=nullptr;
    HoveredControl=nullptr;
    DragCanvas=nullptr;
    Cursor=cursor::Normal;
    DropRefused=false;
    RehoverCanvas=nullptr;
}

bool isDragging()
{
    return CurrentPackage != nullptr;
}

gweni::controls::Canvas *dragCanvas()
{
    return CurrentPackage ? DragCanvas : nullptr;
}

int currentCursor()
{
    return Cursor;
}

void setDropRefused(bool refused)
{
    const bool was=DropRefused;

    DropRefused=CurrentPackage && refused;

    // a target's gap moved with no mouse move (its list auto-scrolled): the
    // cursor follows at once
    if(CurrentPackage && HoveredControl && (DropRefused != was))
        setCursor(DropRefused ? cursor::No : cursor::Normal);
}

bool isDropRefused()
{
    return DropRefused;
}

// The drag is over (dropped or cancelled): the canvas's hover, left where it
// was while the drag lasted, is hit-tested again in its next update phase.
static void ended(gweni::controls::Canvas *canvas)
{
    if(canvas)
        canvas->hoverMayHaveMoved();
}

// Ends the live drag with no drop. \p byRelease: the release itself cancels
// it (over nothing that takes it), so no release is left pending.
static bool cancelDrag(bool byRelease)
{
    LastPressedControl=nullptr;

    if(!CurrentPackage)
        return false;

    if(!byRelease)
        ReleaseCanvas=DragCanvas;

    Package *package=CurrentPackage;
    gweni::controls::Base *source=SourceControl;
    gweni::controls::Base *target=HoveredControl;
    gweni::controls::Canvas *canvas=DragCanvas;
    const gweni::Point pos=LastDragPos;
    const bool controlDrag=!SourceList;

    // the state is cleared first, so a hook that looks finds no live drag
    clear();

    if(target)
        target->dragAndDrop_hoverLeave(package);

    if(source)
    {
        source->dragAndDrop_endDragging(false, pos.x, pos.y);

        // a control drag's source is repainted as it was after a refused drop
        if(controlDrag)
            source->redraw();
    }

    setCursor(cursor::Normal);
    ended(canvas);
    return true;
}

bool cancel()
{
    return cancelDrag(false);
}

bool takeCancelledRelease(gweni::controls::Canvas *canvas, bool down)
{
    if(!ReleaseCanvas || (canvas != ReleaseCanvas))
        return false;

    ReleaseCanvas=nullptr;

    // a press: the release was lost (let go off the window), and this press
    // is the user's next click
    return !down;
}

void controlDeleted(gweni::controls::Base *control)
{
    if(control == ReleaseCanvas)
        ReleaseCanvas=nullptr;

    if(control == RehoverCanvas)
        RehoverCanvas=nullptr;

    if(CurrentPackage && (control == DragCanvas))
    {
        // the canvas goes: nothing it holds may be called
        const bool cursorSet=(Cursor != cursor::Normal);

        clear();
        LastPressedControl=nullptr;
        NewHoveredControl=nullptr;

        // the platform's cursor (a page's CSS cursor) is not left at the
        // drag's No
        if(cursorSet)
            setCursor(cursor::Normal);
        return;
    }

    if(LastPressedControl == control)
        LastPressedControl=nullptr;

    if(HoveredControl == control)
        HoveredControl=nullptr;

    if(NewHoveredControl == control)
        NewHoveredControl=nullptr;

    if(CurrentPackage && ((SourceControl == control) || (SourceList == control)))
    {
        // nothing of the source is called: it is half destroyed
        SourceControl=nullptr;
        cancel();
    }
}

void packageDeleted(Package *package)
{
    // the drag carries it: it ends while the package is still there to be
    // passed to the target's hoverLeave
    if(package && (CurrentPackage == package))
        cancel();
}

bool start(gweni::controls::Base *control, Package *package)
{
    if(CurrentPackage)
        return false;

    CurrentPackage=package;
    SourceControl=control;
    SourceList=package ? package->sourceList : nullptr;
    DragCanvas=control ? control->getCanvas() : nullptr;
    return true;
}

static bool OnDrop(int x, int y)
{
    Package *package=CurrentPackage;
    gweni::controls::Base *target=HoveredControl;

    // released over nothing that accepts it, or where its target refuses it:
    // a cancel
    if(!target || DropRefused)
        return cancelDrag(true);

    gweni::controls::Base *source=SourceControl;
    gweni::controls::Canvas *canvas=DragCanvas;
    // read now: the drop may delete the package's owner (a handler taking
    // reordering off the source list), which cancels and clears SourceControl
    const bool controlDrag=!package->sourceList;

    target->dragAndDrop_hoverLeave(package);

    const bool success=target->dragAndDrop_handleDrop(package, x, y);

    // Report back to the source control, to tell it if we've been successful.
    // (the drop may have deleted it, which cleared SourceControl)
    if(source && (SourceControl == source))
    {
        source->dragAndDrop_endDragging(success, x, y);

        if(controlDrag)
            source->redraw();
    }

    clear();
    setCursor(cursor::Normal);
    ended(canvas);
    return true;
}

bool nnMouseButton(gweni::controls::Base *hoveredControl, int x, int y, bool down)
{
    if(!down)
    {
        LastPressedControl=nullptr;

        // Not carrying anything, allow normal actions
        if(!CurrentPackage)
            return false;

        // We were carrying something, drop it.
        OnDrop(x, y);
        return true;
    }

    if(!hoveredControl)
        return false;

    // The pressed control, or the nearest ancestor that takes drags from its
    // descendants (a reorderable list, whose item may be pressed anywhere).
    if(!hoveredControl->dragAndDrop_draggable())
    {
        gweni::controls::Base *pressed=hoveredControl;

        hoveredControl=nullptr;

        for(gweni::controls::Base *control=pressed->getParent(); control; control=control->getParent())
        {
            if(control->dragAndDrop_takesDescendantDrag(pressed) && control->dragAndDrop_draggable())
            {
                hoveredControl=control;
                break;
            }
        }

        if(!hoveredControl)
            return false;
    }

    // Store the last clicked on control. Don't do anything yet,
    // we'll check it in OnMouseMoved, and if it moves further than
    // x pixels with the mouse down, we'll start to drag.
    LastPressedPos=gweni::Point(x, y);
    LastPressedControl=hoveredControl;
    hoveredControl->dragAndDrop_pressed(x, y);
    return false;
}

static bool ShouldStartDraggingControl(int x, int y)
{
    // We're not holding a control down..
    if(!LastPressedControl)
        return false;

    // Not been dragged far enough
    int iLength=std::abs(x-LastPressedPos.x) + std::abs(y-LastPressedPos.y);

    if(iLength < 5)
        return false;

    // A part of an item that took the mouse for a gesture of its own (a
    // Slider's thumb, a TextBox's selection, a nested scroll bar) keeps it: the
    // press is not the list's. A Button's press is only a click, which the
    // item drag takes from it below.
    gweni::controls::Base *holder=gweni::MouseFocus;

    if(holder && (holder != LastPressedControl) && !gweni_cast<gweni::controls::Button>(holder)
        && LastPressedControl->dragAndDrop_takesDescendantDrag(holder))
    {
        LastPressedControl=nullptr;
        return false;
    }

    // Create the dragging package
    CurrentPackage=LastPressedControl->dragAndDrop_getPackage(LastPressedPos.x,
        LastPressedPos.y);

    // We didn't create a package!
    if(!CurrentPackage)
    {
        LastPressedControl=nullptr;
        SourceControl=nullptr;
        return false;
    }

    // Now we're dragging something! An item drag is the list's, not the
    // pressed row's.
    SourceList=CurrentPackage->sourceList;
    SourceControl=SourceList ? SourceList : LastPressedControl;
    DragCanvas=LastPressedControl->getCanvas();

    // An item drag takes the press from the control holding it (a card's
    // Button, a row): it is let go without a click, as a pooled row given
    // another item is (VirtualList::releaseRow).
    if(CurrentPackage->sourceList && gweni::MouseFocus)
    {
        if(gweni::controls::Button *button=gweni_cast<gweni::controls::Button>(gweni::MouseFocus))
            button->setDepressed(false);
    }

    gweni::MouseFocus=nullptr;

    // An item drag takes the hover away too: the input handler leaves it where
    // it is while the drag lasts, and the pressed row of a pooled list may show
    // another item once the list scrolls (it is hit-tested again at the end).
    if(CurrentPackage->sourceList && gweni::HoveredControl)
    {
        gweni::controls::Base *hovered=gweni::HoveredControl;

        gweni::HoveredControl=nullptr;
        hovered->onMouseLeave();
    }

    LastPressedControl=nullptr;

    // Some controls will want to decide whether they should be dragged at that
    // moment.
    // This function is for them (it defaults to true)
    if(!SourceControl->dragAndDrop_shouldStartDrag())
    {
        clear();
        return false;
    }

    SourceControl->dragAndDrop_startDragging(CurrentPackage,
        LastPressedPos.x, LastPressedPos.y);

    // Whatever cursor the pressed control had set (a Finger, a Beam), the drag
    // starts with the Normal one: the moves only set a cursor that differs
    // from the last the drag asked for.
    if(CurrentPackage)
        setCursor(cursor::Normal);
    return CurrentPackage != nullptr;
}

static void UpdateHoveredControl(gweni::controls::Base *ctrl, int x, int y)
{
    //
    // We use this global variable to represent our hovered control
    // That way, if the new hovered control gets deleted in one of the
    // Hover callbacks, we won't be left with a hanging pointer.
    // This isn't ideal - but it's minimal.
    //
    NewHoveredControl=ctrl;

    // If we're hovering where the control came from, just forget it: it is
    // no target. An item drag's source is its list, which may take its own
    // items back (a reorder within it).
    if(!CurrentPackage->sourceList && (NewHoveredControl == SourceControl))
        NewHoveredControl=nullptr;

    // Check to see if the new potential control can accept this type of
    // package. If not, try its parents.
    while(NewHoveredControl &&
        !NewHoveredControl->dragAndDrop_canAcceptPackage(CurrentPackage))
    {
        NewHoveredControl=NewHoveredControl->getParent();

        if(!CurrentPackage->sourceList && (NewHoveredControl == SourceControl))
            NewHoveredControl=nullptr;
    }

    // Nothing to change..
    if(HoveredControl == NewHoveredControl)
    {
        NewHoveredControl=nullptr;
        return;
    }

    // the old target's refusal was its own
    DropRefused=false;

    // We changed - tell the old hovered control that it's no longer hovered.
    if(HoveredControl)
        HoveredControl->dragAndDrop_hoverLeave(CurrentPackage);

    // Become out new hovered control (the leave may have deleted it, or ended
    // the drag)
    HoveredControl=CurrentPackage ? NewHoveredControl : nullptr;

    // If we exist, tell us that we've started hovering.
    if(HoveredControl)
        HoveredControl->dragAndDrop_hoverEnter(CurrentPackage, x, y);

    NewHoveredControl=nullptr;
}

// The live drag's cursor at (x, y) on its canvas: the target under it found
// (one hit test of the canvas) and told of the point.
static void hoverAt(gweni::controls::Canvas *canvas, int x, int y)
{
    LastDragPos=gweni::Point(x, y);

    // The target is found here, not from the input handler's hover, which
    // stays where it was while the drag lasts.
    UpdateHoveredControl(canvas->getControlAt(x, y), x, y);

    if(!CurrentPackage)
        return;

    if(!HoveredControl)
    {
        // nothing here takes it: the NO WAY cursor
        if(Cursor != cursor::No)
            setCursor(cursor::No);
        return;
    }

    // Tell the target of every move, so it can show where the drop would land.
    // It marks what it changes itself: a move that changes nothing writes
    // nothing.
    HoveredControl->dragAndDrop_hover(CurrentPackage, x, y);

    if(!CurrentPackage)
        return;

    const int wanted=DropRefused ? cursor::No : cursor::Normal;

    if(Cursor != wanted)
        setCursor(wanted);
}

void onMouseMoved(gweni::controls::Canvas *canvas, int x, int y)
{
    // If we're not carrying anything, then check to see if we should
    // pick up from a control that we're holding down. If not, then forget it.
    if(!CurrentPackage && !ShouldStartDraggingControl(x, y))
        return;

    if(DragCanvas != canvas)
        return;

    // a move finds the target itself
    if(RehoverCanvas == canvas)
        RehoverCanvas=nullptr;

    hoverAt(canvas, x, y);
}

void wheeled(gweni::controls::Canvas *canvas)
{
    if(CurrentPackage && canvas && (DragCanvas == canvas))
    {
        RehoverCanvas=canvas;
        // the scroll is laid out in this frame's pass, after its update
        // phase's beforeLayout()
        RehoverLaidOut=false;
    }
}

void targetChanged(gweni::controls::Base *control)
{
    if(!CurrentPackage || !control || (HoveredControl != control) || !DragCanvas)
        return;

    HoveredControl=nullptr;
    DropRefused=false;
    RehoverCanvas=DragCanvas;
    // nothing moved: the cursor is over what it was over
    RehoverLaidOut=true;
}

bool rehoverPending(gweni::controls::Canvas *canvas)
{
    return CurrentPackage && canvas && (RehoverCanvas == canvas);
}

void beforeLayout(gweni::controls::Canvas *canvas)
{
    if(!rehoverPending(canvas) || !RehoverLaidOut)
        return;

    RehoverCanvas=nullptr;

    // The wheel's scroll was laid out in the last frame's pass: what is under
    // the resting cursor now is where the drop would land. Found before this
    // frame's layout, so a new gap's bar is placed in its one pass.
    hoverAt(canvas, LastDragPos.x, LastDragPos.y);
}

bool onCanvasThink(gweni::controls::Canvas *canvas)
{
    if(!CurrentPackage || (DragCanvas != canvas))
        return false;

    // this frame's layout placed what a wheel scrolled (beforeLayout())
    if(RehoverCanvas == canvas)
        RehoverLaidOut=true;

    // An item drag's list hidden: there is no list to drop back into. (A
    // control drag's source hides itself as it starts: a tab, a dock.)
    if(SourceList && !SourceList->visible())
        return cancel();

    // A target hidden under the cursor is the target no more: a release before
    // the next move would drop into it. The next move finds what is there.
    if(HoveredControl && !HoveredControl->visible())
    {
        gweni::controls::Base *target=HoveredControl;

        HoveredControl=nullptr;
        DropRefused=false;
        target->dragAndDrop_hoverLeave(CurrentPackage);

        if(CurrentPackage && (Cursor != cursor::No))
            setCursor(cursor::No);
    }

    return false;
}

}//namespace draganddrop
}//namespace gweni
