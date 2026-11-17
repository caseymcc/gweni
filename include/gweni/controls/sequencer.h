/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_sequencer_h_
#define _gweni_controls_sequencer_h_

#include <gweni/controls/laneView.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <set>
#include <vector>

namespace gweni
{
namespace controls
{

//! A clip of a Sequencer track: a time range in seconds, drawn in a theme
//! surface (SequencerClip when empty), with an id of the app's.
struct SequencerClip
{
    double start=0.0;
    double end=0.0;
    uint64_t id=0;
    String surface;
    //! The widget's: whether it is in the selection.
    bool selected=false;
};

//! A keyframe of a Sequencer track: an instant in seconds and an id of the
//! app's.
struct SequencerKey
{
    double time=0.0;
    uint64_t id=0;
    //! The widget's: whether it is in the selection.
    bool selected=false;
};

//! A clip or a key, by its track and its index there.
struct SequencerItem
{
    enum class Kind
    {
        Clip,
        Key
    };

    Kind kind=Kind::Clip;
    size_t track=0;
    size_t index=0;

    SequencerItem() {}
    SequencerItem(Kind kind_, size_t track_, size_t index_): kind(kind_), track(track_), index(index_) {}

    bool operator==(const SequencerItem &other) const
    {
        return kind == other.kind && track == other.track && index == other.index;
    }
    bool operator!=(const SequencerItem &other) const { return !(*this == other); }
};

//! Items ordered track by track, clips before keys, by index:
//! Sequencer::getSelection()'s order.
struct SequencerItemOrder
{
    bool operator()(const SequencerItem &a, const SequencerItem &b) const
    {
        if(a.track != b.track)
            return a.track < b.track;
        if(a.kind != b.kind)
            return a.kind == SequencerItem::Kind::Clip;
        return a.index < b.index;
    }
};

//! An animation sequencer, a dope sheet (ImSequencer, a DCC tool's timeline):
//! tracks of clips (time ranges) and keyframes (instants) on a time axis in
//! seconds, a playhead, a time zoom and pan, and editing by selecting and
//! dragging with snapping.
//!
//! The view is the LaneView's, shared with the Timeline: the tracks are its
//! lanes, their captions in the fixed column, the ruler its axis row above
//! the lanes, which scroll up and down under it. The axis is relative time:
//! value ticks at 1, 2 or 5 times a power of ten of seconds or, with
//! setTimeFormat(Frames), of whole frames at setFrameRate(). Its labels are
//! the renderer's text runs, not controls (tickLabelsAreControls()), so a zoom
//! or a pan lays nothing out: it moves every clip, key and tick in view, the
//! renderer drawing every item's slot again (linear in the items, by design:
//! each slot is compared with what it holds and only one that differs is
//! written, so what is uploaded is what is in view).
//!
//! The app owns the model; the sequencer holds its view of it, each item with
//! the app's id: a track's clips in the order added (an index is a place in
//! that order), searched as the Timeline's bars are (internal::LaneBarIndex:
//! sorted by start, the running maximum of the ends, an overlap level each,
//! the later of two overlapping clips on top, drawn and picked), and its keys
//! likewise (the later of two keys sharing a point picked). The app's own
//! edits (addClip(), setClip(), select(), ...) fire nothing; the user's do:
//! - a press on an item selects it (Shift adds it, Ctrl toggles it; a plain
//!   press on a selected item keeps the selection, and its release with no
//!   drag makes it the one selected item), onSelectionChanged once;
//! - a drag of a selected item, DragThreshold units from the press, moves
//!   the selection in time; a drag of a clip's edge (GripWidth units inside
//!   it, on a clip 3 grips wide or more) resizes that clip, keeping one frame
//!   at least; onItemsMoved once on the release, getMovedItems() the items;
//!   Escape puts every item back and fires nothing, and so does a removal of
//!   a dragged item or its track (removeClip(), removeKey(), removeTrack(),
//!   clearTracks(), a Delete), before the indices move; a removal of any
//!   other item keeps the drag going, its indices moved with the edit. While
//!   a drag lasts the dragged tracks' searches are not rebuilt (the dragged
//!   items keep their overlap levels until the release; hitTest() searches
//!   them where they were and accepts them where they are, so a dragged item
//!   is hit only where the two overlap), so a drag frame costs the same at
//!   any count;
//! - a drag snaps the pressed item's edges to another clip's edge, a key or
//!   the playhead within SnapDistance on-screen units (the nearest, shown by
//!   the snap guide, getSnapGuide()), else to whole frames; Alt held, or
//!   setSnapping(false), snaps to nothing;
//! - a left press on empty lane space is a marquee, selecting the items its
//!   rect touches (Shift adding to the selection at the press, Ctrl toggling
//!   it), onSelectionChanged on the release if it changed; Escape restores
//!   the selection at the press;
//! - Delete removes the selection: onRemoveRequested per item
//!   (getRemoveRequest(); vetoRemove() keeps it), then onRemoved;
//! - a press or drag in the ruler scrubs the playhead (whole frames), Left
//!   and Right step it a frame, Home and End go to the time range's ends,
//!   onPlayheadChanged on every change of it (setPlayhead() too); Space fires
//!   onPlayToggled, the app playing;
//! - Ctrl+wheel zooms time about the cursor (WheelZoomStep a notch), Shift+
//!   wheel (the canvas's horizontal wheel) or a middle drag pans it, the plain
//!   wheel scrolls the tracks.
class GWENI_EXPORT Sequencer: public LaneView
{
public:

