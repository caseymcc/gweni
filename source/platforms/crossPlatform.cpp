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

#include "urlTarget.h"

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <Windows.h>
#   include <shellapi.h>
#   undef min
#   undef max
#else
#   include <fcntl.h>
#   include <sys/wait.h>
#   include <unistd.h>
#endif
#include <errno.h>
#include <limits.h>
#include <time.h>

#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <map>
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

#if defined(__EMSCRIPTEN__)

// The browser's side of platform.h's URL, history, opener, storage and names.
// Under node (the web tree's unit tests) there is no window: gweni_js_in_browser
// is 0 and every function keeps to the in-process versions the other targets use.

EM_JS_DEPS(gweni_platform_browser, "$stringToNewUTF8,$UTF8ToString");

EM_JS(int, gweni_js_in_browser, (), {
    return (typeof window !== 'undefined' && typeof window.history !== 'undefined' &&
        typeof window.location !== 'undefined')?1:0;
});

EM_JS(char *, gweni_js_current_url, (), {
    return stringToNewUTF8(location.pathname + location.search + location.hash);
});

// url is the whole path, query and hash (resolveUrlTarget made it), made
// absolute against location.href, not the document's base URL, so a <base
// href> changes nothing. Returns null, or why the browser refused it.
EM_JS(char *, gweni_js_set_url, (const char *url, int push), {
    try
    {
        var target = new URL(UTF8ToString(url), location.href).href;
        if(push)
            history.pushState(null, "", target);
        else
            history.replaceState(history.state, "", target);
        return 0;
    }
    catch(e)
    {
        return stringToNewUTF8(String(e));
    }
});

EM_JS(void, gweni_js_history_go, (int step), {
    if(step < 0)
        history.back();
    else
        history.forward();
});

// The popstate listener: each event (the browser's back and forward, a hash
// edited in the address bar, historyBack()/historyForward() a task later) is
// a task of its own, outside the main loop's frame, and runs the handler.
EM_JS(void, gweni_js_listen_popstate, (int on), {
    if(on && !Module['gweniPopstate'])
    {
        Module['gweniPopstate'] = function() { Module['_gweni_platform_url_changed'](); };
        window.addEventListener('popstate', Module['gweniPopstate']);
    }
    else if(!on && Module['gweniPopstate'])
    {
        window.removeEventListener('popstate', Module['gweniPopstate']);
        Module['gweniPopstate'] = null;
    }
});

// noopener makes window.open return null whether or not a popup blocker
// stopped it, so a blocked tab cannot be told apart: 1 unless it threw.
EM_JS(int, gweni_js_open_url, (const char *url), {
    try
    {
        window.open(UTF8ToString(url), '_blank', 'noopener,noreferrer');
        return 1;
    }
    catch(e)
    {
        return 0;
    }
});

// 1 if localStorage can be used: reading it throws where the page may not
// (storage blocked, a sandboxed frame).
EM_JS(int, gweni_js_storage_available, (), {
    try
    {
        return (typeof window !== 'undefined' && window.localStorage &&
            typeof window.localStorage.length === 'number')?1:0;
    }
    catch(e)
    {
        return 0;
    }
});

// The value, or null if there is none (or it cannot be read).
EM_JS(char *, gweni_js_storage_get, (const char *key), {
    try
    {
        var value = localStorage.getItem(UTF8ToString(key));
        return (value === null)?0:stringToNewUTF8(value);
    }
    catch(e)
    {
        return 0;
    }
});

// 0 if setItem threw (a full quota)
EM_JS(int, gweni_js_storage_set, (const char *key, const char *value), {
    try
    {
        localStorage.setItem(UTF8ToString(key), UTF8ToString(value));
        return 1;
    }
    catch(e)
    {
        return 0;
    }
});

EM_JS(int, gweni_js_storage_remove, (const char *key), {
    try
    {
        localStorage.removeItem(UTF8ToString(key));
        return 1;
    }
    catch(e)
    {
        return 0;
    }
});

