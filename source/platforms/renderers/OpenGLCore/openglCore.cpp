/*
 *  Gweni - OpenGL Core renderer.
 *  Copyright (c) 2013-18 Nick Trout
 *  See license in Gweni.h
 */
#ifdef GWENI_DEBUG_LOGGING
#define DEBUG_RENDER
#endif

#include <gweni/platforms/renderers/openglCore.h>
#include <gweni/platforms/renderers/glApi.h>
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
#include <cstring>
#include <cmath>
#include <algorithm>
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

#ifdef GWENI_GL_ES
// GLES names the dual-source blend factors after GL_EXT_blend_func_extended
#   ifndef GL_SRC1_COLOR
#       define GL_SRC1_COLOR GL_SRC1_COLOR_EXT
#       define GL_ONE_MINUS_SRC1_COLOR GL_ONE_MINUS_SRC1_COLOR_EXT
#   endif
#   if !defined(__EMSCRIPTEN__)
// libGLESv2 exports the core entry points only: an extension's comes from EGL
#       include <EGL/egl.h>
#   endif
#endif


namespace gweni
{
namespace renderer
{

// the shader bodies, embedded by cmake/embedFile.cmake (CMakeLists.txt)
extern const unsigned char primitiveVertexGlsl[];
extern const size_t primitiveVertexGlslSize;
extern const unsigned char primitiveFragmentGlsl[];
extern const size_t primitiveFragmentGlslSize;
extern const unsigned char textVertexGlsl[];
extern const size_t textVertexGlslSize;
extern const unsigned char textFragmentGlsl[];
extern const size_t textFragmentGlslSize;

#ifdef GWENI_GL_ES
namespace
{
// GL_EXT_multi_draw_arrays' glMultiDrawElementsEXT, which the chunks' draws use in
// a GLES context listing it; detectCapabilities resolves it (on the web it stays
// null, and the draws loop)
PFNGLMULTIDRAWELEMENTSEXTPROC multiDrawElementsEXT=nullptr;
}//namespace
#endif

// The view's projection: device px, y down, and z (a z index) from 0 at depth 1.0,
// the depth-clear value, to OpenGLCore::kMaxZIndex at depth 0, so a higher z passes
// GL_LESS in front of a lower one.
static glm::mat4 viewProjection(const gweni::Rect &viewRect)
{
    return glm::ortho(
        static_cast<float>(viewRect.x),
        static_cast<float>(viewRect.x + viewRect.w),
        static_cast<float>(viewRect.y + viewRect.h),
        static_cast<float>(viewRect.y),
        -static_cast<float>(OpenGLCore::kMaxZIndex),
        0.0f
    );
}

void OpenGLCore::clearTarget(const gweni::Color &color)
{
    glClearColor(color.r/255.0f, color.g/255.0f, color.b/255.0f, color.a/255.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
}

void OpenGLCore::readPixels(int width, int height, std::vector<uint8_t> &pixels)
{
    const size_t rowBytes=static_cast<size_t>(width)*4;
    std::vector<uint8_t> raw(rowBytes*static_cast<size_t>(height));

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, raw.data());

    // glReadPixels hands back bottom-up rows; the caller wants top-down.
    pixels.assign(raw.size(), 0);
    for(int y=0; y<height; ++y)
        memcpy(&pixels[rowBytes*static_cast<size_t>(y)],
               &raw[rowBytes*static_cast<size_t>(height-1-y)], rowBytes);
}

bool OpenGLCore::textureExists(TextureHandle handle) const
{
    return glIsTexture(handle) == GL_TRUE;
}

bool OpenGLCore::textureChannelSizes(TextureHandle handle, int bits[4]) const
{
    return textureChannelBits(handle, bits);
}

bool OpenGLCore::deviceError()
{
    bool error=false;

    while(glGetError() != GL_NO_ERROR)
        error=true;
    return error;
}

void OpenGLCore::beginFrame()
{
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
}

void OpenGLCore::allocateVertexBuffer(SlotPath path, size_t bytes)
{
    // the buffer name stays, so the chunks' vertex arrays keep pointing at it
    glBindBuffer(GL_ARRAY_BUFFER, (path == SlotPath::Primitives)?m_vertexBuffer:m_textVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, bytes, nullptr, GL_DYNAMIC_DRAW);
}

void OpenGLCore::uploadVertexRange(SlotPath path, size_t offset, size_t bytes, const void *data)
{
    glBindBuffer(GL_ARRAY_BUFFER, (path == SlotPath::Primitives)?m_vertexBuffer:m_textVertexBuffer);
    glBufferSubData(GL_ARRAY_BUFFER, offset, bytes, data);
}

size_t OpenGLCore::maxClipBoxRows() const
{
    GLint maxSize=0;

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    return (size_t)maxSize;
}

void OpenGLCore::allocateClipBoxes(SlotPath path, size_t rows)
{
    // on unit 1, where the draws read them, so unit 0's binding stays as it is
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, (path == SlotPath::Primitives)?m_clipTexture:m_textClipTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16UI, (GLsizei)kClipColumns, (GLsizei)rows, 0,
        GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, nullptr);
    glActiveTexture(GL_TEXTURE0);
}

void OpenGLCore::uploadClipBoxRows(SlotPath path, size_t x, size_t y, size_t width, size_t height,
    const Box *boxes)
{
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, (path == SlotPath::Primitives)?m_clipTexture:m_textClipTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, (GLint)x, (GLint)y, (GLsizei)width, (GLsizei)height,
        GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, boxes);
    glActiveTexture(GL_TEXTURE0);
}

