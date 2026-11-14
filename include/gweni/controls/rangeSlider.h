/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_rangeslider_h_
#define _gweni_controls_rangeslider_h_

#include <gweni/controls/slider.h>
#include <gweni/controls/text.h>

namespace gweni
{
namespace controls
{

class RangeSlider;

namespace internal
{

//! One of a RangeSlider's two thumbs: a SliderBar whose drag, press and keys
//! are the slider's (RangeSlider::thumbPressed and the rest), so the slider
//! decides where it goes (stopped at the other thumb and the gap, snapped to
//! the step) and which thumb a press on two overlapping ones drags. It takes
//! the keyboard focus and Tab, the low thumb first.
class GWENI_EXPORT RangeSliderThumb: public SliderBar
{
    GWENI_CONTROL(RangeSliderThumb, SliderBar);

public:
    //! 0 for the low thumb, 1 for the high one.
    int getIndex() const { return m_index; }
    RangeSlider *getRangeSlider() const { return m_slider; }

    //! Shows (or drops) the dragged look: isDepressed().
    void setDepressed(bool depressed);

    void onMouseClickLeft(int x, int y, bool down) override;
    void onMouseMoved(int x, int y, int deltaX, int deltaY) override;
    void onMouseDoubleClickLeft(int /*x*/, int /*y*/) override {}

    void onKeyboardFocus() override;
    bool onKeyLeft(bool down) override;
    bool onKeyRight(bool down) override;
    bool onKeyUp(bool down) override;
    bool onKeyDown(bool down) override;
    bool onKeyHome(bool down) override;
    bool onKeyend(bool down) override;
    bool onKeyPageUp(bool down) override;
    bool onKeyPageDown(bool down) override;

private:
    friend class controls::RangeSlider;

    RangeSlider *m_slider=nullptr;
    int m_index=0;
};

}//namespace internal

//! Two thumbs on one track picking a range (MUI's range Slider, imgui's
//! DragFloatRange2, Qt's dual-handle sliders): getLow() to getHigh() in
//! getMin() to getMax(), never closer than getMinimumGap(), the range between
//! the thumbs drawn as a fill over the track.
//!
//! The thumbs are internal::RangeSliderThumb children (SliderBars), placed by
//! the slider (Placement::Owner) at their values: a thumb's position along its
//! travel is rounded from its value (Slider::thumbOffset, as a Slider's since
//! #466), and a drag reads the value from the position the mouse gives it, so
//! a continuous drag moves the thumb by exactly the mouse's step. A dragged
//! thumb stops at the other one's value less (or plus) the gap and at the
//! range's ends; it never pushes the other thumb (MUI's default). A press on
//! the track moves the nearer thumb to it and drags it on; on a tie the one
//! that can move that way, and two thumbs at one value (gap 0) are told apart
//! by the drag's first move: toward the minimum the low one, else the high.
//!
//! With a step (setStep) or clamped to its notches (setNotchCount,
//! setClampToNotches, as a Slider's), every value is on that grid; a key moves
//! the focused thumb by a step (by a hundredth of the range on a continuous
//! slider), PageUp and PageDown by ten, Home and End to its limits.
//!
//! The value text (setValueFormat, Slider::validValueFormat's formats) is a
//! controls::internal::Text per thumb (PerThumb) or one "lo – hi"
//! (Joined), each set only when its string changes. Horizontal, the track is a
//! field the thumbs' height with the low text centred in its left half and the
//! high one in its right half (a Joined text in the whole field); vertical,
//! the texts are in a column right of the track, the high one at its top and
//! the low one at its bottom (a Joined one centred), as wide as the widest
//! string the range can give.
//!
//! Dragging the range itself (both thumbs at once, a press on the fill) is
//! not done; it would be a third drag target in thumbPressed()'s place for a
//! press between the thumbs.
class GWENI_EXPORT RangeSlider: public Base
{
    GWENI_CONTROL(RangeSlider, Base);

public:
    enum class ValueTextMode
    {
        PerThumb,
        Joined
    };

    //! A thumb's length along the track (a Slider's thumb's).
    static constexpr int ThumbLength=15;
    //! The gap between a vertical slider's track column and its text column.
    static constexpr int ValueTextGap=Slider::ValueTextGap;
    //! What a Joined text puts between the two values: " – " (an en dash).
    static const char *JoinedSeparator;

