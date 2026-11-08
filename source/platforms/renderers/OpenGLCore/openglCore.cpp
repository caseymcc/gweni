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

#ifdef GWENI_GL_ES
namespace
{
// GL_EXT_multi_draw_arrays' glMultiDrawElementsEXT, which the text draws use in a
// GLES context listing it; detectCapabilities resolves it (on the web it stays
// null, and the text draws loop)
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

    // mipmapped, because an image is often drawn much smaller than it is; clamped,
    // so the linear filter does not wrap the opposite edge into the border texels
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // a sized internal format that matches the data, as GLES requires; an RGB one
    // samples with alpha 1, as the RGBA texture desktop GL made of RGB data did
    const GLenum format=rgba?GL_RGBA:GL_RGB;
    const GLint internalFormat=rgba?GL_RGBA8:GL_RGB8;

    // RGB rows are not padded to 4 bytes
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, image.width, image.height, 0, format,
        GL_UNSIGNED_BYTE, (const GLvoid *)image.m_readData.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glGenerateMipmap(GL_TEXTURE_2D);

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

void OpenGLCore::drawPrimitiveChunk(size_t chunk, size_t firstSlot, size_t slots)
{
    // the chunk's vertex array points the attributes at its first slot
    glBindVertexArray(primitiveVertexArray(chunk));
    glUniform1i(m_slotBaseUniform, (GLint)firstSlot);
    glDrawElements(GL_TRIANGLES, (GLsizei)(slots*6), (GLenum)GL_UNSIGNED_SHORT, 0);
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

void OpenGLCore::drawTextChunk(const TextChunkDraw &draw)
{
    static_assert(sizeof(uintptr_t) == sizeof(const void *), "the offsets are handed to GL as pointers");

    glBindVertexArray(textVertexArray(draw.chunk));
    glUniform1i(m_textSlotBaseUniform, (GLint)(draw.chunk*indexChunkSlots()));
    // one draw of the chunk's ranges either way: GLES 3.0 has no
    // glMultiDrawElements, and its loop draws the same
    if(m_multiDrawText)
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
    // the text draws' loop of glDrawElements even where glMultiDrawElements exists
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
    m_multiDrawText=m_glCapabilities.multiDraw && !m_noMultiDraw;
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

// The bodies of the programs' stages, shared by every GLSLDialect: OpenGLCore::shaderSource
// puts the dialect's #version (and, for ES, precision) header in front. They keep to
// what GLSL 3.30 and GLSL ES 3.00 both have: no implicit conversions, texture() for
// every sample, no buffer textures, no gl_PrimitiveID.
const char *primitiveVertexBody=R"(
//layout (location = 0) in vec3 inPosition;
//layout (location = 1) in vec2 inTexCoords;
//layout (location = 2) in vec4 inColor;

layout (location = 0) in uint style;
layout (location = 1) in ivec3 inPosition;   // device px (signed) and z (16 bits unsigned, read as GL_SHORT with x and y)
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
// The slot this vertex's quad is, for the fragment shader's clip box. A chunk is
// drawn from index 0 of its own slots, 4 vertexes apiece, with its attributes
// pointed at its first slot (slotBase), so gl_VertexID >> 2 is the slot within the
// chunk. Flat: the four vertexes of a slot carry the same value, so whichever is
// the provoking vertex gives it.
uniform int slotBase;
flat out uint vSlot;

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
    vSlot = uint(slotBase) + (uint(gl_VertexID) >> 2);

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
    // GL_SHORT sign-extends z; its low 16 bits are Vertex::z, 0..65535
    float z = float(inPosition.z & 0xffff);

    if(style == 0u)//item has been hidden, move all coords to 0.0f
        gl_Position = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    else
        gl_Position = projection * vec4(float(inPosition.x), float(inPosition.y), z, 1.0f);

    vec4 behind = projection * vec4(float(inPosition.x), float(inPosition.y), z - 0.5, 1.0);
    vBehindDepth = behind.z*0.5 + 0.5;
})";

const char *primitiveFragmentBody=R"(
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

// Style 2's textures, one per image unit (vRadius picks it). GLSL 3.30 and ES 3.00
// index a sampler array only with a constant, hence the switch.
uniform sampler2D images[8];
// One Box (x0, y0, x1, y1) per primitive slot, slot s at texel (s % CLIP_COLUMNS,
// s / CLIP_COLUMNS); the vertex shader gives the slot.
uniform usampler2D clipBoxes;
flat in uint vSlot;

uvec4 clipBox(uint slot)
{
    return texelFetch(clipBoxes, ivec2(int(slot & (CLIP_COLUMNS - 1u)), int(slot >> CLIP_COLUMNS_LOG2)), 0);
}

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
    uvec4 clip = clipBox(vSlot);
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
            // At a pixel's centre vLocal is a multiple of 0.5 exactly (the quad's
            // corners are whole px, vLocal is from its centre), so it is rounded to
            // that: interpolated, it carries a float error that depends on where the
            // quad is and on the rasteriser, which the stroke's anti-aliased edge
            // turned into a level of difference between the glyph drawn here and the
            // same glyph moved, or drawn by another GL.
            vec2 local = floor(vLocal*2.0 + 0.5)*0.5;
            vec2 p = local / max(vHalf, vec2(1.0));
            int dir = int(vBorder + 0.5);