TextureHandle OpenGLCore::createAtlasTexture(const texture_atlas_t &atlas)
{
    const bool lcd=(atlas.depth == 3);
    GLuint texture=0;

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // one byte per texel, coverage or distance alike, or three of subpixel coverage
    glTexImage2D(GL_TEXTURE_2D, 0, lcd?GL_RGB8:GL_R8, atlas.width,
        atlas.height, 0, lcd?GL_RGB:GL_RED, GL_UNSIGNED_BYTE, nullptr);
    return texture;
}

void OpenGLCore::uploadAtlasRows(TextureHandle texture, const texture_atlas_t &atlas, size_t firstRow, size_t rows)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, (GLint)firstRow, (GLsizei)atlas.width, (GLsizei)rows,
        (atlas.depth == 3)?GL_RGB:GL_RED, GL_UNSIGNED_BYTE, atlas.data+firstRow*atlas.width*atlas.depth);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    //setTexture() skips the bind when m_currentTexture already matches
    glBindTexture(GL_TEXTURE_2D, m_currentTexture);
}

void OpenGLCore::destroyAtlasTexture(TextureHandle texture)
{
    glDeleteTextures(1, &texture);
}

TextureHandle OpenGLCore::createImageTexture(const SlotTextureData &image, bool rgba)
{
    GLuint texture=0;

    glGenTextures(1, &texture);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    // mipmapped, because an image is often drawn much smaller than it is. The
    // shader filters it itself, trilinearly (imageTexel), from texels it reads at
    // their centres from one level at a time, so the unit's filter is nearest:
    // it returns each texel as it is.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // a sized internal format that matches the data, as GLES requires; an RGB one
    // samples with alpha 1, as the RGBA texture desktop GL made of RGB data did
    const GLenum format=rgba?GL_RGBA:GL_RGB;
    const GLint internalFormat=rgba?GL_RGBA8:GL_RGB8;

    // every level made on the CPU (imageMipChain), not by glGenerateMipmap, whose
    // filter is the implementation's
    const std::vector<ImageMipLevel> levels=imageMipChain(image.m_readData.data(), image.width, image.height,
        rgba?4:3);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, (GLint)levels.size());
    // RGB rows are not padded to 4 bytes
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, image.width, image.height, 0, format,
        GL_UNSIGNED_BYTE, (const GLvoid *)image.m_readData.data());
    for(size_t i=0; i<levels.size(); ++i)
        glTexImage2D(GL_TEXTURE_2D, (GLint)(i+1), internalFormat, levels[i].width, levels[i].height, 0, format,
            GL_UNSIGNED_BYTE, (const GLvoid *)levels[i].texels.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    if(texture >= m_textureSizes.size())
        m_textureSizes.resize(texture+1);

    m_textureSizes[texture].x=image.width;
    m_textureSizes[texture].y=image.height;

    setTexture(texture);
    return texture;
}

