/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/scrollControl.h>
#include <gweni/controls/scrollBar.h>
#include <gweni/controls/verticalScrollBar.h>
#include <gweni/controls/horizontalScrollBar.h>
#include <gweni/utility.h>

#include <algorithm>
#include <cmath>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(ScrollControl)
{
    setMouseInputEnabled(false);
    m_verticalScrollBar=newChild<VerticalScrollBar>();
    m_verticalScrollBar->setDock(DockPosition::Right);
    m_verticalScrollBar->onBarMovedCaller.add(this, &ScrollControl::onVeritcalBarMoved);
    m_verticalScrollBar->setNudgeAmount(30);
    m_canScrollVertical=true;

    m_horizontalScrollBar=newChild<HorizontalScrollBar>();
    m_horizontalScrollBar->setDock(DockPosition::Bottom);
    m_horizontalScrollBar->onBarMovedCaller.add(this, &ScrollControl::onHorizontalBarMoved);
    m_canScrollHorizontal=true;
    m_horizontalScrollBar->setNudgeAmount(30);

    m_innerPanel=newChild<Base>();
    m_innerPanel->setPos(0, 0);
    m_innerPanel->setMargin(Margin(5, 5, 5, 5));
    m_innerPanel->sendToBack();
    m_innerPanel->setMouseInputEnabled(true);
    m_autoHideBars=true;

    // arrange() places the bars and the inner panel itself (placeContents).
    m_verticalScrollBar->setPlacement(Placement::Owner);
    m_horizontalScrollBar->setPlacement(Placement::Owner);
    m_innerPanel->setPlacement(Placement::Owner);
}

void ScrollControl::setScroll(bool h, bool v)
{
    m_canScrollVertical=v;
    m_canScrollHorizontal=h;
    m_verticalScrollBar->setHidden(!m_canScrollVertical);
    m_horizontalScrollBar->setHidden(!m_canScrollHorizontal);
}

void ScrollControl::setInnerSize(int w, int h)
{
    m_innerPanel->setSize(w, h);
}

void ScrollControl::setVirtualContentHeight(int height)
{
    height=std::max(height, -1);

    if(m_virtualHeight == height)
        return;

    m_virtualHeight=height;
    invalidate();
}

int ScrollControl::getMaxVerticalScrollOffset() const
{
    return std::max(0, m_virtualHeight-m_viewportHeight);
}

void ScrollControl::setVerticalScrollOffset(int offset)
{
    offset=gweni::clamp(offset, 0, getMaxVerticalScrollOffset());

    if(offset == m_virtualOffset)
        return;

    m_virtualOffset=offset;
    showVirtualOffset();
    invalidate();
}

void ScrollControl::showVirtualOffset()
{
    const int maxOffset=getMaxVerticalScrollOffset();
    const float amount=maxOffset > 0?static_cast<float>(m_virtualOffset)/maxOffset:0.f;

    if(m_verticalScrollBar->getScrolledAmount() != amount)
        m_verticalScrollBar->setScrolledAmount(amount, true);

    m_shownVirtualAmount=m_verticalScrollBar->getScrolledAmount();
}

void ScrollControl::onVeritcalBarMoved(event::Info)
{
    invalidate();
}

void ScrollControl::onHorizontalBarMoved(event::Info)
{
    invalidate();
}

void ScrollControl::onChildBoundsChanged(gweni::Rect /*oldChildBounds*/, Base * /*child*/)
{
    // During a pass the inner panel's and the bars' bounds are arrange()'s
    // doing, and it sized them for each other; outside one (app code resizing
    // the inner panel, say) the bars have to be worked out again.
    if(inLayoutPass())
        return;

    invalidate();
}

namespace
{

// Moves a bar by a wheel delta if that changes where it is; a bar at the end
// the wheel pushes towards stays there and the wheel is not used.
bool wheelBar(BaseScrollBar *bar, int iDelta)
{
    const float amount=bar->getScrolledAmount();
    const float moved=gweni::clamp(amount-bar->getNudgeAmount()*iDelta/60.0f, 0.f, 1.f);

    if(moved == amount)
        return false;

    bar->setScrolledAmount(moved, true);
    return true;
}

}//namespace

