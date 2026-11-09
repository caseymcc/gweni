/*
  * Gweni demo
  * See license in gweni.h
 */

#include "demo/showcaseButtons.h"
#include <gweni/controls/layout/layout.h>
#include "gweni/controls/button.h"
#include "gweni/controls/checkBox.h"
#include "gweni/controls/groupBox.h"
#include "gweni/controls/label.h"
#include "gweni/controls/radioButtonController.h"
#include "gweni/controls/windowButtons.h"

namespace gweni
{

// Three columns of titled sections filling the page (the shell's centre pane,
// 865x545, or ControlTest's canvas), each section's controls docked Top in it.
// Every state is set here.
GWENI_CONTROL_CONSTRUCTOR(ShowcaseButtonsDemo)
{
    showcase::initPage(this);

    controls::Base *left=showcase::addColumn(this);
    controls::Base *middle=showcase::addColumn(this);
    controls::Base *right=showcase::addColumn(this);

    {
        controls::GroupBox *section=showcase::addSection(left, "Button", false);

        // A toggle that is on draws as an off one (issue #412), so there is none
        // here until it shows.
        showcase::addRow<controls::Button>(section)->setText("Plain");

        controls::Button *disabled=showcase::addRow<controls::Button>(section);

        disabled->setText("Disabled");
        disabled->setDisabled(true);

        controls::Button *image=showcase::addRow<controls::Button>(section);

        // tall enough for the 16 px icon, which the button clips
        image->setHeight(20);
        image->setText("With an image");
        image->setImage("test16.png");
        m_notes.push_back("Button: one disabled, one with an image");
    }
    {
        // a toolbar: text buttons, then buttons that are only an icon
        controls::GroupBox *section=showcase::addSection(left, "Toolbar", false);
        controls::Base *row=showcase::addRow<controls::Base>(section);
        controls::HorizontalLayout *line=new controls::HorizontalLayout();

        line->setSpacing(4);
        row->setLayout(line);

        for(const char *text : {"Cut", "Copy", "Paste"})
        {
            controls::Button *button=row->newChild<controls::Button>();

            button->setText(text);
            button->setSizeFlags({SizeFlag::Shrink, SizeFlag::Fixed});
            button->setHeight(22);
            button->setDisabled(String(text) == "Paste");
        }
        for(int i=0; i < 3; ++i)
        {
            controls::Button *icon=row->newChild<controls::Button>();

            icon->setImage("test16.png", true);
            icon->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
            icon->setSize(24, 22);
        }
        m_notes.push_back("Toolbar: \"Paste\" disabled, three icon-only buttons");
    }
    {
        controls::GroupBox *section=showcase::addSection(left, "CheckBox");

        const struct
        {
            const char *text;
            bool checked;
            bool disabled;
        } rows[]=
        {
            {"Checked", true, false},
            {"Unchecked", false, false},
            {"Disabled, checked", true, true},
            {"Disabled", false, true},
            {"Remember this choice", true, false},
            {"Send crash reports", false, false},
            {"Check for updates", true, false},
            {"Show line endings", false, false},
            {"Indent with tabs", true, false},
        };

        for(const auto &row : rows)
        {
            controls::CheckBoxWithLabel *check=showcase::addRow<controls::CheckBoxWithLabel>(section);

            check->label()->setText(row.text);
            check->checkBox()->setChecked(row.checked);
            check->setDisabled(row.disabled);
        }
        // CheckBoxes with no label, as in a table's row
        controls::Base *boxes=showcase::addRow<controls::Base>(section);
        controls::HorizontalLayout *boxLine=new controls::HorizontalLayout();

        boxLine->setSpacing(8);
        boxes->setLayout(boxLine);

        for(int i=0; i < 6; ++i)
        {
            controls::CheckBox *check=boxes->newChild<controls::CheckBox>();

            check->setChecked(i%3 != 1);
        }
        m_notes.push_back("CheckBox: checked, unchecked, disabled labels, and alone in a row");
    }
    {
        controls::GroupBox *section=showcase::addSection(middle, "RadioButton", false);

        {
            controls::Label *caption=showcase::addRow<controls::Label>(section);

            caption->setText("Size");

            controls::RadioButtonController *radio=showcase::addRow<controls::RadioButtonController>(section);

            radio->addOption("Small");
            radio->addOption("Medium")->select();
            radio->addOption("Large");
            radio->addOption("Extra large");
        }
        {
            controls::Label *caption=showcase::addRow<controls::Label>(section);

            caption->setText("Quality");

            controls::RadioButtonController *radio=showcase::addRow<controls::RadioButtonController>(section);

            radio->addOption("Draft");
            radio->addOption("Normal");
            radio->addOption("Best")->select();
            radio->addOption("Lossless");
        }
        {
            controls::Label *caption=showcase::addRow<controls::Label>(section);

            caption->setText("Theme");

            controls::RadioButtonController *radio=showcase::addRow<controls::RadioButtonController>(section);

            radio->addOption("Light");
            radio->addOption("Dark")->select();
            radio->addOption("Follow the system");
        }
        m_notes.push_back("RadioButton: three groups, \"Medium\", \"Best\" and \"Dark\" chosen");
    }
    {
        controls::GroupBox *section=showcase::addSection(middle, "Window buttons", false);

        // minimise, maximise, maximised (restore) and close, at the size a
        // WindowControl's title bar gives its close button
        for(bool disabled : {false, true})
        {
            controls::Base *row=showcase::addRow<controls::Base>(section);
            controls::HorizontalLayout *line=new controls::HorizontalLayout();

            line->setSpacing(8);
            row->setLayout(line);

            controls::Label *label=row->newChild<controls::Label>();

            label->setText(disabled ? "Disabled" : "Enabled");
            label->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
            label->setSize(70, 24);
            label->setTextAlignment(Alignment::Left|Alignment::CenterV);

            controls::WindowCloseButton *buttons[]=
            {
                row->newChild<controls::WindowMinimizeButton>(),
                row->newChild<controls::WindowMaximizeButton>(),
                row->newChild<controls::WindowMaximizeButton>(),
                row->newChild<controls::WindowCloseButton>(),
            };

            static_cast<controls::WindowMaximizeButton *>(buttons[2])->setMaximized(true);

            for(controls::WindowCloseButton *button : buttons)
            {
                button->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
                button->setSize(29, 24);
                button->setDisabled(disabled);
            }
        }
        m_notes.push_back("Window buttons: one maximised (restore), a disabled row");
    }
    {
        // a dialog's buttons, sharing a row; Apply waits for a change
        controls::GroupBox *section=showcase::addSection(middle, "Dialog buttons");

        controls::Base *row=showcase::addRow<controls::Base>(section);
        controls::HorizontalLayout *line=new controls::HorizontalLayout();

        line->setSpacing(6);
        row->setLayout(line);

        for(const char *text : {"OK", "Cancel", "Apply"})
        {
            controls::Button *button=row->newChild<controls::Button>();

            button->setText(text);
            button->setSizeFlags({SizeFlag::Bloat, SizeFlag::Shrink});
            button->setDisabled(String(text) == "Apply");
        }
        m_notes.push_back("Dialog buttons: \"Apply\" disabled");
    }
    {
        controls::GroupBox *section=showcase::addSection(right, "Selectable", false);
        const char *const rows[]={"Show hidden files", "Word wrap", "Line numbers", "Minimap", "Auto save", "Spell check", "Format on save"};

        for(const char *text : rows)
        {
            controls::Selectable *row=showcase::addRow<controls::Selectable>(section);

            row->setText(text);
            if(text == rows[1] || text == rows[4])
                row->setSelected(true, false);
        }

        controls::Selectable *disabled=showcase::addRow<controls::Selectable>(section);

        disabled->setText("Disabled row");
        disabled->setDisabled(true);
        m_notes.push_back("Selectable: \"Word wrap\" and \"Auto save\" on, one row disabled");
    }
    {
        controls::GroupBox *section=showcase::addSection(right, "SelectableGroup");

        for(const char *text : {"Draft", "In review", "Changes requested", "Approved", "Scheduled", "Published", "Archived"})
        {
            controls::Selectable *row=showcase::addRow<controls::Selectable>(section);

            row->setText(text);
            m_group.add(row);
        }
        m_group.getMembers()[3]->setSelected(true, false);
        m_notes.push_back("SelectableGroup: \"Approved\" selected");
    }
}

}//namespace gweni
