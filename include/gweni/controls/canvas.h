/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_canvas_h_
#define _gweni_controls_canvas_h_

#include <gweni/controls/base.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/inputEvent.h>
#include <gweni/postQueue.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <set>
#include <vector>

namespace gweni
{
namespace controls
{

class Canvas;
class ToolTip;
class Menu;
class Popup;
enum class PopupPlacement;

/// What takes every input event a Canvas gets while it is the canvas's input
/// capture (Canvas::setInputCapture): tools::DepthView's camera. The events
/// arrive as the canvas got them, in logical units, keys as gweni::keys codes
/// and text as characters (Canvas::inputCharacter's filter not applied), and
/// go nowhere else: no control and none of input::'s state (the hover, the
/// focus, the mouse position, the keys held) sees them, but for the release of
/// a key or button the canvas had down before (Canvas::setInputCapture). Each
/// has a default that ignores it.
class GWENI_EXPORT InputCapture
{
public:
    virtual ~InputCapture() {}

    virtual void captureMouseMoved(int x, int y, int deltaX, int deltaY) {}
    virtual void captureMouseButton(int button, bool down) {}
    /// 60 a wheel notch, positive away from the user (IInputEvent's)
    virtual void captureMouseWheel(int delta) {}
    /// A key (gweni::keys) pressed or released: inputModifierKey's
    virtual void captureKey(int key, bool down) {}
    virtual void captureCharacter(gweni::UnicodeChar chr) {}
    /// The cursor left the window
    virtual void captureMouseLeave() {}
    /// \p canvas, whose capture this is, is being deleted: it no longer calls
    /// anything and must not be used after this returns
    virtual void captureCanvasDeleted(Canvas *canvas) {}
};

//! What the canvas's layout did in one frame: the update phase (doThink) and
//! the render phase (renderCanvas). A frame whose layout converges runs at most
//! one update-phase pass and none in the render phase.
struct LayoutStats
{
    //! Update-phase passes. 0 on a frame nothing invalidated, 1 when the first
    //! pass converged, more when a pass re-dirtied layout or left it dirty (a
    //! child placed in a postLayout(), after its own arrange).
    int passes=0;
    //! The first control that re-dirtied layout during a pass, in either
    //! phase, or, for a pass that left layout dirty with nothing re-dirtied,
    //! the control under which it did (see Canvas::layoutPass): its type name
    //! and control id. Null when every pass converged.
    const char *redirtiedType=nullptr;
    size_t redirtiedId=0;
    //! A phase stopped at Canvas::MaxLayoutPasses still dirty.
    bool capped=false;
    //! renderCanvas's own recurseLayout found layout dirty and ran a pass.
    bool renderLayout=false;
    //! Requests (see Base::setBounds) recorded while a pass ran, in either
    //! phase, and the first control that recorded one: its type name and
    //! control id. Only the app asks for bounds; layout code computes them
    //! (setArrangedBounds, placeChild), so a library control should leave
    //! this 0.
    int requests=0;
    const char *requestedType=nullptr;
    size_t requestedId=0;
    //! Controls the passes entered, in either phase: one per call of a
    //! control's calculateSize() and one per call of its arrange(), each axis
    //! on its own, through Base::doCalculateSize/doArrange (the canvas's own
    //! from recurseLayout, every other from the walk of its parent, a
    //! LayoutItem or a Text over its lines). An override calling its
    //! ParentClass is the same entry, and a control a walk skips (a hidden
    //! one) is not entered, so a pass over C shown controls is 4C plus what
    //! a walk enters twice (a ScrollControl's Y arrange measures and arranges
    //! its children in X too). 0 on a frame with no pass.
    size_t visited=0;
    //! The renderer::Base::measureText(), lineHeight() and measureCarets() calls
    //! the controls made from the start of doThink() to the end of the frame's layout, the
    //! render phase's pass included (see measureTextCalls()): nearly all from
    //! layout, the rest from what the update phase itself measures.
    size_t measureCalls=0;
    //! Children the passes' generic layout loops went over, in either phase:
    //! one per child (hidden ones included) each time Base::sizeOfChildren,
    //! Base::arrangeHorizontal/arrangeVertical (each of their loops), the
    //! child loop of Base::arrange or the dirty-child check that ends
    //! Base::doArrange's Y step reaches it, entered or not, and likewise one
    //! per item of each LayoutItem loop and per line of a wrapped Text's
    //! measure. So it is what visited leaves out: a parent entered for one
    //! dirty child still loops over all its siblings
    //! (Base::countLayoutChildrenScanned). 0 on a frame with no pass.
    size_t childrenScanned=0;
};

//! Layout verification (GWENI_LAYOUT_VERIFY=1, or setLayoutVerify(true)): after
//! a frame's layout has converged, in either phase, the canvas snapshots every
//! control's bounds and preferred size, runs one more pass that enters every
//! shown control (Canvas::verifyingLayout()) and compares. A pass may skip what
//! it finds clean only if that is the same as running it, and running a pass
//! again on a converged tree must change nothing, so any difference is a bug:
//! each one is printed on std::cerr as
//!   gweni: layout verify: <Type>#<id> <X|Y> <field>: pass <value>, full pass <value>
//! and counted in layoutVerifyFailures(), for a harness to fail on. So is a
//! control the full pass created or deleted, and a full pass that re-dirtied
//! layout. The full pass counts in no LayoutStats field and its measures in no
//! LayoutStats::measureCalls. Off by default; the environment variable is read
//! once, the first time a canvas asks.
GWENI_EXPORT bool layoutVerifyEnabled();
//! Turns layout verification on or off for every canvas, whatever
//! GWENI_LAYOUT_VERIFY said.
GWENI_EXPORT void setLayoutVerify(bool verify);
//! The differences layout verification found since the process started.
GWENI_EXPORT size_t layoutVerifyFailures();

//! What updateCanvas() did below the canvas in one frame, outside layout
//! (LayoutStats). There is no per-control think() walk; a settled frame
//! enters no control.
struct UpdateStats
{
    //! Controls the z renumber entered, the canvas included, on a frame after
    //! the tree order changed, else 0: the path to each control whose child
    //! list or layer counts changed, those controls and what a new label
    //! cascades into, but no clean subtree that keeps its labels; none for
    //! removals alone, and the whole tree for the first numbering, a
    //! compaction and after Canvas::invalidateZOrder().
    size_t zVisited=0;
    //! Controls of those whose z index or overlay z index the renumber changed
    //! (each counted once), every one marked StateChange_Geometry.
    size_t zChanged=0;
    //! Animations anim::think() ran for this canvas: its controls' active ones
    //! (a focused TextBox's caret, a moving ProgressBar, a timed animation).
    size_t animsRun=0;
};

//! How long one frame of a canvas took, on platform::profileTimeMicros()
//! (CanvasFrameStats::times): four reads of that clock per frame, and the
//! renderer's own in its end().
struct FrameTimes
{
    //! the clock as updateCanvas() started: the interval from one frame to the
    //! next is the difference of their starts
    uint64_t startMicros=0;
    //! updateCanvas(), whole
    uint64_t updateMicros=0;
    //! renderCanvas(), whole, the renderer's end() included
    uint64_t renderMicros=0;
    //! of renderMicros, the renderer's end() (renderer::FrameCounts::flushMicros,
    //! 0 for a renderer that does not time it): the render phase without the
    //! flush is renderMicros-flushMicros
    uint64_t flushMicros=0;
};

//! What one whole frame of a canvas counted: its updateCanvas() and its
//! renderCanvas() (Canvas::previousFrameStats()).
struct CanvasFrameStats
{
    LayoutStats layout;
    UpdateStats update;
    //! what its renderCanvas() added to getChangeStats(), and the drain's
    //! renderCarried
    ChangeStats render;
    //! how long its updateCanvas(), renderCanvas() and the renderer's end() took
    FrameTimes times;
};

//! Who wrote a canvas's last flush, taken as that flush ended
//! (Canvas::excludedSlots()): the renderer ids its Base::setExcludedFromStats()
//! subtrees (the tools: gweni::tools::Inspector, StatsOverlay) held then, and
//! those they held at the flush before that nobody held any more (the tools'
//! slots this flush cleared), so a tool reading renderer::Base's
//! writtenPrimitiveIds()/writtenTextIds() of the last frame takes every tool's
//! slots out, not only its own. A slot written under any other id was the
//! app's, an id a tool gave back and the app took again before the flush
//! included.
class GWENI_EXPORT ExcludedSlots
{
public:
    //! Whether a written primitive slot under \p id was a tool's.
    bool primitive(size_t id) const;
    //! Whether a written glyph slot under text id \p id was a tool's.
    bool text(size_t id) const;
    //! The ids the subtrees held at the flush.
    size_t heldPrimitives() const { return m_heldPrimitives.size(); }
    size_t heldTexts() const { return m_heldTexts.size(); }
    //! The live ids at the flush less heldPrimitives()/heldTexts(): the app's
    //! (taken at every flush, a tool on the canvas or not; 0 should a skin
    //! Control list more ids than are live).
    size_t appPrimitives() const { return appIds(m_livePrimitives, m_heldPrimitives.size()); }
    size_t appTexts() const { return appIds(m_liveTexts, m_heldTexts.size()); }

private:
    friend class Canvas;
    static size_t appIds(size_t live, size_t held) { return live > held ? live-held : 0; }
    //! each sorted, without repeats
    std::vector<size_t> m_heldPrimitives;
    std::vector<size_t> m_heldTexts;
    //! of the flush before's held ids, those no longer live
    std::vector<size_t> m_gonePrimitives;
    std::vector<size_t> m_goneTexts;
    //! every live id at the flush, the held ones among them
    size_t m_livePrimitives=0;
    size_t m_liveTexts=0;
};

class GWENI_EXPORT Canvas: public Base, public IInputEvent
{
public:

