/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The textured skin's font roles, from resources/DefaultSkin.json:
 *  "monoFont" is getDefaultMonoFont(), DejaVu Sans Mono 11, a fixed-width face
 *  that Font{facename="DejaVuSansMono"} selects as it does any face, and
 *  "fontFallback"'s default chain [DejaVuSansMono] is the renderer's glyph
 *  fallback chain for every other face (the mono face's own is empty). init
 *  opens no chain face and loads no mono font, so a Latin UI reads nothing
 *  more than it did. From a copy of resources/ without DejaVuSansMono.ttf,
 *  init fails (a capture's exit 2, tests/missingResources.cmake's nomono
 *  case), the mono font is the default font's face and the chain is left
 *  without the missing face.
 *
 *  Each case uses a renderer of its own, so the shared one's fonts are not
 *  loaded again.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/platforms/commonPlatform.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/utils/filesystem.h>

#include <memory>
#include <string>
#include <vector>

namespace
{

using gweni::Font;
using gweni::Point;
using gweni::String;
using gweni::renderer::SlotRenderer;
using gweni::skin::textured::TexturedSkin;

//! A renderer of its own, over the shared view, loading from dir, and a
//! textured skin on it, not init()'d yet
struct SkinOn
{
    explicit SkinOn(const String &dir):
        paths(dir),
        renderer(gweni::test::newRenderer(paths, gweni::Rect(Point(0, 0), gweni::test::viewSize())))
    {
        renderer->init();
        skin=std::make_unique<TexturedSkin>(renderer.get());
    }

    gweni::platform::CurrentPath paths;
    std::unique_ptr<SlotRenderer> renderer;
    std::unique_ptr<TexturedSkin> skin; //!< goes before its renderer
};

}//namespace

TEST_CASE("Skin fonts: monoFont is getDefaultMonoFont(), a fixed-width face, and fontFallback the renderer's chain")
{
    SkinOn on("resources");

    REQUIRE(on.skin->init("DefaultSkin.json"));

    const Font &mono=on.skin->getDefaultMonoFont();

    CHECK(mono.facename == "DejaVuSansMono");
    CHECK(mono.size == 11.0f);
    CHECK(on.renderer->glyphFallback("OpenSans") == std::vector<String>{ "DejaVuSansMono" });
    CHECK(on.renderer->glyphFallback("LiberationSans-Regular") == std::vector<String>{ "DejaVuSansMono" });
    CHECK(on.renderer->glyphFallback("DejaVuSansMono").empty());

    // init held the default font alone: no mono atlas, no chain face read
    const size_t fonts=on.renderer->getLoadedFontCount();

    CHECK(on.renderer->getTextureFont(mono) == nullptr);
    on.renderer->measureText(on.skin->getDefaultFont(), "The quick brown fox, 0123456789");
    CHECK(on.renderer->glyphFallbackFilesRead() == 0);
    CHECK(on.renderer->getLoadedFontCount() == fonts);

    // the face it names is the one drawn: every glyph one advance wide
    REQUIRE(static_cast<int>(on.renderer->loadFont(mono)) == static_cast<int>(Font::Status::Loaded));
    CHECK(on.renderer->measureText(mono, "iiiiiiii").x == on.renderer->measureText(mono, "WWWWWWWW").x);
    CHECK(on.renderer->measureText(on.skin->getDefaultFont(), "iiiiiiii").x
        < on.renderer->measureText(on.skin->getDefaultFont(), "WWWWWWWW").x);
    CHECK(on.renderer->glyphFallbackFilesRead() == 0);
    on.renderer->freeFont(mono);
}

TEST_CASE("Skin fonts: without the mono face's file, init fails, mono is the default face and the chain goes without it")
{
    const fs::path dir=fs::temp_directory_path()/"gweni-skinFonts-nomono";

    fs::remove_all(dir);
    fs::create_directories(dir);
    for(const auto &entry:fs::directory_iterator("resources"))
    {
        if(entry.path().filename() != "DejaVuSansMono.ttf")
            fs::copy(entry.path(), dir/entry.path().filename(), fs::copy_options::recursive);
    }

    {
        SkinOn on(dir.string());

        CHECK_FALSE(on.skin->init("DefaultSkin.json"));
        CHECK(on.skin->getDefaultMonoFont().facename == on.skin->getDefaultFont().facename);
        CHECK(on.skin->getDefaultMonoFont().size == 11.0f);
        CHECK(on.renderer->glyphFallback("OpenSans").empty());

        // the default font still loads, and a missing glyph is its own box
        const Font &font=on.skin->getDefaultFont();
        const String arrow="\xE2\x86\x92";

        CHECK(on.renderer->getTextureFont(font) != nullptr);
        CHECK(on.renderer->measureText(font, arrow).x > 0);
        CHECK(on.renderer->glyphFallbackFilesRead() == 0);
    }

    fs::remove_all(dir);
}
