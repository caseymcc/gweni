#ifndef _gweni_renders_openglCore_h_
#define _gweni_renders_openglCore_h_


#include <gweni/platforms/baseRender.h>
#include <gweni/objectIdBank.h>

#include <vector>
#include <glm/glm.hpp>
#include <unordered_map>
#include <memory>
#include <functional>

#include <freetype-gl/freetype-gl.h>
#include <freetype-gl/markup.h>
#include <freetype-gl/font-manager.h>
#include <freetype-gl/text-buffer.h>

#include <imglib/image.h>

namespace gweni
{
namespace renderer
{

struct GLTextureData: public gweni::TextureData
{
    GLTextureData()
    {}
    GLTextureData(const GLTextureData &)=delete;
    GLTextureData(GLTextureData &&other)
        : GLTextureData()
    {
        std::swap(width, other.width);
        std::swap(height, other.height);
        std::swap(readable, other.readable);
        std::swap(m_textureId, other.m_textureId);

        m_readData.swap(other.m_readData);
    }

    ~GLTextureData();

    unsigned int m_textureId;
    //                deleted_unique_ptr<unsigned char> m_ReadData;
    std::vector<unsigned char> m_readData;
};

struct GLFontData
{
    GLFontData():
        spacing(0.f)
    {}

    GLFontData(const GLFontData &)=delete;

    GLFontData(GLFontData &&other)
        : GLFontData()
    {
        faceName=std::move(other.faceName);
        markup=std::move(other.markup);

        atlas=other.atlas;
        other.atlas=nullptr;
        textureFont=other.textureFont;
        other.textureFont=nullptr;

        spacing=other.spacing;
        width=other.width;
        height=other.height;
        //                    std::swap(texture_id, other.texture_id);
        //                    std::swap(baked_chars, other.baked_chars);
    }

    ~GLFontData()
    {
    }
    //                struct stbtt_bakedchar
    //                {
    //                    unsigned short x0, y0, x1, y1; // coordinates of bbox in bitmap
    //                    float xoff, yoff, xadvance;
    //                };
    //
    //                std::vector<stbtt_bakedchar> baked_chars;
    std::string faceName; //holds pointer fir markup string
    ftgl::markup_t markup;

    ftgl::font_manager_t *fontManager;
    texture_atlas_t *atlas;
    ftgl::texture_font_t *textureFont;

    float spacing;

    int width;
    int height;
    //                unsigned int texture_id;
};

//
//! Renderer for [OpenGL](https://www.opengl.org/) Core Profile.
//
class GWENI_EXPORT OpenGLCore: public gweni::renderer::Base
{
    template<typename T>
    using deleted_unique_ptr=std::unique_ptr<T, std::function<void(T *)>>;

    static const wchar_t BeginCharacter=L' ';    // First Character of Wide Character Table
    static const wchar_t LastCharacter=0x2FFF;   // Last Character of Wide Character Table
    static const wchar_t NewLineCharacter=L'\n'; // New Line Character

public:

    OpenGLCore(ResourcePaths &paths, const gweni::Rect &viewRect);
    virtual ~OpenGLCore();

    void init() override;

    void begin() override;
    void end() override;

    void setDrawColor(gweni::Color color) override;
    void drawFilledRect(size_t primitiveId, gweni::Rect rect) override;

    void startClip() override;
    void endClip() override;

    void generatePrimitive(size_t primitiveId) override;
    void removePrimitive(size_t primitiveId) override;

    void drawTexturedRect(const gweni::Texture &texture, size_t primitiveId, gweni::Rect targetRect, float u1=0.0f,
        float v1=0.0f, float u2=1.0f, float v2=1.0f) override;

    void showPrimitive(size_t primitiveId) override;
    void hidePrimitive(size_t primitiveId) override;

    gweni::Color pixelColor(const gweni::Texture &texture,
        unsigned int x, unsigned int y,
        const gweni::Color &col_default) override;

    void renderText(size_t textId, const gweni::Font &font,
        gweni::Point pos,
        const gweni::String &text) override;

    gweni::Point measureText(const gweni::Font &font, const gweni::String &text) override;

    // Resource Loader
    gweni::Font::Status loadFont(const gweni::Font &font) override;
    void freeFont(const gweni::Font &font) override;
    bool ensureFont(const gweni::Font &font) override;

    Texture::Status loadTexture(const gweni::Texture &texture) override;
    void freeTexture(const gweni::Texture &texture) override;
    TextureData getTextureData(const gweni::Texture &texture) const override;
    bool ensureTexture(const gweni::Texture &texture) override;

protected:// Resourses



    std::unordered_map<Font, GLFontData> m_fonts;
    std::unordered_map<Texture, GLTextureData> m_textures;
    std::pair<const Font, GLFontData> *m_lastFont;
    std::pair<const Texture, GLTextureData> *m_lastTexture;
public:

