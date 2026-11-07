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

#include <cstdio>
#include <set>
#include <string>

namespace gweni
{
namespace controls
{

Canvas::Canvas(skin::Base *skin): ParentClass(), m_anyDelete(false)
{
//    enlargePrimitiveIds(this, m_primitiveIds, 1);

    setBounds(0, 0, 10000, 10000);
    setScale(1.0f);
    setBackgroundColor(Color(255, 255, 255, 255));
    setDrawBackground(false);

    if(skin)
        setSkin(skin);
    // a new tree is laid out and rendered whole anyway
    if(skin && skin->getRenderer())
        m_textLayoutSerial=skin->getRenderer()->textLayoutSerial();
}

Canvas::~Canvas()
{
    releaseChildren();
}

void Canvas::updateCanvas()
{
    m_updateStats=UpdateStats();

    // The renderer's text draws or measures otherwise now (a new text AA mode):
    // every control is laid out and re-emitted again, as for a new scale.
    if(m_skin && m_skin->getRenderer()
        && (m_skin->getRenderer()->textLayoutSerial() != m_textLayoutSerial))
    {
        m_textLayoutSerial=m_skin->getRenderer()->textLayoutSerial();
        onScaleChanged();
        redraw();
    }

    doThink();

    // Only after the order changed: otherwise every control keeps its number.
    if(m_zOrderDirty)
    {
        m_zOrderDirty=false;
        int zIndex=1;
        renumberZRecursive(zIndex, m_updateStats.zVisited);
    }
}

void Canvas::renderCanvas()
{
    renderer::Base *render=m_skin->getRenderer();
    render->begin();
    renderPhaseLayout();
    render->setClipRegion(getBounds());
    render->setRenderOffset(Point(0, 0));
    render->setScale(scale());
    // here rather than in render(): doRender() skips the canvas too on a
    // frame nothing below it changed
    m_needsRedraw=false;

    if(m_drawBackground && (m_stateChange != StateChange_Nothing))
    {
        render->setDrawColor(m_backgroundColor);
//        render->drawFilledRect(m_primitiveIds[0], getRenderBounds(), getZIndex());
    }

    doRender(m_skin);

    draganddrop::renderOverlay(this, m_skin);
    tooltip::renderToolTip(m_skin);
    render->end();
}

void Canvas::render(skin::Base * /*render*/)
{
}

void Canvas::onBoundsChanged(Rect oldBounds)
{
    ParentClass::onBoundsChanged(oldBounds);
    invalidateChildren(true);
}

void Canvas::doThink()
{
    m_layoutStats=LayoutStats();
    processDelayedDeletes();

    if(hidden())
        return;

#if GWENI_ANIMATE
    anim::think();
#endif
    processDelayedDeletes();
    // Check has focus etc..
    layoutCanvas();

    input::onCanvasThink(this);
}

Canvas *Canvas::s_layingOut=nullptr;
unsigned Canvas::s_passSerial=0;

void Canvas::layoutPass()
{
    m_passRedirtied=false;
    m_passCulpritType=nullptr;
    m_passCulpritId=0;

    Canvas *outer=s_layingOut;
    s_layingOut=this;
    ++s_passSerial;
    m_inLayout=true;
    recurseLayout(m_skin);
    m_inLayout=false;
    s_layingOut=outer;

    // recurseLayout cleared m_updateLayout as it started, and every layout
    // invalidation since has set it again on its way up. Only the ones Base
    // reported as re-dirtying ask for another pass; the rest were requests
    // this pass consumed after they were raised (see Base::invalidate).
    m_updateLayout=m_passRedirtied;
}

void Canvas::layoutCanvas()
{
    static std::set<std::string> warnedCapped;

    while(m_updateLayout && !hidden())
    {
        if(m_layoutStats.passes == MaxLayoutPasses)
        {
            m_layoutStats.capped=true;
            if(warnedCapped.insert(m_passCulpritType).second)
                std::fprintf(stderr, "gweni: layout still dirty after %d passes in one frame,"
                    " re-dirtied by %s#%zu; the rest is left to the next frame"
                    " (warned once per control type)\n",
                    MaxLayoutPasses, m_passCulpritType, m_passCulpritId);
            break;
        }

        layoutPass();
        ++m_layoutStats.passes;

        if(m_passRedirtied && !m_layoutStats.redirtiedType)
        {
            m_layoutStats.redirtiedType=m_passCulpritType;
            m_layoutStats.redirtiedId=m_passCulpritId;
        }
    }
}

void Canvas::renderPhaseLayout()
{
    if(!m_updateLayout || hidden())
        return;

    m_layoutStats.renderLayout=true;
    layoutPass();
}

void Canvas::layoutInvalidated(controls::Base *control)
{
    if(m_passRedirtied)
        return;

    m_passRedirtied=true;
    m_passCulpritType=control->getTypeName();
    m_passCulpritId=control->getControlId();

#ifndef NDEBUG
    // Once per type, so an app author sees which of their controls lean on the
    // loop without a line per frame.
    static std::set<std::string> warned;
    if(warned.insert(m_passCulpritType).second)
        std::fprintf(stderr, "gweni: %s#%zu re-dirtied layout during a layout pass,"
            " so the canvas lays out again (warned once per control type)\n",
            m_passCulpritType, m_passCulpritId);
#endif
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

    // x and y are logical units already: the platform input layer maps window
    // coordinates through the scale (input::GLFW3::setWindowMetrics).
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

    // Only a codepoint that is text gets in: no C0 controls (backspace, tab,
    // ...), DEL or C1 controls, no surrogate (half of a UTF-16 pair, not a
    // codepoint) and nothing past U+10FFFF.
    if(chr < 0x20 || (chr >= 0x7F && chr <= 0x9F)
        || (chr >= 0xD800 && chr <= 0xDFFF) || chr > 0x10FFFF)
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
