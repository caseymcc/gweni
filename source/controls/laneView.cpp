/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/laneView.h>
#include <gweni/gweni.h>
#include <gweni/controls/text.h>
#include <gweni/skin.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace gweni
{
namespace controls
{

// ---- the body and the caption column --------------------------------------

namespace internal
{

GWENI_CONTROL_CONSTRUCTOR(LaneBody)
{
    // the lanes scroll up and down, never sideways, through virtual content:
    // nothing is ever placed far outside the viewport
    setScroll(false, true);
    setAutoHideBars(true);
    setVirtualContentHeight(0);
    m_innerPanel->setMargin(Margin(0, 0, 0, 0));
    // the view takes the mouse over its lanes: only the bar is the body's
    m_innerPanel->setMouseInputEnabled(false);
}

void LaneBody::contentsPlaced(skin::Base * /*skin*/)
{
    if(m_view)
        m_view->bodyScrolled(getVerticalScrollOffset());
}

GWENI_CONTROL_CONSTRUCTOR(LaneCaptionColumn)
{
    setMouseInputEnabled(false);
}

// The captions are placed by the LaneView (placeCaptions(), in its Y arrange);
// this lays out the shown ones, X then Y, once both are final.
void LaneCaptionColumn::arrange(skin::Base *skin, Dim dim)
{
    if(dim == Dim::X)
        return;

    stampLayoutPass();
    if(needsLayout())
    {
        m_needsLayout=false;
        layout(skin);
    }

    countLayoutChildrenScanned(m_children);
    for(auto &&child : m_children)
    {
        if(child->hidden())
            continue;
        child->doCalculateSize(skin, Dim::X);
        child->doArrange(skin, Dim::X);
    }

    countLayoutChildrenScanned(m_children);
    for(auto &&child : m_children)
    {
        if(!child->hidden())
            child->doCalculateSize(skin, Dim::Y);
    }

    countLayoutChildrenScanned(m_children);
    for(auto &&child : m_children)
    {
        if(!child->hidden())
            child->doArrange(skin, Dim::Y);
    }

    postLayout(skin);
}

}//namespace internal

// ---- the lane view ----------------------------------------------------------

GWENI_CONTROL_CONSTRUCTOR(LaneView)
{
    setBounds(gweni::Rect(0, 0, 480, 240));
    // inside the frame's 1-unit border, with room clear of it
    setPadding(Padding(4, 4, 4, 4));

    m_body=newChild<internal::LaneBody>();
    m_body->m_view=this;
    m_body->setPlacement(Placement::Owner);

    m_captions=newChild<internal::LaneCaptionColumn>();
    m_captions->setPlacement(Placement::Owner);
}

// the labels, the body and the caption column are children: ~Base deletes them
LaneView::~LaneView()
{
}

// ---- lanes ------------------------------------------------------------------

void LaneView::appendLane(const String &caption)
{
    m_laneCaptions.push_back(caption);
    lanesChanged();
}

void LaneView::eraseLane(size_t lane)
{
    if(lane >= m_laneCaptions.size())
        return;

    m_laneCaptions.erase(m_laneCaptions.begin()+static_cast<std::ptrdiff_t>(lane));
    // the lanes after it moved up: every caption is bound again
    unbindCaptions();
    lanesChanged();
}

void LaneView::eraseLanes()
{
    if(m_laneCaptions.empty())
        return;

    m_laneCaptions.clear();
    unbindCaptions();
    lanesChanged();
}

void LaneView::renameLane(size_t lane, const String &caption)
{
    if(lane >= m_laneCaptions.size() || m_laneCaptions[lane] == caption)
        return;

    m_laneCaptions[lane]=caption;
    if(internal::Text *label=getCaptionLabel(lane))
        label->setText(caption);
    laneCaptionChanged(lane);
    // an automatic column follows the widest caption
    if(m_captionWidthSetting <= 0)
        invalidate();
}

String LaneView::getLaneCaption(size_t lane) const
{
    return lane < m_laneCaptions.size() ? m_laneCaptions[lane] : String();
}

internal::Text *LaneView::getCaptionLabel(size_t lane) const
{
    for(const CaptionRow &row : m_captionRows)
    {
        if(row.lane == lane)
            return row.label;
    }
    return nullptr;
}

void LaneView::unbindCaptions()
{
    for(CaptionRow &row : m_captionRows)
        row.lane=NoLane;
}

void LaneView::setLaneHeight(int height)
{
    height=std::max(height, minLaneHeight());
    if(height == m_laneHeight)
        return;

    m_laneHeight=height;
    lanesChanged();
}

void LaneView::setCaptionColumnWidth(int width)
{
    width=std::max(width, 0);
    if(width == m_captionWidthSetting)
        return;

    m_captionWidthSetting=width;
    invalidate();
}

void LaneView::lanesChanged()
{
    // the virtual height, the caption column and the captions in view
    invalidate();
    laneContentMoved();
}

// ---- scrolling --------------------------------------------------------------

int LaneView::getScrollOffset() const
{
    return m_body->getVerticalScrollOffset();
}

void LaneView::setScrollOffset(int offset)
{
    m_body->setVerticalScrollOffset(std::max(offset, 0));
}

void LaneView::scrollToLane(size_t lane)
{
    if(lane >= m_laneCaptions.size())
        return;

    const int top=static_cast<int>(lane)*m_laneHeight;
    const int bottom=top+m_laneHeight;
    const int offset=getScrollOffset();

    if(top < offset)
        setScrollOffset(top);
    else if(bottom > offset+m_area.h)
        setScrollOffset(std::min(top, bottom-m_area.h));
}

// GLFW's translator gives 60 per wheel notch.
bool LaneView::onMouseWheeled(int delta)
{
    const int before=getScrollOffset();
    const long moved=std::lround(-static_cast<double>(delta)*WheelLanes*m_laneHeight/60.0);

    setScrollOffset(static_cast<int>(std::clamp<long>(before+moved, 0, INT32_MAX)));
    if(getScrollOffset() != before)
        return true;

    return ParentClass::onMouseWheeled(delta);
}

void LaneView::bodyScrolled(int offset)
{
    if(offset == m_scrollOffset)
        return;

    m_scrollOffset=offset;
    // everything in the lanes in view moved
    laneContentMoved();
}

// ---- geometry ---------------------------------------------------------------

int LaneView::laneAt(int y) const
{
    if(y < m_area.y || y >= m_area.y+m_area.h)
        return -1;

    const int64_t lane=(static_cast<int64_t>(y)-m_area.y+m_scrollOffset)/m_laneHeight;

    if(lane < 0 || lane >= static_cast<int64_t>(m_laneCaptions.size()))
        return -1;
    return static_cast<int>(lane);
}

Rect LaneView::getCaptionColumnBounds() const
{
    return Rect(m_viewport.x, m_area.y, m_captionWidth, m_area.h);
}

Rect LaneView::getAxisRow() const
{
    return Rect(m_area.x, m_axisTop, m_area.w, std::max(0, m_area.y-m_axisTop));
}

double LaneView::xPosition(double t) const
{
    return m_area.x+axis::positionOf(t, getXMin(), getXMax(), m_area.w);
}

double LaneView::timeAt(double x) const
{
    const double min=getXMin();
    const double max=getXMax();

    if(m_area.w <= 0)
        return min;
    return min+(x-m_area.x)/m_area.w*(max-min);
}

bool LaneView::getLaneRect(size_t lane, Rect &rect) const
{
    if(lane >= m_laneCaptions.size() || m_area.w <= 0 || m_area.h <= 0)
        return false;

    const int64_t top=static_cast<int64_t>(m_area.y)+static_cast<int64_t>(lane)*m_laneHeight-m_scrollOffset;

    if(top+m_laneHeight <= m_area.y || top >= m_area.y+m_area.h)
        return false;

    rect=Rect(m_area.x, static_cast<int>(top), m_area.w, m_laneHeight);
    return true;
}

// ---- measures ---------------------------------------------------------------

void LaneView::renewMeasureKey()
{
    const Font &font=getSkin()->getDefaultFont();
    const renderer::Base *renderer=getSkin()->getRenderer();
    const unsigned serial=renderer ? renderer->textLayoutSerial() : 0;

    if(m_measureFont == &font && m_measureFontValue == font && m_measureRenderer == renderer
        && m_measureSerial == serial)
        return;

    m_measureFont=&font;
    m_measureFontValue=font;
    m_measureRenderer=renderer;
    m_measureSerial=serial;
    m_widths.clear();
    m_lineHeight=-1;
}

int LaneView::labelWidth(const String &text)
{
    const auto found=m_widths.find(text);

    if(found != m_widths.end())
        return found->second;

    // the time axis tries many strings as its range moves, and the captions
    // are a hundred or so: keep it bounded
    if(m_widths.size() >= 1024)
        m_widths.clear();

    const int width=measureText(getSkin()->getDefaultFont(), text).x;

    m_widths.emplace(text, width);
    return width;
}

int LaneView::labelLineHeight()
{
    if(m_lineHeight < 0)
        m_lineHeight=measureLineHeight(getSkin()->getDefaultFont());
    return m_lineHeight;
}

// ---- layout -----------------------------------------------------------------

internal::Text *LaneView::makeLabel(Base *parent, const Color &color)
{
    internal::Text *label=nullptr;

    {
        PartScope part;

        label=parent->newChild<internal::Text>();
    }
    label->setPlacement(Placement::Owner);
    label->setTextColor(color);
    return label;
}

void LaneView::updateColors()
{
    const Color &axisText=getSkin()->Colors.Timeline.AxisText;
    const Color &captionText=getSkin()->Colors.Timeline.CaptionText;

    for(internal::Text *label : m_tickLabels)
        label->setTextColor(axisText);
    for(CaptionRow &row : m_captionRows)
        row.label->setTextColor(captionText);
}

// The body and the caption column, which need nothing of the body's layout:
// the lane area and the ticks, which need its viewport, are placeAxis()'s.
void LaneView::layout(skin::Base *skin)
{
    ParentClass::layout(skin);
    renewMeasureKey();

    // the children are laid out next: what is written to them is placement
    PlacementScope placement;

    const int lineHeight=labelLineHeight();
    const Padding &padding=getPadding();
    const int innerLeft=padding.left;
    const int innerTop=padding.top;
    const int innerWidth=std::max(0, getWidth()-padding.left-padding.right);
    const int innerBottom=std::max(innerTop, getHeight()-padding.bottom);

    int captionWidth=m_captionWidthSetting;

    if(captionWidth <= 0)
    {
        captionWidth=0;
        for(const String &caption : m_laneCaptions)
            captionWidth=std::max(captionWidth, labelWidth(caption)+2*CaptionPadding);
        captionWidth=std::min(captionWidth, innerWidth/3);
    }
    m_captionWidth=std::min(captionWidth, innerWidth);

    // the axis's labels above the lanes
    m_axisTop=innerTop;

    const int bodyTop=std::min(innerBottom, innerTop+lineHeight+LabelGap);
    const int bodyHeight=innerBottom-bodyTop;

    m_body->setVirtualContentHeight(static_cast<int>(std::min<int64_t>(
        static_cast<int64_t>(m_laneCaptions.size())*m_laneHeight, INT32_MAX)));
    placeChild(m_body, innerLeft, bodyTop, innerWidth, bodyHeight);
    placeChild(m_captions, innerLeft, bodyTop, m_captionWidth, bodyHeight);
}

void LaneView::computeTicks()
{
    const double min=getXMin();
    const double max=getXMax();

    m_ticks.clear();
    m_tickRects.clear();
    if(!(max > min) || m_area.w <= 0)
        return;

    std::vector<axis::Tick> ticks=chooseTicks(min, max, m_area.w);
    std::vector<double> positions;
    std::vector<double> widths;

    positions.reserve(ticks.size());
    widths.reserve(ticks.size());
    for(const axis::Tick &tick : ticks)
    {
        positions.push_back(tick.position);
        widths.push_back(labelWidth(tick.label));
    }

    // every stride-th label, so none overlaps the next
    const size_t stride=axis::thinLabels(positions, widths, XLabelGap);

    for(size_t i=0; i < ticks.size(); i+=stride)
    {
        ticks[i].position+=m_area.x;
        m_ticks.push_back(std::move(ticks[i]));
    }

    // each label centred on its tick inside the control
    const int lineHeight=labelLineHeight();
    const Padding &padding=getPadding();
    const int innerLeft=padding.left;
    const int innerRight=std::max(innerLeft, getWidth()-padding.right);

    m_tickRects.reserve(m_ticks.size());
    for(const axis::Tick &tick : m_ticks)
    {
        const int width=labelWidth(tick.label);
        int x=static_cast<int>(std::lround(tick.position))-width/2;

        x=std::max(innerLeft, std::min(x, innerRight-width));
        m_tickRects.emplace_back(x, m_axisTop, width, lineHeight);
    }
}

void LaneView::placeAxis()
{
    // the labels are children laid out next: what is written to them is
    // placement
    PlacementScope placement;

    // the body's viewport right of the caption column
    const int captionWidth=std::min(m_captionWidth, m_viewport.w);

    m_area=Rect(m_viewport.x+captionWidth, m_viewport.y, m_viewport.w-captionWidth, m_viewport.h);

    computeTicks();

    if(!tickLabelsAreControls())
    {
        laneContentMoved();
        return;
    }

    // the labels centred on their ticks inside the control
    while(m_tickLabels.size() > m_ticks.size())
    {
        delete m_tickLabels.back();   // ~Base takes it out of the children
        m_tickLabels.pop_back();
    }
    while(m_tickLabels.size() < m_ticks.size())
        m_tickLabels.push_back(makeLabel(this, getSkin()->Colors.Timeline.AxisText));

    for(size_t t=0; t < m_ticks.size(); ++t)
    {
        const Rect &rect=m_tickRects[t];

        m_tickLabels[t]->setText(m_ticks[t].label);
        placeChild(m_tickLabels[t], rect.x, rect.y, rect.w, rect.h);
    }

    // the renderer draws from what this computed, and what the lanes hold
    // may have moved under the mouse
    laneContentMoved();
}

// The captions of the lanes in view, in a pool of rows as many as the most
// lanes the column can show any of (VirtualList's): a row whose lane is still
// in view keeps it, the lanes that came into view go to the free rows (their
// text set, which writes their glyphs), and a row with no lane is hidden. A
// new pool size (a new column height or lane count) deletes the free rows
// before the pool is topped up, so a timeline resized to a height shows the
// rows one built at it does. Each row is centred on its lane less the scroll
// offset, so a scroll moves the captions in view alone.
void LaneView::placeCaptions(skin::Base *skin)
{
    PlacementScope placement;

    const int lineHeight=labelLineHeight();
    const int width=m_captions->getWidth();
    const int height=m_captions->getHeight();
    const int labelWidth=std::max(0, width-2*CaptionPadding);
    const size_t lanes=m_laneCaptions.size();
    size_t first=0;
    size_t end=0;
    size_t poolSize=0;

    if(width > 0 && height > 0 && lanes > 0)
    {
        const int64_t offset=m_scrollOffset;

        first=std::min(lanes, static_cast<size_t>(offset/m_laneHeight));
        end=std::min(lanes, static_cast<size_t>((offset+height+m_laneHeight-1)/m_laneHeight));
        poolSize=std::min(lanes, static_cast<size_t>((height+m_laneHeight-1)/m_laneHeight)+1);
    }

    std::vector<bool> held(end-first, false);

    for(CaptionRow &row : m_captionRows)
    {
        if(row.lane != NoLane && row.lane >= first && row.lane < end && !held[row.lane-first])
            held[row.lane-first]=true;
        else
            row.lane=NoLane;
    }

    if(m_captionRows.size() != poolSize)
    {
        std::vector<CaptionRow> kept;

        for(CaptionRow &row : m_captionRows)
        {
            if(row.lane == NoLane)
                delete row.label;   // ~Base takes it out of the column's children
            else
                kept.push_back(row);
        }
        m_captionRows.swap(kept);
        while(m_captionRows.size() < poolSize)
        {
            CaptionRow row;

            row.label=makeLabel(m_captions, getSkin()->Colors.Timeline.CaptionText);
            row.label->setHidden(true);
            m_captionRows.push_back(row);
        }
    }

    // the lanes that came into view, to the free rows in order
    size_t freeRow=0;

    for(size_t lane=first; lane < end; ++lane)
    {
        if(held[lane-first])
            continue;
        while(freeRow < m_captionRows.size() && m_captionRows[freeRow].lane != NoLane)
            ++freeRow;
        if(freeRow == m_captionRows.size())
            break;
        m_captionRows[freeRow].lane=lane;
    }

    for(CaptionRow &row : m_captionRows)
    {
        internal::Text *label=row.label;

        if(row.lane == NoLane)
        {
            label->setHidden(true);
            continue;
        }

        const bool wasHidden=label->hidden();
        const bool rebound=(label->getText() != m_laneCaptions[row.lane]);
        const int64_t top=static_cast<int64_t>(row.lane)*m_laneHeight-m_scrollOffset;

        label->setText(m_laneCaptions[row.lane]);
        label->setHidden(false);
        if(wasHidden || rebound)
            label->doCalculateSize(skin, Dim::X);
        placeChild(label, CaptionPadding, static_cast<int>(top)+(m_laneHeight-lineHeight)/2, labelWidth, lineHeight);
    }
}

// The lane area is the body's viewport less the caption column, and the ticks
// need its width: the body is laid out first (all of it is in its Y arrange,
// as a ScrollControl's), then the axis and the captions, then the rest, X then
// Y, as a Chart lays out its labels (docs/layout.md, "Children whose X layout
// needs the parent's height").
void LaneView::arrange(skin::Base *skin, Dim dim)
{
    if(dim == Dim::X)
        return;

    stampLayoutPass();

    const bool relayout=needsLayout();

    if(relayout)
    {
        m_needsLayout=false;
        layout(skin);
    }

    m_body->doCalculateSize(skin, Dim::X);
    m_body->doArrange(skin, Dim::X);
    m_body->doCalculateSize(skin, Dim::Y);
    m_body->doArrange(skin, Dim::Y);

    const Rect &viewport=m_body->getViewport();
    const Rect bodyViewport(m_body->getX()+viewport.x, m_body->getY()+viewport.y, viewport.w, viewport.h);

    if(relayout || !(bodyViewport == m_viewport))
    {
        m_viewport=bodyViewport;
        placeAxis();
    }
    placeCaptions(skin);

    countLayoutChildrenScanned(m_children);
    for(auto &&child : m_children)
    {
        if(child == m_body || child->hidden())
            continue;
        // a label made by placeAxis() was not measured this pass
        child->doCalculateSize(skin, Dim::X);
        child->doArrange(skin, Dim::X);
    }

    countLayoutChildrenScanned(m_children);
    for(auto &&child : m_children)
    {
        if(child != m_body && !child->hidden())
            child->doCalculateSize(skin, Dim::Y);
    }

    countLayoutChildrenScanned(m_children);
    for(auto &&child : m_children)
    {
        if(child != m_body && !child->hidden())
            child->doArrange(skin, Dim::Y);
    }

    postLayout(skin);
}

}//namespace controls
}//namespace gweni
