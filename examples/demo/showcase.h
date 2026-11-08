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
//! pane (DemoCenter, 865x545 at the shell's 1024x768); tools/capture_samples.sh
//! captures the gallery pages at that size, so a page lays out the same in both.
//!
//! The states a page sets in its constructor, one line each, for the shell's
//! Output pane when the page is shown, and what the page does once shown.
class ShowcaseNotes
{
public:
    virtual ~ShowcaseNotes()=default;

    virtual const std::vector<String> &showcaseNotes() const=0;

    //! Called by the shell after it has shown the page, for a state a hidden
    //! control cannot keep, such as the keyboard focus.
    virtual void showcaseShown() {}
};

namespace showcase
{

//! What every showcase page does first: dock to fill its host (the shell's
//! page or ControlTest's canvas) and lay its sections out in a row.
void initPage(controls::Base *page);

//! A column of the page's row: grows and shrinks with the page.
controls::Base *addColumn(controls::Base *page);

//! A titled frame stacked in a column, growing and shrinking with it; its
//! content docks Center in it. With \a fill false it is as high as its
//! content, and the column's other sections share what is left.
controls::GroupBox *addSection(controls::Base *column, const String &title, bool fill=true);

//! A new child of \a parent docked Top, below the ones added before it with a
//! gap: the width of \a parent and its own height. A control with a size of
//! its own from its constructor (a Slider) takes no more of a layout cell
//! than that, but takes all of a dock.
template<typename T>
T *addRow(controls::Base *parent)
{
    T *row=parent->newChild<T>();

    row->setDock(DockPosition::Top);
    row->setMargin(Margin(0, 0, 0, 4));
    return row;
}

}//namespace showcase

}//namespace gweni

#endif//_gweni_showcase_example_h_
