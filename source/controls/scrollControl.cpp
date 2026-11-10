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

Size ScrollControl::contentSize() const
{
    Size content=m_innerPanel->getPreferredSize();

    if(hasVirtualContent())
        content.height=m_virtualHeight;
    return content;
}

int ScrollControl::getMaxVerticalScrollOffset() const
{
    if(hasVirtualContent())
        return std::max(0, m_virtualHeight-m_viewportHeight);
    return m_scrollRange[1];
}

void ScrollControl::setVerticalScrollOffset(int offset)
{
    offset=std::max(offset, 0);

    if(hasVirtualContent())
        offset=std::min(offset, getMaxVerticalScrollOffset());

    if(offset == m_scrollOffset[1])
        return;

    m_scrollOffset[1]=offset;
    m_pendingEdge[1]=ScrollEdge::None;
    showScrollOffset(Dim::Y);
    invalidate();
}

void ScrollControl::setHorizontalScrollOffset(int offset)
{
    offset=std::max(offset, 0);

    if(offset == m_scrollOffset[0])
        return;

    m_scrollOffset[0]=offset;
    m_pendingEdge[0]=ScrollEdge::None;
    showScrollOffset(Dim::X);
    invalidate();
}

void ScrollControl::showScrollOffset(Dim dim)
{
    BaseScrollBar *bar=scrollBar(dim);
    const int maxOffset=getMaxScrollOffset(dim);
    const float amount=maxOffset > 0?
        std::min(1.f, static_cast<float>(m_scrollOffset[axis(dim)])/maxOffset):0.f;

    if(bar->getScrolledAmount() != amount)
        bar->setScrolledAmount(amount, true);

    m_shownAmount[axis(dim)]=bar->getScrolledAmount();
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

    m_innerPanel->doCalculateSize(skin, dim);

    if(!m_verticalScrollBar->hidden())
    {
        m_verticalScrollBar->doCalculateSize(skin, dim);

        if(dim == Dim::X)
            m_barSize.width=m_verticalScrollBar->getPreferredSize().width;
    }

    if(!m_horizontalScrollBar->hidden())
    {
        m_horizontalScrollBar->doCalculateSize(skin, dim);

        if(dim == Dim::Y)
            m_barSize.height=m_horizontalScrollBar->getPreferredSize().height;
    }

    const Size content=contentSize();
    const Margin &margin=m_innerPanel->getMargin();
    const Rect &requested=getRequestedBounds();

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
            child->doCalculateSize(skin, Dim::X);

        child->doArrange(skin, Dim::X);
    }

    const int contentHeight=m_innerPanel->getPreferredSize().height;

    for(auto &&child : m_children)
    {
        if(!child->hidden())
            child->doCalculateSize(skin, Dim::Y);
    }

    // Virtual content is the viewport's height whatever its children ask for.
    if(!hasVirtualContent() && m_innerPanel->getPreferredSize().height != contentHeight)
    {
        placeChildSize(m_innerPanel, m_innerPanel->getWidth(),
            std::max(m_viewport.h, m_innerPanel->getPreferredSize().height));
    }

    for(auto &&child : m_children)
    {
        if(!child->hidden())
            child->doArrange(skin, Dim::Y);
    }

    postLayout(skin);
}

