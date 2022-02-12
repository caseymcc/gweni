#include "demo/imagePanel.h"
#include "gweni/controls/imagePanel.h"

namespace gweni
{

ImagePanelDemo::ImagePanelDemo(const String &name):
    Base()
{
    // Normal
    {
        controls::ImagePanel *img=newChild<controls::ImagePanel>();

        img->setImage("gwen.png");
        img->setBounds(10, 10, 100, 100);
    }
    // Missing
    {
        controls::ImagePanel *img=newChild<controls::ImagePanel>();

        img->setImage("missingimage.png");
        img->setBounds(120, 10, 100, 100);
    }
}

}//namespace gweni
