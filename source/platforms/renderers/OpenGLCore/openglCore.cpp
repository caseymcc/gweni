/*
 *  Gweni - OpenGL Core renderer.
 *  Copyright (c) 2013-18 Nick Trout
 *  See license in Gweni.h
 */
#ifdef GWENI_DEBUG_LOGGING
#define DEBUG_RENDER
#endif

#include <gweni/platforms/renderers/openglCore.h>
#if defined(__APPLE__)
#   include <GL/glew.h>
#   include <OpenGL/gl.h>
#else
#   include <GL/glew.h>
#   include <GL/gl.h>
#endif
#include <gweni/platforms/platformTypes.h>
#include <gweni/platforms/windowProvider.h>
#include <gweni/platforms/commonPlatform.h>
#include <gweni/utility.h>
#include <gweni/primitiveIds.h>
#include <gweni/utils/filesystem.h>

#include <iterator>

#if defined(_WIN32)
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   undef min
#   undef max
#   define CREATE_NATIVE_CONTEXT 1
#else
#   define CREATE_NATIVE_CONTEXT 0
#endif

#include <math.h>
#include <sys/stat.h>
#include <stdio.h>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <cassert>
#include <limits>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <freetype-gl/freetype-gl.h>
#include <freetype-gl/text-buffer.h>
#include <freetype-gl/utf8-utils.h>

#include <iostream>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imglib/imageIo.h>
#include <imglib/simpleImage.h>

