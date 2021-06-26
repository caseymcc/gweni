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
class GWENI_EXPORT Canvas: public Base, public IInputEvent
{
public:

    typedef controls::Base ParentClass;

    Canvas(skin::Base *skin);
    virtual ~Canvas();

    /// For additional initialization
    /// (which is sometimes not appropriate in the constructor)
    virtual void initialize() {}

    /// You should call this to render your canvas.
    virtual void renderCanvas();

    /// Call this whenever you want to process input. This
    /// is usually once a frame..
    virtual void doThink();

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

    virtual void  setScale(float f);
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

    controls::Base *m_firstTab;
    controls::Base *m_nextTab;

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

protected:

    bool m_needsRedraw;
    bool m_anyDelete;
    float m_scale;

    controls::Base::List m_deleteList;
    std::set<controls::Base *> m_deleteSet;
    friend class controls::Base;
    void PreDeleteCanvas(controls::Base *);

    bool m_drawBackground;
    gweni::Color m_backgroundColor;

};


}
}
#endif//_gweni_controls_canvas_h_
