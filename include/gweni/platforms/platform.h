/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  See license in Gweni.h
 */


#ifndef _gweni_platform_h_
#define _gweni_platform_h_

#include <gweni/platforms/commonPlatform.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

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

//! Install a clock for getTimeInSeconds() to read instead of the platform one.
//! Everything time-dependent in the library (animations, progress bars, the text
//! caret blink, double-click and key-repeat timing) then runs off it, which is what
//! makes a capture or a scripted input sequence reproducible.
//! @param source : The clock, in seconds. An empty function restores the platform clock.
GWENI_EXPORT void setTimeSource(std::function<float()> source);

//! The profiling clock, in microseconds: what the library times its own work
//! with (Canvas::updateCanvas() and renderCanvas() in CanvasFrameStats::times,
//! SlotRenderer::flush() in its frame counts), apart from getTimeInSeconds(),
//! which a capture steps a frame at a time. The default is std::chrono's
//! steady_clock; only differences between two reads mean anything.
GWENI_EXPORT uint64_t profileTimeMicros();

//! Install a clock for profileTimeMicros() to read instead of steady_clock. A
//! capture installs one that advances a fixed step per read, so the times it
//! shows depend on the reads a frame makes and not on how fast it ran.
//! @param clock : The clock, in microseconds. An empty function restores steady_clock.
GWENI_EXPORT void setProfileClock(std::function<uint64_t()> clock);

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
// The URL and the history
//
// The page's URL on the web, and an in-process stand-in for it everywhere
// else, so an app that keeps its state in the URL (a deep link, a page in the
// query) is written once. The helpers below are plain string functions, the
// same on every target.
//

//! A URL's path, query and hash, each in its encoded (percent-encoded) form
//! and without its delimiter: "/app/tasks/42?view=backlog#top" is
//! {"/app/tasks/42", "view=backlog", "top"}.
struct GWENI_EXPORT Url
{
    String path;
    String query;
    String hash;
};

//! Split a path, query and hash (what currentUrl() gives) at the first '?'
//! and the first '#'. Nothing is decoded or checked.
GWENI_EXPORT Url parseUrl(const String &url);
//! The inverse of parseUrl(): path, then '?' and the query and '#' and the
//! hash, each only when it is not empty.
GWENI_EXPORT String formatUrl(const Url &url);

//! A query's key=value pairs, decoded, in their order (a key can repeat).
typedef std::vector<std::pair<String, String>> QueryParams;

//! Decode "key=value&..." (a leading '?' skipped): '+' is a space and %XX a
//! byte, so UTF-8 comes back as it was encoded. A key without '=' has an
//! empty value; empty pieces ("a=1&&b=2") are skipped.
GWENI_EXPORT QueryParams parseQuery(const String &query);
//! Encode pairs as "key=value&...", without a leading '?': a space is '+',
//! and every byte but A-Z a-z 0-9 - . _ ~ is %XX. A key with an empty value
//! is written alone ("flag"), as parseQuery() reads it.
GWENI_EXPORT String buildQuery(const QueryParams &params);

//! "/app/tasks/42" is {"app", "tasks", "42"}, each segment percent-decoded on
//! its own, so "%2F" is a '/' inside its segment ('+' stays a '+'). An empty
//! segment is kept ("/a//b" is {"a", "", "b"}, a trailing '/' an empty last
//! segment), and "/" (or "") is no segment at all.
GWENI_EXPORT std::vector<String> splitPath(const String &path);
//! The inverse of splitPath(): '/' before each segment, each one encoded on
//! its own (a '/' in it is "%2F"); no segment is "/".
GWENI_EXPORT String joinPath(const std::vector<String> &segments);

//! The current path, query and hash, encoded (on the web location.pathname +
//! location.search + location.hash); elsewhere the in-process history's
//! current entry, "/" until the app pushes or replaces one.
GWENI_EXPORT String currentUrl();

