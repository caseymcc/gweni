/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/gweni.h>
#include <gweni/controls/canvas.h>
#include <gweni/skin.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/menuStrip.h>
#include <gweni/controls/popup.h>
#include <gweni/dragAndDrop.h>
#include <gweni/controls/toolTip.h>
#include <gweni/platforms/platform.h>

#if GWENI_ANIMATE
#include <gweni/anim.h>
#endif

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

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
    renumberZOrder();
}

void Canvas::renumberZOrder()
{
    // Only after the order changed: otherwise every control keeps its number.
    if(!m_zOrderDirty)
        return;

    keepOverlaysLast();
    // after it: what it moved is numbered in this pass
    m_zOrderDirty=false;

    renderer::Base *render=m_skin?m_skin->getRenderer():nullptr;
    // with nothing to draw into, nothing limits the range
    const int maxZ=render?render->maxZIndex():std::numeric_limits<int>::max();

    // The first numbering spreads the tree out; after that a control keeps
    // its z unless the ones before it now reach past it.
    ZNumbering numbering;
    numbering.full=!m_zNumbered;
    numbering.gap=m_zGap;
    numbering.high=m_zHigh;
    numbering.tailGap=m_zGap;
    numbering.movedTailGap=std::min(m_zGap, MovedZGap);
    numbering.stats=&m_updateStats;
    renumberZRecursive(numbering);

    if(numbering.maxUsed > maxZ)
    {
        // Out of z: one full pass spreads the tree out again, over at most
        // half of the range, so what is added or moved to the end after it
        // (which takes new z at the top) has the other half before the next
        // compaction; packed for a tree that barely fits at all.
        const size_t spare=(numbering.layers < (size_t)maxZ)?(size_t)maxZ-numbering.layers:0;
        m_zGap=(int)std::min<size_t>(ZGap, numbering.ranges?spare/(2*numbering.ranges):0);

        if(numbering.layers > (size_t)maxZ)
        {
            static bool warned=false;

            if(!warned)
            {
                warned=true;
                std::fprintf(stderr, "gweni: %zu z layers do not fit the renderer's %d;"
                    " the last controls share z values (warned once)\n", numbering.layers, maxZ);
            }
        }

        ZNumbering compaction;
        compaction.full=true;
        compaction.gap=m_zGap;
        compaction.clampTo=maxZ;
        compaction.stats=&m_updateStats;
        renumberZRecursive(compaction);
        numbering=compaction;
    }
    m_zNumbered=true;
    m_zHigh=numbering.maxUsed;
}

void Canvas::renderCanvas()
{
    renderer::Base *render=m_skin->getRenderer();
    render->begin();
    renderPhaseLayout();
    // what that layout added is numbered before the walk draws it
    renumberZOrder();
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

    {
        // the walk only draws: a tree change from here on is reported
        RenderWalkScope walk;
        doRender(m_skin);
    }
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
    m_measureCallsAtThink=measureTextCalls();
    processDelayedDeletes();

    if(hidden())
        return;

#if GWENI_ANIMATE
    // only this canvas's controls: another canvas's run in its own update
    // phase, before its own layout
    m_updateStats.animsRun+=anim::think(this);
#endif
    processDelayedDeletes();
    updatePopup();
    updateToolTip();
    // Check has focus etc..
    layoutCanvas();

    input::onCanvasThink(this);
    noteMeasureCalls();
}

void Canvas::noteMeasureCalls()
{
    m_layoutStats.measureCalls=measureTextCalls()-m_measureCallsAtThink;
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

    // Every layout invalidation during the pass set m_updateLayout again on
    // its way up. Only the ones Base reported as re-dirtying ask for another
    // pass; the rest were requests this pass consumed after they were raised
    // (see Base::invalidate). What is still dirty below stays marked on the
    // way down to it (Base::doArrange) for the next pass.
    m_updateLayout=m_passRedirtied;
}

