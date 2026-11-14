#ifndef _gweni_renders_openglCore_h_
#define _gweni_renders_openglCore_h_


#include <gweni/platforms/renderers/slotRenderer.h>

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <cstdint>

namespace gweni
{
namespace renderer
{

//! The GLSL OpenGLCore's programs are compiled as: one shared body per stage
//! behind a version (and, for ES, precision) header chosen by init() from the
//! context it runs in.
enum class GLSLDialect
{
    Glsl330,  //!< desktop GL 3.3 core: `#version 330 core`
    Es300     //!< GLES 3.0 / WebGL 2: `#version 300 es`, every type highp
};

//! The shader stages OpenGLCore compiles, two per program (OpenGLCore::shaderSource)
enum class GLShaderSource
{
    PrimitiveVertex,
    PrimitiveFragment,
    TextVertex,
    TextFragment
};

//
//! Renderer for [OpenGL](https://www.opengl.org/) Core Profile: the GL backend of
//! SlotRenderer, which holds everything that does not depend on the API.
//
//! It keeps to the GL 3.3 core / GLES 3.0 / WebGL 2 subset. A TextureHandle is a GL
//! texture name. The slots are drawn from two persistent vertex buffers, one vertex
//! array per index chunk pointed at the chunk's first slot, through two static
//! 16-bit quad index buffers; the clip boxes are 2D GL_RGBA16UI textures.
//
class GWENI_EXPORT OpenGLCore: public SlotRenderer
{
public:

    OpenGLCore(ResourcePaths &paths, const gweni::Rect &viewRect);
    //! Releases every font, texture, buffer, vertex array and program it owns, so the
    //! GL context it was init()'d in must still be current. Another OpenGLCore in the
    //! same context keeps working: it rebinds its own objects every flush().
    virtual ~OpenGLCore();

    void init() override;
    void setViewRect(const gweni::Rect &viewRect) override;

    void clearTarget(const gweni::Color &color) override;
    void readPixels(int width, int height, std::vector<uint8_t> &pixels) override;
    bool textureExists(TextureHandle handle) const override;
    bool textureChannelSizes(TextureHandle handle, int bits[4]) const override;
    bool deviceError() override;

    //! The GL objects init() created (0 before init()), so a test can check that the
    //! destructor deletes them.
    struct GLObjectNames
    {
        std::vector<unsigned int> buffers;
        std::vector<unsigned int> vertexArrays;
        std::vector<unsigned int> textures; //!< the clip-box textures
        std::vector<unsigned int> programs;
    };

    GLObjectNames getGLObjectNames() const;

    //! The source of one of the programs' stages in a dialect: its version header
    //! (`#version 330 core`, or `#version 300 es` with every float, int and sampler
    //! type highp), then the body every dialect shares. With dualSource the text
    //! fragment shader has the second output an LCD atlas is blended with
    //! (GWENI_DUAL_SOURCE; ES adds `#extension GL_EXT_blend_func_extended`);
    //! without it the shader writes only the colour. init() compiles the dialect
    //! of its context; a test can compile the others.
    static std::string shaderSource(GLShaderSource source, GLSLDialect dialect, bool dualSource);

    //! What the context offers beyond the GL 3.3 / GLES 3.0 / WebGL 2 subset every
    //! other call keeps to, found by init(): desktop GL 3.3 has both, an ES context
    //! what its extensions say.
    struct GLCapabilities
    {
        GLSLDialect dialect=GLSLDialect::Glsl330;
        //! glMultiDrawElements, which the primitive and text draws use; without it
        //! each of a draw's ranges is a glDrawElements of its own
        bool multiDraw=true;
        //! dual-source blending (GL_SRC1_COLOR), which LCD text is blended with;
        //! false also when GWENI_GL_NO_DUAL_SOURCE=1 was set at construction
        bool dualSourceBlend=true;
    };

    const GLCapabilities &glCapabilities() const { return m_glCapabilities; }
    //! Without dual-source blending (GLCapabilities::dualSourceBlend, after init())
    //! the LCD text modes are grayscale: why, as SlotRenderer::setTextAA logs it.
    const char *lcdTextUnavailable() const override;
    //! Whether the current context lists the extension (GL_NUM_EXTENSIONS, glGetStringi)
    static bool hasExtension(const char *name);

