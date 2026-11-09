#include "demo/statsOverlay.h"
#include "gweni/controls/button.h"
#include "gweni/controls/checkBox.h"
#include "gweni/controls/groupBox.h"
#include "gweni/controls/listBox.h"
#include "gweni/tools/statsOverlay.h"

namespace gweni
{

// The app part on the left, the overlay in the page's bottom-right corner. It
// is the demo's own child here, not the canvas's (StatsOverlay::open() makes
// one there), so the page shows and hides it with the rest; it shows the whole
// canvas's frames all the same. Hover a row or a button and its counts blip on
// the next refresh, then drop back to 0.
GWENI_CONTROL_CONSTRUCTOR(StatsOverlayDemo)
{
    setDock(DockPosition::Center);

    controls::GroupBox *group=newChild<controls::GroupBox>();
    group->setText("Settings");
    group->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    group->setPos(10, 10);
    group->setSize(260, 130);

    controls::Button *save=group->newChild<controls::Button>("save");
    save->setText("Save");
    save->setPos(10, 10);

    controls::Button *revert=group->newChild<controls::Button>("revert");
    revert->setText("Revert");
    revert->setPos(120, 10);

    controls::CheckBoxWithLabel *check=group->newChild<controls::CheckBoxWithLabel>("autosave");
    check->label()->setText("Autosave");
    check->setPos(10, 45);

    controls::ListBox *list=newChild<controls::ListBox>("files");
    list->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    list->setPos(10, 150);
    list->setSize(260, 160);
    for(const char *name : {"atlas.png", "build.ninja", "CMakeLists.txt", "LICENSE", "main.cpp", "README.md"})
        list->addItem(name);

    m_overlay=newChild<tools::StatsOverlay>();
}

}//namespace gweni
