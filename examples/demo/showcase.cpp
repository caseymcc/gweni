/*
  * Gweni demo
  * See license in gweni.h
 */

#include "demo/showcase.h"
#include <gweni/controls/layout/layout.h>
#include "gweni/controls/groupBox.h"

namespace gweni
{
namespace showcase
{

void initPage(controls::Base *page)
{
    // Docked, so it fills the shell's page and ControlTest's canvas alike; the
    // alignment the shell gives its pages only places a dock-None control.
    page->setDock(DockPosition::Center);

    controls::HorizontalLayout *row=new controls::HorizontalLayout();

    row->setPadding(Padding(6, 6, 6, 6));
    row->setSpacing(6);
    page->setLayout(row);
}

controls::Base *addColumn(controls::Base *page)
{
    controls::Base *column=page->newChild<controls::Base>();
    controls::VerticalLayout *stack=new controls::VerticalLayout();

    stack->setSpacing(6);
    column->setLayout(stack);
    column->setSizeFlags({SizeFlag::Bloat, SizeFlag::Bloat});
    return column;
}

controls::GroupBox *addSection(controls::Base *column, const String &title)
{
    // A GroupBox is a Label, whose constructor asks for a size, and a control
    // that asked for one takes no more of a layout cell than that or its
    // content (docs/layout.md); a plain Base asks for none and fills its cell,
    // and the frame docked in it fills the Base.
    controls::Base *cell=column->newChild<controls::Base>();

    cell->setSizeFlags({SizeFlag::Bloat, SizeFlag::Bloat});

    controls::GroupBox *section=cell->newChild<controls::GroupBox>();

    section->setText(title);
    section->setDock(DockPosition::Center);
    return section;
}

}//namespace showcase
}//namespace gweni