    typedef controls::Base ParentClass;

    Canvas(skin::Base *skin);
    virtual ~Canvas();

    virtual const char *getTypeName() const { return "Canvas"; }

    /// For additional initialization
    /// (which is sometimes not appropriate in the constructor)
    virtual void initialize() {}

    /// Call this whenever you want to process input. This
    /// is usually once a frame..
    virtual void doThink();

    /// The update phase: the posted functions (post()), then doThink()
    /// (delayed deletes, animations, layout, input state), then the z
    /// renumber if the tree order changed.
    virtual void updateCanvas();

    /// Hands \p function to the UI thread, from any thread: it runs at the top
    /// of the next updateCanvas(), before doThink(), so after the input the
    /// loop has just delivered, in the order posted (gweni::PostQueue: FIFO per
    /// thread, across threads the order they took its lock). One posted while
    /// the posts run waits for the next update. The only thread-safe call on a
    /// canvas: what the function does runs on the UI thread. Returns false,
    /// dropping it, once the canvas is being deleted.
    bool post(std::function<void()> function) { return m_postQueue->post(std::move(function)); }
    /// The canvas's queue, for a worker that may outlive the canvas: ~Canvas
    /// closes it, after which a post() through it is dropped and returns false.
    std::shared_ptr<PostQueue> postHandle() const { return m_postQueue; }
    /// Called, possibly from another thread, when a post() finds the queue
    /// empty, so an idle loop wakes for it (glfwPostEmptyEvent(), or a flag a
    /// web loop reads). The core never wakes anything itself.
    void setWakeHandler(std::function<void()> handler) { m_postQueue->setWakeHandler(std::move(handler)); }

