/*
 *  Gweni - a gweni window (gweni_app, gweni::app): a Shell's parts shown in a
 *  GLFW window (a GL one, or Vulkan's with its swapchain), every GLFW
 *  callback wired to the canvas through input::GLFW3, the canvas fitted to the
 *  framebuffer and the OS content scale, the system clipboard and, in a
 *  browser page, the page's touches and text field; run() is the window's
 *  idle loop. Hooks let an app take keys before the canvas, use the input the
 *  UI did not take and draw around the canvas.
 *
 *  One app::Window (or app::Headless) is alive at a time in a process: the
 *  clock, the input handler's state and GLFW are the process's. A second is
 *  refused (ok() false, error() saying so); one after another works.
 *
 *  In the browser run() never returns and main's stack is unwound before the
 *  first frame: allocate the Window with new there (and natively too, if the
 *  code is shared), and let its hooks hold only what outlives main's locals
 *  (heap objects, statics, copies), never a reference to one of them.
 *
 *  See license in Gweni.h
 */
#ifndef _gweni_app_window_h_
#define _gweni_app_window_h_

#include <gweni/app/device.h>
#include <gweni/app/shell.h>
#include <gweni/platforms/input/glfw3.h>
#include <gweni/platforms/idleLoop.h>
#include <gweni/platforms/platformTypes.h>

#include <functional>
#include <optional>
#include <string>

namespace gweni
{
namespace app
{

struct WindowOptions
{
    //! how the window draws (loopOptions(): idle, GWENI_IDLE_VERIFY's verify)
    platform::LoopOptions loop=loopOptions();
    //! clear the target each frame before the canvas renders (the canvas
    //! draws no background of its own); off for an app that draws its own
    //! scene first
    bool clear=true;
    //! the colour cleared to; the theme's canvas fill when not given
    std::optional<Color> background;
};

class Window
{
public:
    //! Opens a Shell for the window (\a shell's capturing off) and shows it.
    explicit Window(const ShellOptions &shell, const WindowOptions &options=WindowOptions());
    //! Shows \a shell, opened by the app for its window (not capturing).
    explicit Window(Shell &&shell, const WindowOptions &options=WindowOptions());
    ~Window();

    Window(const Window &)=delete;
    Window &operator=(const Window &)=delete;

    //! Whether the window is shown: false when its Shell did not open, it had
    //! no GLFW window, or another Window or Headless is alive (error() says
    //! which, and its line is on stderr).
    bool ok() const { return m_ok; }
    const std::string &error() const { return m_error; }

    Shell &shell() { return m_shell; }
    controls::Canvas *canvas() const;
    renderer::SlotRenderer *renderer() const;
    skin::textured::TexturedSkin *skin() const;
    Device *device() const;
    GLFWwindow *glfwWindow() const;
    input::GLFW3 &input() { return m_input; }
    //! the framebuffer in device px, as the last fit found it
    Point framebufferSize() const { return m_framebuffer; }
    //! a resource did not load (Shell::degraded)
    bool degraded() const { return m_shell.degraded; }

    //! What the app adds to the window. Each is optional; set them before run().
    struct Hooks
    {
        //! a key event before the canvas sees it (an app's tool keys); true:
        //! taken, the canvas is not given it
        std::function<bool(const input::GLFW3::KeyEvent &)> keyFirst;
        //! each event after the canvas, with whether the UI took it (the
        //! GLFW3::process* answer): the input the UI did not take is the app's
        std::function<void(const input::GLFW3::KeyEvent &, bool taken)> key;
        std::function<void(double x, double y, bool taken)> mouseMove;
        std::function<void(int button, int action, int mods, bool taken)> mouseButton;
        std::function<void(double dx, double dy, bool taken)> scroll;
        std::function<void(bool focused)> focus;
        //! the frame: before the update phase, after it, after the clear
        //! (before the canvas renders: an app's scene, clearDepth()) and after
        //! the render
        std::function<void()> beforeUpdate;
        std::function<void()> afterUpdate;
        std::function<void()> beforeRender;
        std::function<void()> afterRender;
        //! after every loop iteration (natively) or callback (in the browser)
        std::function<void(const platform::IdleLoop &)> afterIteration;
    };
    Hooks hooks;

    //! Fits the canvas, the renderer's view and the cursor mapping to the
    //! framebuffer and the content scale times the Shell's scale
    //! (fitCanvasToWindow); the size and content-scale callbacks call it.
    void fit();
    //! One frame as run() draws it: the hooks, the update, the clear, the render.
    void frame();
    //! The window's loop until it closes (Device::runWindowLoop); in the
    //! browser it never returns.
    void run();

private:
    void show();
    static Window *of(GLFWwindow *window);

    static void keyCallback(GLFWwindow *, int key, int scancode, int action, int mods);
    static void charCallback(GLFWwindow *, unsigned int codepoint);
    static void focusCallback(GLFWwindow *, int focused);
    static void cursorPosCallback(GLFWwindow *, double x, double y);
    static void cursorEnterCallback(GLFWwindow *, int entered);
    static void mouseButtonCallback(GLFWwindow *, int button, int action, int mods);
    static void scrollCallback(GLFWwindow *, double x, double y);
    static void framebufferSizeCallback(GLFWwindow *, int, int);
    static void contentScaleCallback(GLFWwindow *, float, float);

    Shell m_shell;
    WindowOptions m_options;
    input::GLFW3 m_input;
    Color m_background;
    Point m_framebuffer;
    bool m_ok=false;
    bool m_claimed=false;
    std::string m_error;
};

}//namespace app
}//namespace gweni

#endif//_gweni_app_window_h_
