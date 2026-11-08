#include "demo/tabOverflow.h"
#include "gweni/controls/label.h"
#include "gweni/controls/tabControl.h"

namespace gweni
{

namespace
{

const char *const g_files[]=
{
    "main.cpp", "app.h", "app.cpp", "window.h", "window.cpp", "render.cpp",
    "shaders.glsl", "README.md", "CMakeLists.txt", "notes.txt", "todo.md", "LICENSE"
};

controls::TabControl *addTabs(controls::Base *parent, int x, int y)
{
    controls::TabControl *tabs=parent->newChild<controls::TabControl>();

    // Fixed: as wide as asked, whatever its tabs need, so they overflow it.
    tabs->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    tabs->setBounds(x, y, 300, 140);

    for(const char *name : g_files)
    {
        controls::Label *label=tabs->addPage(name)->getPage()->newChild<controls::Label>();

        label->setText(String("The page of ")+name);
        label->setDock(DockPosition::Top);
    }
    return tabs;
}

}//namespace

// Twelve tabs in a 300-unit TabControl, more than fit, so the scroll buttons
// show at the strip's end. The first, its strip on top, opens on its first tab
// and has the tab-list button, which lists every tab in a Popup; the second,
// its strip at the bottom, opens on its eighth, scrolled into view.
GWENI_CONTROL_CONSTRUCTOR(TabOverflowDemo)
{
    setDock(DockPosition::Center);

    addTabs(this, 10, 10)->setTabListButton(true);

    controls::TabControl *bottom=addTabs(this, 10, 170);

    bottom->setTabStripPosition(DockPosition::Bottom);
    bottom->getTab(7)->doAction();
}

}//namespace gweni