    GWENI_CONTROL(Sequencer, LaneView);

public:

    ~Sequencer() override;

    //! A clip's inset from its lane's top and bottom.
    static constexpr int ClipInset=3;
    //! A key's side, its disc centred on its instant and its lane.
    static constexpr int KeySize=10;
    //! The width of a clip's resize grip at each edge, inside the clip.
    static constexpr int GripWidth=4;
    //! How far, in on-screen units, a dragged edge snaps to a target.
    static constexpr int SnapDistance=6;
    //! How far the mouse moves from a press before a drag starts.
    static constexpr int DragThreshold=3;
    //! The clips' overlap levels: 0 to this.
    static constexpr int MaxOverlapLevel=3;
    //! The view's span is shrunk by this a Ctrl+wheel notch, and moved a
    //! tenth of itself a horizontal notch.
    static constexpr double WheelZoomStep=1.25;
    static constexpr double WheelPanFraction=0.1;
    //! The least span of the view, in frames.
    static constexpr int MinViewFrames=4;
    //! How far outside the lane area a clip's edge is held before rounding.
    static constexpr int OffAreaMargin=8;

    enum class TimeFormat
    {
        Seconds,
        Frames
    };

    //! What a point of the sequencer is over: nothing, a clip's body or edge
    //! grip, a key, or the ruler.
    enum class Part
    {
        None,
        ClipBody,
        ClipLeftEdge,
        ClipRightEdge,
        Key,
        Ruler
    };

    // ---- tracks -----------------------------------------------------------

    //! Adds a track, last, with the app's \p id, and returns its index.
    size_t addTrack(const String &caption, uint64_t id=0);
    //! Removes the track and its items; the tracks after it move up one.
    void removeTrack(size_t track);
    void clearTracks();
    size_t getTrackCount() const { return m_tracks.size(); }
    uint64_t getTrackId(size_t track) const;
    void setTrackCaption(size_t track, const String &caption) { renameLane(track, caption); }

    // ---- clips and keys ---------------------------------------------------

    //! Adds a clip to \p track, last, and returns its index there (an end
    //! before its start is swapped with it); -1 as a size_t past the tracks.
    size_t addClip(size_t track, double start, double end, const String &surface=String(), uint64_t id=0);
    //! New times for a clip; out of range is ignored.
    void setClip(size_t track, size_t index, double start, double end);
    //! Removes the clip; the later ones move down an index.
    void removeClip(size_t track, size_t index);
    size_t getClipCount(size_t track) const;
    //! The clip, or an empty one out of range.
    const SequencerClip &getClip(size_t track, size_t index) const;
    //! The clip's overlap level, 0 out of range.
    int getOverlapLevel(size_t track, size_t index) const;

    size_t addKey(size_t track, double time, uint64_t id=0);
    void setKey(size_t track, size_t index, double time);
    void removeKey(size_t track, size_t index);
    size_t getKeyCount(size_t track) const;
    const SequencerKey &getKey(size_t track, size_t index) const;

    //! Every track's clips and keys.
    size_t getTotalItemCount() const { return m_itemCount; }
    //! The app's id of an item, 0 out of range.
    uint64_t getItemId(const SequencerItem &item) const;
    bool isValid(const SequencerItem &item) const;

