/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_curveEditor_h_
#define _gweni_controls_curveEditor_h_

#include <gweni/controls/base.h>
#include <gweni/curve.h>

#include <memory>
#include <vector>

namespace gweni
{
namespace controls
{

class CurveEditor;
class DragNumber;
class Menu;
class MenuItem;

namespace internal
{

//! A CurveEditor's plot: the rect its curves are drawn in, which takes the
//! mouse, the wheel and the keys for the editor (each goes to
//! getEditor()). It holds the keyboard focus a press gives it.
class GWENI_EXPORT CurvePlot: public Base
{
public:

    GWENI_CONTROL(CurvePlot, Base);

public:

    CurveEditor *getEditor() const;
    //! The editor's curve count changed: the plot's depth layers with it.
    void layersChanged() { zOrderChanged(); }

    void onMouseMoved(int x, int y, int deltaX, int deltaY) override;
    void onMouseClickLeft(int x, int y, bool down) override;
    void onMouseDoubleClickLeft(int x, int y) override;
    void onMouseClickRight(int x, int y, bool down) override;
    void onMouseLeave() override;
    bool onMouseWheeled(int delta) override;
    bool onMouseWheeledH(int delta) override;
    InputCapture *pressGesture(int button) override;

    bool onKeyDelete(bool down) override;
    bool onKeyLeft(bool down) override;
    bool onKeyRight(bool down) override;
    bool onKeyUp(bool down) override;
    bool onKeyDown(bool down) override;
    bool onKeyEscape(bool down) override;
    bool onChar(gweni::UnicodeChar c) override;
};

}//namespace internal

//! One key of one of a CurveEditor's curves.
struct CurveKeyRef
{
    size_t curve=0;
    size_t key=0;

    bool operator==(const CurveKeyRef &other) const { return (curve == other.curve) && (key == other.key); }
    bool operator!=(const CurveKeyRef &other) const { return !(*this == other); }
};

//! What changed in one curve since its renderer last drew it, in the order it
//! happened (CurveEditor::pendingEdits()): a splice of its spans (as
//! Curve::lastChange() gives one), a splice of its keys, or a key whose look
//! (hovered, selected) changed, at its index then.
struct CurveEditorEdit
{
    enum class Kind
    {
        Spans,
        Keys,
        KeyLook
    };

    Kind kind=Kind::Spans;
    //! the splice: [first, first+removed) replaced by [first, first+inserted);
    //! a KeyLook's key is first
    size_t first=0;
    size_t removed=0;
    size_t inserted=0;
};

//! A function-curve editor (Unity's AnimationCurve window, the ImGui curve
//! editors): one or more gweni::Curves the app owns (addCurve()), drawn
//! over a grid in a plot and edited in place, with a footer row of two
//! DragNumbers for the selected key's time and value.
//!
//! The view is an x range (time) and a y range (value) mapped onto the
//! plot's rect on the CPU (curveToLocal(), localToCurve()): x0 at the plot's
//! left edge, x1 at its right, y0 at its bottom and y1 at its top. Keys,
//! handles and strokes keep their size on screen at any zoom. A zoom or pan
//! changes the view alone: no layout pass, every curve drawn again.
//!
//! The mouse, on the plot:
//! - the wheel zooms about the cursor, WheelStep a notch, both axes, Ctrl
//!   held the x axis only, Shift (the canvas's horizontal wheel) the y axis
//!   only;
//! - a middle or right drag, or a left one with Space held, pans once the
//!   mouse is PanThreshold units from the press; a right press let go before
//!   that is a right-click, which on a key selects it (unless it is selected)
//!   and opens the key menu: the four tangent modes, Broken and Delete,
//!   applying to the selection;
//! - a left press on a key selects it as NodeGraph does: alone, Shift adding
//!   it, Ctrl toggling it; then a drag moves the selection in time and value
//!   (Shift locking it to the axis the mouse moved most), keys passing other
//!   keys reordered, and Escape puts every key back; a plain click on a key
//!   of a selection makes it the selection;
//! - a left press on a selected Free key's tangent handle drags that tangent
//!   (both while the key is not broken); Escape puts it back;
//! - a left press on empty space starts a marquee, which selects the keys it
//!   touches (added to the selection with Shift or Ctrl), Escape giving the
//!   selection back; let go where it was pressed, it clears the selection
//!   (not with Shift or Ctrl);
//! - a double-click on or near a curve (CurveHitDistance on screen from its
//!   value at the mouse's time) adds a key there, at the curve's value
//!   (Curve::addKey(time)), and selects it.
//!
//! The keys, with the plot focused: Delete removes the selection; the arrows
//! nudge it by one plot unit's worth of time (Left/Right) or value (Up/Down),
//! Shift ten; A frames every key, F the selection (Unity's); Escape cancels a
//! drag or a marquee.
//!
//! onChanged fires once per key a user's edit changed (a drag frame that
//! moves it, a nudge, a field, an added or removed key, a menu's mode or
//! Broken), \p info.point (curve, key), the key's index after the edit (a
//! removed key's before it). onSelectionChanged fires once per user action
//! that changed the selection. The app's own calls (setSelection(),
//! clearSelection(), curveChanged()) fire nothing; after editing a curve
//! itself the app calls curveChanged().
class GWENI_EXPORT CurveEditor: public Base
{
public:

