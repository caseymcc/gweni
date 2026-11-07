/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  OpenGLCore::loadFont reports a face it cannot load, then draws it in the
 *  built-in fallback font (OpenSans, compiled into the library) and returns
 *  LoadedFallback. With the fallback off (setFallbackFont(false)) it fails
 *  instead of returning Loaded with a null texture_font_t behind it. Either
 *  way ensureFont does not try a face again for every text on every frame.
 *  The tests count the error lines the renderer logs (through
 *  log::setLogCaller) to see how often a load was attempted.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/platforms/commonPlatform.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <unistd.h>

namespace gweni
{
namespace renderer
{
// generated/fallbackFont.cpp (cmake/embedFile.cmake)
extern const unsigned char fallbackFontData[];
extern const size_t fallbackFontDataSize;
}//namespace renderer
}//namespace gweni

namespace
{

using gweni::Font;
using gweni::String;
using gweni::renderer::TextAA;

std::vector<std::string> g_errors;

void recordError(gweni::log::Level lvl, const char *message)
{
    if(lvl >= gweni::log::Level::Error)
        g_errors.push_back(message);
}

//! Sends the renderer's error lines to g_errors for as long as it lives.
struct ErrorLog
{
    ErrorLog()
    {
        g_errors.clear();
        m_previous=gweni::log::setLogCaller(&recordError);
    }

    ~ErrorLog()
    {
        gweni::log::setLogCaller(m_previous);
    }

    gweni::log::LogCaller m_previous;
};

//! How many of the lines in g_errors contain \p text
size_t errorsWith(const std::string &text)
{
    return std::count_if(g_errors.begin(), g_errors.end(),
        [&](const std::string &line) { return line.find(text) != std::string::npos; });
}

//! Turns the shared renderer's fallback font off for a case, and back on after.
struct NoFallback
{
    NoFallback() { gweni::test::gpuRenderer().setFallbackFont(false); }
    ~NoFallback() { gweni::test::gpuRenderer().setFallbackFont(true); }
};

//! Puts the shared renderer's text AA mode back when a case ends.
struct RestoreTextAA
{
    RestoreTextAA(): aa(gweni::test::gpuRenderer().textAA()) {}
    ~RestoreTextAA() { gweni::test::gpuRenderer().setTextAA(aa); }

    TextAA aa;
};

//! A file freetype-gl cannot read as a font, removed when it goes. Its absolute
//! path as a facename replaces CurrentPath's resources/ directory.
struct BadFontFile
{
    BadFontFile(): path("/tmp/gweni_fontLoad_"+std::to_string(getpid())+".ttf")
    {
        FILE *file=fopen(path.c_str(), "wb");

        REQUIRE(file != nullptr);
        fputs("this is not a font\n", file);
        fclose(file);
    }

    ~BadFontFile() { remove(path.c_str()); }

    std::string path;
};

bool sameSize(const gweni::Point &a, const gweni::Point &b)
{
    return a.x == b.x && a.y == b.y;
}

Font makeFont(const String &facename, float size=11)
{
    Font font;

    font.facename=facename;
    font.size=size;
    return font;
}

//! Draws `text` alone in one frame and returns how many glyph quads that wrote.
size_t renderedGlyphs(const Font &font, const String &text)
{
    gweni::renderer::OpenGLCore &renderer=gweni::test::gpuRenderer();
    size_t textId=0;

    renderer.generateTextPrimitive(textId);
    renderer.begin();
    renderer.setDrawColor(gweni::Color(255, 255, 255, 255));
    renderer.renderText(textId, font, gweni::Point(4, 20), 1, text);
    renderer.end();

    const size_t glyphs=renderer.getFrameStats().textSlotsWritten;

    renderer.releaseTextPrimitive(textId);
    renderer.begin();
    renderer.end();
    return glyphs;
}

}//namespace

