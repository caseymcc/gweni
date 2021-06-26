#include "demo/progressBar.h"
#include "gweni/controls/radioButtonController.h"
#include "gweni/controls/progressBar.h"

namespace gweni
{

ProgressBarDemo::ProgressBarDemo(controls::Base *parent, const String &name):
    Base(parent)
{
    {
        controls::ProgressBar *pb=new controls::ProgressBar(this);
        pb->setBounds(gweni::Rect(110, 20, 200, 20));
        pb->setProgress(0.27f);
    }
    {
        controls::ProgressBar *pb=new controls::ProgressBar(this);
        pb->setBounds(gweni::Rect(110, 50, 200, 20));
        pb->setProgress(0.66f);
        pb->setAlignment(Position::Right | Position::CenterV);
    }
    {
        controls::ProgressBar *pb=new controls::ProgressBar(this);
        pb->setBounds(gweni::Rect(110, 80, 200, 20));
        pb->setProgress(0.88f);
        pb->setAlignment(Position::Left | Position::CenterV);
    }
    {
        controls::ProgressBar *pb=new controls::ProgressBar(this);
        pb->setBounds(gweni::Rect(110, 110, 200, 20));
        pb->setAutoLabel(false);
        pb->setProgress(0.20f);
        pb->setAlignment(Position::Right | Position::CenterV);
        pb->setText("40,245 MB");
    }
    {
        controls::ProgressBar *pb=new controls::ProgressBar(this);
        pb->setBounds(gweni::Rect(110, 140, 200, 20));
        pb->setAutoLabel(false);
        pb->setProgress(1.00f);
    }
    {
        controls::ProgressBar *pb=new controls::ProgressBar(this);
        pb->setBounds(gweni::Rect(110, 170, 200, 20));
        pb->setAutoLabel(false);
        pb->setProgress(0.00f);
    }
    {
        controls::ProgressBar *pb=new controls::ProgressBar(this);
        pb->setBounds(gweni::Rect(110, 200, 200, 20));
        pb->setAutoLabel(false);
        pb->setProgress(0.50f);
    }
    {
        controls::ProgressBar *pb=new controls::ProgressBar(this);
        pb->setBounds(gweni::Rect(20, 20, 25, 200));
        pb->setVertical();
        pb->setProgress(0.25f);
        pb->setAlignment(Position::Top | Position::CenterH);
    }
    {
        controls::ProgressBar *pb=new controls::ProgressBar(this);
        pb->setBounds(gweni::Rect(50, 20, 25, 200));
        pb->setVertical();
        pb->setProgress(0.40f);
    }
    {
        controls::ProgressBar *pb=new controls::ProgressBar(this);
        pb->setBounds(gweni::Rect(80, 20, 25, 200));
        pb->setVertical();
        pb->setAlignment(Position::Bottom | Position::CenterH);
        pb->setProgress(0.65f);
    }
}

}//namespace gweni
