#include "demo/progressBar.h"
#include "gweni/controls/radioButtonController.h"
#include "gweni/controls/progressBar.h"

namespace gweni
{

//ProgressBarDemo::ProgressBarDemo(const String &name):
//    Base()
GWENI_CONTROL_CONSTRUCTOR(ProgressBarDemo)
{
    {
        controls::ProgressBar *pb=newChild<controls::ProgressBar>();
        pb->setBounds(gweni::Rect(110, 20, 200, 20));
        pb->setProgress(0.27f);
    }
    {
        controls::ProgressBar *pb=newChild<controls::ProgressBar>();
        pb->setBounds(gweni::Rect(110, 50, 200, 20));
        pb->setProgress(0.66f);
        pb->setAlignment(Alignment::Right | Alignment::CenterV);
    }
    {
        controls::ProgressBar *pb=newChild<controls::ProgressBar>();
        pb->setBounds(gweni::Rect(110, 80, 200, 20));
        pb->setProgress(0.88f);
        pb->setAlignment(Alignment::Left | Alignment::CenterV);
    }
    {
        controls::ProgressBar *pb=newChild<controls::ProgressBar>();
        pb->setBounds(gweni::Rect(110, 110, 200, 20));
        pb->setAutoLabel(false);
        pb->setProgress(0.20f);
        pb->setAlignment(Alignment::Right | Alignment::CenterV);
        pb->setText("40,245 MB");
    }
    {
        controls::ProgressBar *pb=newChild<controls::ProgressBar>();
        pb->setBounds(gweni::Rect(110, 140, 200, 20));
        pb->setAutoLabel(false);
        pb->setProgress(1.00f);
    }
    {
        controls::ProgressBar *pb=newChild<controls::ProgressBar>();
        pb->setBounds(gweni::Rect(110, 170, 200, 20));
        pb->setAutoLabel(false);
        pb->setProgress(0.00f);
    }
    {
        controls::ProgressBar *pb=newChild<controls::ProgressBar>();
        pb->setBounds(gweni::Rect(110, 200, 200, 20));
        pb->setAutoLabel(false);
        pb->setProgress(0.50f);
    }
    {
        controls::ProgressBar *pb=newChild<controls::ProgressBar>();
        pb->setBounds(gweni::Rect(20, 20, 25, 200));
        pb->setVertical();
        pb->setProgress(0.25f);
        pb->setAlignment(Alignment::Top | Alignment::CenterH);
    }
    {
        controls::ProgressBar *pb=newChild<controls::ProgressBar>();
        pb->setBounds(gweni::Rect(50, 20, 25, 200));
        pb->setVertical();
        pb->setProgress(0.40f);
    }
    {
        controls::ProgressBar *pb=newChild<controls::ProgressBar>();
        pb->setBounds(gweni::Rect(80, 20, 25, 200));
        pb->setVertical();
        pb->setAlignment(Alignment::Bottom | Alignment::CenterH);
        pb->setProgress(0.65f);
    }
}

}//namespace gweni
