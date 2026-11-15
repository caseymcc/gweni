/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  An LCD font's subpixel phase that finds no room in a full atlas
 *  (texture_font_get_glyph_phase, phase 1 or 2) gives NULL, so the text layout
 *  draws the codepoint's phase 0 image on the pixel below, as before the atlas-full
 *  patch; it is not marked unfit and is not drawn as the missing glyph. Only a
 *  phase 0 glyph that does not fit is unfit (drawn as the missing glyph). A
 *  64x64 depth-3 atlas of OpenSans 13 fills up with phase 0 letters, then every
 *  letter's other phases are asked for.
 *
 *  It drives freetype-gl alone, on a CPU atlas, and needs no context; it is in
 *  gweni_unittests for the same reason glyphFallbackChain.cpp is, and reads
 *  resources/OpenSans.ttf from the repo root.
 */
#include <doctest/doctest.h>

#include <freetype-gl/texture-atlas.h>
#include <freetype-gl/texture-font.h>

#include <string>

TEST_CASE("LCD atlas full: a phase with no room gives NULL, not the missing glyph, and leaves the letter fit")
{
    ftgl::texture_atlas_t *atlas=ftgl::texture_atlas_new(64, 64, 3);
    ftgl::texture_font_t *font=ftgl::texture_font_new_from_file(atlas, 13, "resources/OpenSans.ttf");

    REQUIRE(font != nullptr);
    font->subpixel_positions=3;

    // a glyph past ASCII reserves the missing glyph first
    REQUIRE(ftgl::texture_font_get_glyph(font, "\xC3\xA9") != nullptr);
    REQUIRE(font->notdef != nullptr);

    const std::string letters="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::string fitted;

    for(char c:letters)
    {
        const char s[2]={c, 0};

        if(atlas->failed_regions != 0)
            break;
        ftgl::texture_glyph_t *glyph=ftgl::texture_font_get_glyph_phase(font, s, 0);
        if(atlas->failed_regions == 0)
        {
            REQUIRE(glyph != nullptr);
            REQUIRE(glyph != font->notdef);
            fitted+=c;
        }
    }
    REQUIRE(atlas->failed_regions != 0);
    REQUIRE(fitted.size() > 2);

    const size_t unfit=ftgl::vector_size(font->unfit);
    int noRoom=0;

    for(char c:fitted)
    {
        const char s[2]={c, 0};

        for(int phase=1; phase < 3; ++phase)
        {
            ftgl::texture_glyph_t *glyph=ftgl::texture_font_get_glyph_phase(font, s, phase);

            CAPTURE(c);
            CAPTURE(phase);
            CHECK(glyph != font->notdef);
            if(glyph == nullptr)
                ++noRoom;
        }
        // the phase 0 image the layout falls back on is still the letter's own
        ftgl::texture_glyph_t *glyph0=ftgl::texture_font_get_glyph_phase(font, s, 0);
        CHECK(glyph0 != nullptr);
        CHECK(glyph0 != font->notdef);
    }
    CHECK(noRoom > 0);
    CHECK(ftgl::vector_size(font->unfit) == unfit);

    ftgl::texture_font_delete(font);
    ftgl::texture_atlas_delete(atlas);
}