namespace gweni
{
namespace renderer
{

static constexpr size_t minPrimitiveSize=512;
static constexpr size_t minVertexSize=minPrimitiveSize*4;
static constexpr size_t minClippingSize=minPrimitiveSize;

// The SDF text atlas (GLTextMode::Sdf): one per (face, bold), shared by every size
// at or above the threshold. Its glyphs are rasterised once, at sdfBaseSize px with
// a distance field reaching sdfSpread px past each edge, into an R8 atlas.
static constexpr float sdfBaseSize=48.0f;
static constexpr int sdfSpread=6;
static constexpr size_t sdfAtlasSize=1024;
// GWENI_SDF_TEXT_MIN's default, in effective px
static constexpr float defaultSdfTextMin=18.0f;
// An LCD font's glyph positions per device pixel: one per subpixel
static constexpr int lcdSubpixelPositions=3;
// GWENI_TEXT_CONTRAST's default: the LCD coverage boost for white text
static constexpr float defaultTextContrast=1.0f;

// The printable ASCII every font rasterises when it is loaded
static const char *const fontCache=" !\"#$%&'()*+,-./0123456789:;<=>?"
    "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
    "`abcdefghijklmnopqrstuvwxyz{|}~";

GLTextureData::~GLTextureData()
{
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, reinterpret_cast<GLuint *>(&m_textureId));
}

GLFontData::~GLFontData()
{
    // the atlas lives in memory the manager owns, so its texture goes first
    if(atlas && atlas->id)
        glDeleteTextures(1, &atlas->id);
    // frees the texture_font_t, the atlas and the cache string loadFont strdup'd
    if(fontManager)
        ftgl::font_manager_delete(fontManager);
    free(familyPath);
}

float OpenGLCore::textPx(const Font &font) const
{
    return font.size*scale();
}

GLFontKey OpenGLCore::resolveFont(const Font &font, float px) const
{
    GLFontKey key{font, GLTextMode::Coverage};

    if(px >= m_sdfTextMin)
    {
        // LCD is for the coverage range only: an SDF atlas is grayscale in every mode
        key.mode=GLTextMode::Sdf;
        key.font.size=0.f;
    }
    else
    {
        if(m_textAA == TextAA::LcdRGB)
            key.mode=GLTextMode::LcdRGB;
        else if(m_textAA == TextAA::LcdBGR)
            key.mode=GLTextMode::LcdBGR;
        key.font.size=px;
    }
    return key;
}

GLTextMode OpenGLCore::getTextMode(const Font &font) const
{
    return resolveFont(font, textPx(font)).mode;
}

void OpenGLCore::setTextAA(TextAA aa)
{
    if(aa == m_textAA)
        return;

    m_textAA=aa;

    // Every font of the coverage range is now resolved under another mode. Each is
    // freed, its runs leaving every draw list (they draw nothing until renderText
    // lays them out again, in the font of the new mode), and loaded again in the
    // new mode, so what was loaded stays loaded. An SDF atlas is the same in
    // every mode and stays as it is, its runs drawing.
    std::vector<Font> reload;

    for(auto it=m_fonts.begin(); it != m_fonts.end();)
    {
        if(it->second.mode == GLTextMode::Sdf)
        {
            ++it;
            continue;
        }

        reload.push_back(it->first.font);
        for(size_t textId:it->second.textRuns)
            m_textRuns[textId].fontData=nullptr;
        it=m_fonts.erase(it); // GLFontData's destructor deletes the atlas texture and the manager
    }

    m_lastFont=nullptr;
    // a failure in another mode is tried again in this one
    for(auto it=m_failedFonts.begin(); it != m_failedFonts.end();)
    {
        if(it->mode != GLTextMode::Sdf)
            it=m_failedFonts.erase(it);
        else
            ++it;
    }

    // the key's size is the px the atlas is rasterised at, so it resolves to itself
    for(const Font &font:reload)
        loadFont(font, font.size);

    textLayoutChanged();
}

Font::Status OpenGLCore::loadFont(const Font &requested)
{
    return loadFont(requested, textPx(requested));
}

Font::Status OpenGLCore::loadFont(const Font &requested, float px)
{
    GLFontData fontData;
    const GLFontKey key=resolveFont(requested, px);
    const Font &font=key.font;
    const GLTextMode mode=key.mode;
    const bool sdf=(mode == GLTextMode::Sdf);
    const bool lcd=(mode == GLTextMode::LcdRGB) || (mode == GLTextMode::LcdBGR);

    // an explicit load is a fresh attempt; a failure is remembered again below,
    // so ensureFont does not retry it
    m_failedFonts.erase(key);

    // already loaded: a second manager and atlas would only leak
    auto loaded=m_fonts.find(key);

    if(loaded != m_fonts.end())
    {
        loaded->second.freed=false;
        m_lastFont=&(*loaded);
        return Font::Status::Loaded;
    }

    m_lastFont=nullptr;

    const String filename=getResourcePaths().getPath(ResourcePaths::Type::Font, font.facename);

    if(!fs::exists(filename))
    {
        gweni::log::write(log::Level::Error, "Font file not found: %s", filename.c_str());
        m_failedFonts.insert(key);
        return Font::Status::ErrorFileNotFound;
    }

    vec4 black={{0.0, 0.0, 0.0, 1.0}};
    vec4 none={{1.0, 1.0, 1.0, 0.0}};

    fontData.faceName=font.facename;
    fontData.mode=mode;
    fontData.baseSize=sdf?sdfBaseSize:font.size;
    // markup_t keeps the family as a borrowed char*, and the markup outlives this
    // function, so it cannot point at the local filename.
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
    // font's spread or an LCD font's subpixel positions could be set, so those
    // fonts load it themselves below.
    fontData.fontManager->cache=strdup((sdf || lcd)?"":fontCache);

    fontData.markup.font=ftgl::font_manager_get_from_markup(fontData.fontManager, &fontData.markup);

    if(sdf && fontData.markup.font)
    {
        fontData.markup.font->sdf_spread=sdfSpread;
        // unhinted, so the kerning is not rounded to whole base-size pixels
        fontData.markup.font->hinting=0;
        ftgl::texture_font_load_glyphs(fontData.markup.font, fontCache);
    }
    else if(lcd && fontData.markup.font)
    {
        // Pens at 1/3 device px with the unhinted advances and kerning, each glyph
        // drawn from the image rasterised at its pen's phase. The cache is phase 0;
        // the other phases are rasterised as a run first needs them.
        fontData.markup.font->subpixel_positions=lcdSubpixelPositions;
        ftgl::texture_font_load_glyphs(fontData.markup.font, fontCache);
    }

    if(fontData.markup.font == nullptr)
    {
        // freetype-gl could not open the face. fontData's destructor frees the
        // manager and familyPath; no atlas texture exists yet.
        gweni::log::write(log::Level::Error, "Font file failed to load: %s", filename.c_str());
        m_failedFonts.insert(key);
        return Font::Status::ErrorBadData;
    }

    fontData.textureFont=fontData.markup.font;

    fontData.atlas=fontData.fontManager->atlas;

    glGenTextures(1, &fontData.atlas->id);
    glBindTexture(GL_TEXTURE_2D, fontData.atlas->id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // one byte per texel, coverage or distance alike, or three of subpixel coverage
    assert(fontData.atlas->depth == (lcd?3u:1u));
    glTexImage2D(GL_TEXTURE_2D, 0, lcd?GL_RGB8:GL_R8, fontData.atlas->width,
        fontData.atlas->height, 0, lcd?GL_RGB:GL_RED, GL_UNSIGNED_BYTE, nullptr);
    //the next flush() uploads the whole atlas, cache included, as one dirty band, so
    //a load shows in FrameStats::atlasUploadCalls; after that only later glyphs
    fontData.atlas->dirty_y0=0;
    fontData.atlas->dirty_y1=fontData.atlas->height;

    m_lastFont=&(*m_fonts.insert(std::make_pair(key, std::move(fontData))).first);
    return Font::Status::Loaded;
}

void OpenGLCore::freeFont(const gweni::Font &requested)
{
    const GLFontKey drawn=resolveFont(requested, textPx(requested));
    const GLFontKey measured=resolveFont(requested, requested.size);

    freeFont(drawn, false);
    // measureText's font, at another scale(): only measured by this font, unless
    // another size is drawn in it
    if(!(measured == drawn))
        freeFont(measured, true);
}

void OpenGLCore::freeFont(const GLFontKey &font, bool onlyIfUnused)
{
    if(m_lastFont != nullptr && m_lastFont->first == font)
        m_lastFont=nullptr;

    m_failedFonts.erase(font);

    auto it=m_fonts.find(font);

    if(it == m_fonts.end())
        return;

    if(onlyIfUnused && !it->second.textRuns.empty())
        return;

    // An SDF atlas is shared by every size of its face, so the runs in it may be
    // another size's, whose owner has not freed it: Label frees its font when it
    // changes size or is deleted. It stays, every run drawing, until its last run
    // leaves it (setTextRunFont), unless it is used again before that.
    if((it->second.mode == GLTextMode::Sdf) && !it->second.textRuns.empty())
    {
        it->second.freed=true;
        return;
    }

    // The runs laid out in this font keep their slots but leave every draw list:
    // their glyphs sample the atlas the destructor deletes, whose name glGenTextures
    // may hand out again. The next renderText of each lays it out again, in the
    // font loaded anew.
    for(size_t textId:it->second.textRuns)
        m_textRuns[textId].fontData=nullptr;

    m_fonts.erase(it); // GLFontData's destructor deletes the atlas texture and the manager
}

bool OpenGLCore::ensureFont(const Font &requested)
{
    return ensureFont(requested, textPx(requested));
}

bool OpenGLCore::ensureFont(const Font &requested, float px)
{
    const GLFontKey font=resolveFont(requested, px);

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
    return loadFont(requested, px) == Font::Status::Loaded;
}

Texture::Status OpenGLCore::loadTexture(Texture &texture)
{
    freeTexture(texture);
    // only a successful load hands out a slot; a failed one leaves the texture
    // unloaded, so it is drawn as missing and freeing it does nothing
    texture.dataIndex=std::numeric_limits<size_t>::max();
    m_lastTexture=std::numeric_limits<size_t>::max();

    const String filename=getResourcePaths().getPath(ResourcePaths::Type::Texture, texture.name);

    GLTextureData &texData=*getFreeTextureData();

    bool loaded=imglib::load(texData, filename.c_str());

    //    int width, height, n;
    //    {
    //        unsigned char *image = stbi_load(filename.c_str(), &width, &height, &n, 4);
    //        texData.m_readData = deleted_unique_ptr<unsigned char>(image, [](unsigned char* mem) { if (mem) stbi_image_free(mem); });
    //    }

    // Image failed to load. The loaders resize before reading pixels, so a late
    // failure leaves a non-empty buffer; either way hand the slot back.
    if(!loaded || texData.m_readData.empty())
    {
        bool notFound=texData.m_readData.empty();

        texData.m_readData.clear();
        m_freeTextureData.push_back(texData.m_index);

        if(notFound)
        {
            gweni::log::write(log::Level::Error, "Texture file not found: %s", filename.c_str());
            return Texture::Status::ErrorFileNotFound;
        }
        gweni::log::write(log::Level::Error, "Texture file failed to load: %s", filename.c_str());
        return Texture::Status::ErrorBadData;
    }


    // Create the opengl texture
    glGenTextures(1, &texData.m_textureId);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texData.m_textureId);

    // mipmapped, because an image is often drawn much smaller than it is; clamped,
    // so the linear filter does not wrap the opposite edge into the border texels
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    imglib::Format imgFormat=imglib::format(texData);
    GLenum format;

    if(imgFormat == imglib::Format::RGBA)
        format=GL_RGBA;
    else if(imgFormat == imglib::Format::RGB)
        format=GL_RGB;
    else
    {
        glDeleteTextures(1, &texData.m_textureId);
        texData.m_textureId=0;
        texData.m_readData.clear();
        m_freeTextureData.push_back(texData.m_index);
        gweni::log::write(log::Level::Error, "Texture format not supported: %s", filename.c_str());
        return Texture::Status::ErrorBadData;
    }

    // RGB rows are not padded to 4 bytes
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texData.width, texData.height, 0, format,
        GL_UNSIGNED_BYTE, (const GLvoid *)texData.m_readData.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glGenerateMipmap(GL_TEXTURE_2D);

    texData.readable=texture.readable;
    if(!texture.readable)
    {
        texData.m_readData.clear();
    }

    //    texData.width = width;
    //    texData.height = height;
    if(texData.m_textureId >= m_textureSizes.size())
        m_textureSizes.resize(texData.m_textureId+1);

    m_textureSizes[texData.m_textureId].x=texData.width;
    m_textureSizes[texData.m_textureId].y=texData.height;

//    m_lastTexture=&(*m_textures.insert(std::make_pair(texture, std::move(texData))).first);
    setTexture(texData.m_textureId);
    m_lastTexture=texData.m_index;
    texture.dataIndex=texData.m_index;
    return Texture::Status::Loaded;
}

void OpenGLCore::freeTexture(const gweni::Texture &texture)
{
//    if(m_lastTexture != nullptr && m_lastTexture->first == texture)
//        m_lastTexture=nullptr;
    if((m_lastTexture != std::numeric_limits<size_t>::max()) && (m_lastTexture == texture.dataIndex))
        m_lastTexture=std::numeric_limits<size_t>::max();

    if((texture.dataIndex < m_textureData.size()) && (m_textureData[texture.dataIndex]))
    {
        // setTexture() is skipped while m_currentTexture matches, and a later
        // glGenTextures may hand the deleted name out again
        if(m_textureData[texture.dataIndex]->m_textureId == m_currentTexture)
        {
            m_currentTexture=0;
            m_currentTextureSize=glm::uvec2(0, 0);
        }
        // the name may be handed out again; slots still on its unit sample nothing
        for(unsigned int &name:m_imageUnits)
        {
            if(name == m_textureData[texture.dataIndex]->m_textureId)
                name=0;
        }
        m_freeTextureData.push_back(texture.dataIndex);
        delete m_textureData[texture.dataIndex];
        m_textureData[texture.dataIndex]=nullptr;
    }
    m_textures.erase(texture);
//    m_textures.erase(texture); // calls GLTextureData destructor
}

GLTextureData *OpenGLCore::getFreeTextureData()
{
    GLTextureData *textureData;

    if(!m_freeTextureData.empty())
    {
        size_t index=m_freeTextureData.back();
        m_freeTextureData.pop_back();

        // freeTexture() deletes the slot's data but keeps the index on the free
        // list, so a recycled slot can be empty.
        if(!m_textureData[index])
        {
            m_textureData[index]=new GLTextureData();
            m_textureData[index]->m_index=index;
        }
        textureData=m_textureData[index];
    }
    else
    {
        textureData=new GLTextureData();
        textureData->m_index=m_textureData.size();
        m_textureData.resize(m_textureData.size()+1);
        m_textureData.back()=textureData;
    }
    return textureData;
}

unsigned int OpenGLCore::getTextureName(const Texture &texture) const
{
    if((texture.dataIndex < m_textureData.size()) && (m_textureData[texture.dataIndex]))
        return m_textureData[texture.dataIndex]->m_textureId;
    return 0;
}

unsigned int OpenGLCore::getFontAtlasName(const Font &requested) const
{
    auto it=m_fonts.find(resolveFont(requested, textPx(requested)));

    if((it != m_fonts.end()) && (it->second.atlas))
        return it->second.atlas->id;
    return 0;
}

size_t OpenGLCore::getFontAtlasDepth(const Font &requested) const
{
    auto it=m_fonts.find(resolveFont(requested, textPx(requested)));

    if((it != m_fonts.end()) && (it->second.atlas))
        return it->second.atlas->depth;
    return 0;
}

ftgl::texture_font_t *OpenGLCore::getTextureFont(const Font &requested) const
{
    auto it=m_fonts.find(resolveFont(requested, textPx(requested)));

    return (it != m_fonts.end())?it->second.textureFont:nullptr;
}

OpenGLCore::GLObjectNames OpenGLCore::getGLObjectNames() const
{
    GLObjectNames names;

    names.buffers={m_vertexBuffer, m_quadIndexBuffer, m_clipBuffer,
        m_textVertexBuffer, m_textQuadIndexBuffer, m_textClipBuffer};
    names.vertexArrays=std::vector<unsigned int>(m_vertexArrays.begin(), m_vertexArrays.end());
    names.vertexArrays.insert(names.vertexArrays.end(), m_textVertexArrays.begin(), m_textVertexArrays.end());
    names.textures={m_clipTexture, m_textClipTexture};
    names.programs={m_program, m_textProgram};
    return names;
}

TextureData OpenGLCore::getTextureData(const Texture &texture) const
{
//    if(m_lastTexture != nullptr && m_lastTexture->first == texture)
//        return m_lastTexture->second;
//
//    auto it=m_textures.find(texture);
//    if(it != m_textures.cend())
//    {
//        return it->second;
//    }
//    // Texture not loaded :(
//    return TextureData();
    return *m_textureData[texture.dataIndex];
}

bool OpenGLCore::ensureTexture(const gweni::Texture &texture)
{
//    if(m_lastTexture != nullptr)
    if(m_lastTexture != std::numeric_limits<size_t>::max())
    {
        if(m_lastTexture == texture.dataIndex)
            return true;
    }

    // Was it loaded before?
//    auto it=m_textures.find(texture);
//    if(it != m_textures.end())
//    {
//        m_lastTexture=&(*it);
//        return true;
//    }
    if(texture.dataIndex != std::numeric_limits<size_t>::max())
    {
        m_lastTexture=texture.dataIndex;
        return true;
    }

    // No, try load to it

    // LoadTexture sets m_lastTexture, if exist
//    return loadTexture(texture) == Texture::Status::Loaded;
    return false;
}

//-------------------------------------------------------------------------------

OpenGLCore::OpenGLCore(ResourcePaths &paths, const Rect &viewRect)
    : Base(paths)
    , m_lastFont(nullptr)
    , m_lastTexture(std::numeric_limits<size_t>::max())
    , m_context(nullptr)
    , m_viewRect(viewRect)
    , m_currentTexture(0)
//    , m_vertexes(minVertexSize)
//    , m_clippingBoxes(minClippingSize)
{
    m_vertexes.reserve(minVertexSize);

    m_clippingBoxes.reserve(minClippingSize);

    m_uploadStats=(getenv("GWENI_UPLOAD_STATS") != nullptr);

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

bool OpenGLCore::textDebugMatches(const gweni::String &text) const
{
    if(m_textDebug == nullptr)
        return false;
    if(m_textDebug[0] == '\0')
        return true;
    return text.find(m_textDebug) != gweni::String::npos;
}

OpenGLCore::~OpenGLCore()
{
    // each GLFontData deletes its atlas texture and its freetype-gl manager
    m_lastFont=nullptr;
    m_fonts.clear();
    m_failedFonts.clear();
    for(TextRun &run:m_textRuns)
        run.fontData=nullptr;

    for(GLTextureData *textureData:m_textureData)
        delete textureData;
    m_textureData.clear();
    m_freeTextureData.clear();
    m_textures.clear();
    m_currentTexture=0;
    std::fill(std::begin(m_imageUnits), std::end(m_imageUnits), 0u);

    // nothing below exists before init()
    if(m_quadIndexBuffer == 0)
        return;

    glBindVertexArray(0);
    // a program still in use is only flagged for deletion
    glUseProgram(0);

    const GLuint buffers[]={m_vertexBuffer, m_quadIndexBuffer, m_clipBuffer,
        m_textVertexBuffer, m_textQuadIndexBuffer, m_textClipBuffer};
    const GLuint textures[]={m_clipTexture, m_textClipTexture};

    // deleting a bound object unbinds it, so another renderer in this context
    // finds nothing of these left bound; it binds its own every flush()
    if(!m_vertexArrays.empty())
        glDeleteVertexArrays((GLsizei)m_vertexArrays.size(), m_vertexArrays.data());
    if(!m_textVertexArrays.empty())
        glDeleteVertexArrays((GLsizei)m_textVertexArrays.size(), m_textVertexArrays.data());
    glDeleteBuffers(6, buffers);
    glDeleteTextures(2, textures);
    glDeleteProgram(m_program);
    glDeleteProgram(m_textProgram);
}

void checkErrors(unsigned int shader, std::string type)
{
    int success;
    char infoLog[1024];
    if(type != "PROGRAM")
    {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if(!success)
        {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }
    else
    {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if(!success)
        {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }
}

void OpenGLCore::init()
{
    m_projectionMatrix=glm::ortho(
        static_cast<float>(m_viewRect.x),
        static_cast<float>(m_viewRect.x + m_viewRect.w),
        static_cast<float>(m_viewRect.y + m_viewRect.h),
        static_cast<float>(m_viewRect.y),
//        static_cast<float>(m_viewRect.y),
//        static_cast<float>(m_viewRect.y + m_viewRect.h),
        -10000.0f,
        0.0f
    );

    //the buffers below are freshly created, so nothing of the mirror is on the device yet
    m_vertexBufferCapacity=0;
    m_clipBufferCapacity=0;
    m_textVertexBufferCapacity=0;
    m_textClipBufferCapacity=0;

    // Until a canvas sets its region, the whole view is visible: the shaders discard
    // everything outside the clip box, and an unset region would be an empty one.
    setClipRegion(m_viewRect);

    // Creating buffers
    glCreateBuffers(1, &m_vertexBuffer);
    glCreateBuffers(1, &m_quadIndexBuffer);
    glCreateBuffers(1, &m_clipBuffer);
    glCreateBuffers(1, &m_textVertexBuffer);
    glCreateBuffers(1, &m_textQuadIndexBuffer);
    glCreateBuffers(1, &m_textClipBuffer);

    // Every chunk draws its slots from vertex 0 of its own attribute base, so one
    // pattern of kSlotsPerChunk quads per path serves every chunk and never changes.
    // The primitives' quads zigzag (v, v+1, v+2, v+1, v+3, v+2); freetype-gl's glyph
    // quads, and layoutSdfText's in its order, go round their corners (v, v+1, v+2,
    // v, v+2, v+3). Each path keeps the triangles it always had, so the diagonal
    // each quad is split along, and with it every interpolated value, is unchanged.
    {
        static const uint16_t primitiveQuad[6]={ 0, 1, 2, 1, 3, 2 };
        static const uint16_t glyphQuad[6]={ 0, 1, 2, 0, 2, 3 };
        std::vector<uint16_t> quads(kSlotsPerChunk*6);

        glBindVertexArray(0);
        for(const auto &pattern:{ std::make_pair(m_quadIndexBuffer, primitiveQuad),
            std::make_pair(m_textQuadIndexBuffer, glyphQuad) })
        {
            for(size_t slot=0; slot<kSlotsPerChunk; ++slot)
            {
                const uint16_t v=static_cast<uint16_t>(slot*4);

                for(size_t j=0; j<6; ++j)
                    quads[slot*6+j]=v+pattern.second[j];
            }

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pattern.first);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, quads.size()*sizeof(uint16_t), quads.data(), GL_STATIC_DRAW);
        }
    }

    // the chunks' vertex arrays are made as flush() first draws them
    m_vertexArrays.clear();
    m_textVertexArrays.clear();

    // The clip boxes are not vertex attributes: each fragment shader reads its slot's
    // Box from a buffer texture, one RGBA16UI texel per slot (see flush()).
    glGenTextures(1, &m_clipTexture);
    glGenTextures(1, &m_textClipTexture);

    // Loading shaders
    auto vertexSource=R"(
#version 330 core

//layout (location = 0) in vec3 inPosition;
//layout (location = 1) in vec2 inTexCoords;
//layout (location = 2) in vec4 inColor;

layout (location = 0) in uint style;
layout (location = 1) in ivec3 inPosition;   // device px (signed) and z
layout (location = 2) in uvec2 inTexCoords;    // color0 (top fill) or UV
layout (location = 3) in ivec2 inLocal;        // SDF local position (rel to rect center)
layout (location = 4) in uvec2 inRadiusBorder; // SDF .x=radius .y=border (px)
layout (location = 5) in uvec2 inColor1;       // SDF gradient bottom color
layout (location = 6) in uvec2 inBorderColor;  // SDF border color
layout (location = 7) in uvec2 inColor3;       // bilinear bottom edge (6) / checker dark (7)

out vec2 TexCoords;
out vec4 VertexColor;
flat out uint ShaderType;
out vec2 vLocal;          // interpolated local position (px)
flat out vec2 vHalf;      // rect half-size (px)
flat out float vRadius;
flat out float vBorder;
flat out vec4 vColor1;    // gradient bottom color
flat out vec4 vBorderColor;
out vec4 vColor3;         // smooth: style 6 interpolates it along x
flat out vec4 vSegment;   // style 3: the line's endpoints (xy, zw), px from the quad centre
flat out float vBehindDepth; // style 3: the depth half a layer behind this slot's z
out vec2 vPixel;          // pixel-space position, the clip boxes' space

uniform mat4 projection;

vec4 unpackColor(uvec2 c)
{
    return vec4(
        float((c.x >> 8) & 0xffu)/255.0,
        float(c.x & 0xffu)/255.0,
        float((c.y >> 8) & 0xffu)/255.0,
        float(c.y & 0xffu)/255.0);
}

void main()
{
    ShaderType = style;

    TexCoords = vec2(inTexCoords)/65535.0;
    VertexColor = unpackColor(inTexCoords);

    // inLocal is in half-pixel units, so an odd-sized rect's half-size (3.5) is exact
    vLocal = vec2(inLocal)*0.5;
    vHalf = abs(vLocal);
    vRadius = float(inRadiusBorder.x);
    vBorder = float(inRadiusBorder.y);
    vColor1 = unpackColor(inColor1);
    vBorderColor = unpackColor(inBorderColor);
    vColor3 = unpackColor(inColor3);
    // style 3 keeps its endpoints in color1 and color3, int16 bits in half-pixel units
    vSegment = vec4((ivec4(uvec4(inColor1, inColor3)) << 16) >> 16)*0.5;
    vPixel = vec2(inPosition.xy);

    if(style == 0u)//item has been hidden, move all coords to 0.0f
        gl_Position = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    else
        gl_Position = projection * vec4(float(inPosition.x), float(inPosition.y), float(inPosition.z), 1.0f);

    vec4 behind = projection * vec4(float(inPosition.x), float(inPosition.y), float(inPosition.z) - 0.5, 1.0);
    vBehindDepth = behind.z*0.5 + 0.5;
})";

    auto fragmentTexturedSource=R"(
#version 330 core

out vec4 FragColor;

in vec2 TexCoords;
in vec4 VertexColor;
flat in uint ShaderType;
in vec2 vLocal;
flat in vec2 vHalf;
flat in float vRadius;
flat in float vBorder;
flat in vec4 vColor1;
flat in vec4 vBorderColor;
in vec4 vColor3;
flat in vec4 vSegment;
flat in float vBehindDepth;
in vec2 vPixel;

// Style 2's textures, one per image unit (vRadius picks it). GLSL 3.30 indexes a
// sampler array only with a constant, hence the switch.
uniform sampler2D images[8];
// One Box (x0, y0, x1, y1) per primitive slot. A slot is 2 triangles, and the buffer
// is drawn in chunks, each from index 0 of its own slots, so gl_PrimitiveID >> 1 is
// the slot within the chunk and slotBase the chunk's first slot.
uniform usamplerBuffer clipBoxes;
uniform int slotBase;

// Signed distance to a rounded box centered at the origin (negative inside).
float sdRoundBox(vec2 p, vec2 b, float r)
{
    vec2 q = abs(p) - b + vec2(r);
    return min(max(q.x, q.y), 0.0) + length(max(q, vec2(0.0))) - r;
}

// Rounded-rect mask of styles 4, 6 and 7: applies the border to fill and the
// anti-aliased outer edge to its alpha. Both are a 1 px box filter: the part of
// the pixel on the inner side of the edge, with aa the size of a pixel in units
// of d. A pixel-aligned straight edge is then exactly 1 inside and 0 outside, so
// the outermost pixel is fully covered, two rects that abut meet without a seam
// and an N px border is N whole rows; only a curved edge (a corner arc) gets
// partial coverage.
vec4 roundedRect(vec4 fill)
{
    float r = clamp(vRadius, 0.0, min(vHalf.x, vHalf.y));
    float d = sdRoundBox(vLocal, vHalf, r);
    // d is a true distance in vLocal's units, so a pixel is aa of it: vLocal's
    // screen gradient (1 while vLocal is in device px). Not d's own gradient:
    // fwidth(d) is sqrt(2) too wide across a corner diagonal, and at a square
    // corner, where d=max(qx,qy) has a kink, the 2x2 derivative of the corner
    // pixel can span both axes and come out sqrt(2) too.
    float aa = max(0.5*(length(dFdx(vLocal)) + length(dFdy(vLocal))), 1e-4);
    float shapeA = clamp(0.5 - d/aa, 0.0, 1.0);   // outer edge coverage

    vec3 rgb = fill.rgb;
    if(vBorder > 0.0)
    {
        // -d is the inward distance from the edge; border occupies [0, vBorder].
        float bMix = clamp(0.5 + (vBorder + d)/aa, 0.0, 1.0);
        rgb = mix(fill.rgb, vBorderColor.rgb, bMix);
    }
    return vec4(rgb, fill.a * shapeA);
}

// Signed distance to the axis-aligned box [lo, hi] (negative inside).
float sdBox(vec2 p, vec2 lo, vec2 hi)
{
    vec2 v = abs(p - 0.5*(lo + hi)) - 0.5*(hi - lo);
    return length(max(v, vec2(0.0))) + min(max(v.x, v.y), 0.0);
}

// Signed distance to the outline of [lo, hi], s thick on its inner side.
float sdOutline(vec2 p, vec2 lo, vec2 hi, float s)
{
    return max(sdBox(p, lo, hi), -sdBox(p, lo + vec2(s), hi - vec2(s)));
}

// Distance from p to the segment a-b (for stroked icon glyphs).
float sdSegment(vec2 p, vec2 a, vec2 b)
{
    vec2 pa = p - a;
    vec2 ba = b - a;
    float h = clamp(dot(pa, ba)/dot(ba, ba), 0.0, 1.0);
    return length(pa - ba*h);
}

// The part of a pixel (a unit square of device px) on the inner side of a straight
// edge e px from its centre, n the edge's unit normal: exact for a half-plane. Its
// projection on n is |n.x|+|n.y| wide: the corners give the quadratic ends, the
// middle is linear, and an axis-aligned edge (n = (1,0) or (0,1)) is the plain
// clamp(0.5 + e, 0, 1) of style 4's box filter.
float pixelArea(float e, vec2 n)
{
    vec2 m = abs(n);
    float a = max(m.x, m.y);
    float b = min(m.x, m.y);
    float h1 = 0.5*(a - b);
    float h2 = 0.5*(a + b);

    if(e <= -h2)
        return 0.0;
    if(e >= h2)
        return 1.0;
    if(e < -h1)
        return (e + h2)*(e + h2)/(2.0*a*b);
    if(e > h1)
        return 1.0 - (h2 - e)*(h2 - e)/(2.0*a*b);
    return 0.5 + e/a;
}

// The texel of image unit `unit` at uv, with the gradients taken in uniform control flow.
vec4 imageTexel(int unit, vec2 uv, vec2 dx, vec2 dy)
{
    switch(unit)
    {
    case 0: return textureGrad(images[0], uv, dx, dy);
    case 1: return textureGrad(images[1], uv, dx, dy);
    case 2: return textureGrad(images[2], uv, dx, dy);
    case 3: return textureGrad(images[3], uv, dx, dy);
    case 4: return textureGrad(images[4], uv, dx, dy);
    case 5: return textureGrad(images[5], uv, dx, dy);
    case 6: return textureGrad(images[6], uv, dx, dy);
    case 7: return textureGrad(images[7], uv, dx, dy);
    }
    return vec4(0.0);
}

void main()
{
    // Tested here, discarded at the end: the derivatives below (dFdx/dFdy, fwidth)
    // want the whole quad alive.
    uvec4 clip = texelFetch(clipBoxes, slotBase + (gl_PrimitiveID >> 1));
    float depth = gl_FragCoord.z;
    bool clipped = any(lessThan(vPixel, vec2(clip.xy))) || any(greaterThanEqual(vPixel, vec2(clip.zw)));
    vec2 uvDx = dFdx(TexCoords);
    vec2 uvDy = dFdy(TexCoords);

    if(ShaderType == 0u)
        FragColor = vec4(0.0, 0.0, 0.0, 0.0);
    else if(ShaderType == 1u)
        FragColor = VertexColor;
    else if (ShaderType == 2u)
    {
        // Image: its texture's unit is in vRadius, the draw colour (tint) in vColor1.
        FragColor = imageTexel(int(vRadius + 0.5), TexCoords, uvDx, uvDy) * vColor1;
    }
    else if (ShaderType == 3u)
    {
        // Line: a capsule vBorder px thick around the segment vSegment.xy-vSegment.zw
        // (vLocal is in device px). A pixel is covered by the part of it between the
        // stroke's two edges, each taken as a straight edge across the pixel along
        // the normal from the nearest point of the segment (radial in a cap): the
        // pixel's exact area for the body of the stroke, so a thin slanted line is
        // as bright on every row. drawLine puts the stroke's edges of a horizontal
        // or vertical segment on pixel boundaries, where this is style 4's box
        // filter: whole fully covered rows (columns) and nothing beside them.
        vec2 pa = vLocal - vSegment.xy;
        vec2 ba = vSegment.zw - vSegment.xy;
        float h = clamp(dot(pa, ba)/max(dot(ba, ba), 1e-8), 0.0, 1.0);
        vec2 v = pa - ba*h;
        float d = length(v);
        vec2 n = d > 1e-4 ? v/d
            : (dot(ba, ba) > 1e-8 ? normalize(vec2(-ba.y, ba.x)) : vec2(0.0, 1.0));
        float radius = 0.5*vBorder;
        float a = clamp(pixelArea(radius - d, n) + pixelArea(radius + d, n) - 1.0, 0.0, 1.0);
        FragColor = vec4(VertexColor.rgb, VertexColor.a * a);
        // A partly covered pixel's depth is moved toward the layer behind, up to half
        // a layer as its coverage goes to 0. The segments of a polyline share a z, and
        // with GL_LESS the first one's anti-aliased cap rim would otherwise keep the
        // next one off pixels it covers fully; this way a later segment draws over the
        // pixels an earlier one covers less, still in front of every lower z, while
        // a fully covered pixel keeps its z, so a translucent polyline is blended once
        // there however many of its segments cover it.
        depth = mix(vBehindDepth, gl_FragCoord.z, a);
    }
    else if (ShaderType == 4u)
    {
        // SDF rounded rect: vertical gradient fill + optional border, analytic AA.
        // Vertical gradient: local y runs -half (top) .. +half (bottom).
        float t = clamp(vLocal.y/max(vHalf.y, 1.0)*0.5 + 0.5, 0.0, 1.0);
        FragColor = roundedRect(mix(VertexColor, vColor1, t));
    }
    else if (ShaderType == 6u)
    {
        // Bilinear fill: VertexColor and vColor3 are the top and bottom edge colours
        // at this x (each linear in x, so exact across the triangle split), blended
        // along y as style 4's gradient is.
        float t = clamp(vLocal.y/max(vHalf.y, 1.0)*0.5 + 0.5, 0.0, 1.0);
        FragColor = roundedRect(mix(VertexColor, vColor3, t));
    }
    else if (ShaderType == 7u)
    {
        // Checker composite: VertexColor (with alpha) over 4 px cells of vColor1
        // (light) and vColor3 (dark), counted from the rect's top-left; opaque.
        ivec2 cell = ivec2(floor((vLocal + vHalf)/4.0));
        vec3 checker = ((cell.x + cell.y) & 1) == 0 ? vColor1.rgb : vColor3.rgb;
        FragColor = roundedRect(vec4(mix(checker, VertexColor.rgb, VertexColor.a), 1.0));
    }
    else if (ShaderType == 5u)
    {
        // Icon glyph: vRadius=kind, vBorder=dir (the chevron only).
        int kind = int(vRadius + 0.5);
        float a;

        if(kind <= 1)
        {
            // Normalized local coords in [-1,1], so the stroke scales with the rect.
            vec2 p = vLocal / max(vHalf, vec2(1.0));
            int dir = int(vBorder + 0.5);

            if(dir == 1) p = vec2(p.x, -p.y);       // up
            else if(dir == 2) p = vec2(-p.y, p.x);  // left
            else if(dir == 3) p = vec2(p.y, -p.x);  // right
            // dir 0 == down / as-authored

            float d;
            if(kind == 0)
            {
                // checkmark (two strokes); y is down, so B is the low point.
                vec2 A = vec2(-0.55, 0.0);
                vec2 B = vec2(-0.18, 0.38);
                vec2 C = vec2(0.55, -0.45);
                d = min(sdSegment(p, A, B), sdSegment(p, B, C)) - 0.14;
            }
            else
            {
                // chevron "v" pointing down (rotated by dir above).
                vec2 A = vec2(-0.5, -0.2);
                vec2 B = vec2(0.0, 0.28);
                vec2 C = vec2(0.5, -0.2);
                d = min(sdSegment(p, A, B), sdSegment(p, B, C)) - 0.13;
            }

            float aa = fwidth(d) + 1e-4;
            a = 1.0 - smoothstep(0.0, aa, d);
        }
        else
        {
            // Kinds 2-6 (the window buttons' glyphs and a separator's rule) are
            // measured in device px, with
            // a stroke of the whole px drawIcon packs into the border colour's red
            // byte and roundedRect()'s 1 px box filter. Its aa is constant over the
            // quad (fwidth(d) is not: it changes per 2x2 block), so the glyph is
            // mirror-symmetric pixel for pixel. Every edge of the dash, squares and rule is
            // a whole px from the quad's top-left, so each stroke covers whole rows
            // and columns fully and nothing beside them.
            float s = floor(vBorderColor.r*255.0 + 0.5);
            float aa = max(0.5*(length(dFdx(vLocal)) + length(dFdy(vLocal))), 1e-4);
            vec2 size = 2.0*vHalf;
            vec2 q = vLocal + vHalf;    // px from the quad's top-left
            float side = min(size.x, size.y);
            vec2 o = floor((size - vec2(side))*0.5);  // the glyph's square, centred

            float d;
            if(kind == 2)
            {
                // cross: both diagonals of the centred square, their round caps
                // reaching its edges
                float e = 0.5*side - 0.5*s;
                d = min(sdSegment(vLocal, vec2(-e), vec2(e)),
                        sdSegment(vLocal, vec2(-e, e), vec2(e, -e))) - 0.5*s;
            }
            else if(kind == 3)
            {
                // dash: the square's width, on the row(s) at (the floor of) its middle
                float y0 = o.y + floor((side - s)*0.5);
                d = sdBox(q, vec2(o.x, y0), vec2(o.x + side, y0 + s));
            }
            else if(kind == 4)
            {
                // square outline
                d = sdOutline(q, o, o + vec2(side), s);
            }
            else if(kind == 6)
            {
                // rule: the quad's whole length, on the row(s) (column(s) for dir 1)
                // at (the floor of) its middle across
                if(int(vBorder + 0.5) == 0)
                {
                    float y0 = floor((size.y - s)*0.5);
                    d = sdBox(q, vec2(0.0, y0), vec2(size.x, y0 + s));
                }
                else
                {
                    float x0 = floor((size.x - s)*0.5);
                    d = sdBox(q, vec2(x0, 0.0), vec2(x0 + s, size.y));
                }
            }
            else
            {
                // restore: a front square at the bottom left, and the outline of one
                // offset up and right, only where it is outside the front square
                float off = max(floor(side/5.0 + 0.5), s + 1.0);
                vec2 frontLo = o + vec2(0.0, off);
                vec2 frontHi = o + vec2(side - off, side);
                vec2 backLo = o + vec2(off, 0.0);
                vec2 backHi = o + vec2(side, side - off);
                d = min(sdOutline(q, frontLo, frontHi, s),
                        max(sdOutline(q, backLo, backHi, s), -sdBox(q, frontLo, frontHi)));
            }
            a = clamp(0.5 - d/aa, 0.0, 1.0);
        }
        FragColor = vec4(VertexColor.rgb, VertexColor.a * a);
    }

    // a transparent image texel writes no depth, so what lies behind it still draws;
    // nor does a pixel an icon or a line leaves uncovered (a rule's quad reaches past
    // its control, over its neighbours, and a line's quad is its bounding box), whose
    // coverage beside a stroke's edge is 0 give or take rounding: less than half a
    // step of 8-bit alpha changes no pixel
    if(clipped || (ShaderType == 2u && FragColor.a <= 0.0)
        || ((ShaderType == 3u || ShaderType == 5u) && FragColor.a < 0.5/255.0))
        discard;
    // every other style writes the depth the fixed function would
    gl_FragDepth = depth;
})";