void OpenGLCore::destroyImageTexture(TextureHandle texture)
{
    // setTexture() is skipped while m_currentTexture matches, and a later
    // glGenTextures may hand the deleted name out again
    if(texture == m_currentTexture)
    {
        m_currentTexture=0;
        m_currentTextureSize=glm::uvec2(0, 0);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &texture);
}

bool OpenGLCore::readTextureRgba(TextureHandle texture, int width, int height, std::vector<uint8_t> &rgba)
{
    return readTexture(texture, width, height, rgba);
}

void OpenGLCore::beginPrimitiveDraws()
{
    glUseProgram(m_program);

    glUniformMatrix4fv(
        m_projectionUniform,
        1,
        GL_FALSE,
        glm::value_ptr(m_projectionMatrix)
    );

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_clipTexture);
    for(size_t i=0; i<ImageUnits; ++i)
    {
        glActiveTexture(GL_TEXTURE0+FirstImageUnit+(GLenum)i);
        glBindTexture(GL_TEXTURE_2D, m_imageUnits[i]);
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_currentTexture);
}

void OpenGLCore::drawRanges(const ChunkDraw &draw)
{
    static_assert(sizeof(uintptr_t) == sizeof(const void *), "the offsets are handed to GL as pointers");

    // one draw of the chunk's ranges either way: GLES 3.0 has no
    // glMultiDrawElements, and its loop draws the same
    if(m_useMultiDraw)
    {
#ifdef GWENI_GL_ES
        multiDrawElementsEXT(GL_TRIANGLES, draw.counts.data(), (GLenum)GL_UNSIGNED_SHORT,
            reinterpret_cast<const void *const *>(draw.offsets.data()), (GLsizei)draw.counts.size());
#else
        glMultiDrawElements(GL_TRIANGLES, draw.counts.data(), (GLenum)GL_UNSIGNED_SHORT,
            reinterpret_cast<const void *const *>(draw.offsets.data()), (GLsizei)draw.counts.size());
#endif
    }
    else
    {
        for(size_t range=0; range<draw.counts.size(); ++range)
            glDrawElements(GL_TRIANGLES, draw.counts[range], (GLenum)GL_UNSIGNED_SHORT,
                reinterpret_cast<const void *>(draw.offsets[range]));
    }
}

void OpenGLCore::drawPrimitiveChunk(const ChunkDraw &draw)
{
    // the chunk's vertex array points the attributes at its first slot
    glBindVertexArray(primitiveVertexArray(draw.chunk));
    glUniform1i(m_slotBaseUniform, (GLint)(draw.chunk*indexChunkSlots()));
    drawRanges(draw);
}

void OpenGLCore::endPrimitiveDraws()
{
    glBindVertexArray(0);
}

void OpenGLCore::beginTextDraws()
{
    glUseProgram(m_textProgram);

    glUniformMatrix4fv(
        m_textProjectionUniform,
        1,
        GL_FALSE,
        glm::value_ptr(m_projectionMatrix)
    );

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_textClipTexture);
    glActiveTexture(GL_TEXTURE0);

    // An LCD atlas's runs blend each channel by its own coverage, the fragment
    // shader's second output; the blend in place before the first of them is
    // put back for the other atlases and after the pass.
    std::fill(std::begin(m_textBlend), std::end(m_textBlend), 0);
    m_lcdBlend=false;

    glUniform1f(m_textContrastUniform, textContrast());
}

void OpenGLCore::beginTextAtlas(const FontData &fontData)
{
    const bool lcd=(fontData.mode == TextMode::LcdRGB) || (fontData.mode == TextMode::LcdBGR);

    // without dual-source blending an LCD atlas is blended by its mean
    // coverage, as a grayscale one is (the text shader then has no second
    // output); GLES contexts without it are the GLES tree's to fall back on
    if(lcd && !m_lcdBlend && m_glCapabilities.dualSourceBlend)
    {
        glGetIntegerv(GL_BLEND_SRC_RGB, &m_textBlend[0]);
        glGetIntegerv(GL_BLEND_DST_RGB, &m_textBlend[1]);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &m_textBlend[2]);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &m_textBlend[3]);
        glBlendFunc(GL_SRC1_COLOR, GL_ONE_MINUS_SRC1_COLOR);
        m_lcdBlend=true;
    }
    else if(!lcd && m_lcdBlend)
    {
        glBlendFuncSeparate(m_textBlend[0], m_textBlend[1], m_textBlend[2], m_textBlend[3]);
        m_lcdBlend=false;
    }

    glBindTexture(GL_TEXTURE_2D, fontData.atlas->id);
    glUniform1i(m_textModeUniform, (GLint)fontData.mode);
}

