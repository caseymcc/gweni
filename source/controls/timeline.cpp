/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/timeline.h>
#include <gweni/gweni.h>
#include <gweni/controls/text.h>
#include <gweni/skin.h>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace gweni
{
namespace controls
{

namespace
{

bool finiteBar(const TimelineBar &bar)
{
    return std::isfinite(bar.start) && std::isfinite(bar.end);
}

const TimelineBar NoBar;

}//namespace

// ---- the body and the caption column --------------------------------------

namespace internal
{

GWENI_CONTROL_CONSTRUCTOR(TimelineBody)
{
    // the lanes scroll up and down, never sideways, through virtual content:
    // nothing is ever placed far outside the viewport
    setScroll(false, true);
    setAutoHideBars(true);
    setVirtualContentHeight(0);
    m_innerPanel->setMargin(Margin(0, 0, 0, 0));
    // the timeline takes the mouse over its lanes: only the bar is the body's
    m_innerPanel->setMouseInputEnabled(false);
}

void TimelineBody::contentsPlaced(skin::Base * /*skin*/)
{
    if(m_timeline)
        m_timeline->bodyScrolled(getVerticalScrollOffset());
}

GWENI_CONTROL_CONSTRUCTOR(TimelineCaptionColumn)
{
    setMouseInputEnabled(false);
}

// The captions are placed by the Timeline (placeCaptions(), in its Y arrange);
// this lays out the shown ones, X then Y, once both are final.
void TimelineCaptionColumn::arrange(skin::Base *skin, Dim dim)
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

// ---- the timeline -----------------------------------------------------------

GWENI_CONTROL_CONSTRUCTOR(Timeline)
{
    setBounds(gweni::Rect(0, 0, 480, 240));
    // inside the frame's 1-unit border, with room clear of it
    setPadding(Padding(4, 4, 4, 4));
    m_utcOffset=axis::localUtcOffset();

    m_body=newChild<internal::TimelineBody>();
    m_body->m_timeline=this;
    m_body->setPlacement(Placement::Owner);

    m_captions=newChild<internal::TimelineCaptionColumn>();
    m_captions->setPlacement(Placement::Owner);
}

// the labels, the body and the caption column are children: ~Base deletes them
Timeline::~Timeline()
{
}

// ---- lanes ------------------------------------------------------------------

size_t Timeline::addLane(const String &caption)
{
    Lane lane;

    lane.caption=caption;
    lane.label=makeLabel(m_captions, getSkin()->Colors.Timeline.CaptionText);
    lane.label->setText(caption);
    // shown when its lane is first in view (placeCaptions)
    lane.label->setHidden(true);
    m_lanes.push_back(std::move(lane));
    lanesChanged();
    return m_lanes.size()-1;
}

void Timeline::removeLane(size_t lane)
{
    if(lane >= m_lanes.size())
        return;

    m_barCount-=m_lanes[lane].bars.size();
    delete m_lanes[lane].label;   // ~Base takes it out of the column's children
    m_lanes.erase(m_lanes.begin()+static_cast<std::ptrdiff_t>(lane));
    m_rangeDirty=true;
    lanesChanged();
}

void Timeline::clearLanes()
{
    if(m_lanes.empty())
        return;

    for(Lane &lane : m_lanes)
        delete lane.label;
    m_lanes.clear();
    m_barCount=0;
    m_rangeDirty=true;
    lanesChanged();
}

void Timeline::setLaneCaption(size_t lane, const String &caption)
{
    if(lane >= m_lanes.size() || m_lanes[lane].caption == caption)
        return;

    m_lanes[lane].caption=caption;
    m_lanes[lane].label->setText(caption);
    // an automatic column follows the widest caption
    if(m_captionWidthSetting <= 0)
        invalidate();
}

String Timeline::getLaneCaption(size_t lane) const
{
    const Lane *found=laneAtIndex(lane);

    return found ? found->caption : String();
}

internal::Text *Timeline::getCaptionLabel(size_t lane) const
{
    const Lane *found=laneAtIndex(lane);

    return found ? found->label : nullptr;
}

void Timeline::setLaneHeight(int height)
{
    height=std::max(height, 2*BarInset+1);
    if(height == m_laneHeight)
        return;

    m_laneHeight=height;
    lanesChanged();
}

void Timeline::setCaptionColumnWidth(int width)
{
    width=std::max(width, 0);
    if(width == m_captionWidthSetting)
        return;

    m_captionWidthSetting=width;
    invalidate();
}

void Timeline::lanesChanged()
{
    // the virtual height, the caption column and the captions in view
    invalidate();
    addStateChange(StateChange_Background);
}

// ---- bars -------------------------------------------------------------------

size_t Timeline::addBar(size_t lane, TimelineBar bar)
{
    if(lane >= m_lanes.size())
        return static_cast<size_t>(-1);

    if(bar.end < bar.start)
        std::swap(bar.start, bar.end);

    Lane &target=m_lanes[lane];

    target.bars.push_back(std::move(bar));
    ++m_barCount;
    barsChanged(target, nullptr, &target.bars.back());
    return target.bars.size()-1;
}

void Timeline::setBar(size_t lane, size_t index, TimelineBar bar)
{
    if(lane >= m_lanes.size() || index >= m_lanes[lane].bars.size())
        return;

    if(bar.end < bar.start)
        std::swap(bar.start, bar.end);

    Lane &target=m_lanes[lane];
    TimelineBar &current=target.bars[index];

    if(current == bar)
        return;

    const TimelineBar before=current;

    current=std::move(bar);
    barsChanged(target, &before, &current);
}

void Timeline::removeBar(size_t lane, size_t index)
{
    if(lane >= m_lanes.size() || index >= m_lanes[lane].bars.size())
        return;

    Lane &target=m_lanes[lane];
    const TimelineBar before=target.bars[index];

    target.bars.erase(target.bars.begin()+static_cast<std::ptrdiff_t>(index));
    --m_barCount;
    barsChanged(target, &before, nullptr);
}

void Timeline::clearBars(size_t lane)
{
    if(lane >= m_lanes.size() || m_lanes[lane].bars.empty())
        return;

    Lane &target=m_lanes[lane];

    m_barCount-=target.bars.size();
    target.bars.clear();
    target.dirty=true;
    addStateChange(StateChange_Background);
    m_rangeDirty=true;
    if(!m_xFixed)
        invalidate();
}

size_t Timeline::getBarCount(size_t lane) const
{
    const Lane *found=laneAtIndex(lane);

    return found ? found->bars.size() : 0;
}

const TimelineBar &Timeline::getBar(size_t lane, size_t index) const
{
    const Lane *found=laneAtIndex(lane);

    if(!found || index >= found->bars.size())
        return NoBar;
    return found->bars[index];
}

int Timeline::getOverlapLevel(size_t lane, size_t index) const
{
    const Lane *found=laneAtIndex(lane);

    if(!found || index >= found->bars.size())
        return 0;

    ensureLane(*found);
    return found->levels[index];
}

void Timeline::barsChanged(Lane &lane, const TimelineBar *before, const TimelineBar *after)
{
    lane.dirty=true;
    // the renderer redraws what moved
    addStateChange(StateChange_Background);

    // a fixed range is not worked out: it is again once the range is
    // automatic
    if(m_xFixed)
    {
        m_rangeDirty=true;
        return;
    }

    // An automatic range, and the ticks and every bar's place from it, are the
    // layout's: a bar inside the range that did not hold one of its ends moves
    // neither.
    bool moves=m_rangeDirty;

    if(!moves && before && finiteBar(*before))
        moves=(before->start <= m_dataMin) || (before->end >= m_dataMax);
    if(!moves && after && finiteBar(*after))
        moves=!m_hasBars || (after->start < m_dataMin) || (after->end > m_dataMax);

    if(moves)
    {
        m_rangeDirty=true;
        invalidate();
    }
}

// Sorted by start among the lane's finite bars, with the running maximum of
// their ends; each bar's overlap level from the earlier bars it overlaps,
// found the hit test's way: the bars starting before its end, walked back
// while their ends reach past its start.
void Timeline::ensureLane(const Lane &lane) const
{
    if(!lane.dirty)
        return;

    lane.dirty=false;

    const std::vector<TimelineBar> &bars=lane.bars;

    lane.order.clear();
    for(size_t i=0; i < bars.size(); ++i)
    {
        if(finiteBar(bars[i]))
            lane.order.push_back(i);
    }
    std::stable_sort(lane.order.begin(), lane.order.end(),
        [&bars](size_t a, size_t b) { return bars[a].start < bars[b].start; });

    const size_t count=lane.order.size();

    lane.starts.resize(count);
    lane.maxEnds.resize(count);
    for(size_t i=0; i < count; ++i)
    {
        const TimelineBar &bar=bars[lane.order[i]];

        lane.starts[i]=bar.start;
        lane.maxEnds[i]=(i == 0) ? bar.end : std::max(lane.maxEnds[i-1], bar.end);
    }

    lane.levels.assign(bars.size(), 0);
    for(size_t i=0; i < bars.size(); ++i)
    {
        const TimelineBar &bar=bars[i];

        if(!finiteBar(bar))
            continue;

        size_t pos=static_cast<size_t>(std::lower_bound(lane.starts.begin(), lane.starts.end(), bar.end)
            -lane.starts.begin());
        int level=0;

        while(pos > 0 && lane.maxEnds[pos-1] > bar.start)
        {
            --pos;

            const size_t other=lane.order[pos];
            const TimelineBar &earlier=bars[other];

            if(other < i && earlier.end > bar.start && earlier.start < bar.end)
                level=std::max(level, std::min(MaxOverlapLevel, lane.levels[other]+1));
        }
        lane.levels[i]=level;
    }
}

// ---- the x axis -------------------------------------------------------------

void Timeline::setXRange(double min, double max)
{
    if(!(max > min))
        max=min+1.0;

    if(m_xFixed && m_xFixedMin == min && m_xFixedMax == max)
        return;

    m_xFixed=true;
    m_xFixedMin=min;
    m_xFixedMax=max;
    invalidate();
    addStateChange(StateChange_Background);
}

void Timeline::setXAutoRange()
{
    if(!m_xFixed)
        return;

    m_xFixed=false;
    m_rangeDirty=true;
    invalidate();
    addStateChange(StateChange_Background);
}

void Timeline::ensureRange() const
{
    if(!m_rangeDirty)
        return;

    m_rangeDirty=false;
    m_hasBars=false;
    m_dataMin=0.0;
    m_dataMax=1.0;

    for(const Lane &lane : m_lanes)
    {
        for(const TimelineBar &bar : lane.bars)
        {
            if(!finiteBar(bar))
                continue;

            if(!m_hasBars)
            {
                m_dataMin=bar.start;
                m_dataMax=bar.end;
                m_hasBars=true;
                continue;
            }
            m_dataMin=std::min(m_dataMin, bar.start);
            m_dataMax=std::max(m_dataMax, bar.end);
        }
    }
}

double Timeline::getXMin() const
{
    if(m_xFixed)
        return m_xFixedMin;

    ensureRange();
    // a single instant gets a second round it
    return (m_dataMax > m_dataMin) ? m_dataMin : m_dataMin-0.5;
}

double Timeline::getXMax() const
{
    if(m_xFixed)
        return m_xFixedMax;

    ensureRange();
    return (m_dataMax > m_dataMin) ? m_dataMax : m_dataMax+0.5;
}

void Timeline::setUtcOffset(const axis::UtcOffset &offset)
{
    m_utcOffset=offset;
    invalidate();
}

// ---- scrolling --------------------------------------------------------------

int Timeline::getScrollOffset() const
{
    return m_body->getVerticalScrollOffset();
}

void Timeline::setScrollOffset(int offset)
{
    m_body->setVerticalScrollOffset(std::max(offset, 0));
}

void Timeline::scrollToLane(size_t lane)
{
    if(lane >= m_lanes.size())
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
bool Timeline::onMouseWheeled(int delta)
{
    const int before=getScrollOffset();
    const long moved=std::lround(-static_cast<double>(delta)*WheelLanes*m_laneHeight/60.0);

    setScrollOffset(static_cast<int>(std::clamp<long>(before+moved, 0, INT32_MAX)));
    if(getScrollOffset() != before)
        return true;

    return ParentClass::onMouseWheeled(delta);
}

void Timeline::bodyScrolled(int offset)
{
    if(offset == m_scrollOffset)
        return;

    m_scrollOffset=offset;
    // every bar in view moved
    addStateChange(StateChange_Background);
}

// ---- hit test ---------------------------------------------------------------

int Timeline::laneAt(int y) const
{
    if(y < m_area.y || y >= m_area.y+m_area.h)
        return -1;

    const int64_t lane=(static_cast<int64_t>(y)-m_area.y+m_scrollOffset)/m_laneHeight;

    if(lane < 0 || lane >= static_cast<int64_t>(m_lanes.size()))
        return -1;
    return static_cast<int>(lane);
}

// A bar holds the unit x when its left edge, rounded, is at or before x and its
// right edge after it: so its start is before the time at x+0.5 (taken as
// x+1), and its end after the time at x-1 (its end after the time at x, or a
// unit-wide bar starting at x-0.5 or later). The candidates are the bars
// starting before x+1, walked back while the running maximum of their ends
// reaches x-1; each is then held to its rect exactly.
bool Timeline::barAt(const Point &local, size_t &lane, size_t &index) const
{
    const int laneIndex=laneAt(local.y);

    if(laneIndex < 0 || local.x < m_area.x || local.x >= m_area.x+m_area.w)
        return false;

    const Lane &target=m_lanes[static_cast<size_t>(laneIndex)];

    ensureLane(target);

    const double low=timeAt(local.x-1.0);
    const double high=timeAt(local.x+1.0);
    size_t pos=static_cast<size_t>(std::lower_bound(target.starts.begin(), target.starts.end(), high)
        -target.starts.begin());
    bool found=false;
    size_t best=0;

    while(pos > 0 && target.maxEnds[pos-1] >= low)
    {
        --pos;

        const size_t candidate=target.order[pos];
        Rect rect;

        if(!barRect(static_cast<size_t>(laneIndex), target.bars[candidate], rect)
            || local.x < rect.x || local.x >= rect.x+rect.w)
            continue;

        // the one drawn on top: the highest level, then the later
        if(!found || target.levels[candidate] > target.levels[best]
            || (target.levels[candidate] == target.levels[best] && candidate > best))
        {
            best=candidate;
            found=true;
        }
    }

    if(!found)
        return false;

    lane=static_cast<size_t>(laneIndex);
    index=best;
    return true;
}

// ---- geometry ---------------------------------------------------------------

Rect Timeline::getCaptionColumnBounds() const
{
    return Rect(m_viewport.x, m_area.y, m_captionWidth, m_area.h);
}

double Timeline::xPosition(double t) const
{
    return m_area.x+axis::positionOf(t, getXMin(), getXMax(), m_area.w);
}

double Timeline::timeAt(double x) const
{
    const double min=getXMin();
    const double max=getXMax();

    if(m_area.w <= 0)
        return min;
    return min+(x-m_area.x)/m_area.w*(max-min);
}

bool Timeline::getLaneRect(size_t lane, Rect &rect) const
{
    if(lane >= m_lanes.size() || m_area.w <= 0 || m_area.h <= 0)
        return false;

    const int64_t top=static_cast<int64_t>(m_area.y)+static_cast<int64_t>(lane)*m_laneHeight-m_scrollOffset;

    if(top+m_laneHeight <= m_area.y || top >= m_area.y+m_area.h)
        return false;

    rect=Rect(m_area.x, static_cast<int>(top), m_area.w, m_laneHeight);
    return true;
}

bool Timeline::barRect(size_t laneIndex, const TimelineBar &bar, Rect &rect) const
{
    if(!finiteBar(bar) || m_area.w <= 0)
        return false;

    // held a little outside the area before rounding, so a bar too far out to
    // fit an int is cut by the area's clip as a near one is, never wrapped
    const double low=m_area.x-static_cast<double>(OffAreaMargin);
    const double high=m_area.x+m_area.w+static_cast<double>(OffAreaMargin);
    const int x0=static_cast<int>(std::lround(std::clamp(xPosition(bar.start), low, high)));
    int x1=static_cast<int>(std::lround(std::clamp(xPosition(bar.end), low, high)));

    // a bar too short to reach a unit still shows one
    if(x1 <= x0)
        x1=x0+1;

    const int64_t top=static_cast<int64_t>(m_area.y)+static_cast<int64_t>(laneIndex)*m_laneHeight
        -m_scrollOffset+BarInset;

    rect=Rect(x0, static_cast<int>(top), x1-x0, m_laneHeight-2*BarInset);
    return true;
}

bool Timeline::getBarRect(size_t lane, size_t index, Rect &rect) const
{
    const Lane *found=laneAtIndex(lane);

    if(!found || index >= found->bars.size())
        return false;

    // a lane far out of view is never placed: no int to overflow
    Rect laneRect;

    if(!getLaneRect(lane, laneRect))
        return false;

    Rect bar;

    if(!barRect(lane, found->bars[index], bar))
        return false;

    if(bar.x+bar.w <= m_area.x || bar.x >= m_area.x+m_area.w
        || bar.y+bar.h <= m_area.y || bar.y >= m_area.y+m_area.h)
        return false;

    rect=bar;
    return true;
}

// ---- measures ---------------------------------------------------------------

void Timeline::renewMeasureKey()
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

int Timeline::labelWidth(const String &text)
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

int Timeline::labelLineHeight()
{
    if(m_lineHeight < 0)
        m_lineHeight=measureLineHeight(getSkin()->getDefaultFont());
    return m_lineHeight;
}

// ---- layout -----------------------------------------------------------------

internal::Text *Timeline::makeLabel(Base *parent, const Color &color)
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

void Timeline::updateColors()
{
    const Color &axisText=getSkin()->Colors.Timeline.AxisText;
    const Color &captionText=getSkin()->Colors.Timeline.CaptionText;

    for(internal::Text *label : m_tickLabels)
        label->setTextColor(axisText);
    for(Lane &lane : m_lanes)
        lane.label->setTextColor(captionText);
}

// The body and the caption column, which need nothing of the body's layout:
// the bar area and the ticks, which need its viewport, are placeAxis()'s.
void Timeline::layout(skin::Base *skin)
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
        for(const Lane &lane : m_lanes)
            captionWidth=std::max(captionWidth, labelWidth(lane.caption)+2*CaptionPadding);
        captionWidth=std::min(captionWidth, innerWidth/3);
    }
    m_captionWidth=std::min(captionWidth, innerWidth);

    // the axis's labels above the lanes
    m_axisTop=innerTop;

    const int bodyTop=std::min(innerBottom, innerTop+lineHeight+LabelGap);
    const int bodyHeight=innerBottom-bodyTop;

    m_body->setVirtualContentHeight(static_cast<int>(std::min<int64_t>(
        static_cast<int64_t>(m_lanes.size())*m_laneHeight, INT32_MAX)));
    placeChild(m_body, innerLeft, bodyTop, innerWidth, bodyHeight);
    placeChild(m_captions, innerLeft, bodyTop, m_captionWidth, bodyHeight);
}

void Timeline::placeAxis()
{
    // the labels are children laid out next: what is written to them is
    // placement
    PlacementScope placement;

    const int lineHeight=labelLineHeight();
    const Padding &padding=getPadding();
    const int innerLeft=padding.left;
    const int innerRight=std::max(innerLeft, getWidth()-padding.right);

    // the body's viewport right of the caption column
    const int captionWidth=std::min(m_captionWidth, m_viewport.w);

    m_area=Rect(m_viewport.x+captionWidth, m_viewport.y, m_viewport.w-captionWidth, m_viewport.h);

    const double min=getXMin();
    const double max=getXMax();

    m_ticks.clear();
    if(max > min && m_area.w > 0)
    {
        axis::TimeTicks time=axis::chooseTimeTicks(min, max, m_area.w,
            [this](const String &label) { return double(labelWidth(label)); }, XLabelGap, m_utcOffset);
        std::vector<double> positions;
        std::vector<double> widths;

        m_timeStep=time.step;
        positions.reserve(time.ticks.size());
        widths.reserve(time.ticks.size());
        for(const axis::Tick &tick : time.ticks)
        {
            positions.push_back(tick.position);
            widths.push_back(labelWidth(tick.label));
        }

        // every stride-th label, so none overlaps the next
        const size_t stride=axis::thinLabels(positions, widths, XLabelGap);

        for(size_t i=0; i < time.ticks.size(); i+=stride)
        {
            time.ticks[i].position+=m_area.x;
            m_ticks.push_back(std::move(time.ticks[i]));
        }
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
        internal::Text *label=m_tickLabels[t];
        const axis::Tick &tick=m_ticks[t];
        const int width=labelWidth(tick.label);
        int x=static_cast<int>(std::lround(tick.position))-width/2;

        x=std::max(innerLeft, std::min(x, innerRight-width));
        label->setText(tick.label);
        placeChild(label, x, m_axisTop, width, lineHeight);
    }

    // the renderer draws from what this computed
    addStateChange(StateChange_Background);
}

// Each lane's caption in the column at its lane less the scroll offset, centred
// on it, while any of the lane is in view; hidden otherwise, so a scroll moves
// the captions in view alone.
void Timeline::placeCaptions(skin::Base *skin)
{
    PlacementScope placement;

    const int lineHeight=labelLineHeight();
    const int width=m_captions->getWidth();
    const int height=m_captions->getHeight();
    const int labelWidth=std::max(0, width-2*CaptionPadding);

    for(size_t i=0; i < m_lanes.size(); ++i)
    {
        internal::Text *label=m_lanes[i].label;
        const int64_t top=static_cast<int64_t>(i)*m_laneHeight-m_scrollOffset;

        if(width <= 0 || top+m_laneHeight <= 0 || top >= height)
        {
            label->setHidden(true);
            continue;
        }

        if(label->hidden())
        {
            label->setHidden(false);
            label->doCalculateSize(skin, Dim::X);
        }
        placeChild(label, CaptionPadding, static_cast<int>(top)+(m_laneHeight-lineHeight)/2, labelWidth, lineHeight);
    }
}

// The bar area is the body's viewport less the caption column, and the ticks
// need its width: the body is laid out first (all of it is in its Y arrange,
// as a ScrollControl's), then the axis and the captions, then the rest, X then
// Y, as a Chart lays out its labels (docs/layout.md, "Children whose X layout
// needs the parent's height").
void Timeline::arrange(skin::Base *skin, Dim dim)
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
