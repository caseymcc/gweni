#include "demo/imagePanel.h"
#include "gweni/controls/imagePanel.h"

namespace gweni
{

ImagePanelDemo::ImagePanelDemo(controls::Base *parent, const String &name):
    Base(parent)
{
    // Normal
    {
        controls::ImagePanel *img=new controls::ImagePanel(this);

        img->setImage("gwen.png");
        img->setBounds(10, 10, 100, 100);
    }
    // Missing
    {
        controls::ImagePanel *img=new controls::ImagePanel(this);

        img->setImage("missingimage.png");
        img->setBounds(120, 10, 100, 100);
    }
}

}//namespace gweni
