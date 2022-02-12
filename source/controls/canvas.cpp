/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/gweni.h>
#include <gweni/controls/canvas.h>
#include <gweni/skin.h>
#include <gweni/controls/menu.h>
#include <gweni/dragAndDrop.h>
#include <gweni/toolTip.h>

#if GWENI_ANIMATE
#include <gweni/anim.h>
#endif

#include <cctype>

namespace gweni
{
namespace controls
{

Canvas::Canvas(skin::Base *skin): ParentClass(), m_anyDelete(false)
{
    enlargePrimitiveIds(this, m_primitiveIds, 1);

    setBounds(0, 0, 10000, 10000);
    setScale(1.0f);
    setBackgroundColor(Color(255, 255, 255, 255));
    setDrawBackground(false);

    if(skin)
        setSkin(skin);
}

Canvas::~Canvas()
{
    releaseChildren();
}

void Canvas::updateCanvas()
{
    int zIndex=1;

    updateRecursive(zIndex);
}

void Canvas::renderCanvas()
{
    doThink();
    renderer::Base *render=m_skin->getRenderer();
    render->begin();
    recurseLayout(m_skin);
    render->setClipRegion(getBounds());
    render->setRenderOffset(Point(0, 0));
    render->setScale(scale());

    if(m_drawBackground)
    {
        render->setDrawColor(m_backgroundColor);
        render->drawFilledRect(m_primitiveIds[0], getRenderBounds(), getZIndex());
    }

    doRender(m_skin);
    draganddrop::renderOverlay(this, m_skin);
    tooltip::renderToolTip(m_skin);
    render->end();
}

void Canvas::render(skin::Base * /*render*/)
{
    m_needsRedraw=false;
}

void Canvas::onBoundsChanged(Rect oldBounds)
{
    ParentClass::onBoundsChanged(oldBounds);
    invalidateChildren(true);
}

void Canvas::doThink()
{
    processDelayedDeletes();

    if(hidden())
        return;

#if GWENI_ANIMATE
    anim::think();
#endif
    // Reset tabbing
    {
        m_nextTab=nullptr;
        m_firstTab=nullptr;
    }
    processDelayedDeletes();
    // Check has focus etc..
    recurseLayout(m_skin);

    // If we didn't have a next tab, cycle to the start.
    if(m_nextTab == nullptr)
        m_nextTab=m_firstTab;

    input::onCanvasThink(this);
}

void Canvas::setScale(float f)
{
    if(m_scale == f)
        return;

    m_scale=f;

    if(m_skin && m_skin->getRenderer())
        m_skin->getRenderer()->setScale(m_scale);

    onScaleChanged();
    redraw();
}

void Canvas::addDelayedDelete(controls::Base *control)
{
    if(!m_anyDelete || m_deleteSet.find(control) == m_deleteSet.end())
    {
        m_anyDelete=true;
        m_deleteSet.insert(control);
        m_deleteList.push_back(control);
    }
}

void Canvas::PreDeleteCanvas(controls::Base *control)
{
    if(m_anyDelete)
    {
        std::set<controls::Base *>::iterator itFind;

        if((itFind=m_deleteSet.find(control)) != m_deleteSet.end())
        {
            m_deleteList.remove(control);
            m_deleteSet.erase(control);
            m_anyDelete=!m_deleteSet.empty();
        }
    }
}

void Canvas::processDelayedDeletes()
{
    while(m_anyDelete)
    {
        m_anyDelete=false;
        controls::Base::List deleteList=m_deleteList;
        m_deleteList.clear();
        m_deleteSet.clear();

        for(auto &&control : deleteList)
        {
            control->preDelete(getSkin());
            delete control;
            redraw();
        }
    }
}

void Canvas::releaseChildren()
{
    Base::List::iterator iter=m_children.begin();

    while(iter != m_children.end())
    {
        Base *child=*iter;
        iter=m_children.erase(iter);
        delete child;
    }
}

bool Canvas::inputMouseMoved(int x, int y, int deltaX, int deltaY)
{
    if(hidden())
        return false;

    if(tooltip::toolTipActive())
        redraw();

    // Todo: Handle scaling here..
    // float fScale = 1.0f / scale();
    input::onMouseMoved(this, x, y, deltaX, deltaY);

    if(!HoveredControl)
        return false;

    if(HoveredControl == this)
        return false;

    if(HoveredControl->getCanvas() != this)
        return false;

    HoveredControl->onMouseMoved(x, y, deltaX, deltaY);
    HoveredControl->updateCursor();
    draganddrop::onMouseMoved(HoveredControl, x, y);
    return true;
}

bool Canvas::inputMouseButton(int button, bool down)
{
    if(hidden())
        return false;

    return input::onMouseClicked(this, button, down);
}

bool Canvas::inputModifierKey(int key, bool down)
{
    if(hidden())
        return false;

    if(key <= keys::Invalid)
        return false;

    if(key >= keys::KeysCount)
        return false;

    return input::onKeyEvent(this, key, down);
}

bool Canvas::inputCharacter(UnicodeChar chr)
{
    if(hidden())
        return false;

    // Check if character is printable, i.e. don't want hidden codes, like backspace.
    if(!std::isprint(chr))
        return false;

    // Handle Accelerators
    if(input::handleAccelerator(this, chr))
        return true;

    // Handle characters
    if(!KeyboardFocus)
        return false;

    if(KeyboardFocus->getCanvas() != this)
        return false;

    if(!KeyboardFocus->visible())
        return false;

    if(input::isControlDown())
        return false;

    return KeyboardFocus->onChar(chr);
}

bool Canvas::inputMouseWheel(int val)
{
    if(hidden())
        return false;

    if(!HoveredControl)
        return false;

    if(HoveredControl == this)
        return false;

    if(HoveredControl->getCanvas() != this)
        return false;

    return HoveredControl->onMouseWheeled(val);
}

}//namespace controls
}//namespace gweni
