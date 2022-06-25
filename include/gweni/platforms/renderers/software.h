/*
 *  Gweni
 *  Copyright (c) 2011 Facepunch Studios
 
*  See license in Gweni.h
 */



#ifndef _gweni_renderers_software_h_
#define _gweni_renderers_software_h_

#include <gweni/platforms/baseRender.h>
#include <unordered_map>
#include <memory>
#include <vector>
#include <functional>

namespace gweni
{
    namespace renderer
    {
        //! Simple 2D pixel buffer.
        class PixelBuffer
        {
            Point m_size;
            Color *m_buffer;
        public:
            PixelBuffer() : m_buffer(nullptr) {}
            ~PixelBuffer()
            {
                delete [] m_buffer;
            }
            void init(Point const& sz)
            {
                m_size = sz;
                m_buffer = new Color[m_size.x * m_size.y];
            }

            Point getSize() const { return m_size; }

            Color& At(int x, int y) { return m_buffer[y * m_size.x + x]; }
            Color& At(Point const& pt) { return At(pt.x, pt.y); }

            const Color& At(int x, int y) const { return m_buffer[y * m_size.x + x]; }
            const Color& At(Point const& pt) const { return At(pt.x, pt.y); }
        };

        //! \brief Renders to a buffer without needing external dependencies.
        //!
        //! This can be used for screenshots and testing.
        class GWENI_EXPORT Software : public gweni::renderer::Base
        {
            template<typename T>
            using deleted_unique_ptr = std::unique_ptr<T, std::function<void(T*)>>;

            static const wchar_t BeginCharacter = L' ';    // First Character of Wide Character Table
            static const wchar_t LastCharacter = 0x2FFF;   // Last Character of Wide Character Table
            static const wchar_t NewLineCharacter = L'\n'; // New Line Character

        public:

            Software(ResourcePaths& paths, PixelBuffer& pbuff);
            virtual ~Software();

            void startClip() override;
            void endClip() override;

            void setDrawColor(gweni::Color color) override;

            void drawFilledRect(gweni::Rect rect) override;
            void drawLinedRect(gweni::Rect rect) override;
            void drawPixel(int x, int y) override;

            void drawTexturedRect(const gweni::Texture& texture, gweni::Rect targetRect, float u1 = 0.0f,
                                  float v1 = 0.0f, float u2 = 1.0f, float v2 = 1.0f) override;

            gweni::Color pixelColor(const gweni::Texture& texture,
                                  unsigned int x, unsigned int y,
                                  const gweni::Color& col_default) override;

            void renderText(const gweni::Font& font,
                            gweni::Point pos,
                            const gweni::String& text) override;

            gweni::Point measureText(const gweni::Font& font, const gweni::String& text) override;

            // Resource Loader
            gweni::Font::Status loadFont(const gweni::Font& font) override;
            void freeFont(const gweni::Font& font) override;
            bool ensureFont(const gweni::Font& font) override;

            Texture::Status loadTexture(const gweni::Texture& texture) override;
            void freeTexture(const gweni::Texture& texture) override;
            TextureData getTextureData(const gweni::Texture& texture) const override;
            bool ensureTexture(const gweni::Texture& texture) override;
            
        protected:// Resourses

            struct SWTextureData : public gweni::TextureData
            {
                SWTextureData() {}

                SWTextureData(const SWTextureData&) = delete;
                
                SWTextureData(SWTextureData&& other)
                    : SWTextureData()
                {
                    std::swap(width, other.width);
                    std::swap(height, other.height);
                    std::swap(readable, other.readable);

                    m_ReadData.swap(other.m_ReadData);
                }

                ~SWTextureData() {}

                Point getSize() const
                {
                    return Point(static_cast<int>(width), static_cast<int>(height));
                }

                Color& At(int x, int y)
                {
                    return reinterpret_cast<Color*>(m_ReadData.get())[y * static_cast<int>(width) + x];
                }
                Color& At(Point const& pt) { return At(pt.x, pt.y); }

                const Color& At(int x, int y) const
                {
                    return reinterpret_cast<Color*>(m_ReadData.get())[y * static_cast<int>(width) + x];
                }
                const Color& At(Point const& pt) const { return At(pt.x, pt.y); }

                deleted_unique_ptr<unsigned char> m_ReadData;
            };

            struct SWFontData
            {
                SWFontData()
                    : m_Spacing(0.f)
                {}

                SWFontData(const SWFontData&) = delete;
                SWFontData(SWFontData&& other)
                    : SWFontData()
                {
                    std::swap(width, other.width);
                    std::swap(height, other.height);
                    std::swap(baked_chars, other.baked_chars);

                    m_ReadData.swap(other.m_ReadData);
                }

                ~SWFontData()
                {
                }

                struct stbtt_bakedchar
                {
                    unsigned short x0, y0, x1, y1; // coordinates of bbox in bitmap
                    float xoff, yoff, xadvance;
                };

                std::vector<stbtt_bakedchar> baked_chars;

                float   m_Spacing;

                float width;
                float height;
                deleted_unique_ptr<unsigned char> m_ReadData;
            };

            std::unordered_map<Font, SWFontData> m_fonts;
            std::unordered_map<Texture, SWTextureData> m_textures;
            std::pair<const Font, SWFontData>* m_lastFont;
            std::pair<const Texture, SWTextureData>* m_lastTexture;
            
        public:

            bool beginContext(gweni::WindowProvider* window) override
            {
                return false;
            }

            bool endContext(gweni::WindowProvider* window) override
            {
                return false;
            }

            bool presentContext(gweni::WindowProvider* window) override
            {
                return false;
            }

        private:

            bool Clip(Rect& rect);
            bool m_isClipping;

            gweni::Color m_color;
            PixelBuffer *m_pixbuf;
        };


    }
}
#endif //_gweni_renderers_software_h_
