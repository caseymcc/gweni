/*
  * Gweni demo
  * See license in gweni.h
 */

#ifndef _gweni_showcaseDisplay_example_h_
#define _gweni_showcaseDisplay_example_h_

#include "demo/showcase.h"

namespace gweni
{

//! Display & feedback: Labels at several sizes and colours, Bullets,
//! ProgressBars (labelled, vertical, indeterminate), Separators, a VirtualList
//! and a StatusBar, each in a state set here.
class ShowcaseDisplayDemo: public controls::Base, public ShowcaseNotes
{
public:
    GWENI_CONTROL(ShowcaseDisplayDemo, controls::Base);

    const std::vector<String> &showcaseNotes() const override { return m_notes; }

private:
    std::vector<String> m_notes;
    //! the Labels' fonts, which a Text keeps a pointer to
    Font m_fonts[4];
};

}//namespace gweni

#endif//_gweni_showcaseDisplay_example_h_
