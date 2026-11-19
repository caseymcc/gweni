/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  app_window (and app_window_vulkan): gweni_app's app::Window in a real
 *  window, under an X server of its own (tests/app/window.cmake runs it in
 *  xvfb-run). It links gweni_app alone, as an app does, opens a Window of a
 *  Button and a Label on GWENI_RENDERER's backend (gl, or vulkan validated)
 *  and fails, one line naming the check, unless:
 *    1. the Window is ok(), its parts are there and a second Window opened
 *       while it is alive is refused (ok() false, error() saying so);
 *    2. the callbacks it installed are its: a cursor move and a press and
 *       release given to the window's GLFW callbacks over the Button press it
 *       once (onPress), the mouseButton hook told the UI took them, and the
 *       same click on the bare canvas is told not taken;
 *    3. a key event goes to keyFirst before the canvas, and one keyFirst
 *       takes (true) reaches neither the canvas nor the key hook, while one
 *       it leaves reaches the key hook;
 *    4. the scroll hook gets a wheel the UI did not take;
 *    5. a framebuffer-size callback fits the canvas: its size is the
 *       framebuffer's at the window's scale, and framebufferSize() follows;
 *    6. a frame shown in the window and read back (presentWindowFrame) holds
 *       more than one colour, and the frame hooks ran in their order;
 *    7. run() draws, calls afterIteration and returns once the window is
 *       closed from it;
 *    8. once it is gone, a new Window opens.
 *
 *  -- <resources dir>
 */
#include <gweni/app/window.h>
#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/label.h>
#include <gweni/platforms/renderers/slotRenderer.h>

#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace
{

using namespace gweni;

int g_failures=0;

void check(bool ok, const std::string &what)
{
    if(!ok)
    {
        std::fprintf(stderr, "app_window: FAIL: %s\n", what.c_str());
        ++g_failures;
    }
}

//! the Window's own loop draws every iteration, so run() cannot wait forever
//! on a settled canvas
app::WindowOptions continuous()
{
    app::WindowOptions options;

    options.loop.mode=platform::LoopMode::Continuous;
    return options;
}

app::ShellOptions shellOptions(const std::string &resources)
{
    app::ShellOptions options;
    const char *renderer=std::getenv("GWENI_RENDERER");

    options.size=Point(480, 320);
    options.title="app_window";
    options.resources=resources;
    options.backend=(renderer && std::strcmp(renderer, "vulkan") == 0) ? app::Backend::Vulkan
        : app::Backend::GL;
    return options;
}

struct Presses: public event::Handler
{
    int count=0;
    void onPress(event::Info) { ++count; }
};

}//namespace

