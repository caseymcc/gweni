/*
  * Gweni demo
  * See license in gweni.h
 */

#ifndef _gweni_showcaseWindows_example_h_
#define _gweni_showcaseWindows_example_h_

#include "demo/showcase.h"

namespace gweni
{

//! Windows & menus: a MenuStrip, two WindowControls over the page's content, a
//! box with a context menu (opened by a right-click, as the gallery capture
//! does) and the colour pickers, each in a state set here.
class ShowcaseWindowsDemo: public controls::Base, public ShowcaseNotes
{
public:
    GWENI_CONTROL(ShowcaseWindowsDemo, controls::Base);

    const std::vector<String> &showcaseNotes() const override { return m_notes; }

private:
    std::vector<String> m_notes;
};

}//namespace gweni

#endif//_gweni_showcaseWindows_example_h_
