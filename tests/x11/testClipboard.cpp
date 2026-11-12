/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  gweni_test_clipboard: "the other application" of clipboard_x11, an X11
 *  client that reads the CLIPBOARD selection and then owns it, as a text
 *  editor beside a gweni window would.
 *
 *    gweni_test_clipboard --expect <utf8> --offer <utf8> [--timeout <s>]
 *        -- <command> [args...]
 *
 *  It runs <command> and, while it runs:
 *    1. asks for CLIPBOARD as UTF8_STRING every 50 ms until the answer is
 *       exactly <expect> (no owner, or another text, is asked again);
 *    2. then takes CLIPBOARD and serves it: TARGETS (TARGETS, UTF8_STRING)
 *       and UTF8_STRING (<offer>), refusing any other target.
 *  It exits with <command>'s status (128+N for a signal N) if that is not 0;
 *  otherwise 0 only if step 1 read <expect> and step 2 served <offer> at
 *  least once, else 1. Past <timeout> (default 120 s) it stops <command> and
 *  exits 1. Each step prints one line, the texts quoted with their bytes
 *  escaped, so a mismatch shows what came.
 */
#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>

namespace
{

const char *g_tool="gweni_test_clipboard";

// a requestor gone before its answer is not an error here
int onError(Display *, XErrorEvent *)
{
    return 0;
}

double seconds()
{
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<double>(now.tv_sec)+now.tv_nsec*1e-9;
}

// printable ASCII as it is, every other byte as \xNN
std::string quoted(const std::string &text)
{
    std::string out="\"";
    for(unsigned char c : text)
    {
        if(c >= 0x20 && c < 0x7f && c != '"' && c != '\\')
            out+=static_cast<char>(c);
        else
        {
            char escaped[8];
            std::snprintf(escaped, sizeof(escaped), "\\x%02x", c);
            out+=escaped;
        }
    }
    return out+"\"";
}

struct Atoms
{
    Atom clipboard, utf8, targets, incr, property;
};

// The property a SelectionNotify left on our window, deleted as it is read.
bool readProperty(Display *display, Window window, const Atoms &atoms, std::string &text)
{
    Atom type=None;
    int format=0;
    unsigned long count=0, after=0;
    unsigned char *data=nullptr;

    if(XGetWindowProperty(display, window, atoms.property, 0, LONG_MAX/4, True, AnyPropertyType,
        &type, &format, &count, &after, &data) != Success)
        return false;

    const bool ok=(type == atoms.utf8) && (format == 8);
    if(type == atoms.incr)
        std::fprintf(stderr, "%s: the owner sent the text INCR, which this helper does not read\n", g_tool);
    if(ok)
        text.assign(reinterpret_cast<const char *>(data), count);
    if(data)
        XFree(data);
    return ok;
}

void answer(Display *display, const XSelectionRequestEvent &request, const Atoms &atoms,
    const std::string &offer, int &served)
{
    XSelectionEvent notify{};

    notify.type=SelectionNotify;
    notify.display=request.display;
    notify.requestor=request.requestor;
    notify.selection=request.selection;
    notify.target=request.target;
    notify.time=request.time;
    // an obsolete client names no property: the target is its name
    notify.property=(request.property != None) ? request.property : request.target;

    if(request.selection != atoms.clipboard)
        notify.property=None;
    else if(request.target == atoms.targets)
    {
        const Atom list[2]={ atoms.targets, atoms.utf8 };

        XChangeProperty(display, request.requestor, notify.property, XA_ATOM, 32, PropModeReplace,
            reinterpret_cast<const unsigned char *>(list), 2);
    }
    else if(request.target == atoms.utf8)
    {
        XChangeProperty(display, request.requestor, notify.property, atoms.utf8, 8, PropModeReplace,
            reinterpret_cast<const unsigned char *>(offer.data()), static_cast<int>(offer.size()));
        ++served;
    }
    else
        notify.property=None;

    XSendEvent(display, request.requestor, False, NoEventMask, reinterpret_cast<XEvent *>(&notify));
}

int childStatus(int status)
{
    if(WIFEXITED(status))
        return WEXITSTATUS(status);
    return 128+(WIFSIGNALED(status) ? WTERMSIG(status) : 0);
}

}//namespace