// The keys under prefix, without it, as a JSON array.
EM_JS(char *, gweni_js_storage_keys, (const char *prefix), {
    var keys = [];
    try
    {
        var start = UTF8ToString(prefix);
        for(var i = 0; i < localStorage.length; ++i)
        {
            var key = localStorage.key(i);
            if(key !== null && key.startsWith(start))
                keys.push(key.substring(start.length));
        }
    }
    catch(e)
    {
    }
    return stringToNewUTF8(JSON.stringify(keys));
});

EM_JS(char *, gweni_js_time_zone, (), {
    var zone = "";
    try
    {
        zone = Intl.DateTimeFormat().resolvedOptions().timeZone || "";
    }
    catch(e)
    {
    }
    return stringToNewUTF8(zone);
});

EM_JS(char *, gweni_js_locale, (), {
    return stringToNewUTF8((typeof navigator !== 'undefined' && navigator.language) || 'en-US');
});

// The browser's clipboard (useBrowserClipboard), through the page's copy, cut
// and paste events. gweni's Ctrl+C/X/V run on keydown, inside emscripten-glfw's
// keyboard listener, which leaves those three keys to the browser, so the
// event follows in the same task: the copy or cut event writes the text the
// keystroke's setClipboardText() left (gweni_js_clipboard_write), the paste
// event answers the keystroke's requestClipboardText()
// (gweni_js_clipboard_expect_paste). The listeners are on window in the
// capture phase, so they run before emscripten-glfw's on document, which on a
// copy with no DOM selection writes ' ' and reads a paste after gweni asked.
// A copy or paste for another element of the page (a text field, a selection
// in the page's own text) is left to the browser.
EM_JS(void, gweni_js_clipboard_listen, (int on), {
    var state = Module['gweniClipboard'];
    if(on && !state)
    {
        // what web_input reads: writes, the copy and cut events gweni wrote
        // in ('copy', 'cut'); answers, how each paste was answered ('paste',
        // 'readText' or 'none')
        state = Module['gweniClipboard'] = { copy: null, pasteWanted: false, pasted: '', writes: [], answers: [] };
        var forPage = function()
        {
            var active = document.activeElement;
            return active && active !== document.body && active !== document.documentElement
                && active !== Module['canvas'];
        };
        state.onCopy = function(e)
        {
            if(forPage())
                return;
            // nothing to copy (no selection in the focused box): the clipboard
            // is left as it is, not emscripten-glfw's ' '
            e.stopImmediatePropagation();
            if(state.copy === null)
                return;
            e.clipboardData.setData('text/plain', state.copy);
            e.preventDefault();
            state.copy = null;
            state.writes.push(e.type);
        };
        state.answer = function(text, how)
        {
            state.answers.push(how);
            state.pasted = (text === null) ? '' : text;
            Module['_gweni_platform_clipboard_pasted']((text === null) ? 0 : 1);
        };
        state.onPaste = function(e)
        {
            if(!state.pasteWanted || forPage())
                return;
            state.pasteWanted = false;
            e.preventDefault();
            e.stopImmediatePropagation();
            state.answer((e.clipboardData || window.clipboardData).getData('text/plain'), 'paste');
        };
        window.addEventListener('copy', state.onCopy, true);
        window.addEventListener('cut', state.onCopy, true);
        window.addEventListener('paste', state.onPaste, true);
    }
    else if(!on && state)
    {
        window.removeEventListener('copy', state.onCopy, true);
        window.removeEventListener('cut', state.onCopy, true);
        window.removeEventListener('paste', state.onPaste, true);
        Module['gweniClipboard'] = null;
    }
});

// The text for the copy or cut event this task's keystroke fires; a copy with
// no such event (a button's click) is written through navigator.clipboard,
// inside the click's user activation. The keystroke's write gives the same text.
EM_JS(void, gweni_js_clipboard_write, (const char *text), {
    var state = Module['gweniClipboard'];
    var value = UTF8ToString(text);
    if(state)
    {
        state.copy = value;
        setTimeout(function() { state.copy = null; }, 0);
    }
    if(navigator.clipboard && navigator.clipboard.writeText)
        navigator.clipboard.writeText(value).catch(function() {});
});