    //! Asks for its request, as a Slider does: the track is drawn by its skin
    //! and the thumbs are placed by it.
    bool sizedByRequest(Dim /*dim*/) const override { return true; }
    //! The request, and with a value text the room for it (as a Slider's).
    void calculateSize(skin::Base *skin, Dim dim) override;
    void layout(skin::Base *skin) override;
    void arrange(skin::Base *skin, Dim dim) override;

    //! Horizontal (the default) or vertical, the minimum at the left or the
    //! bottom.
    void setHorizontal(bool horizontal);
    bool isHorizontal() const { return m_horizontal; }

    //! The range; reversed bounds are swapped. The values are clamped into it
    //! and the gap to its length.
    void setRange(double min, double max);
    double getMin() const { return m_min; }
    double getMax() const { return m_max; }

    //! Sets both values: clamped to the range, put in order, snapped to the
    //! step and spread to the gap (the high one moved up, or the low one down
    //! at the maximum). Fires nothing, and marks nothing when unchanged.
    void setValues(double low, double high);
    double getLow() const { return m_values[0]; }
    double getHigh() const { return m_values[1]; }
    using Base::getValue;
    //! 0 the low value, 1 the high one.
    double getValue(int thumb) const { return m_values[thumb?1:0]; }

    //! The least getHigh()-getLow(), at most the range's length.
    void setMinimumGap(double gap);
    double getMinimumGap() const { return m_gap; }

    //! Every value a multiple of `step` from the minimum (0, the default, is
    //! continuous; a negative step is 0).
    void setStep(double step);
    double getStep() const { return m_step; }

    //! As a Slider's: the notches are drawn, and the values kept on them, only
    //! while clamped to them.
    void setNotchCount(int count);
    int getNotchCount() const { return m_numNotches; }
    void setClampToNotches(bool clamp);
    bool getClampToNotches() const { return m_clampToNotches; }

    //! The grid the values are on: the step, else the notches' spacing while
    //! clamped to them, else 0.
    double getGrid() const;
    //! What a key moves a thumb by: the grid, or a hundredth of the range.
    double getKeyStep() const;

    //! The least and the most `thumb` may take: the range's end on its outer
    //! side, the other thumb's value less (plus) the gap on its inner one.
    double getLowerLimit(int thumb) const;
    double getUpperLimit(int thumb) const;

    //! Moves `thumb` to `value`, kept within its limits and on the grid, as
    //! a drag or a key does: fires onValueChanged and onLowChanged or
    //! onHighChanged on a real change. Returns whether it changed.
    bool moveThumb(int thumb, double value);

    //! Thumbs; 0 the low one, 1 the high one.
    internal::RangeSliderThumb *getThumb(int thumb) const { return m_thumbs[thumb?1:0]; }
    //! The thumb drawn over the other one where they overlap: the one dragged
    //! or focused last (the high one at first).
    int getFrontThumb() const { return m_front; }
    //! The thumb a press is dragging, -1 for none (or, at one value, until the
    //! drag's first move picks one).
    int getDraggedThumb() const { return m_dragged; }

    //! The value at a thumb position along the travel, and back (horizontal: x
    //! from the left; vertical: y from the top).
    double valueAt(int position) const;
    int positionOf(double value) const;
    //! The thumbs' travel: the track's length less a thumb's.
    int getTravel() const;
    //! Where `thumb` is drawn and hit, in this control's own units: at
    //! positionOf() its value along the track, ThumbLength long, across it the
    //! track's column. The thumb controls' own bounds are the whole column.
    Rect getThumbRect(int thumb) const;
    //! The front thumb where its rect is, else the other one's, else this
    //! control (the value texts take no mouse).
    Base *getControlAt(int x, int y, bool onlyIfMouseEnabled=true) override;

    //! Where the track and thumbs are, as a Slider's getTrackBounds(): the
    //! render bounds (in the parent's units, as a renderer draws them), less a
    //! vertical slider's text column.
    Rect getTrackBounds() const;
    //! The fill between the thumbs' centres along the track, across it the
    //! whole track's bounds (the renderer draws it thinner), in the units of
    //! getTrackBounds().
    Rect getFillBounds() const;
    //! A vertical slider's text column width (0 for a horizontal one or with
    //! no text).
    int getValueTextColumn() const { return m_valueColumn; }

