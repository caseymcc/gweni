#include "demo/toast.h"
#include "gweni/controls/button.h"
#include "gweni/controls/canvas.h"
#include "gweni/controls/toast.h"

namespace gweni
{

namespace
{
controls::Button *columnButton(controls::Base *parent, const String &text, int y)
{
    controls::Button *button=parent->newChild<controls::Button>();

    button->setText(text);
    button->setPos(10, y);
    // room for the longest label; each takes its own text's width
    button->setWidth(200);
    return button;
}
}

// The buttons in a column on the left, clear of the stack in the top-right
// corner.
GWENI_CONTROL_CONSTRUCTOR(ToastDemo)
{
    setDock(DockPosition::Center);

    m_info=columnButton(this, "Info", 10);
    m_info->onPressCaller.add(this, &ToastDemo::onInfo);
    m_positive=columnButton(this, "Positive", 34);
    m_positive->onPressCaller.add(this, &ToastDemo::onPositive);
    m_warning=columnButton(this, "Warning", 58);
    m_warning->onPressCaller.add(this, &ToastDemo::onWarning);
    m_negative=columnButton(this, "Negative", 82);
    m_negative->onPressCaller.add(this, &ToastDemo::onNegative);
    m_long=columnButton(this, "Multi-line", 112);
    m_long->onPressCaller.add(this, &ToastDemo::onLong);
    m_sticky=columnButton(this, "Sticky", 136);
    m_sticky->onPressCaller.add(this, &ToastDemo::onSticky);
}

void ToastDemo::onInfo(event::Info)
{
    getCanvas()->notify(controls::ToastKind::Info, "Task #607 moved to review.");
}

void ToastDemo::onPositive(event::Info)
{
    getCanvas()->notify(controls::ToastKind::Positive, "3 tasks saved.", controls::ToastOptions{-1.0, "Saved"});
}

void ToastDemo::onWarning(event::Info)
{
    getCanvas()->notify(controls::ToastKind::Warning, "The render lock is busy: queued behind 2 runs.");
}

void ToastDemo::onNegative(event::Info)
{
    getCanvas()->notify(controls::ToastKind::Negative, "Run #2686 failed: ctest -L gpu, 1 test failed.",
        controls::ToastOptions{-1.0, "Run failed"});
}

void ToastDemo::onLong(event::Info)
{
    getCanvas()->notify(controls::ToastKind::Info,
        "The merge of task #570 changed ControlTest: every open task that touches it was rebased, "
        "and two of them need their scenarios looked at again before review.");
}

void ToastDemo::onSticky(event::Info)
{
    getCanvas()->notify(controls::ToastKind::Warning, "The host is out of disk space. This stays until closed.",
        controls::ToastOptions{0.0, "Disk full"});
}

}//namespace gweni
