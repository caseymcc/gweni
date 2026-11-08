#include "demo/imagePanel.h"
#include <gweni/controls/layout/layout.h>
#include "gweni/controls/imagePanel.h"

namespace gweni
{

//ImagePanelDemo::ImagePanelDemo(const String &name):
//    Base()
GWENI_CONTROL_CONSTRUCTOR(ImagePanelDemo)
{
    setLayout(new controls::VerticalLayout());

    // Normal
    {
        controls::ImagePanel *img=newChild<controls::ImagePanel>();

        img->setImage("gwen.png");
        img->setBounds(10, 10, 100, 100);
    }
    // Missing
    {
        controls::ImagePanel *img=newChild<controls::ImagePanel>();

        // no image is set, so it draws the missing-image placeholder; a file
        // that does not exist would draw the same but log an error
        img->setBounds(120, 10, 100, 100);
    }
}

}//namespace gweni
