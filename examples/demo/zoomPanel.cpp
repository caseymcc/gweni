#include "demo/zoomPanel.h"
#include "gweni/controls/button.h"
#include "gweni/controls/comboBox.h"
#include "gweni/controls/dragNumber.h"
#include "gweni/controls/label.h"
#include "gweni/controls/scrollControl.h"
#include "gweni/controls/textBox.h"
#include "gweni/controls/zoomPanel.h"

#include <string>

namespace gweni
{

// A line saying what the input does, then the panel filling the rest, its
// content placed where the bounds say in content units.
GWENI_CONTROL_CONSTRUCTOR(ZoomPanelDemo)
{
    setDock(DockPosition::Center);

    controls::Label *hint=newChild<controls::Label>();
    hint->setDock(DockPosition::Top);
    hint->setText("Wheel: zoom about the cursor. Middle or right drag, or Space and drag: pan.");

    m_panel=newChild<controls::ZoomPanel>();
    m_panel->setDock(DockPosition::Center);
    m_panel->setContentSize(Point(420, 260));
    m_panel->setZoom(1.25f);
    m_panel->setPan(PointF(8.5f, 6.25f));

    controls::Label *title=m_panel->newChild<controls::Label>();
    title->setText("Inside a ZoomPanel");
    title->setPos(10, 6);

    controls::Button *button=m_panel->newChild<controls::Button>();
    button->setText("With Tooltip");
    button->setToolTipText("A tooltip opened from inside the panel");
    button->setPos(10, 30);
    button->setSize(110, 22);

    controls::ComboBox *combo=m_panel->newChild<controls::ComboBox>();
    combo->setPos(10, 62);
    combo->setSize(140, 20);
    combo->addItem("Linear");
    combo->addItem("Smooth step");
    combo->addItem("Cubic bezier");

    controls::DragNumber *number=m_panel->newChild<controls::DragNumber>();
    number->setPos(10, 92);
    number->setSize(140, 20);
    number->setRange(0, 100);
    number->setFormat("%.1f");
    number->setNumber(42.5, false);

    controls::TextBox *textBox=m_panel->newChild<controls::TextBox>();
    textBox->setPos(10, 122);
    textBox->setSize(180, 20);
    textBox->setText("Zoomed text");

    controls::ScrollControl *scroll=m_panel->newChild<controls::ScrollControl>();
    scroll->setPos(220, 30);
    scroll->setSize(180, 140);
    scroll->setScroll(false, true);
    for(int i=0; i < 12; ++i)
    {
        controls::Label *row=scroll->newChild<controls::Label>();
        row->setText("Row "+std::to_string(i+1));
        row->setPos(6, 4+i*18);
    }
}

}//namespace gweni