// A paste is wanted: the paste event this task's keystroke fires answers it.
// With none (a paste from a click, not a key), a task later the clipboard is
// read through navigator.clipboard, if the page may; else nothing is pasted.
EM_JS(void, gweni_js_clipboard_expect_paste, (), {
    var state = Module['gweniClipboard'];
    if(!state || state.pasteWanted)
        return;
    state.pasteWanted = true;
    setTimeout(function()
    {
        if(!state.pasteWanted || Module['gweniClipboard'] !== state)
            return;
        state.pasteWanted = false;
        if(navigator.clipboard && navigator.clipboard.readText)
            navigator.clipboard.readText().then(
                function(text) { state.answer(text, 'readText'); },
                function() { state.answer(null, 'none'); });
        else
            state.answer(null, 'none');
    }, 0);
});

EM_JS(char *, gweni_js_clipboard_pasted, (), {
    var state = Module['gweniClipboard'];
    return stringToNewUTF8(state ? state.pasted : '');
});

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

// Wall time since the first call, from steady_clock. Not clock(), which is the
// process's CPU time over all its threads: with a rasteriser on several threads
// it ran many times faster than wall time (and slower with an idle GPU driver),
// so a window's caret blink, tooltip delay, animations and the stats overlay's
// fps and refresh ran on it.
float platformTimeInSeconds()
{
    using Clock=std::chrono::steady_clock;
    static const Clock::time_point start=Clock::now();

    return static_cast<float>(std::chrono::duration<double>(Clock::now()-start).count());
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

namespace
{
struct ClipboardBackend
{
    std::function<String()> get;
    std::function<bool(const String &)> set;
    std::function<void(ClipboardReply)> request;
};

ClipboardBackend &clipboardBackend()
{
    static ClipboardBackend backend;
    return backend;
}
}//namespace

void setClipboardBackend(std::function<String()> get, std::function<bool(const String &)> set,
    std::function<void(ClipboardReply)> request)
{
    ClipboardBackend &backend=clipboardBackend();

    backend.get=std::move(get);
    backend.set=std::move(set);
    backend.request=std::move(request);
}

String getClipboardText()
{
    if(clipboardBackend().get)
        return clipboardBackend().get();
#if defined(__EMSCRIPTEN__)
    reportClipboard();
#endif
    return gs_ClipboardEmulator;
}

bool setClipboardText(const String &str)
{
    if(clipboardBackend().set)
        return clipboardBackend().set(str);
#if defined(__EMSCRIPTEN__)
    reportClipboard();
#endif
    gs_ClipboardEmulator=str;
    return true;
}

void requestClipboardText(ClipboardReply reply)
{
    if(!reply)
        return;

    // a copy: the request may install another backend before it returns
    std::function<void(ClipboardReply)> request=clipboardBackend().request;

    if(request)
        request(std::move(reply));
    else
        reply(getClipboardText());
}

#if defined(__EMSCRIPTEN__)
namespace
{
// the text last copied or pasted in the page: what getClipboardText() knows
String g_browserClipboard;
// the requestClipboardText() replies the next paste answers
std::vector<ClipboardReply> g_browserPastes;
}//namespace

void deliverBrowserPaste(bool hasText)
{
    std::vector<ClipboardReply> replies;
    replies.swap(g_browserPastes);
    if(!hasText)
        return;

    char *text=gweni_js_clipboard_pasted();
    g_browserClipboard=text ? text : "";
    free(text);

    for(ClipboardReply &reply : replies)
        reply(g_browserClipboard);
}

void useBrowserClipboard(bool on)
{
    // under node (the web tree's unit tests) there is no page
    if(!gweni_js_in_browser())
        return;

    gweni_js_clipboard_listen(on ? 1 : 0);
    g_browserPastes.clear();
    if(!on)
    {
        setClipboardBackend(nullptr, nullptr, nullptr);
        return;
    }

    setClipboardBackend(
        []() { return g_browserClipboard; },
        [](const String &text)
        {
            g_browserClipboard=text;
            gweni_js_clipboard_write(text.c_str());
            return true;
        },
        [](ClipboardReply reply)
        {
            g_browserPastes.push_back(std::move(reply));
            gweni_js_clipboard_expect_paste();
        });
}
#else
void useBrowserClipboard(bool)
{}
#endif

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

//
// The URL and the history
//

namespace
{

//! A URL in a log line: control bytes as \xHH (so it stays one line) and at
//! most 200 bytes of it.
String printableUrl(const String &url)
{
    static const char digits[]="0123456789abcdef";
    String out;

    for(const char ch:url)
    {
        const unsigned char c=static_cast<unsigned char>(ch);

        if(out.size() >= 200)
        {
            out+="...";
            break;
        }
        if(c < 0x20 || c == 0x7f)
        {
            out+="\\x";
            out+=digits[c >> 4];
            out+=digits[c & 0xf];
        }
        else
            out+=ch;
    }
    return out;
}

#if defined(__EMSCRIPTEN__)

//! In a page (not under node), where the URL, history and opener are the
//! browser's.
bool inBrowser()
{
    static const bool browser=(gweni_js_in_browser() != 0);
    return browser;
}

//! A string the JS side made with stringToNewUTF8, freed; empty for null.
String takeJsString(char *text)
{
    if(!text)
        return String();

    String result(text);

    free(text);
    return result;
}

//! Removes the popstate listener when the runtime exits (a capture's main
//! returning), so no event calls into a runtime that is gone.
struct PopstateListener
{
    ~PopstateListener()
    {
        gweni_js_listen_popstate(0);
    }
};

#endif

//! The in-process history: its entries, formatted, and the current one. It
//! starts at "/", with nothing back or forward.
struct History
{
    std::vector<String> entries{String("/")};
    size_t current=0;
    UrlChangedHandler handler;
};

History &history()
{
    static History history;
    return history;
}

//! The URL url names from the current one, or false after one line on stderr.
bool resolveTarget(const char *call, const String &url, Url &resultOut)
{
    String why;

    if(detail::resolveUrlTarget(parseUrl(currentUrl()), url, resultOut, why))
        return true;
    log::write(log::Level::Error, "gweni: %s(\"%s\") refused: %s", call,
        printableUrl(url).c_str(), why.c_str());
    return false;
}

//! Make target the current entry (a new one for push): the browser's history
//! in a page, else the in-process one. \return False after one line on
//! stderr if the browser refused it.
bool setUrl(const char *call, const String &url, const Url &target, bool push)
{
    const String formatted=formatUrl(target);
#if defined(__EMSCRIPTEN__)
    if(inBrowser())
    {
        const String error=takeJsString(gweni_js_set_url(formatted.c_str(), push?1:0));

        if(error.empty())
            return true;
        log::write(log::Level::Error, "gweni: %s(\"%s\") refused by the browser: %s", call,
            printableUrl(url).c_str(), printableUrl(error).c_str());
        return false;
    }
#else
    (void)call;
    (void)url;
#endif

    History &state=history();

    if(push)
    {
        state.entries.resize(state.current+1);
        state.entries.push_back(formatted);
        ++state.current;
    }
    else
        state.entries[state.current]=formatted;
    return true;
}

//! Tell the handler the history moved.
void deliverUrlChanged()
{
    // a copy: the handler may set another one, or push
    const UrlChangedHandler handler=history().handler;

    if(handler)
        handler(currentUrl());
}

//! Move the history by step entries if it has one there, then tell the handler
//! (in a page the browser does, on a later task: popstate).
void moveHistory(int step)
{
#if defined(__EMSCRIPTEN__)
    if(inBrowser())
    {
        gweni_js_history_go(step);
        return;
    }
#endif

    History &state=history();

    if(step < 0 && state.current == 0)
        return;
    if(step > 0 && state.current+1 >= state.entries.size())
        return;
    if(step < 0)
        --state.current;
    else
        ++state.current;
    deliverUrlChanged();
}

}//namespace

String currentUrl()
{
#if defined(__EMSCRIPTEN__)
    if(inBrowser())
        return takeJsString(gweni_js_current_url());
#endif

    const History &state=history();
    return state.entries[state.current];
}

bool pushUrl(const String &url)
{
    Url target;

    return resolveTarget("pushUrl", url, target) && setUrl("pushUrl", url, target, true);
}

bool replaceUrl(const String &url)
{
    Url target;

    return resolveTarget("replaceUrl", url, target) && setUrl("replaceUrl", url, target, false);
}

void historyBack()
{
    moveHistory(-1);
}

void historyForward()
{
    moveHistory(1);
}

void setUrlChangedHandler(UrlChangedHandler handler)
{
#if defined(__EMSCRIPTEN__)
    if(inBrowser())
    {
        static PopstateListener listener;

        gweni_js_listen_popstate(handler?1:0);
    }
#endif
    history().handler=std::move(handler);
}

//
// Opening a URL outside the app
//

namespace
{

OpenUrlHandler &openUrlHandler()
{
    static OpenUrlHandler handler;
    return handler;
}

//! Whether openUrl() may open url: http and https with a host, mailto with an
//! address, and no space or control character. \return False with why in whyOut.
bool openableUrl(const String &url, String &whyOut)
{
    for(const char ch:url)
    {
        const unsigned char c=static_cast<unsigned char>(ch);

        if(c <= 0x20 || c == 0x7f)
        {
            whyOut="has a space or a control character";
            return false;
        }
    }

    const size_t colon=url.find(':');
    String scheme=(colon == String::npos)?String():url.substr(0, colon);

    for(char &c:scheme)
        c=static_cast<char>(tolower(static_cast<unsigned char>(c)));

    const String rest=(colon == String::npos)?String():url.substr(colon+1);

    if(scheme == "http" || scheme == "https")
    {
        if(rest.compare(0, 2, "//") != 0 || rest.size() < 3 || rest[2] == '/' ||
            rest[2] == '?' || rest[2] == '#')
        {
            whyOut="an http or https URL needs //host";
            return false;
        }
        return true;
    }
    if(scheme == "mailto")
    {
        if(rest.empty())
        {
            whyOut="a mailto URL needs an address";
            return false;
        }
        return true;
    }
    whyOut="only http, https and mailto URLs are opened";
    return false;
}

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)

//! Run program with argument as its one argument (no shell), in a grandchild
//! the app never has to reap: the child forks it and exits at once. \return
//! Whether the program was started; an exec that fails is reported back
//! through a close-on-exec pipe, which a successful exec closes unwritten.
bool spawnDetached(const char *program, const String &argument)
{
    int report[2];

#if defined(__APPLE__)
    // no pipe2: another thread's fork between the two calls can keep a copy
    if(pipe(report) != 0)
        return false;
    fcntl(report[0], F_SETFD, FD_CLOEXEC);
    fcntl(report[1], F_SETFD, FD_CLOEXEC);
#else
    // close-on-exec from the start, so no other thread's exec keeps it open
    if(pipe2(report, O_CLOEXEC) != 0)
        return false;
#endif

    char *const argv[]={const_cast<char *>(program), const_cast<char *>(argument.c_str()),
        nullptr};
    const pid_t child=fork();

    if(child < 0)
    {
        close(report[0]);
        close(report[1]);
        return false;
    }
    if(child == 0)
    {
        close(report[0]);
        setsid();

        const pid_t opener=fork();

        if(opener == 0)
        {
            execvp(program, argv);

            const int error=errno;

            if(write(report[1], &error, sizeof(error)) < 0)
                _exit(126);
            _exit(127);
        }
        _exit((opener < 0)?1:0);
    }
    close(report[1]);

    int status=0;

    while(waitpid(child, &status, 0) < 0 && errno == EINTR)
    {}

    int execError=0;
    ssize_t got;

    while((got=read(report[0], &execError, sizeof(execError))) < 0 && errno == EINTR)
    {}
    close(report[0]);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 && got == 0;
}

#endif

//! The platform's opener.
bool openWithSystem(const String &url)
{
#if defined(__EMSCRIPTEN__)
    return inBrowser() && gweni_js_open_url(url.c_str()) != 0;
#elif defined(_WIN32)
    return reinterpret_cast<INT_PTR>(ShellExecuteA(nullptr, "open", url.c_str(), nullptr,
        nullptr, SW_SHOWNORMAL)) > 32;
#elif defined(__APPLE__)
    return spawnDetached("open", url);
#else
    return spawnDetached("xdg-open", url);
#endif
}

}//namespace