    //! Whether a chunk's draw (a primitive chunk's ranges, an atlas's text runs in a
    //! chunk) is one glMultiDrawElements call (true where the context has it, unless
    //! GWENI_NO_MULTI_DRAW=1 was set at construction) or its ranges drawn one
    //! glDrawElements each. Either draws the same pixels and counts
    //! the same FrameStats. A test hook: true asks for the multi-draw, which only a
    //! context that has it gives.
    bool useMultiDraw() const { return m_useMultiDraw; }
    void setUseMultiDraw(bool multiDraw);

    //! Reads level 0 of a 2D texture (width x height texels) back as RGBA8 rows,
    //! row 0 (the first uploaded) first, through a framebuffer it is attached to,
    //! since GLES has no glGetTexImage.
    //! The read framebuffer binding is put back. False, with rgba empty, if the
    //! texture cannot be attached (its format is not colour-renderable).
    static bool readTexture(unsigned int textureName, int width, int height, std::vector<uint8_t> &rgba);
    //! The red, green, blue and alpha bits of a 2D texture's level 0, read from a
    //! framebuffer it is attached to (GLES has no glGetTexLevelParameteriv): 8,0,0,0
    //! for GL_R8, 8,8,8,0 for GL_RGB8. False if it cannot be attached.
    static bool textureChannelBits(unsigned int textureName, int bits[4]);

    bool initializeContext(gweni::WindowProvider *window) override;
    bool shutdownContext(gweni::WindowProvider *window) override;
    bool presentContext(gweni::WindowProvider *window) override;
    bool resizedContext(gweni::WindowProvider *window, int w, int h) override;
    bool beginContext(gweni::WindowProvider *window) override;
    bool endContext(gweni::WindowProvider *window) override;

    void *m_context;

protected:

    // SlotRenderer's hooks
    void beginFrame() override;
    void allocateVertexBuffer(SlotPath path, size_t bytes) override;
    void uploadVertexRange(SlotPath path, size_t offset, size_t bytes, const void *data) override;
    size_t maxClipBoxRows() const override;
    void allocateClipBoxes(SlotPath path, size_t rows) override;
    void uploadClipBoxRows(SlotPath path, size_t x, size_t y, size_t width, size_t height,
        const Box *boxes) override;
    TextureHandle createAtlasTexture(const texture_atlas_t &atlas) override;
    void uploadAtlasRows(TextureHandle texture, const texture_atlas_t &atlas, size_t firstRow, size_t rows) override;
    void destroyAtlasTexture(TextureHandle texture) override;
    size_t maxImageLayers() const override;
    bool allocateImagePage(size_t imageClass, size_t layers, size_t keptLayers) override;
    void uploadImageLayer(size_t imageClass, size_t layer, int width, int height, const uint8_t *rgba,
        const ImageMipLevel *levels, size_t levelCount) override;
    bool readImageLayer(size_t imageClass, size_t layer, int width, int height,
        std::vector<uint8_t> &rgba) override;
    TextureHandle imagePageHandle(size_t imageClass) const override;
    void destroyImagePages() override;
    bool readTextureRgba(TextureHandle texture, int width, int height, std::vector<uint8_t> &rgba) override;
    void beginPrimitiveDraws() override;
    void drawPrimitiveChunk(const ChunkDraw &draw) override;
    void endPrimitiveDraws() override;
    void beginTextDraws() override;
    void beginTextAtlas(const FontData &font) override;
    void drawTextChunk(const ChunkDraw &draw) override;
    void endTextDraws() override;
    void submitFrame() override;
    void indexChunkSlotsChanged() override;
    void viewTransformChanged() override;

private:

    uint32_t loadShaders(const char *vertex, const char *fragment);
    //! What the current context offers (GLCapabilities), from GL_VERSION and its extensions
    static GLCapabilities detectCapabilities();