TEST_CASE("loadFont: a missing face is LoadedFallback, drawn in OpenSans, reported once and not retried")
{
    gweni::renderer::OpenGLCore &renderer=gweni::test::gpuRenderer();
    const Font missing=makeFont("NoSuchFace");
    // not test::openSans() ("OpenSans.ttf"), so freeing it leaves the fixture alone
    const Font openSans=makeFont("OpenSans");
    ErrorLog log;

    CHECK(renderer.loadFont(missing) == Font::Status::LoadedFallback);
    REQUIRE(g_errors.size() == 2);
    CHECK(g_errors[0].find("Font file not found") != std::string::npos);
    CHECK(g_errors[0].find("resources/NoSuchFace.ttf") != std::string::npos);
    CHECK(g_errors[1].find("using built-in fallback for NoSuchFace") != std::string::npos);

    // the built-in font is OpenSans: the same metrics as the face loaded from file
    REQUIRE(renderer.loadFont(openSans) == Font::Status::Loaded);

    const gweni::Point size=renderer.measureText(missing, "Hello");

    CHECK(size.x > 0);
    CHECK(size.y > 0);
    CHECK(sameSize(size, renderer.measureText(openSans, "Hello")));
    CHECK(renderedGlyphs(missing, "Hello") == 5);
    CHECK(renderedGlyphs(missing, "Hello") == 5);
    // loaded once: no new attempt, no new line
    CHECK(g_errors.size() == 2);

    // a second loadFont finds it loaded, still a fallback, and logs nothing
    CHECK(renderer.loadFont(missing) == Font::Status::LoadedFallback);
    CHECK(g_errors.size() == 2);

    // the first use after the two loads' freeFonts loads it again, once
    renderer.freeFont(missing);
    renderer.freeFont(missing);
    CHECK(sameSize(renderer.measureText(missing, "Hello"), size));
    CHECK(sameSize(renderer.measureText(missing, "Hello"), size));
    CHECK(g_errors.size() == 4);
    CHECK(errorsWith("using built-in fallback") == 2);

    renderer.freeFont(missing);
    renderer.freeFont(openSans);
}

TEST_CASE("loadFont: a file freetype-gl cannot read is LoadedFallback, reported once and not retried")
{
    gweni::renderer::OpenGLCore &renderer=gweni::test::gpuRenderer();
    BadFontFile file;
    const Font bad=makeFont(file.path);

    {
        ErrorLog log;

        CHECK(renderer.loadFont(bad) == Font::Status::LoadedFallback);
        REQUIRE(g_errors.size() == 2);
        CHECK(g_errors[0].find("Font file failed to load") != std::string::npos);
        CHECK(g_errors[0].find(file.path) != std::string::npos);
        CHECK(g_errors[1].find("using built-in fallback for "+file.path) != std::string::npos);

        CHECK(renderer.measureText(bad, "Hello").x > 0);
        CHECK(renderedGlyphs(bad, "Hello") == 5);
        CHECK(renderedGlyphs(bad, "Hello") == 5);
        CHECK(g_errors.size() == 2);
    }

    renderer.freeFont(bad);
}

TEST_CASE("loadFont: with the fallback off, a missing face is ErrorFileNotFound, reported once and not retried")
{
    gweni::renderer::OpenGLCore &renderer=gweni::test::gpuRenderer();
    const Font missing=makeFont("NoSuchFace");
    NoFallback noFallback;
    ErrorLog log;

    CHECK(renderer.loadFont(missing) == Font::Status::ErrorFileNotFound);
    REQUIRE(g_errors.size() == 1);
    CHECK(g_errors[0].find("resources/NoSuchFace.ttf") != std::string::npos);

    // ensureFont remembers the failure: no new attempt, no new line
    const gweni::Point size=renderer.measureText(missing, "Hello");

    CHECK(size.x == 0);
    CHECK(size.y == 0);
    CHECK(renderer.measureText(missing, "Hello").x == 0);
    CHECK(renderedGlyphs(missing, "Hello") == 0);
    CHECK(renderedGlyphs(missing, "Hello") == 0);
    CHECK(g_errors.size() == 1);

    // an explicit loadFont tries again
    CHECK(renderer.loadFont(missing) == Font::Status::ErrorFileNotFound);
    CHECK(g_errors.size() == 2);
    CHECK(renderer.measureText(missing, "Hello").x == 0);
    CHECK(g_errors.size() == 2);

    // so does the first use after freeFont, once
    renderer.freeFont(missing);
    CHECK(renderer.measureText(missing, "Hello").x == 0);
    CHECK(renderer.measureText(missing, "Hello").x == 0);
    CHECK(g_errors.size() == 3);
    CHECK(errorsWith("fallback") == 0);

    renderer.freeFont(missing);
}

