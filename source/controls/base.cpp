/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/base.h>
#include <gweni/controls/label.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/layout/layout.h>
#include <gweni/gweni.h>
#include <gweni/platforms/baseRender.h>
#include <gweni/skin.h>
#include <gweni/platforms/platform.h>
#include <gweni/dragAndDrop.h>
#include <gweni/inputHandler.h>
#include <gweni/controls/toolTip.h>
#include <gweni/utility.h>
#include <gweni/objectIdBank.h>
#include <algorithm>
#include <list>
#include <vector>

#if GWENI_ANIMATE
#   include <gweni/anim.h>
#endif

#include <cstring>
#include <map>
#include <set>
#include <sstream>
#include <iostream>

namespace gweni
{
namespace controls
{

size_t countParents(Base *base)
{
    Base *parent=base->getParent();
    size_t count=0;

    while(parent)
    {
        count++;
        parent=parent->getParent();
    }
    return count;
}

std::string spaceByParent(Base *base)
{
    size_t parents=countParents(base);
    std::string spaces;

    for(size_t i=0; i<parents; ++i)
        spaces+="  ";
    
    return spaces;
}

size_t getControlType(std::string name)
{
    static std::vector<std::string> controlTypes;

    for(size_t i=0; i<controlTypes.size(); ++i)
    {
        if(controlTypes[i] == name)
            return i;
    }

    controlTypes.push_back(name);
    return controlTypes.size()-1;
}

std::string getFullName(Base *base)
{
    std::string name=(!base->getName().empty())?base->getName():base->getTypeName();
    controls::Base *parent=base->getParent();

    while(parent)
    {
        std::string parentName=(!parent->getName().empty())?parent->getName():parent->getTypeName();

        parentName+="/"+name;
        name=parentName;
        parent=parent->getParent();
    }

    return name;
}

ObjectIdBank<Base *, nullptr> g_controls;

namespace
{
// Open RenderWalkScopes, and the control whose hooks the drain is running in
// the innermost one (the render phase is single-threaded).
int g_renderWalkDepth=0;
Base *g_renderingControl=nullptr;
// Tree changes made inside a RenderWalkScope: a drain that sees this move
// rebuilds its ancestor stack, which the change may have made stale.
unsigned g_renderWalkTreeChanges=0;

// The pending-render queue: every control with a state change the render
// phase has not rendered yet, in the order they were queued, each at its
// m_renderQueueIndex. A deleted control leaves a null slot, which the next
// drain compacts away. One UI thread, as the id banks.
std::vector<Base *> g_renderQueue;
// Slots ~Base nulled since the last drain compacted the queue.
size_t g_renderQueueHoles=0;
// A drain is running: a canvas rendered from a hook drains nothing.
bool g_draining=false;

// The (operation, type) pairs reported, each with its record entry.
std::map<std::pair<std::string, std::string>, std::string> &renderWalkMutationRecord()
{
    static std::map<std::pair<std::string, std::string>, std::string> record;
    return record;
}

void reportRenderWalkMutation(Base *control, const char *operation)
{
    const char *type=control->getTypeName();
    std::string &entry=renderWalkMutationRecord()[{operation, type}];
    if(!entry.empty())
        return;

    const char *renderingType=g_renderingControl?g_renderingControl->getTypeName():"none";
    entry=std::string(type)+" "+operation+" (rendering "+renderingType+")";

    std::cerr<<"gweni: "<<type<<"#"<<control->getControlId()<<" "<<operation
        <<" during the render walk (rendering "<<renderingType;
    if(g_renderingControl)
        std::cerr<<"#"<<g_renderingControl->getControlId();
    std::cerr<<")\n";
}

// At the top of every tree change: one branch outside the walk.
inline void noteRenderWalkMutation(Base *control, const char *operation)
{
    if(g_renderWalkDepth != 0)
    {
        ++g_renderWalkTreeChanges;
        reportRenderWalkMutation(control, operation);
    }
}
}//namespace

RenderWalkScope::RenderWalkScope()
{
    ++g_renderWalkDepth;
}

RenderWalkScope::~RenderWalkScope()
{
    --g_renderWalkDepth;
}

std::vector<std::string> renderWalkMutations()
{
    std::set<std::string> entries;
    for(const auto &reported : renderWalkMutationRecord())
        entries.insert(reported.second);
    return std::vector<std::string>(entries.begin(), entries.end());
}

void resetRenderWalkMutations()
{
    renderWalkMutationRecord().clear();
}

size_t queuedRenderCount()
{
    return g_renderQueue.size()-std::count(g_renderQueue.begin(), g_renderQueue.end(), nullptr);
}

void Base::appendToRenderQueue()
{
    m_renderQueueIndex=(int)g_renderQueue.size();
    g_renderQueue.push_back(this);
}

Base::Base(const String &name)
{

    m_id=g_controls.getNextId(this);

    m_parent=nullptr;
    m_actualParent=nullptr;
    m_ancestorHidden=false;
    m_skin=nullptr;

    m_alignment=Alignment::Absolute;

    init(name);

    addStateChange(StateChange_Created);
}

void Base::init(const String &name)
{
    m_updatePrimitives=true;

    m_stateChange=StateChange_Nothing;
    m_sizeFlags={SizeFlag::Expand, SizeFlag::Expand};
    m_minimumSize={0, 0};
    m_maximumSize={std::numeric_limits<int>::max(), std::numeric_limits<int>::max()};
    m_layoutItem=nullptr;
//    m_parent=nullptr;
//    m_actualParent=nullptr;
    m_innerPanel=nullptr;
//    m_skin=nullptr;
    setName(name);
//    setParent(parent);
    m_hidden=false;
    // not numbered yet: renumberZRecursive() gives it its z before it is drawn
    m_zIndex=0;
    m_bounds=Rect(0, 0, 10, 10);
    m_requestedBounds=m_bounds;
    m_widthRequested=false;
    m_heightRequested=false;
    m_padding=Padding(0, 0, 0, 0);
    m_margin=Margin(0, 0, 0, 0);
    m_dock=DockPosition::None;
    m_dragAndDrop_Package=nullptr;
    restrictToParent(false);
    setMouseInputEnabled(true);
    setKeyboardInputEnabled(false);
    invalidate();
    setCursor(cursor::Normal);
    setToolTip(nullptr);
    setTabable(false);
    setShouldDrawBackground(true);
    m_disabled=false;
    m_cacheTextureDirty=true;
    m_cacheToTexture=false;
    m_includeInSize=true;
    m_updateLayout=true;
}

Base::~Base()
{
    noteRenderWalkMutation(this, "~Base");

    {
        Canvas *canvas=getCanvas();

        // Its setToolTip() control is not its child, so it is shown in a
        // canvas's ToolTip box: that canvas deletes it, whether or not this
        // control is still on it.
        Canvas *shownOn=nullptr;

        if(m_toolTip && (m_toolTip->getParent() != this))
        {
            shownOn=m_toolTip->getCanvas();
            m_toolTip=nullptr;
        }

        if(canvas)
        {
            canvas->PreDeleteCanvas(this);
            canvas->toolTipControlDeleted(this);
            canvas->popupControlDeleted(this);
        }

        // after the canvas has closed it: a menu this control owns goes too
        dropContextMenu();

        if(shownOn && (shownOn != canvas))
            shownOn->toolTipControlDeleted(this);
    }

    Base::List::iterator iter=m_children.begin();

    while(iter != m_children.end())
    {
        Base *child=*iter;
        iter=m_children.erase(iter);
        delete child;
    }

    for(AccelMap::iterator accelIt=m_accelerators.begin();
        accelIt != m_accelerators.end();
        ++accelIt)
    {
        delete accelIt->second;
    }

    m_accelerators.clear();
    setParent(nullptr);

    if(HoveredControl == this)
        HoveredControl=nullptr;

    if(KeyboardFocus == this)
        KeyboardFocus=nullptr;

    if(MouseFocus == this)
        MouseFocus=nullptr;

    draganddrop::controlDeleted(this);
#if GWENI_ANIMATE
    anim::cancel(this);
#endif

    if(m_dragAndDrop_Package)
    {
        delete m_dragAndDrop_Package;
        m_dragAndDrop_Package=nullptr;
    }

    removeSkinControl();

    // Last, after everything above that may have queued it: the queue never
    // hands out a deleted control, and a tree change reported from here on
    // does not name it.
    if(m_renderQueueIndex >= 0)
    {
        g_renderQueue[m_renderQueueIndex]=nullptr;
        ++g_renderQueueHoles;
    }
    if(g_renderingControl == this)
        g_renderingControl=nullptr;
}

namespace
{
// Set while setBounds() invalidates for its own sake: the control's children,
// and the control itself once its size changed. Those are placement, which a
// running layout pass either has still to reach or is reported by setBounds.
bool g_placement=false;
// Set while invalidate() walks up to the canvas, so only its first call, the
// control that asked, is looked at.
bool g_propagating=false;
// Set while a part is made during a layout pass (Base::PartScope): its
// constructor's requests are not counted as layout code's.
bool g_makingPart=false;

// The parts of the bounds the next setBounds() call requests (see
// Base::setBounds): setPos/moveTo ask for the position, setSize for the size.
enum RequestPart
{
    RequestX=1,
    RequestY=2,
    RequestWidth=4,
    RequestHeight=8,
    RequestPosition=RequestX|RequestY,
    RequestSize=RequestWidth|RequestHeight,
    RequestAll=RequestPosition|RequestSize
};
int g_requestParts=RequestAll;

struct RequestScope
{
    explicit RequestScope(int parts): m_outer(g_requestParts) { g_requestParts=parts; }
    ~RequestScope() { g_requestParts=m_outer; }
    int m_outer;
};

}

Base::PlacementScope::PlacementScope(): m_outer(g_placement)
{
    g_placement=true;
}

Base::PlacementScope::~PlacementScope()
{
    g_placement=m_outer;
}

Base::PartScope::PartScope(): m_outer(g_makingPart)
{
    g_makingPart=true;
}

Base::PartScope::~PartScope()
{
    g_makingPart=m_outer;
}

bool Base::laidOutThisPass(Dim dim) const
{
    const unsigned pass=(dim == Dim::X)?m_layoutPassX:m_layoutPassY;

    return Canvas::s_layingOut && pass == Canvas::s_passSerial;
}

void Base::stampLayoutPass(Dim dim)
{
    if(!Canvas::s_layingOut)
        return;

    if(dim == Dim::X)
        m_layoutPassX=Canvas::s_passSerial;
    else
        m_layoutPassY=Canvas::s_passSerial;
}

void Base::stampLayoutPass()
{
    stampLayoutPass(Dim::X);
    stampLayoutPass(Dim::Y);
}

// The control stays dirty for the next pass, whatever this one still does to
// it (see doArrange), and the canvas runs that pass.
void Base::noteRedirty()
{
    m_redirtiedPass=Canvas::s_passSerial;
    Canvas::s_layingOut->layoutInvalidated(this);
}

bool Base::inLayoutPass()
{
    return Canvas::s_layingOut != nullptr;
}

// During a layout pass (Canvas::layoutPass) a layout invalidation either feeds
// the pass or re-dirties it. Placement feeds it: the pass reaches the control
// later and lays it out with its new bounds. Anything else raised during the
// pass (setHidden, setDock, an explicit invalidate() from a layout() or an
// onChildBoundsChanged()) may change what a control already laid out read, so
// it re-dirties and the canvas runs another pass. A resize re-dirties only when
// it hits a control this pass has already laid out; setBounds reports that.
void Base::invalidate(int state)
{
    if((state & StateChange_Created) || 
        (state & StateChange_Visibility) ||
        (state & StateChange_Geometry))
    {
        m_needsLayout=true;
        m_updateLayout=true;

        // A pass walks the children a control actually holds: a child of a
        // control with an inner panel is held by the panel, which the
        // invalidation of the logical parent below does not reach.
        for(Base *holder=m_actualParent; holder && holder != m_parent; holder=holder->m_actualParent)
            holder->m_updateLayout=true;

        if(Canvas::s_layingOut && !g_propagating && !g_placement)
            noteRedirty();
    }

    m_cacheTextureDirty=true;

    const bool outer=g_propagating;
    g_propagating=true;
    invalidateParent(state);
    g_propagating=outer;
}

void Base::delayedDelete()
{
    Canvas *canvas=getCanvas();
    canvas->addDelayedDelete(this);
}

Canvas *Base::getCanvas()
{
    Base *canvas=m_parent;

    if(!canvas)
        return nullptr;

    return canvas->getCanvas();
}

void Base::redraw()
{
    updateColors();
    m_cacheTextureDirty=true;
    addStateChange(StateChange_Background);

    // One hop to the canvas instead of walking the ancestors: their colors do
    // not depend on this control's state, and re-running updateColors() on
    // each of them dirtied their text whenever a descendant was hovered.
    if(Canvas *canvas=getCanvas())
        canvas->redraw();
}

void Base::setParent(Base *parent)
{
    if(m_parent == parent)
        return;

    if(m_parent)
        m_parent->removeChild(this);

    m_parent=parent;
    m_actualParent=nullptr;

    if(m_parent)
    {
        m_parent->addChild(this);
    }
}

void Base::setDock(DockPosition dock)
{
    if(m_dock == dock)
        return;

    m_dock=dock;
    invalidate();
    invalidateParent();
    invalidateParentReaders();
}

DockPosition Base::getDock() const
{
    return m_dock;
}

Alignment Base::getAlignment()
{
    return m_alignment;
}

void Base::setAlignment(Alignment alignment)
{
    if(alignment == m_alignment)
        return;

    m_alignment=alignment;
    invalidate();
}

SizeFlags Base::getSizeFlags()
{
    return m_sizeFlags;
}

void Base::setSizeFlags(SizeFlags sizeFlags)
{
    if((m_sizeFlags.horizontal==sizeFlags.horizontal)&&
        (m_sizeFlags.vertical==sizeFlags.vertical))
        return;

    m_sizeFlags=sizeFlags;
    invalidate();
    invalidateParent();
}

void Base::setLayout(LayoutItem *layoutItem)
{
    if(m_innerPanel)
    {
        m_innerPanel->setLayout(layoutItem);
        return;
    }

    //library owns items so we need to delete
    if(m_layoutItem!=nullptr)
        delete m_layoutItem;

    m_layoutItem=layoutItem;
    m_layoutItem->setParent(this);

    for(Base *child:m_children)
        m_layoutItem->addControl(child);
}

LayoutItem *Base::getLayout()
{
    if(m_innerPanel)
        return m_innerPanel->getLayout();
    return m_layoutItem;
}

bool Base::hidden() const
{
    return m_hidden;
}

bool Base::visible() const
{
    return !m_hidden && !m_ancestorHidden;
}

void Base::setHidden(bool hidden)
{
    if(m_hidden == hidden)
        return;

    m_hidden=hidden;

    if(m_parent)
        m_parent->onChildHiddenChanged(this);

    // Under a hidden ancestor it is not shown either way: only its own flag
    // changes, and nothing below it.
    if(!m_ancestorHidden)
    {
        addStateChange(StateChange_Visibility);

        for(auto &&child : m_children)
            child->setAncestorHidden(hidden);
    }

    invalidate();
    redraw();
}

void Base::setAncestorHidden(bool ancestorHidden)
{
    if(m_ancestorHidden == ancestorHidden)
        return;

    m_ancestorHidden=ancestorHidden;

    // It hid itself: neither it nor anything under it changes.
    if(m_hidden)
        return;

    addStateChange(StateChange_Visibility);

    for(auto &&child : m_children)
        child->setAncestorHidden(ancestorHidden);
}

void Base::invalidateChildren(bool recursive)
{
    for(auto &&child : m_children)
    {
        child->invalidate();

        if(recursive)
            child->invalidateChildren(recursive);
    }

    if(m_innerPanel)
    {
        for(auto &&innerchild : m_innerPanel->getChildren())
        {
            innerchild->invalidate();

            if(recursive)
                innerchild->invalidateChildren(recursive);
        }
    }
}

void Base::invalidateParentReaders()
{
    for(auto &&child : m_children)
    {
        if(child->readsParentLayout())
            child->invalidate();
    }
}

void Base::sendToBack()
{
    noteRenderWalkMutation(this, "sendToBack");

    if(!m_actualParent)
        return;

    if(m_actualParent->m_children.front() == this)
        return;

    m_actualParent->m_children.remove(this);
    m_actualParent->m_children.push_front(this);
    zOrderMoved(this);
    invalidateParent();
}

void Base::bringToFront()
{
    noteRenderWalkMutation(this, "bringToFront");

    if(!m_actualParent)
        return;

    if(m_actualParent->m_children.back() == this)
        return;

    m_actualParent->m_children.remove(this);
    m_actualParent->m_children.push_back(this);
    zOrderMoved(this);
    invalidateParent();
    redraw();
}

controls::Base *Base::findChildByName(const String &name, bool recursive)
{
    for(auto &&child : m_children)
    {
        if(!child->getName().empty() && child->getName() == name)
            return child;

        if(recursive)
        {
            controls::Base *subChild=child->findChildByName(name, true);

            if(subChild)
                return subChild;
        }
    }

    return nullptr;
}

void Base::bringNextToControl(controls::Base *child, bool bBehind)
{
    noteRenderWalkMutation(this, "bringNextToControl");

    if(!m_actualParent)
        return;

    m_actualParent->m_children.remove(this);
    Base::List::iterator it=std::find(
        m_actualParent->m_children.begin(), m_actualParent->m_children.end(), child);

    if(it == m_actualParent->m_children.end())
        return bringToFront();

    if(bBehind)
    {
        ++it;

        if(it == m_actualParent->m_children.end())
            return bringToFront();
    }

    m_actualParent->m_children.insert(it, this);
    zOrderMoved(this);
    invalidateParent();
}

void Base::addChild(Base *child)
{
    if(m_innerPanel)
    {
        m_innerPanel->addChild(child);
        return;
    }

    // the control whose list changes, not one forwarding to its inner panel
    noteRenderWalkMutation(this, "addChild");
    m_children.push_back(child);

    if(m_layoutItem)
        m_layoutItem->addControl(child);

    onChildAdded(child);
    child->m_actualParent=this;
    // Not visible under a hidden parent, e.g. content built into a PageControl
    // page that is hidden before its content is added; its own flag is kept.
    child->setAncestorHidden(!visible());
    // new, or moved here from another place: its old labels mean nothing here
    child->m_zMoved=true;
    child->markZDirty();
    zOrderChanged();
}

void Base::addChildOfLayout(Base *child)
{
    noteRenderWalkMutation(this, "addChildOfLayout");
    m_children.push_back(child);

    onChildAdded(child);
    child->m_actualParent=this;
    child->setAncestorHidden(!visible());
    // new, or moved here from another place: its old labels mean nothing here
    child->m_zMoved=true;
    child->markZDirty();
    zOrderChanged();
}

void Base::removeChild(Base *child)
{
    noteRenderWalkMutation(this, "removeChild");

    // If we removed our innerpanel, remove our pointer to it
    if(m_innerPanel == child)
        m_innerPanel=nullptr;

    if(m_innerPanel)
        m_innerPanel->removeChild(child);

    if(m_layoutItem)
        m_layoutItem->removeControl(child);

    m_children.remove(child);

    // not in our list any more, so its changes must not mark us (nor reach
    // through a stale pointer once we are deleted)
    if(child->m_actualParent == this)
    {
        child->m_actualParent=nullptr;
        // On its own it is shown unless it hid itself; a new parent sets this
        // again as it is attached.
        child->setAncestorHidden(false);
    }

    zChildRemoved();
    onChildRemoved(child);
}

void Base::addStateChangeRecursive(int stateChange)
{
    for(Base *child:m_children)
    {
        child->addStateChangeRecursive(stateChange);
    }

    addStateChange(stateChange);
}

void Base::removeAllChildren()
{
    while(m_children.size() > 0)
    {
        removeChild(*m_children.begin());
    }
}

unsigned int Base::numChildren()
{
    // Include m_innerPanel's children here?
    return static_cast<unsigned int>(m_children.size());
}

controls::Base *Base::getChild(unsigned int i)
{
    if(i >= numChildren())
        return nullptr;

    for(auto &&child : m_children)
    {
        if(i == 0)
            return child;

        i--;
    }

    // Should never happen.
    return nullptr;
}

void Base::onChildAdded(Base * /*child*/)
{
    invalidate();
}

void Base::onChildRemoved(Base * /*child*/)
{
    invalidate();
}

skin::Base *Base::getSkin(void)
{
    if(m_skin)
        return m_skin;

    if(m_parent)
        return m_parent->getSkin();

    GWENI_ASSERT_MSG(false, "Base::GetSkin Returning null!");
    return nullptr;
}

void Base::moveBy(int x, int y)
{
    moveTo(getX()+x, getY()+y);
}

void Base::moveTo(int x, int y)
{
    const Point pos=restrictedPos(x, y);

    RequestScope request(RequestPosition);
    setBounds(pos.x, pos.y, getWidth(), getHeight());
}

Point Base::restrictedPos(int x, int y) const
{
    if(m_restrictToParent && getParent())
    {
        const Base *parent=getParent();

        if(x-getPadding().left < parent->getMargin().left)
            x=parent->getMargin().left+getPadding().left;

        if(y-getPadding().top < parent->getMargin().top)
            y=parent->getMargin().top+getPadding().top;

        if(x+getWidth()+getPadding().right > parent->getWidth()-parent->getMargin().right)
            x=parent->getWidth()-parent->getMargin().right-getWidth()-getPadding().right;

        if(y+getHeight()+getPadding().bottom > parent->getHeight()-parent->getMargin().bottom)
            y=parent->getHeight()-parent->getMargin().bottom-getHeight()-getPadding().bottom;
    }

    return Point(x, y);
}

void Base::setPos(int x, int y)
{
    RequestScope request(RequestPosition);
    setBounds(x, y, getWidth(), getHeight());
}

void Base::setWidth(int w)
{
    RequestScope request(RequestWidth);
    setBounds(getX(), getY(), w, getHeight());
}

void Base::setHeight(int h)
{
    RequestScope request(RequestHeight);
    setBounds(getX(), getY(), getWidth(), h);
}

bool Base::setSize(int w, int h)
{
    RequestScope request(RequestSize);
    return setBounds(getX(), getY(), w, h);
}

bool Base::setSize(const Point &p)
{
    return setSize(p.x, p.y);
}

bool Base::setBounds(const Rect &bounds)
{
    // Taken once: any setBounds() the code below makes is a call of its own.
    const int parts=g_requestParts;
    g_requestParts=RequestAll;

    if(parts)
    {
        // Only the app asks; layout code computes (setArrangedBounds,
        // placeChild). A request made while a pass runs is counted.
        if(Canvas::s_layingOut && !g_makingPart)
            Canvas::s_layingOut->layoutRequested(this);

        const Rect requested=m_requestedBounds;

        if(parts & RequestX)
            m_requestedBounds.x=bounds.x;
        if(parts & RequestY)
            m_requestedBounds.y=bounds.y;
        if(parts & RequestWidth)
            m_requestedBounds.w=bounds.w;
        if(parts & RequestHeight)
            m_requestedBounds.h=bounds.h;

        const bool newRequest=((parts & RequestWidth) && !m_widthRequested) ||
            ((parts & RequestHeight) && !m_heightRequested);

        m_widthRequested=m_widthRequested || (parts & RequestWidth);
        m_heightRequested=m_heightRequested || (parts & RequestHeight);

        // A new request is laid out again: the requested size feeds the rules
        // (dockNoneSize, layoutCellSize) and the position counts in the
        // parent's preferred size, whether or not the bounds already match.
        // From inside a pass it is layout code putting a control where it
        // already is (TabStrip sizing itself, say): no re-dirty.
        if(!inLayoutPass() && (newRequest || !(requested == m_requestedBounds)))
        {
            invalidate();

            if(newRequest || requested.w != m_requestedBounds.w || requested.h != m_requestedBounds.h)
                invalidateParentReaders();
        }
    }

    if(m_bounds == bounds)
        return false;

    // A width is read from the X arrange on, a height from the Y arrange on
    // (layout() runs again there if the height changed, see Base::arrange).
    if((m_bounds.w != bounds.w && laidOutThisPass(Dim::X)) ||
        (m_bounds.h != bounds.h && laidOutThisPass(Dim::Y)))
        noteRedirty();

    // A new size is what the subtree is laid out in. A move is not: children
    // are placed relative to it, and onBoundsChanged marks the subtree for
    // the render walk (absolute positions, clip boxes); the move is laid out
    // as a request of it or as its parent's placement, below.
    if(m_bounds.w != bounds.w || m_bounds.h != bounds.h)
    {
        PlacementScope placement;
        invalidateChildren(true);
    }

    // The pass arranges it again at its new bounds, and its own layout() runs
    // again: one may correct where its parent put it (Menu::layout keeps the
    // menu inside the canvas). The way down to it is marked, not invalidated:
    // no layout() of an ancestor reads it, and the parent placing it in a
    // pass is on that way already.
    m_needsLayout=true;

    for(Base *control=this; control;
        control=control->m_actualParent?control->m_actualParent:control->m_parent)
        control->m_updateLayout=true;

    const Rect oldBounds=getBounds();
    m_bounds=bounds;
    onBoundsChanged(oldBounds);

    addStateChange(StateChange_Geometry);

    return true;
}

bool Base::setBounds(int x, int y, int w, int h)
{
    return setBounds(Rect(x, y, w, h));
}

bool Base::setArrangedBounds(int x, int y, int w, int h)
{
    RequestScope request(0);
    return setBounds(x, y, w, h);
}

bool Base::placeChild(Base *child, const Rect &bounds)
{
    RequestScope request(0);
    PlacementScope placement;
    return child->setBounds(bounds);
}

bool Base::placeChild(Base *child, int x, int y, int w, int h)
{
    return placeChild(child, Rect(x, y, w, h));
}

bool Base::placeChildPos(Base *child, int x, int y)
{
    return placeChild(child, Rect(x, y, child->getWidth(), child->getHeight()));
}

bool Base::placeChildSize(Base *child, int w, int h)
{
    return placeChild(child, Rect(child->getX(), child->getY(), w, h));
}

void Base::setPlacement(Placement placement)
{
    if(m_placement == placement)
        return;

    m_placement=placement;
    // The parent's size and arrange read it.
    invalidateParent();
}

Size Base::getMinimumSize()
{
    Size minimumSize=m_minimumSize;

    if(m_sizeFlags.horizontal==SizeFlag::Fixed)
        minimumSize.width=m_requestedBounds.w;
    else if((m_sizeFlags.horizontal == SizeFlag::Expand) || (m_sizeFlags.horizontal == SizeFlag::Bloat))
        minimumSize.width=std::max(minimumSize.width, m_preferredSize.width);

    if(m_sizeFlags.vertical==SizeFlag::Fixed)
        minimumSize.height=m_requestedBounds.h;
    else if((m_sizeFlags.vertical==SizeFlag::Expand)||(m_sizeFlags.vertical==SizeFlag::Bloat))
        minimumSize.height=std::max(minimumSize.height, m_preferredSize.height);

    return minimumSize;
}

Size Base::getMaximumSize()
{
    Size maximumSize=m_maximumSize;

    if(m_sizeFlags.horizontal==SizeFlag::Fixed)
        maximumSize.width=m_requestedBounds.w;
    else if(m_sizeFlags.horizontal == SizeFlag::Shrink)
        maximumSize.width=std::max(maximumSize.width, m_preferredSize.width);

    if(m_sizeFlags.vertical==SizeFlag::Fixed)
        maximumSize.height=m_requestedBounds.h;
    else if(m_sizeFlags.vertical == SizeFlag::Shrink)
        maximumSize.height=std::max(maximumSize.height, m_preferredSize.height);

    return maximumSize;
}

void Base::onBoundsChanged(Rect oldBounds)
{
    // Anything that needs to update on size changes
    // Iterate my children and tell them I've changed
    //
    if(getParent())
        getParent()->onChildBoundsChanged(oldBounds, this);

    if(m_bounds.w != oldBounds.w || m_bounds.h != oldBounds.h)
    {
        PlacementScope placement;
        invalidate();
    }

    //notify children that we moved
    addStateChangeRecursive(StateChange_Geometry);
    

//    redraw();
    updateRenderBounds();
}

// A new canvas scale (Canvas::setScale): every control is laid out again and
// re-emits its primitives, and the clip boxes they carry, at the new scale, by
// one state change each. A subclass that overrides this calls it too.
void Base::onScaleChanged()
{
    invalidate();
    addStateChange(StateChange_Geometry);

    for(auto &&child : m_children)
    {
        child->onScaleChanged();
    }
}

void Base::onChildBoundsChanged(Rect /*oldChildBounds*/, Base * /*child*/)
{}

void Base::setPrimitiveIdsSize(size_t size)
{
    //enlargePrimitiveIds(this, m_primitiveIds, size);
}

static ChangeStats g_changeStats;

const ChangeStats &getChangeStats()
{
    return g_changeStats;
}

void resetChangeStats()
{
    g_changeStats=ChangeStats();
}

// The types reported by setSkin(), each printed once when it was added.
static std::set<std::string> &typesReportedWithoutRenderer()
{
    static std::set<std::string> types;
    return types;
}

std::vector<std::string> typesWithoutSkinRenderer()
{
    const std::set<std::string> &types=typesReportedWithoutRenderer();
    return std::vector<std::string>(types.begin(), types.end());
}

static size_t g_measureTextCalls=0;

size_t measureTextCalls()
{
    return g_measureTextCalls;
}

gweni::Point Base::measureText(const gweni::Font &font, const gweni::String &text)
{
    ++g_measureTextCalls;
    return getSkin()->getRenderer()->measureText(font, text);
}

int Base::measureLineHeight(const gweni::Font &font)
{
    ++g_measureTextCalls;
    return getSkin()->getRenderer()->lineHeight(font);
}

void Base::render(skin::Base *skin)
{
//    update();
    const int stateChange=getStateChange();

    if(stateChange == StateChange_Nothing)
        return;

    //the one choke point every changed control passes through, so the whole
    //frame's change count is these few increments
    ++g_changeStats.controls;
    if(stateChange & StateChange_Created) ++g_changeStats.created;
    if(stateChange & StateChange_Visibility) ++g_changeStats.visibility;
    if(stateChange & StateChange_Geometry) ++g_changeStats.geometry;
    if(stateChange & StateChange_Background) ++g_changeStats.background;
    if(stateChange & StateChange_Text) ++g_changeStats.text;

    if(!m_skinControl)
        return;

    if(getStateChange() & StateChange_Created)
    {
        m_skinControl->generate(m_skin->getRenderer(), this);
#ifdef GWENI_DEBUG_LOGGING
        std::string name=getFullName(this);
        const std::vector<size_t> &primitives=m_skinControl->getPrimitives();
        std::string primitiveList;

        for(size_t i=0; i<primitives.size(); i++)
        {
            if(i == 0)
                primitiveList+=std::to_string(primitives[i]);
            else
                primitiveList+=", "+std::to_string(primitives[i]);
        }
        std::cout<<"Primitives: "<<name<<" ("<<primitiveList<<")\n";
#endif//GWENI_DEBUG_LOGGING
    }

#ifdef GWENI_DEBUG_LOGGING
    if(getStateChange() != StateChange_Text)
    {
        const Rect &bounds=getBounds();
        renderer::Base *baseRenderer=skin->getRenderer();
        gweni::Point clipPt=baseRenderer->getRenderOffset();
        std::string show=hidden()?"hide":"show";

        std::string name=getFullName(this);
        std::cout<<"Updating: "<<show<<" ("<<bounds.x+clipPt.x<<", "<<bounds.y+clipPt.y<<", w:"<<bounds.w<<", h:"<<bounds.h<<") "<<name<<"\n";
    }
#endif//GWENI_DEBUG_LOGGING

    m_skinControl->update(m_skin->getRenderer(), this);

}

void Base::removeSkinControl()
{
    if(m_skinControl)
    {
        m_skinControl->remove(m_skin->getRenderer(), this);
        m_skinControl=nullptr;
        zOrderChanged();
    }
}

bool Base::placeZRange(ZNumbering &numbering, int &z, int layers, bool take)
{
    int placed=z;
    int gap=0;

    if(take || numbering.full || (z < numbering.cursor))
    {
        placed=numbering.cursor;
        // Packed when a kept label may follow (a short cascade); past every
        // kept label nothing follows, so the gap costs nothing there but range.
        if(numbering.full)
            gap=numbering.gap;
        else if(numbering.cursor > numbering.high)
            gap=(z != 0)?numbering.movedTailGap:numbering.tailGap;

        if(placed+layers-1 > numbering.clampTo)
        {
            placed=std::max(1, numbering.clampTo-layers+1);
            numbering.clamped=true;
        }
    }
    numbering.cursor=placed+layers+gap;
    numbering.maxUsed=std::max(numbering.maxUsed, placed+layers-1);

    const bool changed=(placed != z);
    z=placed;
    return changed;
}

void Base::renumberZRecursive(ZNumbering &numbering, bool moved)
{
    ++numbering.stats->zVisited;
    // a moved control's subtree is out of order wherever it kept its labels
    moved=moved || m_zMoved;
    m_zMoved=false;
    m_zDirty=false;

    const int layers=m_skinControl?(int)m_skinControl->depthLayers():1;
    bool changed=placeZRange(numbering, m_zIndex, layers, moved);

    for(auto &&child:m_children)
    {
        // Nothing changed in it since it was numbered, so its ranges are in
        // order and, its first still past the cursor, it keeps every label: a
        // pass that entered it would leave the cursor at its end.
        if(numbering.skipClean && !moved && !child->m_zDirty
            && (child->m_zIndex >= numbering.cursor))
        {
            numbering.cursor=child->m_zEnd;
            numbering.maxUsed=std::max(numbering.maxUsed, child->m_zEnd-1);
            continue;
        }
        child->renumberZRecursive(numbering, moved);
    }

    const int overlay=m_skinControl?(int)m_skinControl->overlayLayers():0;

    if(overlay)
        changed|=placeZRange(numbering, m_overlayZIndex, overlay, moved);
    else if(m_overlayZIndex != 0)
    {
        m_overlayZIndex=0;
        changed=true;
    }

    m_zEnd=zSubtreeEnd();

    if(changed)
    {
        ++numbering.stats->zChanged;
        addStateChange(StateChange_Geometry);
    }
}

int Base::zSubtreeEnd() const
{
    const int overlay=m_skinControl?(int)m_skinControl->overlayLayers():0;

    if(overlay)
        return m_overlayZIndex+overlay;
    if(!m_children.empty())
        return m_children.back()->m_zEnd;
    return m_zIndex+(m_skinControl?(int)m_skinControl->depthLayers():1);
}

void Base::countZRanges(size_t &layers, size_t &ranges) const
{
    layers+=m_skinControl?m_skinControl->depthLayers():1;
    ++ranges;

    for(const Base *child:m_children)
        child->countZRanges(layers, ranges);

    if(const size_t overlay=m_skinControl?m_skinControl->overlayLayers():0)
    {
        layers+=overlay;
        ++ranges;
    }
}

void Base::zOrderMoved(Base *control)
{
    control->m_zMoved=true;
    control->zOrderChanged();
}

void Base::zOrderChanged()
{
    markZDirty();

    // A control not on a canvas yet is numbered once it is: attaching it is an
    // addChild() on the canvas's tree, which reports there.
    if(Canvas *canvas=getCanvas())
        canvas->m_zOrderDirty=true;
}

void Base::zChildRemoved()
{
    // A dirty control is entered by the next renumber, which sets its end, and
    // its ancestors are dirty too.
    for(Base *control=this; control && !control->m_zDirty; control=control->m_actualParent)
    {
        const int end=control->zSubtreeEnd();

        if(end == control->m_zEnd)
            break;
        control->m_zEnd=end;
    }

    if(Canvas *canvas=getCanvas())
        canvas->m_zOrderDirty=true;
}

// The drain's per-control step, inlined into both of its loops (GCC and
// Clang leave a lambda called twice out of line).
#if defined(__GNUC__) || defined(__clang__)
#define GWENI_RENDER_INLINE __attribute__((always_inline))
#else
#define GWENI_RENDER_INLINE
#endif

namespace
{
// One level of the drain's ancestor stack: a control of the drained tree, and
// the render offset and clip region its children render in (before a
// getChildClipRect() of its own narrows one child's). The control is only
// compared, never dereferenced: a hook may have deleted it.
struct RenderLevel
{
    Base *control;
    Point offset;
    Rect clip;
};

// A queue slot to render and its z, for the sort.
struct RenderOrder
{
    int z;
    unsigned slot;

