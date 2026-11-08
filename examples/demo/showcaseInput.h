/*
  * Gweni demo
  * See license in gweni.h
 */

#ifndef _gweni_showcaseInput_example_h_
#define _gweni_showcaseInput_example_h_

#include "demo/showcase.h"

namespace gweni
{
namespace controls
{
class TextBox;
}

//! Text input: TextBoxes (a placeholder, a password, a number, coloured text,
//! one focused), a TextBoxMultiline, ComboBoxes and sliders, each in a state
//! set here.
class ShowcaseInputDemo: public controls::Base, public ShowcaseNotes
{
public:
    GWENI_CONTROL(ShowcaseInputDemo, controls::Base);

    const std::vector<String> &showcaseNotes() const override { return m_notes; }

    //! Focuses m_focused again: the shell builds its pages hidden, and a hidden
    //! control loses the focus on the canvas's next frame.
    void showcaseShown() override;

private:
    std::vector<String> m_notes;
    //! the box holding the keyboard focus, its caret showing
    controls::TextBox *m_focused=nullptr;
};

}//namespace gweni

#endif//_gweni_showcaseInput_example_h_