bool openUrl(const String &url)
{
    String why;

    if(!openableUrl(url, why))
    {
        log::write(log::Level::Error, "gweni: openUrl(\"%s\") refused: %s",
            printableUrl(url).c_str(), why.c_str());
        return false;
    }

    const OpenUrlHandler handler=openUrlHandler();

    return handler?handler(url):openWithSystem(url);
}

void setOpenUrlHandler(OpenUrlHandler handler)
{
    openUrlHandler()=std::move(handler);
}

//
// Local storage
//

namespace
{

typedef std::map<String, String> StorageValues;

//! The store: every key with its prefix, and the file it is kept in (none:
//! in memory).
struct Storage
{
    String file;
    String prefix;
    StorageValues values;
    bool reportedInMemory=false;
};

Storage &storage()
{
    static Storage storage;
    return storage;
}

#if defined(__EMSCRIPTEN__)
//! In a page whose localStorage can be used, which is then the store.
bool webStorage()
{
    static const bool available=inBrowser() && (gweni_js_storage_available() != 0);
    return available;
}
#else
bool webStorage()
{
    return false;
}
#endif

//! The values are in memory: said once per process, on the first use.
void reportInMemory(Storage &state)
{
    if(!state.file.empty() || state.reportedInMemory || webStorage())
        return;
    state.reportedInMemory=true;
    log::write(log::Level::Error, "gweni: storage has no file (platform::setStorageFile), "
        "so its values are kept in memory only");
}

#if defined(_WIN32)
unsigned long processId()
{
    return GetCurrentProcessId();
}
#else
unsigned long processId()
{
    return static_cast<unsigned long>(getpid());
}
#endif

//! Write values to path whole: into a temporary file beside it, flushed to
//! the disk, then renamed over it, so the file is the old one or the new one
//! and never a part of either.
bool writeStorageFile(const String &path, const StorageValues &values)
{
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);