// The viewport is what the content is seen through: this control less its
// padding, the bars that show (with their margins) and the inner panel's
// margin. calculateSize() asks for the content plus those insets, so at its
// preferred size a ScrollControl shows its content exactly in the viewport,
// with the inset on all four sides. The bars are decided against the viewport,
// the inner panel is placed at its origin (less the scroll offset) and is at
// least its size, and the bars range over the inner panel through it.
void ScrollControl::placeContents()
{
    const int width=getWidth();
    const int height=getHeight();
    const bool virtualContent=hasVirtualContent();
    const Margin &inset=m_innerPanel->getMargin();
    const Margin &verticalMargin=m_verticalScrollBar->getMargin();
    const Margin &horizontalMargin=m_horizontalScrollBar->getMargin();
    const Size content=contentSize();

    // The viewport with no bar shown, and what each bar takes from it.
    const int openWidth=width-m_padding.left-m_padding.right-inset.left-inset.right;
    const int openHeight=height-m_padding.top-m_padding.bottom-inset.top-inset.bottom;
    const int verticalSpan=m_barSize.width+verticalMargin.left+verticalMargin.right;
    const int horizontalSpan=m_barSize.height+horizontalMargin.top+horizontalMargin.bottom;

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
            content.height > openHeight-(horizontalShown?horizontalSpan:0);
        verticalShown=m_canScrollVertical && (verticalRequired || !m_autoHideBars);

        horizontalRequired=m_canScrollHorizontal &&
            content.width > openWidth-(verticalShown?verticalSpan:0);
        horizontalShown=m_canScrollHorizontal && (horizontalRequired || !m_autoHideBars);
    }

    const int barWidth=verticalShown?m_barSize.width:0;
    const int barHeight=horizontalShown?m_barSize.height:0;
    const int viewportWidth=std::max(0, openWidth-(verticalShown?verticalSpan:0));
    const int viewportHeight=std::max(0, openHeight-(horizontalShown?horizontalSpan:0));
    // Virtual content is not scrolled: the inner panel is the viewport.
    const int innerHeight=virtualContent?viewportHeight:std::max(viewportHeight, content.height);
    int innerWidth;

    // When the content can scroll sideways the inner panel is the content's
    // width. When it cannot, it is the viewport's, and one pixel more where it
    // meets the vertical bar (no margin on its right): it runs under the bar
    // by that pixel. Past a margin it would only make the right inset one
    // less than the left.
    if(m_canScrollHorizontal)
        innerWidth=std::max(viewportWidth, content.width);
    else
        innerWidth=viewportWidth+((verticalShown && inset.right == 0)?1:0);

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
    m_verticalScrollBar->setViewableContentSize(viewportHeight);
    m_horizontalScrollBar->setContentSize(innerWidth);
    m_horizontalScrollBar->setViewableContentSize(viewportWidth);

    // Docked as Base::arrangeHorizontal/Vertical would dock them: the vertical
    // bar on the right at full height, the horizontal one along the bottom
    // beside it.
    Rect area(m_padding.left, m_padding.top,
        width-m_padding.left-m_padding.right, height-m_padding.top-m_padding.bottom);

    if(verticalShown)
    {
        placeChild(m_verticalScrollBar, area.x+area.w-barWidth-verticalMargin.right,
            area.y+verticalMargin.top, barWidth,
            area.h-verticalMargin.top-verticalMargin.bottom);
        area.w-=verticalSpan;
    }

    if(horizontalShown)
    {
        placeChild(m_horizontalScrollBar, area.x,
            area.y+area.h-barHeight-horizontalMargin.bottom,
            area.w-horizontalMargin.left-horizontalMargin.right, barHeight);
        area.h-=horizontalSpan;
    }

    m_innerBounds=area;

    const Rect oldViewport=m_viewport;
    const Rect oldInner=m_innerPanel->getBounds();

    m_viewport=Rect(area.x+inset.left, area.y+inset.top, viewportWidth, viewportHeight);

    m_viewportHeight=viewportHeight;

    // The scroll offsets. A bar that is not where this control last put it
    // was moved by the user (its thumb, its buttons, the wheel; a bar no
    // longer required was scrolled back to the start above), so the offset
    // is taken from it, at the content it was moved against: the last pass's.
    // The offset is then kept inside the content and viewport this pass
    // gives, so content that grows keeps it and content that shrinks clamps
    // it, and the bar shows it.
    const int range[2]={
        horizontalRequired?std::max(0, innerWidth-viewportWidth):0,
        verticalRequired?std::max(0, (virtualContent?m_virtualHeight:innerHeight)-viewportHeight):0};

    for(Dim dim : {Dim::X, Dim::Y})
    {
        const int index=axis(dim);
        const float amount=scrollBar(dim)->getScrolledAmount();

        if(amount != m_shownAmount[index])
            m_scrollOffset[index]=static_cast<int>(std::lround(
                static_cast<double>(amount)*m_scrollRange[index]));

        // a scrollTo*() since the last pass, at this pass's content
        if(m_pendingEdge[index] != ScrollEdge::None)
        {
            m_scrollOffset[index]=(m_pendingEdge[index] == ScrollEdge::End)?range[index]:0;
            m_pendingEdge[index]=ScrollEdge::None;
        }

        m_scrollRange[index]=range[index];
        m_scrollOffset[index]=gweni::clamp(m_scrollOffset[index], 0, range[index]);
        showScrollOffset(dim);
    }

    // At offset 0 the content's top-left is the viewport's, at the range its
    // bottom-right is. Virtual content is not scrolled vertically.
    const int scrollX=-m_scrollOffset[0];
    const int scrollY=virtualContent?0:-m_scrollOffset[1];

    placeChild(m_innerPanel, m_viewport.x+scrollX, m_viewport.y+scrollY, innerWidth, innerHeight);

    // The content is clipped to the viewport (getChildClipRect), so a new
    // viewport gives every slot below the inner panel a new clip box. The
    // panel's onBoundsChanged marks its subtree only when it moved: a panel
    // resized at the same origin measures its region against the new
    // viewport alone (a bar hiding: both the old and the new panel cover
    // it), and one that kept its bounds marks nothing. Either way the
    // content is marked here, or its slots keep the old clip.
    const bool innerMoved=m_innerPanel->getX() != oldInner.x
        || m_innerPanel->getY() != oldInner.y;

    if(!(m_viewport == oldViewport) && !innerMoved)
        m_innerPanel->addStateChangeRecursive(StateChange_Geometry);
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

void ScrollControl::scrollToEdge(Dim dim, ScrollEdge edge)
{
    if(!(dim == Dim::X?canScrollHorizontal():canScrollVertical()))
        return;

    m_pendingEdge[axis(dim)]=edge;
    invalidate();
}

void ScrollControl::scrollToBottom()
{
    scrollToEdge(Dim::Y, ScrollEdge::End);
}

void ScrollControl::scrollToTop()
{
    scrollToEdge(Dim::Y, ScrollEdge::Start);
}

void ScrollControl::scrollToLeft()
{
    scrollToEdge(Dim::X, ScrollEdge::Start);
}

void ScrollControl::scrollToRight()
{
    scrollToEdge(Dim::X, ScrollEdge::End);
}

void ScrollControl::clear()
{
    m_innerPanel->removeAllChildren();
}

}//namespace controls
}//namespace gweni
