#include "demo/switch.h"
#include "gweni/controls/button.h"
#include "gweni/controls/label.h"
#include "gweni/controls/layout/layout.h"
#include "gweni/controls/switch.h"

namespace gweni
{

namespace
{

// A section's caption, docked Top in the column.
void caption(controls::Base *column, const char *text)
{
    controls::Label *label=column->newChild<controls::Label>();

    label->setText(text);
    label->setDock(DockPosition::Top);
    label->setMargin(Margin(0, 8, 0, 4));
}

// A row of controls side by side, docked Top in the column.
controls::Base *line(controls::Base *column)
{
    controls::Base *row=column->newChild<controls::Base>();
    controls::HorizontalLayout *layout=new controls::HorizontalLayout();

    layout->setSpacing(16);
    row->setLayout(layout);
    row->setDock(DockPosition::Top);
    return row;
}

// The rest of a row's width, after its controls, so they stay packed.
void endLine(controls::Base *row)
{
    controls::Base *rest=row->newChild<controls::Base>();

    rest->setSizeFlags({SizeFlag::Bloat, SizeFlag::Fixed});
    rest->setHeight(1);
}

controls::Switch *newSwitch(controls::Base *row, const char *text, bool on)
{
    controls::Switch *control=row->newChild<controls::Switch>();

    if(text)
        control->setText(text);
    control->setOn(on);
    return control;
}

}//namespace

GWENI_CONTROL_CONSTRUCTOR(SwitchDemo)
{
    setDock(DockPosition::Center);

    controls::Base *column=newChild<controls::Base>();

    column->setSizeFlags({SizeFlag::Fixed, SizeFlag::Shrink});
    column->setPos(10, 4);
    column->setWidth(500);

    caption(column, "Off and on");
    {
        controls::Base *row=line(column);

        m_off=newSwitch(row, nullptr, false);
        m_on=newSwitch(row, nullptr, true);
        endLine(row);
    }

    caption(column, "With a label, on either side");
    {
        controls::Base *row=line(column);

        m_labelRight=newSwitch(row, "Label on the right", false);
        m_labelLeft=newSwitch(row, "Label on the left", true);
        m_labelLeft->setLabelOnLeft(true);
        endLine(row);
    }

    caption(column, "Disabled");
    {
        controls::Base *row=line(column);

        m_disabledOff=newSwitch(row, "Disabled, off", false);
        m_disabledOff->setDisabled(true);
        m_disabledOn=newSwitch(row, "Disabled, on", true);
        m_disabledOn->setDisabled(true);
        endLine(row);
    }

    caption(column, "Enabling another control");
    {
        controls::Base *row=line(column);

        m_enabling=newSwitch(row, "Allow apply", false);
        m_button=row->newChild<controls::Button>();
        m_button->setText("Apply");
        m_button->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        m_button->setSize(80, 22);
        m_button->setDisabled(true);
        m_enabling->onToggled.add(this, &SwitchDemo::onEnablingToggled);
        endLine(row);
    }
}

void SwitchDemo::onEnablingToggled(event::Info /*info*/)
{
    m_button->setDisabled(!m_enabling->isOn());
}

}//namespace gweni
