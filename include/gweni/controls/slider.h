/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_slider_h_
#define _gweni_controls_slider_h_

#include <gweni/controls/base.h>
#include <gweni/controls/button.h>
#include <gweni/controls/dragger.h>
#include <gweni/controls/text.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>

namespace gweni
{
namespace controls
{
namespace internal
{

class GWENI_EXPORT SliderBar: public internal::Dragger
{
    GWENI_CONTROL(SliderBar, internal::Dragger);

public:
    //void render(skin::Base *skin) override;
    virtual void setHorizontal(bool b)
    {
        m_horizontal=b;
    }

    virtual bool isHorizontal()
    {
        return m_horizontal;
    }

protected:

    bool m_horizontal=true;
};

}//namespace internal

class GWENI_EXPORT Slider: public Base
{
    GWENI_CONTROL(Slider, Base);

public:
    //void render(skin::Base *skin) override=0;
    //! A slider asks for the size it was asked to be (its constructor's
    //! default, or the app's): its track is drawn by its skin, and its thumb
    //! is placed by it (Placement::Owner).
    bool sizedByRequest(Dim /*dim*/) const override { return true; }
    //! The request, and with a value text (setValueFormat()) the room for it:
    //! a horizontal slider at least the text's line high, a vertical one wider
    //! by ValueTextGap and the text's column (getValueTextColumn()).
    void calculateSize(skin::Base *skin, Dim dim) override;
    void layout(skin::Base *skin) override;

    //! Shows the value as text (imgui's SliderFloat), formatted by `format`
    //! with snprintf: e.g. "%.0f", "%.2f", "%.0f %%". A horizontal slider
    //! draws it centred over its track and thumb, a vertical one in a column
    //! right of its track, vertically centred. The text is a controls::internal::Text child
    //! numbered after the thumb, set (and StateChange_Text raised) only when
    //! the formatted string changes, so a drag that keeps the string writes no
    //! glyph. An empty format, the default, shows no text and takes no text id.
    //! `format` must hold exactly one conversion of a double (one of
    //! f F e E g G a A, with optional flags "-+ #0", a width and a precision of
    //! at most 2 digits each, no '*' and no length modifier), plus any text and
    //! "%%": anything else is refused (validValueFormat()), with one line on
    //! stderr, and the format is left as it was. Returns whether it was taken.
    bool setValueFormat(const String &format);
    const String &getValueFormat() const { return m_valueFormat; }
    //! The string shown, empty with no format.
    String getValueText() const;
    //! The Text child showing the value, null with no format.
    internal::Text *getValueTextControl() { return m_valueText; }
    //! Whether setValueFormat() takes `format`.
    static bool validValueFormat(const String &format);
    //! `value` formatted by a valid `format` (validValueFormat()), else "".
    static String formatValue(const String &format, double value);

    //! Where the track and thumb are, in this control's own coordinates: the
    //! whole control, but for a vertical slider with a value text the column
    //! left of the text (getWidth() less ValueTextGap and the text's column).
    gweni::Rect getTrackBounds();
    //! The width of a vertical slider's text column: the wider of the
    //! formatted min and max (so the slider does not change width while it is
    //! dragged), 0 with no value text or for a horizontal slider.
    int getValueTextColumn() const { return m_valueColumn; }

    //! The gap between a vertical slider's track column and its text column.
    static constexpr int ValueTextGap=4;

    //! The notches are drawn only while the value is clamped to them.
    virtual void setClampToNotches(bool bClamp)
    {
        if(m_clampToNotches == bClamp)
            return;

        m_clampToNotches=bClamp;
        redraw();
    }

    virtual bool getClampToNotches()
    {
        return m_clampToNotches;
    }

    virtual void setNotchCount(int num)
    {
        if(m_numNotches == num)
            return;

        m_numNotches=num;
        redraw();
    }

    virtual int getNotchCount()
    {
        return m_numNotches;
    }

    //! Disables the thumb too, so it looks (and drags) disabled with the track,
    //! and draws the value text in the disabled text colour.
    void setDisabled(bool active) override;

    virtual bool isHorizontal()
    {
        return m_sliderBar->isHorizontal();
    }

    //! The thumb, whose size along the axis insets the track.
    virtual internal::SliderBar *getSliderBar()
    {
        return m_sliderBar;
    }

    //! A new range changes the value shown, and the text column's width.
    virtual void  setRange(float fMin, float fMax);
    virtual float getFloatValue();
    virtual void  setFloatValue(float val, bool forceUpdate=true);

    virtual float calculateValue();
    virtual void  onMoved(event::Info info);

    void onMouseClickLeft(int /*x*/, int /*y*/, bool /*down*/) override
    {}

    bool onKeyRight(bool down) override
    {
        if(down)
            setFloatValue(getFloatValue()+1, true);

        return true;
    }

    bool onKeyLeft(bool down) override
    {
        if(down)
            setFloatValue(getFloatValue()-1, true);

        return true;
    }

    bool onKeyUp(bool down) override
    {
        if(down)
            setFloatValue(getFloatValue()+1, true);

        return true;
    }

    bool onKeyDown(bool down) override
    {
        if(down)
            setFloatValue(getFloatValue()-1, true);

        return true;
    }

    void renderFocus(gweni::skin::Base *skin) override;

    gweni::event::Caller onValueChanged;

    virtual float getMin()
    {
        return m_min;
    }

    virtual float getMax()
    {
        return m_max;
    }

protected:

    virtual void setValueInternal(float value);
    virtual void updateBarFromValue() {}
    //! Sets the value text from the value, if its string changed.
    void updateValueText();
    //! Places the value text (an Owner child) at its preferred size: centred
    //! in a horizontal slider, in the column of a vertical one. Called from
    //! layout().
    void placeValueText();

    internal::SliderBar *m_sliderBar;
    bool m_clampToNotches;
    int m_numNotches;
    float m_value;

    float m_min;
    float m_max;

    String m_valueFormat;
    internal::Text *m_valueText=nullptr;
    //! A vertical slider's text column, from its last X measure.
    int m_valueColumn=0;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_slider_h_