namespace
{

// -1 until GWENI_LAYOUT_VERIFY has been read or setLayoutVerify() called.
int g_layoutVerify=-1;
size_t g_layoutVerifyFailures=0;

// What verifyLayout() compares, per control.
struct LayoutSnapshot
{
    Base *control;
    size_t id;
    const char *type;
    Rect bounds;
    Size preferred;
};

void snapshotLayout(Base *control, std::vector<LayoutSnapshot> &out)
{
    out.push_back({control, control->getControlId(), control->getTypeName(),
        control->getBounds(), control->getPreferredSize()});

    for(Base *child : control->getActualChildren())
        snapshotLayout(child, out);
}

// Prints at most this many differences per verification; every one counts.
constexpr size_t MaxVerifyLines=20;

}//namespace

bool layoutVerifyEnabled()
{
    if(g_layoutVerify < 0)
    {
        const char *env=std::getenv("GWENI_LAYOUT_VERIFY");
        g_layoutVerify=(env && *env && std::strcmp(env, "0") != 0)?1:0;
    }
    return g_layoutVerify == 1;
}

void setLayoutVerify(bool verify)
{
    g_layoutVerify=verify?1:0;
}

size_t layoutVerifyFailures()
{
    return g_layoutVerifyFailures;
}

bool Canvas::s_verifyingLayout=false;

void Canvas::verifyLayout()
{
    std::vector<LayoutSnapshot> before;
    snapshotLayout(this, before);

    // The full pass is no part of the frame's layout: its stats and its
    // measures are put back or left out.
    const LayoutStats stats=m_layoutStats;
    const size_t measures=measureTextCalls();

    s_verifyingLayout=true;
    m_updateLayout=true;
    layoutPass();
    s_verifyingLayout=false;

    m_measureCallsAtThink+=measureTextCalls()-measures;
    m_layoutStats=stats;

    std::vector<LayoutSnapshot> after;
    snapshotLayout(this, after);

    std::unordered_map<const Base *, const LayoutSnapshot *> now;
    for(const LayoutSnapshot &entry : after)
        now.emplace(entry.control, &entry);

    size_t failures=0;
    auto report=[&](const char *type, size_t id, const char *what)
    {
        if(++failures <= MaxVerifyLines)
            std::fprintf(stderr, "gweni: layout verify: %s#%zu %s\n", type, id, what);
    };
    auto compare=[&](const LayoutSnapshot &a, const char *axis, const char *field,
        int pass, int full)
    {
        if(pass == full)
            return;

        char what[128];
        std::snprintf(what, sizeof(what), "%s %s: pass %d, full pass %d", axis, field, pass, full);
        report(a.type, a.id, what);
    };

    for(const LayoutSnapshot &a : before)
    {
        // An id is handed out again once released, so a control the full pass
        // deleted may have a successor at its address under another id.
        auto found=now.find(a.control);

        if(found == now.end() || found->second->id != a.id)
        {
            report(a.type, a.id, "was deleted by the full pass");
            continue;
        }

        const LayoutSnapshot &b=*found->second;

        compare(a, "X", "bounds.x", a.bounds.x, b.bounds.x);
        compare(a, "X", "bounds.w", a.bounds.w, b.bounds.w);
        compare(a, "X", "preferred.width", a.preferred.width, b.preferred.width);
        compare(a, "Y", "bounds.y", a.bounds.y, b.bounds.y);
        compare(a, "Y", "bounds.h", a.bounds.h, b.bounds.h);
        compare(a, "Y", "preferred.height", a.preferred.height, b.preferred.height);
        now.erase(found);
    }

    for(const LayoutSnapshot &b : after)
    {
        if(now.count(b.control))
            report(b.type, b.id, "was created by the full pass");
    }

    if(m_updateLayout)
    {
        char what[160];
        std::snprintf(what, sizeof(what), "re-dirtied layout during the full pass (by %s#%zu)",
            m_passCulpritType, m_passCulpritId);
        report(getTypeName(), getControlId(), what);
    }

    if(failures > MaxVerifyLines)
        std::fprintf(stderr, "gweni: layout verify: ... and %zu more\n", failures-MaxVerifyLines);

    g_layoutVerifyFailures+=failures;
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

    if(!hidden() && !m_updateLayout && layoutVerifyEnabled())
        verifyLayout();
}

