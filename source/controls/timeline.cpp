/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/timeline.h>
#include <gweni/gweni.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/text.h>
#include <gweni/skin.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

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

// ---- the timeline -----------------------------------------------------------

// the bounds, the padding, the body and the caption column are LaneView's
GWENI_CONTROL_CONSTRUCTOR(Timeline)
{
    m_utcOffset=axis::localUtcOffset();
}

// the labels, the body and the caption column are children: ~Base deletes them
Timeline::~Timeline()
{
}

// ---- lanes ------------------------------------------------------------------

size_t Timeline::addLane(const String &caption)
{
    m_lanes.emplace_back();
    appendLane(caption);
    return m_lanes.size()-1;
}

void Timeline::removeLane(size_t lane)
{
    if(lane >= m_lanes.size())
        return;

    m_barCount-=m_lanes[lane].bars.size();
    m_lanes.erase(m_lanes.begin()+static_cast<std::ptrdiff_t>(lane));
    m_rangeDirty=true;
    eraseLane(lane);
}

void Timeline::clearLanes()
{
    if(m_lanes.empty())
        return;

    m_lanes.clear();
    m_barCount=0;
    m_rangeDirty=true;
    eraseLanes();
}

void Timeline::setLaneCaption(size_t lane, const String &caption)
{
    renameLane(lane, caption);
}

void Timeline::laneCaptionChanged(size_t lane)
{
    // a shown tooltip of this lane's (the default one names it) set again
    if(m_hovering && m_hoverLane == lane)
        setHover(true, m_hoverLane, m_hoverIndex, true);
}

void Timeline::lanesChanged()
{
    // the virtual height, the caption column and the captions in view
    invalidate();
    barsMoved();
    refreshHover(true);
}

void Timeline::laneContentMoved()
{
    // every bar in view moved, and the one under the mouse may be another
    barsMoved();
    refreshHover();
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
    target.index.dirty=true;
    barsMoved();
    m_rangeDirty=true;
    if(!m_xFixed)
        invalidate();
    refreshHover(true);
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
    return found->index.levels[index];
}

void Timeline::barsChanged(Lane &lane, const TimelineBar *before, const TimelineBar *after)
{
    lane.index.dirty=true;
    // the renderer redraws what moved
    barsMoved();
    // what the mouse is over may be another bar now, or the same one with
    // other times
    refreshHover(true);

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

// The lane's finite bars searched by start, with their overlap levels
// (LaneBarIndex).
void Timeline::ensureLane(const Lane &lane) const
{
    if(!lane.index.dirty)
        return;

    const std::vector<TimelineBar> &bars=lane.bars;

    lane.index.build(bars.size(), [&bars](size_t i, double &start, double &end)
    {
        start=bars[i].start;
        end=bars[i].end;
        return finiteBar(bars[i]);
    }, MaxOverlapLevel);
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
    barsMoved();
}

void Timeline::setXAutoRange()
{
    if(!m_xFixed)
        return;

    m_xFixed=false;
    m_rangeDirty=true;
    invalidate();
    barsMoved();
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
    // a shown tooltip set again (the default one gives the times in the zone)
    if(m_hovering)
        setHover(true, m_hoverLane, m_hoverIndex, true);
}

// ---- hit test ---------------------------------------------------------------

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
    const size_t laneNumber=static_cast<size_t>(laneIndex);
    size_t best=0;

    // the one drawn on top: the highest level, then, sharing a layer (past
    // MaxOverlapLevel, or a column two bars both reach), the one whose slot is
    // drawn first, the earlier, which the depth test keeps
    if(!target.index.pick(low, high, [&](size_t candidate)
        {
            Rect rect;

            return barRect(laneNumber, target.bars[candidate], rect) && local.x >= rect.x
                && local.x < rect.x+rect.w;
        }, best))
        return false;

    lane=static_cast<size_t>(laneIndex);
    index=best;
    return true;
}

// ---- hover and click --------------------------------------------------------

void Timeline::barsMoved()
{
    ++m_barsSerial;
    addStateChange(StateChange_Background);
}

bool Timeline::getHoverRect(Rect &rect) const
{
    return m_hovering && getBarRect(m_hoverLane, m_hoverIndex, rect);
}

void Timeline::setHoverText(const HoverText &text)
{
    m_hoverText=text;
    if(m_hovering && !m_hoverControl)
        setToolTipText(m_hoverText ? m_hoverText(*this, m_hoverLane, m_hoverIndex)
            : defaultHoverText(m_hoverLane, m_hoverIndex));
}

void Timeline::setHoverControl(Base *control, const HoverFill &fill)
{
    if(m_hoverControl && m_hoverControl != control)
    {
        if(getToolTip() == m_hoverControl)
            setToolTip(nullptr);
        delete m_hoverControl;
    }

    m_hoverControl=control;
    m_hoverFill=fill;
    if(control)
    {
        // kept as the tooltip's control is, a hidden child
        control->setParent(this);
        control->setHidden(true);
    }

    // shown as the next hover asks
    const bool hovering=m_hovering;

    m_hovering=false;
    setHover(hovering, m_hoverLane, m_hoverIndex);
}