            // p is turned into the down-pointing shape's frame: its apex,
            // at +y there, lands at -x for left and +x for right.
            if(dir == 1) p = vec2(p.x, -p.y);       // up
            else if(dir == 2) p = vec2(p.y, -p.x);  // left
            else if(dir == 3) p = vec2(-p.y, p.x);  // right
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

            // aa is one device pixel in p's units: vLocal is in device px, so a
            // pixel is 1 over the half size p is divided by. Not fwidth(d): a
            // derivative of d is implementation-defined (fine or coarse, per 2x2
            // block), so the stroke's edge differed by up to 82/255 between llvmpipe
            // and the browser's SwiftShader; nor vLocal's own derivative, whose float
            // rounding depends on where the quad is, which moved a pixel of a glyph
            // drawn across the view's edge.
            vec2 h = max(vHalf, vec2(1.0));
            float aa = 0.5*(1.0/h.x + 1.0/h.y);
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

const char *textVertexBody=R"(
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

    // On a 1/16 px grid, the coarsest sub-pixel grid GL guarantees (GL_SUBPIXEL_BITS
    // is at least 4), so no rasteriser snaps it further: an SDF glyph's pen is
    // fractional, and llvmpipe and the browser's SwiftShader snapped its quad to
    // grids of their own, which moved the field under the pixels by up to 1/32 px
    // and its anti-aliased edge by up to 10/255. Coverage and LCD glyphs are on
    // whole pixels already.
    gl_Position = projection * vec4(floor(vertPosition.xy*16.0 + 0.5)/16.0, vertPosition.z, 1.0f);
}
)";

const char *textFragmentBody=R"(
// Dual-source: FragColor is the colour, Coverage what flush() blends an LCD
// atlas's runs with (GL_SRC1_COLOR), one weight per channel. The other atlases'
// SRC_ALPHA blend does not read Coverage. Without dual-source blending there is
// no second output, and Coverage goes nowhere.
#ifdef GWENI_DUAL_SOURCE
layout(location = 0, index = 0) out vec4 FragColor;
layout(location = 0, index = 1) out vec4 Coverage;
#else
layout(location = 0) out vec4 FragColor;
vec4 Coverage;
#endif

in vec2 texCoord;
in vec4 color;
in float gamma;
in vec2 pixel;
flat in uint slot;

uniform sampler2D texture1;
// One Box (x0, y0, x1, y1) per glyph slot, slot s at texel (s % CLIP_COLUMNS,
// s / CLIP_COLUMNS). Each atlas draws its own runs, some of the slots of each
// chunk, so the vertex shader gives the slot.
uniform usampler2D clipBoxes;