    writer.StartObject();
    for(const auto &value:values)
    {
        writer.Key(value.first.data(), static_cast<rapidjson::SizeType>(value.first.size()));
        writer.String(value.second.data(), static_cast<rapidjson::SizeType>(value.second.size()));
    }
    writer.EndObject();

    String text(buffer.GetString(), buffer.GetSize());
    text+='\n';

    const String temporary=path+".tmp"+std::to_string(processId());
    FILE *file=fopen(temporary.c_str(), "wb");

    if(!file)
        return false;

    bool written=(fwrite(text.data(), 1, text.size(), file) == text.size());

    written=(fflush(file) == 0) && written;
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
    written=written && (fsync(fileno(file)) == 0);
#endif
    written=(fclose(file) == 0) && written;
#if defined(_WIN32)
    written=written && MoveFileExA(temporary.c_str(), path.c_str(),
        MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
#else
    written=written && (rename(temporary.c_str(), path.c_str()) == 0);
#endif
    if(!written)
        remove(temporary.c_str());
    return written;
}

//! Read the storage file at path into valuesOut: no file is no values.
//! \return False if it exists but is not a JSON object (its string members
//! are the values; anything else in it is skipped).
bool readStorageFile(const String &path, StorageValues &valuesOut)
{
    valuesOut.clear();

    FILE *file=fopen(path.c_str(), "rb");

    if(!file)
        return errno == ENOENT;

    String text;
    char chunk[4096];
    size_t got;

    while((got=fread(chunk, 1, sizeof(chunk), file)) > 0)
        text.append(chunk, got);

    const bool readError=(ferror(file) != 0);

    fclose(file);
    if(readError)
        return false;

    rapidjson::Document document;

    document.Parse(text.data(), text.size());
    if(document.HasParseError() || !document.IsObject())
        return false;
    for(auto member=document.MemberBegin(); member != document.MemberEnd(); ++member)
    {
        if(member->name.IsString() && member->value.IsString())
            valuesOut[String(member->name.GetString(), member->name.GetStringLength())]=
                String(member->value.GetString(), member->value.GetStringLength());
    }
    return true;
}

//! Make values the store, writing them to its file if it has one. \return
//! False, the store unchanged, if the file could not be written.
bool commitStorage(Storage &state, StorageValues values)
{
    if(!state.file.empty() && !writeStorageFile(state.file, values))
        return false;
    state.values=std::move(values);
    return true;
}

}//namespace

