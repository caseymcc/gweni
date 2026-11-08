#include "demo/tabClose.h"
#include "gweni/controls/label.h"
#include "gweni/controls/tabControl.h"

namespace gweni
{

namespace
{

controls::TabButton *addTab(controls::TabControl *tabs, const char *caption, bool closable)
{
    controls::TabButton *tab=tabs->addPage(caption);
    controls::Label *label=tab->getPage()->newChild<controls::Label>();

    label->setText(String("The page of ")+caption);
    label->setDock(DockPosition::Top);
    tab->setClosable(closable);
    return tab;
}

}//namespace

// Closable tabs (TabButton::setClosable): a document-style TabControl whose
// first tab, "Start", is pinned and the rest closable, opened on the second;
// and one with its strip on the left. A tab's close button shows on the
// selected tab and the hovered one, and a click on it closes the tab and
// selects its neighbour.
GWENI_CONTROL_CONSTRUCTOR(TabCloseDemo)
{
    setDock(DockPosition::Center);

    {
        controls::TabControl *tabs=newChild<controls::TabControl>();
        tabs->setBounds(10, 10, 360, 160);

        addTab(tabs, "Start", false);
        controls::TabButton *opened=addTab(tabs, "main.cpp", true);
        addTab(tabs, "README.md", true);
        addTab(tabs, "notes.txt", true);
        opened->doAction();
    }
    {
        controls::TabControl *tabs=newChild<controls::TabControl>();
        tabs->setBounds(10, 180, 360, 160);
        tabs->setTabStripPosition(DockPosition::Left);

        addTab(tabs, "Output", true);
        addTab(tabs, "Problems", true);
        addTab(tabs, "Terminal", true);
    }
}

}//namespace gweni
