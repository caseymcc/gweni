#include "demo/crossSplitter.h"
#include "gweni/controls/crossSplitter.h"
#include "gweni/controls/statusBar.h"
#include "gweni/controls/button.h"

namespace gweni
{

CrossSplitterDemo::CrossSplitterDemo(controls::Base *parent, const String &name):
    Base(parent)
{
    dock(Position::Fill);

    m_splittersVisible=false;
    m_curZoom=0;

    m_splitter=new controls::CrossSplitter(this);

    m_splitter->setPos(0, 0);
    m_splitter->dock(Position::Fill);
    {
        controls::Button *testButton=new controls::Button(m_splitter);

        testButton->setText("TOPLEFT");
        m_splitter->setPanel(0, testButton);
    }
    {
        controls::Button *testButton=new controls::Button(m_splitter);

        testButton->setText("TOPRIGHT");
        m_splitter->setPanel(1, testButton);
    }
    {
        controls::Button *testButton=new controls::Button(m_splitter);

        testButton->setText("BOTTOMRIGHT");
        m_splitter->setPanel(2, testButton);
    }
    {
        controls::Button *testButton=new controls::Button(m_splitter);

        testButton->setText("BOTTOMLEFT");
        m_splitter->setPanel(3, testButton);
    }

    //Status bar to hold unit testing buttons
    controls::StatusBar *status=new controls::StatusBar(this);

    status->dock(Position::Bottom);
    {
        controls::Button *button=new controls::Button(status);

        button->setText("Zoom");
        button->onPressCaller.add(this, &CrossSplitterDemo::zoomTest);
        status->addControl(button, false);
    }
    {
        controls::Button *button=new controls::Button(status);

        button->setText("UnZoom");
        button->onPressCaller.add(this, &CrossSplitterDemo::unZoomTest);
        status->addControl(button, false);
    }
    {
        controls::Button *button=new controls::Button(status);

        button->setText("CenterPanels");
        button->onPressCaller.add(this, &CrossSplitterDemo::centerPanels);
        status->addControl(button, true);
    }
}

void CrossSplitterDemo::zoomTest(event::Info info)
{}

void CrossSplitterDemo::unZoomTest(event::Info info)
{}

void CrossSplitterDemo::centerPanels(event::Info info)
{}

}//namespace gweni