// Only a bar that is shown and enabled scrolls: vertical first, then
// horizontal. A wheel neither axis can use goes on to the parent, so it
// reaches an ancestor that can scroll.
bool ScrollControl::onMouseWheeled(int iDelta)
{
    if(canScrollVertical() && m_verticalScrollBar->visible()
        && !m_verticalScrollBar->isDisabled() && wheelBar(m_verticalScrollBar, iDelta))
        return true;

    if(canScrollHorizontal() && m_horizontalScrollBar->visible()
        && !m_horizontalScrollBar->isDisabled() && wheelBar(m_horizontalScrollBar, iDelta))
        return true;

    return ParentClass::onMouseWheeled(iDelta);
}

bool ScrollControl::contentsAreDocked()
{
    if(!m_innerPanel)
        return false;

    for(auto &&child : m_innerPanel->getChildren())
    {
        if(child->getDock() == DockPosition::None)
            return false;
    }

    return true;
}

// On an axis the app asked a size for, a ScrollControl asks for that size: it
// is the outer size, the bars go inside it, and content that does not fit is
// scrolled. On an axis with no request it asks for its content: the inner
// panel's preferred size and margins. Neither the scroll position nor a bar an
// earlier pass showed counts, so the size does not depend on earlier frames;
// placeContents() shows a bar when the control got less than its content.
void ScrollControl::calculateSize(skin::Base *skin, Dim dim)
{
    if(!m_innerPanel)
    {
        ParentClass::calculateSize(skin, dim);
        return;
    }

    m_innerPanel->calculateSize(skin, dim);

    if(!m_verticalScrollBar->hidden())
    {
        m_verticalScrollBar->calculateSize(skin, dim);

        if(dim == Dim::X)
            m_barSize.width=m_verticalScrollBar->getPreferredSize().width;
    }

    if(!m_horizontalScrollBar->hidden())
    {
        m_horizontalScrollBar->calculateSize(skin, dim);

        if(dim == Dim::Y)
            m_barSize.height=m_horizontalScrollBar->getPreferredSize().height;
    }

    Size content=m_innerPanel->getPreferredSize();
    const Margin &margin=m_innerPanel->getMargin();
    const Rect &requested=getRequestedBounds();

    if(hasVirtualContent())
        content.height=m_virtualHeight;

    if(dim == Dim::X)
        m_preferredSize.width=preferredFromContent(sizedByRequest(Dim::X)?requested.w:
            content.width+margin.left+margin.right+m_padding.left+m_padding.right, Dim::X);
    else
        m_preferredSize.height=preferredFromContent(sizedByRequest(Dim::Y)?requested.h:
            content.height+margin.top+margin.bottom+m_padding.top+m_padding.bottom, Dim::Y);
}

