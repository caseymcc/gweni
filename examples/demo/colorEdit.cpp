#include "demo/colorEdit.h"
#include "gweni/controls/colorButton.h"
#include "gweni/controls/colorEdit.h"
#include "gweni/controls/label.h"

namespace gweni
{

namespace
{

controls::Label *addCaption(controls::Base *parent, const char *text, int y)
{
    controls::Label *caption=parent->newChild<controls::Label>();

    caption->setText(text);
    caption->setPos(10, y);
    return caption;
}

}//namespace

// Colour fields: an opaque and a translucent ColorButton (the second over a
// checkerboard), a ColorEdit in hex without alpha and one in R, G, B, A with
// a translucent colour. A click on a ColorEdit's swatch opens its picker in a
// popup below it.
GWENI_CONTROL_CONSTRUCTOR(ColorEditDemo)
{
    setDock(DockPosition::Center);

    addCaption(this, "ColorButton: opaque, translucent", 10);
    {
        controls::ColorButton *opaque=newChild<controls::ColorButton>();

        opaque->setColor(Color(122, 162, 247, 255));
        opaque->setPos(10, 30);

        controls::ColorButton *translucent=newChild<controls::ColorButton>();

        translucent->setColor(Color(247, 118, 142, 128));
        translucent->setSize(40, 20);
        translucent->setPos(40, 30);
    }

    addCaption(this, "ColorEdit, hex without alpha", 64);
    {
        controls::ColorEdit *hex=newChild<controls::ColorEdit>();

        hex->setMode(controls::ColorEditMode::Hex);
        hex->setAlphaEnabled(false);
        hex->setColor(Color(158, 206, 106, 255));
        hex->setPos(10, 84);
    }

    addCaption(this, "ColorEdit, R G B A", 118);
    {
        controls::ColorEdit *rgba=newChild<controls::ColorEdit>();

        rgba->setColor(Color(224, 175, 104, 200));
        rgba->setPos(10, 138);
    }
}

}//namespace gweni