uvec4 clipBox(uint slot)
{
    return texelFetch(clipBoxes, ivec2(int(slot & (CLIP_COLUMNS - 1u)), int(slot >> CLIP_COLUMNS_LOG2)), 0);
}
// TextMode of the atlas this draw samples: 0 coverage, 1 signed distance field,
// 2 subpixel coverage in r, g, b order, 3 in b, g, r order
uniform int textMode;
// The LCD modes' contrast boost for white text (GWENI_TEXT_CONTRAST); 0 is none
uniform float textContrast;
// the SDF edge's anti-aliasing half-width, in screen pixels
const float SDF_AA = 0.7;
void main()
{
    uvec4 clip = clipBox(slot);
    bool clipped = any(lessThan(pixel, vec2(clip.xy))) || any(greaterThanEqual(pixel, vec2(clip.zw)));

    float a = 0.0;

    if(textMode == 0)
    {
        a = texture(texture1, texCoord).r;

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
        // The field is 0.5 on the glyph's edge and changes by 1/(2*SDF_SPREAD) per
        // atlas texel; w is how much it changes over one screen pixel here, so the
        // edge is anti-aliased over about a pixel at any size, as style 4's rounded
        // rect is. Both are computed so that every GL gives the same value, which
        // llvmpipe and the browser's SwiftShader did not, by up to 78/255 at an edge:
        // - the field is filtered here, bilinearly in float from the four texels
        //   around texCoord, as GL_LINEAR does: a texture unit keeps its filter's
        //   weights to a few bits of its own, and smoothstep's narrow w multiplies
        //   that difference by up to about ten at a large size;
        // - w is the field's change per texel times texCoord's screen gradient in
        //   texels, not d's own gradient: a derivative of the sampled field is
        //   implementation-defined (fine or coarse, per 2x2 block), while texCoord
        //   is linear over the glyph's quad, so its derivative is the same anywhere.
        vec2 texels = vec2(textureSize(texture1, 0));
        vec2 t = texCoord*texels - 0.5;
        vec2 f = fract(t);
        ivec2 last = ivec2(texels) - 1;
        ivec2 i0 = clamp(ivec2(floor(t)), ivec2(0), last);
        ivec2 i1 = clamp(ivec2(floor(t)) + 1, ivec2(0), last);
        float d = mix(mix(texelFetch(texture1, i0, 0).r, texelFetch(texture1, ivec2(i1.x, i0.y), 0).r, f.x),
                      mix(texelFetch(texture1, ivec2(i0.x, i1.y), 0).r, texelFetch(texture1, i1, 0).r, f.x), f.y);
        float texelsPerPixel = 0.5*(length(dFdx(texCoord)*texels) + length(dFdy(texCoord)*texels));
        float w = max(SDF_AA*texelsPerPixel/(2.0*SDF_SPREAD), 1e-4);

        a = smoothstep(0.5-w, 0.5+w, d);
        FragColor = vec4(color.rgb, color.a*a);
        Coverage = vec4(0.0);
    }

    // after the sample, as in the primitive shader: its implicit derivatives want the quad
    if(clipped)
        discard;
    // Where a glyph draws nothing it must not write depth either: an SDF glyph's quad
    // reaches the spread past its ink, over its neighbours and the next row, and a
    // coverage glyph's box overlaps the next glyph's, which in a long run can sit at
    // the same depth (glyph i is at z+(i+1)/(glyphs+1)). a is 0 exactly where every
    // mode's colour and coverage are 0.
    if(a <= 0.0)
        discard;
}
)";

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
    case GLShaderSource::PrimitiveVertex: text+=primitiveVertexBody; break;
    case GLShaderSource::PrimitiveFragment: text+=primitiveFragmentBody; break;
    case GLShaderSource::TextVertex: text+=textVertexBody; break;
    case GLShaderSource::TextFragment: text+=textFragmentBody; break;
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

void OpenGLCore::setMultiDrawText(bool multiDraw)
{
    m_multiDrawText=multiDraw && m_glCapabilities.multiDraw;
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