    m_program=loadShaders(vertexSource, fragmentTexturedSource);
    m_projectionUniform=glGetUniformLocation(m_program, "projection");
    m_imagesUniform=glGetUniformLocation(m_program, "images");
    m_clipBoxesUniform=glGetUniformLocation(m_program, "clipBoxes");
    m_slotBaseUniform=glGetUniformLocation(m_program, "slotBase");

    auto textVertexSource=R"(
 #version 330 core

layout (location = 0) in vec3 vertPosition;
layout (location = 1) in vec2 vertTexCoord;
layout (location = 2) in vec4 vertColor;
layout (location = 3) in float vertGamma;

uniform mat4 projection;

out vec2 texCoord;
out vec4 color;
out float gamma;
out vec2 pixel;    // pixel-space position, the clip boxes' space
// The glyph slot. The text is drawn in chunks, each with its attributes pointed at
// its first slot (slotBase) and indexed from there, so gl_VertexID is a vertex of
// the chunk: slot (gl_VertexID >> 2) of it, however its runs are split between
// draws.
uniform int slotBase;
flat out uint slot;

void main()
{
    texCoord=vertTexCoord;
    color=vertColor;
    gamma=vertGamma;
    pixel=vertPosition.xy;
    slot=uint(slotBase)+(uint(gl_VertexID) >> 2);

    gl_Position = projection * vec4(vertPosition, 1.0f);
}
    )";

    auto textFragSource=R"(
