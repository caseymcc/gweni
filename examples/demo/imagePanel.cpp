#include "demo/imagePanel.h"
#include <gweni/controls/groupBox.h>
#include <gweni/controls/layout/layout.h>
#include "gweni/controls/imagePanel.h"

namespace gweni
{

// gwen.png (896x896) in each scale mode, each in a 150x110 GroupBox filled by
// the panel, so the box shows the bounds the image is placed in, and an empty
// panel, which draws nothing.
GWENI_CONTROL_CONSTRUCTOR(ImagePanelDemo)
{
    setDock(DockPosition::Center);

    struct Mode
    {
        const char *caption;
        controls::ImageScaleMode mode;
        bool image;
    };
    const Mode modes[]=
    {
        { "Stretch", controls::ImageScaleMode::Stretch, true },
        { "Fit", controls::ImageScaleMode::Fit, true },
        { "Fill", controls::ImageScaleMode::Fill, true },
        { "None", controls::ImageScaleMode::None, true },
        { "Empty", controls::ImageScaleMode::Fit, false },
    };

    int index=0;

    for(const Mode &mode:modes)
    {
        controls::GroupBox *box=newChild<controls::GroupBox>();

        box->setText(mode.caption);
        box->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        box->setBounds(10+(index%4)*155, 10+(index/4)*120, 150, 110);

        controls::ImagePanel *img=box->newChild<controls::ImagePanel>();

        img->setDock(DockPosition::Center);
        img->setScaleMode(mode.mode);
        if(mode.image)
            img->setImage("gwen.png");
        ++index;
    }
}

}//namespace gweni
