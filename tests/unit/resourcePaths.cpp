/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The resource contract: the application hands the renderer a ResourcePaths
 *  and every resource the library loads (fonts, textures, the skin config) is
 *  resolved through it from a bare name. RelativeToExecutablePaths used to
 *  join by string concatenation ("resources" + "x.json" gave resourcesx.json)
 *  on top of a getExecutableDir() that returned "" on Linux, and the textured
 *  skin opened its config name as given, bypassing ResourcePaths entirely.
 */
#include "headlessFixture.h"

#include <gweni/platforms/commonPlatform.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/utils/filesystem.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

#include <unistd.h>

using namespace gweni;

using Type=ResourcePaths::Type;

namespace
{

//! The directory the running test binary is in, with no trailing separator.
fs::path exeDir()
{
    return fs::canonical("/proc/self/exe").parent_path();
}

//! A fresh directory under the temp dir, removed again on destruction.
struct TempDir
{
    TempDir():
        path(fs::temp_directory_path()/("gweni_resourcePaths_"+std::to_string(getpid())))
    {
        fs::remove_all(path);
        fs::create_directories(path);
    }

    ~TempDir()
    {
        std::error_code ec;
        fs::remove_all(path, ec);
    }

    fs::path path;
};

//! Captures what is written to std::cerr while it is alive.
struct CerrCapture
{
    CerrCapture(): old(std::cerr.rdbuf(buffer.rdbuf())) {}
    ~CerrCapture() { std::cerr.rdbuf(old); }

    std::ostringstream buffer;
    std::streambuf *old;
};

}//namespace

#if defined(__linux__)
TEST_CASE("getExecutableDir is the test binary's directory, with a trailing slash")
{
    const String dir=platform::getExecutableDir();

    REQUIRE_FALSE(dir.empty());
    CHECK(dir.back() == '/');
    CHECK(fs::canonical(dir) == exeDir());
}
#endif

TEST_CASE("RelativeToExecutablePaths joins exe dir, resource dir and name as paths")
{
    platform::RelativeToExecutablePaths bare("resources");
    platform::RelativeToExecutablePaths slashed("resources/");

    const String fromBare=bare.getPath(Type::Other, "DefaultSkin.json");

    CHECK(fromBare == slashed.getPath(Type::Other, "DefaultSkin.json"));
#if defined(__linux__)
    CHECK(fromBare == (exeDir()/"resources"/"DefaultSkin.json").string());
#endif
}

TEST_CASE("CurrentPath takes an absolute dir as it is and a relative one from the cwd")
{
    platform::CurrentPath absolute("/abs/dir");
    CHECK(absolute.getPath(Type::Other, "x.json") == "/abs/dir/x.json");

    platform::CurrentPath relative("rel/dir");
    CHECK(relative.getPath(Type::Other, "x.json") == (fs::current_path()/"rel/dir"/"x.json").string());
}

TEST_CASE("a font name without .ttf gets the suffix, one with it is left alone")
{
    platform::CurrentPath current("/abs/dir");
    CHECK(current.getPath(Type::Font, "OpenSans") == "/abs/dir/OpenSans.ttf");
    CHECK(current.getPath(Type::Font, "OpenSans.ttf") == "/abs/dir/OpenSans.ttf");
    // only fonts get it
    CHECK(current.getPath(Type::Other, "OpenSans") == "/abs/dir/OpenSans");

    platform::RelativeToExecutablePaths relative("resources");
    const String font=relative.getPath(Type::Font, "OpenSans");
    CHECK(font == relative.getPath(Type::Font, "OpenSans.ttf"));
    CHECK(fs::path(font).filename() == "OpenSans.ttf");
}

TEST_CASE("TexturedSkin::init resolves the config name through the renderer's ResourcePaths")
{
    TempDir dir;
    {
        std::ofstream config(dir.path/"DefaultSkin.json");
        config<<"{}\n";
    }

    platform::CurrentPath paths(dir.path.string());
    test::StubRenderer renderer(paths);

    SUBCASE("a bare name in the resource dir loads")
    {
        skin::textured::TexturedSkin skin(&renderer);
        CHECK(skin.init("DefaultSkin.json"));
    }

    SUBCASE("a missing config fails with one stderr line naming the resolved path")
    {
        // a config that does not load paints every surface magenta
        test::SurfaceSnapshot surfaces;
        skin::textured::TexturedSkin skin(&renderer);
        bool loaded;
        String message;
        {
            CerrCapture capture;
            loaded=skin.init("Missing.json");
            message=capture.buffer.str();
        }

        CHECK_FALSE(loaded);
        INFO("stderr: ", message);
        CHECK(message.find((dir.path/"Missing.json").string()) != String::npos);
        CHECK(std::count(message.begin(), message.end(), '\n') == 1);
    }
}