#version 330 core

// Dual-source: FragColor is the colour, Coverage what flush() blends an LCD
// atlas's runs with (GL_SRC1_COLOR), one weight per channel. The other atlases'
// SRC_ALPHA blend does not read Coverage.
layout(location = 0, index = 0) out vec4 FragColor;
layout(location = 0, index = 1) out vec4 Coverage;

in vec2 texCoord;
in vec4 color;
in float gamma;
in vec2 pixel;
flat in uint slot;

uniform sampler2D texture1;
// One Box (x0, y0, x1, y1) per glyph slot. Each atlas draws its own runs, some of
// the slots of each chunk, so gl_PrimitiveID does not give the slot; the vertex
// shader does.
uniform usamplerBuffer clipBoxes;
// GLTextMode of the atlas this draw samples: 0 coverage, 1 signed distance field,
// 2 subpixel coverage in r, g, b order, 3 in b, g, r order
uniform int textMode;
// The LCD modes' contrast boost for white text (GWENI_TEXT_CONTRAST); 0 is none
uniform float textContrast;
// the SDF edge's anti-aliasing half-width, in screen pixels
const float SDF_AA = 0.7;
void main()
{
    uvec4 clip = texelFetch(clipBoxes, int(slot));
    bool clipped = any(lessThan(pixel, vec2(clip.xy))) || any(greaterThanEqual(pixel, vec2(clip.zw)));

    float a = 0.0;

    if(textMode == 0)
    {
        a = texture2D(texture1, texCoord).r;

        // Straight alpha: the coverage scales alpha only, and the SRC_ALPHA blend
        // applies it to the colour once. color * a here would apply it twice.
        FragColor = vec4(color.rgb, color.a * pow( a, 1.0/gamma ));
        Coverage = vec4(0.0);
    }
    else if(textMode >= 2)
    {
        // the atlas texel's r, g, b cover the left, middle and right subpixels;
        // on a BGR panel the left one is blue
        vec3 c = texture(texture1, texCoord).rgb;

        if(textMode == 3)
            c = c.bgr;
        c = pow(c, vec3(1.0/gamma));

        // Blending sRGB values, as this framebuffer does, light text over a dark
        // background comes out thinner than dark over light at the same coverage,
        // and its colour fringes stand out from its thin stems. Light text's
        // coverage is raised through DirectWrite's enhanced-contrast curve
        // c*(k+1)/(c*k+1), which keeps 0 and 1 and lifts what is between, by k
        // growing with the text's luminance; black text is left as it is.
        float k = textContrast*dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));

        c = c*(k+1.0)/(c*k+1.0);
        a = (c.r+c.g+c.b)/3.0;

        // dst = colour*coverage + dst*(1-coverage) per channel; the alpha
        // channel as the grayscale path writes it, with the mean coverage
        FragColor = vec4(color.rgb, color.a*a);
        Coverage = vec4(color.a*c, color.a*a);
    }
    else
    {
        // The field is 0.5 on the glyph's edge and changes by 1/(2*spread) per atlas
        // texel; w is how much it changes over one screen pixel here, so the edge is
        // anti-aliased over about a pixel at any size, as style 4's rounded rect is.
        float d = texture(texture1, texCoord).r;
        float w = max(SDF_AA*length(vec2(dFdx(d), dFdy(d))), 1e-4);

        a = smoothstep(0.5-w, 0.5+w, d);
        FragColor = vec4(color.rgb, color.a*a);
        Coverage = vec4(0.0);
    }

    // after the sample, as in the primitive shader: its implicit derivatives want the quad
    if(clipped)
        discard;
    // An SDF glyph's quad reaches the spread past its ink, over its neighbours and
    // the next row: where it draws nothing it must not write depth either.
    if((textMode == 1) && (a <= 0.0))
        discard;
}
    )";

    m_textProgram=loadShaders(textVertexSource, textFragSource);
    m_textProjectionUniform=glGetUniformLocation(m_textProgram, "projection");
    m_textClipBoxesUniform=glGetUniformLocation(m_textProgram, "clipBoxes");
    m_textModeUniform=glGetUniformLocation(m_textProgram, "textMode");
    m_textContrastUniform=glGetUniformLocation(m_textProgram, "textContrast");
    m_textSlotBaseUniform=glGetUniformLocation(m_textProgram, "slotBase");

    // texture1 stays on unit 0; the clip boxes are on unit 1 for both programs, and
    // the primitive program's images on FirstImageUnit on
    glProgramUniform1i(m_program, m_clipBoxesUniform, 1);

    GLint imageUnits[ImageUnits];

    for(size_t i=0; i<ImageUnits; ++i)
        imageUnits[i]=FirstImageUnit+(GLint)i;
    glProgramUniform1iv(m_program, m_imagesUniform, (GLsizei)ImageUnits, imageUnits);
    glProgramUniform1i(m_textProgram, m_textClipBoxesUniform, 1);

    glViewport(m_viewRect.x, m_viewRect.y, m_viewRect.w, m_viewRect.h);
}

void OpenGLCore::setViewRect(const gweni::Rect &viewRect)
{
    m_viewRect=viewRect;

    m_projectionMatrix=glm::ortho(
        static_cast<float>(m_viewRect.x),
        static_cast<float>(m_viewRect.x + m_viewRect.w),
        static_cast<float>(m_viewRect.y + m_viewRect.h),
        static_cast<float>(m_viewRect.y),
        -10000.0f,
        0.0f
    );

    glViewport(m_viewRect.x, m_viewRect.y, m_viewRect.w, m_viewRect.h);
}