    bool operator<(const RenderOrder &other) const
    {
        return (z != other.z)?(z < other.z):(slot < other.slot);
    }
};

// Kept from frame to frame, so the render phase allocates nothing once they
// have grown.
std::vector<RenderLevel> g_renderStack;
std::vector<Base *> g_renderChain;
std::vector<RenderOrder> g_renderOrder;
std::vector<RenderOrder> g_renderOrderScratch;

// Entries queued during a drain (by a hook, on another control) that it
// renders after the sorted ones, at least: the rest waits for the next frame,
// so a hook that keeps queueing cannot keep the drain going.
constexpr size_t MinQueuedDuringDrain=256;

// Sorts \p order, built in slot order, by z and then slot: nothing to do if it
// is in z order already (a whole tree queued by a layout pass mostly is), else
// a stable radix sort on the bytes z uses, which keeps the slot order of ties.
void sortRenderOrder(std::vector<RenderOrder> &order)
{
    bool sorted=true;
    bool negative=false;
    unsigned highest=0;

    for(size_t i=0; i < order.size(); ++i)
    {
        const int z=order[i].z;

        if(z < 0)
            negative=true;
        else
            highest=std::max(highest, (unsigned)z);
        if((i > 0) && (z < order[i-1].z))
            sorted=false;
    }

    if(sorted)
        return;
    if(negative)
    {
        std::sort(order.begin(), order.end());
        return;
    }

    std::vector<RenderOrder> &scratch=g_renderOrderScratch;
    scratch.resize(order.size());

    for(unsigned shift=0; (shift < 32) && ((highest>>shift) != 0); shift+=8)
    {
        size_t counts[256]={};

        for(const RenderOrder &entry : order)
            ++counts[((unsigned)entry.z>>shift)&0xff];

        size_t at=0;
        for(size_t &count : counts)
        {
            const size_t n=count;
            count=at;
            at+=n;
        }

        for(const RenderOrder &entry : order)
            scratch[counts[((unsigned)entry.z>>shift)&0xff]++]=entry;
        order.swap(scratch);
    }
}
}//namespace

void Base::drainRenderQueue(skin::Base *skin, bool zTied, size_t treeSize)
{
    if(g_draining)
        return;
    g_draining=true;
    // a tree change from a hook is reported, and seen by this drain
    RenderWalkScope walk;

    renderer::Base *baseRenderer=skin->getRenderer();
    std::vector<Base *> &queue=g_renderQueue;

    // Drops the slots of deleted (and rendered) controls; the indices move.
    const auto compact=[&queue]()
        {
            size_t kept=0;

            for(Base *control : queue)
            {
                if(!control)
                    continue;
                control->m_renderQueueIndex=(int)kept;
                queue[kept++]=control;
            }
            queue.resize(kept);
            g_renderQueueHoles=0;
        };
    if(g_renderQueueHoles != 0)
        compact();

    const size_t queuedBefore=queue.size();
    const size_t appendCap=std::max(MinQueuedDuringDrain, queuedBefore);

    // This root's own state, as the canvas set it on the renderer.
    const Point rootOffset=baseRenderer->getRenderOffset();
    const Rect rootClip=baseRenderer->clipRegion();
    unsigned treeChanges=g_renderWalkTreeChanges;

    // Runs the hooks of \p control, queued in \p slot, at (offset, clip), and
    // clears its state; each hook but the first only if the ones before did not
    // delete the control (~Base nulls its slot). Returns whether it is still
    // there.
    const auto renderQueued=[&](Base *control, size_t slot, const Point &offset, const Rect &clip)
        GWENI_RENDER_INLINE
        {
            ++g_changeStats.visited;
            g_renderingControl=control;

            baseRenderer->setRenderOffset(offset);
            baseRenderer->setClipRegion(clip);
            control->renderUnder(skin);
            if(queue[slot] == control)
                control->render(skin);

            if(queue[slot] == control)
            {
                // its own offset, its parent's region
                baseRenderer->setRenderOffset(offset);
                baseRenderer->addRenderOffset(control->getBounds());
                baseRenderer->setClipRegion(clip);
                baseRenderer->startClip();
                control->renderOver(skin);
                if(queue[slot] == control)
                    control->renderFocus(skin);
                baseRenderer->endClip();
            }

            g_renderingControl=nullptr;

            if(queue[slot] != control)
                return false;

            // after the hooks: a change it raised on itself is cleared too
            control->m_stateChange=StateChange_Nothing;
            control->m_renderQueueIndex=-1;
            queue[slot]=nullptr;
            return true;
        };
    // The state \p control's children render in, \p control rendering at
    // (offset, clip): its bounds' origin added, and its bounds clipped to if
    // it clips (renderer::Base::addClipRegion).
    const auto innerState=[&](Base *control, const Point &offset, const Rect &clip,
        Point &innerOffset, Rect &innerClip)
        {
            const Rect &bounds=control->getBounds();

            innerOffset.x=offset.x+bounds.x;
            innerOffset.y=offset.y+bounds.y;
            if(control->shouldClip())
            {
                baseRenderer->setRenderOffset(innerOffset);
                baseRenderer->setClipRegion(clip);
                baseRenderer->addClipRegion(bounds);
                innerClip=baseRenderer->clipRegion();
            }
            else
                innerClip=clip;
        };
    // The state \p child of \p parent renders in, \p parent's children's being
    // (offset, clip): narrowed by the parent's getChildClipRect() for it.
    const auto childState=[&](Base *parent, Base *child, const Point &offset, Rect &clip)
        {
            if(const Rect *childClip=parent->getChildClipRect(child))
            {
                baseRenderer->setRenderOffset(offset);
                baseRenderer->setClipRegion(clip);
                baseRenderer->addRenderOffset(*childClip);
                baseRenderer->addClipRegion(*childClip);
                clip=baseRenderer->clipRegion();
            }
        };

    // A change over much of the tree (the first frame, a resize, a scale) is
    // rendered by one walk of it, in preorder, as the recursive walk did: no
    // sort, no ancestor chains. So is every frame of a tree whose z ties. It
    // stops at a tree change made by a hook, without touching the lists it
    // was in, and leaves the rest to the queue's order below.
    // the queue's slots below queuedBefore the walk emptied
    size_t walked=0;

    if(zTied || ((treeSize != 0) && (queuedBefore*4 >= treeSize)))
    {
        bool stopped=false;
        const auto walkTree=[&](const auto &self, Base *control, const Point &offset,
            const Rect &clip) -> void
            {
                const int slot=control->m_renderQueueIndex;

                if(slot >= 0)
                {
                    if((size_t)slot < queuedBefore)
                        ++walked;
                    if(control->m_stateChange == StateChange_Nothing)
                    {
                        control->m_renderQueueIndex=-1;
                        queue[slot]=nullptr;
                    }
                    else if(!renderQueued(control, (size_t)slot, offset, clip)
                        || (treeChanges != g_renderWalkTreeChanges))
                    {
                        stopped=true;
                        return;
                    }
                }

                if(control->m_children.empty())
                    return;

                Point innerOffset;
                Rect innerClip;

                innerState(control, offset, clip, innerOffset, innerClip);
                for(Base *child : control->m_children)
                {
                    Rect clipForChild=innerClip;

                    childState(control, child, innerOffset, clipForChild);
                    self(self, child, innerOffset, clipForChild);
                    // before the loop steps on through a list a hook changed
                    if(stopped)
                        return;
                }
            };
        walkTree(walkTree, this, rootOffset, rootClip);
        treeChanges=g_renderWalkTreeChanges;
        // stopped: what it had not reached yet goes through the queue's order
        if(stopped)
            walked=0;
    }

    // The queue's order for what is left: z is the tree's preorder (a parent
    // before its children, a child before the next one's subtree), strictly
    // increasing unless the renumber clamped. Entries off this tree sort in
    // among them and are passed over below.
    std::vector<RenderOrder> &order=g_renderOrder;
    order.clear();
    if(walked < queuedBefore)
    {
        for(size_t slot=0; slot < queuedBefore; ++slot)
        {
            if(Base *control=queue[slot])
                order.push_back({control->getZIndex(), (unsigned)slot});
        }
        sortRenderOrder(order);
    }

    // The stack's levels are stack[0, depth): the vector only grows.
    std::vector<RenderLevel> &stack=g_renderStack;
    std::vector<Base *> &chain=g_renderChain;
    size_t depth=0;

    // Pushes \p control, rendering at (offset, clip), with the state its
    // children render in.
    const auto pushLevel=[&](Base *control, const Point &offset, const Rect &clip)
        {
            if(depth == stack.size())
                stack.emplace_back();

            RenderLevel &level=stack[depth];

            level.control=control;
            innerState(control, offset, clip, level.offset, level.clip);
            control->m_renderStackLevel=(int)depth;
            ++depth;
        };
    // The state \p control renders in, its parent at the top of the stack.
    const auto stateUnderTop=[&](Base *control, Point &offset, Rect &clip)
        {
            const RenderLevel &top=stack[depth-1];

            offset=top.offset;
            clip=top.clip;
            childState(control->m_actualParent, control, offset, clip);
        };
    // The stack back to this root alone.
    const auto resetStack=[&]()
        {
            depth=0;
            pushLevel(this, rootOffset, rootClip);
        };
    resetStack();

    const size_t sorted=order.size();
    size_t next=0;
    size_t appended=queuedBefore;
    // entries passed over, still queued
    size_t carried=0;
    // The control rendered last and the state it rendered in, pushed on the
    // stack only if the next entry is its child: a leaf costs no level.
    Base *last=nullptr;
    Point lastOffset;
    Rect lastClip;

    for(;;)
    {
        // the sorted entries, then those queued since, in the order they came
        size_t slot;

        if(next < sorted)
            slot=order[next++].slot;
        else if((appended < queue.size()) && (appended-queuedBefore < appendCap))
            slot=appended++;
        else
            break;

        Base *control=queue[slot];

        if(!control)
            continue;

        if(control->m_stateChange == StateChange_Nothing)
        {
            control->m_renderQueueIndex=-1;
            queue[slot]=nullptr;
            continue;
        }

        // a hook moved or deleted controls: the stack may name them
        if(treeChanges != g_renderWalkTreeChanges)
        {
            treeChanges=g_renderWalkTreeChanges;
            last=nullptr;
            resetStack();
        }

        Point offset=rootOffset;
        Rect clip=rootClip;

        if(control != this)
        {
            Base *ancestor=control->m_actualParent;

            // a child of the last one: its level goes on the stack
            if(last && (ancestor == last))
                pushLevel(last, lastOffset, lastClip);
            last=nullptr;

            // up to the nearest ancestor on the stack, which this root always is
            chain.clear();
            while(ancestor)
            {
                const int level=ancestor->m_renderStackLevel;

                if((level >= 0) && ((size_t)level < depth) && (stack[level].control == ancestor))
                    break;
                chain.push_back(ancestor);
                ancestor=ancestor->m_actualParent;
            }

            // detached, or on another canvas: rendered by a later drain
            if(!ancestor)
            {
                ++carried;
                continue;
            }

            depth=ancestor->m_renderStackLevel+1;

            for(auto it=chain.rbegin(); it != chain.rend(); ++it)
            {
                stateUnderTop(*it, offset, clip);
                pushLevel(*it, offset, clip);
            }
            stateUnderTop(control, offset, clip);
        }

        if(!renderQueued(control, slot, offset, clip)
            || (treeChanges != g_renderWalkTreeChanges))
            continue;

        if(control == this)
        {
            resetStack();
            last=nullptr;
        }
        else
        {
            // the next entry is often a child
            last=control;
            lastOffset=offset;
            lastClip=clip;
        }
    }

    // as the canvas left them, for whatever it draws after
    baseRenderer->setRenderOffset(rootOffset);
    baseRenderer->setClipRegion(rootClip);

    // every slot is null unless something was carried or left past the cap
    if((carried == 0) && (appended == queue.size()))
    {
        queue.clear();
        g_renderQueueHoles=0;
    }
    else
        compact();
    g_changeStats.renderCarried=queue.size();
    g_draining=false;
}

void Base::doHide(skin::Base *skin)
{
}

void Base::hideRecursive(skin::Base *skin)
{
    for(auto &&child : m_children)
    {
        child->doHide(skin);
    }
}

void Base::setSkin(skin::Base *skin, bool doChildren)
{
    if(m_skin == skin)
        return;

    // give the old skin's Control's ids back while m_skin is still the skin
    // whose renderer they came from
    removeSkinControl();

    m_skin=skin;
    m_skinControl=skin->getControlRenderer(getTypeName());

    if(!m_skinControl)
    {
        const char *parentName=getParentTypeName();
        size_t index=0;

        while(parentName)
        {
            m_skinControl=skin->getControlRenderer(parentName);

            if(m_skinControl)
                break;

            parentName=getParentTypeName(index);
            index++;
        }

        if(!m_skinControl)
        {
            std::cerr<<"Failed to get skin for: "<<getTypeName()<<"\n";
        }
#ifdef GWENI_DEBUG_LOGGING
        else
        {
            std::cout<<"Using:"<<parentName<<" skin for: "<<getTypeName()<<"\n";
        }
#endif//GWENI_DEBUG_LOGGING

        // Falling back to an intermediate ancestor's renderer is by design
        // (TextBoxNumeric draws as a TextBox); ending at the Base entry, which
        // draws nothing, means this type has no renderer of its own.
        if(m_skinControl && std::strcmp(parentName, Base::getTypeNameStatic()) == 0
            && !drawsNothing() && typesReportedWithoutRenderer().insert(getTypeName()).second)
        {
            std::cerr<<"gweni: no skin renderer for "<<getTypeName()<<"\n";
        }
    }

    // the new Control's depthLayers() may differ from the old one's
    zOrderChanged();
    invalidate();
    redraw();
    onSkinChanged(skin);

    m_stateChange=StateChange_Created;
    queueForRender();

    if(doChildren)
    {
        for(auto &&child : m_children)
        {
            child->setSkin(skin, true);
        }
    }

//    //need to set prefered size
//    calculateSize(skin, Dim::X);
//    calculateSize(skin, Dim::Y);
}

void Base::onSkinChanged(skin::Base * /*skin*/)
{
    // Do something
}

bool Base::onMouseWheeled(int iDelta)
{
    if(m_actualParent)
        return m_actualParent->onMouseWheeled(iDelta);

    return false;
}

void Base::onMouseMoved(int /*x*/, int /*y*/, int /*deltaX*/, int /*deltaY*/)
{}

void Base::onMouseEnter()
{
    onHoverEnter.call(this);
    redraw();
    notifyAncestorsOfInteraction();
}

void Base::onMouseLeave()
{
    onHoverLeave.call(this);
    redraw();
    notifyAncestorsOfInteraction();
}

void Base::notifyAncestorsOfInteraction()
{
    for(Base *ancestor=getParent(); ancestor; ancestor=ancestor->getParent())
        ancestor->onDescendantInteraction(this);
}

bool Base::isHovered() const
{
    return HoveredControl == this;
}

bool Base::shouldDrawHover() const
{
    return MouseFocus == this || MouseFocus == nullptr;
}

bool Base::isFocussed()
{
    return KeyboardFocus == this;
}

void Base::focus()
{
    if(KeyboardFocus == this)
        return;

    Base *previous=KeyboardFocus;
    if(previous)
        previous->onLostKeyboardFocus();

    KeyboardFocus=this;

    // The outgoing holder repaints too, once KeyboardFocus has moved on, so its
    // skin stops picking the focused look.
    if(previous)
    {
        previous->redraw();
        previous->notifyAncestorsOfInteraction();
    }

    onKeyboardFocus();
    redraw();
    notifyAncestorsOfInteraction();
}

void Base::unfocus()
{
    if(KeyboardFocus != this)
        return;

    KeyboardFocus=nullptr;
    onLostKeyboardFocus();
    redraw();
    notifyAncestorsOfInteraction();
}

bool Base::isOnTop()
{
    if(!getParent())
        return false;

    Base::List::iterator iter=getParent()->m_children.begin();
    Base *child=*iter;

    if(child == this)
        return true;

    return false;
}

void Base::touch()
{
    if(getParent())
        getParent()->onChildTouched(this);
}

void Base::onChildTouched(controls::Base * /*child*/)
{
    touch();
}

// input::hitTestVisits(), defined at the end of this file.
static size_t g_hitTestVisits=0;

void Base::countHitTestVisit()
{
    ++g_hitTestVisits;
}

Base *Base::getControlAt(int x, int y, bool bOnlyIfMouseEnabled)
{
    countHitTestVisit();
    return getControlAtWalk(x, y, bOnlyIfMouseEnabled);
}

Base *Base::getControlAtWalk(int x, int y, bool bOnlyIfMouseEnabled)
{
    if(hidden())
        return nullptr;

    if(x < 0 || y < 0 || x >= getWidth() || y >= getHeight())
        return nullptr;

    for(Base::List::reverse_iterator iter=m_children.rbegin(); iter != m_children.rend(); ++iter)
    {
        Base *child=*iter;

        // outside the rect this control clips the child to, it is not there
        if(const Rect *childClip=getChildClipRect(child))
        {
            if(x < childClip->x || y < childClip->y ||
               x >= childClip->x+childClip->w || y >= childClip->y+childClip->h)
                continue;
        }

        Base *found=child->getControlAt(x-child->getX(), y-child->getY(), bOnlyIfMouseEnabled);

        if(found)
            return found;
    }

    if(bOnlyIfMouseEnabled && !getMouseInputEnabled())
        return nullptr;

    return this;
}

void Base::layout(skin::Base *skin)
{
//    if(skin->getRenderer()->getCTT() && isCachedToTexture())
//        skin->getRenderer()->getCTT()->createControlCacheTexture(this, this->getBounds().getSize());
}

std::string info(Base *control, Dim dim)
{
    const Rect &bounds=control->getBounds();
    const Size &preferredSize=control->getPreferredSize();
    const Margin &margin=control->getMargin();
    const Padding &padding=control->getPadding();
    const SizeFlags &sizeFlags=control->getSizeFlags();

    std::ostringstream output;

    output<<control->getTypeName()<<" : "<<control->getName()<<"\n";
    if(dim==Dim::X)
        output<<"    X dim\n";
    else
        output<<"    Y dim\n";

    output<<"    Bounds: "<<bounds.x<<", "<<bounds.y<<", "<<bounds.w<<", "<<bounds.h<<"\n";
    output<<"    Preferred: "<<preferredSize.width<<", "<<preferredSize.height<<"\n";
    output<<"    Margin: "<<margin.top<<", "<<margin.bottom<<", "<<margin.left<<", "<<margin.right<<"\n";
    output<<"    Padding: "<<padding.top<<", "<<padding.bottom<<", "<<padding.left<<", "<<padding.right<<"\n";
    output<<"    Size flags: "<<sizeFlagName(sizeFlags.horizontal)<<", "<<sizeFlagName(sizeFlags.horizontal)<<"\n";

    return output.str();
}

bool Base::processLayout(skin::Base *skin, Dim dim)
{
    if(m_layoutItem)
    {
        m_layoutItem->calculateSize(skin, dim);

        const Size preferredSize=m_layoutItem->getPreferredSize();

        if(dim==Dim::X)
        {
            m_preferredSize.width=preferredSize.width+m_padding.left+m_padding.right;
        }
        else
        {
            m_preferredSize.height=preferredSize.height+m_padding.top+m_padding.bottom;
        }
        return true;
    }
    return false;
}

Size Base::sizeOfChildren(skin::Base *skin, Dim dim)
{
    Size preferredSize=m_preferredSize;

    if(dim==Dim::X)
    {
        int width=0;
        int dockWidth=0;
        int innerWidth=0;

        for(auto &&child:m_children)
        {
            if(child->hidden())
                continue;

            child->doCalculateSize(skin, dim);

            // Placed by this control (Placement::Owner) or asked not to
            // count: measured for its own subtree, but not counted.
            if(!child->shouldIncludeInSize())
                continue;

            DockPosition dock=child->getDock();
            const Size &preferred=child->getPreferredSize();
            const Margin &margin=child->getMargin();
            int childWidth=preferred.width+margin.left+margin.right;

            if((dock & DockPosition::Top) || (dock & DockPosition::Bottom))
            {
                if(childWidth>width)
                    width=childWidth;
            }
            else if((dock & DockPosition::Left) || (dock & DockPosition::Right))
            {
                dockWidth+=childWidth;
            }
            else if(dock & DockPosition::Center)
            {
                innerWidth+=childWidth;
            }
            else
            {
                // Where the child asked to be, not where the last pass put it.
                if(!hasAlignment())
                    childWidth+=child->getRequestedBounds().x;

                if(childWidth>width)
                    width=childWidth;
            }
        }
        dockWidth+=innerWidth;
        width=std::max(width, dockWidth);
        width+=m_padding.left+m_padding.right;

        preferredSize.width=preferredFromContent(width, Dim::X);
    }
    else
    {
        int height=0;
        int dockHeight=0;
        int innerHeight=0;

        for(auto &&child:m_children)
        {
            if(child->hidden())
                continue;

            child->doCalculateSize(skin, dim);

            // Placed by this control (Placement::Owner) or asked not to
            // count: measured for its own subtree, but not counted.
            if(!child->shouldIncludeInSize())
                continue;

            DockPosition dock=child->getDock();
            const Size &preferred=child->getPreferredSize();
            const Margin &margin=child->getMargin();

            int childHeight=preferred.height+margin.top+margin.bottom;

            if((dock & DockPosition::Top)||(dock & DockPosition::Bottom))
            {
                dockHeight+=childHeight;
            }
            else if((dock & DockPosition::Left)||(dock & DockPosition::Right))
            {
                if(childHeight>height)
                    height=childHeight;
            }
            else if(dock & DockPosition::Center)
            {
                innerHeight+=childHeight;
            }
            else
            {
                if(!hasAlignment())
                    childHeight+=child->getRequestedBounds().y;

                if(childHeight>height)
                    height=childHeight;
            }
        }

        dockHeight+=innerHeight;
        height=std::max(height, dockHeight);
        height+=m_padding.top+m_padding.bottom;

        preferredSize.height=preferredFromContent(height, Dim::Y);
    }

    return preferredSize;
}

int Base::preferredFromContent(int contentSize, Dim dim) const
{
    const bool x=(dim == Dim::X);
    const SizeFlag flag=x?m_sizeFlags.horizontal:m_sizeFlags.vertical;
    const int requested=x?m_requestedBounds.w:m_requestedBounds.h;
    const int minimum=x?m_minimumSize.width:m_minimumSize.height;
    const int maximum=x?m_maximumSize.width:m_maximumSize.height;

    switch(flag)
    {
    case SizeFlag::Fixed:
        return requested;
    case SizeFlag::Shrink:
        return std::min(contentSize, maximum);
    case SizeFlag::Expand:
        return std::max(contentSize, minimum);
    case SizeFlag::Elastic:
        return contentSize;
    case SizeFlag::Bloat:
        return std::max(contentSize, minimum);
    }

    return contentSize;
}

void Base::calculateSize(skin::Base *skin, Dim dim)
{
    if(processLayout(skin, dim))
        return;

    m_preferredSize=sizeOfChildren(skin, dim);

    // Its content is the size it was asked for: the children were measured,
    // for their own subtrees, but do not count.
    if(sizedByRequest(dim))
    {
        if(dim == Dim::X)
            m_preferredSize.width=preferredFromContent(m_requestedBounds.w, Dim::X);
        else
            m_preferredSize.height=preferredFromContent(m_requestedBounds.h, Dim::Y);
    }
}

int Base::dockNoneSize(Dim dim, int preferred) const
{
    const bool x=(dim == Dim::X);
    const SizeFlag flag=x?m_sizeFlags.horizontal:m_sizeFlags.vertical;
    const int requested=x?m_requestedBounds.w:m_requestedBounds.h;

    if(flag == SizeFlag::Fixed)
        return requested;

    if(!hasRequestedSize(dim))
        return preferred;

    switch(flag)
    {
    case SizeFlag::Expand:
        return std::max(requested, preferred);
    case SizeFlag::Shrink:
        return std::min(requested, preferred);
    default: // Elastic and Bloat both grow and shrink to the preferred size
        return preferred;
    }
}

int Base::layoutCellSize(Dim dim, int cell) const
{
    const bool x=(dim == Dim::X);
    const SizeFlag flag=x?m_sizeFlags.horizontal:m_sizeFlags.vertical;
    const int requested=x?m_requestedBounds.w:m_requestedBounds.h;
    const int preferred=x?m_preferredSize.width:m_preferredSize.height;

    if(flag == SizeFlag::Fixed)
        return requested;

    if(!hasRequestedSize(dim))
        return cell;

    if(flag == SizeFlag::Shrink)
        return std::min(requested, cell);

    return std::min(std::max(requested, preferred), cell);
}

void Base::policyResize(const Size &preferred, Dim dim)
{
    const Rect &bounds=getBounds();

    if(dim==Dim::X)
        setArrangedBounds(bounds.x, bounds.y, dockNoneSize(dim, preferred.width), bounds.h);
    else
        setArrangedBounds(bounds.x, bounds.y, bounds.w, dockNoneSize(dim, preferred.height));
}

// Counted only inside a pass: a calculateSize() made outside one (a
// sizeToContents(), say) is no entry of a pass, and is never skipped.
void Base::doCalculateSize(skin::Base *skin, Dim dim)
{
    if(Canvas::s_layingOut)
    {
        if(!m_updateLayout && !Canvas::s_verifyingLayout)
            return;

        ++Canvas::s_layingOut->m_layoutStats.visited;
    }

    calculateSize(skin, dim);
}

// A control is dirty from its invalidation (or a new bounds) until the end of
// its Y arrange, the last step a pass enters it in, so every step of the pass
// in between enters it: a width the X arrange gave it is measured in Y. What
// is dirtied under it after its children's own Y arrange (a child placed in a
// postLayout) keeps it dirty, so the next pass finds the way down.
void Base::doArrange(skin::Base *skin, Dim dim)
{
    if(!Canvas::s_layingOut)
    {
        arrange(skin, dim);
        return;
    }

    if(!m_updateLayout && !Canvas::s_verifyingLayout)
        return;

    ++Canvas::s_layingOut->m_layoutStats.visited;

    arrange(skin, dim);

    if(dim == Dim::Y)
    {
        // re-dirtied in this pass, or a shown child still is: the next pass
        // enters it again
        m_updateLayout=(m_redirtiedPass == Canvas::s_passSerial) ||
            std::any_of(m_children.begin(), m_children.end(), [](const Base *child)
                { return !child->hidden() && child->m_updateLayout; });
    }
}

void Base::arrange(skin::Base *skin, Dim dim)
{
    // Stamped before layout(), so what it does to this control counts as
    // done after this control was laid out.
    stampLayoutPass(dim);

    // layout() runs at the start of the X arrange, when the width is final
    // but the height is the previous pass's. The Y pass gives the control its
    // height before arranging it, and a new one invalidates it again (as
    // placement, see onBoundsChanged), so layout() runs a second time here
    // with both final. What it computes in X must depend on the width only,
    // so the second run writes the same X geometry (docs/layout.md).
    if(needsLayout())
    {
        m_needsLayout=false;
        layout(skin);
    }

    if(m_layoutItem)
    {
        Rect bounds=m_bounds;

        if(dim==Dim::X)
            bounds.x=0;
        else
            bounds.y=0;
        m_layoutItem->setBounds(bounds);
        m_layoutItem->arrange(skin, dim);
        return;
    }

    if(dim==Dim::X)
        arrangeHorizontal(skin);
    else
        arrangeVertical(skin);

    for(auto &&child:m_children)
    {
        if(child->hidden())
            continue;

        child->doArrange(skin, dim);
    }

    if(dim == Dim::Y)
        postLayout(skin);
}

void Base::arrangeHorizontal(skin::Base *skin)
{
    Rect innerBounds=getBounds();

    innerBounds.x=m_padding.left;
    innerBounds.w-=m_padding.left+m_padding.right;

    // The padded content box an aligned dock-None child is placed in; unlike
    // innerBounds it does not shrink by the docked children.
    const int contentX=innerBounds.x;
    const int contentW=innerBounds.w;

    for(auto &&child:m_children)
    {
        if(child->hidden() || child->getPlacement() == Placement::Owner)
            continue;

        DockPosition dock=child->getDock();
        const Margin &margin=child->getMargin();
        Size preferred=child->getPreferredSize();
        const Rect &bounds=child->getBounds();
        SizeFlags sizeFlags=child->getSizeFlags();

        if(dock & DockPosition::Top)
        {
            if(sizeFlags.horizontal==SizeFlag::Fixed)
                child->setArrangedBounds(innerBounds.x+margin.left,
                    bounds.y,
                    bounds.w,
                    bounds.h);
            else
                child->setArrangedBounds(innerBounds.x+margin.left,
                    bounds.y,
                    innerBounds.w-margin.left-margin.right,
                    bounds.h);
        }
        else if(dock & DockPosition::Left)
        {
            child->setArrangedBounds(innerBounds.x+margin.left,
                bounds.y,
                preferred.width,
                bounds.h);
            int iWidth=margin.left+margin.right+preferred.width;
            innerBounds.x+=iWidth;
            innerBounds.w-=iWidth;
        }
        else if(dock & DockPosition::Right)
        {
            child->setArrangedBounds((innerBounds.x+innerBounds.w)-preferred.width-margin.right,
                bounds.y,
                preferred.width,
                bounds.h);
            int iWidth=margin.left+margin.right+preferred.width;
            innerBounds.w-=iWidth;
        }
        else if(dock & DockPosition::Bottom)
        {
            if(sizeFlags.horizontal==SizeFlag::Fixed)
                child->setArrangedBounds(innerBounds.x+margin.left,
                    bounds.y,
                    bounds.w,
                    bounds.h);
            else
                child->setArrangedBounds(innerBounds.x+margin.left,
                    bounds.y,
                    innerBounds.w-margin.left-margin.right,
                    bounds.h);
        }
        else if(dock == DockPosition::None)
        {
            child->policyResize(preferred, Dim::X);

            Alignment alignment=child->getAlignment();

            // Alignment is opt-in: an aligned child is placed in the padded
            // content box, whatever the docked siblings took; an unaligned one
            // stays at the position it asked for (setPos, setBounds, moveTo).
            if(alignment & Alignment::Left)
                child->setArrangedBounds(contentX+margin.left, bounds.y, bounds.w, bounds.h);
            else if(alignment & Alignment::Right)
                child->setArrangedBounds(contentX+contentW-bounds.w-margin.right, bounds.y, bounds.w, bounds.h);
            else if(alignment & Alignment::CenterH)
            {
                int start=contentX+margin.left+(contentW-bounds.w-margin.left-margin.right)/2;
                child->setArrangedBounds(start, bounds.y, bounds.w, bounds.h);
            }
            else //none/Absolute
                child->setArrangedBounds(child->getRequestedBounds().x, bounds.y, bounds.w, bounds.h);
        }

#ifdef DEBUG_LAYOUT
        if(!(dock&DockPosition::Center))
        {
            Alignment alignment=child->getAlignment();

            logUpdates("%zd %s%s - arrangeH - dock: %s, align: %s (%d, %d, %d, %d) p(%d, %d)\n", child->getControlId(), spaceByParent(child).c_str(), child->getTypeName(), getDockPositionName(dock),
                getAlignmentName(alignment), child->getX(), child->getY(), child->getWidth(), child->getHeight(), preferred.width, preferred.height);
        }
#endif//DEBUG_LAYOUT
    }

    m_innerBounds.x=innerBounds.x;
    m_innerBounds.w=innerBounds.w;

    //now fill
    for(auto &&child:m_children)
    {
        DockPosition dock=child->getDock();

        if(!(dock & DockPosition::Center) || child->getPlacement() == Placement::Owner)
            continue;

        const Margin &margin=child->getMargin();
        const Rect &bounds=child->getBounds();
        //            const Rect &preferred=child->getPreferredSize();

        child->setArrangedBounds(innerBounds.x+margin.left, bounds.y,
            innerBounds.w-margin.left-margin.right, bounds.h);

#ifdef DEBUG_LAYOUT
        Alignment alignment=child->getAlignment();
        Size preferred=child->getPreferredSize();

        logUpdates("%zd %s%s - arrangeH - dock: %s, align: %s (%d, %d, %d, %d) p(%d, %d)\n", child->getControlId(), spaceByParent(child).c_str(), child->getTypeName(), getDockPositionName(dock),
            getAlignmentName(alignment), child->getX(), child->getY(), child->getWidth(), child->getHeight(), preferred.width, preferred.height);
#endif//DEBUG_LAYOUT
    }
}

void Base::arrangeVertical(skin::Base *skin)
{
    Rect innerBounds=getBounds();

    innerBounds.y=m_padding.top;
    innerBounds.h-=m_padding.top+m_padding.bottom;

    const int contentY=innerBounds.y;
    const int contentH=innerBounds.h;

    int centerCount=0;

    for(auto &&child:m_children)
    {
        if(child->hidden() || child->getPlacement() == Placement::Owner)
            continue;

        DockPosition dock=child->getDock();
        const Margin &margin=child->getMargin();
        const Size &preferred=child->getPreferredSize();
        const Rect &bounds=child->getBounds();
        SizeFlags sizeFlags=child->getSizeFlags();

        if(dock & DockPosition::Top)
        {
            child->setArrangedBounds(bounds.x,
                innerBounds.y+margin.top,
                bounds.w,
                preferred.height);

            int iHeight=margin.top+margin.bottom+preferred.height;
            innerBounds.y+=iHeight;
            innerBounds.h-=iHeight;
        }
        else if(dock & DockPosition::Left)
        {
            int height;

            if(sizeFlags.vertical==SizeFlag::Fixed)
                height=bounds.h;
            else
                height=innerBounds.h-margin.top-margin.bottom;

            child->setArrangedBounds(bounds.x,
                innerBounds.y+margin.top,
                bounds.w,
                height);
        }
        else if(dock & DockPosition::Right)
        {
            int height;

            if(sizeFlags.vertical==SizeFlag::Fixed)
                height=bounds.h;
            else
                height=innerBounds.h-margin.top-margin.bottom;

            child->setArrangedBounds(bounds.x,
                innerBounds.y+margin.top,
                bounds.w,
                height);
        }
        else if(dock & DockPosition::Bottom)
        {
            child->setArrangedBounds(bounds.x,
                (innerBounds.y+innerBounds.h)-preferred.height-margin.bottom,
                bounds.w,
                preferred.height);

            innerBounds.h-=preferred.height+margin.bottom+margin.top;
        }
        else if(dock == DockPosition::None)
        {
            child->policyResize(preferred, Dim::Y);

            Alignment alignment=child->getAlignment();

            // As in arrangeHorizontal: the padded content box, or the
            // requested position.
            if(alignment & Alignment::Top)
                child->setArrangedBounds(bounds.x, contentY+margin.top, bounds.w, bounds.h);
            else if(alignment & Alignment::Bottom)
                child->setArrangedBounds(bounds.x, contentY+contentH-bounds.h-margin.bottom, bounds.w, bounds.h);
            else if(alignment & Alignment::CenterV)
            {
                int start=contentY+margin.top+(contentH-bounds.h-margin.top-margin.bottom)/2;
                child->setArrangedBounds(bounds.x, start, bounds.w, bounds.h);
            }
            else //none/Absolute
                child->setArrangedBounds(bounds.x, child->getRequestedBounds().y, bounds.w, bounds.h);
        }
        else if(dock==DockPosition::Center)
            centerCount++;

#ifdef DEBUG_LAYOUT
        if(!(dock&DockPosition::Center))
        {
            Alignment alignment=child->getAlignment();

            logUpdates("%zd %s%s - arrangeV - dock: %s, align: %s (%d, %d, %d, %d) p(%d, %d)\n", child->getControlId(), spaceByParent(child).c_str(), child->getTypeName(), getDockPositionName(dock),
                getAlignmentName(alignment), child->getX(), child->getY(), child->getWidth(), child->getHeight(), preferred.width, preferred.height);
        }
#endif//DEBUG_LAYOUT
    }

    m_innerBounds.y=innerBounds.y;
    m_innerBounds.h=innerBounds.h;

    if(centerCount > 0)
    {
        int centerH=innerBounds.h/centerCount;

        centerCount=0;
        for(auto &&child:m_children)
        {
            // Skip hidden children: they are excluded from centerCount above, so
            // including them here would push the visible Center children down by
            // one slot each (e.g. PageControl's hidden pages shoving the current
            // page off-screen).
            if(child->hidden() || child->getPlacement() == Placement::Owner)
                continue;

            DockPosition dock=child->getDock();

            if(!(dock & DockPosition::Center))
                continue;

            const Margin &margin=child->getMargin();
            const Rect &bounds=child->getBounds();

            child->setArrangedBounds(bounds.x, innerBounds.y+(centerCount*centerH)+margin.top,
                bounds.w, centerH-margin.top-margin.bottom);

            centerCount++;
#ifdef DEBUG_LAYOUT
            Alignment alignment=child->getAlignment();
            Size preferred=child->getPreferredSize();

            logUpdates("%zd %s%s - arrangeV - dock: %s, align: %s (%d, %d, %d, %d) p(%d, %d)\n", child->getControlId(), spaceByParent(child).c_str(), child->getTypeName(), getDockPositionName(dock),
                getAlignmentName(alignment), child->getX(), child->getY(), child->getWidth(), child->getHeight(), preferred.width, preferred.height);
#endif//DEBUG_LAYOUT
        }
    }
}

void Base::recurseLayout(skin::Base *skin)
{
    if(m_skin)
        skin=m_skin;

    if(hidden())
        return;

    if(!m_updateLayout)
        return;

    // The root is entered like any dirty control; its Y arrange leaves it
    // dirty only while a child is, and Canvas::layoutPass decides from what
    // the pass re-dirtied whether another runs.
    doCalculateSize(skin, Dim::X);
    doArrange(skin, Dim::X);
    doCalculateSize(skin, Dim::Y);
    doArrange(skin, Dim::Y);

    //postLayout(skin);
}

bool Base::isChild(controls::Base *possiblechild)
{
    for(auto &&child : m_children)
    {
        if(possiblechild == child)
            return true;
    }

    return false;
}

// Up the controls this one is actually placed in, each at its own offset: a
// child added to a control with an inner panel is the inner panel's, which
// may itself be another control's inner panel (a TreeNode is in its
// TreeControl's ScrollControl's panel), so the logical parent chain would
// miss an offset.
Point Base::localPosToCanvas(const Point &pnt)
{
    Base *placedIn=m_actualParent?m_actualParent:m_parent;

    if(placedIn)
        return placedIn->localPosToCanvas(Point(pnt.x+getX(), pnt.y+getY()));

    return pnt;
}

Point Base::canvasPosToLocal(const Point &pnt)
{
    Base *placedIn=m_actualParent?m_actualParent:m_parent;

    if(placedIn)
    {
        const Point local=placedIn->canvasPosToLocal(pnt);

        return Point(local.x-getX(), local.y-getY());
    }

    return pnt;
}

bool Base::isMenuComponent()
{
    if(!m_parent)
        return false;

    return m_parent->isMenuComponent();
}

void Base::closeMenus()
{
    for(auto &&child : m_children)
    {
        child->closeMenus();
    }
}

void Base::updateRenderBounds()
{
    m_renderBounds.x=m_bounds.x;
    m_renderBounds.y=m_bounds.y;
    m_renderBounds.w=m_bounds.w;
    m_renderBounds.h=m_bounds.h;
}

void Base::updateCursor()
{
    platform::setCursor(m_cursor);
}

draganddrop::Package *Base::dragAndDrop_getPackage(int /*x*/, int /*y*/)
{
    return m_dragAndDrop_Package;
}

bool Base::dragAndDrop_handleDrop(draganddrop::Package * /*package*/, int /*x*/, int /*y*/)
{
    draganddrop::SourceControl->setParent(this);
    return true;
}

bool Base::dragAndDrop_draggable()
{
    if(!m_dragAndDrop_Package)
        return false;

    return m_dragAndDrop_Package->draggable;
}

void Base::dragAndDrop_setPackage(bool draggable, const String &name, void *userData)
{
    if(!m_dragAndDrop_Package)
        m_dragAndDrop_Package=new draganddrop::Package();

    m_dragAndDrop_Package->draggable=draggable;
    m_dragAndDrop_Package->name=name;
    m_dragAndDrop_Package->userdata=userData;
}

bool Base::sizeToChildren(bool w, bool h)
{
    Point size=childrenSize();
    size.y+=getPadding().bottom;
    size.x+=getPadding().right;
    return setSize(w?size.x:getWidth(), h?size.y:getHeight());
}

Point Base::childrenSize()
{
    Point size;

    for(auto &&child : m_children)
    {
        if(child->hidden())
            continue;

        if(!child->shouldIncludeInSize())
            continue;

        size.x=std::max(size.x, child->right());
        size.y=std::max(size.y, child->bottom());
    }

    return size;
}

void Base::setPadding(const Padding &padding)
{
    if(m_padding.left == padding.left &&
        m_padding.top == padding.top &&
        m_padding.right == padding.right &&
        m_padding.bottom == padding.bottom)
        return;

    m_padding=padding;
    invalidate();
    invalidateParent();
    invalidateParentReaders();
}

void Base::setMargin(const Margin &margin)
{
    if(m_margin.top == margin.top &&
        m_margin.left == margin.left &&
        m_margin.bottom == margin.bottom &&
        m_margin.right == margin.right)
        return;

    m_margin=margin;
    invalidate();
    invalidateParent();
}

bool Base::handleAccelerator(String &accelerator)
{
    if(KeyboardFocus == this || !AccelOnlyFocus())
    {
        AccelMap::iterator iter=m_accelerators.find(accelerator);

        if(iter != m_accelerators.end())
        {
            iter->second->call(this);
            return true;
        }
    }

    for(auto &&child : m_children)
    {
        if(child->handleAccelerator(accelerator))
            return true;
    }

    return false;
}

bool Base::onKeyPress(int key, bool bPress)
{
    bool handled=false;

    switch(key)
    {
    case keys::Tab:
        handled=onKeyTab(bPress);
        break;

    case keys::Space:
        handled=onKeySpace(bPress);
        break;

    case keys::Home:
        handled=onKeyHome(bPress);
        break;

    case keys::End:
        handled=onKeyend(bPress);
        break;

    case keys::Return:
        handled=onKeyReturn(bPress);
        break;

    case keys::Backspace:
        handled=onKeyBackspace(bPress);
        break;

    case keys::Delete:
        handled=onKeyDelete(bPress);
        break;

    case keys::Right:
        handled=onKeyRight(bPress);
        break;

    case keys::Left:
        handled=onKeyLeft(bPress);
        break;

    case keys::Up:
        handled=onKeyUp(bPress);
        break;

    case keys::Down:
        handled=onKeyDown(bPress);
        break;

    case keys::Escape:
        handled=onKeyEscape(bPress);
        break;

    default:
        break;
    }

    if(!handled && getParent())
        getParent()->onKeyPress(key, bPress);

    return handled;
}

bool Base::onKeyRelease(int key)
{
    return onKeyPress(key, false);
}

bool Base::onKeyTab(bool down)
{
    if(!down)
        return true;

    // focus() repaints both the old and the new focus holder.
    if(Base *next=nextTabable(input::isShiftDown()))
        next->focus();

    return true;
}

Base *Base::nextTabable(bool backwards)
{
    Base *root=getCanvas();

    if(!root)
        return nullptr;

    // One pre-order walk of the canvas tree. Forward, the answer is the first
    // candidate after this one, else (wrapping) the first before it; backwards,
    // the last candidate before it, else the last after it. The whole tree is
    // walked, so this is found even inside a hidden or disabled subtree, whose
    // controls are never candidates.
    bool passed=false;
    Base *before=nullptr;   // forward: the first candidate before this; backwards: the last
    Base *after=nullptr;    // forward: the first candidate after this; backwards: the last

    auto walk=[&](auto &self, Base *control, bool excluded)->bool
    {
        excluded=excluded || control->hidden() || control->isDisabled();

        if(control == this)
        {
            passed=true;

            if(backwards && before)
                return true;
        }
        else if(!excluded && control->isTabable())
        {
            if(!passed)
            {
                if(backwards || !before)
                    before=control;
            }
            else
            {
                after=control;

                if(!backwards)
                    return true;
            }
        }

        for(auto &&child : control->m_children)
        {
            if(self(self, child, excluded))
                return true;
        }
        return false;
    };
    walk(walk, root, false);

    if(backwards)
        return before ? before : after;
    return after ? after : before;
}

void Base::renderFocus(skin::Base *skin)
{
    if(KeyboardFocus != this)
        return;

    if(!isTabable())
        return;

//    skin->drawKeyboardHighlight(this, skin::Generate, getRenderBounds(), 3);
}

// The canvas reads the tooltip when it shows it (Canvas::updateToolTip), so
// neither setter creates anything; one shown now is filled in again.
void Base::setToolTipText(const String &text)
{
    if(m_toolTipText == text)
        return;

    m_toolTipText=text;

    if(Canvas *canvas=getCanvas())
        canvas->toolTipChanged(this);
}

void Base::setToolTip(Base *tooltip)
{
    if(m_toolTip == tooltip)
        return;

    m_toolTip=tooltip;

    if(m_toolTip)
    {
        m_toolTip->setParent(this);
        m_toolTip->setHidden(true);
    }

    if(Canvas *canvas=getCanvas())
        canvas->toolTipChanged(this);
}

void Base::setContextMenu(Menu *menu)
{
    if(menu == m_contextMenu)
        return;

    dropContextMenu();
    m_contextMenu=menu;

    if(menu)
        menu->m_contextOwners.push_back(this);
}

Menu *Base::createContextMenu()
{
    Canvas *canvas=getCanvas();

    if(!canvas)
        return nullptr;

    Menu *menu=canvas->newChild<Menu>();

    menu->setHidden(true);
    setContextMenu(menu);
    m_ownsContextMenu=true;
    return menu;
}

void Base::dropContextMenu()
{
    Menu *menu=m_contextMenu;

    if(!menu)
        return;

    const bool owned=m_ownsContextMenu;

    m_contextMenu=nullptr;
    m_ownsContextMenu=false;

    std::vector<Base *> &owners=menu->m_contextOwners;
    owners.erase(std::remove(owners.begin(), owners.end(), this), owners.end());

    Canvas *canvas=menu->getCanvas();

    if(canvas && (canvas->contextMenuOwner() == this))
        canvas->closeContextMenu();

    if(!owned)
        return;

    // Delayed, as a menu is deleted: this may run from one of its own items'
    // events. A canvas tearing down deletes it with its other children.
    if(canvas)
        menu->delayedDelete();
    else
        delete menu;
}

String Base::getChildValue(const String &name)
{
    Base *child=findChildByName(name, true);

    if(!child)
        return "";

    return child->getValue();
}

String Base::getValue()
{
    // Generic value accessor should be filled in if we have a value to give.
    return "";
}

void Base::setValue(const String &value)
{}

int Base::getNamedChildren(ControlList &list, const String &name, bool bDeep)
{
    int found=0;

    for(auto &&child : m_children)
    {
        if(!child->getName().empty() && child->getName() == name)
        {
            list.add(child);
            found++;
        }

        if(!bDeep)
            continue;

        found+=child->getNamedChildren(list, name, bDeep);
    }

    return found;
}

ControlList Base::getNamedChildren(const String &name, bool bDeep)
{
    ControlList list;
    getNamedChildren(list, name, bDeep);
    return list;
}

#if GWENI_ANIMATE

void Base::anim_widthIn(float length, float delay, float ease)
{
    anim::add(this, new anim::size::Width(0, getWidth(), length, false, delay, ease));
    setWidth(0);
}

void Base::anim_heightIn(float length, float delay, float ease)
{
    anim::add(this, new anim::size::Height(0, getHeight(), length, false, delay, ease));
    setHeight(0);
}

void Base::anim_widthOut(float length, bool hide, float delay, float ease)
{
    anim::add(this, new anim::size::Width(getWidth(), 0, length, hide, delay, ease));
}

void Base::anim_heightOut(float length, bool hide, float delay, float ease)
{
    anim::add(this, new anim::size::Height(getHeight(), 0, length, hide, delay, ease));
}

#endif // if GWENI_ANIMATE


} // namespace gweni
} // namespace controls

namespace gweni
{
namespace input
{

size_t hitTestVisits()
{
    return controls::g_hitTestVisits;
}

}//namespace input
}//namespace gweni
