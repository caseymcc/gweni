/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  freetype-gl's glyph fallback chain (gweni's patch, texture_font_t::fallback):
 *  a codepoint a font's face has no glyph for is drawn with the glyph of the
 *  first chain font whose face has it, a font on the same atlas at the same size
 *  and mode, opened by the fallback callback on the first miss only. These cases
 *  pin that lookup in each mode (coverage, LCD phases, SDF), its order, that the
 *  font's own missing glyph shows only when no chain font has the codepoint, that
 *  a font whose codepoints are all its own never calls the callback, that a
 *  second lookup opens no face, and that two glyphs of different faces are never
 *  kerned: texture_glyph_get_kerning_glyph, with a previous glyph from another
 *  face for a pair the faces kern, gives 0, and the codepoint lookup the text
 *  layouts make (texture_glyph_get_kerning) never spans two faces, since every
 *  pair a font's glyphs hold, its own and those it took from its chain, names a
 *  codepoint the same font draws.
 *
 *  It drives freetype-gl alone, on CPU atlases, and needs no context; it is in
 *  gweni_unittests because that is the binary freetype-gl is linked into with the
 *  GL renderer, and it reads the faces from resources/ in the repo root.
 */
#include <doctest/doctest.h>

#include <freetype-gl/texture-atlas.h>
#include <freetype-gl/texture-font.h>

#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace
{

//! A face file read into memory, as chain faces are loaded.
std::vector<unsigned char> readFile(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);

    REQUIRE_MESSAGE(in.good(), "cannot read ", path);
    return std::vector<unsigned char>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

enum class Mode { Coverage, Lcd, Sdf };

//! freetype-gl's settings for a mode, as SlotRenderer::loadFont makes them.
void setMode(ftgl::texture_font_t *font, Mode mode)
{
    if(mode == Mode::Sdf)
    {
        font->sdf_spread=6;
        font->hinting=0;
    }
    else if(mode == Mode::Lcd)
        font->subpixel_positions=3;
}

//! A primary font with a chain of faces, each opened from memory on the atlas
//! the first time the primary asks for it, and counted.
struct Chain
{
    Chain(const std::string &primaryFile, std::vector<std::string> chainFiles, Mode mode, float size)
        : mode(mode), size(size), files(std::move(chainFiles))
    {
        atlas=ftgl::texture_atlas_new(512, 512, mode == Mode::Lcd?3:1);
        primary=ftgl::texture_font_new_from_file(atlas, size, primaryFile.c_str());
        REQUIRE(primary != nullptr);
        setMode(primary, mode);
        primary->fallback=&Chain::open;
        primary->fallback_data=this;
        blobs.resize(files.size());
        fonts.resize(files.size(), nullptr);
    }

    ~Chain()
    {
        ftgl::texture_font_delete(primary);
        for(ftgl::texture_font_t *font:fonts)
        {
            if(font != nullptr)
                ftgl::texture_font_delete(font);
        }
        ftgl::texture_atlas_delete(atlas);
    }

    static ftgl::texture_font_t *open(void *data, size_t index)
    {
        Chain &self=*static_cast<Chain *>(data);

        ++self.calls;
        if(index >= self.files.size())
            return nullptr;
        if(self.fonts[index] == nullptr)
        {
            self.blobs[index]=readFile(self.files[index]);
            self.fonts[index]=ftgl::texture_font_new_from_memory(self.atlas, self.size,
                self.blobs[index].data(), self.blobs[index].size());
            REQUIRE(self.fonts[index] != nullptr);
            setMode(self.fonts[index], self.mode);
            ++self.opened;
        }
        return self.fonts[index];
    }

    Mode mode;
    float size;
    std::vector<std::string> files;
    std::vector<std::vector<unsigned char>> blobs;
    std::vector<ftgl::texture_font_t *> fonts;
    ftgl::texture_atlas_t *atlas=nullptr;
    ftgl::texture_font_t *primary=nullptr;
    int calls=0;
    int opened=0;
};

const std::string openSans="resources/OpenSans.ttf";
const std::string liberation="resources/LiberationSans-Regular.ttf";
const std::string mono="resources/DejaVuSansMono.ttf";

// U+2192 RIGHTWARDS ARROW (Liberation and DejaVu Sans Mono, not OpenSans),
// U+2713 CHECK MARK (DejaVu Sans Mono alone), U+5355 a CJK ideograph (none of them)
const char *const arrow="\xE2\x86\x92";
const char *const check="\xE2\x9C\x93";
const char *const cjk="\xE5\x8D\x95";

size_t glyphCount(const ftgl::texture_font_t *font)
{
    return ftgl::vector_size(font->glyphs);
}

} // namespace