// Whether a bar shows depends on this control's height, which its parent only
// sets in the Y pass, and whether the vertical bar shows decides the width the
// X pass would lay the content out at. So the X pass leaves this subtree
// alone, and the Y pass does all of it once every input is final: this
// control's size (the parent placed it in X and Y), the content's preferred
// size (calculateSize ran over the whole tree in X and Y) and the bars'.
// placeContents() decides the bars in arithmetic and sizes the inner panel and
// the bars for that; only then are they laid out, X then Y, so nothing a child
// has read changes after it read it.
//
// Content whose height depends on its width (wrapped text) was measured at the
// width it had in the previous pass. It is measured again once laid out at its
// new width; if the height changed, the inner panel is resized for it, which
// is reported as a re-dirty, and the canvas's next pass decides the bars with
// the new height. No library control puts such content in a ScrollControl.
void ScrollControl::arrange(skin::Base *skin, Dim dim)
{
    if(!m_innerPanel)
    {
        ParentClass::arrange(skin, dim);
        return;
    }

    if(dim == Dim::X)
        return;

    stampLayoutPass();

    if(needsLayout())
    {
        m_needsLayout=false;
        layout(skin);
    }

    placeContents();

    {
        PlacementScope placement;

        contentsPlaced(skin);
    }

    for(auto &&child : m_children)
    {
        if(child->hidden())
            continue;

        // A bar placeContents() has just shown was not measured this pass.
        if(child != m_innerPanel)
            child->calculateSize(skin, Dim::X);

        child->arrange(skin, Dim::X);
    }

    const int contentHeight=m_innerPanel->getPreferredSize().height;

    for(auto &&child : m_children)
    {
        if(!child->hidden())
            child->calculateSize(skin, Dim::Y);
    }

    // Virtual content is the viewport's height whatever its children ask for.
    if(!hasVirtualContent() && m_innerPanel->getPreferredSize().height != contentHeight)
    {
        placeChildSize(m_innerPanel, m_innerPanel->getWidth(),
            std::max(getHeight(), m_innerPanel->getPreferredSize().height));
    }

    for(auto &&child : m_children)
    {
        if(!child->hidden())
            child->arrange(skin, Dim::Y);
    }

    postLayout(skin);
}

void ScrollControl::placeContents()
{
    const int width=getWidth();
    const int height=getHeight();
    const bool virtualContent=hasVirtualContent();
    Size content=m_innerPanel->getPreferredSize();

    if(virtualContent)
        content.height=m_virtualHeight;

    // Which bars show. Showing one narrows the viewport the other is compared
    // with, and a narrower viewport never makes a bar go away, so the second
    // round has seen every bar the first one turned on.
    bool verticalRequired=false;
    bool horizontalRequired=false;
    bool verticalShown=false;
    bool horizontalShown=false;

    for(int round=0; round < 2; ++round)
    {
        verticalRequired=m_canScrollVertical &&
            content.height > height-(horizontalShown?m_barSize.height:0);
        verticalShown=m_canScrollVertical && (verticalRequired || !m_autoHideBars);

        horizontalRequired=m_canScrollHorizontal &&
            content.width > width-(verticalShown?m_barSize.width:0);
        horizontalShown=m_canScrollHorizontal && (horizontalRequired || !m_autoHideBars);
    }

    const int barWidth=verticalShown?m_barSize.width:0;
    const int barHeight=horizontalShown?m_barSize.height:0;
    // Virtual content is not scrolled: the inner panel is the viewport.
    const int innerHeight=virtualContent?height-barHeight:std::max(height, content.height);
    int innerWidth;

    // The inner panel runs under the vertical bar: by one pixel when the
    // content cannot scroll sideways, to the full width when it can.
    if(m_canScrollHorizontal)
        innerWidth=std::max(width, content.width);
    else
        innerWidth=width-(verticalShown?barWidth-1:0);

    // Everything below sets up children this pass lays out next.
    PlacementScope placement;

    if(m_canScrollVertical)
        setVerticalScrollRequired(verticalRequired);
    else
        m_verticalScrollBar->setHidden(true);

    if(m_canScrollHorizontal)
        setHorizontalScrollRequired(horizontalRequired);
    else
        m_horizontalScrollBar->setHidden(true);

    m_verticalScrollBar->setContentSize(virtualContent?m_virtualHeight:innerHeight);
    m_verticalScrollBar->setViewableContentSize(height-barHeight);
    m_horizontalScrollBar->setContentSize(innerWidth);
    m_horizontalScrollBar->setViewableContentSize(width-barWidth);

    // Virtual content scrolls by its offset. A bar that is not where this
    // control last put it was moved by the user (its thumb, its buttons, the
    // wheel; a bar no longer required was scrolled back to the start above),
    // so the offset is taken from it, at the viewport it was moved in. The
    // offset is then kept inside the viewport this pass gives, and the bar
    // shows it.
    if(virtualContent)
    {
        const float amount=m_verticalScrollBar->getScrolledAmount();

        if(amount != m_shownVirtualAmount)
            m_virtualOffset=static_cast<int>(std::lround(amount*getMaxVerticalScrollOffset()));

        m_viewportHeight=height-barHeight;
        m_virtualOffset=gweni::clamp(m_virtualOffset, 0, getMaxVerticalScrollOffset());
        showVirtualOffset();
    }

    // Docked as Base::arrangeHorizontal/Vertical would dock them: the vertical
    // bar on the right at full height, the horizontal one along the bottom
    // beside it.
    Rect area(m_padding.left, m_padding.top,
        width-m_padding.left-m_padding.right, height-m_padding.top-m_padding.bottom);

    if(verticalShown)
    {
        const Margin &margin=m_verticalScrollBar->getMargin();

        placeChild(m_verticalScrollBar, area.x+area.w-barWidth-margin.right, area.y+margin.top,
            barWidth, area.h-margin.top-margin.bottom);
        area.w-=barWidth+margin.left+margin.right;
    }

    if(horizontalShown)
    {
        const Margin &margin=m_horizontalScrollBar->getMargin();

        placeChild(m_horizontalScrollBar, area.x, area.y+area.h-barHeight-margin.bottom,
            area.w-margin.left-margin.right, barHeight);
        area.h-=barHeight+margin.top+margin.bottom;
    }

    m_innerBounds=area;

    // The scroll position, from the amount each shown bar is scrolled by.
    int innerX=0;
    int innerY=0;

    if(verticalShown && !virtualContent)
        innerY=-(innerHeight-height+barHeight)*m_verticalScrollBar->getScrolledAmount();

    if(horizontalShown)
        innerX=-(innerWidth-width+barWidth)*m_horizontalScrollBar->getScrolledAmount();

    placeChild(m_innerPanel, innerX, innerY, innerWidth, innerHeight);
}

