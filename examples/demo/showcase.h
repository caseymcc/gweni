/*
  * Gweni demo
  * See license in gweni.h
 */

#ifndef _gweni_showcase_example_h_
#define _gweni_showcase_example_h_

#include <gweni/controls/base.h>

#include <vector>

namespace gweni
{
namespace controls
{
class GroupBox;
}

//! The showcase pages: many controls on one page, each in a state it is not in
//! by default, filling their host. In the demo shell a page fills the centre
//! pane (DemoCenter, 870x545 at the shell's 1024x768); tools/capture_samples.sh
//! captures the gallery pages at that size, so a page lays out the same in both.
//!
//! The states a page sets in its constructor, one line each, for the shell's
//! Output pane when the page is shown.
class ShowcaseNotes
{
public:
    virtual ~ShowcaseNotes()=default;

    virtual const std::vector<String> &showcaseNotes() const=0;
};

namespace showcase
{

//! What every showcase page does first: dock to fill its host (the shell's
//! page or ControlTest's canvas) and lay its sections out in a row.
void initPage(controls::Base *page);

//! A column of the page's row: grows and shrinks with the page.
controls::Base *addColumn(controls::Base *page);

//! A titled frame stacked in a column, growing and shrinking with it; its
//! content docks Center in it.
controls::GroupBox *addSection(controls::Base *column, const String &title);

}//namespace showcase

}//namespace gweni

#endif//_gweni_showcase_example_h_
