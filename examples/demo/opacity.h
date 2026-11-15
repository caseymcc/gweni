/*
 *  Gweni demo
 *  See license in gweni.h
 */
#ifndef _gweni_opacity_example_h_
#define _gweni_opacity_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class Button;
class GroupBox;
class HorizontalSlider;
}

//! Control opacity (#686, Base::setOpacity): a panel (a GroupBox) of a few
//! controls over Labels it hides at 1, a slider for its surface level and one
//! for its text level, a child of the panel that sets its own 1 (so it stays
//! opaque whatever the panel's), and a button that fades the panel out and in
//! over FadeSeconds with an animation calling setOpacity(). Placed by hand on
//! a 640x480 canvas. In ControlTest's --list, not the demo shell.
class OpacityDemo: public controls::Base
{
public:
    GWENI_CONTROL(OpacityDemo, controls::Base);

public:
    //! a fade's length, out and back in
    static constexpr float FadeSeconds=1.0f;

    controls::GroupBox *panel() const { return m_panel; }
    controls::HorizontalSlider *surfaceSlider() const { return m_surface; }
    controls::HorizontalSlider *textSlider() const { return m_text; }
    controls::Button *fadeButton() const { return m_fade; }
    //! the panel's child that keeps its own level of 1
    controls::Button *opaqueChild() const { return m_opaque; }

    //! The panel set to the sliders' levels times \p factor.
    void applyLevels(float factor=1.0f);
    //! What the fade button does: the panel faded out and back in over
    //! FadeSeconds (at once to the sliders' levels without GWENI_ANIMATE).
    void startFade();

private:
    void onLevelChanged(event::Info info);
    void onFade(event::Info info);

    controls::GroupBox *m_panel=nullptr;
    controls::HorizontalSlider *m_surface=nullptr;
    controls::HorizontalSlider *m_text=nullptr;
    controls::Button *m_fade=nullptr;
    controls::Button *m_opaque=nullptr;
};

}//namespace gweni

#endif//_gweni_opacity_example_h_
