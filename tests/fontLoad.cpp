/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  OpenGLCore::loadFont reports a face it cannot load instead of returning
 *  Loaded with a null texture_font_t behind it, and ensureFont does not retry
 *  a failed face for every text on every frame. The tests count the error
 *  lines the renderer logs (through log::setLogCaller) to see how often a
 *  load was attempted.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/platforms/commonPlatform.h>

#include <cstdio>
#include <string>
#include <vector>
#include <unistd.h>

namespace
{

using gweni::Font;
using gweni::String;

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

Font makeFont(const String &facename)
{
    Font font;

    font.facename=facename;
    font.size=11;
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

TEST_CASE("loadFont: a missing face is ErrorFileNotFound, reported once and not retried")
{
    gweni::renderer::OpenGLCore &renderer=gweni::test::gpuRenderer();
    const Font missing=makeFont("NoSuchFace");
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

    renderer.freeFont(missing);
}

TEST_CASE("loadFont: a file freetype-gl cannot read is ErrorBadData, reported once and not retried")
{
    gweni::renderer::OpenGLCore &renderer=gweni::test::gpuRenderer();

    // an absolute facename replaces CurrentPath's resources/ directory
    const std::string path="/tmp/gweni_fontLoad_"+std::to_string(getpid())+".ttf";
    FILE *file=fopen(path.c_str(), "wb");

    REQUIRE(file != nullptr);
    fputs("this is not a font\n", file);
    fclose(file);

    const Font bad=makeFont(path);

    {
        ErrorLog log;

        CHECK(renderer.loadFont(bad) == Font::Status::ErrorBadData);
        REQUIRE(g_errors.size() == 1);
        CHECK(g_errors[0].find(path) != std::string::npos);

        CHECK(renderer.measureText(bad, "Hello").x == 0);
        CHECK(renderedGlyphs(bad, "Hello") == 0);
        CHECK(g_errors.size() == 1);
    }

    renderer.freeFont(bad);
    remove(path.c_str());
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