void Canvas::renderPhaseLayout()
{
    if(!m_updateLayout || hidden())
        return;

    m_layoutStats.renderLayout=true;
    layoutPass();

    if(!m_updateLayout && layoutVerifyEnabled())
        verifyLayout();
    noteMeasureCalls();
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

void Canvas::layoutRequested(controls::Base *control)
{
    ++m_layoutStats.requests;

    if(!m_layoutStats.requestedType)
    {
        m_layoutStats.requestedType=control->getTypeName();
        m_layoutStats.requestedId=control->getControlId();
    }

#ifndef NDEBUG
    // Once per type, like the re-dirty warning: what layout code writes is
    // computed, and a request it records makes a preferred size depend on
    // what an earlier pass did.
    static std::set<std::string> warned;
    if(warned.insert(control->getTypeName()).second)
        std::fprintf(stderr, "gweni: %s#%zu recorded a requested size or position during a"
            " layout pass; layout code should write through setArrangedBounds or"
            " placeChild (warned once per control type)\n",
            control->getTypeName(), control->getControlId());
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
    m_releasingChildren=true;

    Base::List::iterator iter=m_children.begin();

    while(iter != m_children.end())
    {
        Base *child=*iter;
        iter=m_children.erase(iter);
        delete child;
    }

    m_releasingChildren=false;
}

bool Canvas::inputMouseMoved(int x, int y, int deltaX, int deltaY)
{
    if(hidden())
        return false;

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

    // The next update phase hides it, and shows none for this control again
    // until the mouse has been on another one. The control is the one under
    // the mouse now (input::onMouseMoved keeps HoveredControl current), not
    // m_toolTipOwner, which a move in this same frame has not updated yet.
    if(down)
    {
        if(Base *pressed=toolTipOwnerFor(HoveredControl))
            m_toolTipSuppressed=pressed;
    }

    return input::onMouseClicked(this, button, down);
}

//------------------------------------------------------------------------------
// Tooltip
//------------------------------------------------------------------------------

Base *Canvas::toolTipOwnerFor(Base *hovered)
{
    if(!hovered || (hovered == this) || (hovered->getCanvas() != this))
        return nullptr;

    Base *owner=nullptr;

    if(hovered->hasToolTip())
        owner=hovered;
    else if(hovered->getParent() && hovered->getParent()->hasToolTip())
        owner=hovered->getParent();

    if(!owner || !owner->visible() || owner->isDisabled())
        return nullptr;

    return owner;
}

void Canvas::updateToolTip()
{
    Base *owner=toolTipOwnerFor(HoveredControl);

    if(m_toolTipSuppressed)
    {
        if(owner == m_toolTipSuppressed)
            owner=nullptr;
        else
            m_toolTipSuppressed=nullptr;
    }

    const float now=platform::getTimeInSeconds();

    if(owner != m_toolTipOwner)
    {
        // straight from one tooltip to the next: no second wait
        const bool reshow=m_toolTipShown && owner;

        hideToolTip();
        m_toolTipOwner=owner;
        m_toolTipSince=now;
        m_toolTipDirty=false;

        if(reshow)
            showToolTipFor(owner);
        return;
    }

    if(!owner)
        return;

    if(!m_toolTipShown)
    {
        if(now-m_toolTipSince >= m_toolTipDelay)
            showToolTipFor(owner);
    }
    else if(m_toolTipDirty)
        showToolTipFor(owner);
    else
        // the control moved (a scroll, a layout change): the box follows
        m_toolTip->placeFor(owner, Rect(0, 0, getWidth(), getHeight()));

    m_toolTipDirty=false;
}

void Canvas::showToolTipFor(Base *owner)
{
    if(!m_toolTip)
        m_toolTip=newChild<ToolTip>();

    m_toolTip->setContentFor(owner, Rect(0, 0, getWidth(), getHeight()));
    m_toolTip->setHidden(false);
    m_toolTipShown=true;
}

void Canvas::hideToolTip()
{
    if(!m_toolTipShown)
        return;

    m_toolTipShown=false;
    m_toolTip->setHidden(true);
    m_toolTip->releaseContent(false);
}

void Canvas::toolTipChanged(Base *control)
{
    if(control == m_toolTipOwner)
        m_toolTipDirty=true;
}

void Canvas::toolTipControlDeleted(Base *control)
{
    if(control == m_toolTip)
    {
        // ~ToolTip has given back what it held
        m_toolTip=nullptr;
        m_toolTipShown=false;
    }

    if(control == m_toolTipOwner)
    {
        if(m_toolTipShown)
        {
            m_toolTipShown=false;
            m_toolTip->setHidden(true);
            // the owner's setToolTip() control goes with it
            m_toolTip->releaseContent(true);
        }
        m_toolTipOwner=nullptr;
    }

    if(control == m_toolTipSuppressed)
        m_toolTipSuppressed=nullptr;
}

//------------------------------------------------------------------------------
// Popup
//------------------------------------------------------------------------------

namespace
{
//! \p control is \p root or inside it.
bool within(Base *control, Base *root)
{
    for(; control; control=control->getParent())
    {
        if(control == root)
            return true;
    }
    return false;
}

//! A Menu that floats on the canvas (a ComboBox's list, a submenu) and closes
//! with closeMenus(); not a MenuStrip, a bar laid out with the rest that never
//! closes.
bool floatingMenu(Base *control)
{
    return gweni_cast<Menu>(control) && !gweni_cast<MenuStrip>(control);
}
}

bool Canvas::overPopup(Base *control)
{
    if(!m_popup || m_popupIsMenu)
        return false;

    // the canvas's child it is in
    while(control && (control->getParent() != this))
        control=control->getParent();

    if(!control || !floatingMenu(control) || control->hidden())
        return false;

    // opened after the popup (bringToFront), so over it
    Base::List::iterator popup=std::find(m_children.begin(), m_children.end(), m_popup);
    return std::find(popup, m_children.end(), control) != m_children.end();
}

void Canvas::showPopup(Popup *popup, Base *anchor, PopupPlacement side)
{
    closePopup();

    if(popup->getParent() != this)
        popup->setParent(this);

    m_popup=popup;
    m_popupAnchor=anchor;
    m_popupIsMenu=false;
    popup->setAnchor(anchor, side);
    popup->setHidden(false);
    // last among the children, before the ToolTip box (keepOverlaysLast)
    popup->bringToFront();
    popup->invalidate();
    popup->onOpen.call(popup);
}

void Canvas::closePopup()
{
    if(!m_popup)
        return;

    if(m_popupIsMenu)
    {
        closeContextMenu();
        return;
    }

    Popup *popup=static_cast<Popup *>(m_popup);

    m_popup=nullptr;
    m_popupAnchor=nullptr;
    popup->setAnchor(nullptr, popup->getPlacement());
    // a ComboBox's list it opened goes with it
    closeMenus();
    popup->setHidden(true);
    popup->onClose.call(popup);
}

bool Canvas::escapePopup()
{
    if(!m_popup)
        return false;

    if(!m_popupIsMenu)
    {
        for(Base *child : m_children)
        {
            if(overPopup(child))
            {
                closeMenus();
                return true;
            }
        }
    }

    closePopup();
    return true;
}

void Canvas::popupPressed(Base *hovered)
{
    if(!m_popup || m_popupIsMenu)
        return;

    // A press on the anchor is the anchor's (a button toggling its popup); one
    // on a Menu opened over the popup is on a ComboBox's list in it, say. A
    // menu bar is outside it like any other control.
    if(hovered && (within(hovered, m_popup) || within(hovered, m_popupAnchor)
        || overPopup(hovered)))
        return;

    closePopup();
}

void Canvas::updatePopup()
{
    if(!m_popup)
        return;

    if(m_popupIsMenu && m_popup->hidden())
    {
        // closed as menus are (a click outside, an item chosen)
        m_popup=nullptr;
        m_popupAnchor=nullptr;
        m_popupIsMenu=false;
        m_contextMenu=nullptr;
    }
    else if(!m_popupAnchor->visible() || m_popupAnchor->isDisabled())
        closePopup();
    else if(!m_popupIsMenu)
        static_cast<Popup *>(m_popup)->followAnchor();
}

void Canvas::keepOverlaysLast()
{
    if(m_popup && !m_popupIsMenu && (m_popup->getParent() == this))
    {
        // the Menus after it were opened over it (a ComboBox's list in it)
        Base::List after;
        bool stays=false;
        Base::List::iterator iter=std::find(m_children.begin(), m_children.end(), m_popup);

        for(++iter; iter != m_children.end();)
        {
            if((*iter != m_toolTip) && !floatingMenu(*iter))
            {
                stays=true;
                ++iter;
                continue;
            }
            after.push_back(*iter);
            iter=m_children.erase(iter);
        }

        // only what passed a control that stays gives up its z labels
        if(stays)
        {
            zOrderMoved(m_popup);

            for(Base *moved : after)
                zOrderMoved(moved);
        }
        m_children.remove(m_popup);
        m_children.push_back(m_popup);
        m_children.splice(m_children.end(), after);
    }

    if(!m_toolTip || (m_children.back() == m_toolTip))
        return;

    m_children.remove(m_toolTip);
    m_children.push_back(m_toolTip);
    zOrderMoved(m_toolTip);
}

void Canvas::popupControlDeleted(Base *control)
{
    if(!m_popup)
        return;

    if(control == m_popup)
    {
        // a deleted popup fires no onClose
        m_popup=nullptr;
        m_popupAnchor=nullptr;
        m_popupIsMenu=false;
        m_contextMenu=nullptr;
        return;
    }

    if(control != m_popupAnchor)
        return;

    if(m_releasingChildren)
    {
        // A canvas tearing down only hides it: the submenus closeAll() reaches,
        // or what an onClose handler reads, may already be gone. The owner
        // deletes a menu it owns itself.
        m_popup->setHidden(true);
        m_popup=nullptr;
        m_popupAnchor=nullptr;
        m_popupIsMenu=false;
        m_contextMenu=nullptr;
    }
    else
        closePopup();
}

//------------------------------------------------------------------------------
// Context menu
//------------------------------------------------------------------------------

Base *Canvas::contextMenuOwnerFor(Base *hovered)
{
    if(!hovered || (hovered->getCanvas() != this) || hovered->isMenuComponent())
        return nullptr;

    for(Base *control=hovered; control; control=control->getParent())
    {
        if(!control->getContextMenu())
            continue;

        if(!control->visible() || control->isDisabled())
            return nullptr;
        return control;
    }
    return nullptr;
}

void Canvas::showContextMenu(Base *owner, const Point &at)
{
    closePopup();

    // the handler may fill the menu in, or replace or clear it
    owner->onContextMenu.call(owner);

    Menu *menu=owner->getContextMenu();

    if(!menu)
        return;

    if(menu->getParent() != this)
        menu->setParent(this);

    menu->openAt(at);
    m_popup=menu;
    m_popupAnchor=owner;
    m_popupIsMenu=true;
    m_contextMenu=menu;
}

void Canvas::closeContextMenu()
{
    if(!m_popupIsMenu)
        return;

    Menu *menu=m_contextMenu;

    m_popup=nullptr;
    m_popupAnchor=nullptr;
    m_popupIsMenu=false;
    m_contextMenu=nullptr;
    menu->closeAll();
    menu->close();
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

bool Canvas::inputMouseLeave()
{
    if(hidden())
        return false;

    input::onMouseLeft(this);
    return true;
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
