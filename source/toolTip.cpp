/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/toolTip.h>
#include <gweni/utility.h>
#include <gweni/inputHandler.h>
#include <gweni/controls/canvas.h>

namespace gweni
{
namespace tooltip
{

static controls::Base *g_toolTip=nullptr;

bool toolTipActive()
{
    return g_toolTip != nullptr;
}

void enable(controls::Base *control)
{
    if(!control->getToolTip())
        return;

    g_toolTip=control;
}

void disable(controls::Base *control)
{
    if(g_toolTip == control)
        g_toolTip=nullptr;
}

static gweni::Rect ClampRectToRect(gweni::Rect inside, gweni::Rect const &outside, bool clampSize)
{
    if(inside.x < outside.x)
        inside.x=outside.x;

    if(inside.y  < outside.y)
        inside.y=outside.y;

    if(inside.x+inside.w > outside.x+outside.w)
    {
        if(clampSize)
            inside.w=outside.w;
        else
            inside.x=outside.x+outside.w-inside.w;
    }

    if(inside.y+inside.h > outside.y+outside.h)
    {
        if(clampSize)
            inside.h=outside.h;
        else
            inside.y=outside.w+outside.h-inside.h;
    }

    return inside;
}

void renderToolTip(skin::Base *skin)
{
    if(!g_toolTip)
        return;

    gweni::renderer::Base *render=skin->getRenderer();
    gweni::Point oldRenderOffset=render->getRenderOffset();
    gweni::Point MousePos=input::getMousePosition();
    gweni::Rect Bounds=g_toolTip->getToolTip()->getBounds();
    gweni::Rect rOffset=gweni::Rect(MousePos.x - Bounds.w*0.5f, MousePos.y-Bounds.h-10,
        Bounds.w, Bounds.h);
    rOffset=ClampRectToRect(rOffset, g_toolTip->getCanvas()->getBounds(), false);

    // Calculate offset on screen bounds
    render->addRenderOffset(rOffset);
    render->endClip();
//    skin->drawTooltip(g_toolTip->getToolTip(), skin::State::Update);
    g_toolTip->getToolTip()->doRender(skin);
    render->setRenderOffset(oldRenderOffset);
}

void controlDeleted(controls::Base *control)
{
    disable(control);
}

} // namespace tooltip
} // namespace gweni
