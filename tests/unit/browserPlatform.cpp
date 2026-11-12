/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  platform.h's browser integration as every target but the web page has it
 *  (the web tree's node run included): the URL helpers (parseUrl/formatUrl,
 *  parseQuery/buildQuery, splitPath/joinPath, percent-encoding, '+', an
 *  encoded '/', empty segments, a trailing '/', UTF-8), the three forms
 *  pushUrl()/replaceUrl() take and every kind of string they refuse (false,
 *  one stderr line through log::write, nothing changed), the in-process
 *  history starting at "/" (back and forward, a push dropping the forward
 *  entries) and its handler (called on back and forward, never on a push or
 *  replace), openUrl()'s scheme allowlist through setOpenUrlHandler() and,
 *  on Linux, the system opener (an xdg-open of the case's own on PATH) given
 *  the URL as its one argument with no shell between, false with none, the
 *  file-backed storage (written whole with no temporary file left, read back
 *  by a fresh load, a failed write refused with the store unchanged, the
 *  prefix) and the in-memory one, and the time zone and locale names from TZ,
 *  LC_ALL and LANG. The history and the store are process-global and the
 *  cases may share a process, so each case sets its own start: a
 *  replaceUrl(), a setStorageFile().
 */
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>

#include <cstdio>
#include <string>
#include <vector>

using namespace gweni;
using platform::QueryParams;

namespace
{

std::vector<std::string> g_errors;

void recordError(log::Level level, const char *message)
{
    if(level >= log::Level::Error)
        g_errors.push_back(message);
}

//! The error lines logged while it lives
struct ErrorLog
{
    ErrorLog()
    {
        g_errors.clear();
        m_previous=log::setLogCaller(&recordError);
    }
    ~ErrorLog()
    {
        log::setLogCaller(m_previous);
    }

    //! The lines logged so far, forgotten
    std::vector<std::string> take()
    {
        std::vector<std::string> lines;

        lines.swap(g_errors);
        return lines;
    }

    log::LogCaller m_previous;
};

//! Sets an environment variable (or unsets it, for nullptr) while it lives.
struct ScopedEnv
{
    ScopedEnv(const char *name, const char *value):
        m_name(name)
    {
        const char *previous=getenv(name);

        m_had=(previous != nullptr);
        if(m_had)
            m_previous=previous;
        if(value)
            setenv(name, value, 1);
        else
            unsetenv(name);
    }
    ~ScopedEnv()
    {
        if(m_had)
            setenv(m_name.c_str(), m_previous.c_str(), 1);
        else
            unsetenv(m_name.c_str());
    }

    std::string m_name;
    std::string m_previous;
    bool m_had=false;
};

//! A scratch directory, removed with what is left in it.
struct ScratchDir
{
    ScratchDir()
    {
        char path[]="/tmp/gweni-storage-XXXXXX";

        REQUIRE(mkdtemp(path) != nullptr);
        m_path=path;
    }
    ~ScratchDir()
    {
        for(const std::string &name:entries())
            remove((m_path+"/"+name).c_str());
        rmdir(m_path.c_str());
    }

    std::vector<std::string> entries() const
    {
        std::vector<std::string> names;

        if(DIR *dir=opendir(m_path.c_str()))
        {
            while(const dirent *entry=readdir(dir))
            {
                const std::string name=entry->d_name;

                if(name != "." && name != "..")
                    names.push_back(name);
            }
            closedir(dir);
        }
        return names;
    }

    std::string m_path;
};

std::string readFile(const std::string &path)
{
    std::string text;

    if(FILE *file=fopen(path.c_str(), "rb"))
    {
        char chunk[512];
        size_t got;

        while((got=fread(chunk, 1, sizeof(chunk), file)) > 0)
            text.append(chunk, got);
        fclose(file);
    }
    return text;
}

void writeFile(const std::string &path, const std::string &text)
{
    FILE *file=fopen(path.c_str(), "wb");

    REQUIRE(file != nullptr);
    fwrite(text.data(), 1, text.size(), file);
    fclose(file);
}

}//namespace

TEST_CASE("platform url: parseUrl splits at the first ? and # and formatUrl joins them")
{
    platform::Url url=platform::parseUrl("/app/tasks/42?view=backlog&task=42#top");

    CHECK(url.path == "/app/tasks/42");
    CHECK(url.query == "view=backlog&task=42");
    CHECK(url.hash == "top");
    CHECK(platform::formatUrl(url) == "/app/tasks/42?view=backlog&task=42#top");

    // a '?' after the '#' is the hash's, a second '?' the query's
    url=platform::parseUrl("/a#b?c");
    CHECK(url.path == "/a");
    CHECK(url.query.empty());
    CHECK(url.hash == "b?c");
    url=platform::parseUrl("/a?b?c#d#e");
    CHECK(url.query == "b?c");
    CHECK(url.hash == "d#e");

    // nothing is decoded
    url=platform::parseUrl("/a%2Fb?x=%20");
    CHECK(url.path == "/a%2Fb");
    CHECK(url.query == "x=%20");

    // an empty query or hash has no delimiter
    CHECK(platform::formatUrl(platform::parseUrl("/a?#")) == "/a");
    CHECK(platform::formatUrl({"/a", "", "h"}) == "/a#h");
    CHECK(platform::formatUrl({"/", "q=1", ""}) == "/?q=1");
}

TEST_CASE("platform url: parseQuery decodes and buildQuery encodes key=value pairs")
{
    const QueryParams params=platform::parseQuery(
        "?demo=ButtonDemo&q=a+b%2Bc&name=%C3%A9t%C3%A9&flag&&k=1&k=2&bad=%zz%4");

    REQUIRE(params.size() == 7);
    CHECK(params[0] == QueryParams::value_type("demo", "ButtonDemo"));
    CHECK(params[1] == QueryParams::value_type("q", "a b+c"));
    CHECK(params[2] == QueryParams::value_type("name", "\xc3\xa9t\xc3\xa9"));
    CHECK(params[3] == QueryParams::value_type("flag", ""));
    CHECK(params[4] == QueryParams::value_type("k", "1"));
    CHECK(params[5] == QueryParams::value_type("k", "2"));
    // a '%' without two hex digits stays as it is
    CHECK(params[6] == QueryParams::value_type("bad", "%zz%4"));

    CHECK(platform::parseQuery("").empty());
    CHECK(platform::parseQuery("?").empty());
    CHECK(platform::parseQuery("a=b=c") == QueryParams{{"a", "b=c"}});

    const QueryParams odd={{"a b", "c&d=e"}, {"path", "/x/y?z#w"}, {"utf8", "\xc3\xa9"},
        {"plus", "1+1"}, {"keep", "A-z_0.9~"}, {"empty", ""}};
    const std::string built=platform::buildQuery(odd);

    CHECK(built == "a+b=c%26d%3De&path=%2Fx%2Fy%3Fz%23w&utf8=%C3%A9&plus=1%2B1"
        "&keep=A-z_0.9~&empty");
    CHECK(platform::parseQuery(built) == odd);
    CHECK(platform::buildQuery({}).empty());
    // a flag stays a flag, "flag=" becomes one
    CHECK(platform::buildQuery(platform::parseQuery("stats-overlay&demo=X&off=")) ==
        "stats-overlay&demo=X&off");
    CHECK(platform::buildQuery({{"", ""}}) == "=");
}

TEST_CASE("platform url: splitPath decodes and joinPath encodes each segment on its own")
{
    using Segments=std::vector<std::string>;

    CHECK(platform::splitPath("/app/tasks/42") == Segments{"app", "tasks", "42"});
    // an encoded '/' stays inside its segment, '+' is a '+', UTF-8 comes back
    CHECK(platform::splitPath("/files/a%2Fb/c+d/%C3%A9") ==
        Segments{"files", "a/b", "c+d", "\xc3\xa9"});
    // empty segments and a trailing '/'
    CHECK(platform::splitPath("/a//b") == Segments{"a", "", "b"});
    CHECK(platform::splitPath("/a/b/") == Segments{"a", "b", ""});
    CHECK(platform::splitPath("/").empty());
    CHECK(platform::splitPath("").empty());
    CHECK(platform::splitPath("//") == Segments{"", ""});

    CHECK(platform::joinPath({"app", "tasks", "42"}) == "/app/tasks/42");
    CHECK(platform::joinPath({"a/b", "c d", "x?y#z", "100%", "\xc3\xa9", "a+b:c@d"}) ==
        "/a%2Fb/c%20d/x%3Fy%23z/100%25/%C3%A9/a+b:c@d");
    CHECK(platform::joinPath({"a", ""}) == "/a/");
    CHECK(platform::joinPath({}) == "/");

    for(const char *path:{"/app/tasks/42", "/a%2Fb/c%20d", "/a//b", "/a/b/", "/",
        "/%C3%A9t%C3%A9"})
    {
        INFO(path);
        CHECK(platform::joinPath(platform::splitPath(path)) == path);
    }
}

TEST_CASE("platform url: pushUrl and replaceUrl take /path, ?query and #hash")
{
    ErrorLog errors;

    REQUIRE(platform::replaceUrl("/app/tasks/42?view=backlog#top"));
    CHECK(platform::currentUrl() == "/app/tasks/42?view=backlog#top");

    // ?query[#hash] keeps the path, dropping the hash it does not give
    REQUIRE(platform::replaceUrl("?view=hosts"));
    CHECK(platform::currentUrl() == "/app/tasks/42?view=hosts");
    REQUIRE(platform::replaceUrl("?view=log#line-3"));
    CHECK(platform::currentUrl() == "/app/tasks/42?view=log#line-3");

    // #hash keeps the path and the query
    REQUIRE(platform::replaceUrl("#line-9"));
    CHECK(platform::currentUrl() == "/app/tasks/42?view=log#line-9");
    REQUIRE(platform::replaceUrl("#"));
    CHECK(platform::currentUrl() == "/app/tasks/42?view=log");

    // an empty query, and /path replacing all three
    REQUIRE(platform::replaceUrl("?#h"));
    CHECK(platform::currentUrl() == "/app/tasks/42#h");
    REQUIRE(platform::replaceUrl("/app/hosts/7"));
    CHECK(platform::currentUrl() == "/app/hosts/7");
    REQUIRE(platform::replaceUrl("/"));
    CHECK(platform::currentUrl() == "/");

    // encoded bytes, a trailing '/', an empty segment, a dot inside a segment
    REQUIRE(platform::pushUrl("/files/a%2Fb/%C3%A9//v1.2/?q=a+b%27c#%20x"));
    CHECK(platform::currentUrl() == "/files/a%2Fb/%C3%A9//v1.2/?q=a+b%27c#%20x");
    CHECK(errors.take().empty());
}

TEST_CASE("platform url: pushUrl and replaceUrl refuse anything but the three forms")
{
    ErrorLog errors;

    REQUIRE(platform::replaceUrl("/start?a=1#h"));
    REQUIRE(platform::pushUrl("/next"));
    platform::historyBack();
    REQUIRE(platform::currentUrl() == "/start?a=1#h");

    int calls=0;

    platform::setUrlChangedHandler([&calls](const String &) { ++calls; });

    for(const char *url:{"", "tasks/42", "./x", "../x", "x?y", "https://example.com/x",
        "http:/x", "javascript:alert(1)", "mailto:a@b.c", "//host/x", "///x", "/a/./b",
        "/a/../b", "/..", "/.", "/a/%2e%2E/b", "/a/.%2e", "/a b", "/a\nb", "/a\tb",
        "/\xc3\xa9", "?q=\xc3\xa9", "#\x7f", "/a\"b", "/a<b>", "/a\\b", "/a^b", "/a`b",
        "/a{b}", "/a|b", "?q=it's", "/x?q='"})
    {
        for(const bool push:{true, false})
        {
            const std::string call=std::string(push?"pushUrl(\"":"replaceUrl(\"")+url+"\")";

            INFO(call);
            CHECK_FALSE((push?platform::pushUrl(url):platform::replaceUrl(url)));

            const std::vector<std::string> lines=errors.take();

            REQUIRE(lines.size() == 1);
            CHECK(lines[0].find(push?"pushUrl(":"replaceUrl(") != std::string::npos);
            CHECK(lines[0].find("refused") != std::string::npos);
            // one line: a newline in the URL is written as \x0a
            CHECK(lines[0].find('\n') == lines[0].size()-1);
            CHECK(platform::currentUrl() == "/start?a=1#h");
        }
    }

    // nothing was pushed: the forward entry is still there
    platform::historyForward();
    CHECK(platform::currentUrl() == "/next");
    CHECK(calls == 1);
    platform::setUrlChangedHandler(nullptr);

    // a ' in a path or hash is not encoded by a browser, so it is taken there
    CHECK(platform::replaceUrl("/it's#it's"));
    CHECK(platform::currentUrl() == "/it's#it's");
    CHECK(errors.take().empty());
}

TEST_CASE("platform url: the in-process history moves back and forward")
{
    ErrorLog errors;

    REQUIRE(platform::replaceUrl("/app/tasks/42?view=backlog#top"));
    REQUIRE(platform::pushUrl("/app/hosts/7"));
    CHECK(platform::currentUrl() == "/app/hosts/7");

    platform::historyBack();
    CHECK(platform::currentUrl() == "/app/tasks/42?view=backlog#top");
    platform::historyForward();
    CHECK(platform::currentUrl() == "/app/hosts/7");
    // nothing forward of the newest entry
    platform::historyForward();
    CHECK(platform::currentUrl() == "/app/hosts/7");

    // ?query keeps the path, #hash keeps the query too
    REQUIRE(platform::pushUrl("?tab=log"));
    CHECK(platform::currentUrl() == "/app/hosts/7?tab=log");
    REQUIRE(platform::pushUrl("#line-3"));
    CHECK(platform::currentUrl() == "/app/hosts/7?tab=log#line-3");
    // a replace changes the current entry alone
    REQUIRE(platform::replaceUrl("#line-4"));

    platform::historyBack();
    CHECK(platform::currentUrl() == "/app/hosts/7?tab=log");
    platform::historyBack();
    CHECK(platform::currentUrl() == "/app/hosts/7");
    platform::historyBack();
    CHECK(platform::currentUrl() == "/app/tasks/42?view=backlog#top");
    platform::historyForward();
    platform::historyForward();
    platform::historyForward();
    CHECK(platform::currentUrl() == "/app/hosts/7?tab=log#line-4");

    // a push from the middle drops the entries forward of it
    platform::historyBack();
    platform::historyBack();
    REQUIRE(platform::currentUrl() == "/app/hosts/7");
    REQUIRE(platform::pushUrl("/app/tasks/43"));
    platform::historyForward();
    CHECK(platform::currentUrl() == "/app/tasks/43");
    platform::historyBack();
    CHECK(platform::currentUrl() == "/app/hosts/7");
    platform::historyForward();
    CHECK(platform::currentUrl() == "/app/tasks/43");
    CHECK(errors.take().empty());
}

TEST_CASE("platform url: the handler is called on back and forward, never on push or replace")
{
    std::vector<std::string> calls;

    REQUIRE(platform::replaceUrl("/one"));
    platform::setUrlChangedHandler([&calls](const String &url) {
        // the history has moved when it is called
        CHECK(platform::currentUrl() == url);
        calls.push_back(url);
    });

    REQUIRE(platform::pushUrl("/two"));
    REQUIRE(platform::pushUrl("?q=1"));
    REQUIRE(platform::replaceUrl("#h"));
    CHECK(calls.empty());

    platform::historyBack();
    platform::historyBack();
    platform::historyForward();
    CHECK(calls == std::vector<std::string>{"/two", "/one", "/two"});

    // at the newest entry forward moves nothing and calls nothing
    platform::historyForward();
    REQUIRE(calls.size() == 4);
    CHECK(calls[3] == "/two?q=1#h");
    platform::historyForward();
    CHECK(calls.size() == 4);

    // a handler may push from inside the call: it is the app's own, so not
    // delivered either
    platform::setUrlChangedHandler([&calls](const String &url) {
        calls.push_back(url);
        if(url == "/two")
            platform::replaceUrl("/two?seen=1");
    });
    platform::historyBack();
    CHECK(calls.size() == 5);
    CHECK(platform::currentUrl() == "/two?seen=1");

    platform::setUrlChangedHandler(nullptr);
    platform::historyBack();
    CHECK(platform::currentUrl() == "/one");
    CHECK(calls.size() == 5);
}

TEST_CASE("platform openUrl: opens http, https and mailto and refuses every other scheme")
{
    ErrorLog errors;
    std::vector<std::string> opened;

    platform::setOpenUrlHandler([&opened](const String &url) {
        opened.push_back(url);
        return true;
    });

    const std::vector<std::string> allowed={"https://example.com/a?b=c#d",
        "http://localhost:8000/", "HTTPS://Example.com", "Http://[::1]:80/x",
        "mailto:someone@example.com", "MAILTO:a@b.c?subject=hi", "https://\xc3\xa9.example/"};

    for(const std::string &url:allowed)
    {
        INFO(url);
        CHECK(platform::openUrl(url));
    }
    CHECK(opened == allowed);
    CHECK(errors.take().empty());

    for(const char *url:{"", "example.com", "//example.com", "/local/path", "javascript:alert(1)",
        "JavaScript:alert(1)", "file:///etc/passwd", "data:text/html,x", "ftp://example.com",
        "about:blank", "blob:https://example.com/x", "https:", "https:/x", "https:///x",
        "http://?q", "http://#x", "mailto:", "http://exa mple.com", "https://x\n",
        "https://x\x7f", " https://example.com"})
    {
        INFO(url);
        CHECK_FALSE(platform::openUrl(url));

        const std::vector<std::string> lines=errors.take();

        REQUIRE(lines.size() == 1);
        CHECK(lines[0].find("openUrl(") != std::string::npos);
        CHECK(lines[0].find("refused") != std::string::npos);
    }
    CHECK(opened.size() == allowed.size());

    // the opener's answer is openUrl's
    platform::setOpenUrlHandler([](const String &) { return false; });
    CHECK_FALSE(platform::openUrl("https://example.com/"));
    CHECK(errors.take().empty());
    platform::setOpenUrlHandler(nullptr);
}

#if defined(__linux__) && !defined(__EMSCRIPTEN__)
TEST_CASE("platform openUrl: the system opener gets the URL as its one argument, not through a shell")
{
    ErrorLog errors;
    ScratchDir dir;
    const std::string out=dir.m_path+"/args.txt";
    const std::string opener=dir.m_path+"/xdg-open";

    // an xdg-open that writes its argument count and arguments, then renames
    // the file into place so it is read whole
    writeFile(opener, "#!/bin/sh\nprintf '%s\\n' \"$#\" \"$@\" > \"$GWENI_TEST_OUT.part\"\n"
        "/bin/mv \"$GWENI_TEST_OUT.part\" \"$GWENI_TEST_OUT\"\n");
    REQUIRE(chmod(opener.c_str(), 0755) == 0);

    ScopedEnv path("PATH", dir.m_path.c_str());
    ScopedEnv target("GWENI_TEST_OUT", out.c_str());
    // what a shell would run or split, were there one
    const std::string url="https://example.com/a?b=$(touch${IFS}x)&c=`id`;d|e#'f'";

    platform::setOpenUrlHandler(nullptr);
    REQUIRE(platform::openUrl(url));

    std::string args;

    for(int i=0; i < 500 && (args=readFile(out)).empty(); ++i)
        usleep(10000);
    CHECK(args == "1\n"+url+"\n");
    remove(out.c_str());

    // no opener on PATH: openUrl says it failed
    remove(opener.c_str());
    CHECK_FALSE(platform::openUrl("https://example.com/"));
    CHECK(dir.entries().empty());
    CHECK(errors.take().empty());
}
#endif

TEST_CASE("platform storage: a file written whole and read back by a fresh load")
{
    ErrorLog errors;
    ScratchDir dir;
    const std::string path=dir.m_path+"/store.json";
    const std::string text="h\xc3\xa9llo \"quoted\" \\ back\nslash\t\x01";

    platform::setStoragePrefix("");
    REQUIRE(platform::setStorageFile(path));
    CHECK(platform::storageKeys().empty());
    CHECK_FALSE(platform::storageGet("draft").has_value());
    // nothing written until a change
    CHECK(dir.entries().empty());

    REQUIRE(platform::storageSet("draft", text));
    REQUIRE(platform::storageSet("last-page", "ButtonDemo"));
    REQUIRE(platform::storageSet("\xc3\xa9", ""));
    CHECK(platform::storageGet("draft") == text);
    CHECK(platform::storageGet("\xc3\xa9") == std::string());
    CHECK(platform::storageKeys() == std::vector<std::string>{"draft", "last-page", "\xc3\xa9"});
    // the file alone, no temporary file left beside it
    CHECK(dir.entries() == std::vector<std::string>{"store.json"});

    const std::string written=readFile(path);

    CHECK(written.find("\"last-page\": \"ButtonDemo\"") != std::string::npos);
    CHECK(written.back() == '\n');

    // a fresh load: another store, then this file again
    REQUIRE(platform::setStorageFile(dir.m_path+"/other.json"));
    CHECK_FALSE(platform::storageGet("draft").has_value());
    REQUIRE(platform::setStorageFile(path));
    CHECK(platform::storageGet("draft") == text);
    CHECK(platform::storageGet("last-page") == std::string("ButtonDemo"));

    REQUIRE(platform::storageSet("last-page", "SliderDemo"));
    REQUIRE(platform::storageRemove("draft"));
    // removing what is not there is no change and no failure
    CHECK(platform::storageRemove("never-set"));
    REQUIRE(platform::setStorageFile(path));
    CHECK_FALSE(platform::storageGet("draft").has_value());
    CHECK(platform::storageGet("last-page") == std::string("SliderDemo"));
    CHECK(dir.entries() == std::vector<std::string>{"store.json"});

    // the prefix keeps two apps' keys apart in one store
    platform::setStoragePrefix("app1/");
    CHECK(platform::storageKeys().empty());
    REQUIRE(platform::storageSet("page", "one"));
    platform::setStoragePrefix("app2/");
    CHECK_FALSE(platform::storageGet("page").has_value());
    REQUIRE(platform::storageSet("page", "two"));
    CHECK(platform::storageKeys() == std::vector<std::string>{"page"});
    platform::setStoragePrefix("");
    REQUIRE(platform::setStorageFile(path));
    CHECK(platform::storageKeys() ==
        std::vector<std::string>{"app1/page", "app2/page", "last-page", "\xc3\xa9"});
    CHECK(platform::storageGet("app1/page") == std::string("one"));
    CHECK(errors.take().empty());

    REQUIRE(platform::setStorageFile(""));
}

TEST_CASE("platform storage: a write that fails is refused and leaves the store as it was")
{
    ErrorLog errors;
    ScratchDir dir;

    platform::setStoragePrefix("");
    // a file in a directory that does not exist: no values to load, none written
    REQUIRE(platform::setStorageFile(dir.m_path+"/missing/store.json"));
    CHECK_FALSE(platform::storageSet("k", "v"));
    CHECK_FALSE(platform::storageGet("k").has_value());
    CHECK(platform::storageKeys().empty());
    CHECK(platform::storageRemove("k"));
    CHECK(dir.entries().empty());
    CHECK(errors.take().empty());

    // a file that is not a storage file: refused with one line, the store empty
    // and the next change writing it whole
    const std::string path=dir.m_path+"/store.json";

    for(const char *text:{"{\"k\": \"v\", \"cut", "[\"k\"]", "not json"})
    {
        INFO(text);
        writeFile(path, text);
        CHECK_FALSE(platform::setStorageFile(path));
        CHECK(errors.take().size() == 1);
        CHECK(platform::storageKeys().empty());
    }
    REQUIRE(platform::storageSet("k", "v"));
    REQUIRE(platform::setStorageFile(path));
    CHECK(platform::storageGet("k") == std::string("v"));

    // members that are not strings are skipped
    writeFile(path, "{\"n\": 1, \"s\": \"x\", \"o\": {}}");
    REQUIRE(platform::setStorageFile(path));
    CHECK(platform::storageKeys() == std::vector<std::string>{"s"});
    CHECK(errors.take().empty());

    REQUIRE(platform::setStorageFile(""));
}

TEST_CASE("platform storage: with no file the values are kept in memory")
{
    ErrorLog errors;

    platform::setStoragePrefix("");
    REQUIRE(platform::setStorageFile(""));
    CHECK(platform::storageKeys().empty());
    REQUIRE(platform::storageSet("draft", "kept"));
    REQUIRE(platform::storageSet("other", "too"));
    CHECK(platform::storageGet("draft") == std::string("kept"));
    REQUIRE(platform::storageRemove("other"));
    CHECK(platform::storageKeys() == std::vector<std::string>{"draft"});

    // said at most once per process, on the first use, however many follow
    const std::vector<std::string> lines=errors.take();

    REQUIRE(lines.size() <= 1);
    if(!lines.empty())
        CHECK(lines[0].find("in memory") != std::string::npos);

    // a new store starts empty
    REQUIRE(platform::setStorageFile(""));
    CHECK(platform::storageKeys().empty());
    CHECK(errors.take().empty());
}

TEST_CASE("platform names: the time zone from TZ and the locale from LC_ALL and LANG")
{
    {
        ScopedEnv tz("TZ", "Europe/Berlin");
        CHECK(platform::timeZoneName() == "Europe/Berlin");
    }
    {
        ScopedEnv tz("TZ", ":America/New_York");
        CHECK(platform::timeZoneName() == "America/New_York");
    }
    {
        ScopedEnv tz("TZ", "/usr/share/zoneinfo/posix/Asia/Tokyo");
        CHECK(platform::timeZoneName() == "Asia/Tokyo");
    }
    {
        ScopedEnv tz("TZ", "");
        CHECK(platform::timeZoneName() == "UTC");
    }
    {
        // the /etc/localtime link, or UTC without one
        ScopedEnv tz("TZ", nullptr);
        CHECK_FALSE(platform::timeZoneName().empty());
    }

    {
        ScopedEnv all("LC_ALL", nullptr);
        ScopedEnv lang("LANG", "en_US.UTF-8");
        CHECK(platform::localeName() == "en-US");
    }
    {
        ScopedEnv all("LC_ALL", "de_DE@euro");
        ScopedEnv lang("LANG", "en_US.UTF-8");
        CHECK(platform::localeName() == "de-DE");
    }
    {
        ScopedEnv all("LC_ALL", "");
        ScopedEnv lang("LANG", "pt_BR");
        CHECK(platform::localeName() == "pt-BR");
    }
    for(const char *value:{"C", "POSIX", "C.UTF-8"})
    {
        ScopedEnv all("LC_ALL", value);
        CHECK(platform::localeName() == "en-US");
    }
    {
        ScopedEnv all("LC_ALL", nullptr);
        ScopedEnv lang("LANG", nullptr);
        CHECK(platform::localeName() == "en-US");
    }
}