    bool initializeContext(gweni::WindowProvider *window) override;
    bool shutdownContext(gweni::WindowProvider *window) override;
    bool presentContext(gweni::WindowProvider *window) override;
    bool resizedContext(gweni::WindowProvider *window, int w, int h) override;
    bool beginContext(gweni::WindowProvider *window) override;
    bool endContext(gweni::WindowProvider *window) override;

    void *m_context;

private:

    uint32_t loadShaders(const char *vertex, const char *fragment);

    /**
        * @brief Perform actual rendering.
        */
    void flush();

    /**
        * @brief Add vertex to buffer.
        * @param x
        * @param y
        * @param u
        * @param v
        */
    struct Vertex
    {
        uint8_t type;
        uint16_t x, y, z;
        uint16_t tx, ty; //used for color if not texture
//        glm::vec3 pos;
//        glm::vec2 uv;
//        glm::vec4 color;
    };

    struct Box
    {
        uint8_t state;
        uint16_t x0, y0;
        uint16_t x1, y1;
    };

    void setVertex(Vertex &vertex, int x, int y, float u=0.0f, float v=0.0f);
    void addVert(int x, int y, float u=0.0f, float v=0.0f);
    void setTexture(unsigned int texture);

    Rect m_viewRect;
    Color m_color;
    unsigned int m_currentTexture;
    glm::uvec2 m_currentTextureSize;
    unsigned char m_texturesOn;
    std::vector<glm::uvec2> m_textureSizes;
    bool m_clipping;

    std::vector<uint16_t> m_indexes; //argue about indices all you want
    std::vector<Vertex> m_vertexes; //argue about vertices all you want
    std::vector<Box> m_clippingBoxes;

    ObjectIdBank<size_t, std::numeric_limits<size_t>::max()> m_textIds;
    std::vector<std::vector<size_t>> m_textPrimitiveIds;
    std::vector<std::string> m_textStrings;
    std::vector<gweni::Point> m_textPosition;
    std::vector<uint16_t> m_textIndexes;
    std::vector<ftgl::glyph_vertex_t> m_textVertexes;

    glm::mat4 m_projectionMatrix;

    uint32_t m_program;
    uint32_t m_activeProgram;

    uint32_t m_projectionUniform;
    uint32_t m_textureSizeUniform;

    uint32_t m_vertexArray; 
    uint32_t m_vertexBuffer;
    uint32_t m_indexBuffer;
    uint32_t m_clipBuffer;

//    ftgl::font_manager_t *m_fontManager;

    uint32_t m_textTextureId;
    uint32_t m_textProgram;
    uint32_t m_textProjectionUniform;

    uint32_t m_textVertexArray; 
    uint32_t m_textVertexBuffer;
    uint32_t m_textIndexBuffer;

    uint16_t m_zIndex;
};


}//namespace renderer
}//namespace gweni

namespace imglib
{

template<>
inline Format format<gweni::renderer::GLTextureData>(const gweni::renderer::GLTextureData &image)
{
    size_t channels=image.m_readData.size()/(image.width*image.height);

    if(channels == 4)
        return Format::RGBA;
    if(channels == 3)
        return Format::RGB;
    if(channels == 2)
        return Format::RA;
    if(channels == 1)
        return Format::GreyScale;
    return Format::Unknown;
}
template<>
inline Depth depth<gweni::renderer::GLTextureData>(const gweni::renderer::GLTextureData &image) { return Depth::Bit8; }
template<>
inline Location location<gweni::renderer::GLTextureData>(const gweni::renderer::GLTextureData &image) { return Location::System; } //allways system for this guy

template<>
inline size_t width<gweni::renderer::GLTextureData>(const gweni::renderer::GLTextureData &image) { return image.width; }
template<>
inline size_t height<gweni::renderer::GLTextureData>(const gweni::renderer::GLTextureData &image) { return image.height; }
template<>
inline size_t stride<gweni::renderer::GLTextureData>(const gweni::renderer::GLTextureData &image) { return image.width; }

template<>
inline size_t nativeId<gweni::renderer::GLTextureData>(const gweni::renderer::GLTextureData &image) { return (size_t)image.m_textureId; }
template<>
inline uint8_t *data<gweni::renderer::GLTextureData>(gweni::renderer::GLTextureData &image) { return image.m_readData.data(); }
template<>
inline const uint8_t *data<gweni::renderer::GLTextureData>(const gweni::renderer::GLTextureData &image) { return image.m_readData.data(); }
template<>
inline size_t dataSize<gweni::renderer::GLTextureData>(const gweni::renderer::GLTextureData &image) { return image.m_readData.size(); }

template<>
inline bool resize<gweni::renderer::GLTextureData>(gweni::renderer::GLTextureData &image, Format format, Depth depth, size_t width, size_t height)
{
    size_t dataSize=sizeOfPixel(format, depth)*width*height;

    image.m_readData.resize(dataSize);
    image.width=width;
    image.height=height;
    return true;
}

}//namespace imglib

#endif //_gweni_renders_openglCore_h_