    // ---- time ---------------------------------------------------------------

    //! Frames a second (30 by default; at least 1): a frame is the playhead's
    //! step, the snap of a drag and a clip's least length.
    void setFrameRate(double fps);
    double getFrameRate() const { return m_fps; }
    double frameLength() const { return 1.0/m_fps; }
    //! The ruler's labels: seconds ("1.5") or whole frames ("45").
    void setTimeFormat(TimeFormat format);
    TimeFormat getTimeFormat() const { return m_timeFormat; }

    //! The sequence's range, which the playhead keeps to and Home and End go
    //! to (a max not above min is min plus a frame), and the view (the x
    //! range shown) set to it; 0 to 10 s by default.
    void setTimeRange(double start, double end);
    double getRangeStart() const { return m_rangeStart; }
    double getRangeEnd() const { return m_rangeEnd; }
    //! The x range shown, which the zoom and pan move: its span at least
    //! MinViewFrames frames and at most 4 times the time range's.
    void setViewRange(double start, double end);
    double getViewStart() const { return m_viewStart; }
    double getViewEnd() const { return m_viewEnd; }
    double getXMin() const override { return m_viewStart; }
    double getXMax() const override { return m_viewEnd; }
    //! The view's span divided by \p factor, \p at (a time) kept where it is.
    void zoomAt(double factor, double at);
    //! The view moved by \p seconds.
    void panBy(double seconds);

    //! The time of the frame nearest \p t, and the frame number.
    double snapToFrame(double t) const;
    long long frameAt(double t) const;

    // ---- the playhead -------------------------------------------------------

    //! Moved to \p t, held to the range; onPlayheadChanged if it moved.
    void setPlayhead(double t);
    double getPlayhead() const { return m_playhead; }
    gweni::event::Caller onPlayheadChanged;
    //! Space with the sequencer focused: the app starts or stops playing.
    gweni::event::Caller onPlayToggled;

    // ---- snapping -----------------------------------------------------------

    void setSnapping(bool snapping) { m_snapping=snapping; }
    bool getSnapping() const { return m_snapping; }
    //! Where the drag being made snapped to an item or the playhead (not to a
    //! frame): false with none.
    bool getSnapGuide(double &time) const;

    // ---- selection ----------------------------------------------------------

    //! The app's selection edits: they mark what changes and fire nothing.
    void select(const SequencerItem &item, bool selected=true);
    void clearSelection();
    bool isSelected(const SequencerItem &item) const;
    //! The selected items, track by track, clips then keys, in index order
    //! (kept as a set, so this is linear in the selection, not the items).
    std::vector<SequencerItem> getSelection() const;
    size_t getSelectionSize() const { return m_selection.size(); }
    //! Fired once per user change of the selection.
    gweni::event::Caller onSelectionChanged;

    // ---- edits by the user --------------------------------------------------

    //! The items the last drag moved or resized, while onItemsMoved runs and
    //! after it, until the next drag.
    const std::vector<SequencerItem> &getMovedItems() const { return m_moved; }
    gweni::event::Caller onItemsMoved;

    //! Removes the selection as Delete does.
    void removeSelection();
    //! The item an onRemoveRequested handler is asked about (its id in
    //! info.integer), and from such a handler: keep it.
    const SequencerItem &getRemoveRequest() const { return m_removeRequest; }
    void vetoRemove() { m_removeVetoed=true; }
    gweni::event::Caller onRemoveRequested;
    //! Fired after an item is removed, its id in info.integer and in
    //! getRemovedId(), its kind in getRemovedKind().
    gweni::event::Caller onRemoved;
    uint64_t getRemovedId() const { return m_removedId; }
    SequencerItem::Kind getRemovedKind() const { return m_removedKind; }

    //! Whether a drag of items is going on (past the threshold), a marquee
    //! and its rect in this control's coordinates, a scrub.
    bool dragging() const { return m_drag != Drag::None && m_dragStarted; }
    bool marqueeActive() const { return m_drag == Drag::Marquee && m_dragStarted; }
    Rect getMarqueeRect() const;
    bool scrubbing() const { return m_drag == Drag::Scrub; }

    // ---- hit test and geometry, in this control's own coordinates ------------

