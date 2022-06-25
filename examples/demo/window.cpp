#include "demo/window.h"
#include "gweni/controls/button.h"
#include "gweni/controls/windowControl.h"

namespace gweni
{

WindowDemo::WindowDemo(const String &name):
    Base()
{
    {
        controls::Button *button=newChild<controls::Button>();
        button->setText("Normal Window");
        button->onPressCaller.add(this, &ThisClass::openWindow);
        button->setPos(0, 0);
    }
    {
        controls::Button *button=newChild<controls::Button>();
        button->setText("Modal Window");
        button->onPressCaller.add(this, &ThisClass::openModalWindow);
        button->setPos(0, 32);
    }
    m_windowCount=1;
}

void WindowDemo::openWindow(event::Info info)
{
    controls::WindowControl *window=getCanvas()->newChild<controls::WindowControl>();
    window->setTitle(format("Window %i", m_windowCount));
    window->setSize(200 + rand() % 100, 200 + rand() % 100);
    window->setPos(rand() % 700, rand() % 400);
    window->setDeleteOnClose(true);
    m_windowCount++;
}

void WindowDemo::openModalWindow(event::Info info)
{
    controls::WindowControl *window=getCanvas()->newChild<controls::WindowControl>();
    window->setTitle(format("Window %i", m_windowCount));
    window->setSize(200 + rand() % 100, 200 + rand() % 100);
    window->makeModal(true);
    window->setAlignment(Alignment::Center);
//    window->setPosition(Position::Center);
    window->setDeleteOnClose(true);
    m_windowCount++;
}

}//namespace gweni
