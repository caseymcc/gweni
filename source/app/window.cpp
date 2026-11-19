/*
 *  Gweni - a gweni window (gweni/app/window.h), and gweni_app's one-at-a-time
 *  rule (instance.h).
 *
 *  See license in Gweni.h
 */

#include <gweni/app/window.h>

#include "instance.h"

#include <gweni/controls/canvas.h>
#include <gweni/platforms/renderers/slotRenderer.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <iostream>
#include <utility>

namespace gweni
{
namespace app
{
namespace internal
{

static const char *g_instanceOwner=nullptr;

bool claimInstance(const char *who, std::string &error)
{
    if(g_instanceOwner)
    {
        error=std::string(who)+" refused: an "+g_instanceOwner
            +" is alive; one app::Window or app::Headless at a time in a process";
        std::cerr<<toolName()<<": "<<error<<"\n";
        return false;
    }
    g_instanceOwner=who;
    return true;
}

void releaseInstance()
{
    g_instanceOwner=nullptr;
}

}//namespace internal

Window::Window(const ShellOptions &shell, const WindowOptions &options):
    m_options(options)
{
    if(!internal::claimInstance("app::Window", m_error))
        return;
    m_claimed=true;

    ShellOptions windowShell=shell;

    windowShell.capturing=false;
    if(!m_shell.open(windowShell))
    {
        m_error="app::Window: the window's device did not open";
        return;
    }
    show();
}

Window::Window(Shell &&shell, const WindowOptions &options):
    m_shell(std::move(shell)),
    m_options(options)
{
    if(!internal::claimInstance("app::Window", m_error))
        return;
    m_claimed=true;
    show();
}

Window::~Window()
{
    if(GLFWwindow *window=glfwWindow())
    {
        if(of(window) == this)
            glfwSetWindowUserPointer(window, nullptr);
    }
    if(m_ok)
        m_input.useBrowserInput(false);
    if(m_claimed)
        internal::releaseInstance();
}

controls::Canvas *Window::canvas() const
{
    return m_shell.canvas.get();
}

renderer::SlotRenderer *Window::renderer() const
{
    return m_shell.renderer.get();
}

skin::textured::TexturedSkin *Window::skin() const
{
    return m_shell.skin.get();
}

Device *Window::device() const
{
    return m_shell.device.get();
}

GLFWwindow *Window::glfwWindow() const
{
    return m_shell.device ? m_shell.device->context().window : nullptr;
}

void Window::show()
{
    GLFWwindow *window=glfwWindow();

    if(!window || !m_shell.canvas || !m_shell.renderer)
    {
        m_error="app::Window: there is no window to show (a capture's Shell?)";
        std::cerr<<toolName()<<": "<<m_error<<"\n";
        return;
    }

    // the theme's, once the skin has loaded it
    m_background=m_options.background ? *m_options.background
        : skin::textured::tokyonight::Canvas.top;
    m_framebuffer=Point(0, 0);

    m_input.initialize(m_shell.canvas.get());
    // a page's touches and text field (nothing natively)
    m_input.useBrowserInput(true);

    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, &Window::keyCallback);
    glfwSetCharCallback(window, &Window::charCallback);
    glfwSetWindowFocusCallback(window, &Window::focusCallback);
    glfwSetCursorPosCallback(window, &Window::cursorPosCallback);
    glfwSetCursorEnterCallback(window, &Window::cursorEnterCallback);
    glfwSetMouseButtonCallback(window, &Window::mouseButtonCallback);
    glfwSetScrollCallback(window, &Window::scrollCallback);
    glfwSetFramebufferSizeCallback(window, &Window::framebufferSizeCallback);
    glfwSetWindowContentScaleCallback(window, &Window::contentScaleCallback);

