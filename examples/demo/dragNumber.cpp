#include "demo/dragNumber.h"
#include "gweni/controls/dragNumber.h"
#include "gweni/controls/label.h"

namespace gweni
{

// DragNumbers, each after a Label naming it: an integer with no range, a float
// in 0..1 at a fine speed, a float with a format of its own ("%.1f ms") and a
// disabled one. A drag scrubs a field, a double-click or Ctrl+click types into
// it.
GWENI_CONTROL_CONSTRUCTOR(DragNumberDemo)
{
    setDock(DockPosition::Center);

    auto row=[this](const char *caption, int y)
    {
        controls::Label *label=newChild<controls::Label>();
        label->setText(caption);
        label->setPos(10, y+3);

        controls::DragNumber *field=newChild<controls::DragNumber>();
        field->setPos(150, y);
        field->setSize(120, 20);
        return field;
    };

    m_integer=row("Integer", 10);
    m_integer->setInteger(true);
    m_integer->setNumber(42);

    m_ranged=row("Float, 0 to 1", 40);
    m_ranged->setRange(0.0, 1.0);
    m_ranged->setSpeed(0.005);
    m_ranged->setNumber(0.25);

    controls::DragNumber *custom=row("Format \"%.1f ms\"", 70);
    custom->setFormat("%.1f ms");
    custom->setNumber(16.7);

    controls::DragNumber *disabled=row("Disabled", 100);
    disabled->setNumber(3.5);
    disabled->setDisabled(true);
}

}//namespace gweni