void OpenGLCore::drawTextChunk(const ChunkDraw &draw)
{
    glBindVertexArray(textVertexArray(draw.chunk));
    glUniform1i(m_textSlotBaseUniform, (GLint)(draw.chunk*indexChunkSlots()));
    drawRanges(draw);
}

void OpenGLCore::endTextDraws()
{
    if(m_lcdBlend)
        glBlendFuncSeparate(m_textBlend[0], m_textBlend[1], m_textBlend[2], m_textBlend[3]);
    m_lcdBlend=false;

    glBindVertexArray(0);
}

void OpenGLCore::submitFrame()
{
    glBindVertexArray(0);

    glFlush();
}

void OpenGLCore::indexChunkSlotsChanged()
{
    // their attributes start at the old chunk size's slots; made again as the next
    // flush() needs them
    glBindVertexArray(0);
    for(std::vector<uint32_t> *vertexArrays:{ &m_vertexArrays, &m_textVertexArrays })
    {
        if(vertexArrays->empty())
            continue;
        glDeleteVertexArrays((GLsizei)vertexArrays->size(), vertexArrays->data());
        vertexArrays->clear();
    }
}

OpenGLCore::GLObjectNames OpenGLCore::getGLObjectNames() const
{
    GLObjectNames names;

    names.buffers={m_vertexBuffer, m_quadIndexBuffer, m_textVertexBuffer, m_textQuadIndexBuffer};
    names.vertexArrays=std::vector<unsigned int>(m_vertexArrays.begin(), m_vertexArrays.end());
    names.vertexArrays.insert(names.vertexArrays.end(), m_textVertexArrays.begin(), m_textVertexArrays.end());
    names.textures={m_clipTexture, m_textClipTexture};
    names.programs={m_program, m_textProgram};
    return names;
}

//-------------------------------------------------------------------------------

OpenGLCore::OpenGLCore(ResourcePaths &paths, const Rect &viewRect)
    : SlotRenderer(paths, viewRect)
    , m_context(nullptr)
    , m_currentTexture(0)
{
    // the draws' loop of glDrawElements even where glMultiDrawElements exists
    if(const char *noMultiDraw=getenv("GWENI_NO_MULTI_DRAW"))
        m_noMultiDraw=(strcmp(noMultiDraw, "1") == 0);
    // the context taken to have no dual-source blending, as a GLES one without
    // GL_EXT_blend_func_extended has none: LCD text falls back to grayscale
    if(const char *noDualSource=getenv("GWENI_GL_NO_DUAL_SOURCE"))
        m_noDualSource=(strcmp(noDualSource, "1") == 0);
}