    // The framebuffer's real size and the window's content scale (the
    // framebuffer differs from the window's size on a high-DPI display).
    fit();
    m_ok=true;
}

Window *Window::of(GLFWwindow *window)
{
    return static_cast<Window *>(glfwGetWindowUserPointer(window));
}

void Window::fit()
{
    GLFWwindow *window=glfwWindow();

    if(!window || !m_shell.canvas || !m_shell.renderer)
        return;
    glfwGetFramebufferSize(window, &m_framebuffer.x, &m_framebuffer.y);
    fitCanvasToWindow(window, m_shell.scale, *m_shell.renderer, *m_shell.canvas, m_input);
}

void Window::frame()
{
    controls::Canvas *canvas=m_shell.canvas.get();

    if(hooks.beforeUpdate)
        hooks.beforeUpdate();
    canvas->updateCanvas();
    if(hooks.afterUpdate)
        hooks.afterUpdate();
    if(m_options.clear)
        m_shell.renderer->clearTarget(m_background);
    if(hooks.beforeRender)
        hooks.beforeRender();
    canvas->renderCanvas();
    if(hooks.afterRender)
        hooks.afterRender();
}

void Window::run()
{
    if(!m_ok)
        return;
    // The frame holds only this Window, which the browser's loop needs on the
    // heap: there the loop never returns and main's stack is gone.
    m_shell.device->runWindowLoop(m_shell.canvas.get(), m_shell.renderer.get(), m_options.loop,
        [this]() { frame(); },
        [this](const platform::IdleLoop &loop)
        {
            if(hooks.afterIteration)
                hooks.afterIteration(loop);
        });
}

void Window::keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    Window *self=of(window);

    if(!self)
        return;

    const input::GLFW3::KeyEvent event={key, scancode, action, mods};

    if(self->hooks.keyFirst && self->hooks.keyFirst(event))
        return;

    const bool taken=self->m_input.processKeyEvent(event);

    if(self->hooks.key)
        self->hooks.key(event, taken);
}

void Window::charCallback(GLFWwindow *window, unsigned int codepoint)
{
    if(Window *self=of(window))
        self->m_input.processCharEvent(codepoint);
}

void Window::focusCallback(GLFWwindow *window, int focused)
{
    Window *self=of(window);

    if(!self)
        return;
    self->m_input.processFocus(focused == GLFW_TRUE);
    if(self->hooks.focus)
        self->hooks.focus(focused == GLFW_TRUE);
}

void Window::cursorPosCallback(GLFWwindow *window, double x, double y)
{
    Window *self=of(window);

    if(!self)
        return;

    const bool taken=self->m_input.processMouseMove(x, y);

    if(self->hooks.mouseMove)
        self->hooks.mouseMove(x, y, taken);
}

void Window::cursorEnterCallback(GLFWwindow *window, int entered)
{
    if(Window *self=of(window))
        self->m_input.processCursorEnter(entered == GLFW_TRUE);
}

void Window::mouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
    Window *self=of(window);

    if(!self)
        return;

    const bool taken=self->m_input.processMouseButtons(button, action, mods);

    if(self->hooks.mouseButton)
        self->hooks.mouseButton(button, action, mods, taken);
}

void Window::scrollCallback(GLFWwindow *window, double x, double y)
{
    Window *self=of(window);

    if(!self)
        return;

    const bool taken=self->m_input.processScroll(float(x), float(y));

    if(self->hooks.scroll)
        self->hooks.scroll(x, y, taken);
}

// The view is the framebuffer in device pixels and the canvas that in logical
// units, so docked controls lay out again to the new size; the cursor is
// mapped through the same sizes and scale.
void Window::framebufferSizeCallback(GLFWwindow *window, int, int)
{
    if(Window *self=of(window))
        self->fit();
}

// The window moved to a monitor with another scale, or the desktop's changed.
void Window::contentScaleCallback(GLFWwindow *window, float, float)
{
    if(Window *self=of(window))
        self->fit();
}

}//namespace app
}//namespace gweni
