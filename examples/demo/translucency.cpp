/*
 *  Gweni demo
 *  See license in gweni.h
 */
#include "demo/translucency.h"
#include "gweni/controls/button.h"
#include "gweni/controls/canvas.h"
#include "gweni/controls/label.h"

#include <string>

namespace gweni
{

Color TranslucencyDemo::panelColor()
{
    return Color(0x7a, 0xa2, 0xf7, 0x80);
}

GWENI_CONTROL_CONSTRUCTOR(TranslucencyPanel)
{
}

GWENI_CONTROL_CONSTRUCTOR(TranslucencyBlock)
{
}

// Placed by hand at fixed bounds on a 640x480 canvas: the panel over the
// middle Labels' right halves, the stats overlay's corner left clear but for
// the Labels meant to be under it.
GWENI_CONTROL_CONSTRUCTOR(TranslucencyDemo)
{
    setDock(DockPosition::Center);

    auto label=[this](const String &text, int x, int y)
    {
        controls::Label *made=newChild<controls::Label>();

        made->setText(text);
        made->setPos(x, y);
        made->sizeToContents();
        return made;
    };

    for(int i=0; i < 4; ++i)
        m_leftColumn.push_back(label("Left of a dialog "+std::to_string(i+1), 10, 260+24*i));
    for(int i=0; i < 6; ++i)
        m_underPanel.push_back(label("Text under the translucent panel, line "+std::to_string(i+1),
            130, 50+26*i));
    for(int i=0; i < 3; ++i)
        m_underOverlay.push_back(label("Under the stats overlay, label "+std::to_string(i+1), 440,
            196+28*i));

    m_panel=newChild<TranslucencyPanel>();
    m_panel->setBounds(220, 30, 200, 214);

    controls::Label *caption=m_panel->newChild<controls::Label>();
    caption->setText("A translucent panel");
    caption->setPos(10, 6);
    caption->sizeToContents();

    m_button=m_panel->newChild<controls::Button>();
    m_button->setText("On the panel");
    m_button->setBounds(20, 180, 110, 24);

    // made in the second update, after the first frame drew (and so took the
    // panel's slot), so its slot is taken after the panel's; sent to the back,
    // behind it. A timer the first update runs is the first update's, so the
    // one it sets is the next's.
    if(controls::Canvas *canvas=getCanvas())
    {
        canvas->after(0.0, [this, canvas]()
        {
            canvas->after(0.0, [this]()
            {
                m_block=newChild<TranslucencyBlock>();
                m_block->setBounds(330, 120, 120, 60);
                m_block->sendToBack();
            }, this);
        }, this);
    }
}

}//namespace gweni
