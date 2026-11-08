/*
  * Gweni demo
  * See license in gweni.h
 */

#include "demo/showcaseDisplay.h"
#include <gweni/controls/layout/layout.h>
#include "gweni/controls/bullet.h"
#include "gweni/controls/button.h"
#include "gweni/controls/groupBox.h"
#include "gweni/controls/label.h"
#include "gweni/controls/progressBar.h"
#include "gweni/controls/separator.h"
#include "gweni/controls/statusBar.h"
#include "gweni/controls/virtualList.h"

#include <string>

namespace gweni
{

// Three columns of titled sections filling the page (the shell's centre pane,
// 865x545, or ControlTest's canvas), each section's controls docked Top in it.
// Every state is set here.
GWENI_CONTROL_CONSTRUCTOR(ShowcaseDisplayDemo)
{
    showcase::initPage(this);

    controls::Base *left=showcase::addColumn(this);
    controls::Base *middle=showcase::addColumn(this);
    controls::Base *right=showcase::addColumn(this);

    {
        controls::GroupBox *section=showcase::addSection(left, "Label", false);

        // 14 px is coverage text, 18 px and up the SDF atlas
        const float sizes[]={14, 18, 24, 32};

        for(size_t i=0; i < 4; ++i)
        {
            m_fonts[i]=getSkin()->getDefaultFont();
            m_fonts[i].size=sizes[i];

            controls::Label *label=showcase::addRow<controls::Label>(section);

            label->setFont(m_fonts[i]);
            label->setText("Label at "+std::to_string(static_cast<int>(sizes[i]))+" px");
        }

        const struct
        {
            const char *text;
            Color color;
        } colored[]=
        {
            {"Accent blue", Color(122, 162, 247, 255)},
            {"Success green", Color(158, 206, 106, 255)},
            {"Warning orange", Color(255, 158, 100, 255)},
            {"Error red", Color(247, 118, 142, 255)},
        };

        for(const auto &entry : colored)
        {
            controls::Label *label=showcase::addRow<controls::Label>(section);

            label->setText(entry.text);
            label->setTextColor(entry.color);
        }
        m_notes.push_back("Label: 14 to 32 px, four colours");
    }
    {
        controls::GroupBox *section=showcase::addSection(left, "Bullet", false);

        for(const char *text : {"Layout runs in the update phase", "Only what changed is drawn",
            "An idle frame uploads nothing", "Any backend implements renderer::Base"})
            showcase::addRow<controls::Bullet>(section)->setText(text);
        m_notes.push_back("Bullet: four points");
    }
    {
        controls::GroupBox *section=showcase::addSection(left, "Wrapped label");
        controls::Label *text=showcase::addRow<controls::Label>(section);

        text->setWrap(true);
        text->setText("A wrapped Label breaks its text at the width its parent gives it, "
            "and wraps again when that width changes.\n\n"
            "Its lines are laid out once per change: a frame in which nothing "
            "changed lays out and draws nothing.");
        m_notes.push_back("Label: wrapped at its section's width");
    }
    {
        controls::GroupBox *section=showcase::addSection(middle, "ProgressBar", false);

        const struct
        {
            float progress;
            const char *text;
        } bars[]=
        {
            {0.27f, nullptr},
            {0.66f, nullptr},
            {1.00f, nullptr},
            {0.20f, "40,245 MB of 201,226 MB"},
            {0.00f, "Waiting..."},
        };

        for(const auto &entry : bars)
        {
            controls::ProgressBar *bar=showcase::addRow<controls::ProgressBar>(section);

            bar->setHeight(20);
            if(entry.text)
            {
                bar->setAutoLabel(false);
                bar->setText(entry.text);
            }
            bar->setProgress(entry.progress);
        }

        // work of unknown length, its segment moving along the track
        controls::ProgressBar *indeterminate=showcase::addRow<controls::ProgressBar>(section);

        indeterminate->setHeight(20);
        indeterminate->setIndeterminate(true);
        m_notes.push_back("ProgressBar: 27 %, 66 %, full, custom text, one indeterminate");
    }
    {
        // side by side from the left, each the section's height
        controls::GroupBox *section=showcase::addSection(middle, "Vertical ProgressBar");

        const struct
        {
            float progress;
            bool indeterminate;
        } bars[]=
        {
            {0.10f, false},
            {0.25f, false},
            {0.50f, false},
            {0.80f, false},
            {1.00f, false},
            {0.40f, true},
        };

        for(const auto &entry : bars)
        {
            controls::ProgressBar *bar=section->newChild<controls::ProgressBar>();

            // an indeterminate bar has no label to be as wide as
            bar->setSizeFlags({SizeFlag::Fixed, SizeFlag::Expand});
            bar->setDock(DockPosition::Left);
            bar->setWidth(27);
            bar->setMargin(Margin(0, 0, 12, 0));
            bar->setVertical();
            bar->setProgress(entry.progress);
            bar->setIndeterminate(entry.indeterminate);
        }
        m_notes.push_back("ProgressBar: vertical from 10 % to full, and indeterminate");
    }
    {
        // it takes what the fixed-height list below leaves
        controls::GroupBox *section=showcase::addSection(right, "Separator");

        showcase::addRow<controls::Label>(section)->setText("A label above a plain rule");
        showcase::addRow<controls::Separator>(section);
        showcase::addRow<controls::Separator>(section)->setText("Captioned rule");

        // two buttons with a vertical rule between them
        controls::Base *row=showcase::addRow<controls::Base>(section);
        controls::HorizontalLayout *line=new controls::HorizontalLayout();

        line->setSpacing(8);
        row->setLayout(line);
        row->newChild<controls::Button>()->setText("Left");

        controls::Separator *vertical=row->newChild<controls::Separator>();

        vertical->setVertical();
        row->newChild<controls::Button>()->setText("Right");
        m_notes.push_back("Separator: plain, captioned and vertical");
    }
    {
        // As high as the section has room for in the shell's centre pane, and
        // no higher in a larger page: a list whose viewport grows and shrinks
        // back keeps its spare row's old bounds, so a resize round trip would
        // not come back to where it started (issue #415).
        controls::GroupBox *section=showcase::addSection(right, "VirtualList, 100 000 items", false);
        controls::VirtualList *list=showcase::addRow<controls::VirtualList>(section);

        list->setHeight(379);
        list->setMargin(Margin(0, 0, 0, 0));
        list->setRowBinder([](controls::Base *row, size_t index)
            {
                if(controls::Label *label=gweni_cast<controls::Label>(row))
                    label->setText("Item "+std::to_string(index));
            });
        list->setItemCount(100000);
        list->scrollToItem(50000);
        m_notes.push_back("VirtualList: 100 000 items, scrolled to Item 50000");
    }
    {
        controls::GroupBox *section=showcase::addSection(right, "StatusBar", false);
        controls::StatusBar *status=showcase::addRow<controls::StatusBar>(section);

        status->setText("Ready");

        controls::Label *position=status->newChild<controls::Label>();

        position->setText("Ln 12, Col 4");
        status->addControl(position, true);
        m_notes.push_back("StatusBar: \"Ready\", a label on the right");
    }
}

}//namespace gweni