void setStoragePrefix(const String &prefix)
{
    storage().prefix=prefix;
}

bool setStorageFile(const String &path)
{
    if(webStorage())
        return true;

    Storage &state=storage();

    state.file=path;
    state.values.clear();
    if(path.empty())
        return true;
    if(readStorageFile(path, state.values))
        return true;
    log::write(log::Level::Error, "gweni: storage file \"%s\" cannot be read; starting empty",
        printableUrl(path).c_str());
    return false;
}

std::optional<String> storageGet(const String &key)
{
    Storage &state=storage();

    reportInMemory(state);
#if defined(__EMSCRIPTEN__)
    if(webStorage())
    {
        char *value=gweni_js_storage_get((state.prefix+key).c_str());

        if(!value)
            return std::nullopt;
        return takeJsString(value);
    }
#endif

    const auto value=state.values.find(state.prefix+key);

    if(value == state.values.end())
        return std::nullopt;
    return value->second;
}

bool storageSet(const String &key, const String &value)
{
    Storage &state=storage();

    reportInMemory(state);
#if defined(__EMSCRIPTEN__)
    if(webStorage())
        return gweni_js_storage_set((state.prefix+key).c_str(), value.c_str()) != 0;
#endif

    const auto current=state.values.find(state.prefix+key);

    // the value already there: no rewrite of the file
    if(current != state.values.end() && current->second == value)
        return true;

    StorageValues values=state.values;

    values[state.prefix+key]=value;
    return commitStorage(state, std::move(values));
}

