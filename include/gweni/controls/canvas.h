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
#include <set>

namespace gweni
{
namespace controls
{

class ToolTip;
class Menu;

//! What the canvas's layout did in one frame: the update phase (doThink) and
//! the render phase (renderCanvas). A frame whose layout converges runs at most
//! one update-phase pass and none in the render phase.
struct LayoutStats
{
    //! Update-phase passes. 0 on a frame nothing invalidated, 1 when the first
    //! pass converged, more when a pass re-dirtied layout.
    int passes=0;
    //! The first control that re-dirtied layout during an update-phase pass:
    //! its type name and control id. Null when no pass re-dirtied.
    const char *redirtiedType=nullptr;
    size_t redirtiedId=0;
    //! The update phase stopped at Canvas::MaxLayoutPasses still dirty.
    bool capped=false;
    //! renderCanvas's own recurseLayout found layout dirty and ran a pass.
    bool renderLayout=false;
};

//! What updateCanvas() did below the canvas in one frame, outside layout
//! (LayoutStats). There is no per-control think() walk; a settled frame
//! enters no control.
struct UpdateStats
{
    //! Controls the z renumber entered, the canvas included: the whole tree on
    //! a frame after the tree order changed (Canvas::invalidateZOrder()), else 0.
    size_t zVisited=0;
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

    /// The update phase: doThink() (delayed deletes, animations, layout, input
    /// state), then the z renumber if the tree order changed.
    virtual void updateCanvas();

    /// You should call this to render your canvas.
    virtual void renderCanvas();

    

    /// In most situations you will be rendering the canvas
    /// every frame. But in some situations you will only want
    /// to render when there have been changes. You can do this
    /// by checking needsRedraw().
    virtual bool needsRedraw() { return m_needsRedraw; }

    void redraw() override { m_needsRedraw=true; }

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

    void onBoundsChanged(gweni::Rect oldBounds) override;

    /// Delete all children (this is done called in the destructor too)
    virtual void releaseChildren();

    /// Delayed deletes
    virtual void addDelayedDelete(controls::Base *control);
    virtual void processDelayedDeletes();

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

    /// This frame's layout passes, reset at the start of doThink().
    const LayoutStats &getLayoutStats() const { return m_layoutStats; }
    /// \}

    /// \sect{Z order}
    /// The tree order changed (a child added, removed or moved among its
    /// siblings) or a control's skin Control, and with it its depthLayers(): the
    /// next updateCanvas() numbers every z index again. Base calls it for each
    /// of those; call it yourself only after making a skin Control's
    /// depthLayers() return something else.
    void invalidateZOrder() { m_zOrderDirty=true; }

    /// A renumber is pending.
    bool zOrderDirty() const { return m_zOrderDirty; }

    /// This frame's updateCanvas() counts, reset as it starts.
    const UpdateStats &getUpdateStats() const { return m_updateStats; }
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
    /// Menu included, and it is clipped only to the canvas.

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

    /// Base: \p control's tooltip text or control changed; one shown is
    /// filled in again in the next update phase.
    void toolTipChanged(Base *control);
    /// Base: \p control is being deleted.
    void toolTipControlDeleted(Base *control);
    /// \}

    /// \sect{Context menu}
    /// A right-button press opens the context menu (Base::setContextMenu) of
    /// the control under the mouse, or of its nearest ancestor with one
    /// (contextMenuOwnerFor), at the cursor: input::onMouseClicked does it after
    /// the press has closed the menus that were open and reached the control.
    /// One is open at a time. Escape closes it (input::onKeyEvent, whatever
    /// has the keyboard focus), and so does anything that closes a menu (a
    /// click outside it, choosing an item); the update phase closes it when
    /// its control is hidden, and deleting the control closes it at once.

    /// The open context menu, or null.
    Menu *shownContextMenu() const { return m_contextMenu; }
    /// The control shownContextMenu() is open for, or null.
    Base *contextMenuOwner() const { return m_contextMenuOwner; }
    /// The control whose context menu a right-click on \p hovered opens: the
    /// nearest of it and its ancestors with one, if that is visible and
    /// enabled; null for none, or for a part of an open menu.
    Base *contextMenuOwnerFor(Base *hovered);
    /// Opens \p owner's context menu at \p at (canvas coordinates), after
    /// firing its onContextMenu, closing the one open first.
    void showContextMenu(Base *owner, const Point &at);
    void closeContextMenu();
    /// Base: \p control is being deleted.
    void contextMenuControlDeleted(Base *control);
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
    bool m_anyDelete;
    float m_scale;
    /// The renderer's textLayoutSerial() the tree was last laid out for
    unsigned m_textLayoutSerial=0;

    controls::Base::List m_deleteList;
    std::set<controls::Base *> m_deleteSet;
    friend class controls::Base;
    void PreDeleteCanvas(controls::Base *);

    /// renderCanvas's own pass, counted in LayoutStats::renderLayout.
    void renderPhaseLayout();

    /// The update phase's tooltip step (see setToolTipDelay()), run by
    /// doThink() before layout so a box it shows or moves is laid out in the
    /// same pass.
    void updateToolTip();
    /// The control whose tooltip \p hovered shows, or null.
    Base *toolTipOwnerFor(Base *hovered);
    void showToolTipFor(Base *owner);
    void hideToolTip();
    /// The update phase's context-menu step: forgets a menu closed some other
    /// way and closes one whose control is hidden.
    void updateContextMenu();
    /// Puts the box back at the end of the child list; run before the z
    /// renumber, which every change of the list's order asks for.
    void keepToolTipLast();
    /// One recurseLayout with m_inLayout set.
    void layoutPass();
    /// Base's report that `control` re-dirtied layout during the current pass.
    void layoutInvalidated(controls::Base *control);

    /// The canvas whose pass is running, so Base can report to it without
    /// walking up to it. Passes do not nest.
    static Canvas *s_layingOut;
    /// Numbers every pass of every canvas; Base::arrange stamps it on each
    /// control it lays out, so a pass can tell what it has already laid out.
    static unsigned s_passSerial;

    bool m_inLayout=false;
    LayoutStats m_layoutStats;
    /// A new tree has never been numbered.
    bool m_zOrderDirty=true;
    UpdateStats m_updateStats;
    /// The current pass re-dirtied layout, and the first control that did.
    bool m_passRedirtied=false;
    const char *m_passCulpritType=nullptr;
    size_t m_passCulpritId=0;

    bool m_drawBackground;
    gweni::Color m_backgroundColor;

    Menu *m_contextMenu=nullptr;
    Base *m_contextMenuOwner=nullptr;
    /// releaseChildren() is deleting the tree.
    bool m_releasingChildren=false;

    ToolTip *m_toolTip=nullptr;
    Base *m_toolTipOwner=nullptr;
    /// When m_toolTipOwner was first hovered.
    float m_toolTipSince=0.0f;
    float m_toolTipDelay=0.5f;
    bool m_toolTipShown=false;
    /// The owner's tooltip changed while shown.
    bool m_toolTipDirty=false;
    /// Pressed on: shows no tooltip until the mouse is on another control.
    Base *m_toolTipSuppressed=nullptr;
};


}
}
#endif//_gweni_controls_canvas_h_