    //! Chunk `chunk`'s primitive vertex array, made (with its attributes pointed at the
    //! chunk's first vertex and m_quadIndexBuffer as its element buffer) on first use.
    uint32_t primitiveVertexArray(size_t chunk);
    //! The same for the text: chunk `chunk`'s glyph vertex array, its element buffer
    //! m_textQuadIndexBuffer.
    uint32_t textVertexArray(size_t chunk);
    //! A chunk's ranges as one glMultiDrawElements, or a loop of glDrawElements
    //! where the context has none (useMultiDraw()), its vertex array bound
    void drawRanges(const ChunkDraw &draw);

    void setTexture(unsigned int texture);

    //! Unit 0 is setTexture's and the text's atlas, 1 the clip boxes, the image
    //! pages (SlotRenderer::ImageClasses) from this one on
    static constexpr int FirstImageUnit=2;
    //! Each class's GL_TEXTURE_2D_ARRAY, 0 until an image needs it
    unsigned int m_imagePages[ImageClasses]={};

    unsigned int m_currentTexture;
    glm::uvec2 m_currentTextureSize;
    unsigned char m_texturesOn;
    std::vector<glm::uvec2> m_textureSizes;

    //! what init() found the context to offer
    GLCapabilities m_glCapabilities;
    //! the chunks' draws are glMultiDrawElements calls (useMultiDraw())
    bool m_useMultiDraw=true;
    //! GWENI_NO_MULTI_DRAW=1 at construction: the draws loop even where the
    //! context has glMultiDrawElements
    bool m_noMultiDraw=false;
    //! GWENI_GL_NO_DUAL_SOURCE=1 at construction: init() takes the context to have
    //! no dual-source blending, as a GLES one without GL_EXT_blend_func_extended
    //! (or WebGL 2) has none, so LCD text falls back to grayscale
    bool m_noDualSource=false;

    //! The view's orthographic projection, z from 0 (depth 1.0) to kMaxZIndex (depth 0).
    glm::mat4 m_projectionMatrix;

    uint32_t m_program=0;

    uint32_t m_projectionUniform;
    uint32_t m_imagesUniform;
    uint32_t m_clipBoxesUniform;
    uint32_t m_slotBaseUniform; //!< the first slot of the chunk being drawn

    //! kSlotsPerChunk quads of uint16_t indexes (v, v+1, v+2, v+1, v+3, v+2 for v=i*4),
    //! GL_STATIC_DRAW, filled once by init() and the element buffer of every chunk's
    //! vertex array. A slot's indexes never change, so there is no index mirror.
    uint32_t m_quadIndexBuffer=0;

    //! One vertex array per chunk of indexChunkSlots() primitive slots, made as the
    //! slot count first reaches it (primitiveVertexArray()); chunk c's attributes
    //! start at vertex c*indexChunkSlots()*4.
    std::vector<uint32_t> m_vertexArrays;
    uint32_t m_vertexBuffer=0;
    //! GL_RGBA16UI, kClipColumns wide: the primitives' clip boxes
    uint32_t m_clipTexture=0;

    uint32_t m_textProgram=0;
    uint32_t m_textProjectionUniform;
    uint32_t m_textClipBoxesUniform;
    uint32_t m_textModeUniform; //!< the TextMode of the atlas being drawn
    uint32_t m_textContrastUniform; //!< textContrast(), set every flush()

    uint32_t m_textSlotBaseUniform; //!< the first glyph slot of the chunk being drawn

    //! kSlotsPerChunk quads of uint16_t indexes in freetype-gl's glyph order (v, v+1,
    //! v+2, v, v+2, v+3 for v=i*4: its quads go round their corners, where the
    //! primitives' zigzag), GL_STATIC_DRAW, filled once by init().
    uint32_t m_textQuadIndexBuffer=0;
    //! One vertex array per chunk of indexChunkSlots() glyph slots (textVertexArray()).
    std::vector<uint32_t> m_textVertexArrays;
    uint32_t m_textVertexBuffer=0;
    //! as m_clipTexture, for the glyphs' clip boxes
    uint32_t m_textClipTexture=0;

    //! The blend in place before the text pass's first LCD atlas, put back for the
    //! other atlases and after the pass (beginTextAtlas, endTextDraws)
    int m_textBlend[4]={0, 0, 0, 0};
    bool m_lcdBlend=false;
};


}//namespace renderer
}//namespace gweni

#endif //_gweni_renders_openglCore_h_
