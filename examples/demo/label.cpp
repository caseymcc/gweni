#include "demo/label.h"
#include "gweni/controls/label.h"

namespace gweni
{

LabelDemo::LabelDemo(controls::Base *parent, const String &name):
    Base(parent)
{
    {
        controls::Label *label=new controls::Label(this);
        label->setText("Garry's Normal Label");
        label->sizeToContents();
        label->setPos(10, 10);
    }
    {
        controls::Label *label=new controls::Label(this);
        label->setText("Chinese: n/a");
        label->sizeToContents();
        label->setPos(10, 30);
    }
    {
        controls::Label *label=new controls::Label(this);
        label->setText("Japanese: n/a");
        label->sizeToContents();
        label->setPos(10, 50);
    }
    {
        controls::Label *label=new controls::Label(this);
        label->setText("Korean: n/a");
        label->sizeToContents();
        label->setPos(10, 70);
    }
    {
        controls::Label *label=new controls::Label(this);
        label->setText("Hindi: n/a");
        label->sizeToContents();
        label->setPos(10, 90);
    }
    {
        controls::Label *label=new controls::Label(this);
        label->setText("Arabic: n/a");
        label->sizeToContents();
        label->setPos(10, 110);
    }
    {
        controls::Label *label=new controls::Label(this);
        label->setText("Wow, Coloured Text");
        label->setTextColor(gweni::Color(0, 0, 255, 255));
        label->sizeToContents();
        label->setPos(10, 130);
    }
    {
        controls::Label *label=new controls::Label(this);
        label->setText("Coloured Text With Alpha");
        label->setTextColor(gweni::Color(0, 0, 255, 100));
        label->sizeToContents();
        label->setPos(10, 150);
    }
    {
        //
        // Note that when using a custom font, this font object has to stick around
        // for the lifetime of the label. Rethink, or is that ideal?
        //
//        m_font.facename="Comic Sans MS";
//        m_font.size=25;
        controls::Label *label=new controls::Label(this);
        label->setText("Supose to be custom");// Custom Font(Comic Sans 25)");
//        label->setFont(m_font);
        label->sizeToContents();
        label->setPos(10, 170);
    }
}

}//namespace gweni