TEST_CASE("Glyph fallback: a codepoint the face lacks is drawn from the chain face, in every mode")
{
    for(Mode mode:{Mode::Coverage, Mode::Lcd, Mode::Sdf})
    {
        CAPTURE(static_cast<int>(mode));
        Chain chain(openSans, {mono}, mode, mode == Mode::Sdf?48.0f:16.0f);
        const size_t ownGlyphs=glyphCount(chain.primary);
        ftgl::texture_glyph_t *glyph=ftgl::texture_font_get_glyph(chain.primary, arrow);

        REQUIRE(glyph != nullptr);
        REQUIRE(chain.opened == 1);
        CHECK(glyph->font == chain.fonts[0]);
        CHECK(glyph->codepoint == 0x2192u);
        CHECK(glyph->width > 1);
        CHECK(glyph->advance_x > 0.0f);
        // the chain font's own glyph, on the primary's atlas, and the primary
        // loaded nothing of its own for it
        CHECK(glyph == ftgl::texture_font_find_glyph(chain.fonts[0], arrow));
        CHECK(chain.fonts[0]->atlas == chain.primary->atlas);
        CHECK(glyphCount(chain.primary) == ownGlyphs);
        CHECK(ftgl::vector_size(chain.primary->fallback_glyphs) == 1);

        // found again in the primary, with no face opened or callback made
        const int calls=chain.calls;

        CHECK(ftgl::texture_font_get_glyph(chain.primary, arrow) == glyph);
        CHECK(ftgl::texture_font_find_glyph(chain.primary, arrow) == glyph);
        CHECK(chain.calls == calls);

        if(mode == Mode::Lcd)
        {
            // a phase is the chain font's image at that phase
            for(int phase=1; phase < 3; ++phase)
            {
                ftgl::texture_glyph_t *variant=ftgl::texture_font_get_glyph_phase(chain.primary, arrow, phase);

                REQUIRE(variant != nullptr);
                CHECK(variant->font == chain.fonts[0]);
                CHECK(variant->phase == phase);
                CHECK(variant != glyph);
                CHECK(ftgl::texture_font_get_glyph_phase(chain.primary, arrow, phase) == variant);
            }
            CHECK(glyphCount(chain.primary) == ownGlyphs);
        }
    }
}

TEST_CASE("Glyph fallback: the first chain face with the codepoint draws it; with none, the face's own missing glyph")
{
    Chain chain(openSans, {liberation, mono}, Mode::Coverage, 16.0f);

    ftgl::texture_glyph_t *fromLiberation=ftgl::texture_font_get_glyph(chain.primary, arrow);
    REQUIRE(fromLiberation != nullptr);
    CHECK(fromLiberation->font == chain.fonts[0]);
    // the second face is not opened for a codepoint the first has
    CHECK(chain.opened == 1);

    ftgl::texture_glyph_t *fromMono=ftgl::texture_font_get_glyph(chain.primary, check);
    REQUIRE(fromMono != nullptr);
    CHECK(chain.opened == 2);
    CHECK(fromMono->font == chain.fonts[1]);
    // Liberation lacks it, and loaded nothing for it
    CHECK(ftgl::texture_font_find_glyph(chain.fonts[0], check) == nullptr);

    const size_t liberationGlyphs=glyphCount(chain.fonts[0]);
    const size_t monoGlyphs=glyphCount(chain.fonts[1]);
    const size_t ownGlyphs=glyphCount(chain.primary);
    ftgl::texture_glyph_t *missing=ftgl::texture_font_get_glyph(chain.primary, cjk);

    REQUIRE(missing != nullptr);
    CHECK(missing->font == chain.primary);
    CHECK(glyphCount(chain.primary) == ownGlyphs+1);
    CHECK(glyphCount(chain.fonts[0]) == liberationGlyphs);
    CHECK(glyphCount(chain.fonts[1]) == monoGlyphs);

    // the missing glyph is the primary's own now: looked up with no chain call
    const int calls=chain.calls;

    CHECK(ftgl::texture_font_get_glyph(chain.primary, cjk) == missing);
    CHECK(chain.calls == calls);
}

