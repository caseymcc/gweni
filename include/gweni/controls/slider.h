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

    //! Dragger does not repaint on press/release; the thumb has a down look.
    void onMouseClickLeft(int x, int y, bool down) override;

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
    void layout(skin::Base *skin) override;

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

    //! Disables the thumb too, so it looks (and drags) disabled with the track.
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

    internal::SliderBar *m_sliderBar;
    bool m_clampToNotches;
    int m_numNotches;
    float m_value;

    float m_min;
    float m_max;

};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_slider_h_
