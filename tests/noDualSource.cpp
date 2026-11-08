/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  LCD text without dual-source blending: a GLES context without
 *  GL_EXT_blend_func_extended (and WebGL 2) cannot blend each channel by its own
 *  coverage, so OpenGLCore's LCD modes fall back to grayscale.
 *  GWENI_GL_NO_DUAL_SOURCE=1 at construction makes any context look like that,
 *  so the fallback is checked in every tree, whatever the context has.
 *
 *  A renderer constructed with it and GWENI_TEXT_AA=rgb reports no dual-source
 *  blending once init()'d, logs one warning saying why (the first time an LCD
 *  mode is asked for, GWENI_TEXT_AA included, and never again from that
 *  renderer, however often setTextAA asks), and is Grayscale: 12 px text is
 *  drawn from a depth-1 coverage atlas, byte for byte what a renderer started
 *  in grayscale draws. A renderer constructed without the switch, in the same
 *  context, keeps whatever the context has.
 *
 *  GL only (the test suite "gl_only"): the switch and the extension are GL's.
 */
#include <doctest/doctest.h>

#include "glRenderer.h"

#include <gweni/platforms/commonPlatform.h>

#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

TEST_SUITE_BEGIN("gl_only");

namespace
{

using gweni::Color;
using gweni::Font;
using gweni::Point;
using gweni::Rect;
using gweni::renderer::OpenGLCore;
using gweni::renderer::SlotRenderer;
using gweni::renderer::TextAA;
using gweni::renderer::TextMode;

std::vector<std::string> g_warnings;

void recordWarning(gweni::log::Level lvl, const char *message)
{
    if(lvl == gweni::log::Level::Warning)
        g_warnings.push_back(message);
}

//! The warnings logged while it lives
struct WarningLog
{
    WarningLog()
    {
        g_warnings.clear();
        m_previous=gweni::log::setLogCaller(&recordWarning);
    }
    ~WarningLog()
    {
        gweni::log::setLogCaller(m_previous);
    }

    size_t dualSourceLines() const
    {
        size_t count=0;

        for(const std::string &line:g_warnings)
            count+=(line.find("dual-source blending") != std::string::npos)?1:0;
        return count;
    }

    gweni::log::LogCaller m_previous;
};

//! Sets an environment variable while it lives, then puts back what was there.
struct ScopedEnv
{
    ScopedEnv(const char *name, const char *value): m_name(name)
    {
        if(const char *previous=getenv(name))
        {
            m_had=true;
            m_previous=previous;
        }
        setenv(name, value, 1);
    }
    ~ScopedEnv()
    {
        if(m_had)
            setenv(m_name, m_previous.c_str(), 1);
        else
            unsetenv(m_name);
    }

    const char *m_name;
    bool m_had=false;
    std::string m_previous;
};

Font at12()
{
    Font font=gweni::test::openSans();

    font.size=12;
    return font;
}

//! A renderer of its own over the shared view, constructed while the
//! environment holds noDualSource and textAA (null: unset), and init()'d.
struct OwnRenderer
{
    OwnRenderer(bool noDualSource, const char *textAA):
        paths("resources")
    {
        ScopedEnv dualSource("GWENI_GL_NO_DUAL_SOURCE", noDualSource?"1":"0");
        std::unique_ptr<ScopedEnv> aa(textAA?new ScopedEnv("GWENI_TEXT_AA", textAA):nullptr);
        std::unique_ptr<ScopedEnv> gray(textAA?nullptr:new ScopedEnv("GWENI_TEXT_AA", "gray"));

        owned.reset(gweni::test::newRenderer(paths, Rect(Point(0, 0), gweni::test::viewSize())));
        // read at construction only: the environment goes back before init()
    }

    OpenGLCore &gl() { return dynamic_cast<OpenGLCore &>(*owned); }