TEST_CASE("Glyph fallback: a font whose codepoints are all its own never asks its chain")
{
    for(Mode mode:{Mode::Coverage, Mode::Lcd, Mode::Sdf})
    {
        CAPTURE(static_cast<int>(mode));
        Chain chain(openSans, {mono}, mode, 16.0f);
        const std::string text="The quick brown fox jumps over the lazy dog 0123456789 !?#&";

        for(size_t i=0; i < text.size(); ++i)
        {
            const char codepoint[2]={text[i], '\0'};

            REQUIRE(ftgl::texture_font_get_glyph(chain.primary, codepoint) != nullptr);
            if(mode == Mode::Lcd)
                REQUIRE(ftgl::texture_font_get_glyph_phase(chain.primary, codepoint, 2) != nullptr);
        }
        CHECK(chain.calls == 0);
        CHECK(chain.opened == 0);
        CHECK(ftgl::vector_size(chain.primary->fallback_glyphs) == 0);
    }
}

TEST_CASE("Glyph fallback: glyphs of two faces are not kerned")
{
    // OpenSans and Liberation Sans both kern some of these pairs; take the first
    // OpenSans kerns at this size
    Chain chain(openSans, {liberation}, Mode::Coverage, 32.0f);
    ftgl::texture_font_t *other=ftgl::texture_font_new_from_file(chain.atlas, 32.0f, liberation.c_str());
    REQUIRE(other != nullptr);

    const char *const pairs[]={"AV", "VA", "To", "AT", "Yo", "LT", "Av"};
    bool found=false;

    for(const char *pair:pairs)
    {
        const char left[2]={pair[0], '\0'};
        const char right[2]={pair[1], '\0'};
        ftgl::texture_glyph_t *leftOwn=ftgl::texture_font_get_glyph(chain.primary, left);
        ftgl::texture_glyph_t *rightOwn=ftgl::texture_font_get_glyph(chain.primary, right);
        ftgl::texture_glyph_t *leftOther=ftgl::texture_font_get_glyph(other, left);
        ftgl::texture_glyph_t *rightOther=ftgl::texture_font_get_glyph(other, right);

        REQUIRE(leftOwn != nullptr);
        REQUIRE(rightOwn != nullptr);
        REQUIRE(leftOther != nullptr);
        REQUIRE(rightOther != nullptr);

        const float own=ftgl::texture_glyph_get_kerning(rightOwn, left);
        const float otherOwn=ftgl::texture_glyph_get_kerning(rightOther, left);

        if(own == 0.0f || otherOwn == 0.0f)
            continue;
        found=true;
        CAPTURE(pair);
        // a pair of one face is kerned by glyph as by codepoint
        CHECK(ftgl::texture_glyph_get_kerning_glyph(rightOwn, leftOwn) == own);
        CHECK(ftgl::texture_glyph_get_kerning_glyph(rightOther, leftOther) == otherOwn);
        // across the faces it is 0 either way round, though each face kerns it
        CHECK(ftgl::texture_glyph_get_kerning_glyph(rightOwn, leftOther) == 0.0f);
        CHECK(ftgl::texture_glyph_get_kerning_glyph(rightOther, leftOwn) == 0.0f);
        CHECK(ftgl::texture_glyph_get_kerning_glyph(rightOwn, nullptr) == 0.0f);
        break;
    }
    CHECK_MESSAGE(found, "no pair both faces kern at 32 px");

    // and a glyph the primary took from its chain is kerned against none of its own
    ftgl::texture_glyph_t *fromChain=ftgl::texture_font_get_glyph(chain.primary, arrow);
    ftgl::texture_glyph_t *latin=ftgl::texture_font_get_glyph(chain.primary, "A");

    REQUIRE(fromChain != nullptr);
    REQUIRE(fromChain->font == chain.fonts[0]);
    CHECK(ftgl::texture_glyph_get_kerning_glyph(fromChain, latin) == 0.0f);
    CHECK(ftgl::texture_glyph_get_kerning_glyph(latin, fromChain) == 0.0f);

    ftgl::texture_font_delete(other);
}