void OpenGLCore::begin()
{
    m_frameStats.primitiveSlotsWritten=0;
    m_zIndex=0;
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
}

void OpenGLCore::end()
{
    flush();
}

uint32_t OpenGLCore::primitiveVertexArray(size_t chunk)
{
    while(m_vertexArrays.size() <= chunk)
    {
        // chunk n's vertex 0 is slot n*m_chunkSlots's first vertex
        const size_t base=m_vertexArrays.size()*m_chunkSlots*4*sizeof(Vertex);
        auto at=[base](size_t offset){ return reinterpret_cast<const GLvoid *>(base+offset); };
        GLuint vertexArray=0;

        glCreateVertexArrays(1, &vertexArray);
        glBindVertexArray(vertexArray);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_quadIndexBuffer);

        glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
        GLuint attrib=0;
        glEnableVertexAttribArray(attrib); // Attrib '0' is render type
        glVertexAttribIPointer(attrib++, 1, GL_UNSIGNED_SHORT, sizeof(Vertex), at(offsetof(Vertex, style)));
        glEnableVertexAttribArray(attrib); // Attrib '1' is vertex pos (signed device px, and z)
        glVertexAttribIPointer(attrib++, 3, GL_SHORT, sizeof(Vertex), at(offsetof(Vertex, x)));
        glEnableVertexAttribArray(attrib); // Attrib '2' is texture coord or packeted color0
        glVertexAttribIPointer(attrib++, 2, GL_UNSIGNED_SHORT, sizeof(Vertex), at(offsetof(Vertex, tx)));
        glEnableVertexAttribArray(attrib); // Attrib '3' is SDF local position (signed)
        glVertexAttribIPointer(attrib++, 2, GL_SHORT, sizeof(Vertex), at(offsetof(Vertex, lx)));
        glEnableVertexAttribArray(attrib); // Attrib '4' is SDF radius + border
        glVertexAttribIPointer(attrib++, 2, GL_UNSIGNED_SHORT, sizeof(Vertex), at(offsetof(Vertex, radius)));
        glEnableVertexAttribArray(attrib); // Attrib '5' is SDF gradient color1 (packed)
        glVertexAttribIPointer(attrib++, 2, GL_UNSIGNED_SHORT, sizeof(Vertex), at(offsetof(Vertex, cx)));
        glEnableVertexAttribArray(attrib); // Attrib '6' is SDF border color (packed)
        glVertexAttribIPointer(attrib++, 2, GL_UNSIGNED_SHORT, sizeof(Vertex), at(offsetof(Vertex, bx)));
        glEnableVertexAttribArray(attrib); // Attrib '7' is color3 (packed): bilinear bottom edge / checker dark
        glVertexAttribIPointer(attrib++, 2, GL_UNSIGNED_SHORT, sizeof(Vertex), at(offsetof(Vertex, dx)));

        glBindVertexArray(0);
        m_vertexArrays.push_back(vertexArray);
    }

    return m_vertexArrays[chunk];
}

uint32_t OpenGLCore::textVertexArray(size_t chunk)
{
    while(m_textVertexArrays.size() <= chunk)
    {
        // chunk n's vertex 0 is glyph slot n*m_chunkSlots's first vertex
        const size_t base=m_textVertexArrays.size()*m_chunkSlots*4*sizeof(glyph_vertex_t);
        auto at=[base](size_t offset){ return reinterpret_cast<const GLvoid *>(base+offset); };
        GLuint vertexArray=0;

        glCreateVertexArrays(1, &vertexArray);
        glBindVertexArray(vertexArray);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_textQuadIndexBuffer);

        glBindBuffer(GL_ARRAY_BUFFER, m_textVertexBuffer);
        glEnableVertexAttribArray(0); // Attrib '0' is position
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glyph_vertex_t), at(offsetof(glyph_vertex_t, x)));
        glEnableVertexAttribArray(1); // Attrib '1' is texture coords
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(glyph_vertex_t), at(offsetof(glyph_vertex_t, u)));
        glEnableVertexAttribArray(2); // Attrib '2' is color
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(glyph_vertex_t), at(offsetof(glyph_vertex_t, r)));
        glEnableVertexAttribArray(3); // Attrib '3' is gamma
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(glyph_vertex_t), at(offsetof(glyph_vertex_t, gamma)));
        // freetype-gl's subpixel `shift` is not read: the fragment shader samples one channel.

        glBindVertexArray(0);
        m_textVertexArrays.push_back(vertexArray);
    }

    return m_textVertexArrays[chunk];
}

void OpenGLCore::setIndexChunkSlots(size_t slots)
{
    slots=std::min(std::max<size_t>(slots, 1), kSlotsPerChunk);

    if(slots == m_chunkSlots)
        return;

    m_chunkSlots=slots;

    // their attributes start at the old chunk size's slots
    glBindVertexArray(0);
    for(std::vector<uint32_t> *vertexArrays:{ &m_vertexArrays, &m_textVertexArrays })
    {
        if(vertexArrays->empty())
            continue;
        glDeleteVertexArrays((GLsizei)vertexArrays->size(), vertexArrays->data());
        vertexArrays->clear();
    }

    // and the atlases' draws are split at the old chunks' edges
    for(auto &entry:m_fonts)
        entry.second.drawListStale=true;
}

void OpenGLCore::ensurePrimitiveSlots(size_t primitiveId)
{
    if(m_clippingBoxes.size() < primitiveId+1)
        m_clippingBoxes.resize(primitiveId+1);

    if(m_vertexes.size() < primitiveId*4+4)
        m_vertexes.resize(primitiveId*4+4);
}

void OpenGLCore::readPrimitiveSlot(size_t primitiveId, PrimitiveSlot &slot) const
{
    memcpy(slot.vertexes, &m_vertexes[primitiveId*4], sizeof(slot.vertexes));
    slot.box=m_clippingBoxes[primitiveId];
}

OpenGLCore::MirrorSnapshot OpenGLCore::snapshotMirror() const
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

void OpenGLCore::markPrimitiveChanges(size_t primitiveId, const PrimitiveSlot &before)
{
    bool written=false;

    if(memcmp(before.vertexes, &m_vertexes[primitiveId*4], sizeof(before.vertexes)) != 0)
    {
        m_dirtyVertexSlots.mark(primitiveId);
        written=true;
    }

    if(memcmp(&before.box, &m_clippingBoxes[primitiveId], sizeof(Box)) != 0)
    {
        m_dirtyClipSlots.mark(primitiveId);
        written=true;
    }

    if(written)
        ++m_frameStats.primitiveSlotsWritten;
}

void OpenGLCore::uploadDirty(unsigned int target, uint32_t buffer, size_t &capacityElements,
    const void *data, size_t elementSize, size_t elementCount,
    DirtySlots &dirty, size_t elementsPerSlot)
{
    if(elementCount == 0)
        return;

    if((capacityElements >= elementCount) && dirty.empty())
        return; //device already holds the mirror

    glBindBuffer((GLenum)target, buffer);

    if(capacityElements < elementCount)
    {
        //the mirror outgrew the allocation; resize it and resend everything
        glBufferData((GLenum)target, elementCount*elementSize, data, GL_DYNAMIC_DRAW);
        capacityElements=elementCount;
        dirty.clear();

        ++m_frameStats.uploadCalls;
        m_frameStats.uploadBytes+=elementCount*elementSize;
        return;
    }

    const char *bytes=static_cast<const char *>(data);

    dirty.forEachRun([&](size_t firstSlot, size_t lastSlot)
    {
        size_t first=firstSlot*elementsPerSlot;
        size_t last=std::min(lastSlot*elementsPerSlot, elementCount);

        if(last <= first)
            return;

        glBufferSubData((GLenum)target, first*elementSize, (last-first)*elementSize,
            bytes+(first*elementSize));

        ++m_frameStats.uploadCalls;
        m_frameStats.uploadBytes+=(last-first)*elementSize;
    });

    dirty.clear();
}

void OpenGLCore::uploadDirtyAtlases()
{
    bool bound=false;

    for(auto &entry:m_fonts)
    {
        texture_atlas_t *atlas=entry.second.atlas;

        if(atlas->dirty_y0 >= atlas->dirty_y1)
            continue;

        //full-width rows are contiguous in atlas->data, depth bytes per texel
        assert((atlas->depth == 1) || (atlas->depth == 3));

        size_t rows=atlas->dirty_y1-atlas->dirty_y0;

        if(!bound)
        {
            glActiveTexture(GL_TEXTURE0);
            bound=true;
        }
        glBindTexture(GL_TEXTURE_2D, atlas->id);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, (GLint)atlas->dirty_y0, (GLsizei)atlas->width, (GLsizei)rows,
            (atlas->depth == 3)?GL_RGB:GL_RED, GL_UNSIGNED_BYTE, atlas->data+atlas->dirty_y0*atlas->width*atlas->depth);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

        ++m_frameStats.atlasUploadCalls;
        m_frameStats.atlasUploadBytes+=rows*atlas->width*atlas->depth;

        atlas->dirty_y0=0;
        atlas->dirty_y1=0;
    }

    //setTexture() skips the bind when m_currentTexture already matches
    if(bound)
        glBindTexture(GL_TEXTURE_2D, m_currentTexture);
}

