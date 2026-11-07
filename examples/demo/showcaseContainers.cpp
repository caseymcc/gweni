/*
  * Gweni demo
  * See license in gweni.h
 */

#include "demo/showcaseContainers.h"
#include "gweni/controls/button.h"
#include "gweni/controls/collapsibleList.h"
#include "gweni/controls/groupBox.h"
#include "gweni/controls/label.h"
#include "gweni/controls/listBox.h"
#include "gweni/controls/properties.h"
#include "gweni/controls/property/checkBox.h"
#include "gweni/controls/property/comboBox.h"
#include "gweni/controls/scrollControl.h"
#include "gweni/controls/tabControl.h"
#include "gweni/controls/treeControl.h"

namespace gweni
{

// Three columns of titled sections, laid out to fill the page: the shell's
// centre pane (870x545), where it is the README's hero, or ControlTest's canvas.
// Every state is set here, since the hero capture takes no input.
GWENI_CONTROL_CONSTRUCTOR(ShowcaseContainersDemo)
{
    showcase::initPage(this);

    controls::Base *left=showcase::addColumn(this);
    controls::Base *middle=showcase::addColumn(this);
    controls::Base *right=showcase::addColumn(this);

    {
        controls::TreeControl *tree=showcase::addSection(left, "TreeControl")->newChild<controls::TreeControl>();

        tree->setDock(DockPosition::Center);

        controls::TreeNode *assets=tree->addNode("Assets");
        controls::TreeNode *textures=assets->addNode("Textures");

        textures->addNode("brick.png");
        textures->addNode("grass.png");
        assets->addNode("Meshes")->addNode("crate.obj");
        assets->addNode("Audio");

        controls::TreeNode *source=tree->addNode("Source");

        source->addNode("main.cpp");
        source->addNode("ui.cpp");
        tree->addNode("README.md");
        tree->expandAll();
        textures->setSelected(true);
        m_notes.push_back("TreeControl: expanded, \"Textures\" selected");
    }
    {
        controls::ListBox *list=showcase::addSection(left, "ListBox")->newChild<controls::ListBox>();

        list->setDock(DockPosition::Center);
        list->setColumnCount(2);

        const char *const rows[][2]=
        {
            {"Mercury", "0.39 AU"},
            {"Venus", "0.72 AU"},
            {"Earth", "1.00 AU"},
            {"Mars", "1.52 AU"},
            {"Jupiter", "5.20 AU"},
            {"Saturn", "9.54 AU"},
        };

        for(const auto &row : rows)
        {
            controls::layout::TableRow *item=list->addItem(row[0]);

            item->setCellText(1, row[1]);
            if(String(row[0]) == "Earth")
                list->setSelectedRow(item);
        }
        m_notes.push_back("ListBox: \"Earth\" selected");
    }
    {
        controls::TabControl *tabs=showcase::addSection(middle, "TabControl")->newChild<controls::TabControl>();

        tabs->setDock(DockPosition::Center);

        {
            controls::Label *label=tabs->addPage("General")->getPage()->newChild<controls::Label>();

            label->setText("General settings");
            label->setDock(DockPosition::Top);
        }

        controls::TabButton *propertiesTab=tabs->addPage("Properties");
        controls::Properties *props=propertiesTab->getPage()->newChild<controls::Properties>();

        props->setDock(DockPosition::Center);
        props->add("Name", "Showcase");
        props->add("Opacity", "0.85");
        props->add("Visible", props->newChild<controls::property::CheckBox>(), "1");
        props->add("Locked", props->newChild<controls::property::CheckBox>(), "0");
        {
            controls::property::ComboBox *combo=props->newChild<controls::property::ComboBox>();

            combo->getComboBox()->addItem("Normal", "normal");
            combo->getComboBox()->addItem("Multiply", "multiply");
            combo->getComboBox()->addItem("Screen", "screen");
            props->add("Blend", combo, "multiply");
        }

        {
            controls::Label *label=tabs->addPage("History")->getPage()->newChild<controls::Label>();

            label->setText("No changes yet");
            label->setDock(DockPosition::Top);
        }
        propertiesTab->doAction();
        m_notes.push_back("TabControl: \"Properties\" tab selected");
    }
    {
        controls::ScrollControl *scroll=showcase::addSection(middle, "ScrollControl")->newChild<controls::ScrollControl>();

        scroll->setDock(DockPosition::Center);
        scroll->setScroll(false, true);

        for(int i=1; i <= 12; ++i)
        {
            controls::Button *button=scroll->newChild<controls::Button>();

            button->setText("Row "+std::to_string(i));
            button->setDock(DockPosition::Top);
            button->setMargin(Margin(0, 0, 0, 2));
        }
        m_notes.push_back("ScrollControl: 12 rows, the bar showing");
    }
    {
        controls::CollapsibleList *list=showcase::addSection(right, "CollapsibleList")->newChild<controls::CollapsibleList>();

        list->setDock(DockPosition::Center);

        {
            controls::CollapsibleCategory *cat=list->add("Recent");

            cat->add("Report.odt");
            cat->add("Budget.ods")->setToggleState(true);
            cat->add("Slides.odp");
        }
        {
            controls::CollapsibleCategory *cat=list->add("Favourites");

            cat->add("Home");
            cat->add("Projects");
            cat->setExpanded(false);
        }
        {
            controls::CollapsibleCategory *cat=list->add("Shared");

            cat->add("Team notes");
            cat->add("Roadmap");
            cat->add("Designs");
        }
        m_notes.push_back("CollapsibleList: \"Budget.ods\" selected, \"Favourites\" collapsed");
    }
}

}//namespace gweni
