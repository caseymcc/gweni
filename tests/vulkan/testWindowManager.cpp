/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  gweni_test_wm: the least X11 window manager window_vulkan needs, so that
 *  iconifying a window under Xvfb (which has none) does something.
 *
 *    gweni_test_wm <command> [args...]
 *
 *  It takes the root window's substructure redirect (failing, with one line, if
 *  another window manager has it), then runs <command> and manages its windows
 *  until it exits, exiting with its status (128+N for a signal N). Managing is:
 *  a map request is mapped and the window's WM_STATE set to NormalState; a
 *  configure request (a glfwSetWindowSize) is done as asked; and the ICCCM
 *  WM_CHANGE_STATE client message to IconicState (XIconifyWindow, what
 *  glfwIconifyWindow sends) unmaps the window and sets WM_STATE to IconicState,
 *  which GLFW reads as iconified (glfwGetWindowAttrib(GLFW_ICONIFIED) and its
 *  iconify callback). A restore (glfwRestoreWindow) maps the window again,
 *  which comes back as a map request. No frames, no focus, no EWMH.
 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

namespace
{

bool g_otherManager=false;

int onStartupError(Display *, XErrorEvent *error)
{
    if(error->error_code == BadAccess)
        g_otherManager=true;
    return 0;
}

// a window gone before its request is handled is not an error here
int onError(Display *, XErrorEvent *)
{
    return 0;
}

void setState(Display *display, Window window, Atom wmState, long state)
{
    const long data[2]={ state, None };

    XChangeProperty(display, window, wmState, wmState, 32, PropModeReplace,
        reinterpret_cast<const unsigned char *>(data), 2);
}

void handle(Display *display, const XEvent &event, Atom wmState, Atom wmChangeState)
{
    switch(event.type)
    {
    case MapRequest:
        XMapWindow(display, event.xmaprequest.window);
        setState(display, event.xmaprequest.window, wmState, NormalState);
        break;

    case ConfigureRequest:
    {
        const XConfigureRequestEvent &request=event.xconfigurerequest;
        XWindowChanges changes{};

        changes.x=request.x;
        changes.y=request.y;
        changes.width=request.width;
        changes.height=request.height;
        changes.border_width=request.border_width;
        changes.sibling=request.above;
        changes.stack_mode=request.detail;
        XConfigureWindow(display, request.window, (unsigned int)request.value_mask, &changes);
        break;
    }

    case ClientMessage:
        if(event.xclient.message_type == wmChangeState && event.xclient.data.l[0] == IconicState)
        {
            XUnmapWindow(display, event.xclient.window);
            setState(display, event.xclient.window, wmState, IconicState);
        }
        break;

    default:
        break;
    }
}

}//namespace

int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        std::fprintf(stderr, "usage: gweni_test_wm <command> [args...]\n");
        return 2;
    }

    Display *display=XOpenDisplay(nullptr);

    if(!display)
    {
        std::fprintf(stderr, "gweni_test_wm: cannot open display %s\n", XDisplayName(nullptr));
        return 1;
    }

    const Window root=DefaultRootWindow(display);
    const Atom wmState=XInternAtom(display, "WM_STATE", False);
    const Atom wmChangeState=XInternAtom(display, "WM_CHANGE_STATE", False);

    XSetErrorHandler(&onStartupError);
    XSelectInput(display, root, SubstructureRedirectMask|SubstructureNotifyMask);
    XSync(display, False);
    if(g_otherManager)
    {
        std::fprintf(stderr, "gweni_test_wm: another window manager runs on %s\n", XDisplayName(nullptr));
        return 1;
    }
    XSetErrorHandler(&onError);

    // the command's windows are created after the redirect is in place
    const pid_t child=fork();

    if(child < 0)
    {
        std::fprintf(stderr, "gweni_test_wm: fork failed: %s\n", std::strerror(errno));
        return 1;
    }
    if(child == 0)
    {
        close(ConnectionNumber(display));
        execvp(argv[1], argv+1);
        std::fprintf(stderr, "gweni_test_wm: cannot run %s: %s\n", argv[1], std::strerror(errno));
        _exit(127);
    }

    for(;;)
    {
        while(XPending(display) > 0)
        {
            XEvent event;

            XNextEvent(display, &event);
            handle(display, event, wmState, wmChangeState);
        }
        XFlush(display);

        int status=0;
        const pid_t done=waitpid(child, &status, WNOHANG);

        if(done == child)
        {
            XCloseDisplay(display);
            if(WIFEXITED(status))
                return WEXITSTATUS(status);
            return 128+(WIFSIGNALED(status) ? WTERMSIG(status) : 0);
        }

        pollfd fd{ ConnectionNumber(display), POLLIN, 0 };

        poll(&fd, 1, 20);
    }
}