    /// You should call this to render your canvas.
    virtual void renderCanvas();

    

    /// In most situations you will be rendering the canvas
    /// every frame. But in some situations you will only want
    /// to render when there have been changes. You can do this
    /// by checking needsRedraw().
    virtual bool needsRedraw() { return m_needsRedraw; }

    void redraw() override { m_needsRedraw=true; }

    /// Content moved under a mouse that did not move (a VirtualList scrolled
    /// its rows, or bound one to another item): the hovered control is
    /// hit-tested again at the end of this update phase, once the frame's
    /// layout is done (input::onCanvasThink), as when a capture ends.
    void hoverMayHaveMoved() { m_hoverStale=true; }
    /// Whether hoverMayHaveMoved() was called since the last call, and clears it.
    bool takeHoverStale()
    {
        const bool stale=m_hoverStale;
        m_hoverStale=false;
        return stale;
    }

    // Internal. Do not call directly.
    void render(skin::Base *render) override;

    /// Child panels call parent->getCanvas() until they get to
    /// this top level function.
    controls::Canvas *getCanvas() override
    {
        return this;
    }

    virtual void setScale(float f);
    virtual float scale() const
    {
        return m_scale;
    }

    /// Delete all children (this is done called in the destructor too)
    virtual void releaseChildren();
    /// releaseChildren() is deleting the tree, front to back: a control being
    /// deleted then leaves the canvas's other children alone, which may be gone
    /// already and are deleted by the canvas otherwise (~Base leaves the Popups
    /// it owns to it).
    bool releasingChildren() const { return m_releasingChildren; }

    /// Delayed deletes
    virtual void addDelayedDelete(controls::Base *control);
    virtual void processDelayedDeletes();
    /// Whether control is queued for a delayed delete (Base::delayedDelete())
    /// that has not run yet: it is still on the canvas, but gone at the next
    /// processDelayedDeletes(). A tool's find() skips such an instance, so a
    /// toggle() after a close() opens a new one instead of showing the doomed
    /// one again.
    bool isPendingDelete(const controls::Base *control) const
    {
        return m_deleteSet.count(const_cast<controls::Base *>(control)) != 0;
    }