    //! "Il" in white at 12 px on black, read back, its text id released after
    std::vector<uint8_t> draw()
    {
        SlotRenderer &renderer=*owned;
        size_t id=0;
        std::vector<uint8_t> pixels;

        renderer.generateTextPrimitive(id);
        gweni::test::clearView(Color(0, 0, 0, 255));
        renderer.begin();
        renderer.setDrawColor(Color(255, 255, 255, 255));
        renderer.renderText(id, at12(), Point(20, 40), 1, "Il");
        renderer.end();
        gweni::test::readView(pixels);
        renderer.releaseTextPrimitive(id);
        renderer.begin();
        renderer.end();
        return pixels;
    }

    gweni::platform::CurrentPath paths;
    std::unique_ptr<SlotRenderer> owned;
};

bool coverageRange()
{
    if(gweni::test::gpuRenderer().getTextMode(at12()) != TextMode::Sdf)
        return true;
    MESSAGE("skipped: GWENI_SDF_TEXT_MIN draws 12 px from the SDF atlas, which is grayscale in every mode");
    return false;
}

}//namespace

TEST_CASE("No dual-source blending: GWENI_TEXT_AA=rgb falls back to grayscale with one warning")
{
    if(!coverageRange())
        return;

    WarningLog log;
    OwnRenderer fallback(true, "rgb");

    // constructed in rgb; init() finds no dual-source blending and falls back
    CHECK(fallback.owned->textAA() == TextAA::LcdRGB);
    CHECK(log.dualSourceLines() == 0);
    fallback.owned->init();
    CHECK_FALSE(fallback.gl().glCapabilities().dualSourceBlend);
    CHECK_FALSE(fallback.owned->lcdTextAvailable());
    CHECK(fallback.owned->textAA() == TextAA::Grayscale);
    CHECK(log.dualSourceLines() == 1);
    INFO("warnings: ", g_warnings.empty()?std::string():g_warnings.front());
    CHECK(g_warnings.size() == 1);

    // asked again, in either LCD mode: still grayscale, and nothing more logged
    for(TextAA aa:{TextAA::LcdRGB, TextAA::LcdBGR, TextAA::Grayscale, TextAA::LcdRGB})
    {
        fallback.owned->setTextAA(aa);
        CHECK(fallback.owned->textAA() == TextAA::Grayscale);
    }
    CHECK(log.dualSourceLines() == 1);

    const std::vector<uint8_t> drawn=fallback.draw();

    CHECK(fallback.owned->getTextMode(at12()) == TextMode::Coverage);
    CHECK(fallback.owned->getFontAtlasDepth(at12()) == 1);

    // what a renderer started in grayscale, with dual-source blending or not, draws
    OwnRenderer gray(false, nullptr);

    gray.owned->init();
    REQUIRE(gray.owned->textAA() == TextAA::Grayscale);
    CHECK(gray.draw() == drawn);
    CHECK(log.dualSourceLines() == 1);
}

TEST_CASE("No dual-source blending: setTextAA after init() falls back once, per renderer")
{
    if(!coverageRange())
        return;

    WarningLog log;
    OwnRenderer first(true, nullptr);
    OwnRenderer second(true, nullptr);

    first.owned->init();
    second.owned->init();
    // nothing asked for an LCD mode yet
    CHECK(log.dualSourceLines() == 0);

    first.owned->setTextAA(TextAA::LcdBGR);
    first.owned->setTextAA(TextAA::LcdRGB);
    CHECK(first.owned->textAA() == TextAA::Grayscale);
    CHECK(log.dualSourceLines() == 1);

    // each renderer says it once
    second.owned->setTextAA(TextAA::LcdRGB);
    CHECK(second.owned->textAA() == TextAA::Grayscale);
    CHECK(log.dualSourceLines() == 2);

    // the shared renderer, constructed without the switch, keeps what the context has
    const bool contextHasIt=gweni::test::glRenderer().glCapabilities().dualSourceBlend;

    CHECK(gweni::test::glRenderer().lcdTextAvailable() == contextHasIt);
}

TEST_SUITE_END();