    GWENI_CONTROL(CurveEditor, Base);

    //! The plot and the fields draw themselves.
    GWENI_DRAWS_NOTHING

public:

    //! How far, in canvas units, the mouse moves from a press before a right
    //! or middle press pans and a left one on empty space is a marquee.
    static constexpr int PanThreshold=3;
    //! The zoom factor of one wheel notch (60).
    static constexpr double WheelStep=1.1;
    //! A key's marker's side, and how far from its centre a press hits it.
    static constexpr int KeySize=8;
    static constexpr float KeyHitRadius=6.0f;
    //! A tangent handle's distance from its key, on screen, and its marker.
    static constexpr float HandleLength=40.0f;
    static constexpr int HandleSize=6;
    static constexpr float HandleHitRadius=5.0f;
    //! How far above or below a curve a double-click adds a key to it.
    static constexpr float CurveHitDistance=6.0f;
    //! The narrowest span the view keeps on each axis.
    static constexpr double MinViewSpan=1e-9;
    //! The footer row under the plot.
    static constexpr int FieldWidth=96;
    static constexpr int Gap=4;
    //! The share of the keys' extent A and F leave around them on each side.
    static constexpr double FramePadding=0.1;

    ~CurveEditor() override;

    //! \sect{Curves}
    //! Adds \p curve (the app's, kept alive by it while the editor shows it),
    //! drawn in the theme surface \p surface (CurveCurve<i%4> by default), and
    //! returns its index.
    size_t addCurve(Curve *curve, const String &surface="");
    //! Removes every curve; the selection goes with them, with no event.
    void clearCurves();
    size_t curveCount() const { return m_curves.size(); }
    Curve *getCurve(size_t index) const;
    const String &getCurveSurface(size_t index) const;
    //! The app edited curve \p index: it is drawn again, and the selection and
    //! hover drop the keys it no longer has, with no event.
    void curveChanged(size_t index);
    //! \}

    //! \sect{View}
    //! The x range (time) and the y range (value) the plot shows; each span
    //! kept at least MinViewSpan, the ends swapped when given backwards.
    void setView(double x0, double x1, double y0, double y1);
    double viewX0() const { return m_x0; }
    double viewX1() const { return m_x1; }
    double viewY0() const { return m_y0; }
    double viewY1() const { return m_y1; }
    //! Bumped by every change of the view.
    uint64_t viewSerial() const { return m_viewSerial; }
    //! A curve point in the plot's own units, and back.
    gweni::PointF curveToLocal(const CurvePoint &point) const;
    CurvePoint localToCurve(const gweni::PointF &local) const;
    //! Plot units per curve unit on each axis.
    double scaleX() const;
    double scaleY() const;
    //! Zooms by \p factorX and \p factorY (above 1 in) about \p at, a point in
    //! the plot's units: the curve point drawn there stays there.
    void zoomAt(double factorX, double factorY, const gweni::PointF &at);
    //! Moves the view by \p delta plot units: the content moves with it.
    void panBy(const gweni::PointF &delta);
    //! The view around every key, or the selected ones (every key with none
    //! selected), FramePadding of their extent each side; a single time or
    //! value gets a span of 1 around it. Nothing with no key.
    void frameAll();
    void frameSelection();
    //! \}

