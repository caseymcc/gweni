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
#   include <WinBase.h>     // sleep
#   undef min
#   undef max
#else
#   include <unistd.h>
#endif
#include <time.h>

#if defined(__APPLE__)
#   include <errno.h>
#   include <libgen.h>
#   include <libproc.h>
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

#else

    return String();

#endif
}

void sleep(unsigned int ms)
{
    // Only MSVC different.
#ifdef _WIN32
    ::Sleep(ms);
#else
    ::sleep(ms);
#endif
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

float getTimeInSeconds()
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

#else

float getTimeInSeconds()
{
    const float seconds=static_cast<float>(clock()) / CLOCKS_PER_SEC;
    return seconds;
}

#endif

void setCursor(unsigned char iCursor)
{
    // No platform independent way to do this
}

String getClipboardText()
{
    return gs_ClipboardEmulator;
}

bool setClipboardText(const String &str)
{
    gs_ClipboardEmulator=str;
    return true;
}

bool fileOpen(const String &Name, const String &StartPath, const String &Extension,
    String &filePathOut)
{
    return false;
}

bool fileSave(const String &Name, const String &StartPath, const String &Extension,
    String &filePathOut)
{
    return false;
}

bool folderOpen(const String &Name, const String &StartPath, String &filePathOut)
{
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