//! Add a history entry and make it current (history.pushState on the web),
//! dropping the entries forward of the current one. url takes one of three
//! forms, with the same rules on every target:
//! - "/path[?query][#hash]" replaces all three parts;
//! - "?query[#hash]" keeps the path;
//! - "#hash" keeps the path and the query.
//! Anything else is refused: a relative path ("tasks/42"), a scheme or a host
//! ("https://...", "//host/x"), a "." or ".." path segment (encoded or not), a
//! character a browser would percent-encode itself (a space, a control or
//! non-ASCII byte, " < > \ ^ ` { | }, and ' in the query: encode it), so
//! there is no resolver and the targets cannot disagree. A refused call
//! changes nothing, prints one line on stderr and returns false.
GWENI_EXPORT bool pushUrl(const String &url);
//! Replace the current history entry (history.replaceState on the web), with
//! the same forms and refusals as pushUrl(). An app given a deep link on its
//! command line passes it here at start-up.
GWENI_EXPORT bool replaceUrl(const String &url);

//! Move one entry back in the history, if there is one (history.back() on the
//! web, which moves on a later task).
GWENI_EXPORT void historyBack();
//! Move one entry forward in the history, if there is one.
GWENI_EXPORT void historyForward();

//! Called with currentUrl() whenever the history moves to another entry:
//! historyBack()/historyForward(), and on the web the browser's own back and
//! forward and a hash the user edited (popstate). Never for the app's own
//! pushUrl()/replaceUrl(). Elsewhere it runs inside historyBack() or
//! historyForward(); on the web on a later task of its own, between frames.
typedef std::function<void(const String &url)> UrlChangedHandler;
//! @param handler : The handler; an empty function removes it.
GWENI_EXPORT void setUrlChangedHandler(UrlChangedHandler handler);

//! Open an http, https or mailto URL outside the app: a new tab on the web
//! (window.open with noopener, so a blocked popup cannot be told apart), the
//! system's opener elsewhere (xdg-open, open on macOS, ShellExecute on
//! Windows, given the URL as one argument, never through a shell). Any other
//! scheme (javascript:, file:, ...), an http(s) URL without a host, or a
//! space or control character is refused: one line on stderr and false.
//! \return False if refused, or if the opener says it failed.
GWENI_EXPORT bool openUrl(const String &url);
//! What opens an allowed URL, returning whether it did.
typedef std::function<bool(const String &url)> OpenUrlHandler;
//! Replace the opener openUrl() calls after its checks, for tests.
//! @param handler : The opener; an empty function restores the platform's.
GWENI_EXPORT void setOpenUrlHandler(OpenUrlHandler handler);

//
// Local storage
//
// UTF-8 strings kept across runs: localStorage on the web, one JSON file the
// app names elsewhere (setStorageFile), rewritten whole (a temporary file
// renamed over it) on every change. Two instances sharing a file each write
// what they hold, the last one wins: it is not a database.
//

//! Put prefix before every key in the store, so apps sharing one (an origin's
//! localStorage, a file) do not collide; storageKeys() lists the keys under it.
GWENI_EXPORT void setStoragePrefix(const String &prefix);
//! Keep the values in the JSON file at path, loading what it holds (none if
//! it does not exist) in place of the values held now. An empty path keeps
//! them in memory, starting empty; until a file is set they are in memory, and
//! the first use says so once on stderr. On the web localStorage is the store
//! and this does nothing.
//! \return False if the file exists but cannot be read as a storage file (the
//! store is then empty, and the next change rewrites it).
GWENI_EXPORT bool setStorageFile(const String &path);
//! The value stored under key, if there is one.
GWENI_EXPORT std::optional<String> storageGet(const String &key);
//! Store value under key. \return False if it could not be written (a full
//! quota, a disk error), the store then holding what it held before.
GWENI_EXPORT bool storageSet(const String &key, const String &value);
//! Remove key. \return False if that could not be written; true if it is gone
//! (or was never there).
GWENI_EXPORT bool storageRemove(const String &key);
//! The keys stored (under the prefix, without it), sorted.
GWENI_EXPORT std::vector<String> storageKeys();

//
// Time zone and locale
//

//! The IANA time zone name ("Europe/Berlin"): on the web the browser's
//! (Intl.DateTimeFormat().resolvedOptions().timeZone), elsewhere TZ or the
//! zoneinfo file /etc/localtime links to, "UTC" with neither; empty where
//! not known. localtime_r already uses the same zone on every target.
GWENI_EXPORT String timeZoneName();
//! The BCP 47 locale name ("en-US"): on the web navigator.language,
//! elsewhere LC_ALL or LANG with its codeset and modifier dropped
//! ("en_US.UTF-8" is "en-US"); "en-US" for C, POSIX or neither set.
GWENI_EXPORT String localeName();

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
