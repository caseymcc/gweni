#include "demo/crossSplitter.h"
#include "gweni/controls/crossSplitter.h"
#include "gweni/controls/statusBar.h"
#include "gweni/controls/button.h"

namespace gweni
{

CrossSplitterDemo::CrossSplitterDemo(const String &name):
    Base()
{}

void CrossSplitterDemo::init(const String &name)
{
    setDock(DockPosition::Center);

    m_splittersVisible=false;
    m_curZoom=0;

    m_splitter=newChild<controls::CrossSplitter>();

    m_splitter->setPos(0, 0);
    m_splitter->setDock(DockPosition::Center);
    {
        controls::Button *testButton=m_splitter->newChild<controls::Button>();

        testButton->setText("TOPLEFT");
        m_splitter->setPanel(0, testButton);
    }
    {
        controls::Button *testButton=m_splitter->newChild<controls::Button>();

        testButton->setText("TOPRIGHT");
        m_splitter->setPanel(1, testButton);
    }
    {
        controls::Button *testButton=m_splitter->newChild<controls::Button>();

        testButton->setText("BOTTOMRIGHT");
        m_splitter->setPanel(2, testButton);
    }
    {
        controls::Button *testButton=m_splitter->newChild<controls::Button>();

        testButton->setText("BOTTOMLEFT");
        m_splitter->setPanel(3, testButton);
    }

    //Status bar to hold unit testing buttons
    controls::StatusBar *status=newChild<controls::StatusBar>();

    status->setDock(DockPosition::Bottom);
    {
        controls::Button *button=status->newChild<controls::Button>();

        button->setText("Zoom");
        button->onPressCaller.add(this, &CrossSplitterDemo::zoomTest);
        status->addControl(button, false);
    }
    {
        controls::Button *button=status->newChild<controls::Button>();

        button->setText("UnZoom");
        button->onPressCaller.add(this, &CrossSplitterDemo::unZoomTest);
        status->addControl(button, false);
    }
    {
        controls::Button *button=status->newChild<controls::Button>();

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
