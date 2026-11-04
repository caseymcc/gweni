/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/dragAndDrop.h>
#include <gweni/utility.h>
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

void controlDeleted(gweni::controls::Base *control)
{
    if(SourceControl == control)
    {
        SourceControl=nullptr;
        CurrentPackage=nullptr;
        HoveredControl=nullptr;
        LastPressedControl=nullptr;
    }

    if(LastPressedControl == control)
        LastPressedControl=nullptr;

    if(HoveredControl == control)
        HoveredControl=nullptr;

    if(NewHoveredControl == control)
        NewHoveredControl=nullptr;
}

static int m_mouseX=0;
static int m_mouseY=0;

bool start(gweni::controls::Base *control, Package *package)
{
    if(CurrentPackage)
        return false;

    CurrentPackage=package;
    SourceControl=control;
    return true;
}

bool OnDrop(int x, int y)
{
    bool bSuccess=false;

    if(HoveredControl)
    {
        HoveredControl->dragAndDrop_hoverLeave(CurrentPackage);
        bSuccess=HoveredControl->dragAndDrop_handleDrop(CurrentPackage,
            x, y);
    }

    // Report back to the source control, to tell it if we've been successful.
    SourceControl->dragAndDrop_endDragging(bSuccess, x, y);
    SourceControl->redraw();
    CurrentPackage=nullptr;
    SourceControl=nullptr;
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

    if(!hoveredControl->dragAndDrop_draggable())
        return false;

    // Store the last clicked on control. Don't do anything yet,
    // we'll check it in OnMouseMoved, and if it moves further than
    // x pixels with the mouse down, we'll start to drag.
    LastPressedPos=gweni::Point(x, y);
    LastPressedControl=hoveredControl;
    return false;
}

bool ShouldStartDraggingControl(int x, int y)
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

    // Now we're dragging something!
    SourceControl=LastPressedControl;
    gweni::MouseFocus=nullptr;
    LastPressedControl=nullptr;
    CurrentPackage->drawcontrol=nullptr;

    // Some controls will want to decide whether they should be dragged at that
    // moment.
    // This function is for them (it defaults to true)
    if(!SourceControl->dragAndDrop_shouldStartDrag())
    {
        SourceControl=nullptr;
        CurrentPackage=nullptr;
        return false;
    }

    SourceControl->dragAndDrop_startDragging(CurrentPackage,
        LastPressedPos.x, LastPressedPos.y);
    return true;
}

void UpdateHoveredControl(gweni::controls::Base *ctrl, int x, int y)
{
    //
    // We use this global variable to represent our hovered control
    // That way, if the new hovered control gets deleted in one of the
    // Hover callbacks, we won't be left with a hanging pointer.
    // This isn't ideal - but it's minimal.
    //
    NewHoveredControl=ctrl;

    // Nothing to change..
    if(HoveredControl == NewHoveredControl)
        return;

    // We changed - tell the old hovered control that it's no longer hovered.
    if(HoveredControl && HoveredControl != NewHoveredControl)
        HoveredControl->dragAndDrop_hoverLeave(CurrentPackage);

    // If we're hovering where the control came from, just forget it.
    // By changing it to nullptr here we're not going to show any error cursors
    // it will just do nothing if you drop it.
    if(NewHoveredControl == SourceControl)
        NewHoveredControl=nullptr;

    // Check to see if the new potential control can accept this type of
    // package.
    // If not, ignore it and show an error cursor.
    while(NewHoveredControl &&
        !NewHoveredControl->dragAndDrop_canAcceptPackage(CurrentPackage))
    {
        // We can't drop on this control, so lets try to drop
        // onto its parent..
        NewHoveredControl=NewHoveredControl->getParent();

        // Its parents are dead. We can't drop it here.
        // Show the NO WAY cursor.
        if(!NewHoveredControl)
            platform::setCursor(cursor::No);
    }

    // Become out new hovered control
    HoveredControl=NewHoveredControl;

    // If we exist, tell us that we've started hovering.
    if(HoveredControl)
        HoveredControl->dragAndDrop_hoverEnter(CurrentPackage, x, y);

    NewHoveredControl=nullptr;
}

void onMouseMoved(gweni::controls::Base *hoveredControl, int x, int y)
{
    // Always keep these up to date, they're used to draw the dragged control.
    m_mouseX=x;
    m_mouseY=y;

    // If we're not carrying anything, then check to see if we should
    // pick up from a control that we're holding down. If not, then forget it.
    if(!CurrentPackage && !ShouldStartDraggingControl(x, y))
        return;

    // Make sure the canvas redraws when we move
    if(CurrentPackage && CurrentPackage->drawcontrol)
        CurrentPackage->drawcontrol->redraw();

    // Swap to this new hovered control and notify them of the change.
    UpdateHoveredControl(hoveredControl, x, y);

    if(!HoveredControl)
        return;

    // Update the hovered control every mouse move, so it can show where
    // the dropped control will land etc..
    HoveredControl->dragAndDrop_hover(CurrentPackage, x, y);
    // Override the cursor - since it might have been set my underlying controls
    // Ideally this would show the 'being dragged' control. TODO
    platform::setCursor(cursor::Normal);
    hoveredControl->redraw();
}

void renderOverlay(gweni::controls::Canvas * /*canvas*/, skin::Base *skin)
{
    if(!CurrentPackage)
        return;

    if(!CurrentPackage->drawcontrol)
        return;

    gweni::Point pntOld=skin->getRenderer()->getRenderOffset();
    skin->getRenderer()->addRenderOffset(gweni::Rect(m_mouseX-SourceControl->getX()-
        CurrentPackage->holdoffset.x, m_mouseY-
        SourceControl->getY()-CurrentPackage->holdoffset.y,
        0, 0));
    CurrentPackage->drawcontrol->doRender(skin);
    skin->getRenderer()->setRenderOffset(pntOld);
}

}//namespace draganddrop
}//namespace gweni
