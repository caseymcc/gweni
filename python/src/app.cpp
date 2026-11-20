/*
 *  Gweni - gweni.App (python/src): app::Window, a canvas in a window gweni
 *  owns, and run(), the window's idle loop.
 *
 *  run() releases the GIL for the whole loop, the polls, frames, swaps and
 *  waits included, so other Python threads run while it draws as while it
 *  waits (a turning Spinner draws every iteration and never waits). It is
 *  taken back only to run Python: a callback or a post (callBack, callEvent),
 *  a callable released (~Callable), and the signal check after each
 *  iteration. A scoped release gives it back however the loop ends.
 *
 *  Ctrl+C: Python's SIGINT handler only sets a flag and writes the wakeup fd,
 *  and GLFW's wait retries on EINTR, so run() points signal.set_wakeup_fd at a
 *  pipe of its own. A helper thread blocks on it, and for each byte calls
 *  glfwPostEmptyEvent(), ending the wait, and forwards the byte to the fd that
 *  was set before (an asyncio loop's, say); run() restores that fd when it
 *  returns. After every iteration PyErr_CheckSignals() runs the Python
 *  handlers: an error (KeyboardInterrupt) stops the loop and run() raises it,
 *  as it does the first error a callback raised. The handlers run in the main
 *  thread only, so run() off it raises RuntimeError.
 *
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#include "control.h"
#include "events.h"
#include "handles.h"

#include <gweni/app/window.h>
#include <gweni/controls/canvas.h>
#include <gweni/platforms/idleLoop.h>

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <nanobind/stl/optional.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/string.h>

#include <atomic>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <memory>
#include <optional>
#include <pthread.h>
#include <thread>
#include <unistd.h>

namespace nb=nanobind;

namespace gweni
{
namespace python
{

namespace
{

//! The wakeup fd run() sets for its life: a pipe, read by a helper thread
//! that wakes the loop and passes each byte on to the fd set before.
class WakeupPipe
{
public:
    WakeupPipe()
    {
        int fds[2];

        if(pipe2(fds, O_CLOEXEC) != 0)
            throw std::runtime_error("gweni.App.run: pipe2 failed");
        m_read=fds[0];
        m_write=fds[1];
        // set_wakeup_fd needs a non-blocking fd; the helper's end blocks
        fcntl(m_write, F_SETFL, fcntl(m_write, F_GETFL)|O_NONBLOCK);

        nb::module_ signal=nb::module_::import_("signal");

        m_previous=nb::cast<int>(signal.attr("set_wakeup_fd")(m_write, nb::arg("warn_on_full_buffer")=false));
        m_thread=std::thread([read=m_read, previous=m_previous]() { forward(read, previous); });
    }

    ~WakeupPipe()
    {
        // the earlier fd back first, then the pipe closed, which ends the
        // helper's read
        try
        {
            nb::module_::import_("signal").attr("set_wakeup_fd")(m_previous);
        }
        catch(nb::python_error &error)
        {
            error.discard_as_unraisable("gweni.App.run restoring the wakeup fd");
        }
        close(m_write);
        if(m_thread.joinable())
            m_thread.join();
        close(m_read);
    }

    WakeupPipe(const WakeupPipe &)=delete;
    WakeupPipe &operator=(const WakeupPipe &)=delete;

private:
    static void forward(int read, int previous)
    {
        // signals are the main thread's: none lands here
        sigset_t all;

        sigfillset(&all);
        pthread_sigmask(SIG_BLOCK, &all, nullptr);

        char bytes[64];

        for(;;)
        {
            const ssize_t got=::read(read, bytes, sizeof(bytes));

            if(got < 0 && errno == EINTR)
                continue;
            if(got <= 0)
                return;
            if(previous >= 0)
            {
                // as Python writes it: non-blocking, a full pipe dropping it
                ssize_t written;

                do
                    written=::write(previous, bytes, size_t(got));
                while(written < 0 && errno == EINTR);
            }
            glfwPostEmptyEvent();
        }
    }

    int m_read=-1;
    int m_write=-1;
    int m_previous=-1;
    std::thread m_thread;
};

bool onMainThread()
{
    nb::module_ threading=nb::module_::import_("threading");

    return threading.attr("current_thread")().is(threading.attr("main_thread")());
}

}//namespace

class App
{
public:
    App(const std::string &title, std::pair<int, int> size, std::optional<std::string> resources, float scale)
    {
        app::ShellOptions shell;

        shell.size=Point(size.first, size.second);
        shell.title=title;
        shell.scale=scale;
        shell.resources=resources ? *resources : packageResources();

        app::WindowOptions window;

        window.loop=app::loopOptions();
        m_window=std::make_unique<app::Window>(shell, window);
        if(!m_window->ok())
        {
            const std::string error=m_window->error();

            m_window.reset();
            throw std::runtime_error("gweni.App: "+error);
        }
        m_window->hooks.afterIteration=[this](const platform::IdleLoop &loop) { afterIteration(loop); };
        beginSession("App", m_window->canvas(), [this]() { release(); });
    }

    ~App()
    {
        if(m_window)
            release();
    }

    void close()
    {
        if(!m_window)
            return;
        checkThread();
        checkNotInCallback("App.close()");
        if(m_running)
            throw std::runtime_error("gweni.App.close: run() is running; quit() ends it");
        inCall([this]() { release(); });
    }

    void release()
    {
        m_window.reset();
        endSession();
    }

    app::Window &get()
    {
        if(!m_window)
            throw std::runtime_error("gweni.App: it is closed");
        checkThread();
        return *m_window;
    }

    bool closed() const { return !m_window; }
    long frameCount() const { return m_frames.load(); }

    void quit()
    {
        glfwSetWindowShouldClose(get().glfwWindow(), GLFW_TRUE);
    }

    void run()
    {
        app::Window &window=get();

        checkNotInCallback("App.run()");
        if(m_running)
            throw std::runtime_error("gweni.App.run: it is running already");
        if(!onMainThread())
            throw std::runtime_error("gweni.App.run: called off the main thread (Python delivers signals, "
                "Ctrl+C included, to the main thread only)");

        CallScope scope;
        {
            WakeupPipe wakeup;

            m_running=true;
            m_framesAtRun=m_frames.load();
            glfwSetWindowShouldClose(window.glfwWindow(), GLFW_FALSE);
            try
            {
                nb::gil_scoped_release released;

                window.run();
            }
            catch(...)
            {
                m_running=false;
                throw;
            }
            m_running=false;
        }
        scope.finish();
    }

private:
    void afterIteration(const platform::IdleLoop &loop)
    {
        m_frames=m_framesAtRun+long(loop.frames());

        nb::gil_scoped_acquire gil;

        if(PyErr_CheckSignals() != 0)
            keepCallbackError();
        if(callbackErrorPending())
            glfwSetWindowShouldClose(m_window->glfwWindow(), GLFW_TRUE);
    }

    std::unique_ptr<app::Window> m_window;
    std::atomic<long> m_frames{0};
    long m_framesAtRun=0;
    bool m_running=false;
};

void bindApp(nb::module_ &m)
{
    nb::class_<App>(m, "App",
        "A canvas in a window gweni owns (app::Window), in the textured skin. run() is\n"
        "its idle loop: a frame only when the canvas has work. One App or Headless is\n"
        "alive at a time.")
        .def(nb::init<const std::string &, std::pair<int, int>, std::optional<std::string>, float>(),
            nb::arg("title")="gweni", nb::arg("size")=std::make_pair(800, 600), nb::arg("resources")=nb::none(),
            nb::arg("scale")=1.0f,
            "size is the window's framebuffer in device px, resources a directory to load\n"
            "the skin and fonts from (the package's by default), scale a factor on the OS\n"
            "content scale. Raises RuntimeError without a display.")
        .def("run", &App::run,
            "Shows the window and runs its loop until it is closed or quit() is called,\n"
            "raising what a callback raised and KeyboardInterrupt on Ctrl+C. The main\n"
            "thread only.")
        .def("quit", &App::quit, "Ends run() after the current iteration (a callback may call it).")
        .def("close", &App::close, "Deletes the canvas and the window; the handles go stale.")
        .def_prop_ro("closed", &App::closed)
        .def_prop_ro("frame_count", &App::frameCount,
            "Frames run() has drawn (read from any thread).")
        .def("__enter__", [](nb::handle self) { return nb::borrow<nb::typed<nb::object, App>>(self); })
        .def("__exit__", [](App &self, nb::args) { self.close(); }, nb::sig("def __exit__(self, *args: object) -> None"))
        .def_prop_ro("canvas", [](App &self) { return wrapAs<Canvas>(self.get().canvas()); }, "The canvas's handle.")
        .def_prop_ro("degraded", [](App &self) { return self.get().degraded(); },
            "A resource did not load (its line is on stderr).");
}

}//namespace python
}//namespace gweni
