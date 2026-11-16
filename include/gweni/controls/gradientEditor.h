/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_gradienteditor_h_
#define _gweni_controls_gradienteditor_h_

#include <gweni/controls/base.h>
#include <gweni/gradient.h>

#include <vector>

namespace gweni
{
namespace controls
{

class ColorEdit;
class DragNumber;
class GradientEditor;

namespace internal
{

//! The ramp a GradientEditor shows: its gradient over the bar's width, t 0 at
//! the bar's left edge and 1 at its right. It takes no mouse input.
class GWENI_EXPORT GradientBar: public Base
{
public:

    GWENI_CONTROL(GradientBar, Base);

public:

    GradientEditor *getEditor() const;
};

//! The strip under the bar holding the stops' handles: a press on it away
//! from a handle adds a stop there (GradientEditor::addStopAt). It draws
//! nothing itself.
class GWENI_EXPORT GradientStopStrip: public Base
{
public:

    GWENI_CONTROL(GradientStopStrip, Base);

    GWENI_DRAWS_NOTHING

public:

    GradientEditor *getEditor() const;

    void onMouseClickLeft(int x, int y, bool down) override;
    //! Places each handle at its stop's position.
    void layout(skin::Base *skin) override;
};

//! One stop's handle: a marker pointing up at the bar over a swatch of the
//! stop's colour. A press selects the stop and drags it along the strip, a
//! double-click opens the editor's colour picker; while it has the keyboard
//! focus Left/Right nudge it, Delete removes it and Escape ends a drag where
//! it began. The handles are their strip's children in stop order, so Tab and
//! Shift+Tab step through the stops in order.
class GWENI_EXPORT GradientStopHandle: public Base
{
public:

    GWENI_CONTROL(GradientStopHandle, Base);

public:

    GradientEditor *getEditor() const;

    //! The stop's colour, which the swatch shows; a new one marks
    //! StateChange_Background.
    void setStopColor(const gweni::Color &color);
    const gweni::Color &getStopColor() const { return m_color; }

    //! The editor's selected stop.
    bool isSelected() const;
    //! The left button is down on it, dragging it.
    bool isDepressed() const;

    void onMouseClickLeft(int x, int y, bool down) override;
    void onMouseDoubleClickLeft(int x, int y) override;
    void onMouseMoved(int x, int y, int deltaX, int deltaY) override;
    void onKeyboardFocus() override;

    bool onKeyLeft(bool down) override;
    bool onKeyRight(bool down) override;
    bool onKeyDelete(bool down) override;
    bool onKeyEscape(bool down) override;

private:

    gweni::Color m_color=gweni::Color(0, 0, 0, 255);
};

}//namespace internal

//! A colour ramp editor (Blender's ColorRamp, Unity's Gradient editor): a bar
//! showing a gweni::Gradient, a strip of stop handles under it and, under
//! that, a row for the selected stop: a ColorEdit (its swatch opens the HSV
//! picker) and a DragNumber for its position, 0..1.
//!
//! The app owns the gradient (setGradient()); without one the editor edits a
//! gradient of its own (black to white). The user edits it:
//! - a press on the strip away from a handle adds a stop there, of the
//!   colour the gradient has there (evaluate(t)), selects it and drags it;
//! - a press on a handle selects its stop and drags it, clamped to 0..1, past
//!   other stops too (they are reordered); Escape during the drag puts it back;
//! - Tab and Shift+Tab move through the stops in order (each handle takes the
//!   focus, and selects its stop), Left/Right nudge the focused handle's stop
//!   by NudgeStep (Shift: NudgeShiftStep), Delete removes it unless it is the
//!   last;
//! - disabled, the handles take no press, focus or Tab;
//! - a double-click on a handle opens the ColorEdit's picker;
//! - the ColorEdit recolours the selected stop and the DragNumber moves it.
//!
//! onChanged fires once per real change of the gradient by the user (a drag
//! frame that moves the stop, an add, a delete, a colour, a position), with
//! the changed stop's index in info.integer (after a delete, the stop then
//! selected; after an Escape, the dragged stop); the app's own edits fire
//! nothing, and after editing the gradient itself the app calls
//! gradientChanged(). onSelectionChange fires when the selected stop becomes
//! another (not when it is reordered).
//!
//! Layout: the bar spans the control's width less HandleWidth (half a handle
//! in from each side, so a handle centred on a stop at 0 or 1 fits), the strip
//! right under it the whole width, the row Gap under the strip; the control
//! is as high as its parts (Fixed in X, Elastic in Y). Its parts are
//! Owner-placed in layout(); the handles are placed by the strip, so a stop
//! that moves lays out the strip alone.
class GWENI_EXPORT GradientEditor: public Base
{
public:

    GWENI_CONTROL(GradientEditor, Base);

    //! Its parts draw themselves.
    GWENI_DRAWS_NOTHING

public:

    static constexpr int HandleWidth=11;
    static constexpr int BarHeight=20;
    static constexpr int StripHeight=16;
    //! The top of a handle its marker (the triangle pointing at the bar)
    //! takes; its swatch is the rest.
    static constexpr int MarkerHeight=5;
    static constexpr int Gap=4;
    static constexpr int PositionFieldWidth=64;
    static constexpr float NudgeStep=0.01f;
    static constexpr float NudgeShiftStep=0.1f;

