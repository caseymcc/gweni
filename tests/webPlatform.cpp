/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  platform.h's browser integration in a real page: the cases run only in the
 *  web tree's gweni_unittests.html (web_unittests, in Chromium), the one
 *  doctest run with a window; natively this file is empty, and the in-process
 *  versions are tests/unit/browserPlatform.cpp's (which the web tree runs too,
 *  under node, where they are what a page without a window gets).
 *
 *  pushUrl()/replaceUrl() are history.pushState/replaceState: location and
 *  history.length follow them, the three forms keep what they say, a <base
 *  href> changes nothing, and a refused string changes neither (one stderr
 *  line). setUrlChangedHandler() listens to popstate: a popstate event runs
 *  the handler with the page's URL, a push or replace does not, and an empty
 *  handler removes the listener (history.back() fires its popstate on a later
 *  task, which a blocking doctest run never reaches: the demo's browser test
 *  covers that). openUrl() is window.open(url, '_blank', 'noopener,noreferrer')
 *  (stubbed, so no tab opens). Storage is localStorage under the prefix (no
 *  "in memory" line, setStorageFile() a no-op), a value over the quota refused
 *  with the store unchanged. timeZoneName() and localeName() are Intl's and
 *  navigator.language, whatever TZ and LANG say. Each case puts back the URL
 *  and the storage it found.
 */
#if defined(__EMSCRIPTEN__)

#include <gweni/platforms/commonPlatform.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <emscripten.h>

#include <cstdlib>
#include <string>
#include <vector>

EM_JS_DEPS(gweni_web_platform_test, "$stringToNewUTF8,$UTF8ToString");

// A JavaScript expression's value, as a string
EM_JS(char *, gweni_test_js, (const char *expression), {
    return stringToNewUTF8(String((0, eval)(UTF8ToString(expression))));
});

using namespace gweni;

namespace
{

std::string js(const char *expression)
{
    char *text=gweni_test_js(expression);
    std::string result(text);

    free(text);
    return result;
}

std::vector<std::string> g_errors;

void recordError(log::Level level, const char *message)
{
    if(level >= log::Level::Error)
        g_errors.push_back(message);
}

//! The error lines logged while it lives, and the page's URL put back after.
struct PageState
{
    PageState():
        m_url(js("location.href"))
    {
        g_errors.clear();
        m_previous=log::setLogCaller(&recordError);
    }
    ~PageState()
    {
        log::setLogCaller(m_previous);
        platform::setUrlChangedHandler(nullptr);
        EM_ASM({ history.replaceState(null, "", UTF8ToString($0)); }, m_url.c_str());
    }

    std::vector<std::string> take()
    {
        std::vector<std::string> lines;

        lines.swap(g_errors);
        return lines;
    }

    std::string m_url;
    log::LogCaller m_previous;
};

}//namespace

TEST_CASE("browser platform: pushUrl and replaceUrl are history.pushState and replaceState")
{
    PageState page;
    const std::string length=js("history.length");

    REQUIRE(platform::replaceUrl("/app/tasks/42?view=backlog#top"));
    CHECK(js("location.pathname") == "/app/tasks/42");
    CHECK(js("location.search") == "?view=backlog");
    CHECK(js("location.hash") == "#top");
    CHECK(platform::currentUrl() == "/app/tasks/42?view=backlog#top");
    CHECK(js("history.length") == length);

    REQUIRE(platform::pushUrl("?view=hosts"));
    CHECK(platform::currentUrl() == "/app/tasks/42?view=hosts");
    REQUIRE(platform::pushUrl("#line-3"));
    CHECK(platform::currentUrl() == "/app/tasks/42?view=hosts#line-3");
    REQUIRE(platform::pushUrl("/files/a%2Fb/%C3%A9//v1.2/?q=a+b%27c#%20x"));
    // the browser keeps the encoded form as it was pushed
    CHECK(js("location.pathname") == "/files/a%2Fb/%C3%A9//v1.2/");
    CHECK(platform::currentUrl() == "/files/a%2Fb/%C3%A9//v1.2/?q=a+b%27c#%20x");
    CHECK(js("history.length") == std::to_string(std::stoi(length)+3));

    // a <base href> changes nothing: the forms are made whole before pushState
    EM_ASM({
        var base = document.createElement("base");
        base.id = "gweni-test-base";
        base.href = "/somewhere/else/";
        document.head.appendChild(base);
    });
    REQUIRE(platform::replaceUrl("?q=1"));
    CHECK(platform::currentUrl() == "/files/a%2Fb/%C3%A9//v1.2/?q=1");
    REQUIRE(platform::replaceUrl("#h"));
    CHECK(platform::currentUrl() == "/files/a%2Fb/%C3%A9//v1.2/?q=1#h");
    EM_ASM({ document.getElementById("gweni-test-base").remove(); });

    for(const char *url:{"", "tasks/42", "https://example.com/x", "//host/x", "/a/../b",
        "/a/%2e/b", "/a b", "/\xc3\xa9", "?q=it's", "/a|b"})
    {
        INFO(url);
        CHECK_FALSE(platform::pushUrl(url));
        CHECK_FALSE(platform::replaceUrl(url));
        CHECK(page.take().size() == 2);
        CHECK(platform::currentUrl() == "/files/a%2Fb/%C3%A9//v1.2/?q=1#h");
    }
    CHECK(js("history.length") == std::to_string(std::stoi(length)+3));
    CHECK(page.take().empty());
}