    /// \sect{Layout}
    /// A layout pass that re-dirties layout (an invalidate() raised while it
    /// runs) is followed by another, at most this many per frame. A library
    /// control converges in one; the loop is a safety net, see docs/layout.md.
    static constexpr int MaxLayoutPasses=4;

    /// The update phase's layout: passes until one leaves layout clean, at most
    /// MaxLayoutPasses. doThink() calls it.
    void layoutCanvas();

    /// True while one of this canvas's layout passes runs.
    bool inLayout() const { return m_inLayout; }

    /// True while the pass running is layout verification's full pass (see
    /// layoutVerifyEnabled()), which must enter every shown control.
    static bool verifyingLayout() { return s_verifyingLayout; }

    /// This frame's layout passes, reset at the start of doThink().
    const LayoutStats &getLayoutStats() const { return m_layoutStats; }
    /// \}

    /// \sect{Z order}
    /// Some skin Control's depthLayers() or overlayLayers() now returns
    /// something else, on its own: the next updateCanvas() renumbers the z
    /// indices, entering every control, since the canvas cannot tell which
    /// changed. It still gives a new z only to the controls out of order (see
    /// ZGap). Base reports the changes it makes itself (a child added, removed
    /// or moved among its siblings, a skin Control assigned or released) for
    /// the control they happen to, and that renumber enters only the
    /// subtrees those controls are in, the ones a new label reaches into and
    /// the path to them.
    void invalidateZOrder()
    {
        m_zOrderDirty=true;
        m_zWalkAll=true;
        ++m_treeSerial;
    }

    /// The z values left free after each control's range (its depthLayers(),
    /// or its overlayLayers()) where the canvas spreads the tree out: its
    /// first numbering, a compaction (less when the range is short) and
    /// controls added after the last numbered one. A control inserted before
    /// a numbered one takes the next free z, and the controls after it keep
    /// theirs as far as these gaps reach.
    static constexpr int ZGap=7;
    /// The gap a moved subtree leaves after each range where it is moved past
    /// the last numbered control (a Window brought to the front): it takes new
    /// z at the top of the range on every move, so a smaller gap there makes
    /// compactions rarer, and one still takes a control added later.
    static constexpr int MovedZGap=1;

    /// A renumber is pending.
    bool zOrderDirty() const { return m_zOrderDirty; }

    /// This frame's counts, reset as updateCanvas() starts: its own, and those
    /// of a renumber renderCanvas() ran after its layout pass.
    const UpdateStats &getUpdateStats() const { return m_updateStats; }
    /// The counts of the last whole frame (updateCanvas() and renderCanvas()),
    /// kept as the next updateCanvas() starts, so what runs in an update phase
    /// (an animation's think(), a tool like gweni::tools::Inspector) can read
    /// the frame before it: its LayoutStats, UpdateStats and the ChangeStats its
    /// render phase counted, and its times. Zeros before the second
    /// updateCanvas().
    const CanvasFrameStats &previousFrameStats() const { return m_previousFrame; }
    /// Raised with every change of the tree's order that Base reports to the
    /// canvas (a child added, removed or moved, a skin Control assigned or
    /// released: what makes zOrderDirty()) and by invalidateZOrder(), but not
    /// for changes inside a Base::setExcludedFromStats() subtree, and by a
    /// setExcludedFromStats() change on a control on the canvas: a tool showing
    /// the tree rebuilds its view when it changes.
    unsigned treeSerial() const { return m_treeSerial; }
    /// Who wrote the last flush, for a tool reading the renderer's written ids
    /// of the last frame: a snapshot taken as renderCanvas() ends, after the
    /// renderer's end(). A slot written in it is a tool's if its id was held
    /// then by a setExcludedFromStats() subtree on this canvas (the tools, and
    /// the ToolTip box while it is a tool's), or was held by one at the flush
    /// before and is live no more (a tool's slot that flush cleared, however
    /// the id was given back: a control deleted, a skin Control removed or one
    /// shrinking its own slot list). Every other slot is the app's: an id a
    /// tool gave back and the app took again, and a control a tool owns but
    /// has not flagged (its popup). A flag or tree change counts from the next
    /// flush, and the record stays as it is until then, so every reader sees
    /// the same one, however often. One edge counts as the tool's: an id a
    /// tool gave back that the app took and gave back again before the same
    /// flush. The held and gone ids are taken only while a flagged subtree is
    /// on the canvas or the last snapshot held or cleared anything (a tool
    /// closed whole owns what the next flush clears), so with no flagged
    /// control nothing is walked or kept; the live id counts are read at every
    /// flush, so a tool opened over a settled app reads the app's ids.
    const ExcludedSlots &excludedSlots() const { return m_excludedSlots; }
    /// The highest z index the last renumber gave a control's range (its
    /// depth and overlay layers included), 0 before the first
    int highestZIndex() const { return m_zHigh; }
    /// \}

