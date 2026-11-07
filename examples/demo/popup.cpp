#include "demo/popup.h"
#include "gweni/controls/button.h"
#include "gweni/controls/canvas.h"
#include "gweni/controls/comboBox.h"
#include "gweni/controls/horizontalSlider.h"
#include "gweni/controls/label.h"
#include "gweni/controls/popup.h"

namespace gweni
{

// Two buttons that toggle a popup each (controls::Popup): "Options" in the
// top-left corner opens a panel of labels, a slider, a ComboBox (whose list
// opens over the popup) and a Done button below it; "Corner", in the
// bottom-right corner, opens one that has no room below it or on its right,
// so it is placed above the button with their right edges aligned. A Help
// button with a tooltip sits right of Options, so its tooltip lies over the
// open Options popup.
GWENI_CONTROL_CONSTRUCTOR(PopupDemo)
{
    setDock(DockPosition::Center);

    m_options=newChild<controls::Button>();
    m_options->setText("Options");
    m_options->setPos(10, 10);
    m_options->onPressCaller.add(this, &PopupDemo::onToggle);

    controls::Button *help=newChild<controls::Button>();

    help->setText("Help");
    help->setToolTipText("A tooltip draws over an open popup");
    help->setPos(120, 10);

    // Made on the canvas, where opening would move them: so the first open
    // touches nothing in this page. Its rows are docked Top, inside the
    // popup's padding, 4 units apart.
    m_optionsPopup=getCanvas()->newChild<controls::Popup>();
    {
        controls::Label *title=m_optionsPopup->newChild<controls::Label>();

        title->setText("View options");
        title->setDock(DockPosition::Top);
        title->setMargin(Margin(0, 0, 0, 4));

        controls::Label *opacity=m_optionsPopup->newChild<controls::Label>();

        opacity->setText("Grid opacity");
        opacity->setDock(DockPosition::Top);

        controls::HorizontalSlider *slider=m_optionsPopup->newChild<controls::HorizontalSlider>();

        slider->setHeight(20);
        slider->setRange(0, 100);
        slider->setFloatValue(60);
        slider->setDock(DockPosition::Top);
        slider->setMargin(Margin(0, 0, 0, 4));

        controls::ComboBox *zoom=m_optionsPopup->newChild<controls::ComboBox>();

        zoom->setSizeFlags({SizeFlag::Fixed, SizeFlag::Shrink});
        zoom->setWidth(160);
        zoom->setDock(DockPosition::Top);
        zoom->setMargin(Margin(0, 0, 0, 4));
        zoom->addItem("Zoom 100%");
        zoom->addItem("Zoom 150%");
        zoom->addItem("Zoom 200%");

        controls::Button *done=m_optionsPopup->newChild<controls::Button>();

        done->setText("Done");
        done->setDock(DockPosition::Top);
        done->onPressCaller.add(this, &PopupDemo::onDone);
    }

    m_corner=newChild<controls::Button>();
    m_corner->setText("Corner");
    m_corner->setMargin(Margin(0, 0, 10, 10));
    m_corner->setAlignment(Alignment::Right|Alignment::Bottom);
    m_corner->onPressCaller.add(this, &PopupDemo::onToggle);

    m_cornerPopup=getCanvas()->newChild<controls::Popup>();
    {
        controls::Label *title=m_cornerPopup->newChild<controls::Label>();

        title->setText("Opened above, inside the canvas");
        title->setDock(DockPosition::Top);
        title->setMargin(Margin(0, 0, 0, 4));

        for(const char *caption : {"Refresh", "Properties"})
        {
            controls::Button *button=m_cornerPopup->newChild<controls::Button>();

            button->setText(caption);
            button->setDock(DockPosition::Top);
            button->setMargin(Margin(0, 0, 0, 2));
        }
    }
}

PopupDemo::~PopupDemo()
{
    // Delayed, as the canvas deletes them with its other children when it is
    // tearing down.
    for(controls::Popup *popup : {m_optionsPopup, m_cornerPopup})
        popup->delayedDelete();
}

void PopupDemo::onToggle(event::Info info)
{
    controls::Popup *popup=(info.controlCaller == m_options) ? m_optionsPopup : m_cornerPopup;

    if(popup->isOpen())
        popup->close();
    else
        popup->open(info.controlCaller);
}

void PopupDemo::onDone(event::Info)
{
    m_optionsPopup->close();
}

}//namespace gweni