    //! \sect{Selection and hover}
    bool isSelected(size_t curve, size_t key) const;
    std::vector<CurveKeyRef> getSelection() const;
    size_t selectionSize() const;
    //! The app's selection: none of these fire onSelectionChanged.
    void setSelection(const std::vector<CurveKeyRef> &keys);
    void select(size_t curve, size_t key, bool selected=true);
    void clearSelection();
    //! The key under the resting mouse, if any.
    bool getHoveredKey(CurveKeyRef &key) const;
    //! Whether \p key shows its tangent handles (a selected Free key), and
    //! where they are in the plot's units.
    bool showsHandles(size_t curve, size_t key) const;
    void handlePositions(size_t curve, size_t key, gweni::PointF &in, gweni::PointF &out) const;
    //! How many keys show their handles now (selected Free keys).
    size_t shownHandleCount() const;
    //! \}

    //! \sect{Hit tests, in the plot's units}
    //! The key whose marker is nearest \p at within KeyHitRadius (a later
    //! curve's on a tie).
    bool keyAt(const gweni::PointF &at, CurveKeyRef &key) const;
    //! A shown handle within HandleHitRadius of \p at: \p out says which.
    bool handleAt(const gweni::PointF &at, CurveKeyRef &key, bool &out) const;
    //! The curve whose value at \p at's time is drawn nearest it, within
    //! CurveHitDistance (a later curve's on a tie).
    bool curveAt(const gweni::PointF &at, size_t &curve) const;
    //! \}

    //! \sect{The user's edits}
    //! Each is as the mouse and keys make it, firing onChanged (and
    //! onSelectionChanged where the selection changes).
    //! Adds a key to \p curve at \p time, on the curve, and makes it the
    //! selection; returns its index.
    size_t addKeyAt(size_t curve, double time);
    //! Removes the selected keys; false with none.
    bool deleteSelection();
    //! Moves the selected keys by \p dt in time and \p dv in value.
    void nudgeSelection(double dt, double dv);
    //! Sets each selected key's mode, or its Broken (making it Free).
    void setSelectionMode(Curve::TangentMode mode);
    void setSelectionBroken(bool broken);
    //! Moves the single selected key to \p time or \p value (the fields do).
    void setSelectedTime(double time);
    void setSelectedValue(double value);
    //! \}

    //! \sect{Gestures, from the plot}
    //! A left press, a move and a release at canvas points.
    void pressAt(const gweni::Point &canvasPoint);
    void dragTo(const gweni::Point &canvasPoint);
    void release();
    //! Escape: a drag or a marquee is cancelled; false when neither is on.
    bool cancelGesture();
    void doubleClickAt(const gweni::Point &canvasPoint);
    //! A right press at a canvas point: a key under it selected (if it was
    //! not) and the key menu set on the plot, to open; elsewhere none.
    void rightPressAt(const gweni::Point &canvasPoint);
    void hoverAt(const gweni::Point &canvasPoint);
    void clearHover();
    void wheel(int delta, bool horizontal);

    //! Whether the left button drags keys, a tangent, or a marquee.
    bool draggingKeys() const;
    bool draggingTangent() const;
    bool marqueeActive() const;
    //! The marquee's rect in the plot's units, once the mouse has moved
    //! PanThreshold from the press; empty otherwise.
    gweni::Rect getMarqueeRect() const;
    //! Whether a pan gesture has the mouse and has moved the view.
    bool panning() const;
    //! The key menu, made by the first right-click on a key.
    Menu *getKeyMenu() const;
    //! \}

    //! \sect{For the renderer}
    //! What changed in curve \p index since the last clearPendingEdits(), in
    //! order; pendingAll() says to draw the whole curve again instead (a new
    //! curve, the app's edit, a log that grew past MaxPendingEdits).
    const std::vector<CurveEditorEdit> &pendingEdits(size_t index) const;
    bool pendingAll(size_t index) const;
    void clearPendingEdits();
    static constexpr size_t MaxPendingEdits=256;
    //! \}

    internal::CurvePlot *getPlot() const { return m_plot; }
    DragNumber *getTimeField() const { return m_timeField; }
    DragNumber *getValueField() const { return m_valueField; }

    void setDisabled(bool active) override;
    //! Hidden, a gesture ends.
    void onVisibleChanged() override;

    void arrange(skin::Base *skin, Dim dim) override;

    gweni::event::Caller onChanged;
    gweni::event::Caller onSelectionChanged;

protected:

