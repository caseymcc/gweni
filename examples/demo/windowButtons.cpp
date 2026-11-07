#include "demo/windowButtons.h"
#include "gweni/controls/label.h"
#include "gweni/controls/windowButtons.h"

namespace gweni
{

namespace
{

// A row of the title-bar buttons, at the size a WindowControl's title bar
// gives its close button: close, minimise, maximise and maximised (restore).
void addButtonRow(controls::Base *parent, int y, const String &caption, bool disabled)
{
    controls::Label *label=parent->newChild<controls::Label>();

    label->setText(caption);
    label->setPos(10, y+4);

    controls::WindowCloseButton *buttons[4]=
    {
        parent->newChild<controls::WindowCloseButton>(),
        parent->newChild<controls::WindowMinimizeButton>(),
        parent->newChild<controls::WindowMaximizeButton>(),
        parent->newChild<controls::WindowMaximizeButton>(),
    };

    static_cast<controls::WindowMaximizeButton *>(buttons[3])->setMaximized(true);

    for(int i=0; i < 4; ++i)
    {
        buttons[i]->setSize(29, 24);
        buttons[i]->setPos(90+i*37, y);
        buttons[i]->setDisabled(disabled);
    }
}

}//namespace

// The window buttons standalone: a WindowControl shows only the close button,
// and the minimise and maximise buttons are WindowCanvas's, an OS window.
GWENI_CONTROL_CONSTRUCTOR(WindowButtonsDemo)
{
    setDock(DockPosition::Center);

    addButtonRow(this, 10, "Enabled", false);
    addButtonRow(this, 44, "Disabled", true);
}

}//namespace gweni
