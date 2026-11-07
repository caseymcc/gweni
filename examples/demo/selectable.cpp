#include "demo/selectable.h"
#include "gweni/controls/groupBox.h"
#include "gweni/controls/label.h"
#include "gweni/controls/selectable.h"

namespace gweni
{

// Free rows that toggle (one selected, one disabled) on the left, and a
// single-selection group of four in a GroupBox on the right. Every row is
// docked Top in its column, so it is the column's width.
GWENI_CONTROL_CONSTRUCTOR(SelectableDemo)
{
    setDock(DockPosition::Center);

    controls::Base *free=newChild<controls::Base>();

    free->setSizeFlags({SizeFlag::Fixed, SizeFlag::Shrink});
    free->setPos(10, 10);
    free->setWidth(240);

    {
        controls::Label *title=free->newChild<controls::Label>();

        title->setText("Toggle rows");
        title->setDock(DockPosition::Top);
        title->setMargin(Margin(0, 0, 0, 4));
    }

    const char *const freeRows[]={"Show hidden files", "Word wrap", "Line numbers", "Minimap", "Auto save"};

    for(const char *text : freeRows)
    {
        controls::Selectable *row=free->newChild<controls::Selectable>();

        row->setText(text);
        // the second one starts selected
        if(text == freeRows[1])
            row->setSelected(true, false);
    }

    {
        controls::Selectable *disabled=free->newChild<controls::Selectable>();

        disabled->setText("Disabled row");
        disabled->setDisabled(true);
    }

    controls::GroupBox *box=newChild<controls::GroupBox>();

    box->setText("Single selection");
    // a GroupBox's rows are in its inner panel, which its own size does not
    // follow, so it is given one: the caption, four rows and the padding
    box->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    box->setBounds(270, 10, 240, 106);

    const char *const groupRows[]={"Small", "Medium", "Large", "Extra large"};

    for(const char *text : groupRows)
    {
        controls::Selectable *row=box->newChild<controls::Selectable>();

        row->setText(text);
        m_group.add(row);
    }

    m_group.getMembers()[1]->setSelected(true, false);
}

}//namespace gweni