    friend class internal::CurvePlot;
    class PanGesture;

    struct CurveEntry
    {
        Curve *curve=nullptr;
        String surface;
        //! the selected keys' indices, sorted
        std::vector<size_t> selected;
        std::vector<CurveEditorEdit> edits;
        bool all=true;
    };

    //! The view changed: the plot draws again; nothing lays out.
    void viewChanged();
    //! Records an edit of curve \p index (and marks the plot).
    void logEdit(size_t index, const CurveEditorEdit &edit);
    void logLook(size_t curve, size_t key);
    void logAll(size_t index);
    //! The splice key \p from moving to \p to on \p curve makes: every index
    //! the editor keeps (the selection, the hover, the drag) follows it.
    void keyMoved(size_t curve, size_t from, size_t to);
    void keyRemoved(size_t curve, size_t index);
    void keyInserted(size_t curve, size_t index);
    //! Curve \p curve's key \p index moved to (time, value) as a user's edit,
    //! logged, its new index returned; \p changed whether anything moved.
    size_t moveKey(size_t curve, size_t index, double time, double value, bool &changed);
    void fireChanged(size_t curve, size_t key);
    void fireSelectionChanged();
    //! The selection as it is now, for a gesture to compare with or restore.
    std::vector<std::vector<size_t>> snapshotSelection() const;
    void restoreSelection(const std::vector<std::vector<size_t>> &snapshot);
    //! Sets \p curve's selection, logging each key whose state changed.
    void setCurveSelection(size_t curve, std::vector<size_t> selected);
    //! The footer shows the single selected key, blank and disabled else.
    void syncFields();
    void frameKeys(bool selectedOnly);
    //! The keys of every curve inside the rect, in the plot's units.
    void keysIn(const gweni::PointF &min, const gweni::PointF &max,
        std::vector<std::vector<size_t>> &found) const;
    //! The canvas point in the plot's units.
    gweni::PointF plotPoint(const gweni::Point &canvasPoint) const;
    void updateMarquee();
    void endGesture();
    //! A pan for a press of \p button (CurvePlot::pressGesture()), or null.
    InputCapture *startPan(int button);
    //! Every dragged key to its press position plus (\p dt, \p dv).
    void applyDrag(double dt, double dv);
    //! A user's tangent of \p ref, its out one or its in one.
    void setTangent(const CurveKeyRef &ref, bool out, double slope);
    void onTimeFieldChanged(event::Info info);
    void onValueFieldChanged(event::Info info);
    void onMenuMode(event::Info info);
    void onMenuBroken(event::Info info);
    void onMenuDelete(event::Info info);

    std::vector<CurveEntry> m_curves;
    double m_x0=0.0;
    double m_x1=1.0;
    double m_y0=0.0;
    double m_y1=1.0;
    uint64_t m_viewSerial=0;

    bool m_hovered=false;
    CurveKeyRef m_hover;

    enum class Gesture
    {
        None,
        Keys,
        Tangent,
        Marquee
    };

    struct DraggedKey
    {
        CurveKeyRef ref;
        double time=0.0;
        double value=0.0;
    };

    Gesture m_gesture=Gesture::None;
    //! the press in the plot's units
    gweni::PointF m_press;
    gweni::PointF m_mouse;
    bool m_moved=false;
    bool m_cancelled=false;
    //! Shift or Ctrl held at a marquee's press; a plain press on a key of a
    //! selection (made the selection on a release without a move)
    bool m_additive=false;
    bool m_pressedSelected=false;
    CurveKeyRef m_pressedKey;
    std::vector<DraggedKey> m_dragged;
    //! the time step the last drag frame applied
    double m_dragDt=0.0;
    std::vector<std::vector<size_t>> m_snapshot;
    //! the tangent drag's key, side and its tangents at the press
    CurveKeyRef m_tangentKey;
    bool m_tangentOut=false;
    double m_tangentIn=0.0;
    double m_tangentOutStart=0.0;

    std::unique_ptr<PanGesture> m_pan;
    //! the key menu, a canvas child, held by id
    size_t m_menuId=0;
    size_t m_brokenItemId=0;
    //! set while the editor sets the fields, whose events it ignores
    bool m_syncing=false;

    internal::CurvePlot *m_plot=nullptr;
    DragNumber *m_timeField=nullptr;
    DragNumber *m_valueField=nullptr;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_curveEditor_h_