    //! As Slider::setValueFormat: an empty format shows no text and takes no
    //! text id; anything Slider::validValueFormat refuses is refused, with one
    //! stderr line. Returns whether it was taken.
    bool setValueFormat(const String &format);
    const String &getValueFormat() const { return m_valueFormat; }
    void setValueTextMode(ValueTextMode mode);
    ValueTextMode getValueTextMode() const { return m_textMode; }
    //! The Text showing `thumb`'s value (PerThumb), or with Joined the one
    //! Text for either index; null with no format.
    internal::Text *getValueTextControl(int thumb) const;
    //! The string shown for `thumb` (the joined one for either), "" with none.
    String getValueText(int thumb) const;
    //! The joined string for two values: "lo – hi".
    String joinedText(double low, double high) const;

    //! Disables the thumbs too: no mouse, keys or focus.
    void setDisabled(bool disabled) override;
    //! The value text's colour, for the state (a Slider's).
    void updateColors() override;

    //! A press on the track: the nearer thumb moves there and drags on.
    void onMouseClickLeft(int x, int y, bool down) override;

    //! From a thumb: its press (down) or release, and the mouse's moves while
    //! a press lasts. `local` is the mouse in this control's coordinates.
    void thumbPressed(int thumb, Point local, bool down);
    void thumbMoved(int thumb, Point local);
    //! From a thumb's key: `steps` key steps (negative toward the minimum).
    void thumbStepped(int thumb, int steps);
    //! From a thumb's Home (toward the minimum) or End.
    void thumbToLimit(int thumb, bool upper);
    //! From a thumb that took the focus: it goes in front.
    void thumbFocused(int thumb);

    //! Fired once per real change of either value from a drag, a press or a
    //! key (never from a set*), `integer` 0 for the low thumb and 1 for the
    //! high one; then onLowChanged or onHighChanged.
    gweni::event::Caller onValueChanged;
    gweni::event::Caller onLowChanged;
    gweni::event::Caller onHighChanged;

protected:
    void onZIndexChanged() override;
    //! The axis coordinate of `local`.
    int along(Point local) const { return m_horizontal?local.x:local.y; }
    //! `value` on the grid, within the range.
    double snap(double value) const;
    //! Puts the values right after a range, gap or grid change, firing nothing.
    void applyConstraints();
    //! Places the thumbs at their values.
    void placeThumbs();
    void placeThumb(int thumb);
    //! Places the value texts (Owner children) at their preferred sizes.
    void placeValueText();
    //! Sets the value texts from the values, those whose string changed.
    void updateValueText();
    //! Makes (or deletes) the Texts the format and mode need.
    void syncValueTextControls();
    //! The front thumb, both thumbs marked when it changes.
    void setFront(int thumb);
    //! Starts dragging `thumb`, the mouse held `hold` units into it.
    void startDrag(int thumb, int hold);

    internal::RangeSliderThumb *m_thumbs[2]={nullptr, nullptr};
    //! Drawn/hit rects from the last placement. Layout does not redraw a
    //! thumb whose geometry stayed the same.
    Rect m_placedThumbRects[2];
    double m_values[2]={0.0, 1.0};
    double m_min=0.0;
    double m_max=1.0;
    double m_gap=0.0;
    double m_step=0.0;
    int m_numNotches=5;
    bool m_clampToNotches=false;
    bool m_horizontal=true;
    int m_front=1;

    //! The drag: the thumb (-1 none), whether it waits for its first move to
    //! pick between two thumbs at one value, the mouse's hold into the thumb
    //! and the press's axis coordinate.
    int m_dragged=-1;
    bool m_pending=false;
    int m_hold=0;
    int m_pressAt=0;

    String m_valueFormat;
    ValueTextMode m_textMode=ValueTextMode::PerThumb;
    //! PerThumb: the low and high texts; Joined: [0] alone.
    internal::Text *m_valueTexts[2]={nullptr, nullptr};
    int m_valueColumn=0;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_rangeslider_h_
