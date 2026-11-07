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

bool ScrollControl::onMouseWheeled(int iDelta)
{
    if(canScrollVertical() && m_verticalScrollBar->visible())
    {
        if(m_verticalScrollBar->setScrolledAmount(
            m_verticalScrollBar->getScrolledAmount()
            - m_verticalScrollBar->getNudgeAmount()  *iDelta / 60.0f, true))
            return true;
    }

    if(canScrollHorizontal() && m_horizontalScrollBar->visible())
    {
        if(m_horizontalScrollBar->setScrolledAmount(
            m_horizontalScrollBar->getScrolledAmount()
            - m_horizontalScrollBar->getNudgeAmount()  *iDelta / 60.0f, true))
            return true;
    }

    return false;
}

void ScrollControl::render(skin::Base *skin)
{
#if 0
    // Debug render - this shouldn't render ANYTHING REALLY - it should be up to
    // the parent!
    gweni::Rect rect=getRenderBounds();
    gweni::renderer::Base *render=skin->getRenderer();
    render->setDrawColor(gweni::Color(255, 255, 0, 100));
    render->drawFilledRect(rect);
    render->setDrawColor(gweni::Color(255, 0, 0, 100));
    render->drawFilledRect(m_innerPanel->getBounds());
    render->renderText(skin->getDefaultFont(),
        gweni::Point(0, 0),
        format("Offset: %i %i", m_innerPanel->getX(), m_innerPanel->getY()));
#else // 0
    (void)skin;
#endif // 0
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

// What a ScrollControl asks for is its content: the inner panel's preferred
// size and margins. Neither the scroll position nor a bar an earlier pass
// showed counts, so the size does not depend on earlier frames; a bar is only
// shown when this control got less than that anyway.
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

    const Size &content=m_innerPanel->getPreferredSize();
    const Margin &margin=m_innerPanel->getMargin();

    if(dim == Dim::X)
        m_preferredSize.width=preferredFromContent(content.width+margin.left+margin.right
            +m_padding.left+m_padding.right, Dim::X);
    else
        m_preferredSize.height=preferredFromContent(content.height+margin.top+margin.bottom
            +m_padding.top+m_padding.bottom, Dim::Y);
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

    if(m_innerPanel->getPreferredSize().height != contentHeight)
    {
        m_innerPanel->setSize(m_innerPanel->getWidth(),
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
    const Size &content=m_innerPanel->getPreferredSize();

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
    const int innerHeight=std::max(height, content.height);
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

    m_verticalScrollBar->setContentSize(innerHeight);
    m_verticalScrollBar->setViewableContentSize(height-barHeight);
    m_horizontalScrollBar->setContentSize(innerWidth);
    m_horizontalScrollBar->setViewableContentSize(width-barWidth);

    // Docked as Base::arrangeHorizontal/Vertical would dock them: the vertical
    // bar on the right at full height, the horizontal one along the bottom
    // beside it.
    Rect area(m_padding.left, m_padding.top,
        width-m_padding.left-m_padding.right, height-m_padding.top-m_padding.bottom);

    if(verticalShown)
    {
        const Margin &margin=m_verticalScrollBar->getMargin();

        m_verticalScrollBar->setBounds(area.x+area.w-barWidth-margin.right, area.y+margin.top,
            barWidth, area.h-margin.top-margin.bottom);
        area.w-=barWidth+margin.left+margin.right;
    }

    if(horizontalShown)
    {
        const Margin &margin=m_horizontalScrollBar->getMargin();

        m_horizontalScrollBar->setBounds(area.x, area.y+area.h-barHeight-margin.bottom,
            area.w-margin.left-margin.right, barHeight);
        area.h-=barHeight+margin.top+margin.bottom;
    }

    m_innerBounds=area;

    // The scroll position, from the amount each shown bar is scrolled by.
    int innerX=0;
    int innerY=0;

    if(verticalShown)
        innerY=-(innerHeight-height+barHeight)*m_verticalScrollBar->getScrolledAmount();

    if(horizontalShown)
        innerX=-(innerWidth-width+barWidth)*m_horizontalScrollBar->getScrolledAmount();

    m_innerPanel->setBounds(innerX, innerY, innerWidth, innerHeight);
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