bool storageRemove(const String &key)
{
    Storage &state=storage();

    reportInMemory(state);
#if defined(__EMSCRIPTEN__)
    if(webStorage())
        return gweni_js_storage_remove((state.prefix+key).c_str()) != 0;
#endif
    if(state.values.find(state.prefix+key) == state.values.end())
        return true;

    StorageValues values=state.values;

    values.erase(state.prefix+key);
    return commitStorage(state, std::move(values));
}

std::vector<String> storageKeys()
{
    Storage &state=storage();
    std::vector<String> keys;

    reportInMemory(state);
#if defined(__EMSCRIPTEN__)
    if(webStorage())
    {
        const String list=takeJsString(gweni_js_storage_keys(state.prefix.c_str()));
        rapidjson::Document document;

        document.Parse(list.data(), list.size());
        if(!document.HasParseError() && document.IsArray())
        {
            for(const auto &key:document.GetArray())
            {
                if(key.IsString())
                    keys.emplace_back(key.GetString(), key.GetStringLength());
            }
        }
        // localStorage's order is the browser's; sorted by bytes, as elsewhere
        std::sort(keys.begin(), keys.end());
        return keys;
    }
#endif
    for(const auto &value:state.values)
    {
        if(value.first.compare(0, state.prefix.size(), state.prefix) == 0)
            keys.push_back(value.first.substr(state.prefix.size()));
    }
    return keys;
}