String Timeline::defaultHoverText(size_t lane, size_t index) const
{
    const TimelineBar &bar=getBar(lane, index);
    String text=getLaneCaption(lane);

    if(!text.empty())
        text+="  ";
    text+=axis::formatDateTime(bar.start, m_utcOffset)+" - "+axis::formatDateTime(bar.end, m_utcOffset);
    return text;
}

void Timeline::setHover(bool hovering, size_t lane, size_t index, bool refill)
{
    if(!hovering)
    {
        lane=0;
        index=0;
    }

    if(hovering == m_hovering && lane == m_hoverLane && index == m_hoverIndex && !(hovering && refill))
        return;

    // the hover look's slot alone: the bars keep theirs
    if(hovering != m_hovering || lane != m_hoverLane || index != m_hoverIndex)
        addStateChange(StateChange_Background);

    m_hovering=hovering;
    m_hoverLane=lane;
    m_hoverIndex=index;

    if(!hovering)
    {
        setToolTipText(String());
        if(m_hoverControl && getToolTip() == m_hoverControl)
            setToolTip(nullptr);
        return;
    }

    if(m_hoverControl)
    {
        if(m_hoverFill)
            m_hoverFill(*this, lane, index, m_hoverControl);
        if(getToolTip() != m_hoverControl)
            setToolTip(m_hoverControl);
        else if(Canvas *canvas=getCanvas())
            canvas->toolTipChanged(this);
        return;
    }
    setToolTipText(m_hoverText ? m_hoverText(*this, lane, index) : defaultHoverText(lane, index));
}

void Timeline::refreshHover(bool refill)
{
    if(!m_mouseInside)
        return;

    size_t lane=0;
    size_t index=0;
    const bool hovering=barAt(m_mouse, lane, index);

    setHover(hovering, lane, index, refill);
}

Rect Timeline::getToolTipAnchor() const
{
    Rect bar;

    if(!getHoverRect(bar))
        return ParentClass::getToolTipAnchor();

    // the bar as it shows, cut to the area: the box goes below it
    const int x0=std::max(bar.x, m_area.x);
    const int y0=std::max(bar.y, m_area.y);
    const int x1=std::min(bar.x+bar.w, m_area.x+m_area.w);
    const int y1=std::min(bar.y+bar.h, m_area.y+m_area.h);
    Timeline *self=const_cast<Timeline *>(this);
    const Point at=self->localPosToCanvas(Point(x0, y0));
    const Point end=self->localPosToCanvas(Point(std::max(x0, x1), std::max(y0, y1)));

    return Rect(at.x, at.y, end.x-at.x, end.y-at.y);
}

void Timeline::onMouseMoved(int x, int y, int deltaX, int deltaY)
{
    m_mouse=canvasPosToLocal(Point(x, y));
    m_mouseInside=true;
    refreshHover();
    ParentClass::onMouseMoved(x, y, deltaX, deltaY);
}

void Timeline::onMouseLeave()
{
    m_mouseInside=false;
    setHover(false, 0, 0);
    ParentClass::onMouseLeave();
}

void Timeline::onMouseClickLeft(int x, int y, bool down)
{
    const Point local=canvasPosToLocal(Point(x, y));
    size_t lane=0;
    size_t index=0;

    if(down)
    {
        m_pressed=barAt(local, lane, index);
        m_pressLane=lane;
        m_pressIndex=index;
        m_pressId=m_pressed ? getBar(lane, index).id : 0;
        // the release comes here wherever the mouse goes
        if(m_pressed)
            MouseFocus=this;
        return;
    }

    // the press's capture ends with its release, even when an edit
    // meanwhile has dropped the press
    if(MouseFocus == this)
        MouseFocus=nullptr;
    if(!m_pressed)
        return;

    m_pressed=false;

    // the same bar: its place and its id, so an edit between the press and
    // the release that moved another bar there clicks nothing
    if(!barAt(local, lane, index) || lane != m_pressLane || index != m_pressIndex
        || getBar(lane, index).id != m_pressId)
        return;

    m_clickedLane=lane;
    m_clickedIndex=index;
    m_clickedId=m_pressId;

    event::Info info(this);

    info.point=Point(static_cast<int>(lane), static_cast<int>(index));
    info.integer=static_cast<size_t>(m_pressId);
    onBarClicked.call(this, info);
}

// ---- geometry ---------------------------------------------------------------

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

// ---- the axis -----------------------------------------------------------------

std::vector<axis::Tick> Timeline::chooseTicks(double min, double max, int width)
{
    axis::TimeTicks time=axis::chooseTimeTicks(min, max, width,
        [this](const String &label) { return double(labelWidth(label)); }, XLabelGap, m_utcOffset);

    m_timeStep=time.step;
    return std::move(time.ticks);
}

}//namespace controls
}//namespace gweni
