#include "demo/pageControl.h"
#include "gweni/controls/pageControl.h"

namespace gweni
{

PageControlDemo::PageControlDemo(const String &name):
    Base()
{
    controls::PageControl *control=newChild<controls::PageControl>();
    control->setSize(500, 300);
    //control->setPageCount(5);
    control->addPages(5);
    control->onPageChanged.add(this, &ThisClass::OnPageChanged);
    control->onFinish.add(this, &ThisClass::OnFinish);
    // Page 0
    {
        controls::Button *button=control->getPage(0)->newChild<controls::Button>();
        button->setDock(DockPosition::Center);
        button->setText("This button is fill docked on page 0");
    }
    // Page 1
    {
        controls::Button *button=control->getPage(1)->newChild<controls::Button>();
        button->setDock(DockPosition::Top);
        button->setText("This button is top docked on page 1");
    }
    // Page 2
    {
        controls::Button *button=control->getPage(2)->newChild<controls::Button>();
        button->setSize(400, 1000);
        button->setPos(50, 50);
        button->setText("This button is long to test scrolling (page 2)");
    }
}

void PageControlDemo::OnPageChanged(event::Info info)
{
//    UnitPrint(Format(L"PageControl Changed Page: %i", info.Integer));
}

void PageControlDemo::OnFinish(event::Info info)
{
//    UnitPrint(L"Finish Pressed");
}

}//namespace gweni
