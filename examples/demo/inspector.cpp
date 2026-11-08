#include "demo/inspector.h"
#include "gweni/controls/button.h"
#include "gweni/controls/checkBox.h"
#include "gweni/controls/groupBox.h"
#include "gweni/controls/label.h"
#include "gweni/tools/inspector.h"

namespace gweni
{

// The app part on the left, the inspector on the right. The inspector is the
// demo's own child here, not the canvas's, so the page shows and hides it with
// the rest; it inspects the whole canvas all the same. Its tree is opened down
// to the Button it selects, which its highlight box frames.
GWENI_CONTROL_CONSTRUCTOR(InspectorDemo)
{
    setDock(DockPosition::Center);

    controls::GroupBox *group=newChild<controls::GroupBox>();
    group->setText("Settings");
    group->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    group->setPos(10, 10);
    group->setSize(260, 130);

    m_button=group->newChild<controls::Button>("save");
    m_button->setText("Save");
    m_button->setPos(10, 10);

    controls::CheckBoxWithLabel *check=group->newChild<controls::CheckBoxWithLabel>("autosave");
    check->label()->setText("Autosave");
    check->setPos(10, 45);

    controls::Label *status=group->newChild<controls::Label>("status");
    status->setText("Ready");
    status->setPos(10, 75);

    m_inspector=newChild<tools::Inspector>();
    // placed in a demo not laid out yet, which a clamp to it would put at 0,0
    m_inspector->setClampMovement(false);
    m_inspector->setPos(285, 5);
    m_inspector->setSize(570, 530);
    m_inspector->select(m_button);
}

}//namespace gweni
