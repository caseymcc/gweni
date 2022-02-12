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
    m_skin=nullptr;

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
    m_bounds=Rect(0, 0, 10, 10);
    m_padding=Padding(0, 0, 0, 0);
    m_margin=Margin(0, 0, 0, 0);
    m_dock=Position::None;
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

    for(size_t i=0; i<m_primitiveIds.size(); ++i)
        releasePrimitiveId(m_primitiveIds[i]);
}

void Base::invalidate()
{
    m_needsLayout=true;
    m_cacheTextureDirty=true;
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

void Base::dock(Position dock)
{
    if(m_dock == dock)
        return;

    m_dock=dock;
    invalidate();
    invalidateParent();
}

Position Base::getDock() const
{
    return m_dock;
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

void Base::setPosition(Position pos, int xpadding, int ypadding)
{
    const Rect &bounds=getParent()->getInnerBounds();
    const Margin &margin=getMargin();
    int x=getX();
    int y=getY();

    if(pos  &Position::Left)
        x=bounds.x + xpadding + margin.left;

    if(pos  &Position::Right)
        x=bounds.x + (bounds.w - getWidth() - xpadding - margin.right);

    if(pos  &Position::CenterH)
        x=bounds.x + (bounds.w - getWidth())/2;

    if(pos  &Position::Top)
        y=bounds.y + ypadding;

    if(pos  &Position::Bottom)
        y=bounds.y + (bounds.h - getHeight() - ypadding);

    if(pos  &Position::CenterV)
        y=bounds.y + (bounds.h - getHeight())/2 + ypadding;

    x=std::max(0, x);
    y=std::max(0, y);

    setPos(x, y);
}

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
}

void Base::addChildOfLayout(Base *child)
{
    m_children.push_back(child);

    onChildAdded(child);
    child->m_actualParent=this;
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
    onChildRemoved(child);
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

    setBounds(x, y, getWidth(), getHeight());
}

void Base::setPos(int x, int y)
{
    setBounds(x, y, getWidth(), getHeight());
}

bool Base::setSize(int w, int h)
{
    return setBounds(getX(), getY(), w, h);
}

bool Base::setSize(const Point &p)
{
    return setSize(p.x, p.y);
}

bool Base::setBounds(const Rect &bounds)
{
    if(m_name == "Basic")
        m_bounds.x=m_bounds.x;

    if(m_bounds == bounds)
        return false;

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

Size Base::getMinimumSize()
{
    Size minimumSize=m_minimumSize;

    if(m_sizeFlags.horizontal==SizeFlag::Fixed)
        minimumSize.width=m_bounds.w;
    else if((m_sizeFlags.horizontal == SizeFlag::Expand) || (m_sizeFlags.horizontal == SizeFlag::Bloat))
        minimumSize.width=std::max(minimumSize.width, m_preferredSize.width);

    if(m_sizeFlags.vertical==SizeFlag::Fixed)
        minimumSize.height=m_bounds.h;
    else if((m_sizeFlags.vertical==SizeFlag::Expand)||(m_sizeFlags.vertical==SizeFlag::Bloat))
        minimumSize.height=std::max(minimumSize.height, m_preferredSize.height);

    return minimumSize;
}

Size Base::getMaximumSize()
{
    Size maximumSize=m_maximumSize;

    if(m_sizeFlags.horizontal==SizeFlag::Fixed)
        maximumSize.width=m_bounds.w;
    else if(m_sizeFlags.horizontal == SizeFlag::Shrink)
        maximumSize.width=std::max(maximumSize.width, m_preferredSize.width);

    if(m_sizeFlags.vertical==SizeFlag::Fixed)
        maximumSize.height=m_bounds.h;
    else if(m_sizeFlags.horizontal == SizeFlag::Shrink)
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
        invalidate();

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
    enlargePrimitiveIds(this, m_primitiveIds, size);
}

void Base::render(skin::Base *skin)
{
//    update();
    if(getStateChange() == StateChange_Nothing)
        return;
    
    if(!m_skinControl)
        return;

    if(getStateChange() & StateChange_Created)
    {
        m_skinControl->generate(m_skin->getRenderer(), this);
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
        std::cout<<"Pimitives: "<<name<<" ("<<primitiveList<<")\n";
    }

    const Rect &bounds=getBounds();
    
    if(getStateChange() != StateChange_Text)
    {
        renderer::Base *baseRenderer=skin->getRenderer();
        gweni::Point clipPt=baseRenderer->getRenderOffset();
        std::string show=hidden()?"hide":"show";

        std::string name=getFullName(this);
        std::cout<<"Updating: "<<show<<" ("<<bounds.x+clipPt.x<<", "<<bounds.y+clipPt.y<<", w:"<<bounds.w<<", h:"<<bounds.h<<") \""<<name<<"\"\n";
    }

    m_skinControl->update(m_skin->getRenderer(), this);
}

void Base::update()
{
}

void Base::updateRecursive(int &zIndex)
{
    update();
    m_zIndex=zIndex++;
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

void Base::doRender(skin::Base *skin)
{
    // If this control has a different skin,
    // then so does its children.
    if(m_skin)
        skin=m_skin;

    // Do think
    think();


    renderer::Base *baseRenderer=skin->getRenderer();

    if(m_stateChange && StateChange_Created)
    {
    }

    if(baseRenderer->getCTT() && isCachedToTexture())
    {
        doCacheRender(skin, this);
        return;
    }

    renderRecursive(skin, getBounds());

    //reset state change to capture anything that happens next iteration
    m_stateChange=StateChange_Nothing;
}

void Base::renderRecursive(skin::Base *skin, const Rect &cliprect)
{
    renderer::Base *baseRenderer=skin->getRenderer();
    Point oldRenderOffset=baseRenderer->getRenderOffset();
    baseRenderer->addRenderOffset(cliprect);
    renderUnder(skin);
    Rect rOldRegion=baseRenderer->clipRegion();

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

    // Render this control and children controls
    baseRenderer->startClip();
    {
        render(skin);

        if(!m_children.empty())
        {
            // Now render my kids
            for(auto &&child : m_children)
            {
//                if(child->hidden())
//                {
//                    child->doHide(skin);
//                    continue;
//                }

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
    renderer::Base *baseRenderer=skin->getRenderer();

    for(size_t i=0; i<m_primitiveIds.size(); ++i)
        baseRenderer->hidePrimitive(m_primitiveIds[i]);

    hideRecursive(skin);
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
            std::cout<<"Failed to get skin for: "<<getTypeName()<<"\n";
    }

    invalidate();
    redraw();
    onSkinChanged(skin);

    m_stateChange=StateChange_Created;

    if(doChildren)
    {
        for(auto &&child : m_children)
        {
            child->setSkin(skin, true);
        }
    }

    //need to set prefered size
    calculateSize(skin, Dim::X);
    calculateSize(skin, Dim::Y);
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

    if(KeyboardFocus)
        KeyboardFocus->onLostKeyboardFocus();

    KeyboardFocus=this;
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
    if(skin->getRenderer()->getCTT() && isCachedToTexture())
        skin->getRenderer()->getCTT()->createControlCacheTexture(this, this->getBounds().getSize());
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

            Position dock=child->getDock();
            const Size &preferred=child->getPreferredSize();
            const Margin &margin=child->getMargin();
            int childWidth=preferred.width+margin.left+margin.right;

            if((dock  &Position::Top)||(dock  &Position::Bottom))
            {
                if(childWidth>width)
                    width=childWidth;
            }
            else if((dock  &Position::Left)||(dock  &Position::Right))
            {
                dockWidth+=childWidth;
            }
            else if(dock  &Position::Fill)
            {
                innerWidth+=childWidth;
            }
            else
            {
                const Rect &bounds=child->getBounds();

                if(!hasAlignment())
                    childWidth+=bounds.x;

                if(childWidth>width)
                    width=childWidth;
            }
        }
        dockWidth+=innerWidth;
        width=std::max(width, dockWidth);
        width+=m_padding.left+m_padding.right;

        switch(m_sizeFlags.horizontal)
        {
        case SizeFlag::Fixed:
            preferredSize.width=m_bounds.w;
            break;
        case SizeFlag::Shrink:
            preferredSize.width=std::min(width, m_maximumSize.width);
            break;
        case SizeFlag::Expand:
            preferredSize.width=std::max(width, m_minimumSize.width);
            break;
        case SizeFlag::Elastic:
            preferredSize.width=width;
            break;
        case SizeFlag::Bloat:
            preferredSize.width=std::max(width, m_minimumSize.width);
            break;
        }
    }
    else
    {
        int height=0;
        int dockHeight=0;
        int innerHeight=0;

        for(auto &&child:m_children)
        {
            Label *label=dynamic_cast<Label *>(child);

            if(label)
            {
                if(label->getText()=="Basic")
                    label=label;
            }
            if(child->hidden())
                continue;

            child->calculateSize(skin, dim);

            Position dock=child->getDock();
            const Size &preferred=child->getPreferredSize();
            const Margin &margin=child->getMargin();

            int childHeight=preferred.height+margin.top+margin.bottom;

            if((dock  &Position::Top)||(dock  &Position::Bottom))
            {
                dockHeight+=childHeight;
            }
            else if((dock  &Position::Left)||(dock  &Position::Right))
            {
                if(childHeight>height)
                    height=childHeight;
            }
            else if(dock  &Position::Fill)
            {
                innerHeight+=childHeight;
            }
            else
            {
                const Rect &bounds=child->getBounds();

                if(!hasAlignment())
                    childHeight+=bounds.y;

                if(childHeight>height)
                    height=childHeight;
            }
        }

        dockHeight+=innerHeight;
        height=std::max(height, dockHeight);
        height+=m_padding.top+m_padding.bottom;

        switch(m_sizeFlags.vertical)
        {
        case SizeFlag::Fixed:
            preferredSize.height=m_bounds.h;
            break;
        case SizeFlag::Shrink:
            preferredSize.height=std::min(height, m_maximumSize.height);
            break;
        case SizeFlag::Expand:
            preferredSize.height=std::max(height, m_minimumSize.height);
            break;
        case SizeFlag::Elastic:
            preferredSize.height=height;
            break;
        case SizeFlag::Bloat:
            preferredSize.height=std::max(height, m_minimumSize.height);
            break;
        }
    }

    return preferredSize;
}

void Base::calculateSize(skin::Base *skin, Dim dim)
{
    if(m_name == "Basic")
        m_bounds.x=m_bounds.x;

    if(processLayout(skin, dim))
        return;

    m_preferredSize=sizeOfChildren(skin, dim);
}

void Base::arrange(skin::Base *skin, Dim dim)
{
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

        Position dock=child->getDock();
        const Margin &margin=child->getMargin();
        Size preferred=child->getPreferredSize();
        const Rect &bounds=child->getBounds();
        SizeFlags sizeFlags=child->getSizeFlags();

        if(dock  &Position::Top)
        {
            if(sizeFlags.horizontal==SizeFlag::Fixed)
                child->setBounds(innerBounds.x+margin.left,
                    bounds.y,
                    bounds.w,
                    bounds.h);
            else
                child->setBounds(innerBounds.x+margin.left,
                    bounds.y,
                    innerBounds.w-margin.left-margin.right,
                    bounds.h);
        }
        else if(dock  &Position::Left)
        {
            child->setBounds(innerBounds.x+margin.left,
                bounds.y,
                preferred.width,
                bounds.h);
            int iWidth=margin.left+margin.right+preferred.width;
            innerBounds.x+=iWidth;
            innerBounds.w-=iWidth;
        }
        else if(dock  &Position::Right)
        {
            child->setBounds((innerBounds.x+innerBounds.w)-preferred.width-margin.right,
                bounds.y,
                preferred.width,
                bounds.h);
            int iWidth=margin.left+margin.right+preferred.width;
            innerBounds.w-=iWidth;
        }
        else if(dock  &Position::Bottom)
        {
            if(sizeFlags.horizontal==SizeFlag::Fixed)
                child->setBounds(innerBounds.x,
                    bounds.y,
                    bounds.w,
                    bounds.h);
            else
                child->setBounds(innerBounds.x,
                    bounds.y,
                    innerBounds.w-margin.left-margin.right,
                    bounds.h);
        }
        else if(dock==Position::None)
        {
            //            child->setBounds(bounds.x, bounds.y, preferred.width, bounds.h);
            child->setBounds(bounds.x, bounds.y, bounds.w, bounds.h);
        }
    }

    m_innerBounds.x=innerBounds.x;
    m_innerBounds.w=innerBounds.w;

    //now fill
    for(auto &&child:m_children)
    {
        Position dock=child->getDock();

        if(!(dock  &Position::Fill))
            continue;

        const Margin &margin=child->getMargin();
        const Rect &bounds=child->getBounds();
        //            const Rect &preferred=child->getPreferredSize();

        child->setBounds(innerBounds.x+margin.left, bounds.y,
            innerBounds.w-margin.left-margin.right, bounds.h);
    }
}

void Base::arrangeVertical(skin::Base *skin)
{
    Rect innerBounds=getBounds();

    innerBounds.y=m_padding.top;
    innerBounds.h-=m_padding.top+m_padding.bottom;

    for(auto &&child:m_children)
    {
        if(child->hidden())
            continue;

        Position dock=child->getDock();
        const Margin &margin=child->getMargin();
        const Size &preferred=child->getPreferredSize();
        const Rect &bounds=child->getBounds();
        SizeFlags sizeFlags=child->getSizeFlags();

        if(dock & Position::Top)
        {
            child->setBounds(bounds.x,
                innerBounds.y+margin.top,
                bounds.w,
                preferred.height);

            int iHeight=margin.top+margin.bottom+preferred.height;
            innerBounds.y+=iHeight;
            innerBounds.h-=iHeight;
        }
        else if(dock & Position::Left)
        {
            int height;

            if(sizeFlags.vertical==SizeFlag::Fixed)
                height=bounds.h;
            else
                height=innerBounds.h-margin.top-margin.bottom;

            child->setBounds(bounds.x,
                innerBounds.y+margin.top,
                bounds.w,
                height);
        }
        else if(dock & Position::Right)
        {
            int height;

            if(sizeFlags.vertical==SizeFlag::Fixed)
                height=bounds.h;
            else
                height=innerBounds.h-margin.top-margin.bottom;

            child->setBounds(bounds.x,
                innerBounds.y+margin.top,
                bounds.w,
                height);
        }
        else if(dock & Position::Bottom)
        {
            child->setBounds(bounds.x,
                (innerBounds.y+innerBounds.h)-preferred.height-margin.bottom,
                bounds.w,
                preferred.height);

            innerBounds.h-=preferred.height+margin.bottom+margin.top;
        }
        else if(dock==Position::None)
        {
            const Rect &bounds=child->getBounds();

            child->setBounds(bounds.x, bounds.y, bounds.w, bounds.h);
        }
    }

    m_innerBounds.y=innerBounds.y;
    m_innerBounds.h=innerBounds.h;

    for(auto &&child:m_children)
    {
        Position dock=child->getDock();

        if(!(dock  &Position::Fill))
            continue;

        const Margin &margin=child->getMargin();
        const Rect &bounds=child->getBounds();

        child->setBounds(bounds.x, innerBounds.y+margin.top,
            bounds.w, innerBounds.h-margin.top-margin.bottom);
    }
}

void Base::recurseLayout(skin::Base *skin)
{
    if(m_skin)
        skin=m_skin;

    if(hidden())
        return;

    calculateSize(skin, Dim::X);
    arrange(skin, Dim::X);
    calculateSize(skin, Dim::Y);
    arrange(skin, Dim::Y);

    //    postLayout(skin);

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
    m_renderBounds.x=0;
    m_renderBounds.y=0;
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
