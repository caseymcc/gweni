/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  See license in Gweni.h
 */


#ifndef _gweni_platform_h_
#define _gweni_platform_h_

#include <gweni/platforms/commonPlatform.h>

namespace gweni
{
//! The platform abstraction layer.
//! Each platform implements these functions in their own specific way.
namespace platform
{
//! Go to sleep for a time. Stops CPU hogging.
//! @param ms : Number of milliseconds to sleep for.
GWENI_EXPORT void sleep(unsigned int ms);

// Needed for things like double click
GWENI_EXPORT float getTimeInSeconds();

//! Get the directory of the running executable.
//! \return Path string, blank if not known.
//! \note Path is terminated with directory separator.
GWENI_EXPORT String getExecutableDir();

//! Set the system cursor.
//! @param cursor : Cursor number to use. @see CursorType
GWENI_EXPORT void setCursor(unsigned char cursor);

GWENI_EXPORT void getCursorPos(gweni::Point &p);
GWENI_EXPORT void getDesktopSize(int &w, int &h);

//! Get platform clipboard as a string.
GWENI_EXPORT String getClipboardText();
//! Set platform clipboard from a string.
GWENI_EXPORT bool   setClipboardText(const String &str);

//
// System Dialogs (Can return false if unhandled)
//
GWENI_EXPORT bool fileOpen(const String &Name, const String &StartPath,
    const String &Extension, String &filePathOut);
GWENI_EXPORT bool fileSave(const String &Name, const String &StartPath,
    const String &Extension, String &filePathOut);
GWENI_EXPORT bool folderOpen(const String &Name, const String &StartPath,
    String &filePathOut);

//
// Window Creation
//
GWENI_EXPORT void *createPlatformWindow(int x, int y, int w, int h,
    const gweni::String &windowTitle);
GWENI_EXPORT void destroyPlatformWindow(void *ptr);
GWENI_EXPORT void setBoundsPlatformWindow(void *ptr, int x, int y, int w, int h);
// Returns true for redraw.
GWENI_EXPORT bool messagePump(void *window);
GWENI_EXPORT bool isFocussedPlatformWindow(void *ptr);
GWENI_EXPORT void setWindowMaximized(void *ptr, bool bMaximized, gweni::Point &newPos,
    gweni::Point &newSize);
GWENI_EXPORT void setWindowMinimized(void *ptr, bool bMinimized);

//! If the user created the window, notify the plaform so it can do its housekeeping.
GWENI_EXPORT void setPlatformWindow(void *handle);

}//namespace platform
}//namespace gweni

#endif //_gweni_platform_h_