    /// \sect{Overlays}
    /// Keeps control, a child of this canvas, after every other child but a
    /// shown Popup's Menus and the ToolTip box, so the z renumber numbers it
    /// above the tree, the Popup included (an inspector's highlight box), and
    /// the layout reaches it after the app (a child added after it is moved
    /// before it ahead of the next layout, doThink). The
    /// overlays stay in the order they were added. A deleted overlay is
    /// forgotten.
    void addOverlay(Base *control);
    void removeOverlay(Base *control);
    /// \}

    /// \sect{Tooltip}
    /// The canvas shows the tooltip of the control the mouse rests on (or of
    /// that control's parent, when it has none of its own) in one ToolTip box
    /// it owns. The update phase decides, before layout: once the same control
    /// has been hovered for getToolTipDelay() seconds of
    /// platform::getTimeInSeconds() the box is filled in, placed below the
    /// control (ToolTip::placement) and shown. Moving to another control with
    /// a tooltip while one is shown fills the same box in for it at once. It
    /// hides when the mouse leaves the control or the window
    /// (inputMouseLeave), on a mouse press (until the
    /// mouse is on another control), and when the control is hidden,
    /// disabled, deleted or loses its tooltip. The box is kept as the canvas's
    /// last child, so the z renumber puts it above everything else, an open
    /// Menu included, and it is clipped only to the canvas. The box is
    /// setExcludedFromStats() while the control it shows a tooltip for is
    /// statsExcluded() (a tool's, as a StatsOverlay name cell is): from its
    /// creation, or from a show for such a control, to the next show for one
    /// that is not, so the frames that show, change and hide a tool's tooltip
    /// are counted as the tool's, and the app's count again from its own.

    /// Seconds the mouse has to rest on a control before its tooltip shows;
    /// 0.5 by default.
    void setToolTipDelay(float seconds) { m_toolTipDelay=seconds; }
    float getToolTipDelay() const { return m_toolTipDelay; }

    /// The ToolTip box, created the first time a tooltip is shown; null before.
    ToolTip *toolTipControl() const { return m_toolTip; }
    /// The control whose tooltip is shown or waiting for its delay, or null.
    Base *toolTipOwner() const { return m_toolTipOwner; }
    /// The box is showing toolTipOwner()'s tooltip.
    bool toolTipShown() const { return m_toolTipShown; }
    /// The control id (Base::getControlId()) of the control the box last
    /// showed a tooltip for, kept once it is hidden until it shows another's;
    /// 0 before any. A tool whose controls' tooltips the box shows takes the
    /// box's slots for its own while this is one of them.
    size_t toolTipShownForId() const { return m_toolTipShownForId; }

    /// Base: \p control's tooltip text or control changed; one shown is
    /// filled in again in the next update phase.
    void toolTipChanged(Base *control);
    /// Base: \p control is being deleted.
    void toolTipControlDeleted(Base *control);
    /// \}

    /// \sect{Popup}
    /// The canvas shows one popup at a time: a Popup (Popup::open, next to
    /// its anchor) or a context menu (below). Opening one closes the one
    /// shown. A press of either button closes a shown Popup unless it is on
    /// the popup, on its anchor or on a Menu opened over it (overPopup();
    /// input::onMouseClicked,
    /// before the press is delivered, which it still is); a context menu
    /// closes as menus do. Escape closes either, whatever has the keyboard
    /// focus (input::onKeyEvent), a Menu open over a Popup first. The update phase closes it when its anchor
    /// is hidden or disabled, and deleting the anchor (delayed or outright)
    /// closes it at once.

