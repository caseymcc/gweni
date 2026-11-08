/*
  * Gweni demo
  * See license in gweni.h
 */

#ifndef _gweni_showcaseButtons_example_h_
#define _gweni_showcaseButtons_example_h_

#include "demo/showcase.h"
#include <gweni/controls/selectable.h>

namespace gweni
{

//! Buttons & toggles: Buttons, CheckBoxes, a RadioButton group, Selectable
//! rows with a single-selection group, and the window buttons, each in a state
//! set here.
class ShowcaseButtonsDemo: public controls::Base, public ShowcaseNotes
{
public:
    GWENI_CONTROL(ShowcaseButtonsDemo, controls::Base);

    const std::vector<String> &showcaseNotes() const override { return m_notes; }

private:
    std::vector<String> m_notes;
    //! the Selectable rows of which one at a time is selected
    controls::SelectableGroup m_group;
};

}//namespace gweni

#endif//_gweni_showcaseButtons_example_h_
