/*
 *  Gweni
 *  Copyright (c) 2011 Facepunch Studios
 
*  See license in Gweni.h
 */

#ifndef _gweni_renderers_opengl_h_
#define _gweni_renderers_opengl_h_

#include <gweni/platforms/baseRender.h>
#include <unordered_map>
#include <memory>
#include <vector>
#include <functional>

namespace gweni
{
    namespace renderer
    {

        //
        //! Renderer for [OpenGL](https://www.opengl.org/).
        //
        class GWENI_EXPORT OpenGL : public gweni::renderer::Base
        {
            template<typename T>
            using deleted_unique_ptr = std::unique_ptr<T, std::function<void(T*)>>;

            static const wchar_t BeginCharacter = L' ';    // First Character of Wide Character Table
            static const wchar_t LastCharacter = 0x2FFF;   // Last Character of Wide Character Table
            static const wchar_t NewLineCharacter = L'\n'; // New Line Character

        public:
            OpenGL(ResourcePaths& paths, const gweni::Rect& viewRect);
            virtual ~OpenGL();

            void init() override;

            void begin() override;
            void end() override;

            void setDrawColor(gweni::Color color) override;
            void drawFilledRect(gweni::Rect rect) override;

            void startClip() override;
            void endClip() override;

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

            struct GLTextureData : public gweni::TextureData
            {
                GLTextureData()
                {
                }
                GLTextureData(const GLTextureData&) = delete;
                GLTextureData(GLTextureData&& other)
                    : GLTextureData()
                {
                    std::swap(width, other.width);
                    std::swap(height, other.height);
                    std::swap(readable, other.readable);
                    std::swap(texture_id, other.texture_id);

                    m_ReadData.swap(other.m_ReadData);
                }

                ~GLTextureData();

                unsigned int texture_id;
                deleted_unique_ptr<unsigned char> m_ReadData;
            };

            struct GLFontData
            {
                GLFontData()
                    : m_Spacing(0.f)
                {
                }

                GLFontData(const GLFontData&) = delete;
                GLFontData(GLFontData&& other)
                    : GLFontData()
                {
                    std::swap(width, other.width);
                    std::swap(height, other.height);
                    std::swap(texture_id, other.texture_id);
                    std::swap(baked_chars, other.baked_chars);
                }

                ~GLFontData()
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
                unsigned int texture_id;
            };

            std::unordered_map<Font, GLFontData> m_fonts;
            std::unordered_map<Texture, GLTextureData> m_textures;
            std::pair<const Font, GLFontData>* m_lastFont;
            std::pair<const Texture, GLTextureData>* m_lastTexture;
        protected:

            Rect m_viewRect;
            Color m_color;
            unsigned int m_current_texture;
            unsigned char m_textures_on;

            static const int MaxVerts = 1024;
            struct Vertex
            {
                float x, y, z;
                float u, v;
                unsigned char r, g, b, a;
            };

            int m_vertNum;
            Vertex m_vertices[ MaxVerts ];

            void Flush();
            void AddVert(int x, int y, float u = 0.0f, float v = 0.0f);
            void SetTexture(unsigned int texture);

        public:

            bool initializeContext(gweni::WindowProvider* window) override;
            bool shutdownContext(gweni::WindowProvider* window) override;
            bool presentContext(gweni::WindowProvider* window) override;
            bool resizedContext(gweni::WindowProvider* window, int w, int h) override;
            bool beginContext(gweni::WindowProvider* window) override;
            bool endContext(gweni::WindowProvider* window) override;

            void*   m_context;
        };


    }
}
#endif //_gweni_renderers_opengl_h_
