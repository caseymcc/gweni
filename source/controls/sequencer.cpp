/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/sequencer.h>
#include <gweni/gweni.h>
#include <gweni/inputHandler.h>
#include <gweni/controls/canvas.h>
#include <gweni/skin.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace gweni
{
namespace controls
{

namespace
{

const SequencerClip NoClip;
const SequencerKey NoKey;

bool finite(double a, double b)
{
    return std::isfinite(a) && std::isfinite(b);
}

bool contains(const Rect &rect, const Point &p)
{
    return p.x >= rect.x && p.x < rect.x+rect.w && p.y >= rect.y && p.y < rect.y+rect.h;
}

bool intersects(const Rect &a, const Rect &b)
{
    return a.x < b.x+b.w && b.x < a.x+a.w && a.y < b.y+b.h && b.y < a.y+a.h;
}

// getSelection()'s order, for a binary search of a selection.
bool itemLess(const SequencerItem &a, const SequencerItem &b)
{
    return SequencerItemOrder()(a, b);
}

bool holds(const std::vector<SequencerItem> &sorted, const SequencerItem &item)
{
    return std::binary_search(sorted.begin(), sorted.end(), item, itemLess);
}

}//namespace

// A middle drag pans the view by the mouse's motion, with the canvas's input
// until the button is let go.
class Sequencer::PanGesture: public InputCapture
{
public:

    explicit PanGesture(Sequencer *sequencer): m_sequencer(sequencer) {}
    ~PanGesture() override { end(); }

    void start(Canvas *canvas)
    {
        m_canvas=canvas;
        m_lastX=input::getMousePosition().x;
    }

    bool active() const { return m_canvas && (m_canvas->inputCapture() == this); }

    void captureMouseMoved(int x, int /*y*/, int /*deltaX*/, int /*deltaY*/) override
    {
        if(!m_sequencer->visible())
        {
            end();
            return;
        }

        const Rect area=m_sequencer->getBarArea();
        const double span=m_sequencer->getViewEnd()-m_sequencer->getViewStart();

        if(area.w > 0 && x != m_lastX)
            m_sequencer->panBy(-(x-m_lastX)*span/area.w);
        m_lastX=x;
    }

    void captureMouseButton(int button, bool down) override
    {
        if(!down && button == 2)
            end();
    }

    void captureCanvasDeleted(Canvas * /*canvas*/) override { m_canvas=nullptr; }

    void end()
    {
        if(active())
            m_canvas->setInputCapture(nullptr);
        m_canvas=nullptr;
    }

private:

    Sequencer *m_sequencer;
    Canvas *m_canvas=nullptr;
    int m_lastX=0;
};

GWENI_CONTROL_CONSTRUCTOR(Sequencer)
{
    setMouseInputEnabled(true);
    setKeyboardInputEnabled(true);
    m_panGesture=std::make_unique<PanGesture>(this);
}

Sequencer::~Sequencer()
{
}

// ---- tracks -------------------------------------------------------------------

size_t Sequencer::addTrack(const String &caption, uint64_t id)
{
    Track track;

    track.id=id;
    m_tracks.push_back(std::move(track));
    appendLane(caption);
    return m_tracks.size()-1;
}

void Sequencer::removeTrack(size_t track)
{
    if(track >= m_tracks.size())
        return;

    Removal removal;

    removal.wholeTrack=true;
    removal.item.track=track;
    removing(removal);
    m_itemCount-=m_tracks[track].clips.size()+m_tracks[track].keys.size();
    m_tracks.erase(m_tracks.begin()+static_cast<std::ptrdiff_t>(track));
    eraseLane(track);
}

void Sequencer::clearTracks()
{
    if(m_tracks.empty())
        return;

    // nothing is left for a drag or a press to hold or put back
    m_drag=Drag::None;
    m_pressItemGone=true;
    m_dragStarted=false;
    m_originals.clear();
    m_pressSelection.clear();
    m_marqueeHits.clear();
    m_marquee=Rect();
    m_snapGuide=false;
    m_moved.clear();
    m_selection.clear();
    m_tracks.clear();
    m_itemCount=0;
    allItemsChanged();
    eraseLanes();
}

uint64_t Sequencer::getTrackId(size_t track) const
{
    const Track *found=trackAt(track);

    return found ? found->id : 0;
}

// ---- clips and keys -------------------------------------------------------

void Sequencer::noteChanged(const SequencerItem &item) const
{
    if(m_changedOverflow)
        return;
    if(m_changed.size() >= MaxChangedItems)
    {
        m_changed.clear();
        m_changedOverflow=true;
        return;
    }
    m_changed.push_back(item);
}

void Sequencer::itemChanged(const SequencerItem &item, bool search)
{
    if(search && item.track < m_tracks.size())
    {
        Track &track=m_tracks[item.track];

        if(item.kind == SequencerItem::Kind::Clip)
            track.clipIndex.dirty=true;
        else
            track.keyIndex.dirty=true;
    }
    noteChanged(item);
    addStateChange(StateChange_Background);
}

void Sequencer::allItemsChanged()
{
    m_changed.clear();
    m_changedOverflow=false;
    ++m_itemsSerial;
    addStateChange(StateChange_Background);
}

bool Sequencer::takeChangedItems(std::vector<SequencerItem> &items)
{
    // the searches first: a rebuilt one notes the clips whose level moved
    for(size_t t=0; t < m_tracks.size(); ++t)
    {
        if(m_tracks[t].clipIndex.dirty || m_tracks[t].keyIndex.dirty)
            ensureTrack(t);
    }

    const bool listed=!m_changedOverflow;

    items.clear();
    items.swap(m_changed);
    m_changedOverflow=false;
    return listed;
}

bool Sequencer::remap(SequencerItem &item, const Removal &removal)
{
    if(item.track != removal.item.track)
    {
        if(removal.wholeTrack && item.track > removal.item.track)
            --item.track;
        return true;
    }
    if(removal.wholeTrack)
        return false;
    if(item.kind != removal.item.kind)
        return true;
    if(item.index == removal.item.index)
        return false;
    if(item.index > removal.item.index)
        --item.index;
    return true;
}

void Sequencer::removing(const Removal &removal)
{
    auto remapList=[&removal](std::vector<SequencerItem> &items)
    {
        std::vector<SequencerItem> kept;

        kept.reserve(items.size());
        for(SequencerItem item : items)
        {
            if(remap(item, removal))
                kept.push_back(item);
        }
        items.swap(kept);
    };

    // a press's onSelectionChanged handler removing the pressed item: the
    // press drags nothing
    if(m_pressFiring && !m_pressItemGone && !remap(m_pressItem, removal))
        m_pressItemGone=true;

    if(m_drag == Drag::Move || m_drag == Drag::ResizeLeft || m_drag == Drag::ResizeRight)
    {
        bool held=false;

        for(const Original &o : m_originals)
        {
            SequencerItem item=o.item;

            held|=!remap(item, removal);
        }
        if(held)
        {
            // put back while the indices still hold: the app hears of no move
            endDrag(true);
            refreshHover();
        }
        else
        {
            for(Original &o : m_originals)
                remap(o.item, removal);
            remap(m_pressItem, removal);
        }
    }
    else if(m_drag == Drag::Marquee)
    {
        remapList(m_pressSelection);
        remapList(m_marqueeHits);
    }
    remapList(m_moved);
    if(m_hovering && !remap(m_hoverItem, removal))
        setHover(false, SequencerItem(), Part::None);

    // the selection: the items at or after what goes, in its track (or every
    // later track), taken out and put back moved
    SequencerItem from=removal.item;

    if(removal.wholeTrack)
    {
        from.kind=SequencerItem::Kind::Clip;
        from.index=0;
    }

    std::vector<SequencerItem> moved;

    for(auto it=m_selection.lower_bound(from); it != m_selection.end()
        && (removal.wholeTrack || (it->track == from.track && it->kind == from.kind));)
    {
        moved.push_back(*it);
        it=m_selection.erase(it);
    }
    for(SequencerItem item : moved)
    {
        if(remap(item, removal))
            m_selection.insert(item);
    }
    // a removal moves the indices after it: every slot drawn again
    allItemsChanged();
}

size_t Sequencer::addClip(size_t track, double start, double end, const String &surface, uint64_t id)
{
    if(track >= m_tracks.size())
        return static_cast<size_t>(-1);

    if(end < start)
        std::swap(start, end);

    SequencerClip clip;

    clip.start=start;
    clip.end=end;
    clip.surface=surface;
    clip.id=id;
    m_tracks[track].clips.push_back(std::move(clip));
    ++m_itemCount;
    itemChanged(SequencerItem(SequencerItem::Kind::Clip, track, m_tracks[track].clips.size()-1));
    hoverMayHaveChanged();
    return m_tracks[track].clips.size()-1;
}

void Sequencer::setClip(size_t track, size_t index, double start, double end)
{
    if(track >= m_tracks.size() || index >= m_tracks[track].clips.size())
        return;

    if(end < start)
        std::swap(start, end);

    SequencerClip &clip=m_tracks[track].clips[index];

    if(clip.start == start && clip.end == end)
        return;

    clip.start=start;
    clip.end=end;
    itemChanged(SequencerItem(SequencerItem::Kind::Clip, track, index));
    hoverMayHaveChanged();
}

void Sequencer::removeClip(size_t track, size_t index)
{
    if(track >= m_tracks.size() || index >= m_tracks[track].clips.size())
        return;

    Removal removal;

    removal.item=SequencerItem(SequencerItem::Kind::Clip, track, index);
    removing(removal);
    m_tracks[track].clips.erase(m_tracks[track].clips.begin()+static_cast<std::ptrdiff_t>(index));
    --m_itemCount;
    m_tracks[track].clipIndex.dirty=true;
    hoverMayHaveChanged();
}

size_t Sequencer::getClipCount(size_t track) const
{
    const Track *found=trackAt(track);

    return found ? found->clips.size() : 0;
}

const SequencerClip &Sequencer::getClip(size_t track, size_t index) const
{
    const Track *found=trackAt(track);

    if(!found || index >= found->clips.size())
        return NoClip;
    return found->clips[index];
}

int Sequencer::getOverlapLevel(size_t track, size_t index) const
{
    const Track *found=trackAt(track);

    if(!found || index >= found->clips.size())
        return 0;

    ensureTrack(track);
    return found->clipIndex.levels[index];
}

size_t Sequencer::addKey(size_t track, double time, uint64_t id)
{
    if(track >= m_tracks.size())
        return static_cast<size_t>(-1);

    SequencerKey key;

    key.time=time;
    key.id=id;
    m_tracks[track].keys.push_back(key);
    ++m_itemCount;
    itemChanged(SequencerItem(SequencerItem::Kind::Key, track, m_tracks[track].keys.size()-1));
    hoverMayHaveChanged();
    return m_tracks[track].keys.size()-1;
}

void Sequencer::setKey(size_t track, size_t index, double time)
{
    if(track >= m_tracks.size() || index >= m_tracks[track].keys.size())
        return;

    SequencerKey &key=m_tracks[track].keys[index];

    if(key.time == time)
        return;

    key.time=time;
    itemChanged(SequencerItem(SequencerItem::Kind::Key, track, index));
    hoverMayHaveChanged();
}

void Sequencer::removeKey(size_t track, size_t index)
{
    if(track >= m_tracks.size() || index >= m_tracks[track].keys.size())
        return;

    Removal removal;

    removal.item=SequencerItem(SequencerItem::Kind::Key, track, index);
    removing(removal);
    m_tracks[track].keys.erase(m_tracks[track].keys.begin()+static_cast<std::ptrdiff_t>(index));
    --m_itemCount;
    m_tracks[track].keyIndex.dirty=true;
    hoverMayHaveChanged();
}

size_t Sequencer::getKeyCount(size_t track) const
{
    const Track *found=trackAt(track);

    return found ? found->keys.size() : 0;
}

const SequencerKey &Sequencer::getKey(size_t track, size_t index) const
{
    const Track *found=trackAt(track);

    if(!found || index >= found->keys.size())
        return NoKey;
    return found->keys[index];
}

bool Sequencer::isValid(const SequencerItem &item) const
{
    const Track *found=trackAt(item.track);

    if(!found)
        return false;
    return item.index < (item.kind == SequencerItem::Kind::Clip ? found->clips.size() : found->keys.size());
}

uint64_t Sequencer::getItemId(const SequencerItem &item) const
{
    if(!isValid(item))
        return 0;
    return item.kind == SequencerItem::Kind::Clip ? m_tracks[item.track].clips[item.index].id
        : m_tracks[item.track].keys[item.index].id;
}

// The clips searched by start with their overlap levels, and the keys as
// instants, the later of two at one point found (pick()'s rule is the earlier
// at a level, so the keys' search keeps the highest index itself). A clip
// whose level the build changed is noted for the renderer.
void Sequencer::ensureTrack(size_t trackIndex) const
{
    const Track &track=m_tracks[trackIndex];

    if(track.clipIndex.dirty)
    {
        const std::vector<SequencerClip> &clips=track.clips;
        const std::vector<int> before=track.clipIndex.levels;

        ++m_searchBuilds;

        track.clipIndex.build(clips.size(), [&clips](size_t i, double &start, double &end)
        {
            start=clips[i].start;
            end=clips[i].end;
            return finite(start, end);
        }, MaxOverlapLevel);

        const std::vector<int> &after=track.clipIndex.levels;

        for(size_t i=0; i < after.size(); ++i)
        {
            if(i >= before.size() || before[i] != after[i])
                noteChanged(SequencerItem(SequencerItem::Kind::Clip, trackIndex, i));
        }
    }
    if(track.keyIndex.dirty)
    {
        const std::vector<SequencerKey> &keys=track.keys;

        ++m_searchBuilds;
        track.keyIndex.build(keys.size(), [&keys](size_t i, double &start, double &end)
        {
            start=keys[i].time;
            end=keys[i].time;
            return std::isfinite(start);
        }, 0);
    }
}

// ---- time ---------------------------------------------------------------------

void Sequencer::setFrameRate(double fps)
{
    if(!(fps >= 1.0) || !std::isfinite(fps))
        fps=1.0;
    if(fps == m_fps)
        return;

    m_fps=fps;
    // the least span and the frame ticks follow it
    const double start=m_viewStart;
    const double end=m_viewEnd;

    setViewRange(m_viewStart, m_viewEnd);
    // setViewRange() told the view of it only if the span moved
    if(start == m_viewStart && end == m_viewEnd)
        viewChanged();
}

void Sequencer::setTimeFormat(TimeFormat format)
{
    if(format == m_timeFormat)
        return;

    m_timeFormat=format;
    viewChanged();
}

void Sequencer::setTimeRange(double start, double end)
{
    if(!finite(start, end))
        return;
    if(!(end > start))
        end=start+frameLength();

    m_rangeStart=start;
    m_rangeEnd=end;
    setViewRange(start, end);
    setPlayhead(m_playhead);
}

void Sequencer::setViewRange(double start, double end)
{
    if(!finite(start, end))
        return;

    const double minSpan=MinViewFrames*frameLength();
    const double maxSpan=std::max(minSpan, 4.0*(m_rangeEnd-m_rangeStart));
    double span=std::clamp(end-start, minSpan, maxSpan);

    if(!(span > 0.0))
        span=minSpan;
    end=start+span;
    if(start == m_viewStart && end == m_viewEnd)
        return;

    m_viewStart=start;
    m_viewEnd=end;
    viewChanged();
}

void Sequencer::zoomAt(double factor, double at)
{
    if(!(factor > 0.0) || !std::isfinite(factor) || !std::isfinite(at))
        return;

    const double span=m_viewEnd-m_viewStart;
    const double minSpan=MinViewFrames*frameLength();
    const double maxSpan=std::max(minSpan, 4.0*(m_rangeEnd-m_rangeStart));
    const double newSpan=std::clamp(span/factor, minSpan, maxSpan);

    if(newSpan == span || !(span > 0.0))
        return;

    // the time at \p at stays where it is on screen
    const double share=(at-m_viewStart)/span;

    setViewRange(at-share*newSpan, at-share*newSpan+newSpan);
}

void Sequencer::panBy(double seconds)
{
    if(!std::isfinite(seconds) || seconds == 0.0)
        return;

    const double span=m_viewEnd-m_viewStart;

    setViewRange(m_viewStart+seconds, m_viewStart+seconds+span);
}

double Sequencer::snapToFrame(double t) const
{
    return static_cast<double>(frameAt(t))/m_fps;
}

long long Sequencer::frameAt(double t) const
{
    return std::llround(t*m_fps);
}

// The view moved: no layout, only the ticks (in the area the last layout gave)
// and every slot drawn from the view.
void Sequencer::viewChanged()
{
    if(m_area.w > 0)
    {
        renewMeasureKey();
        computeTicks();
    }
    // a drag of items goes on from the time now under the mouse
    if(m_dragStarted && (m_drag == Drag::Move || m_drag == Drag::ResizeLeft || m_drag == Drag::ResizeRight))
        dragTo(m_mouse);
    laneContentMoved();
}

void Sequencer::laneContentMoved()
{
    ++m_itemsSerial;
    playheadMoved();
    refreshHover();
}

void Sequencer::playheadMoved()
{
    addStateChange(StateChange_Background);
}

// Value ticks of seconds, or of whole frames at the frame rate.
std::vector<axis::Tick> Sequencer::chooseTicks(double min, double max, int width)
{
    const int maxTicks=std::max(1, width/60);

    if(m_timeFormat == TimeFormat::Frames)
    {
        const double step=std::max(1.0, axis::niceStep((max-min)*m_fps, maxTicks));

        return axis::valueAxisTicks(min*m_fps, max*m_fps, step, width);
    }

    double step=axis::niceStep(max-min, maxTicks);

    return axis::valueAxisTicks(min, max, step, width);
}

// ---- the playhead -------------------------------------------------------------

void Sequencer::setPlayhead(double t)
{
    if(!std::isfinite(t))
        return;

    t=std::clamp(t, m_rangeStart, m_rangeEnd);
    if(t == m_playhead)
        return;

    m_playhead=t;
    playheadMoved();

    event::Info info(this);

    onPlayheadChanged.call(this, info);
}

bool Sequencer::onKeyLeft(bool down)
{
    if(down)
        setPlayhead(static_cast<double>(frameAt(m_playhead)-1)/m_fps);
    return true;
}

bool Sequencer::onKeyRight(bool down)
{
    if(down)
        setPlayhead(static_cast<double>(frameAt(m_playhead)+1)/m_fps);
    return true;
}

bool Sequencer::onKeyHome(bool down)
{
    if(down)
        setPlayhead(m_rangeStart);
    return true;
}

bool Sequencer::onKeyend(bool down)
{
    if(down)
        setPlayhead(m_rangeEnd);
    return true;
}

bool Sequencer::onKeySpace(bool down)
{
    if(down)
    {
        event::Info info(this);

        onPlayToggled.call(this, info);
    }
    return true;
}

// ---- selection ------------------------------------------------------------------

bool Sequencer::setSelectedFlag(const SequencerItem &item, bool selected)
{
    if(!isValid(item))
        return false;

    bool &flag=(item.kind == SequencerItem::Kind::Clip) ? m_tracks[item.track].clips[item.index].selected
        : m_tracks[item.track].keys[item.index].selected;

    if(flag == selected)
        return false;

    flag=selected;
    if(selected)
        m_selection.insert(item);
    else
        m_selection.erase(item);
    // the item's look alone; its search is unchanged
    itemChanged(item, false);
    return true;
}

bool Sequencer::isSelected(const SequencerItem &item) const
{
    if(!isValid(item))
        return false;
    return (item.kind == SequencerItem::Kind::Clip) ? m_tracks[item.track].clips[item.index].selected
        : m_tracks[item.track].keys[item.index].selected;
}

void Sequencer::select(const SequencerItem &item, bool selected)
{
    setSelectedFlag(item, selected);
}

void Sequencer::clearSelection()
{
    for(const SequencerItem &item : getSelection())
        setSelectedFlag(item, false);
}

std::vector<SequencerItem> Sequencer::getSelection() const
{
    return std::vector<SequencerItem>(m_selection.begin(), m_selection.end());
}

void Sequencer::restoreSelection(const std::vector<SequencerItem> &selection)
{
    for(const SequencerItem &item : getSelection())
    {
        if(!holds(selection, item))
            setSelectedFlag(item, false);
    }
    for(const SequencerItem &item : selection)
        setSelectedFlag(item, true);
}

void Sequencer::fireSelectionChanged()
{
    event::Info info(this);

    onSelectionChanged.call(this, info);
}

// ---- geometry and hit test ----------------------------------------------------

bool Sequencer::clipRect(size_t track, const SequencerClip &clip, Rect &rect) const
{
    if(!finite(clip.start, clip.end) || m_area.w <= 0)
        return false;

    const double low=m_area.x-static_cast<double>(OffAreaMargin);
    const double high=m_area.x+m_area.w+static_cast<double>(OffAreaMargin);
    const int x0=static_cast<int>(std::lround(std::clamp(xPosition(clip.start), low, high)));
    int x1=static_cast<int>(std::lround(std::clamp(xPosition(clip.end), low, high)));

    if(x1 <= x0)
        x1=x0+1;

    const int64_t top=static_cast<int64_t>(m_area.y)+static_cast<int64_t>(track)*m_laneHeight-m_scrollOffset
        +ClipInset;

    rect=Rect(x0, static_cast<int>(top), x1-x0, m_laneHeight-2*ClipInset);
    return true;
}

bool Sequencer::keyRect(size_t track, const SequencerKey &key, Rect &rect) const
{
    if(!std::isfinite(key.time) || m_area.w <= 0)
        return false;

    const double low=m_area.x-static_cast<double>(OffAreaMargin+KeySize);
    const double high=m_area.x+m_area.w+static_cast<double>(OffAreaMargin+KeySize);
    const int centre=static_cast<int>(std::lround(std::clamp(xPosition(key.time), low, high)));
    const int64_t top=static_cast<int64_t>(m_area.y)+static_cast<int64_t>(track)*m_laneHeight-m_scrollOffset
        +(m_laneHeight-KeySize)/2;

    rect=Rect(centre-KeySize/2, static_cast<int>(top), KeySize, KeySize);
    return true;
}

bool Sequencer::getClipRect(size_t track, size_t index, Rect &rect) const
{
    const Track *found=trackAt(track);
    Rect lane;
    Rect clip;

    if(!found || index >= found->clips.size() || !getLaneRect(track, lane)
        || !clipRect(track, found->clips[index], clip) || !intersects(clip, m_area))
        return false;

    rect=clip;
    return true;
}

bool Sequencer::getKeyRect(size_t track, size_t index, Rect &rect) const
{
    const Track *found=trackAt(track);
    Rect lane;
    Rect key;

    if(!found || index >= found->keys.size() || !getLaneRect(track, lane)
        || !keyRect(track, found->keys[index], key) || !intersects(key, m_area))
        return false;

    rect=key;
    return true;
}

Sequencer::Part Sequencer::hitTest(const Point &local, SequencerItem &item) const
{
    if(m_area.w <= 0)
        return Part::None;

    if(local.x >= m_area.x && local.x < m_area.x+m_area.w && local.y >= m_axisTop && local.y < m_area.y)
        return Part::Ruler;

    const int laneIndex=laneAt(local.y);

    if(laneIndex < 0 || local.x < m_area.x || local.x >= m_area.x+m_area.w)
        return Part::None;

    const size_t trackIndex=static_cast<size_t>(laneIndex);
    const Track &track=m_tracks[trackIndex];

    ensureTrack(trackIndex);

    // the keys first, drawn in front of the clips: the later of two under the
    // point, the walk keeping the highest index itself
    {
        const double half=KeySize/2.0+1.0;
        bool found=false;
        size_t best=0;
        size_t unused=0;

        track.keyIndex.pick(timeAt(local.x-half), timeAt(local.x+half), [&](size_t candidate)
        {
            Rect rect;

            if(keyRect(trackIndex, track.keys[candidate], rect) && contains(rect, local)
                && (!found || candidate > best))
            {
                best=candidate;
                found=true;
            }
            return false;
        }, unused);

        if(found)
        {
            item=SequencerItem(SequencerItem::Kind::Key, trackIndex, best);
            return Part::Key;
        }
    }

    size_t best=0;

    if(!track.clipIndex.pick(timeAt(local.x-1.0), timeAt(local.x+1.0), [&](size_t candidate)
        {
            Rect rect;

            return clipRect(trackIndex, track.clips[candidate], rect) && contains(rect, local);
        }, best))
        return Part::None;

    Rect rect;

    clipRect(trackIndex, track.clips[best], rect);
    item=SequencerItem(SequencerItem::Kind::Clip, trackIndex, best);
    if(rect.w >= 3*GripWidth)
    {
        if(local.x < rect.x+GripWidth)
            return Part::ClipLeftEdge;
        if(local.x >= rect.x+rect.w-GripWidth)
            return Part::ClipRightEdge;
    }
    return Part::ClipBody;
}

Rect Sequencer::getMarqueeRect() const
{
    return m_marquee;
}

bool Sequencer::getSnapGuide(double &time) const
{
    if(!m_snapGuide)
        return false;
    time=m_snapGuideTime;
    return true;
}

// ---- hover --------------------------------------------------------------------

void Sequencer::setHover(bool hovering, const SequencerItem &item, Part part)
{
    const SequencerItem shown=hovering ? item : SequencerItem();

    if(!hovering)
        part=Part::None;
    if(hovering == m_hovering && shown == m_hoverItem && part == m_hoverPart)
        return;

    // the old and the new item's looks
    if(hovering != m_hovering || shown != m_hoverItem)
        addStateChange(StateChange_Background);

    m_hovering=hovering;
    m_hoverItem=shown;
    m_hoverPart=part;
    setCursor((part == Part::ClipLeftEdge || part == Part::ClipRightEdge) ? cursor::SizeWE : cursor::Normal);
}

void Sequencer::hoverMayHaveChanged()
{
    if(!m_mouseInside || m_hoverRefreshPosted)
        return;

    Canvas *canvas=getCanvas();

    if(!canvas)
        return;

    const size_t id=getControlId();

    m_hoverRefreshPosted=canvas->post([id]()
        {
            if(auto *seq=gweni_cast<Sequencer>(Base::getControlById(id)))
            {
                seq->m_hoverRefreshPosted=false;
                seq->refreshHover();
            }
        });
}

void Sequencer::refreshHover()
{
    // a drag keeps the item it holds; the mouse off the sequencer hovers nothing
    if(!m_mouseInside || m_drag != Drag::None)
        return;

    SequencerItem item;
    const Part part=hitTest(m_mouse, item);
    const bool onItem=(part != Part::None && part != Part::Ruler);

    setHover(onItem, item, part);
}

// ---- the mouse ------------------------------------------------------------------

void Sequencer::onMouseMoved(int x, int y, int deltaX, int deltaY)
{
    m_mouse=canvasPosToLocal(Point(x, y));
    m_mouseInside=true;

    if(m_drag != Drag::None && MouseFocus == this)
    {
        if(m_drag == Drag::Scrub)
            scrubTo(m_mouse);
        else
        {
            if(!m_dragStarted)
            {
                if(std::abs(m_mouse.x-m_press.x) < DragThreshold && std::abs(m_mouse.y-m_press.y) < DragThreshold)
                    return;
                m_dragStarted=true;
                if(m_drag == Drag::Marquee && !m_marqueeAdd && !m_marqueeToggle)
                {
                    // a plain marquee replaces the selection
                    for(const SequencerItem &item : m_pressSelection)
                        setSelectedFlag(item, false);
                }
            }
            if(m_drag == Drag::Marquee)
                marqueeTo(m_mouse);
            else
                dragTo(m_mouse);
        }
        ParentClass::onMouseMoved(x, y, deltaX, deltaY);
        return;
    }

    refreshHover();
    ParentClass::onMouseMoved(x, y, deltaX, deltaY);
}

void Sequencer::onMouseLeave()
{
    m_mouseInside=false;
    if(m_drag == Drag::None)
        setHover(false, SequencerItem(), Part::None);
    ParentClass::onMouseLeave();
}

void Sequencer::onMouseClickLeft(int x, int y, bool down)
{
    const Point local=canvasPosToLocal(Point(x, y));

    if(!down)
    {
        if(MouseFocus == this)
            MouseFocus=nullptr;
        if(m_drag != Drag::None)
            endDrag(false);
        m_mouse=local;
        refreshHover();
        return;
    }

    // a drag whose release never came (lost with the window's focus): put
    // back, as Escape puts it back
    if(m_drag != Drag::None)
        endDrag(true);

    if(isDisabled())
        return;

    focus();

    SequencerItem item;
    const Part part=hitTest(local, item);
    const bool shift=input::isShiftDown();
    const bool ctrl=input::isControlDown();

    m_press=local;
    m_pressTime=timeAt(local.x);
    m_mouse=local;
    m_drag=Drag::None;
    m_dragStarted=false;
    m_pressSelectsOnly=false;
    m_snapGuide=false;
    m_originals.clear();

    if(part == Part::Ruler)
    {
        m_drag=Drag::Scrub;
        m_scrubStart=m_playhead;
        MouseFocus=this;
        setHover(false, SequencerItem(), Part::None);
        scrubTo(local);
        return;
    }

    if(part == Part::None)
    {
        // empty lane space: a marquee
        if(!contains(m_area, local))
            return;

        m_drag=Drag::Marquee;
        m_pressSelection=getSelection();
        m_marqueeHits.clear();
        m_marqueeAdd=shift && !ctrl;
        m_marqueeToggle=ctrl;
        m_marquee=Rect(local.x, local.y, 0, 0);
        MouseFocus=this;
        return;
    }

    bool changed=false;

    // the handlers of onSelectionChanged may edit the model: the pressed
    // item followed through their removals, the press dragging nothing if
    // it is what went
    m_pressItem=item;
    m_pressItemGone=false;
    m_pressFiring=true;
    if(ctrl)
    {
        changed=setSelectedFlag(item, !isSelected(item));
        if(changed)
            fireSelectionChanged();
    }
    else if(shift)
        changed=setSelectedFlag(item, true);
    else if(!isSelected(item))
    {
        for(const SequencerItem &selected : getSelection())
            changed|=setSelectedFlag(selected, false);
        changed|=setSelectedFlag(item, true);
    }
    else
        m_pressSelectsOnly=true;

    if(changed && !ctrl)
        fireSelectionChanged();
    m_pressFiring=false;
    // gone, or toggled off with Ctrl: nothing to drag
    if(m_pressItemGone || !isValid(m_pressItem) || (ctrl && !isSelected(m_pressItem)))
        return;
    item=m_pressItem;
    if(part == Part::ClipLeftEdge)
        m_drag=Drag::ResizeLeft;
    else if(part == Part::ClipRightEdge)
        m_drag=Drag::ResizeRight;
    else
        m_drag=Drag::Move;

    auto original=[this](const SequencerItem &which)
    {
        Original o;

        o.item=which;
        if(which.kind == SequencerItem::Kind::Clip)
        {
            o.start=m_tracks[which.track].clips[which.index].start;
            o.end=m_tracks[which.track].clips[which.index].end;
        }
        else
        {
            o.start=m_tracks[which.track].keys[which.index].time;
            o.end=o.start;
        }
        return o;
    };

    if(m_drag == Drag::Move)
    {
        // the pressed item first: its edges are the ones snapped
        m_originals.push_back(original(item));
        for(const SequencerItem &selected : getSelection())
        {
            if(selected != item)
                m_originals.push_back(original(selected));
        }
    }
    else
        m_originals.push_back(original(item));

    MouseFocus=this;
}

void Sequencer::onVisibleChanged()
{
    if(visible() || m_drag == Drag::None)
        return;

    endDrag(true);
    if(MouseFocus == this)
        MouseFocus=nullptr;
}

InputCapture *Sequencer::pressGesture(int button)
{
    Canvas *canvas=getCanvas();

    if(button != 2 || !canvas || isDisabled() || m_panGesture->active())
        return nullptr;

    m_panGesture->start(canvas);
    return m_panGesture.get();
}

// Ctrl+wheel zooms time about the cursor; the plain wheel scrolls the tracks.
bool Sequencer::onMouseWheeled(int delta)
{
    if(isDisabled())
        return false;
    if(input::isControlDown())
    {
        const Point local=canvasPosToLocal(input::getMousePosition());
        const double at=(m_area.w > 0) ? timeAt(std::clamp(local.x, m_area.x, m_area.x+m_area.w)) : m_viewStart;

        zoomAt(std::pow(WheelZoomStep, delta/60.0), at);
        return true;
    }
    return ParentClass::onMouseWheeled(delta);
}

// The horizontal wheel (Shift+wheel) pans time.
bool Sequencer::onMouseWheeledH(int delta)
{
    if(isDisabled())
        return false;
    panBy(-delta/60.0*WheelPanFraction*(m_viewEnd-m_viewStart));
    return true;
}

// ---- drags ----------------------------------------------------------------------

void Sequencer::lanesBetween(int top, int bottom, int &first, int &last) const
{
    first=0;
    last=-1;
    if(m_tracks.empty() || bottom < top || m_laneHeight <= 0)
        return;

    const int64_t low=std::max<int64_t>(0, (static_cast<int64_t>(top)-m_area.y+m_scrollOffset)/m_laneHeight);
    const int64_t high=std::min<int64_t>(static_cast<int64_t>(m_tracks.size())-1,
        (static_cast<int64_t>(bottom)-m_area.y+m_scrollOffset)/m_laneHeight);

    if(high < low || bottom < m_area.y)
        return;
    first=static_cast<int>(low);
    last=static_cast<int>(high);
}

bool Sequencer::isDragged(const SequencerItem &item) const
{
    if(m_drag == Drag::Move)
        return isSelected(item);
    if(m_drag == Drag::ResizeLeft || m_drag == Drag::ResizeRight)
        return item == m_pressItem;
    return false;
}

double Sequencer::snapEdge(double t, bool allowItems, double &distance, bool &guide) const
{
    const double span=m_viewEnd-m_viewStart;
    const double perSecond=(m_area.w > 0 && span > 0.0) ? m_area.w/span : 1.0;
    const double window=SnapDistance/perSecond;
    bool found=false;
    double best=t;
    double bestDistance=std::numeric_limits<double>::infinity();
    auto consider=[&](double target)
    {
        const double d=std::abs(target-t);

        if(d <= window && d < bestDistance)
        {
            best=target;
            bestDistance=d;
            found=true;
        }
    };

    if(allowItems)
    {
        consider(m_playhead);

        // the tracks in view, their clips' edges and keys near t
        int first=0;
        int last=-1;

        lanesBetween(m_area.y, m_area.y+m_area.h-1, first, last);
        for(int lane=first; lane <= last; ++lane)
        {
            const size_t trackIndex=static_cast<size_t>(lane);
            const Track &track=m_tracks[trackIndex];

            // a drag leaves its tracks' searches as at the press (built again
            // here only after an edit of the app's): the dragged items in
            // them are passed over, every other item is where it was
            ensureTrack(trackIndex);

            const internal::LaneBarIndex &clips=track.clipIndex;
            size_t pos=static_cast<size_t>(std::lower_bound(clips.starts.begin(), clips.starts.end(), t-window)
                -clips.starts.begin());

            for(size_t k=pos; k < clips.starts.size() && clips.starts[k] <= t+window; ++k)
            {
                if(!isDragged(SequencerItem(SequencerItem::Kind::Clip, trackIndex, clips.order[k])))
                    consider(clips.starts[k]);
            }

            size_t back=static_cast<size_t>(std::upper_bound(clips.starts.begin(), clips.starts.end(), t+window)
                -clips.starts.begin());

            while(back > 0 && clips.maxEnds[back-1] >= t-window)
            {
                --back;

                const size_t index=clips.order[back];

                if(!isDragged(SequencerItem(SequencerItem::Kind::Clip, trackIndex, index)))
                    consider(track.clips[index].end);
            }

            const internal::LaneBarIndex &keys=track.keyIndex;

            pos=static_cast<size_t>(std::lower_bound(keys.starts.begin(), keys.starts.end(), t-window)
                -keys.starts.begin());
            for(size_t k=pos; k < keys.starts.size() && keys.starts[k] <= t+window; ++k)
            {
                if(!isDragged(SequencerItem(SequencerItem::Kind::Key, trackIndex, keys.order[k])))
                    consider(keys.starts[k]);
            }
        }
    }

    if(found)
    {
        guide=true;
        distance=bestDistance*perSecond;
        return best;
    }

    const double frame=snapToFrame(t);

    guide=false;
    distance=std::abs(frame-t)*perSecond;
    return frame;
}

void Sequencer::dragTo(const Point &local)
{
    if(m_originals.empty())
        return;

    const bool snap=m_snapping && !input::isKeyDown(keys::Alt);
    const Original &pressed=m_originals.front();
    double dt=timeAt(local.x)-m_pressTime;
    bool guide=false;
    double guideTime=0.0;

    if(m_drag == Drag::Move)
    {
        if(snap)
        {
            // the pressed item's edges: the nearest item or playhead target
            // either reaches, else the start on a whole frame
            double startDistance=0.0;
            double endDistance=0.0;
            bool startGuide=false;
            bool endGuide=false;
            const double start=snapEdge(pressed.start+dt, true, startDistance, startGuide);
            const bool clip=(pressed.item.kind == SequencerItem::Kind::Clip);
            const double end=clip ? snapEdge(pressed.end+dt, true, endDistance, endGuide) : start;

            if(clip && endGuide && (!startGuide || endDistance < startDistance))
            {
                dt=end-pressed.end;
                guide=true;
                guideTime=end;
            }
            else if(startGuide)
            {
                dt=start-pressed.start;
                guide=true;
                guideTime=start;
            }
            else
                dt=snapToFrame(pressed.start+dt)-pressed.start;
        }

        for(const Original &o : m_originals)
        {
            Track &track=m_tracks[o.item.track];

            if(o.item.kind == SequencerItem::Kind::Clip)
            {
                SequencerClip &clip=track.clips[o.item.index];

                if(clip.start != o.start+dt || clip.end != o.end+dt)
                {
                    clip.start=o.start+dt;
                    clip.end=o.end+dt;
                    itemChanged(o.item, false);
                }
            }
            else
            {
                SequencerKey &key=track.keys[o.item.index];

                if(key.time != o.start+dt)
                {
                    key.time=o.start+dt;
                    itemChanged(o.item, false);
                }
            }
        }
    }
    else
    {
        // a resize keeps at least a frame
        const bool left=(m_drag == Drag::ResizeLeft);
        double edge=(left ? pressed.start : pressed.end)+dt;

        if(snap)
        {
            double distance=0.0;

            edge=snapEdge(edge, true, distance, guide);
            guideTime=edge;
        }
        if(left)
            edge=std::min(edge, pressed.end-frameLength());
        else
            edge=std::max(edge, pressed.start+frameLength());
        if(guide && edge != guideTime)
            guide=false;

        Track &track=m_tracks[pressed.item.track];
        SequencerClip &clip=track.clips[pressed.item.index];
        const double start=left ? edge : pressed.start;
        const double end=left ? pressed.end : edge;

        if(clip.start != start || clip.end != end)
        {
            clip.start=start;
            clip.end=end;
            itemChanged(pressed.item, false);
        }
    }

    if(guide != m_snapGuide || (guide && guideTime != m_snapGuideTime))
    {
        m_snapGuide=guide;
        m_snapGuideTime=guideTime;
        addStateChange(StateChange_Background);
    }
}

void Sequencer::marqueeTo(const Point &local)
{
    const int x0=std::min(local.x, m_press.x);
    const int y0=std::min(local.y, m_press.y);
    const Rect rect(x0, y0, std::max(local.x, m_press.x)-x0, std::max(local.y, m_press.y)-y0);

    if(rect == m_marquee)
        return;

    m_marquee=rect;
    addStateChange(StateChange_Background);

    // the items the rect touches, in the tracks it crosses
    std::vector<SequencerItem> hits;
    int firstLane=0;
    int lastLane=-1;
    const Rect touch(rect.x, rect.y, std::max(1, rect.w), std::max(1, rect.h));

    lanesBetween(std::max(rect.y, m_area.y), std::min(rect.y+rect.h, m_area.y+m_area.h-1), firstLane, lastLane);
    for(int lane=firstLane; lane <= lastLane; ++lane)
    {
        const size_t trackIndex=static_cast<size_t>(lane);
        const Track &track=m_tracks[trackIndex];

        ensureTrack(trackIndex);

        const double low=timeAt(rect.x-KeySize);
        const double high=timeAt(rect.x+rect.w+KeySize);
        size_t unused=0;

        track.clipIndex.pick(low, high, [&](size_t candidate)
        {
            Rect clip;

            if(getClipRect(trackIndex, candidate, clip) && intersects(clip, touch))
                hits.emplace_back(SequencerItem::Kind::Clip, trackIndex, candidate);
            return false;
        }, unused);
        track.keyIndex.pick(low, high, [&](size_t candidate)
        {
            Rect key;

            if(getKeyRect(trackIndex, candidate, key) && intersects(key, touch))
                hits.emplace_back(SequencerItem::Kind::Key, trackIndex, candidate);
            return false;
        }, unused);
    }
    std::sort(hits.begin(), hits.end(), itemLess);

    // every item is as at the press (none for a plain marquee), or, in the
    // rect, selected (toggled): only what the rect left and what it touches
    // can change
    for(const SequencerItem &item : m_marqueeHits)
    {
        if(!holds(hits, item))
            setSelectedFlag(item, (m_marqueeAdd || m_marqueeToggle) && holds(m_pressSelection, item));
    }
    for(const SequencerItem &item : hits)
        setSelectedFlag(item, m_marqueeToggle ? !holds(m_pressSelection, item) : true);
    m_marqueeHits.swap(hits);
}

void Sequencer::scrubTo(const Point &local)
{
    if(m_area.w <= 0)
        return;

    double t=timeAt(std::clamp(local.x, m_area.x, m_area.x+m_area.w));

    if(!input::isKeyDown(keys::Alt))
        t=snapToFrame(t);
    setPlayhead(t);
}

void Sequencer::endDrag(bool cancelled)
{
    const Drag drag=m_drag;
    const bool started=m_dragStarted;

    m_drag=Drag::None;
    m_dragStarted=false;
    if(m_snapGuide)
    {
        m_snapGuide=false;
        addStateChange(StateChange_Background);
    }

    // the dragged tracks' searches were left as at the press
    auto dirtyDragged=[this]()
    {
        for(const Original &o : m_originals)
        {
            if(o.item.track < m_tracks.size())
            {
                Track &track=m_tracks[o.item.track];

                if(o.item.kind == SequencerItem::Kind::Clip)
                    track.clipIndex.dirty=true;
                else
                    track.keyIndex.dirty=true;
            }
        }
        // the levels the release changes are drawn
        addStateChange(StateChange_Background);
    };

    switch(drag)
    {
    case Drag::Move:
    case Drag::ResizeLeft:
    case Drag::ResizeRight:
    {
        if(started)
            dirtyDragged();
        if(cancelled)
        {
            for(const Original &o : m_originals)
            {
                Track &track=m_tracks[o.item.track];

                if(o.item.kind == SequencerItem::Kind::Clip)
                {
                    SequencerClip &clip=track.clips[o.item.index];

                    if(clip.start != o.start || clip.end != o.end)
                    {
                        clip.start=o.start;
                        clip.end=o.end;
                        itemChanged(o.item, false);
                    }
                }
                else if(track.keys[o.item.index].time != o.start)
                {
                    track.keys[o.item.index].time=o.start;
                    itemChanged(o.item, false);
                }
            }
            m_originals.clear();
            break;
        }

        if(!started)
        {
            // a plain press on a selected item, let go where it was: that item
            // alone selected
            if(m_pressSelectsOnly)
            {
                bool changed=false;

                for(const SequencerItem &selected : getSelection())
                {
                    if(selected != m_pressItem)
                        changed|=setSelectedFlag(selected, false);
                }
                if(changed)
                    fireSelectionChanged();
            }
            m_originals.clear();
            break;
        }

        m_moved.clear();
        for(const Original &o : m_originals)
        {
            const Track &track=m_tracks[o.item.track];
            const bool moved=(o.item.kind == SequencerItem::Kind::Clip)
                ? (track.clips[o.item.index].start != o.start || track.clips[o.item.index].end != o.end)
                : (track.keys[o.item.index].time != o.start);

            if(moved)
                m_moved.push_back(o.item);
        }
        m_originals.clear();
        std::sort(m_moved.begin(), m_moved.end(), itemLess);
        if(!m_moved.empty())
        {
            event::Info info(this);

            onItemsMoved.call(this, info);
        }
        break;
    }
    case Drag::Marquee:
    {
        m_marqueeHits.clear();
        if(started)
        {
            m_marquee=Rect();
            addStateChange(StateChange_Background);
        }
        if(cancelled)
        {
            restoreSelection(m_pressSelection);
            break;
        }
        if(!started && !m_marqueeAdd && !m_marqueeToggle)
        {
            // a click on empty space clears the selection
            for(const SequencerItem &item : m_pressSelection)
                setSelectedFlag(item, false);
        }
        if(getSelection() != m_pressSelection)
            fireSelectionChanged();
        break;
    }
    case Drag::Scrub:
        if(cancelled)
            setPlayhead(m_scrubStart);
        break;
    case Drag::None:
        break;
    }
    m_marquee=Rect();
}

bool Sequencer::onKeyEscape(bool down)
{
    if(m_drag == Drag::None)
        return false;
    if(down)
    {
        endDrag(true);
        refreshHover();
    }
    return true;
}

// ---- removal --------------------------------------------------------------------

bool Sequencer::onKeyDelete(bool down)
{
    if(down)
        removeSelection();
    return true;
}

// Back to front, so the indices of the items still to go hold.
void Sequencer::removeSelection()
{
    // a drag of the selection is put back first, so the app hears of no move
    if(m_drag == Drag::Move || m_drag == Drag::ResizeLeft || m_drag == Drag::ResizeRight)
    {
        endDrag(true);
        refreshHover();
    }

    const std::vector<SequencerItem> selection=getSelection();
    bool removed=false;

    for(auto it=selection.rbegin(); it != selection.rend(); ++it)
    {
        const SequencerItem item=*it;
        const uint64_t id=getItemId(item);
        event::Info info(this);

        m_removeRequest=item;
        m_removeVetoed=false;
        info.integer=static_cast<size_t>(id);
        onRemoveRequested.call(this, info);
        if(m_removeVetoed || !isValid(item) || getItemId(item) != id)
            continue;

        if(item.kind == SequencerItem::Kind::Clip)
            removeClip(item.track, item.index);
        else
            removeKey(item.track, item.index);
        removed=true;
        m_removedId=id;
        m_removedKind=item.kind;

        event::Info removedInfo(this);

        removedInfo.integer=static_cast<size_t>(id);
        onRemoved.call(this, removedInfo);
    }
    m_removeVetoed=false;
    if(removed)
        fireSelectionChanged();
}

}//namespace controls
}//namespace gweni