int main(int argc, char *argv[])
{
    app::setToolName("app_window");
    app::installGlfwErrorHandler();

    const std::string resources=(argc > 1) ? argv[1] : "resources";

    {
        app::Window window(shellOptions(resources), continuous());

        check(window.ok(), "the window did not open: "+window.error());
        if(!window.ok())
            return EXIT_FAILURE;
        check(window.canvas() && window.renderer() && window.skin() && window.device()
            && window.glfwWindow(), "a part of the window is missing");
        check(!window.degraded(), "the resources did not load");

        // 1. one at a time
        {
            app::Window second(shellOptions(resources));

            check(!second.ok(), "a second Window opened while the first was alive");
            check(second.error().find("refused") != std::string::npos,
                "the second Window's error does not say it was refused: "+second.error());
        }

        controls::Canvas *canvas=window.canvas();
        controls::Button *button=canvas->newChild<controls::Button>();
        controls::Label *label=canvas->newChild<controls::Label>();
        Presses presses;

        button->setText("Press me");
        button->setPos(20, 20);
        button->onPressCaller.add(&presses, &Presses::onPress);
        label->setText("app::Window");
        label->setPos(20, 70);
        label->sizeToContents();

        std::vector<std::string> order;
        int keysFirst=0;
        std::vector<int> keysAfter;
        std::vector<bool> buttonsTaken;
        std::vector<bool> scrollsTaken;

        window.hooks.keyFirst=[&](const input::GLFW3::KeyEvent &event)
        {
            ++keysFirst;
            return event.key == GLFW_KEY_F5;
        };
        window.hooks.key=[&](const input::GLFW3::KeyEvent &event, bool) { keysAfter.push_back(event.key); };
        window.hooks.mouseButton=[&](int, int, int, bool taken) { buttonsTaken.push_back(taken); };
        window.hooks.scroll=[&](double, double, bool taken) { scrollsTaken.push_back(taken); };
        window.hooks.beforeUpdate=[&]() { order.push_back("beforeUpdate"); };
        window.hooks.afterUpdate=[&]() { order.push_back("afterUpdate"); };
        window.hooks.beforeRender=[&]() { order.push_back("beforeRender"); };
        window.hooks.afterRender=[&]() { order.push_back("afterRender"); };

        GLFWwindow *glfw=window.glfwWindow();
        // the callbacks the Window installed (setting one returns the one before)
        const GLFWcursorposfun cursorPos=glfwSetCursorPosCallback(glfw, nullptr);
        glfwSetCursorPosCallback(glfw, cursorPos);
        const GLFWmousebuttonfun mouseButton=glfwSetMouseButtonCallback(glfw, nullptr);
        glfwSetMouseButtonCallback(glfw, mouseButton);
        const GLFWkeyfun key=glfwSetKeyCallback(glfw, nullptr);
        glfwSetKeyCallback(glfw, key);
        const GLFWscrollfun scroll=glfwSetScrollCallback(glfw, nullptr);
        glfwSetScrollCallback(glfw, scroll);
        const GLFWframebuffersizefun framebufferSize=glfwSetFramebufferSizeCallback(glfw, nullptr);
        glfwSetFramebufferSizeCallback(glfw, framebufferSize);

        check(cursorPos && mouseButton && key && scroll && framebufferSize,
            "the Window installed no GLFW callbacks");
        if(!(cursorPos && mouseButton && key && scroll && framebufferSize))
            return EXIT_FAILURE;

        window.frame();
        window.frame();

        // 2. a click on the Button, and one on the bare canvas
        // the Button's centre, in window coordinates at the window's scale
        const Point at=button->localPosToCanvas();
        const float scale=canvas->scale();

        cursorPos(glfw, (at.x+button->getWidth()/2)*scale, (at.y+button->getHeight()/2)*scale);
        window.frame();
        mouseButton(glfw, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
        window.frame();
        mouseButton(glfw, GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
        window.frame();
        check(presses.count == 1, "the click on the Button pressed it "+std::to_string(presses.count)
            +" times, not once");
        check(buttonsTaken.size() == 2 && buttonsTaken[0] && buttonsTaken[1],
            "the mouseButton hook was not told the UI took the click on the Button");

        cursorPos(glfw, 400.0, 250.0);
        window.frame();
        mouseButton(glfw, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
        mouseButton(glfw, GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
        window.frame();
        check(buttonsTaken.size() == 4 && !buttonsTaken[2] && !buttonsTaken[3],
            "the mouseButton hook was told the UI took a click on the bare canvas");
        check(presses.count == 1, "the click on the bare canvas pressed the Button");

        // 3. keyFirst before the canvas
        key(glfw, GLFW_KEY_F5, 0, GLFW_PRESS, 0);
        key(glfw, GLFW_KEY_F5, 0, GLFW_RELEASE, 0);
        key(glfw, GLFW_KEY_A, 0, GLFW_PRESS, 0);
        key(glfw, GLFW_KEY_A, 0, GLFW_RELEASE, 0);
        check(keysFirst == 4, "keyFirst saw "+std::to_string(keysFirst)+" of 4 key events");
        check(keysAfter.size() == 2 && keysAfter[0] == GLFW_KEY_A && keysAfter[1] == GLFW_KEY_A,
            "the key hook did not get exactly the events keyFirst left");

        // 4. a wheel over the bare canvas
        scroll(glfw, 0.0, 1.0);
        check(scrollsTaken.size() == 1 && !scrollsTaken[0],
            "the scroll hook was not told the UI left a wheel over the bare canvas");

        // 5. a new framebuffer size fits the canvas
        glfwSetWindowSize(glfw, 400, 300);
        for(int i=0; i < 50; ++i)
            glfwPollEvents();

        Point framebuffer;

        glfwGetFramebufferSize(glfw, &framebuffer.x, &framebuffer.y);
        framebufferSize(glfw, framebuffer.x, framebuffer.y);

        const Point logical=app::logicalSize(framebuffer, app::effectiveScale(glfw, 1.0f));

        check(window.framebufferSize().x == framebuffer.x && window.framebufferSize().y == framebuffer.y, "framebufferSize() did not follow the window");
        check(canvas->getSize().x == logical.x && canvas->getSize().y == logical.y, "the canvas is "+std::to_string(canvas->getSize().x)+"x"
            +std::to_string(canvas->getSize().y)+", not the framebuffer's "+std::to_string(logical.x)+"x"
            +std::to_string(logical.y));

        // 6. a frame shown in the window, read back
        order.clear();

        std::vector<uint8_t> pixels;
        Point shown;
        bool presented=false;

        for(int attempt=0; attempt < 5 && !presented; ++attempt)
        {
            canvas->redraw();
            if(!window.device()->beginWindowFrame())
                continue;
            window.frame();
            presented=window.device()->presentWindowFrame(&pixels, &shown);
        }
        check(presented, "no frame was shown in the window");
        check(order == std::vector<std::string>({"beforeUpdate", "afterUpdate", "beforeRender", "afterRender"}),
            "the frame hooks did not run in their order");

        std::set<uint32_t> colors;

        for(size_t i=0; i+3 < pixels.size() && colors.size() < 2; i+=4)
            colors.insert(uint32_t(pixels[i]) | uint32_t(pixels[i+1]) << 8 | uint32_t(pixels[i+2]) << 16);
        check(colors.size() > 1, "the frame shown in the window is one flat colour");

        // 7. run() until the window closes
        int iterations=0;

        window.hooks.afterIteration=[&](const platform::IdleLoop &)
        {
            if(++iterations == 3)
                glfwSetWindowShouldClose(glfw, GLFW_TRUE);
        };
        canvas->redraw();
        window.run();
        check(iterations >= 3, "run() returned after "+std::to_string(iterations)+" iterations");
    }

    // 8. one after another
    {
        app::Window again(shellOptions(resources));

        check(again.ok(), "a Window after the first one was refused: "+again.error());
    }

    if(g_failures)
        return EXIT_FAILURE;
    std::printf("app_window: passed\n");
    return EXIT_SUCCESS;
}