    //! The item and part under \p local: a key before a clip (keys are drawn
    //! in front), the clip on top where clips overlap, its edge grips; the
    //! ruler over the axis row; Part::None elsewhere.
    Part hitTest(const Point &local, SequencerItem &item) const;
    //! The clip's rect (whole units, its edges held OffAreaMargin outside
    //! the area) and the key's: false out of range, out of view or not finite.
    bool getClipRect(size_t track, size_t index, Rect &rect) const;
    bool getKeyRect(size_t track, size_t index, Rect &rect) const;
    //! The playhead's x (fractional), as xPosition().
    double getPlayheadX() const { return xPosition(m_playhead); }

    //! The hovered item, and its part.
    bool isHovering() const { return m_hovering; }
    const SequencerItem &getHoverItem() const { return m_hoverItem; }
    Part getHoverPart() const { return m_hoverPart; }

    //! Bumped by every change that can move every item's slot or the ruler
    //! (the tracks, a removal, the view, the layout, the scroll), not by an
    //! item's own change, the hover or the playhead: the renderer draws every
    //! item again only when it moved.
    unsigned getItemsSerial() const { return m_itemsSerial; }
    //! The renderer's: the items whose slot may have changed on their own
    //! (moved, set, added, selected, a new overlap level) since the last
    //! call, moved into \p items (cleared first). False when more than
    //! MaxChangedItems were noted: then everything is to be drawn again.
    bool takeChangedItems(std::vector<SequencerItem> &items);
    static constexpr size_t MaxChangedItems=1024;
    //! How many times a track's clip or key search was built: an edit's
    //! track once, a drag's tracks once on its release, never on a drag frame.
    size_t getSearchBuildCount() const { return m_searchBuilds; }

    // ---- input --------------------------------------------------------------

    void onMouseMoved(int x, int y, int deltaX, int deltaY) override;
    void onMouseLeave() override;
    void onMouseClickLeft(int x, int y, bool down) override;
    bool onMouseWheeled(int delta) override;
    bool onMouseWheeledH(int delta) override;
    InputCapture *pressGesture(int button) override;
    //! Hidden mid-drag: the release never comes, so the drag is put back as
    //! Escape puts it back.
    void onVisibleChanged() override;
    void setDisabled(bool active) override;

    bool onKeyLeft(bool down) override;
    bool onKeyRight(bool down) override;
    bool onKeyHome(bool down) override;
    bool onKeyend(bool down) override;
    bool onKeySpace(bool down) override;
    bool onKeyDelete(bool down) override;
    bool onKeyEscape(bool down) override;

protected:

    std::vector<axis::Tick> chooseTicks(double min, double max, int width) override;
    bool tickLabelsAreControls() const override { return false; }
    void laneContentMoved() override;
    int minLaneHeight() const override { return std::max(2*ClipInset+1, KeySize); }

    struct Track
    {
        uint64_t id=0;
        std::vector<SequencerClip> clips;
        std::vector<SequencerKey> keys;
        mutable internal::LaneBarIndex clipIndex;
        mutable internal::LaneBarIndex keyIndex;
    };

    const Track *trackAt(size_t track) const { return track < m_tracks.size() ? &m_tracks[track] : nullptr; }
    //! The track's searches built again if they are dirty, every clip whose
    //! overlap level that changes noted.
    void ensureTrack(size_t track) const;
    //! An item changed: noted, the sequencer marked, the track's search
    //! dirtied (unless \p search is false: a drag frame).
    void itemChanged(const SequencerItem &item, bool search=true);
    void noteChanged(const SequencerItem &item) const;
    //! Every item drawn again (the indices moved).
    void allItemsChanged();

    //! What a removal takes: a whole track, or one item.
    struct Removal
    {
        bool wholeTrack=false;
        SequencerItem item;
    };
    //! \p item after \p removal: false if it is what goes, else its index
    //! (or track) moved down past it.
    static bool remap(SequencerItem &item, const Removal &removal);
    //! Before a removal: a drag holding what goes is cancelled (its items
    //! put back while their indices hold), and the indices the drag, the
    //! marquee, the selection, the moved items and the selection Delete
    //! is removing hold follow the edit.
    void removing(const Removal &removal);

    //! The rect of a clip / key in lane \p track, without the view checks.
    bool clipRect(size_t track, const SequencerClip &clip, Rect &rect) const;
    bool keyRect(size_t track, const SequencerKey &key, Rect &rect) const;

    //! The ticks moved with the view: computed again (no layout) and every
    //! item in view drawn again.
    void viewChanged();
    void playheadMoved();

