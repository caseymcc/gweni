/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The hello window (docs/building.md -> "A window with gweni_app"): an
 *  app::Window with a gweni Window holding a Label and a Button that counts its
 *  clicks. The app::Window is on the heap and the handler holds what it needs
 *  by value: in the browser run() never returns and main's stack is gone.
 */
#include <gweni/app/window.h>
#include <gweni/controls.h>

#include <string>

int main()
{
    using namespace gweni;

    auto *app=new app::Window(app::ShellOptions());
    if(!app->ok()) // no display, no GL driver, ...: the reason is on stderr
        return 1;
    auto *window=app->canvas()->newChild<controls::WindowControl>();
    auto *label=window->newChild<controls::Label>();
    auto *button=window->newChild<controls::Button>();

    window->setTitle("Hello");
    window->setBounds(20, 20, 240, 120);
    label->setText("0 clicks");
    label->setDock(DockPosition::Top);
    button->setText("Click me");
    button->setDock(DockPosition::Top);
    button->onPressCaller.addCallback(button, [label, clicks=0](event::Handler &, event::Info) mutable
    {
        label->setText(std::to_string(++clicks)+" clicks");
    });
    app->run();
    delete app;
    return 0;
}
