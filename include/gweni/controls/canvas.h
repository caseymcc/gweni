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

    //Called to calculate window positions and prep for render call
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
    /// The current pass re-dirtied layout, and the first control that did.
    bool m_passRedirtied=false;
    const char *m_passCulpritType=nullptr;
    size_t m_passCulpritId=0;

    bool m_drawBackground;
    gweni::Color m_backgroundColor;

};


}
}
#endif//_gweni_controls_canvas_h_