    void setHover(bool hovering, const SequencerItem &item, Part part);
    void refreshHover();
    //! An edit of the app's may have moved what is under the mouse: the hover
    //! is hit-tested again once, at the top of the next update phase (a
    //! post), so a batch of edits builds each track's search once.
    void hoverMayHaveChanged();

    bool setSelectedFlag(const SequencerItem &item, bool selected);
    void restoreSelection(const std::vector<SequencerItem> &selection);
    void fireSelectionChanged();

    //! The item edges a target may be: the clips' edges and the keys in the
    //! tracks in view and the playhead, but not the items being dragged.
    bool isDragged(const SequencerItem &item) const;
    //! The tracks whose lanes reach from \p top to \p bottom (this control's
    //! y), at the scroll offset: none (\p last below \p first) past them.
    void lanesBetween(int top, int bottom, int &first, int &last) const;
    //! \p t snapped (with \p allowItems, to an item edge or the playhead
    //! within SnapDistance, nearest first, then to a frame); \p distance the
    //! on-screen distance moved, \p guide whether it snapped to an item.
    double snapEdge(double t, bool allowItems, double &distance, bool &guide) const;
    //! The mouse at \p local during a drag of items.
    void dragTo(const Point &local);
    void endDrag(bool cancelled);
    //! The marquee from the press to local; force takes its hits again with
    //! the rect unchanged (the view or the lanes moved under it).
    void marqueeTo(const Point &local, bool force=false);
    void scrubTo(const Point &local);

    class PanGesture;
    std::unique_ptr<PanGesture> m_panGesture;

    std::vector<Track> m_tracks;
    size_t m_itemCount=0;
    std::set<SequencerItem, SequencerItemOrder> m_selection;
    mutable std::vector<SequencerItem> m_changed;
    mutable bool m_changedOverflow=false;
    mutable size_t m_searchBuilds=0;

    double m_fps=30.0;
    TimeFormat m_timeFormat=TimeFormat::Seconds;
    double m_rangeStart=0.0;
    double m_rangeEnd=10.0;
    double m_viewStart=0.0;
    double m_viewEnd=10.0;
    double m_playhead=0.0;
    bool m_snapping=true;

    unsigned m_itemsSerial=0;

    bool m_hovering=false;
    SequencerItem m_hoverItem;
    Part m_hoverPart=Part::None;
    bool m_mouseInside=false;
    Point m_mouse;
    bool m_hoverRefreshPosted=false;

    enum class Drag
    {
        None,
        Move,
        ResizeLeft,
        ResizeRight,
        Marquee,
        Scrub
    };

    struct Original
    {
        SequencerItem item;
        double start=0.0;
        double end=0.0;
    };

    Drag m_drag=Drag::None;
    bool m_dragStarted=false;
    Point m_press;
    //! the time under the press: a drag moves by the time under the mouse
    //! less it, so a pan or zoom mid-drag keeps the item under the cursor
    double m_pressTime=0.0;
    SequencerItem m_pressItem;
    //! while a press fires onSelectionChanged: m_pressItem follows the
    //! handler's removals, and is cleared if it is what goes
    bool m_pressFiring=false;
    bool m_pressItemGone=false;
    //! a plain press on a selected item: its release with no drag selects it alone
    bool m_pressSelectsOnly=false;
    std::vector<Original> m_originals;
    std::vector<SequencerItem> m_moved;
    std::vector<SequencerItem> m_pressSelection;
    //! what the marquee touched as it was last moved, sorted
    std::vector<SequencerItem> m_marqueeHits;
    //! the marquee's modifiers at the press
    bool m_marqueeAdd=false;
    bool m_marqueeToggle=false;
    Rect m_marquee;
    double m_scrubStart=0.0;
    bool m_snapGuide=false;
    double m_snapGuideTime=0.0;

    //! the selection Delete has still to remove, back to front, remapped
    //! by removing() so a handler's edit leaves it naming the same items
    std::vector<SequencerItem> m_pendingRemovals;
    bool m_removingSelection=false;
    SequencerItem m_removeRequest;
    //! the item asked about went in its handler (removing())
    bool m_removeRequestGone=false;
    bool m_removeVetoed=false;
    uint64_t m_removedId=0;
    SequencerItem::Kind m_removedKind=SequencerItem::Kind::Clip;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_sequencer_h_