    /// The shown popup, a Popup or a context Menu, or null.
    Base *shownPopup() const { return m_popup; }
    /// The control shownPopup() is open for (a context menu's owner), or null.
    Base *popupAnchor() const { return m_popupAnchor; }
    /// Closes shownPopup(), whichever kind it is. Closing a Popup closes the
    /// Menus open over it too.
    void closePopup();
    /// input::onKeyEvent's Escape: closes the open Menus over a shown Popup
    /// (a ComboBox's list in it), or else the shown popup. False when nothing
    /// was shown.
    bool escapePopup();
    /// Popup::open: shows \p popup next to \p anchor, closing the one shown.
    void showPopup(Popup *popup, Base *anchor, PopupPlacement side);
    /// input::onMouseClicked: a press on \p hovered closes a shown Popup it
    /// is not part of (see above).
    void popupPressed(Base *hovered);
    /// \p control is in a visible Menu opened over the shown Popup: a
    /// floating Menu (a ComboBox's list, not a MenuStrip) after it among the
    /// canvas's children.
    bool overPopup(Base *control);
    /// Base's report that control is being deleted: forgets it as an overlay.
    void overlayDeleted(controls::Base *control);
    /// Base: \p control is being deleted.
    void popupControlDeleted(Base *control);
    /// \}

    /// \sect{Anchored menus}
    /// A Menu opened next to a control (Menu::openNextTo: a MenuItem's menu, a
    /// strip menu or a submenu) is followed by the canvas while it is open, as
    /// an open Popup is: every layout pass the app runs enters it
    /// (enterFollowers()), placing it in that pass from where the pass puts
    /// its anchor, and the update phase lays it out again when its anchor's
    /// canvas rect is not the one it was last placed for (a ZoomPanel's pan or
    /// zoom, which lays nothing out) and closes it when its anchor is no
    /// longer visible(). Only the menus open so are looked at, so the cost is
    /// the open strip menu and its open submenus, and a frame where no anchor
    /// moved lays out nothing. A menu leaves the list when it closes, when it
    /// is placed some other way and when it is deleted; deleting its anchor
    /// closes it.

    /// The menus followed now, in the order they opened.
    const std::vector<Menu *> &anchoredMenus() const { return m_anchoredMenus; }
    /// Menu::openNextTo: follow \p menu.
    void followAnchoredMenu(Menu *menu);
    /// ~Menu: \p menu is being deleted.
    void anchoredMenuDeleted(Menu *menu);
    /// Base: \p control is being deleted; a menu open next to it closes.
    void menuAnchorDeleted(Base *control);
    /// \}

    /// \sect{Context menu}
    /// A right-button press opens the context menu (Base::setContextMenu) of
    /// the control under the mouse, or of its nearest ancestor with one
    /// (contextMenuOwnerFor), at the cursor: input::onMouseClicked does it after
    /// the press has closed the menus that were open and reached the control.
    /// It is the canvas's popup (shownPopup()), so one is open at a time and
    /// opening a Popup closes it. Escape closes it, and so does anything that
    /// closes a menu (a click outside it, choosing an item); the update phase
    /// closes it when its control is hidden or disabled, and deleting the
    /// control closes it at once.

    /// The open context menu, or null: shownPopup() when that is one.
    Menu *shownContextMenu() const { return m_popupIsMenu ? m_contextMenu : nullptr; }
    /// The control shownContextMenu() is open for, or null.
    Base *contextMenuOwner() const { return m_popupIsMenu ? m_popupAnchor : nullptr; }
    /// The control whose context menu a right-click on \p hovered opens: the
    /// nearest of it and its ancestors with one, if that is visible and
    /// enabled; null for none, or for a part of an open menu.
    Base *contextMenuOwnerFor(Base *hovered);
    /// Opens \p owner's context menu at \p at (canvas coordinates), after
    /// firing its onContextMenu, closing the popup shown first.
    void showContextMenu(Base *owner, const Point &at);
    /// Closes shownContextMenu(), if one is open; a Popup stays.
    void closeContextMenu();
    /// \}

    /// \sect{Input capture}
    /// While a capture is set, every input* call below hands its event to it and
    /// returns true without doing anything else, so nothing on the canvas is
    /// hit-tested, hovered, focused or pressed by it, and input::'s state stays
    /// as it was: the hovered control stays hovered, the focused one focused,
    /// and once the capture is gone the next move hit-tests from the position
    /// the canvas had (a stale hover, takeHoverStale, waits for that too).
    /// What the canvas has down when it is set does not stay down: no key
    /// repeats while it is set, and the release of a key or a left or right
    /// button input:: has down goes up on the canvas as well as to the
    /// capture, so a held press ends where the canvas has the mouse (a Button
    /// held under it is clicked). The update and render phases go on as
    /// without it. The capture is not owned; clear it
    /// (nullptr) before deleting it. One test per event, nothing per frame.
    /// A canvas deleted with a capture set tells it (captureCanvasDeleted).
    void setInputCapture(InputCapture *capture) { m_inputCapture=capture; }
    InputCapture *inputCapture() const { return m_inputCapture; }
    /// \}

