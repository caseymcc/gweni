/*
 *  Gweni
 *  Copyright (c) 2012 Facepunch Studios
 
 *  See license in Gweni.h
 */

#include <gweni/platforms/platform.h>

namespace gweni
{
namespace platform
{

static String gs_ClipboardEmulator;

String getExecutableDir()
{
    return String();
}

void sleep(unsigned int ms)
{
}

float getTimeInSeconds()
{
    return 0.f;
}

// Default place log messages are sent to.
void defaultLogListener(log::Level lvl, const char *message)
{
    if (lvl >= log::Level::Error)
        fputs(message, stderr);
    else
        fputs(message, stdout);
}

void setCursor(unsigned char iCursor)
{
}

String getClipboardText()
{
    return gs_ClipboardEmulator;
}

bool setClipboardText(const String& str)
{
    gs_ClipboardEmulator = str;
    return true;
}

bool fileOpen(const String& Name, const String& StartPath, const String& Extension,
                             String& filePathOut)
{
    return false;
}

bool fileSave(const String& Name, const String& StartPath, const String& Extension,
                             String& filePathOut)
{
    return false;
}

bool folderOpen(const String& Name, const String& StartPath, String& filePathOut)
{
    return false;
}

void* createPlatformWindow(int x, int y, int w, int h,
                                          const String& strWindowTitle)
{
    return nullptr;
}

void destroyPlatformWindow(void* ptr)
{
}

bool messagePump(void* window)
{
    return false;
}

void setBoundsPlatformWindow(void* ptr, int x, int y, int w, int h)
{
}

void setWindowMaximized(void* ptr, bool bMax, Point& newPos,
                                       Point& newSize)
{
}

void setWindowMinimized(void* ptr, bool bMinimized)
{
}

bool isFocussedPlatformWindow(void* ptr)
{
    return true;
}

void getDesktopSize(int& w, int& h)
{
    w = 1024;
    h = 768;
}

void getCursorPos(Point& po)
{
}

}//namespace platform
}//namespace gweni
