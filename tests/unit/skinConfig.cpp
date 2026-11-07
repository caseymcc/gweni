/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A skin config that cannot be used degrades the textured skin visibly
 *  instead of leaving it half initialised: TexturedSkin::init reports the file
 *  on one stderr line, returns false, and leaves every tokyonight surface in
 *  colors::Missing (magenta, with a MissingDark border) and the text colours
 *  at their compiled defaults, so labels stay readable over it. A config whose
 *  atlas texture does not load is reported and fails init too, but the rest of
 *  it (the theme included) still applies.
 */
#include "headlessFixture.h"

#include <gweni/platforms/commonPlatform.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>
#include <gweni/utils/filesystem.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

#include <unistd.h>

using namespace gweni;

namespace tokyonight=gweni::skin::textured::tokyonight;

namespace
{

//! A fresh directory under the temp dir, removed again on destruction.
struct TempDir
{
    TempDir():
        path(fs::temp_directory_path()/("gweni_skinConfig_"+std::to_string(getpid())))
    {
        fs::remove_all(path);
        fs::create_directories(path);
    }

    ~TempDir()
    {
        std::error_code ec;
        fs::remove_all(path, ec);
    }

    void write(const String &name, const String &text) const
    {
        std::ofstream file(path/name);
        file<<text;
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

//! What one init() did: its result and what it printed on stderr.
struct InitResult
{
    bool loaded;
    String message;
};

InitResult initSkin(skin::textured::TexturedSkin &skin)
{
    CerrCapture capture;
    const bool loaded=skin.init("DefaultSkin.json");

    return {loaded, capture.buffer.str()};
}

size_t lines(const String &text)
{
    return std::count(text.begin(), text.end(), '\n');
}

//! Every registered surface is magenta, its radius and border width as \p before.
void checkAllMissing(const test::SurfaceSnapshot &before)
{
    for(const auto &entry : tokyonight::surfaceRegistry())
    {
        const tokyonight::Surface &s=*entry.second;
        const tokyonight::Surface &was=before.saved.at(entry.first);

        INFO("surface ", entry.first);
        CHECK((s.top == colors::Missing));
        CHECK((s.bottom == colors::Missing));
        CHECK((s.border == colors::MissingDark));
        CHECK(s.radius == was.radius);
        CHECK(s.borderWidth == was.borderWidth);
    }
}

//! \p skin's text colours are the compiled ones a skin's setThemeColors() gives.
void checkCompiledTextColors(skin::textured::TexturedSkin &skin, test::StubRenderer &renderer)
{
    skin::textured::TexturedSkin compiled(&renderer);

    compiled.setThemeColors();
    CHECK((skin.Colors.Label.Default == compiled.Colors.Label.Default));
    CHECK((skin.Colors.Button.Normal == compiled.Colors.Button.Normal));
    CHECK_FALSE((skin.Colors.Label.Default == colors::Missing));
}

}//namespace

TEST_CASE("TexturedSkin::init with a config it cannot use paints every surface magenta and fails")
{
    TempDir dir;
    platform::CurrentPath paths(dir.path.string());
    test::StubRenderer renderer(paths);
    test::SurfaceSnapshot surfaces;
    const String configPath=(dir.path/"DefaultSkin.json").string();

    SUBCASE("no DefaultSkin.json")
    {
    }

    SUBCASE("a directory named DefaultSkin.json")
    {
        fs::create_directories(dir.path/"DefaultSkin.json");
    }

    SUBCASE("DefaultSkin.json that does not parse")
    {
        dir.write("DefaultSkin.json", "{\"theme\": {\"surfaces\": ");
    }

    SUBCASE("an empty DefaultSkin.json")
    {
        dir.write("DefaultSkin.json", "");
    }

    skin::textured::TexturedSkin skin(&renderer);
    const InitResult result=initSkin(skin);

    INFO("stderr: ", result.message);
    CHECK_FALSE(result.loaded);
    CHECK(lines(result.message) == 1);
    CHECK(result.message.find(configPath) != String::npos);

    checkAllMissing(surfaces);
    checkCompiledTextColors(skin, renderer);

    // the rest of init still ran
    CHECK(skin.getDefaultFont().facename == "OpenSans");
    CHECK(skin.getDefaultFont().size == 11);
}

TEST_CASE("TexturedSkin::init with an atlas that does not load fails but still applies the theme")
{
    TempDir dir;
    platform::CurrentPath paths(dir.path.string());
    test::StubRenderer renderer(paths);
    test::SurfaceSnapshot surfaces;

    dir.write("DefaultSkin.json",
        "{\n"
        "    \"textureFileName\": \"NoSuchAtlas.png\",\n"
        "    \"theme\": {\"surfaces\": {\"ButtonNormal\": {\"top\": [1, 2, 3]}}}\n"
        "}\n");
    renderer.textureStatus=Texture::Status::ErrorFileNotFound;

    skin::textured::TexturedSkin skin(&renderer);
    const InitResult result=initSkin(skin);

    INFO("stderr: ", result.message);
    CHECK_FALSE(result.loaded);
    CHECK(lines(result.message) == 1);
    CHECK(result.message.find("Unable to load skin texture") != String::npos);
    CHECK(result.message.find(paths.getPath(ResourcePaths::Type::Texture, "NoSuchAtlas.png")) != String::npos);

    // the theme applied, and nothing went magenta
    CHECK((tokyonight::ButtonNormal.top == Color(1, 2, 3)));
    CHECK((tokyonight::ButtonNormal.bottom == surfaces.saved.at("ButtonNormal").bottom));

    for(const auto &entry : tokyonight::surfaceRegistry())
    {
        INFO("surface ", entry.first);
        CHECK_FALSE((entry.second->top == colors::Missing));
        CHECK_FALSE((entry.second->border == colors::MissingDark));
    }
    checkCompiledTextColors(skin, renderer);
}

TEST_CASE("TexturedSkin::init with a config and atlas that load succeeds silently")
{
    TempDir dir;
    platform::CurrentPath paths(dir.path.string());
    test::StubRenderer renderer(paths);
    test::SurfaceSnapshot surfaces;

    dir.write("DefaultSkin.json", "{\"textureFileName\": \"Atlas.png\"}\n");

    skin::textured::TexturedSkin skin(&renderer);
    const InitResult result=initSkin(skin);

    INFO("stderr: ", result.message);
    CHECK(result.loaded);
    CHECK(result.message.empty());
    CHECK((tokyonight::Canvas.top == surfaces.saved.at("Canvas").top));
}
