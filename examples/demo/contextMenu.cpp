#include "demo/contextMenu.h"
#include "gweni/controls/button.h"
#include "gweni/controls/groupBox.h"
#include "gweni/controls/label.h"
#include "gweni/controls/menu.h"

namespace gweni
{

// Two boxes with context menus of their own (Base::createContextMenu): the
// top-left one with a divider and a disabled item, the other in the
// bottom-right corner, where its menu opens flipped to the cursor's left and
// above it. A button with a tooltip sits right of the first box, so a menu
// opened at that box's right edge lies under the tooltip. The label below the
// first box names the item last chosen.
GWENI_CONTROL_CONSTRUCTOR(ContextMenuDemo)
{
    setDock(DockPosition::Center);

    controls::GroupBox *document=newChild<controls::GroupBox>();

    document->setText("Document");
    document->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    document->setBounds(10, 10, 300, 120);

    {
        controls::Label *hint=document->newChild<controls::Label>();

        hint->setText("Right-click in this box");
        hint->setPos(10, 10);
    }

    if(controls::Menu *menu=document->createContextMenu())
    {
        menu->addItem("Cut", "", "Ctrl+X")->setAction(this, &ContextMenuDemo::onChosen);
        menu->addItem("Copy", "", "Ctrl+C")->setAction(this, &ContextMenuDemo::onChosen);
        menu->addItem("Paste", "", "Ctrl+V")->setAction(this, &ContextMenuDemo::onChosen);
        menu->addDivider();
        menu->addItem("Select all")->setAction(this, &ContextMenuDemo::onChosen);
        menu->addItem("Delete")->setDisabled(true);
    }

    controls::Button *help=newChild<controls::Button>();

    help->setText("Help");
    help->setToolTipText("Right-click a box for its menu");
    help->setPos(330, 10);

    m_chosen=newChild<controls::Label>();
    m_chosen->setText("Chosen: nothing yet");
    m_chosen->setPos(10, 140);

    controls::GroupBox *corner=newChild<controls::GroupBox>();

    corner->setText("Corner");
    corner->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    corner->setSize(220, 100);
    corner->setMargin(Margin(0, 0, 10, 10));
    corner->setAlignment(Alignment::Right|Alignment::Bottom);

    {
        controls::Label *hint=corner->newChild<controls::Label>();

        hint->setText("Right-click near the corner");
        hint->setPos(10, 10);
    }

    if(controls::Menu *menu=corner->createContextMenu())
    {
        menu->addItem("Refresh", "", "F5")->setAction(this, &ContextMenuDemo::onChosen);
        menu->addItem("Properties")->setAction(this, &ContextMenuDemo::onChosen);
    }
}

void ContextMenuDemo::onChosen(event::Info info)
{
    if(controls::Button *item=gweni_cast<controls::Button>(info.controlCaller))
        m_chosen->setText("Chosen: "+item->getText());
}

}//namespace gweni