    /// \sect{Input}
    bool inputMouseMoved(int x, int y, int deltaX, int deltaY) override;
    bool inputMouseButton(int button, bool down) override;
    /// A modifier key was inputted.
    /// \param key - Key code
    /// \param down - Key state. Pressed?
    /// \return Event consumed?
    bool inputModifierKey(int key, bool down) override;
    /// A character was entered on the keyboard.
    /// \param chr - Unicode character code.
    /// \return Event consumed?
    bool inputCharacter(gweni::UnicodeChar chr) override;

    bool inputMouseWheel(int val) override;
    /// The cursor left the window: nothing on this canvas is hovered any more
    /// (input::onMouseLeft), so the next update phase hides its tooltip.
    bool inputMouseLeave() override;
    bool inputQuit() override
    {
        return true;
    }
    /// \}

    // Background
    virtual void setBackgroundColor(const gweni::Color &color)
    {
        m_backgroundColor=color;
    }

    virtual void setDrawBackground(bool bShouldDraw)
    {
        m_drawBackground=bShouldDraw;
    }

    virtual const gweni::Color &getBackgroundColor() const
    {
        return m_backgroundColor;
    }

protected:

    bool m_needsRedraw;
    float m_scale;
    /// post()'s queue, shared with postHandle()'s holders, closed by ~Canvas
    std::shared_ptr<PostQueue> m_postQueue=std::make_shared<PostQueue>();
    /// The renderer's textLayoutSerial() the tree was last laid out for
    unsigned m_textLayoutSerial=0;

    /// The delayed deletes: the set holds what is still queued, the list the
    /// order they go in, with entries the set no longer holds (controls
    /// already deleted) skipped as they come up (processDelayedDeletes).
    /// Each queued control points back here (Base::m_deleteQueuedOn), so it
    /// leaves this queue when it is deleted wherever it is by then: off this
    /// canvas's tree, or on another canvas's. It is in one queue at most (a
    /// second Base::delayedDelete() does nothing; one with no canvas deletes
    /// at once), and ~Canvas deletes what is still queued after the tree, so
    /// no back pointer outlives the canvas.
    controls::Base::List m_deleteList;
    std::set<controls::Base *> m_deleteSet;
    friend class controls::Base;
    void PreDeleteCanvas(controls::Base *);
    /// As renderCanvas() ends, after the renderer's end(): excludedSlots() of
    /// the flush just run.
    void snapshotExcludedSlots();
    ExcludedSlots m_excludedSlots;

    /// renderCanvas's own pass, counted in LayoutStats::renderLayout.
    void renderPhaseLayout();
    /// The render walk: renders the controls queued on this canvas
    /// (Base::drainRenderQueue), from the render offset and clip region just
    /// set on the renderer, which are this canvas's own. Called inside a
    /// RenderWalkScope, after renderPhaseLayout() and the z renumber.
    void renderPendingControls();
    /// After a tree-order change (m_zOrderDirty), keepOverlaysLast() and the z
    /// renumber, counted in m_updateStats; else nothing. Run by updateCanvas()
    /// and again after renderPhaseLayout(), so a control that pass created is
    /// drawn at its z in the same frame.
    void renumberZOrder();

    /// The update phase's tooltip step (see setToolTipDelay()), run by
    /// doThink() before layout so a box it shows or moves is laid out in the
    /// same pass.
    void updateToolTip();
    /// The control whose tooltip \p hovered shows, or null.
    Base *toolTipOwnerFor(Base *hovered);
    void showToolTipFor(Base *owner);
    void hideToolTip();
    /// The update phase's popup step: forgets a context menu closed some other
    /// way, closes the popup whose anchor is hidden or disabled, and lays a
    /// Popup out again when its anchor moved.
    void updatePopup();
    /// The update phase's step for the anchored menus (see above).
    void updateAnchoredMenus();
    /// Before each layout pass the app runs (layoutEntryCounted(): not one
    /// for setExcludedFromStats() subtrees alone): invalidates the open Popup
    /// and the open anchored menus, so the pass places each from where it
    /// puts its anchor.
    void enterFollowers();
    /// Puts a shown Popup after every other child but the Menus that were
    /// after it, and the ToolTip box at the end of the child list; run before
    /// the z renumber, which every change of the list's order asks for.
    void keepOverlaysLast();
    /// One recurseLayout with m_inLayout set. The canvas is left dirty when
    /// the pass re-dirtied layout or its own Y arrange ended dirty (a shown
    /// control under it still is), so the caller runs another.
    void layoutPass();
    /// Passes until the layout is clean, at most MaxLayoutPasses, counted in
    /// LayoutStats::passes (\p updatePhase) or as LayoutStats::renderLayout.
    void layoutUntilClean(bool updatePhase);
    /// Names the control the pass left dirty under (see layoutPass()) as the
    /// pass's culprit, when nothing re-dirtied layout.
    void nameLeftDirty();
    /// With layoutVerifyEnabled(), after a converged layout: the full pass and
    /// the comparison (see layoutVerifyEnabled()).
    void verifyLayout();
    /// Sets LayoutStats::measureCalls to the measures made since doThink() began.
    void noteMeasureCalls();
    /// Base's entry of `control` in the current pass: one LayoutStats::visited,
    /// unless !control->layoutEntryCounted().
    void countLayoutEntry(controls::Base *control);
    /// The current pass counted none of its entries but entered something
    /// (layoutEntryCounted()): it ran for setExcludedFromStats() subtrees
    /// alone, and is not counted.
    bool passOnlyExcluded() const { return m_passExcludedEntries && !m_passCountedEntries; }
    /// Base's report that `control` re-dirtied layout during the current pass.
    void layoutInvalidated(controls::Base *control);
    /// Base's report that `control` recorded a request during the current pass.
    void layoutRequested(controls::Base *control);

