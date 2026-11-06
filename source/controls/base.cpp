/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/base.h>
#include <gweni/controls/label.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/layout/layout.h>
#include <gweni/gweni.h>
#include <gweni/platforms/baseRender.h>
#include <gweni/skin.h>
#include <gweni/platforms/platform.h>
#include <gweni/dragAndDrop.h>
#include <gweni/toolTip.h>
#include <gweni/utility.h>
#include <gweni/objectIdBank.h>
#include <list>

#if GWENI_ANIMATE
#   include <gweni/anim.h>
#endif

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

Base::Base(const String &name)
{

    m_id=g_controls.getNextId(this);

    m_parent=nullptr;
    m_actualParent=nullptr;
    m_childStateChanged=false;
    m_skin=nullptr;

    m_alignment=Alignment::Left | Alignment::Top;
    m_position=Position::Absolute;

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
    // Read by the skin when a control created by the render phase's layout is
    // drawn before updateRecursive() has numbered it.
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
    {
        Canvas *canvas=getCanvas();

        if(canvas)
            canvas->PreDeleteCanvas(this);
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
    tooltip::controlDeleted(this);
#if GWENI_ANIMATE
    anim::cancel(this);
#endif

    if(m_dragAndDrop_Package)
    {
        delete m_dragAndDrop_Package;
        m_dragAndDrop_Package=nullptr;
    }

    removeSkinControl();
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

        if(Canvas::s_layingOut && !g_propagating && !g_placement)
            Canvas::s_layingOut->layoutInvalidated(this);
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
}

DockPosition Base::getDock() const
{
    return m_dock;
}

Position Base::getPosition() const
{
    return m_position;
}

void Base::setPosition(Position position)
{
    if(position ==m_position)
        return;

    m_position=position;
    invalidate();
}

void Base::setPosition(Position position, int x, int y)
{
    if(position==m_position)
        return;

    m_position=position;
    {
        RequestScope request(RequestPosition);
        setBounds(x, y, getWidth(), getHeight());
    }
    invalidate();
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
    if(hidden())
        return false;

    if(getParent())
        return getParent()->visible();

    return true;
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

//void Base::setPosition(Position pos, int xpadding, int ypadding)
//{
//    m_position=pos;
//
//    const Rect &bounds=getParent()->getInnerBounds();
//    const Margin &margin=getMargin();
//    int x=getX();
//    int y=getY();
//
//    if(pos  &Position::Left)
//        x=bounds.x + xpadding + margin.left;
//
//    if(pos  &Position::Right)
//        x=bounds.x + (bounds.w - getWidth() - xpadding - margin.right);
//
//    if(pos  &Position::CenterH)
//        x=bounds.x + (bounds.w - getWidth())/2;
//
//    if(pos  &Position::Top)
//        y=bounds.y + ypadding;
//
//    if(pos  &Position::Bottom)
//        y=bounds.y + (bounds.h - getHeight() - ypadding);
//
//    if(pos  &Position::CenterV)
//        y=bounds.y + (bounds.h - getHeight())/2 + ypadding;
//
//    x=std::max(0, x);
//    y=std::max(0, y);
//
//    setPos(x, y);
//}

void Base::sendToBack()
{
    if(!m_actualParent)
        return;

    if(m_actualParent->m_children.front() == this)
        return;

    m_actualParent->m_children.remove(this);
    m_actualParent->m_children.push_front(this);
    invalidateParent();
}

void Base::bringToFront()
{
    if(!m_actualParent)
        return;

    if(m_actualParent->m_children.back() == this)
        return;

    m_actualParent->m_children.remove(this);
    m_actualParent->m_children.push_back(this);
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
    invalidateParent();
}

void Base::addChild(Base *child)
{
    if(m_innerPanel)
    {
        m_innerPanel->addChild(child);
        return;
    }

    m_children.push_back(child);

    if(m_layoutItem)
        m_layoutItem->addControl(child);

    onChildAdded(child);
    child->m_actualParent=this;
    markAttachedChanged(child);

    // A child added to a hidden parent inherits the hidden state; otherwise it
    // renders (and bleeds) even though its parent is hidden — e.g. content built
    // into a PageControl page that was hidden before its content was added.
    if(m_hidden)
        child->propagateHidden(true);
}

void Base::addChildOfLayout(Base *child)
{
    m_children.push_back(child);

    onChildAdded(child);
    child->m_actualParent=this;
    markAttachedChanged(child);

    if(m_hidden)
        child->propagateHidden(true);
}

void Base::removeChild(Base *child)
{
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
        child->m_actualParent=nullptr;

    onChildRemoved(child);
}

void Base::markAttachedChanged(Base *child)
{
    // Created before it had a parent, or moved here from another branch: the
    // walk has to reach it in its new place.
    if(child->m_stateChange != StateChange_Nothing || child->m_childStateChanged)
        child->markAncestorsChanged();
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
    if(m_restrictToParent && getParent())
    {
        Base *parent=getParent();

        if(x-getPadding().left < parent->getMargin().left)
            x=parent->getMargin().left+getPadding().left;

        if(y-getPadding().top < parent->getMargin().top)
            y=parent->getMargin().top+getPadding().top;

        if(x+getWidth()+getPadding().right > parent->getWidth()-parent->getMargin().right)
            x=parent->getWidth()-parent->getMargin().right-getWidth()-getPadding().right;

        if(y+getHeight()+getPadding().bottom > parent->getHeight()-parent->getMargin().bottom)
            y=parent->getHeight()-parent->getMargin().bottom-getHeight()-getPadding().bottom;
    }

    RequestScope request(RequestPosition);
    setBounds(x, y, getWidth(), getHeight());
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

        // A request the bounds already match leaves the size the rules give
        // (dockNoneSize, layoutCellSize) at these bounds, but the position
        // counts in the parent's preferred size, so an app's request is laid
        // out again. From inside a pass it is layout code putting a control
        // where it already is (TabStrip sizing itself, say): no re-dirty.
        if(m_bounds == bounds && !inLayoutPass() &&
            (newRequest || !(requested == m_requestedBounds)))
            invalidate();
    }

    if(m_bounds == bounds)
        return false;

    // A width is read from the X arrange on, a height from the Y arrange on
    // (layout() runs again there if the height changed, see Base::arrange).
    if((m_bounds.w != bounds.w && laidOutThisPass(Dim::X)) ||
        (m_bounds.h != bounds.h && laidOutThisPass(Dim::Y)))
        Canvas::s_layingOut->layoutInvalidated(this);

    {
        PlacementScope placement;
        invalidateChildren(true);
    }

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

void Base::onScaleChanged()
{
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
    }
}

void Base::update()
{
}

void Base::updateRecursive(int &zIndex)
{
    update();

    if(m_zIndex != zIndex)
    {
        m_zIndex=zIndex;
        addStateChange(StateChange_Geometry);
    }
    zIndex++;

    if(!m_children.empty())
    {
        for(auto &&child:m_children)
        {
            child->updateRecursive(zIndex);
        }
    }
}

void Base::doCacheRender(skin::Base *skin, controls::Base *master)
{
    renderer::Base *baseRenderer=skin->getRenderer();
    renderer::ICacheToTexture *cache=baseRenderer->getCTT();

    if(!cache)
        return;

    Point oldRenderOffset=baseRenderer->getRenderOffset();
    Rect rOldRegion=baseRenderer->clipRegion();

    if(this != master)
    {
        baseRenderer->addRenderOffset(getBounds());
        baseRenderer->addClipRegion(getBounds());
    }
    else
    {
        baseRenderer->setRenderOffset(Point(0, 0));
        baseRenderer->setClipRegion(getBounds());
    }

    // See if we need to update the cached texture. Dirty?
    if(m_cacheTextureDirty && baseRenderer->clipRegionVisible())
    {
        baseRenderer->startClip();
        {
            if(isCachedToTexture())
                cache->setupCacheTexture(this);

            // Render myself first
            render(skin);

            if(!m_children.empty())
            {
                // Now render my kids
                for(auto &&child : m_children)
                {
//                    if(child->hidden())
//                        child->doHide(skin);
//                    else
                    {
                        // Draw child control using normal render. If it is cached it will
                        // be handled in the same way as this one.
                        child->doRender(skin);
                    }
                }
            }

            if(isCachedToTexture())
            {
                cache->finishCacheTexture(this);
                m_cacheTextureDirty=false;
            }
        }
        baseRenderer->endClip();
    }

    // Draw the cached texture.
    baseRenderer->setClipRegion(rOldRegion);
    baseRenderer->startClip();
    {
        baseRenderer->setRenderOffset(oldRenderOffset);
        cache->drawCachedControlTexture(this);
    }
    baseRenderer->endClip();
}

void Base::doThink()
{
    think();
    if(!m_children.empty())
    {
        for(auto &&child:m_children)
        {
            child->doThink();
        }
    }
}

void Base::doRender(skin::Base *skin)
{
    // If this control has a different skin,
    // then so does its children.
    if(m_skin)
        skin=m_skin;

    // Do think
    //think();

//    if(baseRenderer->getCTT() && isCachedToTexture())
//    {
//        doCacheRender(skin, this);
//        return;
//    }
    // Nothing pending here or below: the subtree's primitives are as they
    // were, so skip it before any offset, clip or children work.
    if(getStateChange() == StateChange_Nothing && !m_childStateChanged)
        return;

    renderRecursive(skin, getBounds());

    //reset state change to capture anything that happens next iteration
    m_stateChange=StateChange_Nothing;
}

void Base::renderRecursive(skin::Base *skin, const Rect &cliprect)
{
    renderer::Base *baseRenderer=skin->getRenderer();

    ++g_changeStats.visited;

    //render current control
    renderUnder(skin);
    render(skin);

    //set clipping to this control
    Point oldRenderOffset=baseRenderer->getRenderOffset();
    Rect rOldRegion=baseRenderer->clipRegion();

    baseRenderer->addRenderOffset(cliprect);

    // If this control is clipping, change the clip rect to ourselves
    // else clip using our parents clip rect.
//    if(shouldClip())
//    {
//        baseRenderer->addClipRegion(cliprect);
//
//        if(!baseRenderer->clipRegionVisible())
//        {
//            baseRenderer->setRenderOffset(oldRenderOffset);
//            baseRenderer->setClipRegion(rOldRegion);
//            return;
//        }
//    }

    // Cleared before the children are visited, so a change raised on one of
    // them from here on (a sibling's render(), say) marks this control again
    // and is rendered next frame, if not this one.
    m_childStateChanged=false;

    // Render this child controls under current objects clip
    baseRenderer->startClip();
    {
        if(!m_children.empty())
        {
            // Iterate a snapshot: a control's render() may mutate the sibling
            // list (e.g. sendToBack() during a state change), which would
            // otherwise invalidate this iterator (use-after-free).
            Base::List children=m_children;

            // Now render my kids
            for(auto &&child : children)
            {
                child->doRender(skin);
            }
        }
    }
    baseRenderer->endClip();

    // Render overlay/focus
    {
        baseRenderer->setClipRegion(rOldRegion);
        baseRenderer->startClip();
        {
            renderOver(skin);
            renderFocus(skin);
        }
        baseRenderer->endClip();
        baseRenderer->setRenderOffset(oldRenderOffset);
    }
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
    }

    invalidate();
    redraw();
    onSkinChanged(skin);

    m_stateChange=StateChange_Created;
    markAncestorsChanged();

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

    if(getToolTip())
        tooltip::enable(this);
    else if(getParent() && getParent()->getToolTip())
        tooltip::enable(getParent());

    redraw();
}