int main(int argc, char *argv[])
{
    std::string expect, offer;
    bool haveExpect=false, haveOffer=false;
    double timeout=120.0;
    int command=0;

    for(int i=1; i < argc; ++i)
    {
        const std::string arg=argv[i];
        if(arg == "--" && i+1 < argc){ command=i+1; break; }
        else if(arg == "--expect" && i+1 < argc){ expect=argv[++i]; haveExpect=true; }
        else if(arg == "--offer" && i+1 < argc){ offer=argv[++i]; haveOffer=true; }
        else if(arg == "--timeout" && i+1 < argc) timeout=std::atof(argv[++i]);
        else break;
    }
    if(!haveExpect || !haveOffer || command == 0)
    {
        std::fprintf(stderr, "usage: %s --expect <utf8> --offer <utf8> [--timeout <s>] -- <command> [args...]\n",
            g_tool);
        return 2;
    }

    Display *display=XOpenDisplay(nullptr);

    if(!display)
    {
        std::fprintf(stderr, "%s: cannot open display %s\n", g_tool, XDisplayName(nullptr));
        return 1;
    }
    XSetErrorHandler(&onError);

    const Window window=XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0, 1, 1, 0, 0, 0);
    const Atoms atoms{
        XInternAtom(display, "CLIPBOARD", False),
        XInternAtom(display, "UTF8_STRING", False),
        XInternAtom(display, "TARGETS", False),
        XInternAtom(display, "INCR", False),
        XInternAtom(display, "GWENI_TEST_CLIPBOARD", False)
    };
    XFlush(display);

    const pid_t child=fork();

    if(child < 0)
    {
        std::fprintf(stderr, "%s: fork failed: %s\n", g_tool, std::strerror(errno));
        return 1;
    }
    if(child == 0)
    {
        close(ConnectionNumber(display));
        execvp(argv[command], argv+command);
        std::fprintf(stderr, "%s: cannot run %s: %s\n", g_tool, argv[command], std::strerror(errno));
        _exit(127);
    }

    const double start=seconds();
    bool read=false;            // step 1 done
    bool asking=false;          // a conversion is in flight
    double lastAsk=0.0;
    std::string lastText;       // the last text an answer held
    bool sawText=false;
    int served=0;               // UTF8_STRING answers with the offer

    for(;;)
    {
        int status=0;

        if(waitpid(child, &status, WNOHANG) == child)
        {
            XCloseDisplay(display);
            std::printf("%s: served the offer %d time(s)\n", g_tool, served);
            if(!read)
                std::fprintf(stderr, "%s: never read %s from CLIPBOARD; the last text it read was %s\n", g_tool,
                    quoted(expect).c_str(), sawText ? quoted(lastText).c_str() : "none (no owner or no text)");
            if(childStatus(status) != 0)
            {
                std::fprintf(stderr, "%s: %s exited %d\n", g_tool, argv[command], childStatus(status));
                return childStatus(status);
            }
            if(!read)
                return 1;
            if(served == 0)
            {
                std::fprintf(stderr, "%s: nothing asked for the offered text\n", g_tool);
                return 1;
            }
            return 0;
        }

        if(seconds()-start > timeout)
        {
            std::fprintf(stderr, "%s: timed out after %.0f s (read=%s, served=%d); stopping %s\n", g_tool,
                timeout, read ? "yes" : "no", served, argv[command]);
            kill(child, SIGTERM);
            waitpid(child, &status, 0);
            return 1;
        }

        if(!read && !asking && seconds()-lastAsk >= 0.05)
        {
            XConvertSelection(display, atoms.clipboard, atoms.utf8, atoms.property, window, CurrentTime);
            asking=true;
            lastAsk=seconds();
        }

        while(XPending(display) > 0)
        {
            XEvent event;

            XNextEvent(display, &event);
            if(event.type == SelectionNotify && !read)
            {
                asking=false;
                std::string text;

                const bool hasText=(event.xselection.property != None)
                    && readProperty(display, window, atoms, text);
                if(hasText)
                {
                    lastText=text;
                    sawText=true;
                }
                if(hasText && text == expect)
                {
                    read=true;
                    std::printf("%s: read %s from CLIPBOARD\n", g_tool, quoted(text).c_str());
                    XSetSelectionOwner(display, atoms.clipboard, window, CurrentTime);
                    if(XGetSelectionOwner(display, atoms.clipboard) != window)
                    {
                        std::fprintf(stderr, "%s: could not take CLIPBOARD\n", g_tool);
                        kill(child, SIGTERM);
                        waitpid(child, &status, 0);
                        return 1;
                    }
                    std::printf("%s: owns CLIPBOARD, offering %s\n", g_tool, quoted(offer).c_str());
                    std::fflush(stdout);
                }
            }
            else if(event.type == SelectionRequest && read)
                answer(display, event.xselectionrequest, atoms, offer, served);
            else if(event.type == SelectionClear)
                std::printf("%s: lost CLIPBOARD\n", g_tool);
        }
        XFlush(display);

        pollfd fd{ ConnectionNumber(display), POLLIN, 0 };

        poll(&fd, 1, 20);
    }
}