    /// The canvas whose pass is running, so Base can report to it without
    /// walking up to it. Passes do not nest.
    static Canvas *s_layingOut;
    /// Numbers every pass of every canvas; Base::arrange stamps it on each
    /// control it lays out, so a pass can tell what it has already laid out.
    static unsigned s_passSerial;
    /// verifyLayout()'s full pass is running.
    static bool s_verifyingLayout;

    bool m_inLayout=false;
    LayoutStats m_layoutStats;
    /// measureTextCalls() when doThink() began, for LayoutStats::measureCalls.
    size_t m_measureCallsAtThink=0;
    /// A new tree has never been numbered.
    bool m_zOrderDirty=true;
    /// renumberZOrder() has numbered the tree once (else the next pass is the
    /// first, spread out), the highest z it used, and the gap of the last full
    /// pass, ZGap unless a compaction had to make it smaller.
    bool m_zNumbered=false;
    int m_zHigh=0;
    /// invalidateZOrder() was called: the next renumber enters every control.
    bool m_zWalkAll=false;
    /// The last pass clamped ranges to the renderer's maxZIndex(), so they
    /// tie: the next one enters every control (as the first would).
    bool m_zClamped=false;
    int m_zGap=ZGap;
    UpdateStats m_updateStats;
    /// See previousFrameStats(); m_renderChange is this frame's render-phase
    /// ChangeStats, kept for it.
    CanvasFrameStats m_previousFrame;
    ChangeStats m_renderChange;
    /// This frame's times, kept for previousFrameStats() like m_renderChange.
    FrameTimes m_frameTimes;
    unsigned m_treeSerial=0;
    /// See addOverlay().
    std::vector<Base *> m_overlays;
    /// The current pass re-dirtied layout, and the first control that did.
    bool m_passRedirtied=false;
    const char *m_passCulpritType=nullptr;
    size_t m_passCulpritId=0;
    /// The current pass's entries not counted and counted, for passOnlyExcluded().
    size_t m_passExcludedEntries=0;
    size_t m_passCountedEntries=0;

    bool m_drawBackground;
    gweni::Color m_backgroundColor;

    /// The shown popup and its anchor; m_popupIsMenu when it is a context
    /// menu, then also m_contextMenu.
    Base *m_popup=nullptr;
    Base *m_popupAnchor=nullptr;
    bool m_popupIsMenu=false;
    Menu *m_contextMenu=nullptr;
    /// The menus open next to an anchor (anchoredMenus()).
    std::vector<Menu *> m_anchoredMenus;
    /// releaseChildren() is deleting the tree.
    bool m_releasingChildren=false;

    ToolTip *m_toolTip=nullptr;
    Base *m_toolTipOwner=nullptr;
    /// When m_toolTipOwner was first hovered.
    double m_toolTipSince=0.0;
    float m_toolTipDelay=0.5f;
    //! setInputCapture's, or null
    InputCapture *m_inputCapture=nullptr;
    bool m_toolTipShown=false;
    size_t m_toolTipShownForId=0;
    /// The owner's tooltip changed while shown.
    bool m_toolTipDirty=false;
    /// Pressed on: shows no tooltip until the mouse is on another control.
    Base *m_toolTipSuppressed=nullptr;

    bool m_hoverStale=false;
};


}
}
#endif//_gweni_controls_canvas_h_
