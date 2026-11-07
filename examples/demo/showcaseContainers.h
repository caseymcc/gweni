/*
  * Gweni demo
  * See license in gweni.h
 */

#ifndef _gweni_showcaseContainers_example_h_
#define _gweni_showcaseContainers_example_h_

#include "demo/showcase.h"

namespace gweni
{

//! Containers & data: a TreeControl, a ListBox, a TabControl holding a
//! Properties grid, a ScrollControl and a CollapsibleList, each in a state set
//! here. The README's hero, shown in the demo shell.
class ShowcaseContainersDemo: public controls::Base, public ShowcaseNotes
{
public:
    GWENI_CONTROL(ShowcaseContainersDemo, controls::Base);

    const std::vector<String> &showcaseNotes() const override { return m_notes; }

private:
    std::vector<String> m_notes;
};

}//namespace gweni

#endif//_gweni_showcaseContainers_example_h_