TEST_CASE("loadFont: with the fallback off, a file freetype-gl cannot read is ErrorBadData, reported once and not retried")
{
    gweni::renderer::OpenGLCore &renderer=gweni::test::gpuRenderer();
    BadFontFile file;
    const Font bad=makeFont(file.path);
    NoFallback noFallback;

    {
        ErrorLog log;

        CHECK(renderer.loadFont(bad) == Font::Status::ErrorBadData);
        REQUIRE(g_errors.size() == 1);
        CHECK(g_errors[0].find(file.path) != std::string::npos);

        CHECK(renderer.measureText(bad, "Hello").x == 0);
        CHECK(renderedGlyphs(bad, "Hello") == 0);
        CHECK(g_errors.size() == 1);
    }

    renderer.freeFont(bad);
}

TEST_CASE("loadFont: the fallback draws in every text mode, measuring as OpenSans does")
{
    gweni::renderer::OpenGLCore &renderer=gweni::test::gpuRenderer();
    RestoreTextAA restore;

    struct Mode
    {
        const char *name;
        TextAA aa;
        float size;
        gweni::renderer::GLTextMode expected;
    };
    // 11 is below GWENI_SDF_TEXT_MIN's default (18), 24 above it
    const Mode modes[]={
        {"coverage", TextAA::Grayscale, 11, gweni::renderer::GLTextMode::Coverage},
        {"lcd rgb", TextAA::LcdRGB, 11, gweni::renderer::GLTextMode::LcdRGB},
        {"lcd bgr", TextAA::LcdBGR, 11, gweni::renderer::GLTextMode::LcdBGR},
        {"sdf", TextAA::Grayscale, 24, gweni::renderer::GLTextMode::Sdf},
    };

    for(const Mode &mode:modes)
    {
        CAPTURE(mode.name);
        renderer.setTextAA(mode.aa);

        const Font missing=makeFont("NoSuchFace", mode.size);
        const Font openSans=makeFont("OpenSans", mode.size);
        ErrorLog log;

        REQUIRE(renderer.getTextMode(missing) == mode.expected);
        CHECK(renderer.loadFont(missing) == Font::Status::LoadedFallback);
        REQUIRE(renderer.loadFont(openSans) == Font::Status::Loaded);
        CHECK(renderer.getTextureFont(missing) != nullptr);

        const gweni::Point size=renderer.measureText(missing, "Hello");

        CHECK(size.x > 0);
        CHECK(sameSize(renderer.measureText(openSans, "Hello"), size));
        CHECK(renderedGlyphs(missing, "Hello") == 5);
        CHECK(errorsWith("using built-in fallback for NoSuchFace") == 1);

        renderer.freeFont(missing);
        renderer.freeFont(openSans);
    }
}

TEST_CASE("loadFont: a fallback freed and loaded again, over and over, keeps drawing")
{
    gweni::renderer::OpenGLCore &renderer=gweni::test::gpuRenderer();
    const Font missing=makeFont("NoSuchFace");
    const size_t fonts=renderer.getLoadedFontCount();
    ErrorLog log;

    for(int i=0; i < 5; ++i)
    {
        CAPTURE(i);
        CHECK(renderer.loadFont(missing) == Font::Status::LoadedFallback);
        CHECK(renderer.getLoadedFontCount() == fonts+1);
        CHECK(renderedGlyphs(missing, "Hello") == 5);
        renderer.freeFont(missing);
        CHECK(renderer.getLoadedFontCount() == fonts);
    }
    CHECK(errorsWith("using built-in fallback") == 5);
}

TEST_CASE("loadFont: the built-in fallback is resources/OpenSans.ttf byte for byte")
{
    std::ifstream file("resources/OpenSans.ttf", std::ios::binary);

    REQUIRE(file.good());

    const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    REQUIRE(bytes.size() == gweni::renderer::fallbackFontDataSize);
    CHECK(std::memcmp(bytes.data(), gweni::renderer::fallbackFontData, bytes.size()) == 0);
}

TEST_CASE("loadFont: OpenSans loads and measures")
{
    gweni::renderer::OpenGLCore &renderer=gweni::test::gpuRenderer();
    // not test::openSans() ("OpenSans.ttf"), so freeing it leaves the fixture alone
    const Font openSans=makeFont("OpenSans");
    ErrorLog log;

    CHECK(renderer.loadFont(openSans) == Font::Status::Loaded);
    CHECK(renderer.measureText(openSans, "Hello").x > 0);
    CHECK(renderedGlyphs(openSans, "Hello") == 5);
    CHECK(g_errors.empty());

    renderer.freeFont(openSans);
}