TEST_CASE("Glyph fallback: every kerning pair a font's glyphs hold is of one face, so a codepoint lookup never kerns across faces")
{
    // text_buffer_add_char, walkPens and layoutSdfText kern by codepoint
    // (texture_glyph_get_kerning): a pair can be across faces only if a glyph's
    // table names a codepoint that another font draws. A table holds the pairs
    // of its own font's glyphs, and a chain font is asked only for codepoints the
    // primary lacks, so every codepoint a table names is drawn by that table's
    // font. Liberation, the chain face here, kerns Latin as OpenSans does.
    auto utf8=[](uint32_t cp)
    {
        std::string s;

        if(cp < 0x80)
            s+=static_cast<char>(cp);
        else if(cp < 0x800)
        {
            s+=static_cast<char>(0xC0|(cp>>6));
            s+=static_cast<char>(0x80|(cp&0x3F));
        }
        else
        {
            s+=static_cast<char>(0xE0|(cp>>12));
            s+=static_cast<char>(0x80|((cp>>6)&0x3F));
            s+=static_cast<char>(0x80|(cp&0x3F));
        }
        return s;
    };

    for(Mode mode:{Mode::Coverage, Mode::Lcd, Mode::Sdf})
    {
        CAPTURE(static_cast<int>(mode));
        Chain chain(openSans, {liberation, mono}, mode, mode == Mode::Sdf?48.0f:32.0f);
        // Latin pairs both faces kern, arrows Liberation has and OpenSans lacks,
        // a check mark only DejaVu Sans Mono has, a CJK ideograph none has
        const char *const text[]={"A", "V", "T", "o", "Y", "L", "a", "v", arrow,
            "\xE2\x86\x90", "\xE2\x86\x91", "\xE2\x86\x93", "\xE2\x86\x94", check, cjk};

        for(const char *codepoint:text)
            REQUIRE(ftgl::texture_font_get_glyph(chain.primary, codepoint) != nullptr);
        REQUIRE(ftgl::vector_size(chain.primary->fallback_glyphs) > 0);

        size_t pairs=0;

        for(ftgl::vector_t *glyphs:{chain.primary->glyphs, chain.primary->fallback_glyphs})
        {
            for(size_t i=0; i < ftgl::vector_size(glyphs); ++i)
            {
                const ftgl::texture_glyph_t *glyph=*static_cast<ftgl::texture_glyph_t *const *>(ftgl::vector_get(glyphs, i));

                for(size_t k=0; k < ftgl::vector_size(glyph->kerning); ++k)
                {
                    const ftgl::kerning_t *pair=static_cast<const ftgl::kerning_t *>(ftgl::vector_get(glyph->kerning, k));
                    const ftgl::texture_glyph_t *previous=ftgl::texture_font_get_glyph(chain.primary, utf8(pair->codepoint).c_str());

                    CAPTURE(glyph->codepoint);
                    CAPTURE(pair->codepoint);
                    REQUIRE(previous != nullptr);
                    CHECK(previous->font == glyph->font);
                    ++pairs;
                }
            }
        }
        // the Latin pairs are there (the check is not vacuous)
        CHECK(pairs > 0);
        // and so by codepoint, a Latin letter after an arrow, or an arrow after
        // one, is not kerned
        const ftgl::texture_glyph_t *a=ftgl::texture_font_get_glyph(chain.primary, "A");
        const ftgl::texture_glyph_t *right=ftgl::texture_font_get_glyph(chain.primary, arrow);

        CHECK(ftgl::texture_glyph_get_kerning(a, arrow) == 0.0f);
        CHECK(ftgl::texture_glyph_get_kerning(right, "A") == 0.0f);
    }
}