void ScrollControl::updateScrollBars()
{
    if(!m_innerPanel || inLayoutPass())
        return;

    invalidate();
}

void ScrollControl::setVerticalScrollRequired(bool required)
{
    if(!required)
    {
        if(m_verticalScrollBar->getScrolledAmount() != 0)
            m_verticalScrollBar->setScrolledAmount(0, true);

        m_verticalScrollBar->setDisabled(true);

        if(m_autoHideBars)
            m_verticalScrollBar->setHidden(true);
    }
    else
    {
        m_verticalScrollBar->setHidden(false);
        m_verticalScrollBar->setDisabled(false);
    }
}

void ScrollControl::setHorizontalScrollRequired(bool required)
{
    if(!required)
    {
        if(m_horizontalScrollBar->getScrolledAmount() != 0)
            m_horizontalScrollBar->setScrolledAmount(0, true);

        m_horizontalScrollBar->setDisabled(true);

        if(m_autoHideBars)
            m_horizontalScrollBar->setHidden(true);
    }
    else
    {
        m_horizontalScrollBar->setHidden(false);
        m_horizontalScrollBar->setDisabled(false);
    }
}

void ScrollControl::scrollToBottom()
{
    if(!canScrollVertical())
        return;

    updateScrollBars();
    m_verticalScrollBar->scrollToBottom();
}

void ScrollControl::scrollToTop()
{
    if(canScrollVertical())
    {
        updateScrollBars();
        m_verticalScrollBar->scrollToTop();
    }
}

void ScrollControl::scrollToLeft()
{
    if(canScrollHorizontal())
    {
        updateScrollBars();
        m_horizontalScrollBar->scrollToLeft();
    }
}

void ScrollControl::scrollToRight()
{
    if(canScrollHorizontal())
    {
        updateScrollBars();
        m_horizontalScrollBar->scrollToRight();
    }
}

void ScrollControl::clear()
{
    m_innerPanel->removeAllChildren();
}

}//namespace controls
}//namespace gweni