    //! The gradient to edit, the app's (null: the editor's own). Its stops get
    //! handles, the first is selected; nothing fires.
    void setGradient(Gradient *gradient);
    Gradient *getGradient() const { return m_gradient; }
    //! The app edited the gradient: the handles, the bar and the row follow,
    //! the selection kept where it can be (clamped to the stops), with no event.
    void gradientChanged();

    //! The selected stop's index (there is always one).
    size_t getSelectedStop() const { return m_selected; }
    //! Selects stop \p index (clamped); with \p doEvents (a handle taking the
    //! focus) onSelectionChange fires if it is another stop.
    void setSelectedStop(size_t index, bool doEvents=false);

    //! The user's edits, as the mouse and keys make them: each fires
    //! onChanged when it changed the gradient. addStopAt() adds a stop at t of
    //! evaluate(t)'s colour and selects it, returning its index; moveSelectedTo
    //! moves the selected stop (reordering it); nudgeSelected() moves it by
    //! \p delta; deleteSelected() removes it (false for the last stop).
    size_t addStopAt(float t);
    void moveSelectedTo(float t);
    void nudgeSelected(float delta);
    bool deleteSelected();
    //! nudgeSelected() and deleteSelected() for stop \p index (a focused
    //! handle's own stop, which an app's setSelectedStop() can have moved the
    //! selection away from): the stop is selected first. A delete is refused
    //! during a drag.
    void nudgeStop(size_t index, float delta);
    bool deleteStop(size_t index);
    void setSelectedColor(const gweni::Color &color);

    //! A drag of \p handle's stop (selected by the press), grabbed \p grab
    //! units from the handle's left edge: dragTo() follows the mouse with that
    //! handle's stop whatever is selected meanwhile, endDrag() is the release,
    //! cancelDrag() Escape (every stop back as the press found it, order
    //! included, so two stops at one position keep their hard edge).
    void beginDrag(internal::GradientStopHandle *handle, int grab);
    void dragTo(int x);
    void endDrag();
    bool cancelDrag();
    //! A drag lasts while its handle holds the mouse (gweni::MouseFocus): one
    //! the canvas took it from (the editor hidden, another capture) has ended.
    bool isDragging() const;
    internal::GradientStopHandle *getDraggedHandle() const { return isDragging()?m_drag.handle:nullptr; }

    //! The t a strip-local x stands for (a handle's centre), clamped to 0..1,
    //! and a handle's left edge in the strip for a t.
    float stripPosition(int stripX) const;
    int handleLeft(float t) const;

    internal::GradientBar *getBar() const { return m_bar; }
    internal::GradientStopStrip *getStrip() const { return m_strip; }
    size_t getHandleCount() const { return m_handles.size(); }
    internal::GradientStopHandle *getHandle(size_t index) const;
    //! The stop \p handle stands for, or getHandleCount() for none.
    size_t indexOf(const internal::GradientStopHandle *handle) const;
    ColorEdit *getColorEdit() const { return m_colorEdit; }
    DragNumber *getPositionField() const { return m_position; }

    void setDisabled(bool active) override;
    //! Hidden, the editor's drag ends (the canvas takes the mouse from it).
    void onVisibleChanged() override;

    void calculateSize(skin::Base *skin, Dim dim) override;
    void layout(skin::Base *skin) override;

    gweni::event::Caller onChanged;
    gweni::event::Caller onSelectionChange;

protected:

    //! The handles follow the gradient's stop count, each its stop's colour.
    void syncHandles();
    //! The row shows the selected stop.
    void syncRow();
    //! The bar shows the gradient again and the strip places the handles.
    void stopsMoved();
    void select(size_t index, bool doEvents);
    //! Moves stop \p index to \p t, keeping its handle with it (reordered in
    //! the strip when it passes another), \p index its new place; false when
    //! nothing changed.
    bool moveStop(size_t &index, float t);
    //! moveStop() as the user's edit: the row follows, onChanged fires.
    void userMove(size_t index, float t);
    //! A handle takes the keyboard (and Tab) only while the editor is enabled.
    void applyHandleInput(internal::GradientStopHandle *handle);
    void fireChanged(size_t index);

    void onColorEditChanged(event::Info info);
    void onPositionChanged(event::Info info);

    Gradient m_ownGradient;
    Gradient *m_gradient=&m_ownGradient;
    size_t m_selected=0;
    //! Set while the editor sets the row's values, whose events it ignores.
    bool m_syncing=false;

    struct Drag
    {
        internal::GradientStopHandle *handle=nullptr;
        //! The stops and their handles at the press, which Escape gives back.
        std::vector<GradientStop> stops;
        std::vector<internal::GradientStopHandle *> handles;
        int grab=0;
        bool cancelled=false;
    };
    Drag m_drag;

    internal::GradientBar *m_bar=nullptr;
    internal::GradientStopStrip *m_strip=nullptr;
    std::vector<internal::GradientStopHandle *> m_handles;
    ColorEdit *m_colorEdit=nullptr;
    DragNumber *m_position=nullptr;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_gradienteditor_h_
