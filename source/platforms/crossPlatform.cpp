/*
 *  Gweni - CrossPlatform
 *
 *  - This is functionality that is implemented in a cross platform way. Some of the
 *    cross-platform APIs are treated as platforms and their APIs used instead.
 *
 *  Copyright (c) 2012 Facepunch Studios

 *  See license in Gweni.h
 */

#include <gweni/platforms/platform.h>
#include <gweni/platforms/commonPlatform.h>

#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <Windows.h>
#   undef min
#   undef max
#else
#   include <unistd.h>
#endif
#include <time.h>

#include <chrono>
#include <thread>

#if defined(__APPLE__)
#   include <errno.h>
#   include <libgen.h>
#   include <libproc.h>
#elif defined(__linux__)
#   include <limits.h>
#elif defined(__EMSCRIPTEN__)
#   include <emscripten.h>
#   include <cstdio>
#endif

namespace gweni
{

namespace log
{

// Default place log messages are sent to.
void defaultLogCaller(Level lvl, const char *message)
{
#if defined(_MSC_VER)
    OutputDebugString(message);
#else
    if(lvl >= log::Level::Error)
        fputs(message, stderr);
    else
        fputs(message, stdout);
#endif
}

}//namespace log

namespace platform
{

static String gs_ClipboardEmulator;

#if defined(__EMSCRIPTEN__)

// What the web build does not have, said once per process on stderr, never
// silently: the file dialogs and the system clipboard.
static void reportMissingOnce(bool &reported, const char *what)
{
    if(reported)
        return;
    reported=true;
    fprintf(stderr, "gweni: %s\n", what);
}

#endif

String getExecutableDir()
{
#if defined(__APPLE__)

    // OSX: Use process information.

    const pid_t pid=getpid();
    char pathbuf[PROC_PIDPATHINFO_MAXSIZE];
    const int ret=proc_pidpath(pid, pathbuf, sizeof(pathbuf));
    if(ret > 0)
    {
        return String(dirname(pathbuf)) + "/";
    }

    // fprintf(stderr, "PID %d: %s\n", pid, strerror(errno));
    return "";

#elif defined(WIN32)

    // Windows: Use the module information.

    char path[MAX_PATH]={'\0'};
    HMODULE hModule=GetModuleHandle(NULL);
    if(hModule != NULL)
    {
        GetModuleFileName(hModule, path, sizeof(path));

        if(path)
        {
            // get directory name
            char *dir=strrchr(path, '\\');
            if(dir)
                dir[1]='\0';
        }
    }
    return String(path);

#elif defined(__linux__)

    // Linux: Resolve the /proc link to the running executable. readlink()
    // does not NUL-terminate, and a result that fills the buffer may have
    // been truncated.

    char path[PATH_MAX];
    const ssize_t len=readlink("/proc/self/exe", path, sizeof(path));
    if(len <= 0 || len >= (ssize_t)sizeof(path))
        return String();

    const String exePath(path, (size_t)len);
    const size_t slash=exePath.rfind('/');
    if(slash == String::npos)
        return String();

    return exePath.substr(0, slash+1);

#elif defined(__EMSCRIPTEN__)

    // Web: there is no executable file. The build preloads resources/ into the
    // virtual file system (MEMFS) at /resources, so the root is the directory
    // RelativeToExecutablePaths("resources") resolves against.

    return "/";

#else

    return String();

#endif
}

// On the web (no threads), sleep_for busy-waits on the main thread: it waits,
// but the page does not draw meanwhile.
void sleep(unsigned int ms)
{
    if(ms == 0)
    {
        std::this_thread::yield();
        return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

#ifdef WIN32

static double GetPerformanceFrequency()
{
    static double Frequency=0.0f;

    if(Frequency == 0.0f)
    {
        __int64 perfFreq;
        QueryPerformanceFrequency((LARGE_INTEGER *)&perfFreq);
        Frequency=1.0 / perfFreq;
    }

    return Frequency;
}

float platformTimeInSeconds()
{
    static float currentTime=0.0f;
    static __int64 iLastTime=0;
    __int64 thistime;

    QueryPerformanceCounter((LARGE_INTEGER *)&thistime);
    double fSecondsDifference=(thistime - iLastTime)*GetPerformanceFrequency();

    if(fSecondsDifference > 0.1)
        fSecondsDifference=0.1;

    currentTime+=fSecondsDifference;
    iLastTime=thistime;
    return currentTime;
}

#elif defined(__EMSCRIPTEN__)

// Wall time since the page (or node) started, from performance.now().
float platformTimeInSeconds()
{
    return static_cast<float>(emscripten_get_now()/1000.0);
}

#else

float platformTimeInSeconds()
{
    const float seconds=static_cast<float>(clock()) / CLOCKS_PER_SEC;
    return seconds;
}

#endif

void setCursor(unsigned char iCursor)
{
    // No platform independent way to do this
#if defined(__EMSCRIPTEN__)
    // The web: the CSS cursor of the page's canvas (Module.canvas), by the CSS
    // name of each cursor:: kind; anything else is the default.
    static const char *const names[cursor::Cursor_Count]=
    {
        "default",      // Normal
        "text",         // Beam
        "ns-resize",    // SizeNS
        "ew-resize",    // SizeWE
        "nwse-resize",  // SizeNWSE
        "nesw-resize",  // SizeNESW
        "move",         // SizeAll
        "not-allowed",  // No
        "wait",         // Wait
        "pointer",      // Finger
    };
    const char *name=(iCursor < cursor::Cursor_Count) ? names[iCursor] : names[cursor::Normal];

    EM_ASM({
        var canvas=(typeof Module !== 'undefined') && Module['canvas'];
        if(canvas && canvas.style)
            canvas.style.cursor=UTF8ToString($0);
    }, name);
#endif
}

#if defined(__EMSCRIPTEN__)
static void reportClipboard()
{
    static bool reported=false;
    reportMissingOnce(reported, "the web build's clipboard is in-process only, not the system clipboard");
}
#endif

String getClipboardText()
{
#if defined(__EMSCRIPTEN__)
    reportClipboard();
#endif
    return gs_ClipboardEmulator;
}

bool setClipboardText(const String &str)
{
#if defined(__EMSCRIPTEN__)
    reportClipboard();
#endif
    gs_ClipboardEmulator=str;
    return true;
}

#if defined(__EMSCRIPTEN__)
static void reportFileDialogs()
{
    static bool reported=false;
    reportMissingOnce(reported, "the web build has no file dialogs; they return cancelled");
}
#endif

bool fileOpen(const String &Name, const String &StartPath, const String &Extension,
    String &filePathOut)
{
#if defined(__EMSCRIPTEN__)
    reportFileDialogs();
#endif
    return false;
}

bool fileSave(const String &Name, const String &StartPath, const String &Extension,
    String &filePathOut)
{
#if defined(__EMSCRIPTEN__)
    reportFileDialogs();
#endif
    return false;
}

bool folderOpen(const String &Name, const String &StartPath, String &filePathOut)
{
#if defined(__EMSCRIPTEN__)
    reportFileDialogs();
#endif
    return false;
}

void *createPlatformWindow(int x, int y, int w, int h,
    const String &strWindowTitle)
{
    return nullptr;
}

void destroyPlatformWindow(void *ptr)
{}

bool messagePump(void *window)
{
    return false;
}

void setBoundsPlatformWindow(void *ptr, int x, int y, int w, int h)
{}

void setWindowMaximized(void *ptr, bool bMax, Point &newPos,
    Point &newSize)
{}

void setWindowMinimized(void *ptr, bool bMinimized)
{}

bool isFocussedPlatformWindow(void *ptr)
{
    return true;
}

void getDesktopSize(int &w, int &h)
{
    w=1024;
    h=768;
}

void getCursorPos(Point &po)
{}

}//namespace platform
}//namespace gweni