OpenGLCore::~OpenGLCore()
{
    // each font's atlas texture and its freetype-gl manager, then each image's texture
    releaseResources();
    m_currentTexture=0;

    // nothing below exists before init()
    if(m_quadIndexBuffer == 0)
        return;

    glBindVertexArray(0);
    // a program still in use is only flagged for deletion
    glUseProgram(0);

    const GLuint buffers[]={m_vertexBuffer, m_quadIndexBuffer, m_textVertexBuffer, m_textQuadIndexBuffer};
    const GLuint textures[]={m_clipTexture, m_textClipTexture};

    // deleting a bound object unbinds it, so another renderer in this context
    // finds nothing of these left bound; it binds its own every flush()
    if(!m_vertexArrays.empty())
        glDeleteVertexArrays((GLsizei)m_vertexArrays.size(), m_vertexArrays.data());
    if(!m_textVertexArrays.empty())
        glDeleteVertexArrays((GLsizei)m_textVertexArrays.size(), m_textVertexArrays.data());
    glDeleteBuffers(4, buffers);
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
    const Rect &view=viewRect();

    m_projectionMatrix=viewProjection(view);

    // nothing of the mirror is on the buffers below yet, and the view is the clip region
    SlotRenderer::init();

    m_glCapabilities=detectCapabilities();
    if(m_noDualSource)
        m_glCapabilities.dualSourceBlend=false;
    m_useMultiDraw=m_glCapabilities.multiDraw && !m_noMultiDraw;
    // GWENI_TEXT_AA=rgb|bgr, read at construction, falls back to grayscale here
    // if the context cannot blend it
    applyLcdTextSupport();

    // Creating buffers. A name from glGenBuffers is a buffer once it is first bound,
    // and that first target is the only one it is ever bound to (WebGL holds a buffer
    // to it): the vertex buffers are GL_ARRAY_BUFFERs, bound here so they exist from
    // init() on, and the quad index buffers GL_ELEMENT_ARRAY_BUFFERs, below.
    glGenBuffers(1, &m_vertexBuffer);
    glGenBuffers(1, &m_quadIndexBuffer);
    glGenBuffers(1, &m_textVertexBuffer);
    glGenBuffers(1, &m_textQuadIndexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_textVertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

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
    // Box from a 2D texture, one RGBA16UI texel per slot, kClipColumns slots a row
    // (see uploadDirtyClipBoxes()). An integer texture is only complete with nearest
    // filtering, and is read with texelFetch alone. Made on unit 1, where the draws
    // bind them, so unit 0's setTexture() binding stays as it is.
    glGenTextures(1, &m_clipTexture);
    glGenTextures(1, &m_textClipTexture);
    glActiveTexture(GL_TEXTURE1);
    for(GLuint texture:{ m_clipTexture, m_textClipTexture })
    {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glActiveTexture(GL_TEXTURE0);

    // Loading shaders, in the dialect of the context
    const GLSLDialect dialect=m_glCapabilities.dialect;
    const bool dualSource=m_glCapabilities.dualSourceBlend;

    m_program=loadShaders(shaderSource(GLShaderSource::PrimitiveVertex, dialect, dualSource).c_str(),
        shaderSource(GLShaderSource::PrimitiveFragment, dialect, dualSource).c_str());
    m_projectionUniform=glGetUniformLocation(m_program, "projection");
    m_imagesUniform=glGetUniformLocation(m_program, "images");
    m_clipBoxesUniform=glGetUniformLocation(m_program, "clipBoxes");
    m_slotBaseUniform=glGetUniformLocation(m_program, "slotBase");

    m_textProgram=loadShaders(shaderSource(GLShaderSource::TextVertex, dialect, dualSource).c_str(),
        shaderSource(GLShaderSource::TextFragment, dialect, dualSource).c_str());
    m_textProjectionUniform=glGetUniformLocation(m_textProgram, "projection");
    m_textClipBoxesUniform=glGetUniformLocation(m_textProgram, "clipBoxes");
    m_textModeUniform=glGetUniformLocation(m_textProgram, "textMode");
    m_textContrastUniform=glGetUniformLocation(m_textProgram, "textContrast");
    m_textSlotBaseUniform=glGetUniformLocation(m_textProgram, "slotBase");

    // texture1 stays on unit 0; the clip boxes are on unit 1 for both programs, and
    // the primitive program's images on FirstImageUnit on. A uniform is set in the
    // program in use; flush() binds its own every frame.
    GLint imageUnits[ImageUnits];

    for(size_t i=0; i<ImageUnits; ++i)
        imageUnits[i]=FirstImageUnit+(GLint)i;
    glUseProgram(m_program);
    glUniform1i(m_clipBoxesUniform, 1);
    glUniform1iv(m_imagesUniform, (GLsizei)ImageUnits, imageUnits);
    glUseProgram(m_textProgram);
    glUniform1i(m_textClipBoxesUniform, 1);
    glUseProgram(0);

    glViewport(view.x, view.y, view.w, view.h);
}

namespace
{

// The bodies of the programs' stages (source/platforms/renderers/shaders/*.glsl,
// embedded at build time by cmake/embedFile.cmake), shared by every GLSLDialect and
// by the Vulkan backend: OpenGLCore::shaderSource puts the dialect's #version (and,
// for ES, precision) header and GL's prologue in front. They keep to what GLSL 3.30,
// GLSL ES 3.00 and Vulkan GLSL 4.50 all have: no implicit conversions, texture() for
// every sample, no buffer textures, no gl_PrimitiveID.
std::string embeddedText(const unsigned char *data, size_t size)
{
    return std::string(reinterpret_cast<const char *>(data), size);
}

// GL's prologue of each stage: what the bodies leave to the backend. The uniforms
// are found by name (init()); VERTEX_ID is gl_VertexID, which counts from the
// chunk's first vertex because each chunk's vertex array points there; and the
// clip space is GL's own.
const char *primitiveVertexPrologue=
    "#define VERTEX_ID gl_VertexID\n"
    "#define CLIP_SPACE_TO_API(p)\n"
    "uniform mat4 projection;\n"
    "uniform int slotBase;\n";
const char *primitiveFragmentPrologue=
    "uniform sampler2D images[8];\n"
    "uniform usampler2D clipBoxes;\n";
const char *textVertexPrologue=
    "#define VERTEX_ID gl_VertexID\n"
    "#define CLIP_SPACE_TO_API(p)\n"
    "uniform mat4 projection;\n"
    "uniform int slotBase;\n";
const char *textFragmentPrologue=
    "uniform sampler2D texture1;\n"
    "uniform usampler2D clipBoxes;\n"
    "uniform int textMode;\n"
    "uniform float textContrast;\n";

}//namespace

std::string OpenGLCore::shaderSource(GLShaderSource source, GLSLDialect dialect, bool dualSource)
{
    const bool fragment=(source == GLShaderSource::PrimitiveFragment) || (source == GLShaderSource::TextFragment);
    const bool secondOutput=dualSource && (source == GLShaderSource::TextFragment);
    std::string text;

    if(dialect == GLSLDialect::Es300)
    {
        text="#version 300 es\n";
        if(secondOutput)
            text+="#extension GL_EXT_blend_func_extended : require\n";
        // highp everywhere, as desktop GLSL computes: ES's fragment defaults (mediump
        // int, lowp samplers) would not give the desktop trees' bytes
        text+="precision highp float;\n"
            "precision highp int;\n"
            "precision highp sampler2D;\n"
            "precision highp usampler2D;\n";
    }
    else
        text="#version 330 core\n";

    if(fragment)
    {
        text+="#define CLIP_COLUMNS_LOG2 "+std::to_string(kClipColumnsLog2)+"u\n";
        text+="#define CLIP_COLUMNS "+std::to_string(kClipColumns)+"u\n";
    }
    // how far an SDF glyph's field reaches past each edge, in atlas texels
    if(source == GLShaderSource::TextFragment)
        text+="#define SDF_SPREAD "+std::to_string(kSdfSpread)+".0\n";
    if(secondOutput)
        text+="#define GWENI_DUAL_SOURCE 1\n";

    switch(source)
    {
    case GLShaderSource::PrimitiveVertex:
        text+=primitiveVertexPrologue;
        text+=embeddedText(primitiveVertexGlsl, primitiveVertexGlslSize);
        break;
    case GLShaderSource::PrimitiveFragment:
        text+=primitiveFragmentPrologue;
        text+=embeddedText(primitiveFragmentGlsl, primitiveFragmentGlslSize);
        break;
    case GLShaderSource::TextVertex:
        text+=textVertexPrologue;
        text+=embeddedText(textVertexGlsl, textVertexGlslSize);
        break;
    case GLShaderSource::TextFragment:
        text+=textFragmentPrologue;
        text+=embeddedText(textFragmentGlsl, textFragmentGlslSize);
        break;
    }
    return text;
}

OpenGLCore::GLCapabilities OpenGLCore::detectCapabilities()
{
    GLCapabilities capabilities;
    const GLubyte *version=glGetString(GL_VERSION);

    // an ES context's GL_VERSION is "OpenGL ES N.M ..."
    if(version && (strncmp(reinterpret_cast<const char *>(version), "OpenGL ES", 9) == 0))
    {
        capabilities.dialect=GLSLDialect::Es300;
        // GLES 3.0 has neither; GL_EXT_multi_draw_arrays and GL_EXT_blend_func_extended
        // give them. The multi-draw's entry point is EGL's to hand out, so only the
        // GLES tree, which links EGL, draws through it.
        capabilities.multiDraw=false;
#if defined(GWENI_GL_ES) && !defined(__EMSCRIPTEN__)
        if(hasExtension("GL_EXT_multi_draw_arrays"))
        {
            multiDrawElementsEXT=reinterpret_cast<PFNGLMULTIDRAWELEMENTSEXTPROC>(
                eglGetProcAddress("glMultiDrawElementsEXT"));
            capabilities.multiDraw=(multiDrawElementsEXT != nullptr);
        }
#endif
        capabilities.dualSourceBlend=hasExtension("GL_EXT_blend_func_extended");
    }
    else
    {
        // both are core in GL 3.3
        capabilities.dialect=GLSLDialect::Glsl330;
        capabilities.multiDraw=true;
        capabilities.dualSourceBlend=true;
    }
    return capabilities;
}

bool OpenGLCore::hasExtension(const char *name)
{
    GLint count=0;

    glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    for(GLint i=0; i<count; ++i)
    {
        const GLubyte *extension=glGetStringi(GL_EXTENSIONS, (GLuint)i);

        if(extension && (strcmp(reinterpret_cast<const char *>(extension), name) == 0))
            return true;
    }
    return false;
}

const char *OpenGLCore::lcdTextUnavailable() const
{
    if(m_glCapabilities.dualSourceBlend)
        return nullptr;
    if(m_noDualSource)
        return "LCD text needs dual-source blending, which GWENI_GL_NO_DUAL_SOURCE=1 turned off";
    return "LCD text needs dual-source blending (GL_EXT_blend_func_extended), which this context does not have";
}

void OpenGLCore::setUseMultiDraw(bool multiDraw)
{
    m_useMultiDraw=multiDraw && m_glCapabilities.multiDraw;
}

void OpenGLCore::setViewRect(const gweni::Rect &viewRect)
{
    SlotRenderer::setViewRect(viewRect);

    m_projectionMatrix=viewProjection(viewRect);

    glViewport(viewRect.x, viewRect.y, viewRect.w, viewRect.h);
}

uint32_t OpenGLCore::primitiveVertexArray(size_t chunk)
{
    while(m_vertexArrays.size() <= chunk)
    {
        // chunk n's vertex 0 is slot n*indexChunkSlots()'s first vertex
        const size_t base=m_vertexArrays.size()*indexChunkSlots()*4*sizeof(Vertex);
        auto at=[base](size_t offset){ return reinterpret_cast<const GLvoid *>(base+offset); };
        GLuint vertexArray=0;

        glGenVertexArrays(1, &vertexArray);
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
        // chunk n's vertex 0 is glyph slot n*indexChunkSlots()'s first vertex
        const size_t base=m_textVertexArrays.size()*indexChunkSlots()*4*sizeof(glyph_vertex_t);
        auto at=[base](size_t offset){ return reinterpret_cast<const GLvoid *>(base+offset); };
        GLuint vertexArray=0;

        glGenVertexArrays(1, &vertexArray);
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

bool OpenGLCore::readTexture(unsigned int textureName, int width, int height, std::vector<uint8_t> &rgba)
{
    GLint readFramebuffer=0;
    GLint packAlignment=4;
    GLuint framebuffer=0;

    rgba.clear();
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
    glGetIntegerv(GL_PACK_ALIGNMENT, &packAlignment);
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureName, 0);

    const bool complete=(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    if(complete)
    {
        // RGBA8 rows are whole 4-byte words, so any pack alignment reads them tight
        rgba.resize(static_cast<size_t>(width)*height*4);
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        glPixelStorei(GL_PACK_ALIGNMENT, packAlignment);
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)readFramebuffer);
    glDeleteFramebuffers(1, &framebuffer);
    return complete;
}

bool OpenGLCore::textureChannelBits(unsigned int textureName, int bits[4])
{
    GLint readFramebuffer=0;
    GLuint framebuffer=0;

    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureName, 0);

    const bool complete=(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    if(complete)
    {
        const GLenum channels[4]={ GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE,
            GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE, GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE };

        for(size_t i=0; i<4; ++i)
        {
            GLint value=0;

            glGetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, channels[i], &value);
            bits[i]=value;
        }
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)readFramebuffer);
    glDeleteFramebuffers(1, &framebuffer);
    return complete;
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
        // the programs project with their own uniform; only the viewport is GL state
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
        // the programs project with their own uniform; only the viewport is GL state
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
