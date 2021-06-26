/*
 *  Software Renderer for Gweni.
 
 *  See license in Gweni.h
 */

#include <gweni/Renderers/Software.h>
#include <gweni/platformCommon.h>
#include <gweni/Utility.h>
#define STB_IMAGE_IMPLEMENTATION
#include <gweni/External/stb_image.h>
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include <gweni/External/stb_truetype.h>
#include <sys/stat.h>

#include <fstream>

namespace gweni
{
namespace renderer
{

namespace drawing
{
    //! Blend two colors using: result = S_rgb*S_alpha + D_rgb*(1 - S_alpha)
    static inline Color BlendAlpha(Color const& src, Color const& dst)
    {
        // use fixed point to blend
        constexpr unsigned sh = 16;
        constexpr uint32_t s = (1u << sh) / (255u);
        const uint32_t a = (src.a * s);
        const uint32_t b = (1u << sh) - a;;
        
        const Color r(((src.r * a) + (dst.r * b)) >> sh,
                      ((src.g * a) + (dst.g * b)) >> sh,
                      ((src.b * a) + (dst.b * b)) >> sh,
                      src.a);
        return r;
    }

    //! Draw filled rectangle.
    template <typename T>
    void RectFill(T& pb, Rect const& r, Color const& c)
    {
        for (int y = 0; y < r.h; ++y)
        {
            Color *px = &pb.At(r.x, r.y + y);
            for (int x = r.w; x > 0; --x)
            {
                *px = BlendAlpha(c, *px);
                ++px;
            }
        }
    }

    //! Draw rectangle outline.
    template <typename T>
    void RectOutline(T& pb, Rect const& r, Color const& c)
    {
        for (int x = 0; x < r.w; ++x)
        {
            pb.At(r.x + x, r.y) = BlendAlpha(c, pb.At(r.x + x, r.y));  // top
            if (r.h > 1)
                pb.At(r.x + x, r.y + r.h - 1) = BlendAlpha(c, pb.At(r.x + x, r.y + r.h - 1));  // bottom
        }
        for (int y = 0; y < r.h; ++y)
        {
            pb.At(r.x, r.y + y) = BlendAlpha(c, pb.At(r.x, r.y + y));  // left
            if (r.w > 1)
                pb.At(r.x + r.w - 1, r.y + y) = BlendAlpha(c, pb.At(r.x + r.w - 1, r.y + y));  // right
        }
    }