//
// Time zone and locale
//

namespace
{

#if !defined(_WIN32)
//! The zone name in a zoneinfo file's path ("/usr/share/zoneinfo/Europe/Berlin"
//! is "Europe/Berlin", its posix/ and right/ trees too); empty if it is none.
String zoneFromPath(const String &path)
{
    const size_t at=path.find("zoneinfo/");

    if(at == String::npos)
        return String();

    String name=path.substr(at+9);

    for(const char *tree:{"posix/", "right/"})
    {
        if(name.compare(0, strlen(tree), tree) == 0)
            name.erase(0, strlen(tree));
    }
    return name;
}
#endif

}//namespace

String timeZoneName()
{
#if defined(__EMSCRIPTEN__)
    if(inBrowser())
        return takeJsString(gweni_js_time_zone());
#endif
#if defined(_WIN32)
    // Windows has zone names of its own, not IANA's
    return String();
#else
    // TZ, when set, is the zone (empty, as glibc reads it, UTC); a ':' before
    // it is the POSIX "implementation-defined" mark, a path a zoneinfo file
    if(const char *tz=getenv("TZ"))
    {
        String name(tz);

        if(!name.empty() && name[0] == ':')
            name.erase(0, 1);
        if(name.empty())
            return "UTC";
        if(name[0] == '/')
        {
            const String zone=zoneFromPath(name);
            return zone.empty()?name:zone;
        }
        return name;
    }

    char link[PATH_MAX];
    const ssize_t length=readlink("/etc/localtime", link, sizeof(link));

    // a link outside a zoneinfo tree names no zone
    if(length > 0 && length < static_cast<ssize_t>(sizeof(link)))
        return zoneFromPath(String(link, static_cast<size_t>(length)));
    // no /etc/localtime: glibc's UTC
    if(length < 0 && errno == ENOENT)
        return "UTC";

    // a copy, not a link: Debian's /etc/timezone names it, else it is not known
    if(FILE *file=fopen("/etc/timezone", "r"))
    {
        char line[256];
        String name;

        if(fgets(line, sizeof(line), file))
            name=line;
        fclose(file);
        while(!name.empty() && std::isspace(static_cast<unsigned char>(name.back())))
            name.pop_back();
        return name;
    }
    return String();
#endif
}

String localeName()
{
#if defined(__EMSCRIPTEN__)
    if(inBrowser())
        return takeJsString(gweni_js_locale());
#endif
#if defined(_WIN32)
    wchar_t name[LOCALE_NAME_MAX_LENGTH];

    if(GetUserDefaultLocaleName(name, LOCALE_NAME_MAX_LENGTH) > 0)
    {
        String narrow;

        for(const wchar_t *c=name; *c; ++c)
            narrow+=static_cast<char>(*c);
        return narrow;
    }
    return "en-US";
#else
    String name;

    for(const char *variable:{"LC_ALL", "LANG"})
    {
        const char *value=getenv(variable);

        if(value && *value)
        {
            name=value;
            break;
        }
    }

    // "en_US.UTF-8@euro": the language and region alone, joined by '-'
    name=name.substr(0, name.find_first_of(".@"));
    if(name.empty() || name == "C" || name == "POSIX")
        return "en-US";
    for(char &c:name)
    {
        if(c == '_')
            c='-';
    }
    return name;
#endif
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

#if defined(__EMSCRIPTEN__)
// Called by the popstate listener (gweni_js_listen_popstate), between frames.
extern "C" EMSCRIPTEN_KEEPALIVE void gweni_platform_url_changed()
{
    gweni::platform::deliverUrlChanged();
}

// Called by the paste listener (gweni_js_clipboard_listen) with the pasted
// text in Module.gweniClipboard, or with 0 when nothing can be pasted.
extern "C" EMSCRIPTEN_KEEPALIVE void gweni_platform_clipboard_pasted(int hasText)
{
    gweni::platform::deliverBrowserPaste(hasText != 0);
}
#endif
