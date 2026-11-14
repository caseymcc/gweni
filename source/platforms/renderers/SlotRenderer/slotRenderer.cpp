/*
 *  Gweni - the API-independent part of a retained renderer.
 *  Copyright (c) 2013-18 Nick Trout
 *  See license in Gweni.h
 */
#ifdef GWENI_DEBUG_LOGGING
#define DEBUG_RENDER
#endif

#include <gweni/platforms/renderers/slotRenderer.h>
#include <gweni/platforms/platformTypes.h>
#include <gweni/platforms/commonPlatform.h>
#include <gweni/platforms/platform.h>
#include <gweni/utility.h>
#include <gweni/primitiveIds.h>
#include <gweni/utils/filesystem.h>

#include <cassert>
#include <iterator>

#include <math.h>
#include <stdio.h>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cmath>
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iterator>
#include <limits>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <freetype-gl/freetype-gl.h>
#include <freetype-gl/text-buffer.h>
#include <freetype-gl/utf8-utils.h>

#include <gweni/text/cells.h>

#include <iostream>

#include <imglib/imageIo.h>
#include <imglib/simpleImage.h>

namespace gweni
{
namespace renderer
{

static constexpr size_t minPrimitiveSize=512;
static constexpr size_t minVertexSize=minPrimitiveSize*4;
static constexpr size_t minClippingSize=minPrimitiveSize;

// The SDF text atlas (TextMode::Sdf): one per (face, bold), shared by every size
// at or above the threshold. Its glyphs are rasterised once, at sdfBaseSize px with
// a distance field reaching kSdfSpread px past each edge, into an R8 atlas.
static constexpr float sdfBaseSize=48.0f;
static constexpr size_t sdfAtlasSize=1024;
// GWENI_SDF_TEXT_MIN's default, in effective px
static constexpr float defaultSdfTextMin=18.0f;
// An LCD font's glyph positions per device pixel: one per subpixel
static constexpr int lcdSubpixelPositions=3;
// GWENI_TEXT_CONTRAST's default: the LCD coverage boost for white text
static constexpr float defaultTextContrast=1.0f;

// The built-in fallback font's file, resources/OpenSans.ttf, compiled in by
// cmake/embedFile.cmake (generated/fallbackFont.cpp in the build tree)
extern const unsigned char fallbackFontData[];
extern const size_t fallbackFontDataSize;

// The printable ASCII every font rasterises when it is loaded
static const char *const fontCache=" !\"#$%&'()*+,-./0123456789:;<=>?"
    "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
    "`abcdefghijklmnopqrstuvwxyz{|}~";

GlyphChain::~GlyphChain()
{
    // textureFont's fallback_glyphs point into these fonts: they go with it
    for(Link &link:links)
    {
        if(link.font != nullptr)
            ftgl::texture_font_delete(link.font);
    }
}

FontData::~FontData()
{
    // the atlas's texture went first (SlotRenderer::eraseFont): the atlas lives in
    // memory the manager owns
    assert(!atlas || !atlas->id);
    // its fonts' glyphs are on the manager's atlas, and only textureFont points at them
    glyphChain.reset();
    // frees the texture_font_t, the atlas and the cache string loadFont strdup'd
    if(fontManager)
        ftgl::font_manager_delete(fontManager);
    free(familyPath);
}

ftgl::texture_glyph_t *FontData::glyph(const char *codepoint)
{
    const uint32_t key=ftgl::utf8_to_utf32(codepoint);

    if(key < asciiGlyphs.size())
    {
        ftgl::texture_glyph_t *&known=asciiGlyphs[key];

        if(known == nullptr)
            known=ftgl::texture_font_get_glyph(textureFont, codepoint);
        return known;
    }

    // a default-ignorable draws nothing, whatever the face has for it (no ASCII
    // codepoint is one, so the walks over ASCII text do not ask)
    if(ftgl::utf8_is_ignorable(codepoint))
        return nullptr;

    const auto found=otherGlyphs.find(key);

    if(found != otherGlyphs.end())
        return found->second;

    ftgl::texture_glyph_t *loaded=ftgl::texture_font_get_glyph(textureFont, codepoint);

    if(loaded != nullptr)
        otherGlyphs.emplace(key, loaded);
    return loaded;
}

// texture_font_t::cells: a terminal's cells by codepoint (gweni::text::cellCount)
extern "C" {
static int fontCells(uint32_t codepoint)
{
    return gweni::text::cellCount(codepoint);
}
}

float SlotRenderer::textPx(const Font &font) const
{
    return font.size*scale();
}

float SlotRenderer::coverageSize(float px)
{
    return std::max(1.0f, std::floor(px+0.5f));
}

FontKey SlotRenderer::resolveFont(const Font &font, float px) const
{
    FontKey key{font, TextMode::Coverage};

    if(px >= m_sdfTextMin)
    {
        // LCD is for the coverage range only: an SDF atlas is grayscale in every mode
        key.mode=TextMode::Sdf;
        key.font.size=0.f;
    }
    else
    {
        if(m_textAA == TextAA::LcdRGB)
            key.mode=TextMode::LcdRGB;
        else if(m_textAA == TextAA::LcdBGR)
            key.mode=TextMode::LcdBGR;
        // A grayscale atlas is rasterised at a whole device px, so a continuous zoom
        // (or a fractional canvas scale) uses one of the few whole sizes below the
        // SDF range; measuredPenShifts puts its glyphs at the pens of the size
        // asked for, so a run still ends where the layout sized it. An LCD atlas
        // keeps the exact px: its glyph images are rasterised at the phases of the
        // unhinted pens at that size, which a whole-px shift would not keep.
        key.font.size=(key.mode == TextMode::Coverage)?coverageSize(px):px;
    }
    return key;
}

void SlotRenderer::applyLcdTextSupport()
{
    if((m_textAA != TextAA::Grayscale) && !lcdTextAvailable())
        setTextAA(m_textAA);
}

TextMode SlotRenderer::getTextMode(const Font &font) const
{
    return resolveFont(font, textPx(font)).mode;
}

void SlotRenderer::setTextAA(TextAA aa)
{
    // an LCD atlas needs dual-source blending to be drawn as LCD text; without it
    // the coverage fonts stay grayscale
    if((aa == TextAA::LcdRGB) || (aa == TextAA::LcdBGR))
    {
        if(const char *why=lcdTextUnavailable())
        {
            if(!m_lcdFallbackLogged)
                gweni::log::write(log::Level::Warning, "%s, using grayscale", why);
            m_lcdFallbackLogged=true;
            aa=TextAA::Grayscale;
        }
    }

    if(aa == m_textAA)
        return;

    m_textAA=aa;

    // Every font of the coverage range is now resolved under another mode and is
    // loaded again in it; an SDF atlas is the same in every mode and stays as it
    // is, its runs drawing. A failure in another mode is tried again in this one.
    for(auto it=m_failedFonts.begin(); it != m_failedFonts.end();)
    {
        if(it->mode != TextMode::Sdf)
            it=m_failedFonts.erase(it);
        else
            ++it;
    }

    reloadFonts([](const FontData &fontData) { return fontData.mode != TextMode::Sdf; });
}

void SlotRenderer::reloadFonts(const std::function<bool(const FontData &)> &which)
{
    std::vector<std::pair<Font, int>> reload;

    for(auto it=m_fonts.begin(); it != m_fonts.end();)
    {
        if(!which(it->second))
        {
            ++it;
            continue;
        }

        reload.emplace_back(it->first.font, it->second.owners);
        for(size_t textId:it->second.textRuns)
            m_textRuns[textId].fontData=nullptr;
        it=eraseFont(it); // the atlas texture, then FontData's destructor deletes the manager
    }

    m_lastFont=nullptr;

    // the key's size is the px the atlas is rasterised at, so it resolves to
    // itself; an Sdf key's is 0, "every size", so it is loaded at the threshold
    for(const auto &[font, owners]:reload)
    {
        const Font::Status status=loadFont(font, (font.size > 0.0f)?font.size:m_sdfTextMin);

        // the load counted one owner, which the old entry's count replaces
        if((status == Font::Status::Loaded) || (status == Font::Status::LoadedFallback))
            m_lastFont->second.owners+=owners-1;
    }

    textLayoutChanged();
}

void SlotRenderer::setGlyphFallback(const String &face, const std::vector<String> &chain)
{
    // each loaded face's chain before, to load again only the fonts it changes
    std::map<String, std::vector<String>> before;

    for(const auto &[key, fontData]:m_fonts)
    {
        if(before.count(fontData.faceName) == 0)
            before.emplace(fontData.faceName, glyphFallback(fontData.faceName));
    }

    m_glyphFallback[face]=chain;

    std::unordered_set<String> changed;

    for(const auto &[name, faces]:before)
    {
        if(faces != glyphFallback(name))
            changed.insert(name);
    }
    if(!changed.empty())
    {
        reloadFonts([&changed](const FontData &fontData)
            {
                return changed.count(fontData.faceName) > 0;
            });
    }
}

std::vector<String> SlotRenderer::glyphFallback(const String &face) const
{
    auto own=m_glyphFallback.find(face);

    if(own == m_glyphFallback.end())
        own=m_glyphFallback.find(String());

    std::vector<String> chain;

    if(own == m_glyphFallback.end())
        return chain;
    for(const String &link:own->second)
    {
        if(!link.empty() && (link != face) && (std::find(chain.begin(), chain.end(), link) == chain.end()))
            chain.push_back(link);
    }
    return chain;
}

void SlotRenderer::attachGlyphChain(FontData &fontData)
{
    const std::vector<String> faces=glyphFallback(fontData.faceName);

    if(faces.empty())
        return;

    fontData.glyphChain=std::make_unique<GlyphChain>();
    fontData.glyphChain->renderer=this;
    fontData.glyphChain->primary=fontData.markup.font;
    for(const String &face:faces)
        fontData.glyphChain->links.push_back(GlyphChain::Link{ face, nullptr, nullptr, false });
    fontData.markup.font->fallback=&SlotRenderer::glyphChainFont;
    fontData.markup.font->fallback_data=fontData.glyphChain.get();
}

ftgl::texture_font_t *SlotRenderer::glyphChainFont(void *data, size_t index)
{
    GlyphChain &chain=*static_cast<GlyphChain *>(data);
    const ftgl::texture_font_t *primary=chain.primary;
    size_t n=0;

    for(GlyphChain::Link &link:chain.links)
    {
        if(link.failed)
            continue;

        if(link.font == nullptr)
        {
            link.blob=chain.renderer->glyphFallbackBlob(link.face);
            if(link.blob)
            {
                // on the primary's atlas, at its size, with no cache: only the
                // codepoints the primary lacks are ever loaded into it
                link.font=ftgl::texture_font_new_from_memory(primary->atlas, primary->size,
                    link.blob->data(), link.blob->size());
            }
            if(link.font == nullptr)
            {
                if(link.blob)
                    gweni::log::write(log::Level::Error, "Glyph fallback font failed to load: %s", link.face.c_str());
                link.blob.reset();
                link.failed=true;
                continue;
            }

            // the primary's rendering, as loadFont set it: hinting, an SDF spread or
            // LCD phases
            link.font->hinting=primary->hinting;
            link.font->kerning=primary->kerning;
            link.font->filtering=primary->filtering;
            std::copy(std::begin(primary->lcd_weights), std::end(primary->lcd_weights),
                std::begin(link.font->lcd_weights));
            link.font->rendermode=primary->rendermode;
            link.font->outline_thickness=primary->outline_thickness;
            link.font->padding=primary->padding;
            link.font->sdf_spread=primary->sdf_spread;
            link.font->subpixel_positions=primary->subpixel_positions;
            ++chain.renderer->m_glyphFallbackFontsOpened;
        }

        if(n == index)
            return link.font;
        ++n;
    }
    return nullptr;
}

std::shared_ptr<const std::vector<unsigned char>> SlotRenderer::glyphFallbackBlob(const String &face)
{
    const String path=getResourcePaths().getPath(ResourcePaths::Type::Font, face);
    std::weak_ptr<const std::vector<unsigned char>> &shared=m_glyphFallbackBlobs[path];

    if(std::shared_ptr<const std::vector<unsigned char>> blob=shared.lock())
        return blob;
    if(m_failedGlyphFallbackFiles.count(path) > 0)
        return nullptr;

    std::ifstream in(path, std::ios::binary);
    auto blob=std::make_shared<std::vector<unsigned char>>();

    if(in)
        blob->assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    if(!in.good() && !in.eof())
        blob->clear();
    if(blob->empty())
    {
        gweni::log::write(log::Level::Error, "Glyph fallback font file not found: %s", path.c_str());
        m_failedGlyphFallbackFiles.insert(path);
        return nullptr;
    }

    ++m_glyphFallbackFilesRead;
    shared=blob;
    return blob;
}

Font::Status SlotRenderer::loadFont(const Font &requested)
{
    return loadFont(requested, textPx(requested));
}

Font::Status SlotRenderer::loadFont(const Font &requested, float px)
{
    FontData fontData;
    const FontKey key=resolveFont(requested, px);
    const Font &font=key.font;
    const TextMode mode=key.mode;
    const bool sdf=(mode == TextMode::Sdf);
    const bool lcd=(mode == TextMode::LcdRGB) || (mode == TextMode::LcdBGR);

    // an explicit load is a fresh attempt; a failure is remembered again below,
    // so ensureFont does not retry it
    m_failedFonts.erase(key);

    // already loaded: a second manager and atlas would only leak
    auto loaded=m_fonts.find(key);

    if(loaded != m_fonts.end())
    {
        ++loaded->second.owners;
        loaded->second.freed=false;
        loaded->second.idleSince=0;
        m_lastFont=&(*loaded);
        return loaded->second.fallback?Font::Status::LoadedFallback:Font::Status::Loaded;
    }

    m_lastFont=nullptr;

    const String filename=getResourcePaths().getPath(ResourcePaths::Type::Font, font.facename);
    const bool found=fs::exists(filename);
    // what the face's own file gave; the built-in font may still draw it
    Font::Status status=Font::Status::Loaded;

    if(!found)
    {
        gweni::log::write(log::Level::Error, "Font file not found: %s", filename.c_str());
        status=Font::Status::ErrorFileNotFound;

        if(!m_fallbackFont)
        {
            m_failedFonts.insert(key);
            return status;
        }
    }

    vec4 black={{0.0, 0.0, 0.0, 1.0}};
    vec4 none={{1.0, 1.0, 1.0, 0.0}};

    fontData.faceName=font.facename;
    fontData.mode=mode;
    fontData.baseSize=sdf?sdfBaseSize:font.size;
    // markup_t keeps the family as a borrowed char*, and the markup outlives this
    // function, so it cannot point at the local filename. A fallback keeps the
    // requested path too: only font_manager_get_from_markup below opens it.
    fontData.familyPath=strdup(filename.c_str());
    fontData.markup.family=fontData.familyPath;
    fontData.markup.size=fontData.baseSize;
    fontData.markup.bold=0;
    fontData.markup.italic=0;
    fontData.markup.spacing=0.0;
    fontData.markup.gamma=1.0;
    fontData.markup.foreground_color=black;
    fontData.markup.background_color=none;
    fontData.markup.underline=0;
    fontData.markup.underline_color=black;
    fontData.markup.overline=0;
    fontData.markup.overline_color=black;
    fontData.markup.strikethrough=0;
    fontData.markup.strikethrough_color=black;
    fontData.markup.font=0;

    // Depth 1 for a coverage atlas, one coverage byte per texel rasterised
    // grayscale with light (vertical-only) hinting, and for an SDF one, one
    // distance byte. Depth 3 for an LCD one: freetype-gl rasterises it with
    // light hinting, FT_RENDER_MODE_LCD and the LCD filter, one coverage byte
    // per subpixel, at up to lcdSubpixelPositions phases per glyph.
    fontData.fontManager=sdf?ftgl::font_manager_new(sdfAtlasSize, sdfAtlasSize, LCD_FILTERING_OFF)
        :ftgl::font_manager_new(1024, 1024, lcd?LCD_FILTERING_ON:LCD_FILTERING_OFF);

    free(fontData.fontManager->cache); // font_manager_new strdup'd a default
    // The manager rasterises its cache as soon as it opens the face, before an SDF
    // font's spread, an LCD font's subpixel positions or the glyph chain could be
    // set, so every font loads it itself below, as the manager would have.
    fontData.fontManager->cache=strdup("");

    if(found)
    {
        fontData.markup.font=ftgl::font_manager_get_from_markup(fontData.fontManager, &fontData.markup);

        if(fontData.markup.font == nullptr)
        {
            gweni::log::write(log::Level::Error, "Font file failed to load: %s", filename.c_str());
            status=Font::Status::ErrorBadData;
        }
    }

    if((fontData.markup.font == nullptr) && m_fallbackFont)
    {
        // The built-in font, from memory into this font's own atlas, so it is drawn
        // like any face of this mode. The manager frees it with its atlas
        // (font_manager_delete); it is never asked for a face again, whose lookup
        // would compare this font's memory location as a filename. The manager
        // rasterises its cache only for faces it opens itself, so every mode loads
        // it below.
        fontData.markup.font=ftgl::texture_font_new_from_memory(fontData.fontManager->atlas,
            fontData.baseSize, fallbackFontData, fallbackFontDataSize);

        if(fontData.markup.font != nullptr)
        {
            ftgl::vector_push_back(fontData.fontManager->fonts, &fontData.markup.font);
            fontData.fallback=true;
            gweni::log::write(log::Level::Error, "using built-in fallback for %s", font.facename.c_str());
        }
        else
            gweni::log::write(log::Level::Error, "The built-in fallback font failed to load for %s", font.facename.c_str());
    }

    if(sdf && fontData.markup.font)
    {
        fontData.markup.font->sdf_spread=kSdfSpread;
        // unhinted, so the kerning is not rounded to whole base-size pixels
        fontData.markup.font->hinting=0;
    }
    else if(lcd && fontData.markup.font)
    {
        // Pens at 1/3 device px with the unhinted advances and kerning, each glyph
        // drawn from the image rasterised at its pen's phase. The cache is phase 0;
        // the other phases are rasterised as a run first needs them.
        fontData.markup.font->subpixel_positions=lcdSubpixelPositions;
    }

    if(fontData.markup.font)
    {
        // the chain first, so a codepoint of the cache the face lacks is the
        // chain's too
        attachGlyphChain(fontData);
        ftgl::texture_font_load_glyphs(fontData.markup.font, fontCache);

        // A fixed-width face lays every codepoint in whole cells, a terminal's: one
        // cell is the advance of '0' as this font loads it (whole px when hinted,
        // the base size's unhinted one in an SDF font, scaled with the run)
        if(fontData.markup.font->fixed_width)
        {
            if(ftgl::texture_glyph_t *zero=ftgl::texture_font_get_glyph(fontData.markup.font, "0"))
            {
                fontData.markup.font->cell_width=zero->advance_x;
                fontData.markup.font->cells=&fontCells;
            }
        }
    }

    if(fontData.markup.font == nullptr)
    {
        // Neither the face nor the fallback could be opened. fontData's destructor
        // frees the manager and familyPath; no atlas texture exists yet.
        m_failedFonts.insert(key);
        return status;
    }

    fontData.textureFont=fontData.markup.font;

    fontData.atlas=fontData.fontManager->atlas;

    // one byte per texel, coverage or distance alike, or three of subpixel coverage
    assert(fontData.atlas->depth == (lcd?3u:1u));
    fontData.atlas->id=createAtlasTexture(*fontData.atlas);
    //the next flush() uploads the whole atlas, cache included, as one dirty band, so
    //a load shows in FrameStats::atlasUploadCalls; after that only later glyphs
    fontData.atlas->dirty_y0=0;
    fontData.atlas->dirty_y1=fontData.atlas->height;

    const bool fallback=fontData.fallback;

    fontData.owners=1;
    m_lastFont=&(*m_fonts.insert(std::make_pair(key, std::move(fontData))).first);
    return fallback?Font::Status::LoadedFallback:Font::Status::Loaded;
}

void SlotRenderer::freeFont(const gweni::Font &requested)
{
    const FontKey drawn=resolveFont(requested, textPx(requested));
    const FontKey measured=resolveFont(requested, requested.size);

    // The owner a loadFont took: on the drawn key, or on the measured one if the
    // font was loaded at another scale() (scale 1, the measured size).
    for(const FontKey &key:{ drawn, measured })
    {
        auto it=m_fonts.find(key);

        if((it != m_fonts.end()) && (it->second.owners > 0))
        {
            --it->second.owners;
            break;
        }
    }

    freeFont(drawn, false);
    // measureText's font, at another scale(): only measured by this font, unless
    // another size is drawn in it
    if(!(measured == drawn))
        freeFont(measured, true);
}

void SlotRenderer::freeFont(const FontKey &font, bool onlyIfUnused)
{
    if(m_lastFont != nullptr && m_lastFont->first == font)
        m_lastFont=nullptr;

    m_failedFonts.erase(font);

    auto it=m_fonts.find(font);

    if(it == m_fonts.end())
        return;

    // another caller loaded it too and has not freed it
    if(it->second.owners > 0)
        return;

    if(!it->second.textRuns.empty())
    {
        // The runs laid out in it hold it: another caller's Text in an equal Font,
        // or, in an SDF atlas shared by every size of its face, another size's. It
        // stays, every run drawing, until its last run leaves it (setTextRunFont),
        // unless it is used or loaded again before that.
        if(!onlyIfUnused)
            it->second.freed=true;
        return;
    }

    eraseFont(it);
}

SlotRenderer::FontMap::iterator SlotRenderer::eraseFont(FontMap::iterator it)
{
    texture_atlas_t *atlas=it->second.atlas;

    // the atlas lives in memory the manager owns, so its texture goes first
    if(atlas && atlas->id)
    {
        destroyAtlasTexture(atlas->id);
        atlas->id=0;
    }
    // FontData's destructor deletes the manager
    return m_fonts.erase(it);
}

bool SlotRenderer::ensureFont(const Font &requested)
{
    return ensureFont(requested, textPx(requested));
}

bool SlotRenderer::ensureFont(const Font &requested, float px)
{
    const FontKey font=resolveFont(requested, px);

    if(m_lastFont != nullptr)
    {
        if(m_lastFont->first == font)
            return true;
    }

    // Was it loaded before?
    auto it=m_fonts.find(font);
    if(it != m_fonts.end())
    {
        it->second.freed=false;
        m_lastFont=&(*it);
        return true;
    }

    // Did it fail before? Then it is not tried (and reported) again for every
    // text on every frame; freeFont or an explicit loadFont clears this.
    if(m_failedFonts.count(font) != 0)
        return false;

    // No, try load to it

    // LoadFont sets m_lastFont, if loaded, and adds the font to m_failedFonts if not
    const Font::Status status=loadFont(requested, px);

    if((status != Font::Status::Loaded) && (status != Font::Status::LoadedFallback))
        return false;

    // a use is no owner: the load made a new entry and counted its caller
    m_lastFont->second.owners=0;
    return true;
}

ImageMipLevel imageMipLevel(const uint8_t *above, int aboveWidth, int aboveHeight, int depth)
{
    ImageMipLevel level;

    level.width=std::max(aboveWidth/2, 1);
    level.height=std::max(aboveHeight/2, 1);
    level.texels.resize((size_t)level.width*level.height*depth);

    const size_t rowBytes=(size_t)aboveWidth*depth;
    uint8_t *out=level.texels.data();

    for(int y=0; y<level.height; ++y)
    {
        const uint8_t *row0=above+(size_t)std::min(2*y, aboveHeight-1)*rowBytes;
        const uint8_t *row1=above+(size_t)std::min(2*y+1, aboveHeight-1)*rowBytes;

        for(int x=0; x<level.width; ++x)
        {
            const size_t x0=(size_t)std::min(2*x, aboveWidth-1)*depth;
            const size_t x1=(size_t)std::min(2*x+1, aboveWidth-1)*depth;

            for(int c=0; c<depth; ++c)
                *out++=(uint8_t)((row0[x0+c]+row0[x1+c]+row1[x0+c]+row1[x1+c]+2)/4);
        }
    }
    return level;
}

std::vector<ImageMipLevel> imageMipChain(const uint8_t *texels, int width, int height, int depth)
{
    std::vector<ImageMipLevel> levels;
    const uint8_t *above=texels;
    int aboveWidth=width;
    int aboveHeight=height;

    while(aboveWidth > 1 || aboveHeight > 1)
    {
        levels.push_back(imageMipLevel(above, aboveWidth, aboveHeight, depth));
        above=levels.back().texels.data();
        aboveWidth=levels.back().width;
        aboveHeight=levels.back().height;
    }
    return levels;
}

namespace
{

//! The pixels a load decodes at most, before any reduction (kMaxImagePixels);
//! the product in 64 bits, as size_t is 32 on wasm32
bool overPixelLimit(size_t width, size_t height)
{
    return (width > SlotRenderer::kMaxImagePixels) || (height > SlotRenderer::kMaxImagePixels) ||
        ((uint64_t)width*height > SlotRenderer::kMaxImagePixels);
}

//! The longest side a load keeps: maxSize, at most the largest image page's side
int keptSide(int maxSize)
{
    return (maxSize > 0)?std::min(maxSize, SlotRenderer::kMaxImageSide):SlotRenderer::kMaxImageSide;
}

}//namespace

Texture::Status SlotRenderer::beginTextureLoad(Texture &texture)
{
    freeTexture(texture);
    // only a successful load hands out a slot; a failed one leaves the texture
    // unloaded, so it is drawn as missing and freeing it does nothing
    texture.dataIndex=std::numeric_limits<size_t>::max();
    m_lastTexture=std::numeric_limits<size_t>::max();
    return Texture::Status::Unloaded;
}

Texture::Status SlotRenderer::failTextureLoad(SlotTextureData *texData, const char *what, const char *source)
{
    if(texData)
    {
        texData->m_readData.clear();
        texData->m_readData.shrink_to_fit();
        m_freeTextureData.push_back(texData->m_index);
    }
    gweni::log::write(log::Level::Error, "Texture %s: %s", what, source);
    return Texture::Status::ErrorBadData;
}

Texture::Status SlotRenderer::loadTexture(Texture &texture)
{
    beginTextureLoad(texture);

    const String filename=getResourcePaths().getPath(ResourcePaths::Type::Texture, texture.name);
    std::ifstream file(filename, std::ios::binary);

    if(!file)
    {
        gweni::log::write(log::Level::Error, "Texture file not found: %s", filename.c_str());
        return Texture::Status::ErrorFileNotFound;
    }

    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // a directory opens and reads nothing
    if(bytes.empty())
    {
        gweni::log::write(log::Level::Error, "Texture file not found: %s", filename.c_str());
        return Texture::Status::ErrorFileNotFound;
    }
    return loadEncodedTexture(texture, bytes.data(), bytes.size(), 0, filename.c_str());
}

Texture::Status SlotRenderer::loadTexture(Texture &texture, const uint8_t *bytes, size_t size, int maxSize)
{
    beginTextureLoad(texture);
    return loadEncodedTexture(texture, bytes, size, maxSize, "image bytes");
}

Texture::Status SlotRenderer::loadEncodedTexture(Texture &texture, const uint8_t *bytes, size_t size, int maxSize,
    const char *source)
{
    imglib::ImageHeader header;

    // the size from the header, before any pixel buffer is allocated
    if(!imglib::probeImage(bytes, size, header))
    {
        return failTextureLoad(nullptr, (header.format == imglib::FileFormat::Unknown)?
            "is no PNG or JPEG":"has a bad header", source);
    }
    if(overPixelLimit(header.width, header.height))
        return failTextureLoad(nullptr, "is over the 64 megapixel decode limit", source);

    const int side=keptSide(maxSize);
    const size_t longer=std::max(header.width, header.height);

    if((maxSize <= 0) && (longer > (size_t)kMaxImageSide))
        return failTextureLoad(nullptr, "is larger than the largest image page and has no max size", source);

    imglib::BufferLoadOptions options;

    // a JPEG decodes straight at 1/2, 1/4 or 1/8 while that still holds the
    // side kept (libjpeg rounds each side up), so its full size is never
    // allocated; a PNG decodes whole and is reduced below
    options.maxPixels=kMaxImagePixels;
    if(header.format == imglib::FileFormat::Jpeg)
    {
        while((options.scaleDenom < 8) && ((longer+options.scaleDenom*2-1)/(options.scaleDenom*2) >= (size_t)side))
            options.scaleDenom*=2;
    }

    SlotTextureData &texData=*getFreeTextureData();

    if(!imglib::loadBuffer(texData, bytes, size, options) || texData.m_readData.empty() ||
        (texData.width <= 0) || (texData.height <= 0))
        return failTextureLoad(&texData, "failed to decode", source);
    m_lastDecodedSize=Point((int)texData.width, (int)texData.height);
    return finishTextureLoad(texture, texData, maxSize, source);
}

Texture::Status SlotRenderer::loadTexturePixels(Texture &texture, int width, int height, size_t stride,
    const uint8_t *rgba, int maxSize)
{
    beginTextureLoad(texture);

    const char *source="RGBA pixels";

    if(!rgba || (width <= 0) || (height <= 0) || (stride < (size_t)width*4))
        return failTextureLoad(nullptr, "has no pixels or a stride shorter than its rows", source);
    if(overPixelLimit((size_t)width, (size_t)height))
        return failTextureLoad(nullptr, "is over the 64 megapixel decode limit", source);
    if((maxSize <= 0) && (std::max(width, height) > kMaxImageSide))
        return failTextureLoad(nullptr, "is larger than the largest image page and has no max size", source);

    SlotTextureData &texData=*getFreeTextureData();

    texData.width=(float)width;
    texData.height=(float)height;
    texData.m_readData.resize((size_t)width*height*4);
    for(int y=0; y<height; ++y)
        memcpy(texData.m_readData.data()+(size_t)y*width*4, rgba+(size_t)y*stride, (size_t)width*4);
    m_lastDecodedSize=Point(width, height);
    return finishTextureLoad(texture, texData, maxSize, source);
}

Texture::Status SlotRenderer::finishTextureLoad(Texture &texture, SlotTextureData &texData, int maxSize,
    const char *source)
{
    const size_t pixels=(size_t)texData.width*(size_t)texData.height;
    const size_t channels=texData.m_readData.size()/pixels;

    // RGBA8: RGB with alpha 1, as GL sampled an RGB8 texture; grey and grey with
    // alpha into each colour channel
    if((channels != 4) || (texData.m_readData.size() != pixels*4))
    {
        if((channels < 1) || (channels > 3) || (texData.m_readData.size() != pixels*channels))
            return failTextureLoad(&texData, "has a pixel format that is not drawn", source);

        std::vector<unsigned char> rgba(pixels*4);
        const unsigned char *in=texData.m_readData.data();

        for(size_t i=0; i<pixels; ++i, in+=channels)
        {
            const bool grey=(channels < 3);

            rgba[i*4+0]=in[0];
            rgba[i*4+1]=grey?in[0]:in[1];
            rgba[i*4+2]=grey?in[0]:in[2];
            rgba[i*4+3]=(channels == 2)?in[1]:255;
        }
        texData.m_readData.swap(rgba);
    }

    // the largest level of the image's mip chain that fits the side kept, each
    // level freeing the one above, so the full size is gone before the upload
    const int side=keptSide(maxSize);
    int width=(int)texData.width;
    int height=(int)texData.height;

    while(std::max(width, height) > side)
    {
        ImageMipLevel level=imageMipLevel(texData.m_readData.data(), width, height, 4);

        texData.m_readData.swap(level.texels);
        width=level.width;
        height=level.height;
    }
    texData.width=(float)width;
    texData.height=(float)height;

    if(!placeImage(texData))
        return failTextureLoad(&texData, "has no image layer left", source);

    texData.readable=texture.readable;
    if(!texture.readable)
    {
        texData.m_readData.clear();
        texData.m_readData.shrink_to_fit();
    }

    ++m_loadedTextures;
    m_lastTexture=texData.m_index;
    texture.dataIndex=texData.m_index;
    return Texture::Status::Loaded;
}

void SlotRenderer::freeTexture(const gweni::Texture &texture)
{
//    if(m_lastTexture != nullptr && m_lastTexture->first == texture)
//        m_lastTexture=nullptr;
    if((m_lastTexture != std::numeric_limits<size_t>::max()) && (m_lastTexture == texture.dataIndex))
        m_lastTexture=std::numeric_limits<size_t>::max();

    if((texture.dataIndex < m_textureData.size()) && (m_textureData[texture.dataIndex]))
    {
        // the layer may be handed out again; a slot still drawing it draws that image
        if(m_textureData[texture.dataIndex]->m_layer != NoImageLayer)
            --m_loadedTextures;
        m_freeTextureData.push_back(texture.dataIndex);
        deleteTextureData(m_textureData[texture.dataIndex]);
        m_textureData[texture.dataIndex]=nullptr;
    }
}

void SlotRenderer::deleteTextureData(SlotTextureData *textureData)
{
    if(textureData->m_layer != NoImageLayer)
        m_imagePages[textureData->m_imageClass].freeLayers.push_back(textureData->m_layer);
    delete textureData;
}

size_t SlotRenderer::imageClassFor(int width, int height)
{
    const int longer=std::max(width, height);
    size_t imageClass=0;

    while((imageClass+1 < ImageClasses) && (imageClassSide(imageClass) < longer))
        ++imageClass;
    return imageClass;
}

size_t SlotRenderer::allocateImageLayer(size_t imageClass)
{
    ImagePage &page=m_imagePages[imageClass];

    if(!page.freeLayers.empty())
    {
        const size_t layer=page.freeLayers.back();

        page.freeLayers.pop_back();
        return layer;
    }
    if(page.used < page.layers)
        return page.used++;

    // a page holds 8 of the smallest images at first, one of the largest, and
    // doubles from there
    const size_t limit=maxImageLayers();
    const size_t layers=std::min(page.layers?page.layers*2:std::max<size_t>(1, size_t(8) >> imageClass), limit);

    if(layers <= page.layers)
    {
        gweni::log::write(log::Level::Error, "image page %dx%d is full: %zu layers",
            imageClassSide(imageClass), imageClassSide(imageClass), page.layers);
        return NoImageLayer;
    }
    if(!allocateImagePage(imageClass, layers, page.used))
        return NoImageLayer;
    if(page.layers > 0)
        ++m_imagePageRegrows;
    page.layers=layers;
    return page.used++;
}

bool SlotRenderer::placeImage(SlotTextureData &texData)
{
    const int width=(int)texData.width;
    const int height=(int)texData.height;

    if(width <= 0 || height <= 0 || std::max(width, height) > kMaxImageSide)
        return false;

    const size_t imageClass=imageClassFor(width, height);
    const size_t layer=allocateImageLayer(imageClass);

    if(layer == NoImageLayer)
        return false;

    const std::vector<ImageMipLevel> levels=imageMipChain(texData.m_readData.data(), width, height, 4);

    uploadImageLayer(imageClass, layer, width, height, texData.m_readData.data(), levels.data(), levels.size());
    texData.m_imageClass=imageClass;
    texData.m_layer=layer;
    return true;
}

SlotTextureData *SlotRenderer::getFreeTextureData()
{
    SlotTextureData *textureData;

    if(!m_freeTextureData.empty())
    {
        size_t index=m_freeTextureData.back();
        m_freeTextureData.pop_back();

        // freeTexture() deletes the slot's data but keeps the index on the free
        // list, so a recycled slot can be empty.
        if(!m_textureData[index])
        {
            m_textureData[index]=new SlotTextureData();
            m_textureData[index]->m_index=index;
        }
        textureData=m_textureData[index];
    }
    else
    {
        textureData=new SlotTextureData();
        textureData->m_index=m_textureData.size();
        m_textureData.resize(m_textureData.size()+1);
        m_textureData.back()=textureData;
    }
    return textureData;
}

TextureHandle SlotRenderer::getTextureHandle(const Texture &texture) const
{
    size_t imageClass=0;
    size_t layer=0;

    if(!getTextureLayer(texture, imageClass, layer))
        return 0;
    return imagePageHandle(imageClass);
}

bool SlotRenderer::getTextureLayer(const Texture &texture, size_t &imageClass, size_t &layer) const
{
    if((texture.dataIndex >= m_textureData.size()) || !m_textureData[texture.dataIndex] ||
        (m_textureData[texture.dataIndex]->m_layer == NoImageLayer))
        return false;
    imageClass=m_textureData[texture.dataIndex]->m_imageClass;
    layer=m_textureData[texture.dataIndex]->m_layer;
    return true;
}

TextureHandle SlotRenderer::getFontAtlasHandle(const Font &requested) const
{
    auto it=m_fonts.find(resolveFont(requested, textPx(requested)));

    if((it != m_fonts.end()) && (it->second.atlas))
        return it->second.atlas->id;
    return 0;
}

size_t SlotRenderer::getFontAtlasDepth(const Font &requested) const
{
    auto it=m_fonts.find(resolveFont(requested, textPx(requested)));

    if((it != m_fonts.end()) && (it->second.atlas))
        return it->second.atlas->depth;
    return 0;
}

ftgl::texture_font_t *SlotRenderer::getTextureFont(const Font &requested) const
{
    auto it=m_fonts.find(resolveFont(requested, textPx(requested)));

    return (it != m_fonts.end())?it->second.textureFont:nullptr;
}

bool SlotRenderer::readFontAtlas(const Font &requested, std::vector<uint8_t> &rgba)
{
    auto it=m_fonts.find(resolveFont(requested, textPx(requested)));

    rgba.clear();
    if((it == m_fonts.end()) || !it->second.atlas || !it->second.atlas->id)
        return false;

    const texture_atlas_t &atlas=*it->second.atlas;

    return readTextureRgba(atlas.id, (int)atlas.width, (int)atlas.height, rgba);
}

TextureData SlotRenderer::getTextureData(const Texture &texture) const
{
    // a texture is its dataIndex: an unloaded or freed one has no data
    if((texture.dataIndex < m_textureData.size()) && m_textureData[texture.dataIndex] &&
        (m_textureData[texture.dataIndex]->m_layer != NoImageLayer))
        return *m_textureData[texture.dataIndex];
    return TextureData();
}

bool SlotRenderer::ensureTexture(const gweni::Texture &texture)
{
    if((texture.dataIndex >= m_textureData.size()) || !m_textureData[texture.dataIndex] ||
        (m_textureData[texture.dataIndex]->m_layer == NoImageLayer))
        return false;
    m_lastTexture=texture.dataIndex;
    return true;
}

//-------------------------------------------------------------------------------

SlotRenderer::SlotRenderer(ResourcePaths &paths, const Rect &viewRect)
    : Base(paths)
    , m_lastFont(nullptr)
    , m_lastTexture(std::numeric_limits<size_t>::max())
    , m_viewRect(viewRect)
{
    m_vertexes.reserve(minVertexSize);

    m_clippingBoxes.reserve(minClippingSize);

    m_uploadStats=(getenv("GWENI_UPLOAD_STATS") != nullptr);

    // <gap>,<cap>, two decimal counts that fit a size_t, the cap at least 1;
    // anything else (a sign, a space, a count past size_t) keeps the defaults
    if(const char *ranges=getenv("GWENI_PRIMITIVE_RANGES"))
    {
        auto parseCount=[](const char *&text, size_t &value)
        {
            if((*text < '0') || (*text > '9'))
                return false;
            char *end=nullptr;

            errno=0;
            const unsigned long long parsed=strtoull(text, &end, 10);

            if((errno == ERANGE) || (parsed > std::numeric_limits<size_t>::max()))
                return false;
            value=(size_t)parsed;
            text=end;
            return true;
        };
        const char *text=ranges;
        size_t gap=0, cap=0;

        if(parseCount(text, gap) && (*text++ == ',') && parseCount(text, cap) && (*text == '\0') && (cap > 0))
            setPrimitiveRangeLimits(gap, cap);
        else if(ranges[0] != '\0')
            gweni::log::write(log::Level::Error,
                "GWENI_PRIMITIVE_RANGES=%s: not <gap>,<cap>; the ranges keep %zu,%zu",
                ranges, kPrimitiveRangeGap, kMaxPrimitiveRanges);
    }
    // set but empty, or not a number, keeps the default
    if(const char *chunkSlots=getenv("GWENI_INDEX_CHUNK_SLOTS"))
    {
        char *end=nullptr;
        const unsigned long long value=strtoull(chunkSlots, &end, 10);

        if((end != chunkSlots) && (*end == '\0') && (chunkSlots[0] != '-'))
            m_chunkSlots=(size_t)std::min<unsigned long long>(std::max(value, 1ull), kSlotsPerChunk);
        else if(chunkSlots[0] != '\0')
            gweni::log::write(log::Level::Error,
                "GWENI_INDEX_CHUNK_SLOTS=%s: not a slot count; draws keep %zu slots",
                chunkSlots, kSlotsPerChunk);
    }
    m_sdfTextMin=defaultSdfTextMin;
    const char *sdfTextMin=getenv("GWENI_SDF_TEXT_MIN");

    // set but empty keeps the default, rather than reading as 0 (SDF everywhere)
    if(sdfTextMin && (sdfTextMin[0] != '\0'))
        m_sdfTextMin=std::max(0.0f, (float)atof(sdfTextMin));
    m_textDebug=getenv("GWENI_TEXT_DEBUG");

    if(const char *clipDebug=getenv("GWENI_CLIP_DEBUG"))
        m_clipDebugInset=std::max(0, atoi(clipDebug));

    m_textContrast=defaultTextContrast;
    if(const char *textContrast=getenv("GWENI_TEXT_CONTRAST"))
    {
        if(textContrast[0] != '\0')
            m_textContrast=std::max(0.0f, (float)atof(textContrast));
    }

    // set but empty keeps the default, as GWENI_SDF_TEXT_MIN does
    if(const char *textAA=getenv("GWENI_TEXT_AA"))
    {
        const std::string value(textAA);

        if(value == "gray")
            m_textAA=TextAA::Grayscale;
        else if(value == "rgb")
            m_textAA=TextAA::LcdRGB;
        else if(value == "bgr")
            m_textAA=TextAA::LcdBGR;
        else if(!value.empty())
            gweni::log::write(log::Level::Error,
                "GWENI_TEXT_AA=%s: not gray, rgb or bgr; text stays grayscale", textAA);
    }
}

bool SlotRenderer::textDebugMatches(const gweni::String &text) const
{
    if(m_textDebug == nullptr)
        return false;
    if(m_textDebug[0] == '\0')
        return true;
    return text.find(m_textDebug) != gweni::String::npos;
}

SlotRenderer::~SlotRenderer()
{
    // the backend's destructor released every texture (releaseResources); what is
    // left is CPU-side
    m_lastFont=nullptr;
    for(TextRun &run:m_textRuns)
        run.fontData=nullptr;
}

void SlotRenderer::releaseResources()
{
    // each font's atlas texture, then its freetype-gl manager
    m_lastFont=nullptr;
    for(auto it=m_fonts.begin(); it != m_fonts.end();)
        it=eraseFont(it);
    m_failedFonts.clear();
    for(TextRun &run:m_textRuns)
        run.fontData=nullptr;

    for(SlotTextureData *textureData:m_textureData)
    {
        if(textureData)
            deleteTextureData(textureData);
    }
    m_textureData.clear();
    m_freeTextureData.clear();
    m_loadedTextures=0;
    destroyImagePages();
    for(ImagePage &page:m_imagePages)
        page=ImagePage();
    m_imagePageRegrows=0;
}

void SlotRenderer::init()
{
    //the backend's buffers are freshly created, so nothing of the mirror is on the device yet
    m_vertexBufferCapacity=0;
    m_vertexBufferSent=0;
    m_clipBoxesSent=0;
    m_textVertexBufferCapacity=0;
    m_textVertexBufferSent=0;
    m_textClipBoxesSent=0;
    m_clipTextureRows=0;
    m_textClipTextureRows=0;

    // Until a canvas sets its region, the whole view is visible: the shaders discard
    // everything outside the clip box, and an unset region would be an empty one.
    setClipRegion(m_viewRect);
}

void SlotRenderer::setViewRect(const gweni::Rect &viewRect)
{
    m_viewRect=viewRect;
}

void SlotRenderer::setViewTransform(const float matrix[16])
{
    if(m_hasViewTransform && std::equal(matrix, matrix+16, m_viewTransform))
        return;

    std::copy(matrix, matrix+16, m_viewTransform);
    m_hasViewTransform=true;
    viewTransformChanged();
}

void SlotRenderer::clearViewTransform()
{
    if(!m_hasViewTransform)
        return;

    static const float identity[16]={ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

    std::copy(identity, identity+16, m_viewTransform);
    m_hasViewTransform=false;
    viewTransformChanged();
}

void SlotRenderer::begin()
{
    beginFrame();
}

void SlotRenderer::end()
{
    flush();
}

void SlotRenderer::setIndexChunkSlots(size_t slots)
{
    slots=std::min(std::max<size_t>(slots, 1), kSlotsPerChunk);

    if(slots == m_chunkSlots)
        return;

    m_chunkSlots=slots;

    // what the backend made per chunk starts at the old chunk size's slots
    indexChunkSlotsChanged();

    // and each chunk's slots that can show are counted over the old chunks
    recountPrimitiveChunks();

    // and the atlases' draws are split at the old chunks' edges
    for(auto &entry:m_fonts)
        entry.second.drawListStale=true;
}

void SlotRenderer::ensurePrimitiveSlots(size_t primitiveId)
{
    if(m_clippingBoxes.size() < primitiveId+1)
        m_clippingBoxes.resize(primitiveId+1);

    if(m_vertexes.size() < primitiveId*4+4)
        m_vertexes.resize(primitiveId*4+4);
}

//! The lowest set bit of a word that is not 0
static size_t lowestBit(uint64_t word)
{
#if defined(__GNUC__) || defined(__clang__)
    return (size_t)__builtin_ctzll(word);
#else
    size_t bit=0;

    while(((word>>bit)&1) == 0)
        ++bit;
    return bit;
#endif
}

void SlotRenderer::updateSlotVisibility(size_t slot)
{
    if(m_vertexes.size() < slot*4+4)
        return;

    const Vertex *vertexes=&m_vertexes[slot*4];
    const bool visible=(vertexes[0].style|vertexes[1].style|vertexes[2].style|vertexes[3].style) != 0;
    const size_t word=slot/64;
    const uint64_t bit=uint64_t(1)<<(slot%64);

    if(m_visibleSlots.size() <= word)
        m_visibleSlots.resize(word+1, 0);
    if(((m_visibleSlots[word]&bit) != 0) == visible)
        return;

    const size_t chunk=slot/m_chunkSlots;

    if(m_primitiveChunks.size() <= chunk)
        m_primitiveChunks.resize(chunk+1);

    PrimitiveChunkSlots &slots=m_primitiveChunks[chunk];

    slots.stale=true;
    if(visible)
    {
        m_visibleSlots[word]|=bit;
        ++slots.visible;
    }
    else
    {
        m_visibleSlots[word]&=~bit;
        --slots.visible;
    }
}

void SlotRenderer::buildPrimitiveRanges(size_t chunk)
{
    PrimitiveChunkSlots &slots=m_primitiveChunks[chunk];
    const size_t first=chunk*m_chunkSlots;
    const size_t limit=std::min(first+m_chunkSlots, m_visibleSlots.size()*64);
    std::vector<std::pair<size_t, size_t>> &runs=m_primitiveRuns;

    // the runs of set bits in [first, limit), a run less than the gap after the
    // one before it joined to it
    runs.clear();
    for(size_t slot=first; slot < limit;)
    {
        const uint64_t shown=m_visibleSlots[slot/64]>>(slot%64);

        if(shown == 0)
        {
            slot=(slot/64+1)*64;
            continue;
        }

        const size_t start=slot+lowestBit(shown);

        // the word's bits past the chunk are the next chunk's
        if(start >= limit)
            break;

        size_t stop=start;

        // the first slot from start on that cannot show; the shift brings in
        // zeros, which read as slots that can
        while(stop < limit)
        {
            const uint64_t hidden=(~m_visibleSlots[stop/64])>>(stop%64);

            if(hidden == 0)
            {
                stop=(stop/64+1)*64;
                continue;
            }
            stop+=lowestBit(hidden);
            break;
        }
        stop=std::min(stop, limit);

        if(!runs.empty() && (start-runs.back().second < m_primitiveRangeGap))
            runs.back().second=stop;
        else
            runs.emplace_back(start, stop);
        slot=stop;
    }

    // too many: the smallest gaps merged, the leftmost first among equal ones,
    // until the cap is met
    if(runs.size() > m_maxPrimitiveRanges)
    {
        std::vector<std::pair<size_t, size_t>> &gaps=m_primitiveGaps;
        const size_t merges=runs.size()-m_maxPrimitiveRanges;

        gaps.clear();
        for(size_t i=0; i+1 < runs.size(); ++i)
            gaps.emplace_back(runs[i+1].first-runs[i].second, i);
        std::nth_element(gaps.begin(), gaps.begin()+(merges-1), gaps.end());
        // gaps[0..merges) are the gaps to close; mark each by emptying the run
        // after it into the one before
        std::sort(gaps.begin(), gaps.begin()+merges,
            [](const std::pair<size_t, size_t> &a, const std::pair<size_t, size_t> &b) { return a.second < b.second; });

        size_t kept=0;

        for(size_t i=0, merge=0; i < runs.size(); ++i)
        {
            if((i > 0) && (merge < merges) && (gaps[merge].second == i-1))
            {
                runs[kept-1].second=runs[i].second;
                ++merge;
            }
            else
                runs[kept++]=runs[i];
        }
        runs.resize(kept);
    }

    ChunkDraw &draw=slots.draw;

    draw.chunk=chunk;
    draw.counts.clear();
    draw.offsets.clear();
    slots.slots=0;
    for(const std::pair<size_t, size_t> &run:runs)
    {
        draw.counts.push_back(int((run.second-run.first)*6));
        draw.offsets.push_back(uintptr_t((run.first-first)*6*sizeof(uint16_t)));
        slots.slots+=run.second-run.first;
    }
    slots.stale=false;
    ++m_frameStats.primitiveRangeBuilds;
}

void SlotRenderer::recountPrimitiveChunks()
{
    m_primitiveChunks.clear();
    for(size_t word=0; word<m_visibleSlots.size(); ++word)
    {
        for(uint64_t bits=m_visibleSlots[word]; bits != 0; bits&=bits-1)
        {
            const size_t slot=word*64+lowestBit(bits);
            const size_t chunk=slot/m_chunkSlots;

            if(m_primitiveChunks.size() <= chunk)
                m_primitiveChunks.resize(chunk+1);
            ++m_primitiveChunks[chunk].visible;
        }
    }
}

void SlotRenderer::setPrimitiveRangeLimits(size_t gap, size_t maxRanges)
{
    m_primitiveRangeGap=gap;
    m_maxPrimitiveRanges=std::max<size_t>(maxRanges, 1);
    for(PrimitiveChunkSlots &slots:m_primitiveChunks)
        slots.stale=true;
}

void SlotRenderer::readPrimitiveSlot(size_t primitiveId, PrimitiveSlot &slot) const
{
    memcpy(slot.vertexes, &m_vertexes[primitiveId*4], sizeof(slot.vertexes));
    slot.box=m_clippingBoxes[primitiveId];
}

SlotRenderer::MirrorSnapshot SlotRenderer::snapshotMirror() const
{
    MirrorSnapshot snapshot;

    const size_t primitiveLimit=std::min(primitiveIdCount(), m_vertexes.size()/4);

    for(size_t id=0; id<primitiveLimit; ++id)
    {
        if(!isPrimitiveIdLive(id))
            continue;

        const uint8_t *vertexes=reinterpret_cast<const uint8_t *>(&m_vertexes[id*4]);

        snapshot.primitives[id].assign(vertexes, vertexes+4*sizeof(Vertex));
    }

    const size_t textLimit=std::min(textIdCount(), m_textRuns.size());

    for(size_t id=0; id<textLimit; ++id)
    {
        if(!isTextIdLive(id))
            continue;

        const TextRun &run=m_textRuns[id];
        std::vector<uint8_t> &bytes=snapshot.texts[id];

        bytes.push_back(run.hidden?1:0);
        if(run.glyphs == 0)
            continue;

        const uint8_t *vertexes=reinterpret_cast<const uint8_t *>(&m_textVertexes[run.start*4]);

        bytes.insert(bytes.end(), vertexes, vertexes+run.glyphs*4*sizeof(ftgl::glyph_vertex_t));
    }

    return snapshot;
}

void SlotRenderer::markPrimitiveChanges(size_t primitiveId, const PrimitiveSlot &before)
{
    if(memcmp(before.vertexes, &m_vertexes[primitiveId*4], sizeof(before.vertexes)) != 0)
        m_dirtyVertexSlots.mark(primitiveId);

    if(memcmp(&before.box, &m_clippingBoxes[primitiveId], sizeof(Box)) != 0)
        m_dirtyClipSlots.mark(primitiveId);
}

void SlotRenderer::uploadDirty(SlotPath path, size_t &capacityElements, size_t &sentElements,
    const void *data, size_t elementSize, size_t elementCount,
    DirtySlots &dirty, size_t elementsPerSlot)
{
    if(elementCount == 0)
        return;

    if((sentElements >= elementCount) && dirty.empty())
        return; //device already holds the mirror

    if(capacityElements < elementCount)
    {
        // The mirror outgrew the allocation: allocate half as much again as it
        // holds now (whole slots), more than 1.5 times the old capacity, so a
        // mirror growing a slot at a time regrows O(log n) times, and one that has
        // just grown a lot (a first frame) has room to grow before the next; then
        // send all of it once. The slots past the mirror are undefined on the
        // device, and nothing draws them: the draws stop at the mirror's slots, and
        // a slot is sent when the mirror reaches it.
        size_t capacity=elementCount+elementCount/2;

        capacity=(capacity+elementsPerSlot-1)/elementsPerSlot*elementsPerSlot;
        allocateVertexBuffer(path, capacity*elementSize);
        uploadVertexRange(path, 0, elementCount*elementSize, data);
        capacityElements=capacity;
        sentElements=elementCount;
        dirty.clear();

        ++m_frameStats.bufferRegrows;
        ++m_frameStats.uploadCalls;
        m_frameStats.uploadBytes+=elementCount*elementSize;
        return;
    }

    // the slots the mirror gained since the last upload go up with the dirty runs
    dirty.markRange(sentElements/elementsPerSlot, (elementCount+elementsPerSlot-1)/elementsPerSlot);
    sentElements=elementCount;

    const char *bytes=static_cast<const char *>(data);

    dirty.forEachRun([&](size_t firstSlot, size_t lastSlot)
    {
        size_t first=firstSlot*elementsPerSlot;
        size_t last=std::min(lastSlot*elementsPerSlot, elementCount);

        if(last <= first)
            return;

        uploadVertexRange(path, first*elementSize, (last-first)*elementSize,
            bytes+(first*elementSize));

        ++m_frameStats.uploadCalls;
        m_frameStats.uploadBytes+=(last-first)*elementSize;
    });

    dirty.clear();
}

void SlotRenderer::uploadDirtyClipBoxes(SlotPath path, size_t &rows, size_t &sentBoxes,
    const std::vector<Box> &boxes, DirtySlots &dirty)
{
    const size_t count=boxes.size();

    if(count == 0)
        return;

    if((sentBoxes >= count) && dirty.empty())
        return; //device already holds the mirror

    const size_t needed=(count+kClipColumns-1)/kClipColumns;

    if(rows < needed)
    {
        // The mirror outgrew the table's rows: re-allocate it with half as many
        // rows again as the mirror needs, as a vertex buffer, rounded up to a power
        // of two, as the width is (llvmpipe compiles a shader variant per sampler
        // shape, power-of-two or not among it, and a texture of any other height
        // would compile one more, mid-frame), so it regrows O(log n) times, and
        // send every box again.
        const size_t wanted=needed+needed/2;
        size_t height=std::max<size_t>(rows, 1);

        while(height < wanted)
            height*=2;

        const size_t maxRows=maxClipBoxRows();

        if(height > maxRows)
        {
            if(needed > maxRows)
                gweni::log::write(log::Level::Error,
                    "%zu clip boxes need %zu rows of %zu, past the device's %zu",
                    count, needed, kClipColumns, maxRows);
            height=std::max(needed, maxRows);
        }
        allocateClipBoxes(path, height);
        rows=height;
        uploadClipBoxRange(path, boxes, 0, count);
        ++m_frameStats.bufferRegrows;
    }
    else
    {
        // the rows hold the mirror: its dirty boxes, and the ones it gained
        dirty.markRange(sentBoxes, count);
        dirty.forEachRun([&](size_t firstSlot, size_t lastSlot)
        {
            const size_t last=std::min(lastSlot, count);

            if(last > firstSlot)
                uploadClipBoxRange(path, boxes, firstSlot, last);
        });
    }

    sentBoxes=count;
    dirty.clear();
}

void SlotRenderer::uploadClipBoxRange(SlotPath path, const std::vector<Box> &boxes, size_t first, size_t last)
{
    // A row's boxes are consecutive in the mirror, and so are whole rows, so each
    // piece is sent straight from it.
    auto send=[&](size_t x, size_t y, size_t width, size_t height)
    {
        uploadClipBoxRows(path, x, y, width, height, &boxes[y*kClipColumns+x]);

        ++m_frameStats.uploadCalls;
        m_frameStats.uploadBytes+=width*height*sizeof(Box);
    };

    const size_t firstRow=first/kClipColumns;
    const size_t lastRow=(last-1)/kClipColumns;

    if(firstRow == lastRow)
    {
        send(first%kClipColumns, firstRow, last-first, 1);
        return;
    }

    size_t wholeFirst=firstRow;

    if(first%kClipColumns != 0)
    {
        send(first%kClipColumns, firstRow, kClipColumns-first%kClipColumns, 1);
        ++wholeFirst;
    }

    const size_t lastWidth=last-lastRow*kClipColumns;
    const size_t wholeEnd=(lastWidth == kClipColumns)?lastRow+1:lastRow;

    if(wholeEnd > wholeFirst)
        send(0, wholeFirst, kClipColumns, wholeEnd-wholeFirst);
    if(lastWidth != kClipColumns)
        send(0, lastRow, lastWidth, 1);
}

void SlotRenderer::uploadDirtyAtlases()
{
    for(auto &entry:m_fonts)
    {
        texture_atlas_t *atlas=entry.second.atlas;

        if(atlas->dirty_y0 >= atlas->dirty_y1)
            continue;

        //full-width rows are contiguous in atlas->data, depth bytes per texel
        assert((atlas->depth == 1) || (atlas->depth == 3));

        size_t rows=atlas->dirty_y1-atlas->dirty_y0;

        uploadAtlasRows(atlas->id, *atlas, atlas->dirty_y0, rows);

        ++m_frameStats.atlasUploadCalls;
        m_frameStats.atlasUploadBytes+=rows*atlas->width*atlas->depth;

        atlas->dirty_y0=0;
        atlas->dirty_y1=0;
    }
}

void SlotRenderer::recordWrittenIds()
{
    // The slots the counts above take, by id: a primitive id is its slot, and a
    // glyph slot is reported under the text id whose run holds it, or held it
    // last. Only the dirty slots are visited, so a settled frame costs nothing.
    m_writtenPrimitiveIds=m_dirtyVertexSlots.slots();
    for(size_t slot: m_dirtyClipSlots.slots())
    {
        if(!m_dirtyVertexSlots.marked(slot))
            m_writtenPrimitiveIds.push_back(slot);
    }
    std::sort(m_writtenPrimitiveIds.begin(), m_writtenPrimitiveIds.end());

    m_writtenTextSlotOwners.clear();

    const auto owner=[this](size_t slot)
    {
        return (slot < m_textSlotOwner.size())?m_textSlotOwner[slot]:kNoTextOwner;
    };

    for(size_t slot: m_dirtyTextVertexSlots.slots())
        m_writtenTextSlotOwners.push_back(owner(slot));
    for(size_t slot: m_dirtyTextClipSlots.slots())
    {
        if(!m_dirtyTextVertexSlots.marked(slot))
            m_writtenTextSlotOwners.push_back(owner(slot));
    }
    std::sort(m_writtenTextSlotOwners.begin(), m_writtenTextSlotOwners.end());

    m_writtenTextIds.clear();
    for(size_t textId: m_writtenTextSlotOwners)
    {
        if(m_writtenTextIds.empty() || (m_writtenTextIds.back().textId != textId))
            m_writtenTextIds.push_back({textId, 0});
        ++m_writtenTextIds.back().glyphSlots;
    }
}

FrameCounts SlotRenderer::frameStats() const
{
    FrameCounts counts;

    counts.counted=true;
    counts.frameIndex=m_frameStats.frameIndex;
    counts.primitiveSlotsWritten=m_frameStats.primitiveSlotsWritten;
    counts.textSlotsWritten=m_frameStats.textSlotsWritten;
    counts.uploadCalls=m_frameStats.uploadCalls+m_frameStats.atlasUploadCalls;
    counts.uploadBytes=m_frameStats.uploadBytes+m_frameStats.atlasUploadBytes;
    counts.drawCalls=m_frameStats.drawCalls;
    counts.flushMicros=m_frameStats.flushMicros;
    return counts;
}

void SlotRenderer::eraseIdleFonts()
{
    size_t waiting=0;

    for(auto it=m_fonts.begin(); it != m_fonts.end();)
    {
        FontData &fontData=it->second;

        if(fontData.idleSince == 0)
        {
            ++it;
            continue;
        }
        // a run or an owner took it back
        if((fontData.owners > 0) || !fontData.textRuns.empty())
        {
            fontData.idleSince=0;
            ++it;
            continue;
        }
        if(m_flushes+1-fontData.idleSince < kIdleFontFlushes)
        {
            ++waiting;
            ++it;
            continue;
        }
        if(m_lastFont == &(*it))
            m_lastFont=nullptr;
        it=eraseFont(it);
    }
    m_idleFonts=waiting;
}

void SlotRenderer::flush()
{
    const uint64_t flushStart=platform::profileTimeMicros();

    // no run is laid out in them, so nothing this flush draws uses them
    if(m_idleFonts > 0)
        eraseIdleFonts();
    ++m_flushes;

    m_frameStats.uploadCalls=0;
    m_frameStats.uploadBytes=0;
    m_frameStats.drawCalls=0;
    m_frameStats.primitiveSlotsDrawn=0;
    m_frameStats.primitiveRanges=0;
    m_frameStats.primitiveRangeBuilds=0;
    m_frameStats.textDraws=0;
    m_frameStats.atlasUploadCalls=0;
    m_frameStats.atlasUploadBytes=0;
    m_frameStats.bufferRegrows=0;
    m_frameStats.imagePageRegrows=m_imagePageRegrows;
    m_imagePageRegrows=0;
    //the slots this frame uploads: every draw, hide, show and release since the last
    //flush, each slot once however many of them touched it
    m_frameStats.primitiveSlotsWritten=m_dirtyVertexSlots.count();
    //a slot whose clip box changed and its vertexes did not is written too
    for(size_t slot: m_dirtyClipSlots.slots())
    {
        if(!m_dirtyVertexSlots.marked(slot))
            ++m_frameStats.primitiveSlotsWritten;
    }
    m_frameStats.textSlotsWritten=m_dirtyTextVertexSlots.count();
    //a glyph slot whose clip box changed and its vertexes did not is written too
    for(size_t slot: m_dirtyTextClipSlots.slots())
    {
        if(!m_dirtyTextVertexSlots.marked(slot))
            ++m_frameStats.textSlotsWritten;
    }
    recordWrittenIds();
    m_frameStats.mirrorBytes=
        m_vertexes.size()*sizeof(Vertex)
        +m_clippingBoxes.size()*sizeof(Box)
        +m_textVertexes.size()*sizeof(glyph_vertex_t)
        +m_textClipBoxes.size()*sizeof(Box);

    //glyphs measureText (logic phase) or renderText rasterised since the last frame; done
    //even when nothing draws text this frame, so they are on the GPU before first use
    uploadDirtyAtlases();

    if(!m_vertexes.empty())
    {
        //which slots can show changes only with a slot's bytes, so only in a dirty
        //slot; the upload below clears them
        for(size_t slot: m_dirtyVertexSlots.slots())
            updateSlotVisibility(slot);

        //copy only the primitive slots that changed since the last frame
        uploadDirty(SlotPath::Primitives, m_vertexBufferCapacity, m_vertexBufferSent,
            m_vertexes.data(), sizeof(Vertex), m_vertexes.size(), m_dirtyVertexSlots, 4);
        uploadDirtyClipBoxes(SlotPath::Primitives, m_clipTextureRows, m_clipBoxesSent,
            m_clippingBoxes, m_dirtyClipSlots);

        // One draw per chunk of m_chunkSlots slots, in slot order as a single draw
        // over the whole buffer would go, so blending and depth come out the same.
        // Each draws its runs of slots that can show (small gaps between them
        // included, as 4 vertexes and 6 indexes that make nothing apiece), every
        // slot 2 triangles of the static quad index buffer from the run's offset,
        // with the chunk's vertex input pointed at its first slot; the vertex
        // shader's slot is then slotBase + (vertex index >> 2). A chunk with no
        // slot that can show is not drawn: a style-0 slot makes no fragment, so
        // leaving it out changes no pixel. A chunk's ranges are built again only
        // when its bits changed.
        const size_t slots=m_vertexes.size()/4;

        beginPrimitiveDraws();
        for(size_t chunk=0, first=0; first<slots; ++chunk, first+=m_chunkSlots)
        {
            if(m_drawEveryPrimitiveSlot)
            {
                const size_t count=std::min(slots-first, m_chunkSlots);

                m_wholeChunkDraw.chunk=chunk;
                m_wholeChunkDraw.counts.assign(1, int(count*6));
                m_wholeChunkDraw.offsets.assign(1, 0);
                drawPrimitiveChunk(m_wholeChunkDraw);
                ++m_frameStats.drawCalls;
                ++m_frameStats.primitiveRanges;
                m_frameStats.primitiveSlotsDrawn+=count;
                continue;
            }

            if(chunk >= m_primitiveChunks.size())
                break;

            PrimitiveChunkSlots &chunkSlots=m_primitiveChunks[chunk];

            if(chunkSlots.visible == 0)
                continue;
            if(chunkSlots.stale)
                buildPrimitiveRanges(chunk);

            drawPrimitiveChunk(chunkSlots.draw);
            ++m_frameStats.drawCalls;
            m_frameStats.primitiveRanges+=chunkSlots.draw.counts.size();
            m_frameStats.primitiveSlotsDrawn+=chunkSlots.slots;
        }
        endPrimitiveDraws();
    }

    if(!m_textVertexes.empty())
    {
        //copy only the glyph slots that changed since the last frame
        uploadDirty(SlotPath::Text, m_textVertexBufferCapacity, m_textVertexBufferSent,
            m_textVertexes.data(), sizeof(glyph_vertex_t), m_textVertexes.size(), m_dirtyTextVertexSlots, 4);
        uploadDirtyClipBoxes(SlotPath::Text, m_textClipTextureRows, m_textClipBoxesSent,
            m_textClipBoxes, m_dirtyTextClipSlots);

        // Each atlas draws the runs laid out in its font: one draw per index chunk
        // its runs reach (one per atlas until the glyph slots outgrow a chunk),
        // none for an atlas with nothing to show. A chunk's draw reads the static
        // quad indexes from its own first slot, and the vertex shader's slot is
        // slotBase plus the slot within the chunk. The atlases go in the same
        // order, and each one's chunks in slot order, as the one draw per atlas
        // over the whole buffer did. The slots are not moved, so the upload above
        // stays per dirty slot.
        beginTextDraws();
        for(auto &entry:m_fonts)
        {
            FontData &fontData=entry.second;

            if(fontData.drawListStale)
                buildTextDrawList(fontData);
            else
                verifyTextDrawList(fontData);
            if(fontData.drawChunks.empty())
                continue;

            beginTextAtlas(fontData);
            for(const ChunkDraw &draw:fontData.drawChunks)
            {
                drawTextChunk(draw);
                ++m_frameStats.drawCalls;
                ++m_frameStats.textDraws;
            }
        }
        endTextDraws();
    }

    submitFrame();

    ++m_frameStats.frameIndex;
    m_frameStats.flushMicros=platform::profileTimeMicros()-flushStart;

    if(m_uploadStats)
    {
        //qualified: gweni::printf(String&, ...) would otherwise hide the C one
        ::printf("[upload] frame %zu: %zu calls, %zu bytes (mirror %zu bytes, %zu regrows), atlas %zu calls, %zu bytes,"
            " %zu of %zu primitive slots drawn in %zu ranges (%zu chunks' ranges built)\n",
            m_frameStats.frameIndex, m_frameStats.uploadCalls, m_frameStats.uploadBytes,
            m_frameStats.mirrorBytes, m_frameStats.bufferRegrows, m_frameStats.atlasUploadCalls,
            m_frameStats.atlasUploadBytes, m_frameStats.primitiveSlotsDrawn, m_vertexes.size()/4,
            m_frameStats.primitiveRanges, m_frameStats.primitiveRangeBuilds);
        fflush(stdout);
    }
}

void SlotRenderer::setVertex(Vertex &vertex, uint8_t type, int x, int y, int z, float u, float v)
{
    vertex.style=type;

    // a corner outside int16_t wraps here, and cullOutOfRangeQuad then collapses the quad
    vertex.x=(int16_t)x;
//    vertex.y=m_viewRect.h - y;
    vertex.y=(int16_t)y;
    vertex.z=(uint16_t)std::clamp(z, 0, kMaxZIndex);

//    if(m_activeProgram == 1)
    {
        vertex.tx=u;
        vertex.ty=v;
    }

    // SDF fields are only used by styles 3-9 (drawLine, writeRoundedQuad, drawIcon, drawArc, drawGrid, drawTrapezoid)
    // and style 2's page, layer and size (drawTexturedRect); keep them 0 otherwise.
    vertex.lx=0;
    vertex.ly=0;
    vertex.radius=0;
    vertex.border=0;
    vertex.cx=0;
    vertex.cy=0;
    vertex.bx=0;
    vertex.by=0;
    vertex.dx=0;
    vertex.dy=0;
//    else
//    {
//        vertex.tx=(m_color.r<<8)&m_color.g;
//        vertex.ty=(m_color.b<<8)&m_color.a;
////        vertex.color=glm::vec4(
////            m_color.r / 255.0f,
////            m_color.g / 255.0f,
////            m_color.b / 255.0f,
////            m_color.a / 255.0f
////        );
//    }
}

void SlotRenderer::cullOutOfRangeQuad(size_t primitiveId, const gweni::Rect &rect)
{
    const int64_t low=std::numeric_limits<int16_t>::min();
    const int64_t high=std::numeric_limits<int16_t>::max();
    const int64_t x0=rect.x;
    const int64_t y0=rect.y;
    const int64_t x1=x0+rect.w;
    const int64_t y1=y0+rect.h;

    auto inRange=[&](int64_t v) { return v >= low && v <= high; };

    if(inRange(x0) && inRange(y0) && inRange(x1) && inRange(y1))
        return;

    for(size_t i=0; i<4; ++i)
    {
        Vertex &vertex=m_vertexes[primitiveId*4+i];

        vertex.x=0;
        vertex.y=0;
    }
}

void SlotRenderer::drawFilledRect(size_t primitiveId, gweni::Rect rect, int zIndex)
{
    translate(rect);

    size_t vertexIndex=primitiveId*4;

    ensurePrimitiveSlots(primitiveId);

    PrimitiveSlot before;

    readPrimitiveSlot(primitiveId, before);

    m_clippingBoxes[primitiveId]=makeClipBox();

#ifdef DEBUG_RENDER
    std::cout<<"Rendering Fill: "<<primitiveId<<"("<<rect.x<<", "<<rect.y<<", "<<rect.w<<", "<<rect.h<<" : "<<zIndex<<") color: "<<(int)m_color.r<<", "<<(int)m_color.g<<", "<<(int)m_color.b<<", "<<(int)m_color.a<<"\n";
#endif//DEBUG_RENDER

    setVertex(m_vertexes[vertexIndex++], 1, rect.x, rect.y, zIndex, m_color.rg(), m_color.ba());
    setVertex(m_vertexes[vertexIndex++], 1, rect.x+rect.w, rect.y, zIndex, m_color.rg(), m_color.ba());
    setVertex(m_vertexes[vertexIndex++], 1, rect.x, rect.y+rect.h, zIndex, m_color.rg(), m_color.ba());
//    setVertex(m_vertexes[vertexIndex++], rect.x+rect.w, rect.y);
    setVertex(m_vertexes[vertexIndex++], 1, rect.x+rect.w, rect.y+rect.h, zIndex, m_color.rg(), m_color.ba());
//    setVertex(m_vertexes[vertexIndex++], rect.x, rect.y+rect.h);

    cullOutOfRangeQuad(primitiveId, rect);
    markPrimitiveChanges(primitiveId, before);

//    addVert(rect.x, rect.y);
//    addVert(rect.x+rect.w, rect.y);
//    addVert(rect.x, rect.y+rect.h);
//    addVert(rect.x+rect.w, rect.y);
//    addVert(rect.x+rect.w, rect.y+rect.h);
//    addVert(rect.x, rect.y+rect.h);
}

void SlotRenderer::drawRoundedRect(size_t primitiveId, gweni::Rect rect, float radius, float border,
    gweni::Color fillTop, gweni::Color fillBottom, gweni::Color borderColor, int zIndex)
{
    writeRoundedQuad(primitiveId, rect, zIndex, 4, radius, border, fillTop, fillTop,
        fillBottom, gweni::Color(0, 0, 0, 0), gweni::Color(0, 0, 0, 0), borderColor);
}

void SlotRenderer::drawRoundedRect(size_t primitiveId, gweni::Rect rect, float radius, float border,
    gweni::Color fillTop, gweni::Color fillBottom, gweni::Color borderColor, int zIndex, uint8_t edges,
    float reach)
{
    writeRoundedQuad(primitiveId, rect, zIndex, 4, radius, border, fillTop, fillTop,
        fillBottom, gweni::Color(0, 0, 0, 0), gweni::Color(0, 0, 0, 0), borderColor, edges, reach);
}

void SlotRenderer::drawBilinearRect(size_t primitiveId, gweni::Rect rect, int zIndex,
    gweni::Color c00, gweni::Color c10, gweni::Color c01, gweni::Color c11,
    float radius, float border, gweni::Color borderColor)
{
    // top edge in color0, bottom edge in color3, each per side
    writeRoundedQuad(primitiveId, rect, zIndex, 6, radius, border, c00, c10,
        gweni::Color(0, 0, 0, 0), c01, c11, borderColor);
}

void SlotRenderer::drawCheckerRect(size_t primitiveId, gweni::Rect rect, int zIndex,
    gweni::Color fill, gweni::Color light, gweni::Color dark,
    float radius, float border, gweni::Color borderColor)
{
    writeRoundedQuad(primitiveId, rect, zIndex, 7, radius, border, fill, fill,
        light, dark, dark, borderColor);
}

//! A rect's size as its half-size in half-pixel units, the Vertex::lx/ly of its
//! corners: the size itself, clamped to what an int16_t holds.
static int halfPixelSize(int size)
{
    return std::min(std::max(size, 0), (int)std::numeric_limits<int16_t>::max());
}

void SlotRenderer::writeRoundedQuad(size_t primitiveId, gweni::Rect rect, int zIndex, uint8_t style,
    float radius, float border, gweni::Color color0Left, gweni::Color color0Right,
    gweni::Color color1, gweni::Color color3Left, gweni::Color color3Right,
    gweni::Color borderColor, uint8_t edges, float reach)
{
    translate(rect);

    // A side without a border reaches over a neighbour's border of `reach`, as
    // wide as that border is drawn (below), whatever its edge snapped to.
    if(reach > 0.0f)
    {
        const int over=std::max(snap(reach), 1);

        if(edges&NoBorderLeft)
        {
            rect.x-=over;
            rect.w+=over;
        }
        if(edges&NoBorderRight)
            rect.w+=over;
        if(edges&NoBorderTop)
        {
            rect.y-=over;
            rect.h+=over;
        }
        if(edges&NoBorderBottom)
            rect.h+=over;
    }

    size_t vertexIndex=primitiveId*4;

    ensurePrimitiveSlots(primitiveId);

    PrimitiveSlot before;

    readPrimitiveSlot(primitiveId, before);

    m_clippingBoxes[primitiveId]=makeClipBox();

    // The quad is the rect, and the SDF's local position runs from -size/2 to
    // +size/2 across it, so the drawn shape and the quad coincide exactly at any
    // size. lx/ly are in half-pixel units (+/-w, +/-h), which keeps an odd size's
    // half (3.5) exact.
    const int x0=rect.x;
    const int y0=rect.y;
    const int x1=rect.x+rect.w;
    const int y1=rect.y+rect.h;
    const int hw=halfPixelSize(rect.w);
    const int hh=halfPixelSize(rect.h);

    // Scaled to device pixels. A border stays a whole number of them, and at
    // least one, so it stays crisp at a fractional scale. The radius is the low
    // byte and the edge mask the high one: a radius over 255 px is 255, which the
    // shader cuts to the rect's half-size anyway, so it draws the same unless the
    // rect is over 510 px on its shorter side.
    const long limit=std::numeric_limits<uint16_t>::max();
    const uint16_t rad=(uint16_t)(std::min(std::max((long)snap(radius), 0L), 255L)
        | ((uint16_t)edges << 8));
    const uint16_t bor=(uint16_t)((border > 0.0f)
        ? std::min(std::max((long)snap(border), 1L), limit) : 0L);

    auto setRounded=[&](size_t vi, int px, int py, int lx, int ly, const gweni::Color &color0,
        const gweni::Color &color3)
    {
        Vertex &v=m_vertexes[vi];
        setVertex(v, style, px, py, zIndex, color0.rg(), color0.ba());
        v.lx=(int16_t)lx;
        v.ly=(int16_t)ly;
        v.radius=rad;
        v.border=bor;
        v.cx=color1.rg(); v.cy=color1.ba();
        v.bx=borderColor.rg(); v.by=borderColor.ba();
        v.dx=color3.rg(); v.dy=color3.ba();
    };

    setRounded(vertexIndex++, x0, y0, -hw, -hh, color0Left, color3Left);
    setRounded(vertexIndex++, x1, y0, +hw, -hh, color0Right, color3Right);
    setRounded(vertexIndex++, x0, y1, -hw, +hh, color0Left, color3Left);
    setRounded(vertexIndex++, x1, y1, +hw, +hh, color0Right, color3Right);

    cullOutOfRangeQuad(primitiveId, rect);
    markPrimitiveChanges(primitiveId, before);
}

void SlotRenderer::drawIcon(size_t primitiveId, gweni::Rect rect, int kind, int direction,
    gweni::Color color, int zIndex)
{
    translate(rect);

    size_t vertexIndex=primitiveId*4;

    ensurePrimitiveSlots(primitiveId);

    PrimitiveSlot before;

    readPrimitiveSlot(primitiveId, before);

    m_clippingBoxes[primitiveId]=makeClipBox();

    // the quad is the rect, lx/ly in half-pixel units as in writeRoundedQuad
    const int x0=rect.x;
    const int y0=rect.y;
    const int x1=rect.x+rect.w;
    const int y1=rect.y+rect.h;
    const int hw=halfPixelSize(rect.w);
    const int hh=halfPixelSize(rect.h);
    // whole device px, so every stroke of kinds 3-6 covers whole rows and columns
    const int stroke=std::min(std::max(1, snap(1.0f)), 255);

    auto setIcon=[&](size_t vi, int px, int py, int lx, int ly)
    {
        Vertex &v=m_vertexes[vi];
        setVertex(v, 5, px, py, zIndex, color.rg(), color.ba());
        v.lx=(int16_t)lx;
        v.ly=(int16_t)ly;
        v.radius=(uint16_t)kind;
        v.border=(uint16_t)direction;
        v.bx=(uint16_t)(stroke<<8);    // kinds 2-6 and 8: the stroke in px, the "red" byte
    };

    setIcon(vertexIndex++, x0, y0, -hw, -hh);
    setIcon(vertexIndex++, x1, y0, +hw, -hh);
    setIcon(vertexIndex++, x0, y1, -hw, +hh);
    setIcon(vertexIndex++, x1, y1, +hw, +hh);

    cullOutOfRangeQuad(primitiveId, rect);
    markPrimitiveChanges(primitiveId, before);
}

void SlotRenderer::drawArc(size_t primitiveId, gweni::Rect rect, float startTurns, float sweepTurns,
    float thickness, gweni::Color color, int zIndex)
{
    // a negative sweep is the same arc from its other end
    if(sweepTurns < 0.0f)
    {
        startTurns+=sweepTurns;
        sweepTurns=-sweepTurns;
    }

    const uint16_t start=renderer::arcStartBits(startTurns);
    const uint16_t sweep=renderer::arcSweepBits(sweepTurns);
    // the directions from the centre to the start and the end, clockwise from
    // the top (y down), from the bits, as int16_t in 1/32767: the shader takes
    // no sin, cos or atan, whose precision differs between backends
    const double tau=6.283185307179586;
    const double a0=tau*start/65536.0;
    const double a1=a0+tau*(sweep == 0xffff?1.0:sweep/65535.0);
    auto unit=[](double v) { return (uint16_t)(int16_t)std::lround(v*32767.0); };
    // whole device px, as a border is snapped
    const int stroke=std::min(std::max(1, snap(thickness)), 255);

    translate(rect);

    size_t vertexIndex=primitiveId*4;

    ensurePrimitiveSlots(primitiveId);

    PrimitiveSlot before;

    readPrimitiveSlot(primitiveId, before);

    m_clippingBoxes[primitiveId]=makeClipBox();

    // the quad is the rect, lx/ly in half-pixel units as in drawIcon
    const int x0=rect.x;
    const int y0=rect.y;
    const int x1=rect.x+rect.w;
    const int y1=rect.y+rect.h;
    const int hw=halfPixelSize(rect.w);
    const int hh=halfPixelSize(rect.h);

    auto setArc=[&](size_t vi, int px, int py, int lx, int ly)
    {
        Vertex &v=m_vertexes[vi];
        setVertex(v, 5, px, py, zIndex, color.rg(), color.ba());
        v.lx=(int16_t)lx;
        v.ly=(int16_t)ly;
        v.radius=9;
        v.cx=start;
        v.cy=sweep;
        v.bx=(uint16_t)(stroke<<8);
        v.border=unit(std::sin(a0));
        v.by=unit(-std::cos(a0));
        v.dx=unit(std::sin(a1));
        v.dy=unit(-std::cos(a1));
    };

    setArc(vertexIndex++, x0, y0, -hw, -hh);
    setArc(vertexIndex++, x1, y0, +hw, -hh);
    setArc(vertexIndex++, x0, y1, -hw, +hh);
    setArc(vertexIndex++, x1, y1, +hw, +hh);

    cullOutOfRangeQuad(primitiveId, rect);
    markPrimitiveChanges(primitiveId, before);
}

void SlotRenderer::drawGrid(size_t primitiveId, gweni::Rect rect, gweni::PointF origin, float spacing,
    int majorEvery, gweni::Color minor, gweni::Color major, int zIndex)
{
    // the origin's device px, as drawLine places an endpoint coordinate, before
    // translate() moves the rect into device px
    const gweni::Point &offset=getRenderOffset();
    const RenderTransform &transform=renderTransform();
    const double originX=transform.originX+(double(origin.x)+offset.x)*scale()+transform.fractionX;
    const double originY=transform.originY+(double(origin.y)+offset.y)*scale()+transform.fractionY;

    translate(rect);

    size_t vertexIndex=primitiveId*4;

    ensurePrimitiveSlots(primitiveId);

    PrimitiveSlot before;

    readPrimitiveSlot(primitiveId, before);

    m_clippingBoxes[primitiveId]=makeClipBox();

    const int stroke=std::min(std::max(1, snap(1.0f)), 255);

    majorEvery=std::min(std::max(majorEvery, 1), 255);

    // a phase of 1/16 px in 16 bits holds a period up to 4095 px
    const double maxPitch=4095.0/majorEvery;
    const float pitch=float(std::min(std::max(double(spacing)*scale(), 1.0), maxPitch));
    const double period=double(pitch)*majorEvery;
    // where a major line is from the quad's left (top) edge, within one period
    auto phase=[&](double at, int edge)
    {
        double p=std::fmod(at-edge, period);

        if(p < 0.0)
            p+=period;
        return (uint16_t)std::min(std::lround(p*16.0), 65535L);
    };
    const uint16_t phaseX=phase(originX, rect.x);
    const uint16_t phaseY=phase(originY, rect.y);
    uint32_t pitchBits;

    std::memcpy(&pitchBits, &pitch, sizeof(pitchBits));

    const int x0=rect.x;
    const int y0=rect.y;
    const int x1=rect.x+rect.w;
    const int y1=rect.y+rect.h;
    const int hw=halfPixelSize(rect.w);
    const int hh=halfPixelSize(rect.h);

    auto setGrid=[&](size_t vi, int px, int py, int lx, int ly)
    {
        Vertex &v=m_vertexes[vi];

        setVertex(v, 8, px, py, zIndex, 0.0f, 0.0f);
        v.tx=(uint16_t)(pitchBits >> 16);
        v.ty=(uint16_t)(pitchBits & 0xffff);
        v.lx=(int16_t)lx;
        v.ly=(int16_t)ly;
        v.border=(uint16_t)(majorEvery | (stroke << 8));
        v.cx=minor.rg(); v.cy=minor.ba();
        v.bx=phaseX; v.by=phaseY;
        v.dx=major.rg(); v.dy=major.ba();
    };

    setGrid(vertexIndex++, x0, y0, -hw, -hh);
    setGrid(vertexIndex++, x1, y0, +hw, -hh);
    setGrid(vertexIndex++, x0, y1, -hw, +hh);
    setGrid(vertexIndex++, x1, y1, +hw, +hh);

    cullOutOfRangeQuad(primitiveId, rect);
    markPrimitiveChanges(primitiveId, before);
}

void SlotRenderer::drawTrapezoid(size_t primitiveId, float x0, float x1, float top0, float top1,
    float bottom0, float bottom1, gweni::Color color, int zIndex)
{
    size_t vertexIndex=primitiveId*4;

    ensurePrimitiveSlots(primitiveId);

    PrimitiveSlot before;

    readPrimitiveSlot(primitiveId, before);

    m_clippingBoxes[primitiveId]=makeClipBox();

    if(x1 < x0)
    {
        std::swap(x0, x1);
        std::swap(top0, top1);
        std::swap(bottom0, bottom1);
    }

    const gweni::Point &offset=getRenderOffset();
    const RenderTransform &transform=renderTransform();

    // A side's device px, RenderTransform::deviceX's rounding (translate()'s, so a
    // whole-unit side lands where a rect's edge does), kept in range.
    auto side=[&](float v)
    {
        const float px=std::floor(transform.fractionX+(v+float(offset.x))*transform.scale+0.5f);

        return transform.originX+(int64_t)std::max(std::min(px, 1e9f), -1e9f);
    };
    // An edge's height, unsnapped: the centre a drawLine stroke along it has.
    auto height=[&](float v)
    {
        const double y=transform.originY+(double(v)+offset.y)*transform.scale+transform.fractionY;

        return std::max(std::min(y, 1e9), -1e9);
    };
    const int64_t left=side(x0);
    const int64_t right=side(x1);
    const double t0=height(top0);
    const double t1=height(top1);
    const double b0=height(bottom0);
    const double b1=height(bottom1);

    // The quad: the sides' columns and every row an edge passes through (the
    // edges may cross).
    const int64_t top=(int64_t)std::floor(std::min(std::min(t0, t1), std::min(b0, b1)));
    const int64_t bottom=(int64_t)std::ceil(std::max(std::max(t0, t1), std::max(b0, b1)));
    const int64_t hw=right-left;
    const int64_t hh=bottom-top;

    auto bits=[&](double y)
    {
        const float f=float(y-double(top));
        uint32_t b;

        std::memcpy(&b, &f, sizeof(b));
        return b;
    };
    const uint32_t edge[4]={bits(t0), bits(t1), bits(b0), bits(b1)};

    auto setTrapezoid=[&](size_t vi, int64_t px, int64_t py, int64_t lx, int64_t ly)
    {
        Vertex &v=m_vertexes[vi];

        setVertex(v, 9, (int)px, (int)py, zIndex, color.rg(), color.ba());
        v.lx=(int16_t)lx;
        v.ly=(int16_t)ly;
        v.cx=(uint16_t)(edge[0] >> 16); v.cy=(uint16_t)(edge[0] & 0xffff);
        v.bx=(uint16_t)(edge[1] >> 16); v.by=(uint16_t)(edge[1] & 0xffff);
        v.dx=(uint16_t)(edge[2] >> 16); v.dy=(uint16_t)(edge[2] & 0xffff);
        v.radius=(uint16_t)(edge[3] >> 16); v.border=(uint16_t)(edge[3] & 0xffff);
    };

    setTrapezoid(vertexIndex++, left, top, -hw, -hh);
    setTrapezoid(vertexIndex++, right, top, +hw, -hh);
    setTrapezoid(vertexIndex++, left, bottom, -hw, +hh);
    setTrapezoid(vertexIndex++, right, bottom, +hw, +hh);

    cullOutOfRangeQuad(primitiveId, gweni::Rect((int)left, (int)top, (int)hw, (int)hh));

    // a quad lx/ly cannot hold (32767 half px across) is collapsed as drawLine's is
    const int64_t maxHalf=std::numeric_limits<int16_t>::max();

    if(hw > maxHalf || hh > maxHalf)
    {
        for(size_t i=0; i<4; ++i)
        {
            m_vertexes[primitiveId*4+i].x=0;
            m_vertexes[primitiveId*4+i].y=0;
        }
    }
    markPrimitiveChanges(primitiveId, before);
}

void SlotRenderer::drawLine(size_t primitiveId, gweni::PointF p0, gweni::PointF p1,
    float thickness, int zIndex, const gweni::PointF *prev, const gweni::PointF *next)
{
    size_t vertexIndex=primitiveId*4;

    ensurePrimitiveSlots(primitiveId);

    PrimitiveSlot before;

    readPrimitiveSlot(primitiveId, before);

    m_clippingBoxes[primitiveId]=makeClipBox();

    // Whole device px, like a border, so the stroke can sit on the pixel grid.
    const int stroke=std::min(std::max(1, snap(thickness)), 4096);
    const gweni::Point &offset=getRenderOffset();
    const RenderTransform &transform=renderTransform();

    // The device px where the stroke across an endpoint coordinate starts: its
    // centre at v*scale (from the space's origin), moved to put both its edges on
    // pixel boundaries. For an even stroke that is snap(v)-stroke/2, the edge
    // translate() snaps v to; for an odd one, the pixel v*scale falls in is its
    // middle row (column).
    auto strokeStart=[&](float v, int offset, int origin, float fraction)
    {
        const double centre=(double(v)+offset)*scale()+fraction;
        const double start=origin+std::floor(centre-0.5*stroke+0.5);

        return (int64_t)std::max(std::min(start, 1e9), -1e9);
    };
    auto strokeStartX=[&](float v)
    {
        return strokeStart(v, offset.x, transform.originX, transform.fractionX);
    };
    auto strokeStartY=[&](float v)
    {
        return strokeStart(v, offset.y, transform.originY, transform.fractionY);
    };
    const int64_t ax=strokeStartX(p0.x);
    const int64_t ay=strokeStartY(p0.y);
    const int64_t bx=strokeStartX(p1.x);
    const int64_t by=strokeStartY(p1.y);

    // The quad: both endpoints' stroke squares, which hold the round caps, and 1 px
    // around them for the anti-aliased edge of a slanted stroke.
    const int64_t x0=std::min(ax, bx)-1;
    const int64_t y0=std::min(ay, by)-1;
    const int64_t x1=std::max(ax, bx)+stroke+1;
    const int64_t y1=std::max(ay, by)+stroke+1;

    // lx/ly and the endpoints in half-pixel units from the quad's centre, as in
    // writeRoundedQuad: an endpoint (start+stroke/2) is a whole number of them.
    const int64_t hw=x1-x0;
    const int64_t hh=y1-y0;
    const int64_t centreX=x0+x1;
    const int64_t centreY=y0+y1;
    const uint16_t a0=(uint16_t)(int16_t)(2*ax+stroke-centreX);
    const uint16_t a1=(uint16_t)(int16_t)(2*ay+stroke-centreY);
    const uint16_t b0=(uint16_t)(int16_t)(2*bx+stroke-centreX);
    const uint16_t b1=(uint16_t)(int16_t)(2*by+stroke-centreY);

    // A neighbour point is placed as an endpoint is, at this segment's stroke, so it
    // is where the neighbour's own slot has that endpoint, in this quad's half px.
    // One that int16_t cannot hold (16383.5 px from the centre) is left out.
    uint16_t joints=0;
    uint16_t neighbour[2][2]={{0, 0}, {b0, b1}};

    for(int i=0; i<2; ++i)
    {
        const gweni::PointF *point=(i == 0) ? prev : next;

        if(!point)
            continue;

        const int64_t px=2*strokeStartX(point->x)+stroke-centreX;
        const int64_t py=2*strokeStartY(point->y)+stroke-centreY;
        const int64_t limit=std::numeric_limits<int16_t>::max();

        if(std::abs(px) > limit || std::abs(py) > limit)
            continue;
        neighbour[i][0]=(uint16_t)(int16_t)px;
        neighbour[i][1]=(uint16_t)(int16_t)py;
        joints=(uint16_t)(joints | ((i == 0) ? LineHasPrev : LineHasNext));
    }

    auto setLine=[&](size_t vi, int64_t px, int64_t py, int64_t lx, int64_t ly)
    {
        Vertex &v=m_vertexes[vi];

        setVertex(v, 3, (int)px, (int)py, zIndex, m_color.rg(), m_color.ba());
        v.lx=(int16_t)lx;
        v.ly=(int16_t)ly;
        v.radius=joints;
        v.border=(uint16_t)stroke;
        v.cx=a0; v.cy=a1;
        v.bx=neighbour[0][0]; v.by=neighbour[0][1];
        v.dx=neighbour[1][0]; v.dy=neighbour[1][1];
    };

    setLine(vertexIndex++, x0, y0, -hw, -hh);
    setLine(vertexIndex++, x1, y0, +hw, -hh);
    setLine(vertexIndex++, x0, y1, -hw, +hh);
    setLine(vertexIndex++, x1, y1, +hw, +hh);

    cullOutOfRangeQuad(primitiveId, gweni::Rect((int)x0, (int)y0, (int)hw, (int)hh));

    // A quad inside the vertex range can still be wider or higher than lx/ly hold
    // (32767 half px); it is collapsed the same way.
    const int64_t maxHalf=std::numeric_limits<int16_t>::max();

    if(hw > maxHalf || hh > maxHalf)
    {
        for(size_t i=0; i<4; ++i)
        {
            m_vertexes[primitiveId*4+i].x=0;
            m_vertexes[primitiveId*4+i].y=0;
        }
    }
    markPrimitiveChanges(primitiveId, before);
}

void SlotRenderer::setDrawColor(gweni::Color color)
{
    m_color=color;
}

// Every primitive and glyph slot is stamped with the clipRegion() current when it is
// drawn (makeClipBox), and the shaders discard outside it, so there is no clipping
// state to switch on and off.
void SlotRenderer::startClip()
{}

void SlotRenderer::endClip()
{}

SlotRenderer::Box SlotRenderer::makeClipBox() const
{
    const gweni::Rect &clip=clipRegion();
    const RenderTransform &transform=renderTransform();

    // translated as translate() translates a rect's edges, so a box edge lands where
    // a quad drawn over the same rect has its edge
    long x0=transform.deviceX(float(clip.x));
    long y0=transform.deviceY(float(clip.y));
    long x1=transform.deviceX(float(clip.x+clip.w));
    long y1=transform.deviceY(float(clip.y+clip.h));

    // inside a space of its own (a ZoomPanel), cut to the region it was entered from
    if(transform.bounded)
    {
        const gweni::Rect &outer=transform.deviceClip;

        x0=std::max(x0, (long)outer.x);
        y0=std::max(y0, (long)outer.y);
        x1=std::min(x1, (long)outer.x+outer.w);
        y1=std::min(y1, (long)outer.y+outer.h);
    }

    x0+=m_clipDebugInset;
    y0+=m_clipDebugInset;
    x1-=m_clipDebugInset;
    y1-=m_clipDebugInset;

    // a corner left or above the view must not wrap in uint16_t, and an empty (or
    // inverted) region becomes an empty box
    const long limit=std::numeric_limits<uint16_t>::max();

    x0=std::min(std::max(x0, 0L), limit);
    y0=std::min(std::max(y0, 0L), limit);
    x1=std::min(std::max(x1, x0), limit);
    y1=std::min(std::max(y1, y0), limit);

    Box box;

    box.x0=(uint16_t)x0;
    box.y0=(uint16_t)y0;
    box.x1=(uint16_t)x1;
    box.y1=(uint16_t)y1;
    return box;
}

void SlotRenderer::generatePrimitive(size_t &primitiveId)
{
    primitiveId=getPrimitiveId(nullptr);

    //resize indexes/vertexes to cover the primitive
    ensurePrimitiveSlots(primitiveId);
}

void SlotRenderer::releasePrimitive(size_t primitiveId)
{
    // A released slot's vertexes go back to what a never-written slot holds, all
    // zero bytes, not only a hidden style: the id bank hands the lowest free id
    // out first, so a control made after another is deleted gets its ids, and a
    // skin that hides a slot and shows it again later (showPrimitive) would
    // otherwise start from the dead control's bytes (issue #479). Nothing shows a
    // released slot again. The clip box is left: every draw writes it, and a slot
    // whose vertexes are 0 draws nothing whatever its box. So the release uploads
    // the slot's vertexes alone, as the hide of a shown slot did; a hidden slot
    // still holding its bytes is uploaded too, one already all 0 is not.
    if(m_vertexes.size() >= primitiveId*4+4)
    {
        Vertex *vertexes=&m_vertexes[primitiveId*4];
        const Vertex never[4]={};

        if(memcmp(vertexes, never, sizeof(never)) != 0)
        {
            std::fill_n(vertexes, 4, Vertex());
            m_dirtyVertexSlots.mark(primitiveId);
        }
    }
    releasePrimitiveId(primitiveId);
}

void SlotRenderer::drawTexturedRect(const gweni::Texture &texture, size_t primitiveId, gweni::Rect rect, int zIndex,
    float u1, float v1, float u2, float v2)
{
    if(!ensureTexture(texture))
        return drawMissingImage(primitiveId, rect, zIndex);

    const SlotTextureData &texData=*m_textureData[m_lastTexture];

    translate(rect);

    size_t vertexIndex=primitiveId*4;

    ensurePrimitiveSlots(primitiveId);

    PrimitiveSlot before;

    readPrimitiveSlot(primitiveId, before);

    m_clippingBoxes[primitiveId]=makeClipBox();

#ifdef DEBUG_RENDER
    std::cout<<"Rendering: "<<primitiveId<<"("<<rect.x<<", "<<rect.y<<", "<<rect.w<<", "<<rect.h<<" : "<<zIndex<<")\n";
#endif//DEBUG_RENDER

    // UVs as 0..65535 for 0..1, the tint (the draw colour), and the image's
    // page (radius), layer (border) and size (bx, by: the shader clamps each
    // level's taps to the image's part of the layer)
    auto uv=[](float t)
    {
        return (float)std::lround(std::min(std::max(t, 0.0f), 1.0f)*65535.0f);
    };
    auto setImageVertex=[&](size_t vi, int x, int y, float u, float v)
    {
        Vertex &vertex=m_vertexes[vi];

        setVertex(vertex, 2, x, y, zIndex, uv(u), uv(v));
        vertex.radius=(uint16_t)texData.m_imageClass;
        vertex.border=(uint16_t)texData.m_layer;
        vertex.cx=m_color.rg();
        vertex.cy=m_color.ba();
        vertex.bx=(uint16_t)texData.width;
        vertex.by=(uint16_t)texData.height;
    };

    setImageVertex(vertexIndex++, rect.x, rect.y, u1, v1);
    setImageVertex(vertexIndex++, rect.x + rect.w, rect.y, u2, v1);
    setImageVertex(vertexIndex++, rect.x, rect.y + rect.h, u1, v2);
    setImageVertex(vertexIndex++, rect.x + rect.w, rect.y + rect.h, u2, v2);

    cullOutOfRangeQuad(primitiveId, rect);
    markPrimitiveChanges(primitiveId, before);

//    addVert(rect.x, rect.y, u1, v1);
//    addVert(rect.x + rect.w, rect.y, u2, v1);
//    addVert(rect.x, rect.y + rect.h, u1, v2);
//    addVert(rect.x + rect.w, rect.y, u2, v1);
//    addVert(rect.x + rect.w, rect.y + rect.h, u2, v2);
//    addVert(rect.x, rect.y + rect.h, u1, v2);
}

void SlotRenderer::showPrimitive(int style, size_t primitiveId)
{
//    if(m_clippingBoxes.size() < primitiveId+1)
//        m_clippingBoxes.resize(primitiveId+1);
//
//    Box &box=m_clippingBoxes[primitiveId];
//
//    box.state=style;
    
    size_t vertexIndex=primitiveId*4;

    if(m_vertexes.size() < vertexIndex+4)
        return;

//    std::cout<<"Show: "<<primitiveId<<"\n";
    for(size_t i=0; i<4; ++i)
    {
        if(m_vertexes[vertexIndex+i].style == style)
            continue;

        m_vertexes[vertexIndex+i].style=style;
        m_dirtyVertexSlots.mark(primitiveId);
    }
}

void SlotRenderer::hidePrimitive(size_t primitiveId)
{
//    if(m_clippingBoxes.size() < primitiveId+1)
//        m_clippingBoxes.resize(primitiveId+1);
//
//    Box &box=m_clippingBoxes[primitiveId];
//
//    box.state=0;
    
    size_t vertexIndex=primitiveId*4;

    if(m_vertexes.size() < vertexIndex+4)
        return;

//    std::cout<<"Hide: "<<primitiveId<<"\n";
    for(size_t i=0; i<4; ++i)
    {
        if(m_vertexes[vertexIndex+i].style == 0)
            continue;

        m_vertexes[vertexIndex+i].style=0;
        m_dirtyVertexSlots.mark(primitiveId);
    }
}

gweni::Color SlotRenderer::pixelColor(const gweni::Texture &texture, unsigned int x, unsigned int y,
    const gweni::Color &col_default)
{
    if(!ensureTexture(texture))
        return col_default;

    SlotTextureData &texData=*m_textureData[m_lastTexture];

    static const unsigned int iPixelSize=sizeof(unsigned char) * 4;

    if(texData.readable)
    {
        unsigned char *pPixel=texData.m_readData.data() + (x + (y * static_cast<unsigned int>(texData.width))) * iPixelSize;
        return gweni::Color(pPixel[0], pPixel[1], pPixel[2], pPixel[3]);
    }

    // read back once from the device, then kept
    if(!readImageLayer(texData.m_imageClass, texData.m_layer, (int)texData.width, (int)texData.height,
        texData.m_readData))
        return col_default;
    texData.readable=true;

    unsigned char *pPixel=texData.m_readData.data() + (x + (y * static_cast<unsigned int>(texData.width))) * iPixelSize;
    return gweni::Color(pPixel[0], pPixel[1], pPixel[2], pPixel[3]);
}

void SlotRenderer::ensureTextSlots(size_t glyphSlots)
{
    if(m_textVertexes.size() < glyphSlots*4)
        m_textVertexes.resize(glyphSlots*4);
    if(m_textClipBoxes.size() < glyphSlots)
        m_textClipBoxes.resize(glyphSlots);
    if(m_textSlotOwner.size() < glyphSlots)
        m_textSlotOwner.resize(glyphSlots, kNoTextOwner);
}

void SlotRenderer::addFreeTextRun(size_t start, size_t capacity)
{
    if(capacity == 0)
        return;

    size_t end=start+capacity;

    //merge with the free block that ends where this one starts
    auto next=m_freeTextRuns.lower_bound(start);

    if(next != m_freeTextRuns.begin())
    {
        auto previous=std::prev(next);

        if(previous->first+previous->second == start)
        {
            start=previous->first;
            m_freeTextRuns.erase(previous);
        }
    }

    //and with the one that starts where this one ends
    if((next != m_freeTextRuns.end()) && (next->first == end))
    {
        end+=next->second;
        m_freeTextRuns.erase(next);
    }

    // A block reaching the end of the in-use slots goes back to the bump
    // allocator rather than the list, so the last run can grow in place.
    if(end == m_textSlotsUsed)
        m_textSlotsUsed=start;
    else
        m_freeTextRuns.emplace(start, end-start);
}

void SlotRenderer::freeTextRun(TextRun &run)
{
    if(run.capacity == 0)
        return;

    // Out of its font's draw list before handing it back: a freed run must draw
    // nothing until somebody allocates its slots again.
    const std::pair<size_t, size_t> drawn=drawnRange(run);

    run.glyphs=0;
    markTextDrawList(run, drawn);
    addFreeTextRun(run.start, run.capacity);

    run.start=0;
    run.capacity=0;
}

void SlotRenderer::allocateTextRun(size_t textId, TextRun &run, size_t glyphs)
{
    if(run.capacity >= glyphs)
    {
        // Keeps the run where it is; the tail past `glyphs` is not in the draw
        // list (markTextDrawList), so a string that shrinks leaves no ghost glyphs.
        run.glyphs=glyphs;
        return;
    }

    // A run that outgrew its slots (a line typed into, a counter gaining a digit)
    // gets a quarter as many again, in whole groups of kTextRunSlackGranule, so
    // the next characters fit where it lands: it moves O(log n) times growing to
    // n glyphs, rather than once per glyph, each move leaving a hole and, at the
    // end of the slots, growing the mirror. A first layout gets exactly its
    // glyphs: most strings never change length.
    const bool grows=(run.capacity > 0);
    const size_t capacity=grows?
        (glyphs+glyphs/4+kTextRunSlackGranule-1)/kTextRunSlackGranule*kTextRunSlackGranule:glyphs;

    freeTextRun(run);

    // Best fit: the smallest free block that holds the string, split if it is
    // larger than the capacity (the slack kept). Blocks are merged as they are
    // freed, so the list stays short and a linear scan is enough.
    auto it=m_freeTextRuns.end();

    for(auto block=m_freeTextRuns.begin(); block != m_freeTextRuns.end(); ++block)
    {
        if((block->second >= glyphs) &&
            ((it == m_freeTextRuns.end()) || (block->second < it->second)))
            it=block;
    }

    if(it != m_freeTextRuns.end())
    {
        run.start=it->first;
        run.capacity=it->second;
        m_freeTextRuns.erase(it);

        if(run.capacity > capacity)
        {
            addFreeTextRun(run.start+capacity, run.capacity-capacity);
            run.capacity=capacity;
        }
    }
    else
    {
        run.start=m_textSlotsUsed;
        run.capacity=capacity;
        m_textSlotsUsed+=capacity;
    }

    run.glyphs=glyphs;
    ensureTextSlots(run.start+run.capacity);
    std::fill(m_textSlotOwner.begin()+run.start, m_textSlotOwner.begin()+run.start+run.capacity, textId);
}

std::pair<size_t, size_t> SlotRenderer::drawnRange(const TextRun &run)
{
    if(run.hidden || (run.glyphs == 0))
        return {0, 0};
    return {run.start, run.glyphs};
}

void SlotRenderer::markTextDrawList(const TextRun &run, std::pair<size_t, size_t> before)
{
    // A string relaid in the slots and glyph count it had (a Label given a new
    // string of the same length) leaves the list as it is: the rebuild would give
    // the same chunks, at O(runs in the font) and a sort.
    if(run.fontData && (drawnRange(run) != before))
        run.fontData->drawListStale=true;
}

void SlotRenderer::markTextVertexes(const TextRun &run)
{
    for(size_t i=0; i<run.glyphs; ++i)
        m_dirtyTextVertexSlots.mark(run.start+i);
}

void SlotRenderer::writeTextClipBoxes(const TextRun &run, const Box &box)
{
    for(size_t i=0; i<run.glyphs; ++i)
    {
        Box &slotBox=m_textClipBoxes[run.start+i];

        if(memcmp(&slotBox, &box, sizeof(Box)) == 0)
            continue;

        slotBox=box;
        m_dirtyTextClipSlots.mark(run.start+i);
    }
}

void SlotRenderer::setTextRunFont(size_t textId, FontData *fontData)
{
    TextRun &run=m_textRuns[textId];

    if(run.fontData == fontData)
        return;

    FontData *previous=run.fontData;

    if(previous)
    {
        previous->textRuns.erase(textId);
        previous->drawListStale=true;
    }

    run.fontData=fontData;

    // The last run has left a font no loadFont holds. One freeFont kept for it
    // goes now; one only a use loaded (the whole px sizes, and the SDF atlas, a
    // zoom or a canvas scale passes through) waits out kIdleFontFlushes, so they
    // do not pile up and one used again soon is not rasterised again.
    if(previous && (previous->owners == 0) && previous->textRuns.empty())
    {
        if(previous->freed)
        {
            for(auto it=m_fonts.begin(); it != m_fonts.end(); ++it)
            {
                if(&it->second != previous)
                    continue;
                if(m_lastFont == &(*it))
                    m_lastFont=nullptr;
                eraseFont(it);
                break;
            }
        }
        else
        {
            previous->idleSince=m_flushes+1;
            ++m_idleFonts;
        }
    }

    if(fontData)
    {
        fontData->textRuns.insert(textId);
        fontData->drawListStale=true;
        fontData->idleSince=0;
    }
}

void SlotRenderer::buildTextDrawList(FontData &fontData)
{
    const size_t used=fillTextDrawList(fontData, fontData.drawChunks);

    // the chunks the list no longer reaches: flush() draws every entry
    fontData.drawChunks.erase(fontData.drawChunks.begin()+used, fontData.drawChunks.end());
    fontData.drawListStale=false;
}

size_t SlotRenderer::fillTextDrawList(const FontData &fontData, std::vector<ChunkDraw> &drawChunks)
{
    // (first glyph slot, glyph count) of each run that draws something, in a
    // vector kept for its capacity
    std::vector<std::pair<size_t, size_t>> &ranges=m_textDrawRanges;

    ranges.clear();
    ranges.reserve(fontData.textRuns.size());
    for(size_t textId:fontData.textRuns)
    {
        const std::pair<size_t, size_t> range=drawnRange(m_textRuns[textId]);

        if(range.second > 0)
            ranges.push_back(range);
    }

    // in slot order, as the single draw over the whole buffer went
    std::sort(ranges.begin(), ranges.end());

    // The entries already there are refilled in order, their vectors cleared but
    // not freed, so a rebuild into as many chunks as last time allocates nothing.
    size_t used=0;

    // Only glyphs in use are listed, and a merge joins ranges that touch, so a
    // run's tail, a freed block or another font's run between two runs keeps them
    // apart: nothing outside the listed runs' glyphs is drawn.
    size_t i=0;

    while(i < ranges.size())
    {
        size_t start=ranges[i].first;
        size_t end=start+ranges[i].second;

        for(++i; (i < ranges.size()) && (ranges[i].first == end); ++i)
            end+=ranges[i].second;

        // split at the chunks' edges, each piece indexed from its chunk's first slot
        while(start < end)
        {
            const size_t chunk=start/m_chunkSlots;
            const size_t chunkStart=chunk*m_chunkSlots;
            const size_t pieceEnd=std::min(end, chunkStart+m_chunkSlots);

            if((used == 0) || (drawChunks[used-1].chunk != chunk))
            {
                if(used == drawChunks.size())
                    drawChunks.emplace_back();

                ChunkDraw &next=drawChunks[used++];

                next.chunk=chunk;
                next.counts.clear();
                next.offsets.clear();
            }

            ChunkDraw &draw=drawChunks[used-1];

            draw.counts.push_back(static_cast<int>((pieceEnd-start)*6));
            draw.offsets.push_back((start-chunkStart)*6*sizeof(uint16_t));
            start=pieceEnd;
        }
    }
    return used;
}

void SlotRenderer::verifyTextDrawList(const FontData &fontData)
{
#ifndef NDEBUG
    // the scratch is every font's, so it keeps the entries past this font's
    // list, with their vectors, for the next font with more chunks
    const size_t used=fillTextDrawList(fontData, m_verifyDrawChunks);

    bool same=(used == fontData.drawChunks.size());

    for(size_t i=0; same && (i < used); ++i)
    {
        const ChunkDraw &rebuilt=m_verifyDrawChunks[i];
        const ChunkDraw &kept=fontData.drawChunks[i];

        same=(rebuilt.chunk == kept.chunk) && (rebuilt.counts == kept.counts) &&
            (rebuilt.offsets == kept.offsets);
    }
    // a run's start, glyph count, hidden flag or font changed without markTextDrawList
    assert(same && "a text run changed its font's draw list without marking it stale");
#else
    (void)fontData;
#endif
}

void SlotRenderer::generateTextPrimitive(size_t &primitiveId)
{
    primitiveId=getTextId(nullptr);

    if(m_textRuns.size() < primitiveId+1)
        m_textRuns.resize(primitiveId+1);
}

void SlotRenderer::releaseTextPrimitive(size_t primitiveId)
{
    if(primitiveId < m_textRuns.size())
    {
        TextRun &run=m_textRuns[primitiveId];

        // The run goes back on the free list and the bookkeeping is reset, so a
        // reused text id cannot take renderText's "string hasn't changed" path
        // and inherit the dead control's glyphs.
        freeTextRun(run);
        setTextRunFont(primitiveId, nullptr);
        if(run.hasSpans)
            m_textRunSpans.erase(primitiveId);
        run=TextRun();
    }

    releaseTextId(primitiveId);
}

void SlotRenderer::renderText(size_t textId, const gweni::Font &font, gweni::Point pos, int zIndex,
    const gweni::String &text)
{
    renderTextRun(textId, font, pos, zIndex, text, nullptr);
}

void SlotRenderer::renderText(size_t textId, const gweni::Font &font, gweni::Point pos, int zIndex,
    const gweni::String &text, const std::vector<ColorSpan> &spans)
{
    renderTextRun(textId, font, pos, zIndex, text, spans.empty()?nullptr:&spans);
}

void SlotRenderer::glyphByteOffsets(FontData &fontData, const gweni::String &text,
    std::vector<uint32_t> &bytes)
{
    const bool sdf=(fontData.mode == TextMode::Sdf);

    bytes.clear();
    for(size_t i=0; i < text.size(); i+=std::max<size_t>(1, ftgl::utf8_surrogate_len(text.c_str()+i)))
    {
        const char *current=text.c_str()+i;

        // utf8_strlen, which gives text_buffer_add_text its length, ends at a NUL
        if(!sdf && (*current == '\0'))
            break;
        if(*current == '\n')
            continue;
        if(fontData.glyph(current) != nullptr)
            bytes.push_back(static_cast<uint32_t>(i));
    }
}

void SlotRenderer::colorTextRun(TextRun &run, const TextRunSpans *spans, bool markChanged)
{
    // the spans' bytes and the glyphs' both ascend, so one pass finds every glyph's
    size_t span=0;

    for(size_t i=0; i < run.glyphs; ++i)
    {
        const gweni::Color *color=&run.color;

        if(spans)
        {
            const std::vector<ColorSpan> &ranges=spans->spans;
            // a walk that did not step as the layout did (it should) colours by glyph 0
            const uint32_t byte=(i < spans->glyphBytes.size())?spans->glyphBytes[i]:0;

            while((span < ranges.size()) && (ranges[span].end <= byte))
                ++span;
            if((span < ranges.size()) && (ranges[span].begin <= byte))
                color=&ranges[span].color;
        }

        // the arithmetic the markup's foreground colour is made with, so a glyph
        // in the draw colour has the bytes the plain call gives it; opaque
        const float r=color->r/255.0f, g=color->g/255.0f, b=color->b/255.0f, a=1.0f;
        const size_t vertIndex=(run.start+i)*4;
        const ftgl::glyph_vertex_t &first=m_textVertexes[vertIndex];

        if((first.r == r) && (first.g == g) && (first.b == b) && (first.a == a))
            continue;

        for(size_t j=0; j < 4; ++j)
        {
            ftgl::glyph_vertex_t &vertex=m_textVertexes[vertIndex+j];

            vertex.r=r;
            vertex.g=g;
            vertex.b=b;
            vertex.a=a;
        }
        if(markChanged)
            m_dirtyTextVertexSlots.mark(run.start+i);
    }
}

void SlotRenderer::renderTextRun(size_t textId, const gweni::Font &font, gweni::Point pos, int zIndex,
    const gweni::String &text, const std::vector<ColorSpan> *spans)
{
    // drawn at the effective size, in device pixels from the translated origin
    const float px=textPx(font);

    if(!ensureFont(font, px))
        return;

    if(m_textRuns.size() < textId+1)
        m_textRuns.resize(textId+1);

    TextRun &run=m_textRuns[textId];

    translate(pos.x, pos.y);

    // The colour the rebuild below bakes into the glyphs. Its alpha is always
    // opaque, so the draw colour's alpha takes no part: a change in alpha alone
    // would rebuild the run into the same bytes.
    const gweni::Color color(m_color.r, m_color.g, m_color.b, 255);

    FontData &fontData=m_lastFont->second;

    // Same string, font, effective size and colour, laid out in this font's atlas
    // (not one freed since): the glyphs are the ones already there, at most offset.
    // Anything else falls through to the full rebuild; a new scale() is a new px, and
    // in an SDF font the same atlas.
    // With spans on either side (the call's or the run's), a new colour or new spans
    // keep the glyphs too: only their colours are written, and only where they
    // changed. The plain call on a run without spans rebuilds for a new colour.
    const bool withSpans=(spans != nullptr) || run.hasSpans;

    if((run.text == text) && (run.font == font) && (run.px == px) && ((run.color == color) || withSpans)
        && (run.fontData == &fontData))
    {
        if(withSpans)
        {
            TextRunSpans *kept=nullptr;
            bool changed=!(run.color == color);

            if(spans)
            {
                kept=&m_textRunSpans[textId];
                if(!run.hasSpans)
                    glyphByteOffsets(fontData, text, kept->glyphBytes);
                if(!run.hasSpans || (kept->spans != *spans))
                {
                    kept->spans=*spans;
                    changed=true;
                }
                run.hasSpans=true;
            }
            else
            {
                m_textRunSpans.erase(textId);
                run.hasSpans=false;
                changed=true;
            }
            run.color=color;
            if(changed)
                colorTextRun(run, kept, true);
        }

        if((pos.x != run.position.x) || (pos.y != run.position.y) || (zIndex != run.zIndex))
        {
            const gweni::Point delta=pos-run.position;
            // the same offsets as the rebuild below gives
            const float zDelta=1.0f/(run.glyphs+1);
            float zOffset=zDelta;

#ifdef DEBUG_RENDER
            std::cout<<"Moving text run: ("<<pos.x<<", "<<pos.y<<" : "<<zIndex<<") "<<text<<"\n";
#endif//DEBUG_RENDER

            for(size_t i=0; i<run.glyphs; ++i)
            {
                const size_t vertIndex=(run.start+i)*4;
                const float z=zIndex+zOffset;

                zOffset+=zDelta;

                for(size_t j=0; j<4; ++j)
                {
                    m_textVertexes[vertIndex+j].x+=delta.x;
                    m_textVertexes[vertIndex+j].y+=delta.y;
                    m_textVertexes[vertIndex+j].z=z;
                }
            }

            markTextVertexes(run);
            run.position=pos;
            run.zIndex=zIndex;
        }

        // the region can change with or without a move; an unchanged box marks nothing
        writeTextClipBoxes(run, makeClipBox());
        return;
    }

    ftgl::text_buffer_t *textBuffer=nullptr;
    std::vector<ftgl::glyph_vertex_t> sdfVertexes;
    const ftgl::glyph_vertex_t *vertexBuffer;
    size_t glyphCount;

    if(fontData.mode == TextMode::Sdf)
    {
        layoutSdfText(fontData, px, text, color, &sdfVertexes);
        vertexBuffer=sdfVertexes.data();
        glyphCount=sdfVertexes.size()/4;
    }
    else
    {
        textBuffer=ftgl::text_buffer_new();
        ftgl::vec2 pen={ 0.0f, 0.0f };

        // Bake the current draw color (set by the control's text renderer from its
        // text color) into the glyphs, so per-control text colors actually apply
        // instead of the static black the markup was initialised with.
        fontData.markup.foreground_color.r=color.r/255.0f;
        fontData.markup.foreground_color.g=color.g/255.0f;
        fontData.markup.foreground_color.b=color.b/255.0f;
        fontData.markup.foreground_color.a=color.a/255.0f;

        //the length is in codepoints, not bytes: text.length() overran the string for
        //anything non-ASCII and drew whatever followed it in memory as glyphs
        ftgl::text_buffer_add_text(textBuffer, &pen, &fontData.markup, text.c_str(), ftgl::utf8_strlen(text.c_str()));

        //with every markup decoration off, freetype-gl emits exactly one quad per glyph
        glyphCount=textBuffer->buffer->indices->size/6;
        vertexBuffer=(const ftgl::glyph_vertex_t *)textBuffer->buffer->vertices->items;
    }

    std::vector<float> shifts, lineShifts;

    measuredPenShifts(font, fontData, px, text, shifts, lineShifts);

    // what the run drew before, in the font it was laid out in (a new font is
    // setTextRunFont's to mark, in both fonts)
    const std::pair<size_t, size_t> drawnBefore=drawnRange(run);

    // hidden is left as it is: a hidden run stays out of its font's draw list
    run.text=text;
    run.position=pos;
    run.zIndex=zIndex;
    run.font=font;
    run.px=px;
    run.color=color;

    TextRunSpans *kept=nullptr;

    if(spans)
    {
        kept=&m_textRunSpans[textId];
        kept->spans=*spans;
        glyphByteOffsets(fontData, text, kept->glyphBytes);
        run.hasSpans=true;
    }
    else if(run.hasSpans)
    {
        m_textRunSpans.erase(textId);
        run.hasSpans=false;
    }
    setTextRunFont(textId, &fontData);
    allocateTextRun(textId, run, glyphCount);

    const float zDelta=1.0f/(glyphCount+1);
    float zOffset=zDelta;

#ifdef DEBUG_RENDER
    std::cout<<"Regenerating text run: ("<<pos.x<<", "<<pos.y<<" : "<<zIndex<<") "<<text<<"\n";
#endif//DEBUG_RENDER

    for(size_t i=0; i<glyphCount; ++i)
    {
        const size_t vertIndex=(run.start+i)*4;
        const float shift=shifts.empty()?0.0f:shifts[i];
        const float lineShift=lineShifts.empty()?0.0f:lineShifts[i];
        // Each glyph of a run sits a fraction of a z unit in front of the one
        // before it, so the LESS depth test cannot reject the overlap between
        // two neighbouring glyph boxes. Glyph i is at zIndex+(i+1)/(glyphs+1),
        // strictly between zIndex and zIndex+1: a primitive drawn at the run's
        // own z (a TextBox's selection) is behind every glyph, not tied with the
        // first. Accumulated rather than (i+1)*zDelta, which rounds differently
        // and would move a glyph by an ulp of depth.
        const float z=zIndex+zOffset;

        zOffset+=zDelta;

        for(size_t j=0; j<4; ++j)
        {
            const ftgl::glyph_vertex_t &source=vertexBuffer[i*4+j];
            ftgl::glyph_vertex_t &vertex=m_textVertexes[vertIndex+j];
            // The corner's offset in the run, rounded to 1/256 device px before the
            // run's whole-pixel position is added. An SDF quad sits at pen+offset
            // times px/48, which a float holds only rounded, and the move above adds
            // its delta to these vertexes: with a dyadic offset every such sum is
            // exact (while |x| < 65536), so a run moved any number of times is
            // exactly a fresh layout where it ends. Without it a drag there and
            // back left the corners some ulps off and changed SDF edge pixels.
            const float offsetX=std::round((source.x+shift)*256.0f)/256.0f;
            const float offsetY=std::round(source.y*256.0f)/256.0f;

            vertex=source;
            vertex.x=pos.x+offsetX;
            vertex.y=pos.y-offsetY+lineShift;
            vertex.z=z;
        }
    }

    // the layout baked the draw colour into every glyph; the spans' glyphs take theirs
    if(kept)
        colorTextRun(run, kept, false);

    markTextDrawList(run, drawnBefore);
    markTextVertexes(run);
    writeTextClipBoxes(run, makeClipBox());

    if(textDebugMatches(text))
    {
        size_t freeSlots=0, liveGlyphs=0;

        for(const auto &block:m_freeTextRuns)
            freeSlots+=block.second;
        for(const TextRun &textRun:m_textRuns)
            liveGlyphs+=textRun.glyphs;

        ::printf("[mirror] textId=%zu \"%s\" chars=%zu run=[%zu,%zu) capacity=%zu "
            "%s vertices=%zu\n",
            textId, text.c_str(), text.length(), run.start, run.start+run.glyphs,
            run.capacity, textBuffer?"ftgl":"sdf", glyphCount*4);
        ::printf("[mirror]   pos=(%d,%d) z=%d hidden=%s atlas=%zux%zu depth=%zu "
            "mirror verts=%zu slots used=%zu free runs=%zu free slots=%zu "
            "live glyphs=%zu chunk slots=%zu\n",
            pos.x, pos.y, zIndex, run.hidden?"yes":"no",
            fontData.atlas->width, fontData.atlas->height, fontData.atlas->depth,
            m_textVertexes.size(), m_textSlotsUsed,
            m_freeTextRuns.size(), freeSlots, liveGlyphs, m_chunkSlots);
        for(size_t i=0; i<run.capacity; ++i)
        {
            const size_t slot=run.start+i;
            const size_t vertIndex=slot*4;
            float minX=m_textVertexes[vertIndex].x, maxX=minX;
            float minY=m_textVertexes[vertIndex].y, maxY=minY;

            // what buildTextDrawList lists for the run: its glyphs in use, unless hidden
            const bool drawn=!run.hidden && (i < run.glyphs);

            ::printf("[mirror]   slot %zu (glyph %zu, char '%c')%s verts %zu..%zu chunk %zu %s\n",
                slot, i, i<text.length()?text[i]:'?', i<run.glyphs?"":" [unused tail]",
                vertIndex, vertIndex+3, slot/m_chunkSlots, drawn?"drawn":"not drawn");
            for(size_t j=0; j<4; ++j)
            {
                const ftgl::glyph_vertex_t &v=m_textVertexes[vertIndex+j];
                minX=std::min(minX, v.x); maxX=std::max(maxX, v.x);
                minY=std::min(minY, v.y); maxY=std::max(maxY, v.y);
                ::printf("[mirror]     v%zu x=%9.3f y=%9.3f z=%9.3f u=%8.5f v=%8.5f "
                    "rgba=%.2f,%.2f,%.2f,%.2f gamma=%.2f\n",
                    j, (double)v.x, (double)v.y, (double)v.z, (double)v.u, (double)v.v,
                    (double)v.r, (double)v.g, (double)v.b, (double)v.a, (double)v.gamma);
            }
            ::printf("[mirror]     box w=%.3f h=%.3f  x=[%.3f..%.3f] y=[%.3f..%.3f]\n",
                (double)(maxX-minX), (double)(maxY-minY),
                (double)minX, (double)maxX, (double)minY, (double)maxY);
        }
    }

    if(textBuffer)
        text_buffer_delete(textBuffer);
}

void SlotRenderer::textPens(FontData &fontData, float px, const gweni::String &text,
    std::vector<float> &pens, std::vector<int> *lines)
{
    pens.clear();
    if(lines)
        lines->clear();
    walkPens(fontData, px, text, &pens, nullptr, lines);
}

// The walk of layoutSdfText, or of freetype-gl's text_buffer_add_text with only the
// pen kept: text_buffer_add_char adds the kerning against the previous codepoint,
// then the advance (markup.spacing is 0), and a '\n' takes the pen back to the
// origin, 0 in a run's own buffer, and starts the next line. Its LCD quads sit on
// 1/3 px phases of the pen, which moves the quad, not the pen.
SlotRenderer::PenWalk SlotRenderer::walkPens(FontData &fontData, float px, const gweni::String &text,
    std::vector<float> *pens, std::vector<int> *carets, std::vector<int> *glyphLines)
{
    // whether the font lays its codepoints in terminal cells (a fixed-width face)
    if(fontData.textureFont->cell_width > 0.0f)
        return walkPensIn<true>(fontData, px, text, pens, carets, glyphLines);
    return walkPensIn<false>(fontData, px, text, pens, carets, glyphLines);
}

template<bool Cells>
SlotRenderer::PenWalk SlotRenderer::walkPensIn(FontData &fontData, float px, const gweni::String &text,
    std::vector<float> *pens, std::vector<int> *carets, std::vector<int> *glyphLines)
{
    ftgl::texture_font_t *font=fontData.textureFont;
    const bool sdf=(fontData.mode == TextMode::Sdf);
    const float s=sdf?px/fontData.baseSize:1.0f;
    float pen=0.0f;
    float width=0.0f;
    int lines=1;
    const char *previous=nullptr;

    for(size_t i=0; i < text.size(); i+=std::max<size_t>(1, ftgl::utf8_surrogate_len(text.c_str()+i)))
    {
        const char *current=text.c_str()+i;

        // utf8_strlen, which gives text_buffer_add_text its length, ends at a NUL
        if(!sdf && (*current == '\0'))
            break;

        // the width of the prefix that ends here, as layoutSdfText gives it
        if(carets)
            carets->push_back((int)ceil(std::max(width, pen)));

        if(*current == '\n')
        {
            width=std::max(width, pen);
            pen=0.0f;
            ++lines;
            // text_buffer_add_text kerns the next glyph against the '\n'
            previous=sdf?nullptr:current;
            continue;
        }

        ftgl::texture_glyph_t *glyph=fontData.glyph(current);

        // a default-ignorable, which glyph() gives none, draws nothing and moves no
        // pen, and no pair holds it, in every mode (text_buffer_add_char,
        // layoutSdfText); text_buffer_add_text kerns against a codepoint with no glyph
        if(glyph == nullptr)
        {
            if(!sdf || ftgl::utf8_is_ignorable(current))
                previous=current;
            continue;
        }

        // a font with cells: no kerning, the glyph centred in its cells, the pen
        // moved by them (texture_font_cell_place, as text_buffer_add_char); a
        // font without goes on as it always did
        if constexpr(Cells)
        {
            float cellOffset=0.0f;
            float advance=0.0f;

            ftgl::texture_font_cell_place(font, glyph, ftgl::utf8_to_utf32(current), &cellOffset, &advance);
            cellOffset*=s;
            if(pens)
                pens->push_back(sdf?pen+cellOffset:roundf(pen+cellOffset));
            if(glyphLines)
                glyphLines->push_back(lines-1);
            pen+=advance*s;
            previous=current;
            continue;
        }

        if(previous && font->kerning)
            pen+=ftgl::texture_glyph_get_kerning(glyph, previous)*s;

        // a coverage glyph's quad sits at the pen rounded to a whole pixel
        if(pens)
            pens->push_back(sdf?pen:roundf(pen));
        if(glyphLines)
            glyphLines->push_back(lines-1);
        pen+=glyph->advance_x*s;
        previous=current;
    }
    return PenWalk{ std::max(width, pen), lines };
}

void SlotRenderer::measuredPenShifts(const gweni::Font &font, FontData &drawn, float px,
    const gweni::String &text, std::vector<float> &shifts, std::vector<float> &lineShifts)
{
    shifts.clear();
    lineShifts.clear();
    if(px == font.size)
        return;

    // An SDF font lays a run out linearly in px, so measured and drawn as SDF, at
    // font.size and at px, its pens differ by the scale alone. So do an LCD font's,
    // the unhinted advances and kerning, to within its 1/3 px quantisation, whatever
    // the run was measured in. Their lines are not linear in px (an SDF step is
    // rounded to a whole pixel), so only a run with a '\n' walks its lines, and only
    // they move. A run drawn as grayscale coverage has hinted whole-pixel pens of its
    // own, so it is moved to the measured ones, whether it was measured as coverage
    // or, at a scale below 1, as SDF.
    const bool multiLine=(text.find('\n') != gweni::String::npos);
    const bool measuredSdf=(resolveFont(font, font.size).mode == TextMode::Sdf);
    const bool lcd=(drawn.mode == TextMode::LcdRGB) || (drawn.mode == TextMode::LcdBGR);
    const bool linear=lcd || (measuredSdf && (drawn.mode == TextMode::Sdf));

    if(linear && !multiLine)
        return;

    std::pair<const FontKey, FontData> *drawFont=m_lastFont;

    if(!ensureFont(font, font.size))
    {
        m_lastFont=drawFont;
        return;
    }

    FontData &measured=m_lastFont->second;
    std::vector<float> measuredPens, drawnPens;
    std::vector<int> drawnLines;
    const float scale=px/font.size;

    m_lastFont=drawFont;
    textPens(drawn, px, text, drawnPens, multiLine?&drawnLines:nullptr);

    // Line L of the run is drawn L*lineStep(drawn) below the first and measured
    // L*lineStep(measured) below it, which scaled is not the same distance: each
    // line is moved by whole pixels to within half a pixel of the measured one,
    // whichever mode either font is in. Whole pixels keep an SDF run's baselines on
    // whole pixels, as layoutSdfText puts them, so a moved run is still a fresh
    // layout. A run of one line moves nothing.
    if(!drawnLines.empty() && (drawnLines.back() > 0))
    {
        const float lineError=scale*lineStep(measured, font.size)-lineStep(drawn, px);

        lineShifts.resize(drawnLines.size());
        for(size_t i=0; i < drawnLines.size(); ++i)
            lineShifts[i]=roundf(drawnLines[i]*lineError);
    }

    // the x pens of an SDF run drawn as SDF, or of an LCD run, are the measured ones
    // scaled; every other run is drawn at its measured pens below
    if(linear)
        return;

    textPens(measured, font.size, text, measuredPens, nullptr);

    // a glyph one font has and the other has not: left as drawn
    if(measuredPens.size() != drawnPens.size())
        return;

    shifts.resize(drawnPens.size());
    for(size_t i=0; i < drawnPens.size(); ++i)
    {
        const float target=measuredPens[i]*scale;

        // a coverage glyph stays on whole pixels
        shifts[i]=((drawn.mode == TextMode::Sdf)?target:roundf(target))-drawnPens[i];
    }
}

gweni::Point SlotRenderer::layoutSdfText(FontData &fontData, float px, const gweni::String &text,
    const gweni::Color &color, std::vector<ftgl::glyph_vertex_t> *vertexes,
    std::vector<int> *carets)
{
    ftgl::texture_font_t *font=fontData.textureFont;
    const float s=px/fontData.baseSize;
    const float r=color.r/255.0f, g=color.g/255.0f, b=color.b/255.0f, a=color.a/255.0f;

    // The baseline sits a whole number of pixels below the line's top, and the run's
    // origin is a whole pixel, so a run moved by a sub-pixel amount cannot shimmer.
    // Within the line the glyphs keep their fractional, unhinted pen positions.
    const float ascender=roundf(font->ascender*s);
    const float step=lineStep(fontData, px);
    float baseline=-ascender;
    float pen=0.0f;
    float width=0.0f;
    int lines=1;
    const char *previous=nullptr;
    // whether the font lays its codepoints in terminal cells (a fixed-width face)
    const bool cells=(font->cell_width > 0.0f);

    //one codepoint at a time, as text_buffer_add_text goes
    for(size_t i=0; i < text.size(); i+=std::max<size_t>(1, ftgl::utf8_surrogate_len(text.c_str()+i)))
    {
        const char *current=text.c_str()+i;

        // the width of the prefix that ends here: the next glyph's kerning is not in it
        if(carets)
            carets->push_back((int)ceil(std::max(width, pen)));

        if(*current == '\n')
        {
            width=std::max(width, pen);
            pen=0.0f;
            baseline-=step;
            ++lines;
            previous=nullptr;
            continue;
        }

        ftgl::texture_glyph_t *glyph=fontData.glyph(current);

        // a default-ignorable, which glyph() gives none: nothing drawn, no advance,
        // and no pair across it, as text_buffer_add_char
        if(glyph == nullptr)
        {
            if(ftgl::utf8_is_ignorable(current))
                previous=current;
            continue;
        }

        // a font with cells: no kerning, the glyph centred in its cells
        float cellOffset=0.0f;
        float advance=glyph->advance_x*s;

        if(cells)
        {
            ftgl::texture_font_cell_place(font, glyph, ftgl::utf8_to_utf32(current), &cellOffset, &advance);
            cellOffset*=s;
            advance*=s;
        }
        else if(previous && font->kerning)
            pen+=ftgl::texture_glyph_get_kerning(glyph, previous)*s;

        if(vertexes)
        {
            // the glyph's field, its spread padding included, scaled about the pen
            const float x0=pen+cellOffset+glyph->offset_x*s;
            const float x1=x0+glyph->width*s;
            const float y0=baseline+glyph->offset_y*s;
            const float y1=y0-glyph->height*s;
            // in text_buffer_add_char's vertex order; shift is unused, gamma 1
            const ftgl::glyph_vertex_t quad[4]=
            {
                {x0, y0, 0.0f, glyph->s0, glyph->t0, r, g, b, a, 0.0f, 1.0f},
                {x0, y1, 0.0f, glyph->s0, glyph->t1, r, g, b, a, 0.0f, 1.0f},
                {x1, y1, 0.0f, glyph->s1, glyph->t1, r, g, b, a, 0.0f, 1.0f},
                {x1, y0, 0.0f, glyph->s1, glyph->t0, r, g, b, a, 0.0f, 1.0f}
            };

            vertexes->insert(vertexes->end(), quad, quad+4);
        }

        pen+=advance;
        previous=current;
    }
    width=std::max(width, pen);

    if(carets)
        carets->push_back((int)ceil(width));

    return gweni::Point((int)ceil(width), (int)ceil(lineBoxHeight(fontData, px)+(lines-1)*step));
}

float SlotRenderer::lineBoxHeight(const FontData &fontData, float px)
{
    if(fontData.mode == TextMode::Sdf)
    {
        const float s=px/fontData.baseSize;

        return fontData.textureFont->height*s;
    }

    return fontData.markup.font->height;
}

float SlotRenderer::lineStep(const FontData &fontData, float px)
{
    if(fontData.mode == TextMode::Sdf)
    {
        const ftgl::texture_font_t *font=fontData.textureFont;
        const float s=px/fontData.baseSize;

        return roundf((font->ascender-font->descender)*s);
    }

    const ftgl::texture_font_t *font=fontData.markup.font;

    return font->ascender-(int)font->descender;
}

void SlotRenderer::showText(size_t textId)
{
    if(textId >= m_textRuns.size())
        return;

    TextRun &run=m_textRuns[textId];

    if(!run.hidden)
        return;

#ifdef DEBUG_RENDER
    std::cout<<"Showing text: "<<run.text<<"\n";
#endif//DEBUG_RENDER

    const std::pair<size_t, size_t> drawn=drawnRange(run);

    run.hidden=false;
    markTextDrawList(run, drawn);
}

void SlotRenderer::hideText(size_t textId)
{
    if(textId >= m_textRuns.size())
        return;

    TextRun &run=m_textRuns[textId];

    if(run.hidden)
        return;

#ifdef DEBUG_RENDER
    std::cout<<"Hiding text: "<<run.text<<"\n";
#endif//DEBUG_RENDER

    const std::pair<size_t, size_t> drawn=drawnRange(run);

    run.hidden=true;
    markTextDrawList(run, drawn);
}

// The size is in canvas (unscaled) units at any scale(): the text is measured at
// font.size, in the font it is drawn in at scale 1, so a layout is the same at every
// scale, and renderText draws it at font.size*scale() from the scaled origin. This
// deliberately differs from the legacy OpenGL and Software renderers, whose
// measureText returns scaled pixels (GWEN's long-standing inconsistency).
gweni::Point SlotRenderer::measureText(const gweni::Font &font, const gweni::String &text)
{
    if(!ensureFont(font, font.size))
        return gweni::Point(0, 0);

    FontData &fontData=m_lastFont->second;

    // In an SDF font: the same layout renderText draws, so sizeToContents fits it
    if(fontData.mode == TextMode::Sdf)
    {
        const gweni::Point sz=layoutSdfText(fontData, font.size, text, gweni::Color(), nullptr);

        if(textDebugMatches(text))
            ::printf("[measure] \"%s\" sdf font.size=%f scale=%f base=%f => sz.x=%d sz.y=%d"
                " (atlas %zux%zu used=%zu glyphs=%zu)\n",
                text.c_str(), (double)font.size, (double)scale(), (double)fontData.baseSize, sz.x, sz.y,
                fontData.atlas->width, fontData.atlas->height, fontData.atlas->used,
                fontData.textureFont->glyphs->size);
        return sz;
    }

    const bool debug=textDebugMatches(text);
    std::vector<float> pens;
    const PenWalk walk=walkPens(fontData, font.size, text, debug?&pens:nullptr, nullptr);

    // A line's pen ends at the sum of its glyph advances (plus kerning), which is
    // the width the next string would start at. The right edge of the last glyph's
    // ink is a different and smaller quantity, and is not what a control laying
    // text out wants. renderText draws every line, so the size is the widest one
    // and every line, as layoutSdfText measures an Sdf run.
    Point sz((int)ceil(walk.width),
        (int)ceil(lineBoxHeight(fontData, font.size)+(walk.lines-1)*lineStep(fontData, font.size)));

    if(debug)
    {
        ::printf("[measure] \"%s\" len=%zu font.size=%f scale=%f\n",
            text.c_str(), text.length(), (double)font.size, (double)scale());
        ::printf("[measure]   markup.size=%f font->size=%f height=%f ascender=%f descender=%f\n",
            (double)fontData.markup.size, (double)fontData.markup.font->size,
            (double)fontData.markup.font->height, (double)fontData.markup.font->ascender,
            (double)fontData.markup.font->descender);
        for(size_t i=0; i<pens.size(); ++i)
            ::printf("[measure]   glyph %-3zu pen x=%10.3f\n", i, (double)pens[i]);
        ::printf("[measure]   => sz.x=%d sz.y=%d (widest line=%f, lines=%d)\n",
            sz.x, sz.y, (double)walk.width, walk.lines);
        ::printf("[measure]   atlas %zux%zu depth=%zu used=%zu\n",
            fontData.atlas->width, fontData.atlas->height, fontData.atlas->depth,
            fontData.atlas->used);
        for(size_t i=0; i<text.length(); ++i)
        {
            char cp[2]={text[i], '\0'};
            ftgl::texture_glyph_t *g=ftgl::texture_font_find_glyph(fontData.markup.font, cp);
            if(g == nullptr)
            {
                ::printf("[measure]   glyph '%c' NOT CACHED\n", cp[0]);
                continue;
            }
            ::printf("[measure]   glyph '%c' w=%zu h=%zu off=(%d,%d) advance=(%.3f,%.3f) "
                "st=(%.5f,%.5f)-(%.5f,%.5f)\n",
                cp[0], g->width, g->height, g->offset_x, g->offset_y,
                (double)g->advance_x, (double)g->advance_y,
                (double)g->s0, (double)g->t0, (double)g->s1, (double)g->t1);
        }
    }

    return sz;
}

// The coverage measure as freetype-gl lays the run out for renderText: a whole
// text_buffer_t built and thrown away for its pen. tests/measureTextPens.cpp holds
// measureText to it, line by line.
gweni::Point SlotRenderer::measureTextWithTextBuffer(const gweni::Font &font, const gweni::String &text,
    float *penY, float *lineBox)
{
    if(!ensureFont(font, font.size))
        return gweni::Point(0, 0);

    FontData &fontData=m_lastFont->second;

    if(fontData.mode == TextMode::Sdf)
        return layoutSdfText(fontData, font.size, text, gweni::Color(), nullptr);

    ftgl::text_buffer_t *textBuffer=ftgl::text_buffer_new();
    ftgl::vec2 pen={0.f, 0.f};

    ftgl::text_buffer_add_text(textBuffer, &pen, &fontData.markup, text.c_str(), ftgl::utf8_strlen(text.c_str()));
    text_buffer_delete(textBuffer);
    if(penY)
        *penY=pen.y;
    if(lineBox)
        *lineBox=lineBoxHeight(fontData, font.size);
    return Point((int)ceil(pen.x), (int)ceil(lineBoxHeight(fontData, font.size)));
}

// Whether freetype-gl's walk of text (utf8_surrogate_len's steps, as
// text_buffer_add_text and layoutSdfText go) stops at exactly the boundaries
// utf8Next gives, and so lays out each caret's prefix as measureText would. Not
// for malformed UTF-8, nor for a NUL, where utf8_strlen ends a coverage measure.
static bool ftglStepsAreCodepoints(const gweni::String &text)
{
    for(size_t i=0; i < text.size(); )
    {
        const size_t next=utf8Next(text, i);

        if(text[i] == '\0' || ftgl::utf8_surrogate_len(text.c_str()+i) != next-i)
            return false;
        i=next;
    }
    return true;
}

// measureText of every caret's prefix from one layout: measureText(prefix) is the
// same walk stopped at the prefix's end, so each x is equal by construction.
void SlotRenderer::measureCarets(const gweni::Font &font, const gweni::String &text,
    std::vector<int> &xs)
{
    if(!ftglStepsAreCodepoints(text) || !ensureFont(font, font.size))
    {
        renderer::Base::measureCarets(font, text, xs);
        return;
    }

    FontData &fontData=m_lastFont->second;

    xs.clear();

    if(fontData.mode == TextMode::Sdf)
    {
        layoutSdfText(fontData, font.size, text, gweni::Color(), nullptr, &xs);
        return;
    }

    // the pen walk measureText makes, read before each codepoint and at the end
    const PenWalk walk=walkPens(fontData, font.size, text, nullptr, &xs);

    xs.push_back((int)ceil(walk.width));
}

// measureText's y for a one-line string, without laying one out: the font's line
// box at font.size, in canvas units at any scale() as measureText is.
int SlotRenderer::lineHeight(const gweni::Font &font)
{
    if(!ensureFont(font, font.size))
        return 0;

    return (int)ceil(lineBoxHeight(m_lastFont->second, font.size));
}

} // namespace renderer
} // namespace gweni