void Base::onMouseLeave()
{
    onHoverLeave.call(this);

    if(getToolTip())
        tooltip::disable(this);

    redraw();
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
        previous->redraw();

    onKeyboardFocus();
    redraw();
}

void Base::unfocus()
{
    if(KeyboardFocus != this)
        return;

    KeyboardFocus=nullptr;
    onLostKeyboardFocus();
    redraw();
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

Base *Base::getControlAt(int x, int y, bool bOnlyIfMouseEnabled)
{
    if(hidden())
        return nullptr;

    if(x < 0 || y < 0 || x >= getWidth() || y >= getHeight())
        return nullptr;

    for(Base::List::reverse_iterator iter=m_children.rbegin(); iter != m_children.rend(); ++iter)
    {
        Base *child=*iter;
        Base *found=nullptr;
        found=child->getControlAt(x-child->getX(), y-child->getY(), bOnlyIfMouseEnabled);

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

            child->calculateSize(skin, dim);

            // Laid out like the others, but its size is taken from this
            // control's (Properties' splitter bar), so it does not count.
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

            child->calculateSize(skin, dim);

            // Laid out like the others, but its size is taken from this
            // control's (Properties' splitter bar), so it does not count.
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

        child->arrange(skin, dim);
    }

    if(dim == Dim::Y)
        postLayout(skin);
}

void Base::arrangeHorizontal(skin::Base *skin)
{
    Rect innerBounds=getBounds();

    innerBounds.x=m_padding.left;
    innerBounds.w-=m_padding.left+m_padding.right;

    for(auto &&child:m_children)
    {
        if(child->hidden())
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
                child->setArrangedBounds(innerBounds.x,
                    bounds.y,
                    bounds.w,
                    bounds.h);
            else
                child->setArrangedBounds(innerBounds.x,
                    bounds.y,
                    innerBounds.w-margin.left-margin.right,
                    bounds.h);
        }
        else if(dock == DockPosition::None)
        {
            child->policyResize(preferred, Dim::X);

            Alignment alignment=child->getAlignment();

            //            child->setBounds(bounds.x, bounds.y, preferred.width, bounds.h);
            if(alignment & Alignment::Left)
                child->setArrangedBounds(margin.left, bounds.y, bounds.w, bounds.h);
            else if(alignment & Alignment::Right)
                child->setArrangedBounds(innerBounds.w-bounds.w-margin.right, bounds.y, bounds.w, bounds.h);
            else if(alignment & Alignment::CenterH)
            {
                int start=(innerBounds.w-bounds.w-margin.left-margin.right)/2;
                child->setArrangedBounds(start, bounds.y, bounds.w, bounds.h);
            }
            else //none/Absolute
                child->setArrangedBounds(bounds.x, bounds.y, bounds.w, bounds.h);
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

        if(!(dock & DockPosition::Center))
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

    int centerCount=0;

    for(auto &&child:m_children)
    {
        if(child->hidden())
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

//            const Rect &bounds=child->getBounds();
            Alignment alignment=child->getAlignment();

            if(alignment & Alignment::Top)
                child->setArrangedBounds(bounds.x, margin.top, bounds.w, bounds.h);
            else if(alignment & Alignment::Bottom)
                child->setArrangedBounds(bounds.x, innerBounds.h-bounds.h-margin.bottom, bounds.w, bounds.h);
            else if(alignment & Alignment::CenterV)
            {
                int start=(innerBounds.h-bounds.h-margin.top-margin.bottom)/2;
                child->setArrangedBounds(bounds.x, start, bounds.w, bounds.h);
            }
            else //none/Absolute
                child->setArrangedBounds(bounds.x, bounds.y, bounds.w, bounds.h);
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
            if(child->hidden())
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

    // Cleared before the pass rather than after it, so an invalidate() raised
    // while it runs is kept and Canvas::layoutCanvas runs another.
    m_updateLayout=false;

    calculateSize(skin, Dim::X);
    arrange(skin, Dim::X);
    calculateSize(skin, Dim::Y);
    arrange(skin, Dim::Y);

    //postLayout(skin);

    if(isTabable() && !isDisabled())
    {
        if(!getCanvas()->m_firstTab)
            getCanvas()->m_firstTab=this;

        if(!getCanvas()->m_nextTab)
            getCanvas()->m_nextTab=this;
    }

    if(KeyboardFocus == this)
        getCanvas()->m_nextTab=nullptr;
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

Point Base::localPosToCanvas(const Point &pnt)
{
    if(m_parent)
    {
        int x=pnt.x+getX();
        int y=pnt.y+getY();

        // If our parent has an innerpanel and we're a child of it
        // add its offset onto us.
        //
        if(m_parent->m_innerPanel && m_parent->m_innerPanel->isChild(this))
        {
            x+=m_parent->m_innerPanel->getX();
            y+=m_parent->m_innerPanel->getY();
        }

        return m_parent->localPosToCanvas(Point(x, y));
    }

    return pnt;
}

Point Base::canvasPosToLocal(const Point &pnt)
{
    if(m_parent)
    {
        int x=pnt.x-getX();
        int y=pnt.y-getY();

        // If our parent has an innerpanel and we're a child of it
        // add its offset onto us.
        //
        if(m_parent->m_innerPanel && m_parent->m_innerPanel->isChild(this))
        {
            x-=m_parent->m_innerPanel->getX();
            y-=m_parent->m_innerPanel->getY();
        }

        return m_parent->canvasPosToLocal(Point(x, y));
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

void Base::dragAndDrop_startDragging(draganddrop::Package *package, int x, int y)
{
    package->holdoffset=canvasPosToLocal(Point(x, y));
    package->drawcontrol=this;
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

    if(getCanvas()->m_nextTab)
    {
        getCanvas()->m_nextTab->focus();
        redraw();
    }

    return true;
}

void Base::renderFocus(skin::Base *skin)
{
    if(KeyboardFocus != this)
        return;

    if(!isTabable())
        return;

//    skin->drawKeyboardHighlight(this, skin::Generate, getRenderBounds(), 3);
    update();
}

void Base::setToolTipText(const String &text)
{
    Label *tooltip=newChild<Label>();

    tooltip->setText(text);
    tooltip->setTextColorOverride(getSkin()->Colors.TooltipText);
    tooltip->setPadding(Padding(5, 3, 5, 3));
    tooltip->sizeToContents();

    setToolTip(tooltip);
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