void OpenGLCore::flush()
{
    m_frameStats.uploadCalls=0;
    m_frameStats.uploadBytes=0;
    m_frameStats.drawCalls=0;
    m_frameStats.textDraws=0;
    m_frameStats.atlasUploadCalls=0;
    m_frameStats.atlasUploadBytes=0;
    m_frameStats.textSlotsWritten=m_dirtyTextVertexSlots.count();
    //a glyph slot whose clip box changed and its vertexes did not is written too
    for(size_t slot: m_dirtyTextClipSlots.slots())
    {
        if(!m_dirtyTextVertexSlots.marked(slot))
            ++m_frameStats.textSlotsWritten;
    }
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
        //copy only the primitive slots that changed since the last frame
        uploadDirty(GL_ARRAY_BUFFER, m_vertexBuffer, m_vertexBufferCapacity,
            m_vertexes.data(), sizeof(Vertex), m_vertexes.size(), m_dirtyVertexSlots, 4);

        const size_t clipCapacity=m_clipBufferCapacity;

        uploadDirty(GL_TEXTURE_BUFFER, m_clipBuffer, m_clipBufferCapacity,
            m_clippingBoxes.data(), sizeof(Box), m_clippingBoxes.size(), m_dirtyClipSlots, 1);

        //a regrow re-specified the data store; attach it again (cheap, and only then)
        if(m_clipBufferCapacity != clipCapacity)
        {
            glBindTexture(GL_TEXTURE_BUFFER, m_clipTexture);
            glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA16UI, m_clipBuffer);
        }

        //draw primitives
        glUseProgram(m_program);

        glProgramUniformMatrix4fv(
            m_program,
            m_projectionUniform,
            1,
            GL_FALSE,
            glm::value_ptr(m_projectionMatrix)
        );

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_BUFFER, m_clipTexture);
        for(size_t i=0; i<ImageUnits; ++i)
        {
            glActiveTexture(GL_TEXTURE0+FirstImageUnit+(GLenum)i);
            glBindTexture(GL_TEXTURE_2D, m_imageUnits[i]);
        }
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_currentTexture);

        // One draw per chunk of m_chunkSlots slots, in slot order as a single draw
        // over the whole buffer would go, so blending and depth come out the same.
        // Each draws its slots, hidden ones included, as 4 vertexes and 6 indexes
        // (2 triangles) apiece from the start of the static quad index buffer, with
        // the chunk's vertex array pointing the attributes at its first slot; the
        // fragment shader's slot is then slotBase + (gl_PrimitiveID >> 1).
        const size_t slots=m_vertexes.size()/4;

        for(size_t first=0; first<slots; first+=m_chunkSlots)
        {
            const size_t count=std::min(slots-first, m_chunkSlots);

            glBindVertexArray(primitiveVertexArray(first/m_chunkSlots));
            glProgramUniform1i(m_program, m_slotBaseUniform, (GLint)first);
            glDrawElements(GL_TRIANGLES, (GLsizei)(count*6), (GLenum)GL_UNSIGNED_SHORT, 0);
            ++m_frameStats.drawCalls;
        }

        glBindVertexArray(0);
    }

    if(!m_textVertexes.empty())
    {
        //copy only the glyph slots that changed since the last frame
        uploadDirty(GL_ARRAY_BUFFER, m_textVertexBuffer, m_textVertexBufferCapacity,
            m_textVertexes.data(), sizeof(glyph_vertex_t), m_textVertexes.size(), m_dirtyTextVertexSlots, 4);

        const size_t clipCapacity=m_textClipBufferCapacity;

        uploadDirty(GL_TEXTURE_BUFFER, m_textClipBuffer, m_textClipBufferCapacity,
            m_textClipBoxes.data(), sizeof(Box), m_textClipBoxes.size(), m_dirtyTextClipSlots, 1);

        if(m_textClipBufferCapacity != clipCapacity)
        {
            glBindTexture(GL_TEXTURE_BUFFER, m_textClipTexture);
            glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA16UI, m_textClipBuffer);
        }

        //draw text
        glUseProgram(m_textProgram);

        glProgramUniformMatrix4fv(
            m_textProgram,
            m_textProjectionUniform,
            1,
            GL_FALSE,
            glm::value_ptr(m_projectionMatrix)
        );

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_BUFFER, m_textClipTexture);
        glActiveTexture(GL_TEXTURE0);

        // Each atlas draws the runs laid out in its font: one draw per index chunk
        // its runs reach (one per atlas until the glyph slots outgrow a chunk),
        // none for an atlas with nothing to show. A chunk's draw reads the static
        // quad indexes from its own first slot, through its vertex array, and the
        // vertex shader's slot is slotBase plus the slot within the chunk. The
        // atlases go in the same order, and each one's chunks in slot order, as the
        // one draw per atlas over the whole buffer did. The slots are not moved, so
        // the upload above stays per dirty slot.
        //
        // An LCD atlas's runs blend each channel by its own coverage, the fragment
        // shader's second output; the blend in place before the first of them is
        // put back for the other atlases and after the pass.
        GLint blend[4]={0, 0, 0, 0};
        bool lcdBlend=false;

        glProgramUniform1f(m_textProgram, m_textContrastUniform, m_textContrast);

        for(auto &entry:m_fonts)
        {
            GLFontData &fontData=entry.second;

            if(fontData.drawListStale)
                buildTextDrawList(fontData);
            if(fontData.drawChunks.empty())
                continue;

            const bool lcd=(fontData.mode == GLTextMode::LcdRGB) || (fontData.mode == GLTextMode::LcdBGR);

            if(lcd && !lcdBlend)
            {
                glGetIntegerv(GL_BLEND_SRC_RGB, &blend[0]);
                glGetIntegerv(GL_BLEND_DST_RGB, &blend[1]);
                glGetIntegerv(GL_BLEND_SRC_ALPHA, &blend[2]);
                glGetIntegerv(GL_BLEND_DST_ALPHA, &blend[3]);
                glBlendFunc(GL_SRC1_COLOR, GL_ONE_MINUS_SRC1_COLOR);
                lcdBlend=true;
            }
            else if(!lcd && lcdBlend)
            {
                glBlendFuncSeparate(blend[0], blend[1], blend[2], blend[3]);
                lcdBlend=false;
            }

            glBindTexture(GL_TEXTURE_2D, fontData.atlas->id);
            glProgramUniform1i(m_textProgram, m_textModeUniform, (GLint)fontData.mode);
            for(const GLTextChunkDraw &draw:fontData.drawChunks)
            {
                glBindVertexArray(textVertexArray(draw.chunk));
                glProgramUniform1i(m_textProgram, m_textSlotBaseUniform, (GLint)(draw.chunk*m_chunkSlots));
                glMultiDrawElements(GL_TRIANGLES, draw.counts.data(), (GLenum)GL_UNSIGNED_SHORT,
                    draw.offsets.data(), (GLsizei)draw.counts.size());
                ++m_frameStats.drawCalls;
                ++m_frameStats.textDraws;
            }
        }

        if(lcdBlend)
            glBlendFuncSeparate(blend[0], blend[1], blend[2], blend[3]);

        glBindVertexArray(0);
    }

    glBindVertexArray(0);

    glFlush();

    ++m_frameStats.frameIndex;

    if(m_uploadStats)
    {
        //qualified: gweni::printf(String&, ...) would otherwise hide the C one
        ::printf("[upload] frame %zu: %zu calls, %zu bytes (mirror %zu bytes), atlas %zu calls, %zu bytes\n",
            m_frameStats.frameIndex, m_frameStats.uploadCalls, m_frameStats.uploadBytes,
            m_frameStats.mirrorBytes, m_frameStats.atlasUploadCalls, m_frameStats.atlasUploadBytes);
        fflush(stdout);
    }

//    // Binding VAO
//    glBindVertexArray(m_vertexArray);
//
//    // Binding VBO
//    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
//
//    // Loading data into VBO
//    glNamedBufferData(
//        m_vertexBuffer,
//        m_vertexes.size() * sizeof(Vertex),
//        m_vertexes.data(),
//        GL_DYNAMIC_DRAW
//    );
//
//    // Binding VBO to VAO
//    glVertexArrayVertexBuffer(
//        m_vertexArray,
//        0,
//        m_vertexBuffer,
//        0,
//        sizeof(Vertex)
//    );
//
//    // Enabling attributes
//    glEnableVertexArrayAttrib(m_vertexArray, 0); // Position
//    glEnableVertexArrayAttrib(m_vertexArray, 1); // UV
//    glEnableVertexArrayAttrib(m_vertexArray, 2); // Color
//
//    // Setting format
//    glVertexArrayAttribFormat(m_vertexArray, 0, 3, GL_FLOAT, GL_FALSE, static_cast<GLuint>(offsetof(Vertex, pos)));
//    glVertexArrayAttribFormat(m_vertexArray, 1, 2, GL_FLOAT, GL_FALSE, static_cast<GLuint>(offsetof(Vertex, uv)));
//    glVertexArrayAttribFormat(m_vertexArray, 2, 4, GL_FLOAT, GL_FALSE, static_cast<GLuint>(offsetof(Vertex, color)));
//
//    // Binding vertex array to buffer
//    glVertexArrayAttribBinding(m_vertexArray, 0, 0);
//    glVertexArrayAttribBinding(m_vertexArray, 1, 0);
//    glVertexArrayAttribBinding(m_vertexArray, 2, 0);
//
//    // Program
//    glUseProgram(m_program);
//
//    // Loading matricies
//    glProgramUniformMatrix4fv(
//        m_program,
//        glGetUniformLocation(m_program, "projection"),
//        1,
//        GL_FALSE,
//        glm::value_ptr(m_projectionMatrix)
//    );
//
//    // Drawing
//    glDrawArrays(
//        GL_TRIANGLES,
//        0,
//        static_cast<GLsizei>(m_vertexes.size())
//    );
//
//    // Unbinding
//    glBindVertexArray(0);
//    glUseProgram(0);
//
//    // Clearing
//    m_activeProgram=0;
//    m_vertexes.clear();
//    glFlush();
}

