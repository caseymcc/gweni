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

static gweni::controls::Base *LastPressedControl=nullptr;
static gweni::controls::Base *NewHoveredControl=nullptr;
static gweni::Point LastPressedPos;
// the live drag's canvas, and the last point the cursor was at in it
static gweni::controls::Canvas *DragCanvas=nullptr;
static gweni::Point LastDragPos;
static int Cursor=cursor::Normal;
static bool DropRefused=false;

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
    HoveredControl=nullptr;
    DragCanvas=nullptr;
    Cursor=cursor::Normal;
    DropRefused=false;
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
    DropRefused=CurrentPackage && refused;
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

bool cancel()
{
    LastPressedControl=nullptr;

    if(!CurrentPackage)
        return false;

    Package *package=CurrentPackage;
    gweni::controls::Base *source=SourceControl;
    gweni::controls::Base *target=HoveredControl;
    gweni::controls::Canvas *canvas=DragCanvas;
    const gweni::Point pos=LastDragPos;

    // the state is cleared first, so a hook that looks finds no live drag
    clear();

    if(target)
        target->dragAndDrop_hoverLeave(package);

    if(source)
    {
        source->dragAndDrop_endDragging(false, pos.x, pos.y);

        // a control drag's source is repainted as it was after a refused drop
        if(!package->sourceList)
            source->redraw();
    }

    setCursor(cursor::Normal);
    ended(canvas);
    return true;
}

void controlDeleted(gweni::controls::Base *control)
{
    if(CurrentPackage && (control == DragCanvas))
    {
        // the canvas goes: nothing it holds may be called
        clear();
        LastPressedControl=nullptr;
        NewHoveredControl=nullptr;
        return;
    }

    if(LastPressedControl == control)
        LastPressedControl=nullptr;

    if(HoveredControl == control)
        HoveredControl=nullptr;

    if(NewHoveredControl == control)
        NewHoveredControl=nullptr;

    if(CurrentPackage && ((SourceControl == control) || (CurrentPackage->sourceList == control)))
    {
        // nothing of the source is called: it is half destroyed
        SourceControl=nullptr;
        cancel();
    }
}

bool start(gweni::controls::Base *control, Package *package)
{
    if(CurrentPackage)
        return false;

    CurrentPackage=package;
    SourceControl=control;
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
        return cancel();

    gweni::controls::Base *source=SourceControl;
    gweni::controls::Canvas *canvas=DragCanvas;

    target->dragAndDrop_hoverLeave(package);

    const bool success=target->dragAndDrop_handleDrop(package, x, y);

    // Report back to the source control, to tell it if we've been successful.
    // (the drop may have deleted it, which cleared SourceControl)
    if(source && (SourceControl == source))
    {
        source->dragAndDrop_endDragging(success, x, y);

        if(!package->sourceList)
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
    SourceControl=CurrentPackage->sourceList ? CurrentPackage->sourceList : LastPressedControl;
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

void onMouseMoved(gweni::controls::Canvas *canvas, int x, int y)
{
    // If we're not carrying anything, then check to see if we should
    // pick up from a control that we're holding down. If not, then forget it.
    if(!CurrentPackage && !ShouldStartDraggingControl(x, y))
        return;

    if(DragCanvas != canvas)
        return;

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

bool onCanvasThink(gweni::controls::Canvas *canvas)
{
    if(!CurrentPackage || (DragCanvas != canvas))
        return false;

    // An item drag's list hidden: there is no list to drop back into. (A
    // control drag's source hides itself as it starts: a tab, a dock.)
    if(CurrentPackage->sourceList && !CurrentPackage->sourceList->visible())
        return cancel();

    return false;
}

}//namespace draganddrop
}//namespace gweni
