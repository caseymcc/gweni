#include "demo/window.h"
#include <gweni/controls/layout/layout.h>
#include <gweni/controls/canvas.h>
#include "gweni/controls/button.h"
#include "gweni/controls/windowControl.h"

#include <random>

namespace gweni
{

namespace
{
//the process-wide rand() is never seeded, so these windows would land in the same
//place only by accident; a capture of this demo has to be repeatable
std::mt19937 g_rng(1337u);

int pick(int range)
{
    return static_cast<int>(g_rng()%static_cast<unsigned>(range));
}
}//namespace

GWENI_CONTROL_CONSTRUCTOR(WindowDemo)
{
    setLayout(new controls::VerticalLayout());

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
    window->setSize(200 + pick(100), 200 + pick(100));
    window->setPos(pick(700), pick(400));
    window->setDeleteOnClose(true);
    m_windowCount++;
}

void WindowDemo::openModalWindow(event::Info info)
{
    controls::WindowControl *window=getCanvas()->newChild<controls::WindowControl>();
    window->setTitle(format("Window %i", m_windowCount));
    window->setSize(200 + pick(100), 200 + pick(100));
    window->makeModal();
    window->setAlignment(Alignment::Center);
    window->setDeleteOnClose(true);
    m_windowCount++;
}

}//namespace gweni