void OpenGLCore::setVertex(Vertex &vertex, uint8_t type, int x, int y, int z, float u, float v)
{
    vertex.style=type;

    // a corner outside int16_t wraps here, and cullOutOfRangeQuad then collapses the quad
    vertex.x=(int16_t)x;
//    vertex.y=m_viewRect.h - y;
    vertex.y=(int16_t)y;
    vertex.z=(int16_t)z;// m_activeProgram;

//    if(m_activeProgram == 1)
    {
        vertex.tx=u;
        vertex.ty=v;
    }

    // SDF fields are only used by styles 3-7 (drawLine, writeRoundedQuad, drawIcon); keep them 0 otherwise.
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

void OpenGLCore::cullOutOfRangeQuad(size_t primitiveId, const gweni::Rect &rect)
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

void OpenGLCore::addVert(int x, int y, float u, float v)
{
    // OpenGL origin is bottom-left. Gweni origin is top-left.

//    Vertex vertex;
//
//    setVertex(vertex, x, y, u, v);
//    m_vertexes.emplace_back(vertex);
}

void OpenGLCore::setTexture(unsigned int texture)
{
    if(texture == 0 && !m_texturesOn)
        return;

    if(texture == 0)
    {
        m_texturesOn=false;
    }
    else
    {
        m_texturesOn=true;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    m_currentTexture=texture;

    glm::uvec2 textureSize(0, 0);

    if(texture < m_textureSizes.size())
        textureSize=m_textureSizes[texture];

    m_currentTextureSize.x=textureSize.x;
    m_currentTextureSize.y=textureSize.y;
}

void OpenGLCore::drawFilledRect(size_t primitiveId, gweni::Rect rect, int zIndex)
{
    m_activeProgram=0;
    m_zIndex++;

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

void OpenGLCore::drawRoundedRect(size_t primitiveId, gweni::Rect rect, float radius, float border,
    gweni::Color fillTop, gweni::Color fillBottom, gweni::Color borderColor, int zIndex)
{
    writeRoundedQuad(primitiveId, rect, zIndex, 4, radius, border, fillTop, fillTop,
        fillBottom, gweni::Color(0, 0, 0, 0), gweni::Color(0, 0, 0, 0), borderColor);
}

void OpenGLCore::drawBilinearRect(size_t primitiveId, gweni::Rect rect, int zIndex,
    gweni::Color c00, gweni::Color c10, gweni::Color c01, gweni::Color c11,
    float radius, float border, gweni::Color borderColor)
{
    // top edge in color0, bottom edge in color3, each per side
    writeRoundedQuad(primitiveId, rect, zIndex, 6, radius, border, c00, c10,
        gweni::Color(0, 0, 0, 0), c01, c11, borderColor);
}

void OpenGLCore::drawCheckerRect(size_t primitiveId, gweni::Rect rect, int zIndex,
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

void OpenGLCore::writeRoundedQuad(size_t primitiveId, gweni::Rect rect, int zIndex, uint8_t style,
    float radius, float border, gweni::Color color0Left, gweni::Color color0Right,
    gweni::Color color1, gweni::Color color3Left, gweni::Color color3Right,
    gweni::Color borderColor)
{
    m_activeProgram=0;
    m_zIndex++;

    translate(rect);

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
    // least one, so it stays crisp at a fractional scale.
    const long limit=std::numeric_limits<uint16_t>::max();
    const uint16_t rad=(uint16_t)std::min(std::max((long)snap(radius), 0L), limit);
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

void OpenGLCore::drawIcon(size_t primitiveId, gweni::Rect rect, int kind, int direction,
    gweni::Color color, int zIndex)
{
    m_activeProgram=0;
    m_zIndex++;

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
        v.bx=(uint16_t)(stroke<<8);    // kinds 2-6: the stroke in px, the "red" byte
    };

    setIcon(vertexIndex++, x0, y0, -hw, -hh);
    setIcon(vertexIndex++, x1, y0, +hw, -hh);
    setIcon(vertexIndex++, x0, y1, -hw, +hh);
    setIcon(vertexIndex++, x1, y1, +hw, +hh);

    cullOutOfRangeQuad(primitiveId, rect);
    markPrimitiveChanges(primitiveId, before);
}

void OpenGLCore::drawLine(size_t primitiveId, gweni::PointF p0, gweni::PointF p1,
    float thickness, int zIndex)
{
    m_activeProgram=0;
    m_zIndex++;

    size_t vertexIndex=primitiveId*4;

    ensurePrimitiveSlots(primitiveId);

    PrimitiveSlot before;

    readPrimitiveSlot(primitiveId, before);

    m_clippingBoxes[primitiveId]=makeClipBox();

    // Whole device px, like a border, so the stroke can sit on the pixel grid.
    const int stroke=std::min(std::max(1, snap(thickness)), 4096);
    const gweni::Point &offset=getRenderOffset();

    // The device px where the stroke across an endpoint coordinate starts: its
    // centre at v*scale, moved to put both its edges on pixel boundaries. For an
    // even stroke that is snap(v)-stroke/2, the edge translate() snaps v to; for an
    // odd one, the pixel v*scale falls in is its middle row (column).
    auto strokeStart=[&](float v, int origin)
    {
        const double centre=(double(v)+origin)*scale();
        const double start=std::floor(centre-0.5*stroke+0.5);

        return (int64_t)std::max(std::min(start, 1e9), -1e9);
    };
    const int64_t ax=strokeStart(p0.x, offset.x);
    const int64_t ay=strokeStart(p0.y, offset.y);
    const int64_t bx=strokeStart(p1.x, offset.x);
    const int64_t by=strokeStart(p1.y, offset.y);

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

    auto setLine=[&](size_t vi, int64_t px, int64_t py, int64_t lx, int64_t ly)
    {
        Vertex &v=m_vertexes[vi];

        setVertex(v, 3, (int)px, (int)py, zIndex, m_color.rg(), m_color.ba());
        v.lx=(int16_t)lx;
        v.ly=(int16_t)ly;
        v.border=(uint16_t)stroke;
        v.cx=a0; v.cy=a1;
        v.dx=b0; v.dy=b1;
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

void OpenGLCore::setDrawColor(gweni::Color color)
{
    m_color=color;
}

// Every primitive and glyph slot is stamped with the clipRegion() current when it is
// drawn (makeClipBox), and the shaders discard outside it, so there is no clipping
// state to switch on and off.
void OpenGLCore::startClip()
{}

void OpenGLCore::endClip()
{}

OpenGLCore::Box OpenGLCore::makeClipBox() const
{
    const gweni::Rect &clip=clipRegion();

    // snapped as translate() snaps a rect's edges, so a box edge lands where a quad
    // drawn over the same rect has its edge
    long x0=snap(float(clip.x));
    long y0=snap(float(clip.y));
    long x1=snap(float(clip.x+clip.w));
    long y1=snap(float(clip.y+clip.h));

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

void OpenGLCore::generatePrimitive(size_t &primitiveId)
{
    primitiveId=getPrimitiveId(nullptr);

    //resize indexes/vertexes to cover the primitive
    ensurePrimitiveSlots(primitiveId);
}

void OpenGLCore::releasePrimitive(size_t primitiveId)
{
    hidePrimitive(primitiveId);
    releasePrimitiveId(primitiveId);
}

void OpenGLCore::drawTexturedRect(const gweni::Texture &texture, size_t primitiveId, gweni::Rect rect, int zIndex,
    float u1, float v1, float u2, float v2)
{
    if(!ensureTexture(texture))
        return drawMissingImage(primitiveId, rect, zIndex);

    GLTextureData &texData=*m_textureData[m_lastTexture];
    const size_t unit=imageUnit(texData.m_textureId);

    if(unit == ImageUnits)
    {
        if(!m_imageUnitsFullLogged)
        {
            gweni::log::write(log::Level::Error, "drawTexturedRect: more than %zu textures in use, "
                "drawing the missing image", ImageUnits);
            m_imageUnitsFullLogged=true;
        }
        return drawMissingImage(primitiveId, rect, zIndex);
    }

    translate(rect);

    m_activeProgram=1;

    size_t vertexIndex=primitiveId*4;

    ensurePrimitiveSlots(primitiveId);

    PrimitiveSlot before;

    readPrimitiveSlot(primitiveId, before);

    m_clippingBoxes[primitiveId]=makeClipBox();

#ifdef DEBUG_RENDER
    std::cout<<"Rendering: "<<primitiveId<<"("<<rect.x<<", "<<rect.y<<", "<<rect.w<<", "<<rect.h<<" : "<<zIndex<<")\n";
#endif//DEBUG_RENDER

    // UVs as 0..65535 for 0..1, the image unit and the tint (the draw colour)
    auto uv=[](float t)
    {
        return (float)std::lround(std::min(std::max(t, 0.0f), 1.0f)*65535.0f);
    };
    auto setImageVertex=[&](size_t vi, int x, int y, float u, float v)
    {
        Vertex &vertex=m_vertexes[vi];

        setVertex(vertex, 2, x, y, zIndex, uv(u), uv(v));
        vertex.radius=(uint16_t)unit;
        vertex.cx=m_color.rg();
        vertex.cy=m_color.ba();
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

size_t OpenGLCore::imageUnit(unsigned int textureName)
{
    size_t freeUnit=ImageUnits;

    for(size_t i=0; i<ImageUnits; ++i)
    {
        if(m_imageUnits[i] == textureName)
            return i;
        if((m_imageUnits[i] == 0) && (freeUnit == ImageUnits))
            freeUnit=i;
    }

    if(freeUnit != ImageUnits)
        m_imageUnits[freeUnit]=textureName;
    return freeUnit;
}

void OpenGLCore::showPrimitive(int style, size_t primitiveId)
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

void OpenGLCore::hidePrimitive(size_t primitiveId)
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

gweni::Color OpenGLCore::pixelColor(const gweni::Texture &texture, unsigned int x, unsigned int y,
    const gweni::Color &col_default)
{
    if(!ensureTexture(texture))
        return col_default;

    GLTextureData &texData=*m_textureData[m_lastTexture];

    static const unsigned int iPixelSize=sizeof(unsigned char) * 4;

    if(texData.readable)
    {
        unsigned char *pPixel=texData.m_readData.data() + (x + (y * static_cast<unsigned int>(texData.width))) * iPixelSize;
        return gweni::Color(pPixel[0], pPixel[1], pPixel[2], pPixel[3]);
    }

    setTexture(texData.m_textureId);
    //    texData.m_readData = deleted_unique_ptr<unsigned char>(new unsigned char[texData.width * texData.height * iPixelSize], [](unsigned char* mem) { if (mem) delete[](mem); });
    texData.m_readData.resize(texData.width * texData.height * iPixelSize);
    texData.readable=true;

    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData.m_readData.data());

    unsigned char *pPixel=texData.m_readData.data() + (x + (y * static_cast<unsigned int>(texData.width))) * iPixelSize;
    return gweni::Color(pPixel[0], pPixel[1], pPixel[2], pPixel[3]);
}

void OpenGLCore::ensureTextSlots(size_t glyphSlots)
{
    if(m_textVertexes.size() < glyphSlots*4)
        m_textVertexes.resize(glyphSlots*4);
    if(m_textClipBoxes.size() < glyphSlots)
        m_textClipBoxes.resize(glyphSlots);
}

void OpenGLCore::addFreeTextRun(size_t start, size_t capacity)
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

void OpenGLCore::freeTextRun(TextRun &run)
{
    if(run.capacity == 0)
        return;

    // Out of its font's draw list before handing it back: a freed run must draw
    // nothing until somebody allocates its slots again.
    run.glyphs=0;
    markTextDrawList(run);
    addFreeTextRun(run.start, run.capacity);

    run.start=0;
    run.capacity=0;
}

void OpenGLCore::allocateTextRun(TextRun &run, size_t glyphs)
{
    if(run.capacity >= glyphs)
    {
        // Keeps the run where it is; the tail past `glyphs` is not in the draw
        // list (markTextDrawList), so a string that shrinks leaves no ghost glyphs.
        run.glyphs=glyphs;
        return;
    }

    freeTextRun(run);

    // Best fit: the smallest free block that holds the string, split if it is
    // larger. Blocks are merged as they are freed, so the list stays short and
    // a linear scan is enough.
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

        if(run.capacity > glyphs)
        {
            addFreeTextRun(run.start+glyphs, run.capacity-glyphs);
            run.capacity=glyphs;
        }
    }
    else
    {
        run.start=m_textSlotsUsed;
        run.capacity=glyphs;
        m_textSlotsUsed+=glyphs;
    }

    run.glyphs=glyphs;
    ensureTextSlots(run.start+run.capacity);
}

void OpenGLCore::markTextDrawList(const TextRun &run)
{
    // what the run draws, glyphs [start, start+glyphs) unless hidden, changed
    if(run.fontData)
        run.fontData->drawListStale=true;
}

void OpenGLCore::markTextVertexes(const TextRun &run)
{
    for(size_t i=0; i<run.glyphs; ++i)
        m_dirtyTextVertexSlots.mark(run.start+i);
}

void OpenGLCore::writeTextClipBoxes(const TextRun &run, const Box &box)
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

void OpenGLCore::setTextRunFont(size_t textId, GLFontData *fontData)
{
    TextRun &run=m_textRuns[textId];

    if(run.fontData == fontData)
        return;

    GLFontData *previous=run.fontData;

    if(previous)
    {
        previous->textRuns.erase(textId);
        previous->drawListStale=true;
    }

    run.fontData=fontData;

    // the last run has left an SDF atlas freeFont kept for it
    if(previous && previous->freed && previous->textRuns.empty())
    {
        for(auto it=m_fonts.begin(); it != m_fonts.end(); ++it)
        {
            if(&it->second != previous)
                continue;
            if(m_lastFont == &(*it))
                m_lastFont=nullptr;
            m_fonts.erase(it);
            break;
        }
    }

    if(fontData)
    {
        fontData->textRuns.insert(textId);
        fontData->drawListStale=true;
    }
}

void OpenGLCore::buildTextDrawList(GLFontData &fontData)
{
    // (first glyph slot, glyph count) of each run that draws something
    std::vector<std::pair<size_t, size_t>> ranges;

    ranges.reserve(fontData.textRuns.size());
    for(size_t textId:fontData.textRuns)
    {
        const TextRun &run=m_textRuns[textId];

        if(!run.hidden && (run.glyphs > 0))
            ranges.emplace_back(run.start, run.glyphs);
    }

    // in slot order, as the single draw over the whole buffer went
    std::sort(ranges.begin(), ranges.end());

    fontData.drawChunks.clear();

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

            if(fontData.drawChunks.empty() || (fontData.drawChunks.back().chunk != chunk))
            {
                fontData.drawChunks.emplace_back();
                fontData.drawChunks.back().chunk=chunk;
            }

            GLTextChunkDraw &draw=fontData.drawChunks.back();

            draw.counts.push_back(static_cast<int>((pieceEnd-start)*6));
            draw.offsets.push_back(reinterpret_cast<const void *>((start-chunkStart)*6*sizeof(uint16_t)));
            start=pieceEnd;
        }
    }

    fontData.drawListStale=false;
}

void OpenGLCore::generateTextPrimitive(size_t &primitiveId)
{
    primitiveId=getTextId(nullptr);

    if(m_textRuns.size() < primitiveId+1)
        m_textRuns.resize(primitiveId+1);
}

void OpenGLCore::releaseTextPrimitive(size_t primitiveId)
{
    if(primitiveId < m_textRuns.size())
    {
        TextRun &run=m_textRuns[primitiveId];

        // The run goes back on the free list and the bookkeeping is reset, so a
        // reused text id cannot take renderText's "string hasn't changed" path
        // and inherit the dead control's glyphs.
        freeTextRun(run);
        setTextRunFont(primitiveId, nullptr);
        run=TextRun();
    }

    releaseTextId(primitiveId);
}

void OpenGLCore::renderText(size_t textId, const gweni::Font &font, gweni::Point pos, int zIndex,
    const gweni::String &text)
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

    GLFontData &fontData=m_lastFont->second;

    // Same string, font, effective size and colour, laid out in this font's atlas
    // (not one freed since): the glyphs are the ones already there, at most offset.
    // Anything else falls through to the full rebuild; a new scale() is a new px, and
    // in an SDF font the same atlas.
    if((run.text == text) && (run.font == font) && (run.px == px) && (run.color == color)
        && (run.fontData == &fontData))
    {
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

    if(fontData.mode == GLTextMode::Sdf)
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

    std::vector<float> shifts;

    measuredPenShifts(font, fontData, px, text, shifts);

    // hidden is left as it is: a hidden run stays out of its font's draw list
    run.text=text;
    run.position=pos;
    run.zIndex=zIndex;
    run.font=font;
    run.px=px;
    run.color=color;
    setTextRunFont(textId, &fontData);
    allocateTextRun(run, glyphCount);

    const float zDelta=1.0f/(glyphCount+1);
    float zOffset=zDelta;

#ifdef DEBUG_RENDER
    std::cout<<"Regenerating text run: ("<<pos.x<<", "<<pos.y<<" : "<<zIndex<<") "<<text<<"\n";
#endif//DEBUG_RENDER

    for(size_t i=0; i<glyphCount; ++i)
    {
        const size_t vertIndex=(run.start+i)*4;
        const float shift=shifts.empty()?0.0f:shifts[i];
        // Each glyph of a run sits a fraction of a z unit in front of the one
        // before it, so the GL_LESS depth test cannot reject the overlap between
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

            vertex=source;
            vertex.x=pos.x+source.x+shift;
            vertex.y=pos.y-source.y;
            vertex.z=z;
        }
    }

    markTextDrawList(run);
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

void OpenGLCore::textPens(GLFontData &fontData, float px, const gweni::String &text,
    std::vector<float> &pens)
{
    ftgl::texture_font_t *font=fontData.textureFont;
    const bool sdf=(fontData.mode == GLTextMode::Sdf);
    const float s=sdf?px/fontData.baseSize:1.0f;
    float pen=0.0f;
    const char *previous=nullptr;

    pens.clear();
    // the walk of layoutSdfText, or of freetype-gl's text_buffer_add_text
    for(size_t i=0; i < text.size(); i+=std::max<size_t>(1, ftgl::utf8_surrogate_len(text.c_str()+i)))
    {
        const char *current=text.c_str()+i;

        if(*current == '\n')
        {
            pen=0.0f;
            // text_buffer_add_text kerns the next glyph against the '\n'
            previous=sdf?nullptr:current;
            continue;
        }

        ftgl::texture_glyph_t *glyph=ftgl::texture_font_get_glyph(font, current);

        if(glyph == nullptr)
        {
            if(!sdf)
                previous=current;
            continue;
        }

        if(previous && font->kerning)
            pen+=ftgl::texture_glyph_get_kerning(glyph, previous)*s;

        // a coverage glyph's quad sits at the pen rounded to a whole pixel
        pens.push_back(sdf?pen:roundf(pen));
        pen+=glyph->advance_x*s;
        previous=current;
    }
}

void OpenGLCore::measuredPenShifts(const gweni::Font &font, GLFontData &drawn, float px,
    const gweni::String &text, std::vector<float> &shifts)
{
    shifts.clear();
    if(px == font.size)
        return;

    // An SDF font lays a run out linearly in px, so measured at font.size and drawn
    // at px its pens differ by the scale alone. So does an LCD font, whose pens are
    // the unhinted advances and kerning, to within its 1/3 px quantisation.
    if(resolveFont(font, font.size).mode == GLTextMode::Sdf)
        return;
    if((drawn.mode == GLTextMode::LcdRGB) || (drawn.mode == GLTextMode::LcdBGR))
        return;

    std::pair<const GLFontKey, GLFontData> *drawFont=m_lastFont;

    if(!ensureFont(font, font.size))
    {
        m_lastFont=drawFont;
        return;
    }

    GLFontData &measured=m_lastFont->second;
    std::vector<float> measuredPens, drawnPens;

    m_lastFont=drawFont;
    textPens(measured, font.size, text, measuredPens);
    textPens(drawn, px, text, drawnPens);

    // a glyph one font has and the other has not: left as drawn
    if(measuredPens.size() != drawnPens.size())
        return;

    const float scale=px/font.size;

    shifts.resize(drawnPens.size());
    for(size_t i=0; i < drawnPens.size(); ++i)
    {
        const float target=measuredPens[i]*scale;

        // a coverage glyph stays on whole pixels
        shifts[i]=((drawn.mode == GLTextMode::Sdf)?target:roundf(target))-drawnPens[i];
    }
}

gweni::Point OpenGLCore::layoutSdfText(GLFontData &fontData, float px, const gweni::String &text,
    const gweni::Color &color, std::vector<ftgl::glyph_vertex_t> *vertexes)
{
    ftgl::texture_font_t *font=fontData.textureFont;
    const float s=px/fontData.baseSize;
    const float r=color.r/255.0f, g=color.g/255.0f, b=color.b/255.0f, a=color.a/255.0f;

    // The baseline sits a whole number of pixels below the line's top, and the run's
    // origin is a whole pixel, so a run moved by a sub-pixel amount cannot shimmer.
    // Within the line the glyphs keep their fractional, unhinted pen positions.
    const float ascender=roundf(font->ascender*s);
    const float lineStep=roundf((font->ascender-font->descender)*s);
    float baseline=-ascender;
    float pen=0.0f;
    float width=0.0f;
    int lines=1;
    const char *previous=nullptr;

    //one codepoint at a time, as text_buffer_add_text goes
    for(size_t i=0; i < text.size(); i+=std::max<size_t>(1, ftgl::utf8_surrogate_len(text.c_str()+i)))
    {
        const char *current=text.c_str()+i;

        if(*current == '\n')
        {
            width=std::max(width, pen);
            pen=0.0f;
            baseline-=lineStep;
            ++lines;
            previous=nullptr;
            continue;
        }

        ftgl::texture_glyph_t *glyph=ftgl::texture_font_get_glyph(font, current);

        if(glyph == nullptr)
            continue;

        if(previous && font->kerning)
            pen+=ftgl::texture_glyph_get_kerning(glyph, previous)*s;

        if(vertexes)
        {
            // the glyph's field, its spread padding included, scaled about the pen
            const float x0=pen+glyph->offset_x*s;
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

        pen+=glyph->advance_x*s;
        previous=current;
    }
    width=std::max(width, pen);

    return gweni::Point((int)ceil(width), (int)ceil(font->height*s+(lines-1)*lineStep));
}

void OpenGLCore::showText(size_t textId)
{
    if(textId >= m_textRuns.size())
        return;

    TextRun &run=m_textRuns[textId];

    if(!run.hidden)
        return;

#ifdef DEBUG_RENDER
    std::cout<<"Showing text: "<<run.text<<"\n";
#endif//DEBUG_RENDER

    run.hidden=false;
    markTextDrawList(run);
}

void OpenGLCore::hideText(size_t textId)
{
    if(textId >= m_textRuns.size())
        return;

    TextRun &run=m_textRuns[textId];

    if(run.hidden)
        return;

#ifdef DEBUG_RENDER
    std::cout<<"Hiding text: "<<run.text<<"\n";
#endif//DEBUG_RENDER

    run.hidden=true;
    markTextDrawList(run);
}

// The size is in canvas (unscaled) units at any scale(): the text is measured at
// font.size, in the font it is drawn in at scale 1, so a layout is the same at every
// scale, and renderText draws it at font.size*scale() from the scaled origin. This
// deliberately differs from the legacy OpenGL and Software renderers, whose
// measureText returns scaled pixels (GWEN's long-standing inconsistency).
gweni::Point OpenGLCore::measureText(const gweni::Font &font, const gweni::String &text)
{
    if(!ensureFont(font, font.size))
        return gweni::Point(0, 0);

    GLFontData &fontData=m_lastFont->second;

    // In an SDF font: the same layout renderText draws, so sizeToContents fits it
    if(fontData.mode == GLTextMode::Sdf)
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

    ftgl::text_buffer_t *textBuffer=ftgl::text_buffer_new();
    ftgl::vec2 pen={0.f, 0.f};

    //codepoints, as in renderText
    ftgl::text_buffer_add_text(textBuffer, &pen, &fontData.markup, text.c_str(), ftgl::utf8_strlen(text.c_str()));

    ftgl::vector_t *vertices=textBuffer->buffer->vertices;
    float *verticesBuffer=(float *)vertices->items;

    assert(vertices->item_size == sizeof(ftgl::glyph_vertex_t));

    // The pen ends at the sum of the glyph advances (plus kerning), which is the
    // width the next string would start at. The right edge of the last glyph's
    // ink is a different and smaller quantity, and is not what a control laying
    // text out wants.
    Point sz((int)ceil(pen.x), (int)ceil(fontData.markup.font->height));

    if(textDebugMatches(text))
    {
        ::printf("[measure] \"%s\" len=%zu font.size=%f scale=%f\n",
            text.c_str(), text.length(), (double)font.size, (double)scale());
        ::printf("[measure]   markup.size=%f font->size=%f height=%f ascender=%f descender=%f\n",
            (double)fontData.markup.size, (double)fontData.markup.font->size,
            (double)fontData.markup.font->height, (double)fontData.markup.font->ascender,
            (double)fontData.markup.font->descender);
        ::printf("[measure]   vertices->size=%zu item_size=%zu capacity=%zu floats_available=%zu "
            "floats_read_max=%zu\n",
            vertices->size, vertices->item_size, vertices->capacity,
            (vertices->size*vertices->item_size)/sizeof(float),
            vertices->size*11);
        size_t dumpIndex=0;
        for(size_t i=0; i<vertices->size; ++i)
        {
            ::printf("[measure]   v%-3zu x=%10.3f y=%10.3f z=%6.2f u=%8.5f v=%8.5f\n", i,
                (double)verticesBuffer[dumpIndex], (double)verticesBuffer[dumpIndex+1],
                (double)verticesBuffer[dumpIndex+2], (double)verticesBuffer[dumpIndex+3],
                (double)verticesBuffer[dumpIndex+4]);
            dumpIndex+=11;
        }
        ::printf("[measure]   => sz.x=%d sz.y=%d (pen advance=%f)\n",
            sz.x, sz.y, (double)pen.x);
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

    text_buffer_delete(textBuffer);

    return sz;
}

bool OpenGLCore::initializeContext(gweni::WindowProvider *window)
{

#if CREATE_NATIVE_CONTEXT
    HWND hwnd=(HWND)window->getWindow();

    if(!hwnd)
        return false;

    HDC hDC=GetDC(hwnd);
    //
    // Set the pixel format
    //
    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize=sizeof(pfd);
    pfd.nVersion=1;
    pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
    pfd.iPixelType=PFD_TYPE_RGBA;
    pfd.cColorBits=24;
    pfd.cDepthBits=32;
    pfd.iLayerType=PFD_MAIN_PLANE;
    int iFormat=ChoosePixelFormat(hDC, &pfd);
    SetPixelFormat(hDC, iFormat, &pfd);
    HGLRC hRC;
    hRC=wglCreateContext(hDC);
    wglMakeCurrent(hDC, hRC);
    RECT r;

    if(GetClientRect(hwnd, &r))
    {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(r.left, r.right, r.bottom, r.top, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glViewport(0, 0, r.right-r.left, r.bottom-r.top);
    }

    m_context=(void *)hRC;
    return true;
#endif // ifdef _WIN32
    return false;
}

bool OpenGLCore::shutdownContext(gweni::WindowProvider *window)
{

#if CREATE_NATIVE_CONTEXT
    wglDeleteContext((HGLRC)m_context);
    return true;
#endif
    return false;
}

bool OpenGLCore::presentContext(gweni::WindowProvider *window)
{
#if CREATE_NATIVE_CONTEXT
    HWND hwnd=(HWND)window->getWindow();

    if(!hwnd)
        return false;

    HDC hDC=GetDC(hwnd);
    SwapBuffers(hDC);
    return true;
#endif
    return false;
}

bool OpenGLCore::resizedContext(gweni::WindowProvider *window, int w, int h)
{
#ifdef DEBUG_RENDER
    std::cout << "ResizedContext w=" << w << ", h=" << h << std::endl;
#endif//DEBUG_RENDER

#if CREATE_NATIVE_CONTEXT
    RECT r;

    if(GetClientRect((HWND)window->getWindow(), &r))
    {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(r.left, r.right, r.bottom, r.top, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glViewport(0, 0, r.right-r.left, r.bottom-r.top);
    }

    return true;
#endif // ifdef _WIN32
    return false;
}

bool OpenGLCore::beginContext(gweni::WindowProvider *window)
{
#ifdef DEBUG_RENDER
    std::cout << "BeginContext" << std::endl;
#endif//DEBUG_RENDER
    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    return true;
}

bool OpenGLCore::endContext(gweni::WindowProvider *window)
{
#ifdef DEBUG_RENDER
    std::cout << "EndContext" << std::endl;
#endif//DEBUG_RENDER
    return true;
}

uint32_t OpenGLCore::loadShaders(const char *vertex, const char *fragment)
{
    auto vertexShader=glCreateShader(GL_VERTEX_SHADER);
    auto fragmentShader=glCreateShader(GL_FRAGMENT_SHADER);

    // Binding shaders with sources
    glShaderSource(
        vertexShader,
        1,
        reinterpret_cast<const GLchar *const *>(&vertex),
        nullptr
    );
    glShaderSource(
        fragmentShader,
        1,
        reinterpret_cast<const GLchar *const *>(&fragment),
        nullptr
    );

    // Compiling shaders
    glCompileShader(vertexShader);

    checkErrors(vertexShader, "VERTEX_SHADER");

    glCompileShader(fragmentShader);

    checkErrors(fragmentShader, "FRAGMENT_SHADER");

    GLuint program=glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);

    glLinkProgram(program);

    checkErrors(program, "PROGRAM");

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

} // namespace renderer
} // namespace gweni