TEST_CASE("browser platform: the URL-changed handler listens to popstate")
{
    PageState page;
    std::vector<std::string> calls;

    REQUIRE(platform::replaceUrl("/one?a=1"));
    platform::setUrlChangedHandler([&calls](const String &url) { calls.push_back(url); });

    // the app's own changes are not delivered
    REQUIRE(platform::pushUrl("/two"));
    REQUIRE(platform::replaceUrl("#h"));
    CHECK(calls.empty());

    // a popstate event (dispatched here, so it runs inside the call) is
    EM_ASM({ window.dispatchEvent(new PopStateEvent("popstate")); });
    CHECK(calls == std::vector<std::string>{"/two#h"});

    // a second handler replaces the first, with one listener
    std::vector<std::string> second;

    platform::setUrlChangedHandler([&second](const String &url) { second.push_back(url); });
    EM_ASM({ window.dispatchEvent(new PopStateEvent("popstate")); });
    CHECK(calls.size() == 1);
    CHECK(second == std::vector<std::string>{"/two#h"});

    // no handler, no listener
    platform::setUrlChangedHandler(nullptr);
    CHECK(js("Module.gweniPopstate") == "null");
    EM_ASM({ window.dispatchEvent(new PopStateEvent("popstate")); });
    CHECK(second.size() == 1);
    CHECK(page.take().empty());
}

TEST_CASE("browser platform: openUrl is window.open with noopener")
{
    PageState page;

    EM_ASM({
        Module.gweniTestOpen = window.open;
        Module.gweniTestOpened = [];
        window.open = function() {
            Module.gweniTestOpened.push(Array.prototype.slice.call(arguments));
            return null;
        };
    });
    platform::setOpenUrlHandler(nullptr);

    // window.open gives null with noopener, opened or blocked alike
    CHECK(platform::openUrl("https://example.com/a?b=c#d"));
    CHECK(platform::openUrl("mailto:someone@example.com"));
    CHECK_FALSE(platform::openUrl("javascript:alert(1)"));
    CHECK_FALSE(platform::openUrl("file:///etc/passwd"));
    CHECK(page.take().size() == 2);
    CHECK(js("JSON.stringify(Module.gweniTestOpened)") ==
        "[[\"https://example.com/a?b=c#d\",\"_blank\",\"noopener,noreferrer\"],"
        "[\"mailto:someone@example.com\",\"_blank\",\"noopener,noreferrer\"]]");

    EM_ASM({ window.open = Module.gweniTestOpen; });
}

TEST_CASE("browser platform: storage is localStorage under the prefix")
{
    PageState page;
    const std::string prefix="gweni-unittests/";
    const std::string text="h\xc3\xa9llo \"quoted\"\n\xf0\x9f\x98\x80";

    platform::setStoragePrefix(prefix);
    for(const std::string &key:platform::storageKeys())
        platform::storageRemove(key);
    EM_ASM({ localStorage.setItem("other-app/draft", "theirs"); });

    REQUIRE(platform::storageSet("draft", text));
    REQUIRE(platform::storageSet("last-page", "ButtonDemo"));
    CHECK(js("localStorage.getItem('gweni-unittests/last-page')") == "ButtonDemo");
    CHECK(js("encodeURIComponent(localStorage.getItem('gweni-unittests/draft'))") ==
        "h%C3%A9llo%20%22quoted%22%0A%F0%9F%98%80");
    CHECK(platform::storageGet("draft") == text);
    CHECK_FALSE(platform::storageGet("missing").has_value());
    CHECK(platform::storageKeys() == std::vector<std::string>{"draft", "last-page"});

    // the store is localStorage: a file changes nothing
    CHECK(platform::setStorageFile("/nowhere/store.json"));
    CHECK(platform::storageGet("last-page") == std::string("ButtonDemo"));

    // over the quota: refused, the old value kept
    REQUIRE(platform::storageSet("big", "small"));
    CHECK_FALSE(platform::storageSet("big", std::string(12*1024*1024, 'a')));
    CHECK(platform::storageGet("big") == std::string("small"));

    REQUIRE(platform::storageRemove("draft"));
    REQUIRE(platform::storageRemove("big"));
    CHECK(js("localStorage.getItem('gweni-unittests/draft')") == "null");
    CHECK(platform::storageKeys() == std::vector<std::string>{"last-page"});
    CHECK(js("localStorage.getItem('other-app/draft')") == "theirs");

    platform::storageRemove("last-page");
    EM_ASM({ localStorage.removeItem("other-app/draft"); });
    platform::setStoragePrefix("");
    // localStorage is backed: no "in memory" line
    CHECK(page.take().empty());
}

TEST_CASE("browser platform: the time zone and locale are the browser's")
{
    setenv("TZ", "Europe/Berlin", 1);
    setenv("LANG", "de_DE.UTF-8", 1);
    CHECK(platform::timeZoneName() == js("Intl.DateTimeFormat().resolvedOptions().timeZone"));
    CHECK(platform::localeName() == js("navigator.language"));
    CHECK_FALSE(platform::localeName().empty());
    unsetenv("TZ");
    unsetenv("LANG");
}

#endif