    //! Draw textured rectangle.
    template <typename T, typename U>
    void RectTextured(T& pb, const U& pbsrc,
                      const gweni::Rect& rect, float u1, float v1, float u2, float v2)
    {
        const Point srcsz(pbsrc.getSize());
        const Point uvtl(srcsz.x * u1, srcsz.y * v1);

        const float dv = (v2 - v1) * srcsz.y / rect.h;
        float v = uvtl.y;
        for (int y = 0; y < rect.h; ++y)
        {
            const float du = (u2 - u1) * srcsz.x / rect.w;
            float u = uvtl.x;
            Color *px = &pb.At(rect.x, rect.y + y);
            for (int x = 0; x < rect.w; ++x)
            {
                *px = BlendAlpha(pbsrc.At(u, v), *px);
                ++px;
                u += du;
            }
            v += dv;
        }
    }
}

//-------------------------------------------------------------------------------

// See "Font Size in Pixels or Points" in "stb_truetype.h"
static constexpr float c_pointsToPixels = 1.333f;
// arbitrary font cache texture size
// Texture size too small for wchar_t characters but stbtt_BakeFontBitmap crashes on larger sizes
static constexpr int c_texsz = 800;

Font::Status Software::loadFont(const Font& font)
{
    freeFont(font);
    m_lastFont = nullptr;

    const String filename = getResourcePaths().getPath(ResourcePaths::Type::Font, font.facename);

    std::ifstream inFile(filename, std::ifstream::in | std::ifstream::binary);

    if (!inFile.good())
    {
        gweni::log::write(log::Level::Error, "Font file not found: %s", filename.c_str());
        return Font::Status::ErrorFileNotFound;
    }

    std::streampos begin = inFile.tellg();
    inFile.seekg(0, std::ios::end);
    const size_t fsz = inFile.tellg() - begin;
    inFile.seekg(0, std::ios::beg);
    assert(fsz > 0);

    auto ttfdata = std::unique_ptr<unsigned char[]>(new unsigned char[fsz]);
    inFile.read(reinterpret_cast<char*>(ttfdata.get()), fsz);
    inFile.close();

    SWFontData fontData;
    fontData.m_ReadData =
        deleted_unique_ptr<unsigned char>(new unsigned char[c_texsz * c_texsz],
                                          [](unsigned char* mem) { delete [] mem; });
    fontData.width = c_texsz;
    fontData.height = c_texsz;

    const float realsize = font.size * scale();
    fontData.baked_chars.resize(LastCharacter - BeginCharacter + 1);

    stbtt_BakeFontBitmap(ttfdata.get(), 0,
        realsize * c_pointsToPixels, // height
        fontData.m_ReadData.get(),
        c_texsz, c_texsz,
        BeginCharacter, LastCharacter,             // range to bake
        reinterpret_cast<stbtt_bakedchar*>(fontData.baked_chars.data()));

    m_lastFont = &(*m_fonts.insert(std::make_pair(font, std::move(fontData))).first);
    return Font::Status::Loaded;
}

void Software::freeFont(const gweni::Font& font)
{
    if (m_lastFont != nullptr && m_lastFont->first == font)
        m_lastFont = nullptr;

    m_fonts.erase(font); // calls SWFontData destructor
}

bool Software::ensureFont(const Font& font)
{
    if (m_lastFont != nullptr)
    {
        if (m_lastFont->first == font)
            return true;
    }

    // Was it loaded before?
    auto it = m_fonts.find(font);
    if (it != m_fonts.end())
    {
        m_lastFont = &(*it);
        return true;
    }

    // No, try load to it

    // LoadFont sets m_lastFont, if loaded
    return loadFont(font) == Font::Status::Loaded;
}

Texture::Status Software::loadTexture(const Texture& texture)
{
    freeTexture(texture);
    m_lastTexture = nullptr;

    const String filename = getResourcePaths().getPath(ResourcePaths::Type::Texture, texture.name);

    SWTextureData texData;

    int width, height, n;
    {
        unsigned char *image = stbi_load(filename.c_str(), &width, &height, &n, 4);
        texData.m_ReadData =
            deleted_unique_ptr<unsigned char>(image,
                                              [](unsigned char* mem) {
                                                  if (mem) stbi_image_free(mem);
                                              });
    }

    // Image failed to load..
    if (!texData.m_ReadData)
    {
        gweni::log::write(log::Level::Error, "Texture file not found: %s", filename.c_str());
        return Texture::Status::ErrorFileNotFound;
    }

    texData.readable = true;

    texData.width = width;
    texData.height = height;

    m_lastTexture = &(*m_textures.insert(std::make_pair(texture, std::move(texData))).first);
    return Texture::Status::Loaded;
}

void Software::freeTexture(const gweni::Texture& texture)
{
    if (m_lastTexture != nullptr && m_lastTexture->first == texture)
        m_lastTexture = nullptr;

    m_textures.erase(texture); // calls SWTextureData destructor
}

TextureData Software::getTextureData(const Texture& texture) const
{
    if (m_lastTexture != nullptr && m_lastTexture->first == texture)
        return m_lastTexture->second;

    auto it = m_textures.find(texture);
    if (it != m_textures.cend())
    {
        return it->second;
    }
    // Texture not loaded :(
    return TextureData();
}

bool Software::ensureTexture(const gweni::Texture& texture)
{
    if (m_lastTexture != nullptr)
    {
        if (m_lastTexture->first == texture)
            return true;
    }

    // Was it loaded before?
    auto it = m_textures.find(texture);
    if (it != m_textures.end())
    {
        m_lastTexture = &(*it);
        return true;
    }

    // No, try load to it

    // LoadTexture sets m_lastTexture, if exist
    return loadTexture(texture) == Texture::Status::Loaded;
}
//-------------------------------------------------------------------------------

Software::Software(ResourcePaths& paths, PixelBuffer& pbuff)
    :   Base(paths)
    ,   m_lastFont(nullptr)
    ,   m_lastTexture(nullptr)
    ,   m_isClipping(false)
    ,   m_pixbuf(&pbuff)
{
}

Software::~Software()
{
}

void Software::setDrawColor(gweni::Color color)
{
    m_color = color;
}

gweni::Point Software::measureText(const gweni::Font& font, const gweni::String& text)
{
    if (!ensureFont(font))
        return gweni::Point(0, 0);

    SWFontData& fontData = m_lastFont->second;

    Point sz(0, font.size * scale() * c_pointsToPixels);

    float x = 0.f, y = 0.f;
    char* text_ptr = const_cast<char*>(text.c_str());

    while (const auto wide_char = utility::strings::utf8_to_wchart(text_ptr))
    {
        const auto c = wide_char - BeginCharacter;

        if (wide_char < BeginCharacter || wide_char > LastCharacter)
            continue;

        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(reinterpret_cast<stbtt_bakedchar*>(fontData.baked_chars.data()),
                           c_texsz, c_texsz,
                           c,
                           &x, &y, &q, 1); // 1=opengl & d3d10+,0=d3d9

        sz.x = q.x1;
        sz.y = std::max(sz.y, int((q.y1 - q.y0) * c_pointsToPixels));
    }

    return sz;
}

void Software::renderText(const gweni::Font& font, gweni::Point pos,
    const gweni::String& text)
{
    if (!ensureFont(font))
        return;
    
    SWFontData& fontData = m_lastFont->second;

    float x = pos.x, y = pos.y;
    char* text_ptr = const_cast<char*>(text.c_str());
    const auto clipRect = clipRegion();
    const Point srcSize(fontData.width, fontData.height);
    const unsigned char * const fontBmp = fontData.m_ReadData.get();

    // Height of font, allowing for descenders, because baseline is bottom of capitals.
    const float offset = font.size * scale() * c_pointsToPixels * 0.8f;

    Color col(m_color);
    while (const auto wide_char = utility::strings::utf8_to_wchart(text_ptr))
    {
        const auto c = wide_char - BeginCharacter;

        if (wide_char < BeginCharacter || wide_char > LastCharacter)
            continue;

        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(reinterpret_cast<stbtt_bakedchar*>(fontData.baked_chars.data()),
            c_texsz, c_texsz,
            c,
            &x, &y, &q, 1); // 1=opengl & d3d10+,0=d3d9

        Rect srcCharRect(q.x0, q.y0, q.x1 - q.x0, q.y1 - q.y0);
        translate(srcCharRect);
        Point fpos(0, srcCharRect.y + offset);
        for (int fy = 0; fy < srcCharRect.h; ++fy, ++fpos.y)
        {
            if (fpos.y < clipRect.top())
                continue;

            if (fpos.y > clipRect.bottom())
                return;

            fpos.x = srcCharRect.x;
            for (int fx = 0; fx < srcCharRect.w; ++fx, ++fpos.x)
            {
                if (fpos.x < clipRect.left())
                    continue;

                if (fpos.x > clipRect.right())
                    return;

                const Point srcPix(q.s0*srcSize.x + fx, q.t0*srcSize.y + fy);
                const unsigned char fi = fontBmp[srcPix.y * srcSize.x + srcPix.x];
                col.a = fi;

                Color& dst = m_pixbuf->At(fpos);
                dst = Drawing::BlendAlpha(col, dst);
            }
        }
    }
}

void Software::startClip()
{
    m_isClipping = true;
}

void Software::endClip()
{
    m_isClipping = false;
}

bool Software::Clip(Rect& rect)
{
    if (m_isClipping)
    {
        if (!clipRegionVisible())
        {
            rect = Rect();
            return false;
        }

        const Rect& cr(clipRegion());

        // left
        if (rect.x < cr.x)
        {
            rect.w -= cr.x - rect.x;
            rect.x = cr.x;
        }

        // top
        if (rect.y < cr.y)
        {
            rect.h -= cr.y - rect.y;
            rect.y = cr.y;
        }

        // right
        if (rect.right() > cr.right())
        {
            rect.w -= rect.right() - cr.right();
        }

        // bottom
        if (rect.bottom() > cr.bottom())
        {
            rect.h -= rect.bottom() - cr.bottom();
        }

        return rect.w > 0 && rect.h > 0;
    }

    return true;
}

void Software::drawPixel(int x, int y)
{
    translate(x, y);

    Rect r(x,y,1,1);
    if (Clip(r))
        m_pixbuf->At(x, y) = m_color;
}

void Software::drawFilledRect(gweni::Rect rect)
{
    translate(rect);
    if (Clip(rect))
        Drawing::RectFill(*m_pixbuf, rect, m_color);
}

void Software::drawLinedRect(gweni::Rect rect)
{
    translate(rect);
    if (Clip(rect))
        Drawing::RectOutline(*m_pixbuf, rect, m_color);
}

void Software::drawTexturedRect(const gweni::Texture& texture, gweni::Rect rect,
                                float u1, float v1, float u2, float v2)
{
    if (!ensureTexture(texture))
        return drawMissingImage(rect);

    const SWTextureData& texData = m_lastTexture->second;

    translate(rect);
    if (!Clip(rect))
        return;

    Drawing::RectTextured<PixelBuffer, SWTextureData>(*m_pixbuf, texData, rect, u1,v1, u2,v2);
}

gweni::Color Software::pixelColor(const gweni::Texture& texture, unsigned int x, unsigned int y,
                                const gweni::Color& col_default)
{
    if (!ensureTexture(texture))
        return col_default;

    const SWTextureData& texData = m_lastTexture->second;

    return texData.At(x, y);
}


} // Renderer
} // Gweni
