/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Native CJK fallback: Latin and out-of-range misses never ask for the lazy
 *  file; the first eligible miss reads it once, shared across primary sizes.
 *  Real CJK glyphs draw in proportional and terminal-cell runs on either
 *  backend. Capture preloading retains bytes without creating an atlas.
 *  Browser coverage follows with task #605's separate lazy package.
 */
#include <doctest/doctest.h>
#include "gpuRenderer.h"
#include <gweni/platforms/commonPlatform.h>
#include <freetype-gl/texture-font.h>
#include <memory>
#include <vector>

#ifndef __EMSCRIPTEN__
namespace
{
using namespace gweni;

struct Paths:ResourcePaths
{
    platform::CurrentPath files{"resources"};
    size_t lazyPaths=0;
    String getPath(Type type, const String &name) override
    {
        if(type == Type::LazyFont)
            ++lazyPaths;
        return files.getPath(type, name);
    }
};

struct Scene
{
    Paths paths;
    std::unique_ptr<renderer::SlotRenderer> renderer;
    size_t id=0;
    explicit Scene(bool chain=true):
        renderer(test::newRenderer(paths, Rect(Point(0, 0), test::viewSize())))
    {
        renderer->init();
        renderer->generateTextPrimitive(id);
        if(chain)
        {
            REQUIRE(renderer->setGlyphFallbackSource("DroidSansFallbackFull", "DroidSansFallbackFull",
                "U+3400-4DB5, U+4E00-9FA5"));
            renderer->setGlyphFallback("", {"DroidSansFallbackFull"});
        }
    }
    ~Scene()
    {
        renderer->releaseTextPrimitive(id);
    }
    std::vector<uint8_t> draw(const Font &font, const String &text)
    {
        test::clearView(Color(0, 0, 0, 255));
        renderer->begin();
        renderer->setDrawColor(Color(255, 255, 255, 255));
        renderer->renderText(id, font, Point(20, 30), 1, text);
        renderer->end();
        std::vector<uint8_t> pixels;
        test::readView(pixels);
        return pixels;
    }
};

Font font(const String &face, float size=13)
{
    Font value;
    value.facename=face;
    value.size=size;
    return value;
}
const String cjk="\u5355\u5143\u6D4B\u8BD5";
}

TEST_CASE("CJK fallback: Latin and out-of-range misses never read the lazy face, first CJK miss reads once")
{
    Scene chained, plain(false);
    const Font face=font("OpenSans");
    CHECK(chained.paths.lazyPaths == 0);
    CHECK(chained.draw(face, "Latin only 123") == plain.draw(face, "Latin only 123"));
    CHECK(chained.paths.lazyPaths == 0);
    CHECK(chained.draw(face, "\xF4\x8F\xBF\xBF") == plain.draw(face, "\xF4\x8F\xBF\xBF"));
    CHECK(chained.paths.lazyPaths == 0);
    CHECK(chained.renderer->glyphFallbackFilesRead() == 0);
    CHECK(chained.renderer->glyphFallbackFontsOpened() == 0);

    CHECK(chained.draw(face, cjk) != plain.draw(face, cjk));
    CHECK(chained.paths.lazyPaths == 1);
    CHECK(chained.renderer->glyphFallbackFilesRead() == 1);
    const auto *primary=chained.renderer->getTextureFont(face);
    REQUIRE(primary != nullptr);
    auto *glyph=ftgl::texture_font_get_glyph(const_cast<ftgl::texture_font_t *>(primary), cjk.c_str());
    REQUIRE(glyph != nullptr);
    CHECK(glyph->font != primary);
    CHECK(glyph->codepoint == 0x5355u);
    CHECK(glyph->width > 1);
    chained.draw(font("OpenSans", 26), cjk);
    chained.draw(font("DejaVuSansMono"), cjk);
    CHECK(chained.renderer->glyphFallbackFilesRead() == 1);
    CHECK(chained.renderer->glyphFallbackFontsOpened() == 3);
}

TEST_CASE("CJK fallback: mono CJK takes two cells and the table rules align in coverage and SDF")
{
    for(float size:{13.0f, 26.0f})
    {
        Scene chained, plain(false);
        const Font mono=font("DejaVuSansMono", size);
        CHECK(chained.draw(mono, cjk) != plain.draw(mono, cjk));
        std::vector<int> latin, wide;
        chained.renderer->measureCarets(mono, "\u2502ab\u2502cd\u2502", latin);
        chained.renderer->measureCarets(mono, "\u2502\u5355\u2502cd\u2502", wide);
        REQUIRE(latin.size() == 8);
        REQUIRE(wide.size() == 7);
        CHECK(wide[2] == latin[3]);
        CHECK(wide[5] == latin[6]);
        CHECK(wide.back() == latin.back());
        CHECK(chained.renderer->measureText(mono, cjk).x ==
            chained.renderer->measureText(mono, "00000000").x);
        const auto pixels=chained.draw(mono, cjk);
        CHECK(chained.draw(mono, cjk) == pixels);
        CHECK(chained.renderer->getFrameStats().textSlotsWritten == 0);
        CHECK(chained.renderer->getFrameStats().atlasUploadBytes == 0);
    }
}

TEST_CASE("CJK fallback: capture preload reads bytes without opening a chain font and misses reuse them")
{
    Scene scene;
    REQUIRE(scene.renderer->preloadGlyphFallbacks());
    CHECK(scene.renderer->glyphFallbackFilesRead() == 1);
    CHECK(scene.renderer->glyphFallbackFontsOpened() == 0);
    CHECK(scene.renderer->getLoadedFontCount() == 0);
    REQUIRE(scene.renderer->preloadGlyphFallbacks());
    scene.draw(font("OpenSans"), cjk);
    scene.draw(font("DejaVuSansMono", 26), cjk);
    CHECK(scene.renderer->glyphFallbackFilesRead() == 1);
    CHECK(scene.renderer->glyphFallbackFontsOpened() == 2);
}
#endif
