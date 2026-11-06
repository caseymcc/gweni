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
static constexpr size_t minIndexSize=minPrimitiveSize*6;
static constexpr size_t minVertexSize=minPrimitiveSize*4;
static constexpr size_t minClippingSize=minPrimitiveSize;

GLTextureData::~GLTextureData()
{
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, reinterpret_cast<GLuint *>(&m_textureId));
}

Font::Status OpenGLCore::loadFont(const Font &font)
{
    GLFontData fontData;
    static const char *cache = " !\"#$%&'()*+,-./0123456789:;<=>?"
                         "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
                         "`abcdefghijklmnopqrstuvwxyz{|}~";

    m_lastFont=nullptr;

    const String filename=getResourcePaths().getPath(ResourcePaths::Type::Font, font.facename);

    vec4 black={{0.0, 0.0, 0.0, 1.0}};
    vec4 none={{1.0, 1.0, 1.0, 0.0}};

    fontData.faceName=font.facename;
    // markup_t keeps the family as a borrowed char*, and the markup outlives this
    // function, so it cannot point at the local filename.
    fontData.familyPath=strdup(filename.c_str());
    fontData.markup.family=fontData.familyPath;
    fontData.markup.size=font.size;
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

    fontData.fontManager=ftgl::font_manager_new(1024, 1024, LCD_FILTERING_ON);

    free(fontData.fontManager->cache); // font_manager_new strdup'd a default
    fontData.fontManager->cache=strdup(cache);

    fontData.markup.font=ftgl::font_manager_get_from_markup(fontData.fontManager, &fontData.markup);
    fontData.textureFont=fontData.markup.font;

    fontData.atlas=fontData.fontManager->atlas;

    glGenTextures(1, &fontData.atlas->id);
    glBindTexture(GL_TEXTURE_2D, fontData.atlas->id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, fontData.atlas->width,
        fontData.atlas->height, 0, GL_RGB, GL_UNSIGNED_BYTE,
        fontData.atlas->data);
    //the whole atlas, cache included, is on the GPU now; flush() uploads only later glyphs
    fontData.atlas->dirty_y0=0;
    fontData.atlas->dirty_y1=0;

    m_lastFont=&(*m_fonts.insert(std::make_pair(font, std::move(fontData))).first);
    return Font::Status::Loaded;
}

void OpenGLCore::freeFont(const gweni::Font &font)
{
    if(m_lastFont != nullptr && m_lastFont->first == font)
        m_lastFont=nullptr;

    m_fonts.erase(font); // calls GLFontData destructor
}

bool OpenGLCore::ensureFont(const Font &font)
{
    if(m_lastFont != nullptr)
    {
        if(m_lastFont->first == font)
            return true;
    }

    // Was it loaded before?
    auto it=m_fonts.find(font);
    if(it != m_fonts.end())
    {
        m_lastFont=&(*it);
        return true;
    }

    // No, try load to it

    // LoadFont sets m_lastFont, if loaded
    return loadFont(font) == Font::Status::Loaded;
}

Texture::Status OpenGLCore::loadTexture(Texture &texture)
{
    freeTexture(texture);
    m_lastTexture=std::numeric_limits<size_t>::max();

    const String filename=getResourcePaths().getPath(ResourcePaths::Type::Texture, texture.name);

    GLTextureData &texData=*getFreeTextureData();

    imglib::load(texData, filename.c_str());

    //    int width, height, n;
    //    {
    //        unsigned char *image = stbi_load(filename.c_str(), &width, &height, &n, 4);
    //        texData.m_readData = deleted_unique_ptr<unsigned char>(image, [](unsigned char* mem) { if (mem) stbi_image_free(mem); });
    //    }

        // Image failed to load..
    if(texData.m_readData.empty())
    {
        gweni::log::write(log::Level::Error, "Texture file not found: %s", filename.c_str());
        return Texture::Status::ErrorFileNotFound;
    }


    // Create the opengl texture
    glGenTextures(1, &texData.m_textureId);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texData.m_textureId);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    imglib::Format imgFormat=imglib::format(texData);
    GLenum format;

    if(imgFormat == imglib::Format::RGBA)
        format=GL_RGBA;
    else if(imgFormat == imglib::Format::RGB)
        format=GL_RGB;
    else
    {
        glDeleteTextures(1, &texData.m_textureId);
        return Texture::Status::ErrorBadData;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texData.width, texData.height, 0, format,
        GL_UNSIGNED_BYTE, (const GLvoid *)texData.m_readData.data());

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

    if((texture.dataIndex != std::numeric_limits<size_t>::max()) && (m_textureData[texture.dataIndex]))
    {
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
//    , m_indexes(minIndexSize)
//    , m_vertexes(minVertexSize)
    , m_clipping(false)
//    , m_clippingBoxes(minClippingSize)
{
    m_indexes.reserve(minIndexSize);
    m_vertexes.reserve(minVertexSize);

    m_clippingBoxes.reserve(minClippingSize);

    m_uploadStats=(getenv("GWENI_UPLOAD_STATS") != nullptr);
    m_textDebug=getenv("GWENI_TEXT_DEBUG");
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
{}

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
    m_indexBufferCapacity=0;
    m_clipBufferCapacity=0;
    m_textVertexBufferCapacity=0;
    m_textIndexBufferCapacity=0;

    // Creating buffers
    glCreateVertexArrays(1, &m_vertexArray);
    glCreateBuffers(1, &m_vertexBuffer);
    glCreateBuffers(1, &m_indexBuffer);
    glCreateBuffers(1, &m_clipBuffer);

    //setup vertex array
    glBindVertexArray(m_vertexArray);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);

    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
    size_t attrib=0;
    glEnableVertexAttribArray(attrib); // Attrib '0' is render type
    glVertexAttribIPointer(attrib++, 1, GL_UNSIGNED_SHORT, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, style)));
    glEnableVertexAttribArray(attrib); // Attrib '1' is vertex pos
    glVertexAttribIPointer(attrib++, 3, GL_UNSIGNED_SHORT, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, x)));
    glEnableVertexAttribArray(attrib); // Attrib '2' is texture coord or packeted color0
    glVertexAttribIPointer(attrib++, 2, GL_UNSIGNED_SHORT, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, tx)));
    glEnableVertexAttribArray(attrib); // Attrib '3' is SDF local position (signed)
    glVertexAttribIPointer(attrib++, 2, GL_SHORT, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, lx)));
    glEnableVertexAttribArray(attrib); // Attrib '4' is SDF radius + border
    glVertexAttribIPointer(attrib++, 2, GL_UNSIGNED_SHORT, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, radius)));
    glEnableVertexAttribArray(attrib); // Attrib '5' is SDF gradient color1 (packed)
    glVertexAttribIPointer(attrib++, 2, GL_UNSIGNED_SHORT, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, cx)));
    glEnableVertexAttribArray(attrib); // Attrib '6' is SDF border color (packed)
    glVertexAttribIPointer(attrib++, 2, GL_UNSIGNED_SHORT, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, bx)));
    glEnableVertexAttribArray(attrib); // Attrib '7' is color3 (packed): bilinear bottom edge / checker dark
    glVertexAttribIPointer(attrib++, 2, GL_UNSIGNED_SHORT, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, dx)));

//    glBindBuffer(GL_ARRAY_BUFFER, m_clipBuffer);
//    glEnableVertexAttribArray(2); // Attrib '3' is show
//    glVertexAttribIPointer(2, 1, GL_UNSIGNED_SHORT, sizeof(Box), (GLvoid*)(offsetof(Box, state)));
//    glVertexAttribDivisor(2, 1);
//    glEnableVertexAttribArray(3); // Attrib '4' is clipping
//    glVertexAttribIPointer(3, 4, GL_UNSIGNED_SHORT, sizeof(Box), (GLvoid*)(offsetof(Box, x0)));
//    glVertexAttribDivisor(3, 1);

    glBindVertexArray(0);

    //setup text vertex array
    glCreateVertexArrays(1, &m_textVertexArray);
    glCreateBuffers(1, &m_textVertexBuffer);
    glCreateBuffers(1, &m_textIndexBuffer);

    glBindVertexArray(m_textVertexArray);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_textIndexBuffer);

    glBindBuffer(GL_ARRAY_BUFFER, m_textVertexBuffer);
    glEnableVertexAttribArray(0); // Attrib '0' is position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glyph_vertex_t), (GLvoid*)(offsetof(glyph_vertex_t, x)));
    glEnableVertexAttribArray(1); // Attrib '1' is texture coords
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(glyph_vertex_t), (GLvoid*)(offsetof(glyph_vertex_t, u)));
    glEnableVertexAttribArray(2); // Attrib '2' is color
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(glyph_vertex_t), (GLvoid*)(offsetof(glyph_vertex_t, r)));
    glEnableVertexAttribArray(3); // Attrib '3' is gamma
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(glyph_vertex_t), (GLvoid*)(offsetof(glyph_vertex_t, gamma)));
    // freetype-gl's subpixel `shift` is not read: the fragment shader samples one channel.

    glBindVertexArray(0);

    // Loading shaders
    auto vertexSource=R"(
#version 330 core

//layout (location = 0) in vec3 inPosition;
//layout (location = 1) in vec2 inTexCoords;
//layout (location = 2) in vec4 inColor;

layout (location = 0) in uint style;
layout (location = 1) in uvec3 inPosition;
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

uniform mat4 projection;
uniform uvec2 textureSize;

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

    TexCoords = vec2(inTexCoords)/vec2(textureSize);
    VertexColor = unpackColor(inTexCoords);

    vLocal = vec2(inLocal);
    vHalf = abs(vec2(inLocal));
    vRadius = float(inRadiusBorder.x);
    vBorder = float(inRadiusBorder.y);
    vColor1 = unpackColor(inColor1);
    vBorderColor = unpackColor(inBorderColor);
    vColor3 = unpackColor(inColor3);

    if(style == 0u)//item has been hidden, move all coords to 0.0f
        gl_Position = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    else
        gl_Position = projection * vec4(float(inPosition.x), float(inPosition.y), float(inPosition.z), 1.0f);
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

uniform sampler2D texture1;

// Signed distance to a rounded box centered at the origin (negative inside).
float sdRoundBox(vec2 p, vec2 b, float r)
{
    vec2 q = abs(p) - b + vec2(r);
    return min(max(q.x, q.y), 0.0) + length(max(q, vec2(0.0))) - r;
}

// Rounded-rect mask of styles 4, 6 and 7: applies the border to fill and the
// anti-aliased outer edge to its alpha.
vec4 roundedRect(vec4 fill)
{
    float r = clamp(vRadius, 0.0, min(vHalf.x, vHalf.y));
    float d = sdRoundBox(vLocal, vHalf, r);
    float aa = max(fwidth(d), 1e-4);
    float shapeA = 1.0 - smoothstep(-aa, aa, d);   // outer edge coverage

    vec3 rgb = fill.rgb;
    if(vBorder > 0.0)
    {
        // -d is the inward distance from the edge; border occupies [0, vBorder].
        float bMix = 1.0 - smoothstep(vBorder - aa, vBorder + aa, -d);
        rgb = mix(fill.rgb, vBorderColor.rgb, clamp(bMix, 0.0, 1.0));
    }
    return vec4(rgb, fill.a * shapeA);
}

// Distance from p to the segment a-b (for stroked icon glyphs).
float sdSegment(vec2 p, vec2 a, vec2 b)
{
    vec2 pa = p - a;
    vec2 ba = b - a;
    float h = clamp(dot(pa, ba)/dot(ba, ba), 0.0, 1.0);
    return length(pa - ba*h);
}

void main()
{
    if(ShaderType == 0u)
        FragColor = vec4(0.0, 0.0, 0.0, 0.0);
    else if(ShaderType == 1u)
        FragColor = VertexColor;
    else if (ShaderType == 2u)
    {
        FragColor = texture(texture1, TexCoords);// * VertexColor;
        FragColor.a=1.0f;
    }
    else if (ShaderType == 3u)
    {
        float color = texture(texture1, TexCoords).r;
        FragColor = vec4(VertexColor.rgb, color);
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
        // Icon glyph: normalized local coords in [-1,1]; vRadius=kind, vBorder=dir.
        vec2 p = vLocal / max(vHalf, vec2(1.0));
        int kind = int(vRadius + 0.5);
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
        float a = 1.0 - smoothstep(0.0, aa, d);
        FragColor = vec4(VertexColor.rgb, VertexColor.a * a);
    }
})";

    m_program=loadShaders(vertexSource, fragmentTexturedSource);
    m_projectionUniform=glGetUniformLocation(m_program, "projection");
    m_textureSizeUniform=glGetUniformLocation(m_program, "textureSize");

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

void main()
{
    texCoord=vertTexCoord;
    color=vertColor;
    gamma=vertGamma;

    gl_Position = projection * vec4(vertPosition, 1.0f);
}
    )";

    auto textFragSource=R"(
#version 330 core

out vec4 FragColor;

in vec2 texCoord;
in vec4 color;
in float gamma;

uniform sampler2D texture1;
void main()
{
    float a = texture2D(texture1, texCoord).r;

    FragColor = color * pow( a, 1.0/gamma );
}
    )";

    m_textProgram=loadShaders(textVertexSource, textFragSource);
    m_textProjectionUniform=glGetUniformLocation(m_textProgram, "projection");

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

void OpenGLCore::ensurePrimitiveSlots(size_t primitiveId)
{
    if(m_clippingBoxes.size() < primitiveId+1)
        m_clippingBoxes.resize(primitiveId+1);

    if(m_indexes.size() < primitiveId*6+6)
        m_indexes.resize(primitiveId*6+6);

    if(m_vertexes.size() < primitiveId*4+4)
        m_vertexes.resize(primitiveId*4+4);
}

void OpenGLCore::readPrimitiveSlot(size_t primitiveId, PrimitiveSlot &slot) const
{
    memcpy(slot.vertexes, &m_vertexes[primitiveId*4], sizeof(slot.vertexes));
    memcpy(slot.indexes, &m_indexes[primitiveId*6], sizeof(slot.indexes));
    slot.box=m_clippingBoxes[primitiveId];
}

OpenGLCore::MirrorSnapshot OpenGLCore::snapshotMirror() const
{
    MirrorSnapshot snapshot;

    const size_t primitiveLimit=std::min(primitiveIdCount(),
        std::min(m_vertexes.size()/4, m_indexes.size()/6));

    for(size_t id=0; id<primitiveLimit; ++id)
    {
        if(!isPrimitiveIdLive(id))
            continue;

        const uint8_t *vertexes=reinterpret_cast<const uint8_t *>(&m_vertexes[id*4]);
        const uint8_t *indexes=reinterpret_cast<const uint8_t *>(&m_indexes[id*6]);
        std::vector<uint8_t> &bytes=snapshot.primitives[id];

        bytes.assign(vertexes, vertexes+4*sizeof(Vertex));
        bytes.insert(bytes.end(), indexes, indexes+6*sizeof(uint16_t));
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

    if(memcmp(before.indexes, &m_indexes[primitiveId*6], sizeof(before.indexes)) != 0)
    {
        m_dirtyIndexSlots.mark(primitiveId);
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

        //full-width rows are contiguous in atlas->data, and the default
        //GL_UNPACK_ALIGNMENT of 4 holds as long as a row is a multiple of 4 bytes
        assert((atlas->width*atlas->depth)%4 == 0);
        assert(atlas->depth == 3);

        size_t rows=atlas->dirty_y1-atlas->dirty_y0;

        if(!bound)
        {
            glActiveTexture(GL_TEXTURE0);
            bound=true;
        }
        glBindTexture(GL_TEXTURE_2D, atlas->id);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, (GLint)atlas->dirty_y0, (GLsizei)atlas->width, (GLsizei)rows,
            GL_RGB, GL_UNSIGNED_BYTE, atlas->data+atlas->dirty_y0*atlas->width*atlas->depth);

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
    m_frameStats.atlasUploadCalls=0;
    m_frameStats.atlasUploadBytes=0;
    m_frameStats.textSlotsWritten=m_dirtyTextVertexSlots.count();
    m_frameStats.mirrorBytes=
        m_vertexes.size()*sizeof(Vertex)+m_indexes.size()*sizeof(uint16_t)
        +m_clippingBoxes.size()*sizeof(Box)
        +m_textVertexes.size()*sizeof(glyph_vertex_t)+m_textIndexes.size()*sizeof(uint32_t);

    //glyphs measureText (logic phase) or renderText rasterised since the last frame; done
    //even when nothing draws text this frame, so they are on the GPU before first use
    uploadDirtyAtlases();

    if(!m_vertexes.empty())
    {
        //copy only the primitive slots that changed since the last frame
        uploadDirty(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer, m_indexBufferCapacity,
            m_indexes.data(), sizeof(uint16_t), m_indexes.size(), m_dirtyIndexSlots, 6);

        uploadDirty(GL_ARRAY_BUFFER, m_vertexBuffer, m_vertexBufferCapacity,
            m_vertexes.data(), sizeof(Vertex), m_vertexes.size(), m_dirtyVertexSlots, 4);

        uploadDirty(GL_ARRAY_BUFFER, m_clipBuffer, m_clipBufferCapacity,
            m_clippingBoxes.data(), sizeof(Box), m_clippingBoxes.size(), m_dirtyClipSlots, 1);

        //draw primitives
        glUseProgram(m_program);

        glProgramUniformMatrix4fv(
            m_program,
            m_projectionUniform,
            1,
            GL_FALSE,
            glm::value_ptr(m_projectionMatrix)
        );

        glProgramUniform2ui(m_program, m_textureSizeUniform, m_currentTextureSize.x, m_currentTextureSize.y);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_currentTexture);
        glBindVertexArray(m_vertexArray);

        glDrawElements(GL_TRIANGLES, m_indexes.size(), (GLenum)GL_UNSIGNED_SHORT, 0);
        ++m_frameStats.drawCalls;

        glBindVertexArray(0);
    }

    if(!m_textIndexes.empty())
    {
        //copy only the glyph slots that changed since the last frame
        uploadDirty(GL_ELEMENT_ARRAY_BUFFER, m_textIndexBuffer, m_textIndexBufferCapacity,
            m_textIndexes.data(), sizeof(uint32_t), m_textIndexes.size(), m_dirtyTextIndexSlots, 6);

        uploadDirty(GL_ARRAY_BUFFER, m_textVertexBuffer, m_textVertexBufferCapacity,
            m_textVertexes.data(), sizeof(glyph_vertex_t), m_textVertexes.size(), m_dirtyTextVertexSlots, 4);

        //draw text
        glUseProgram(m_textProgram);

        glProgramUniformMatrix4fv(
            m_textProgram,
            m_textProjectionUniform,
            1,
            GL_FALSE,
            glm::value_ptr(m_projectionMatrix)
        );

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_textTextureId);
        glBindVertexArray(m_textVertexArray);

        glDrawElements(GL_TRIANGLES, m_textIndexes.size(), (GLenum)GL_UNSIGNED_INT, 0);
        ++m_frameStats.drawCalls;

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

    vertex.x=x;
//    vertex.y=m_viewRect.h - y;
    vertex.y=y;
    vertex.z=z;// m_activeProgram;

//    if(m_activeProgram == 1)
    {
        vertex.tx=u;
        vertex.ty=v;
    }

    // SDF fields are only used by styles 4, 6 and 7 (writeRoundedQuad); keep them 0 otherwise.
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

    size_t index=primitiveId*6;
    size_t vertexIndex=primitiveId*4;

    ensurePrimitiveSlots(primitiveId);

    PrimitiveSlot before;

    readPrimitiveSlot(primitiveId, before);

    Box &box=m_clippingBoxes[primitiveId];

//    box.state=1;
    if(m_clipping)
    {
        gweni::Rect rect=clipRegion();

        box.x0=rect.x;
        box.y0=rect.y;
        box.x1=rect.x+rect.w;
        box.y1=rect.y+rect.h;
    }
    else
    {
        box.x0=m_viewRect.x;
        box.y0=m_viewRect.y;
        box.x1=m_viewRect.x+m_viewRect.w;
        box.y1=m_viewRect.y+m_viewRect.h;
    }

    m_indexes[index++]=vertexIndex;
    m_indexes[index++]=vertexIndex+1;
    m_indexes[index++]=vertexIndex+2;
    m_indexes[index++]=vertexIndex+1;
    m_indexes[index++]=vertexIndex+3;
    m_indexes[index++]=vertexIndex+2;

#ifdef DEBUG_RENDER
    std::cout<<"Rendering Fill: "<<primitiveId<<"("<<rect.x<<", "<<rect.y<<", "<<rect.w<<", "<<rect.h<<" : "<<zIndex<<") color: "<<(int)m_color.r<<", "<<(int)m_color.g<<", "<<(int)m_color.b<<", "<<(int)m_color.a<<"\n";
#endif//DEBUG_RENDER

    setVertex(m_vertexes[vertexIndex++], 1, rect.x, rect.y, zIndex, m_color.rg(), m_color.ba());
    setVertex(m_vertexes[vertexIndex++], 1, rect.x+rect.w, rect.y, zIndex, m_color.rg(), m_color.ba());
    setVertex(m_vertexes[vertexIndex++], 1, rect.x, rect.y+rect.h, zIndex, m_color.rg(), m_color.ba());
//    setVertex(m_vertexes[vertexIndex++], rect.x+rect.w, rect.y);
    setVertex(m_vertexes[vertexIndex++], 1, rect.x+rect.w, rect.y+rect.h, zIndex, m_color.rg(), m_color.ba());
//    setVertex(m_vertexes[vertexIndex++], rect.x, rect.y+rect.h);

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

void OpenGLCore::writeRoundedQuad(size_t primitiveId, gweni::Rect rect, int zIndex, uint8_t style,
    float radius, float border, gweni::Color color0Left, gweni::Color color0Right,
    gweni::Color color1, gweni::Color color3Left, gweni::Color color3Right,
    gweni::Color borderColor)
{
    m_activeProgram=0;
    m_zIndex++;

    translate(rect);

    size_t index=primitiveId*6;
    size_t vertexIndex=primitiveId*4;

    ensurePrimitiveSlots(primitiveId);

    PrimitiveSlot before;

    readPrimitiveSlot(primitiveId, before);

    Box &box=m_clippingBoxes[primitiveId];

    if(m_clipping)
    {
        gweni::Rect clip=clipRegion();

        box.x0=clip.x;
        box.y0=clip.y;
        box.x1=clip.x+clip.w;
        box.y1=clip.y+clip.h;
    }
    else
    {
        box.x0=m_viewRect.x;
        box.y0=m_viewRect.y;
        box.x1=m_viewRect.x+m_viewRect.w;
        box.y1=m_viewRect.y+m_viewRect.h;
    }

    m_indexes[index++]=vertexIndex;
    m_indexes[index++]=vertexIndex+1;
    m_indexes[index++]=vertexIndex+2;
    m_indexes[index++]=vertexIndex+1;
    m_indexes[index++]=vertexIndex+3;
    m_indexes[index++]=vertexIndex+2;

    // Center the SDF rect inside the control rect so the drawn shape and the quad
    // positions coincide exactly (avoids edge clipping for odd sizes).
    const int hw=rect.w/2;
    const int hh=rect.h/2;
    const int cx=rect.x+hw;
    const int cy=rect.y+hh;

    const uint16_t rad=(uint16_t)(radius<0.0f?0.0f:radius);
    const uint16_t bor=(uint16_t)(border<0.0f?0.0f:border);

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

    setRounded(vertexIndex++, cx-hw, cy-hh, -hw, -hh, color0Left, color3Left);
    setRounded(vertexIndex++, cx+hw, cy-hh, +hw, -hh, color0Right, color3Right);
    setRounded(vertexIndex++, cx-hw, cy+hh, -hw, +hh, color0Left, color3Left);
    setRounded(vertexIndex++, cx+hw, cy+hh, +hw, +hh, color0Right, color3Right);

    markPrimitiveChanges(primitiveId, before);
}

void OpenGLCore::drawIcon(size_t primitiveId, gweni::Rect rect, int kind, int direction,
    gweni::Color color, int zIndex)
{
    m_activeProgram=0;
    m_zIndex++;

    translate(rect);

    size_t index=primitiveId*6;
    size_t vertexIndex=primitiveId*4;

    ensurePrimitiveSlots(primitiveId);

    PrimitiveSlot before;

    readPrimitiveSlot(primitiveId, before);

    Box &box=m_clippingBoxes[primitiveId];

    if(m_clipping)
    {
        gweni::Rect clip=clipRegion();

        box.x0=clip.x;
        box.y0=clip.y;
        box.x1=clip.x+clip.w;
        box.y1=clip.y+clip.h;
    }
    else
    {
        box.x0=m_viewRect.x;
        box.y0=m_viewRect.y;
        box.x1=m_viewRect.x+m_viewRect.w;
        box.y1=m_viewRect.y+m_viewRect.h;
    }

    m_indexes[index++]=vertexIndex;
    m_indexes[index++]=vertexIndex+1;
    m_indexes[index++]=vertexIndex+2;
    m_indexes[index++]=vertexIndex+1;
    m_indexes[index++]=vertexIndex+3;
    m_indexes[index++]=vertexIndex+2;

    const int hw=rect.w/2;
    const int hh=rect.h/2;
    const int cx=rect.x+hw;
    const int cy=rect.y+hh;

    auto setIcon=[&](size_t vi, int px, int py, int lx, int ly)
    {
        Vertex &v=m_vertexes[vi];
        setVertex(v, 5, px, py, zIndex, color.rg(), color.ba());
        v.lx=(int16_t)lx;
        v.ly=(int16_t)ly;
        v.radius=(uint16_t)kind;
        v.border=(uint16_t)direction;
    };

    setIcon(vertexIndex++, cx-hw, cy-hh, -hw, -hh);
    setIcon(vertexIndex++, cx+hw, cy-hh, +hw, -hh);
    setIcon(vertexIndex++, cx-hw, cy+hh, -hw, +hh);
    setIcon(vertexIndex++, cx+hw, cy+hh, +hw, +hh);

    markPrimitiveChanges(primitiveId, before);
}

void OpenGLCore::setDrawColor(gweni::Color color)
{
    m_color=color;
}

void OpenGLCore::startClip()
{
    m_clipping=true;
//    flush();
//    gweni::Rect rect=clipRegion();
//
//    rect.y=m_viewRect.h - (rect.y + rect.h);
//
//    glScissor(rect.x * scale(), rect.y * scale(),
//        rect.w * scale(), rect.h * scale());
//    glEnable(GL_SCISSOR_TEST);
}

void OpenGLCore::endClip()
{
    m_clipping=false;
//    flush();
//    glDisable(GL_SCISSOR_TEST);
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

//    m_zIndex++;

    GLTextureData &texData=*m_textureData[m_lastTexture];

    translate(rect);

    if(!m_currentTexture || texData.m_textureId != m_currentTexture)
    {
//        flush();
        setTexture(texData.m_textureId);
    }

    m_activeProgram=1;

    size_t index=primitiveId*6;
    size_t vertexIndex=primitiveId*4;

    ensurePrimitiveSlots(primitiveId);

    PrimitiveSlot before;

    readPrimitiveSlot(primitiveId, before);

    Box &box=m_clippingBoxes[primitiveId];

//    box.state=2;
    if(m_clipping)
    {
        gweni::Rect rect=clipRegion();

        box.x0=rect.x;
        box.y0=rect.y;
        box.x1=rect.x+rect.w;
        box.y1=rect.y+rect.h;
    }
    else
    {
        box.x0=m_viewRect.x;
        box.y0=m_viewRect.y;
        box.x1=m_viewRect.x+m_viewRect.w;
        box.y1=m_viewRect.y+m_viewRect.h;
    }

    m_indexes[index++]=vertexIndex;
    m_indexes[index++]=vertexIndex+1;
    m_indexes[index++]=vertexIndex+2;
    m_indexes[index++]=vertexIndex+1;
    m_indexes[index++]=vertexIndex+3;
    m_indexes[index++]=vertexIndex+2;

#ifdef DEBUG_RENDER
    std::cout<<"Rendering: "<<primitiveId<<"("<<rect.x<<", "<<rect.y<<", "<<rect.w<<", "<<rect.h<<" : "<<zIndex<<")\n";
#endif//DEBUG_RENDER

    setVertex(m_vertexes[vertexIndex++], 2, rect.x, rect.y, zIndex, u1, v1);
    setVertex(m_vertexes[vertexIndex++], 2, rect.x + rect.w, rect.y, zIndex, u2, v1);
    setVertex(m_vertexes[vertexIndex++], 2, rect.x, rect.y + rect.h, zIndex, u1, v2);
//    setVertex(m_vertexes[vertexIndex++], rect.x + rect.w, rect.y, u2, v1);
    setVertex(m_vertexes[vertexIndex++], 2, rect.x + rect.w, rect.y + rect.h, zIndex, u2, v2);
//    setVertex(m_vertexes[vertexIndex++], rect.x, rect.y + rect.h, u1, v2);

    markPrimitiveChanges(primitiveId, before);

//    addVert(rect.x, rect.y, u1, v1);
//    addVert(rect.x + rect.w, rect.y, u2, v1);
//    addVert(rect.x, rect.y + rect.h, u1, v2);
//    addVert(rect.x + rect.w, rect.y, u2, v1);
//    addVert(rect.x + rect.w, rect.y + rect.h, u2, v2);
//    addVert(rect.x, rect.y + rect.h, u1, v2);
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
    if(m_textIndexes.size() < glyphSlots*6)
        m_textIndexes.resize(glyphSlots*6);
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

    // Blank the whole run before handing it back: a freed run must draw nothing
    // until somebody allocates its slots again.
    run.glyphs=0;
    writeTextIndexes(run);
    addFreeTextRun(run.start, run.capacity);

    run.start=0;
    run.capacity=0;
}

void OpenGLCore::allocateTextRun(TextRun &run, size_t glyphs)
{
    if(run.capacity >= glyphs)
    {
        // Keeps the run where it is; the tail past `glyphs` is blanked by
        // writeTextIndexes, so a string that shrinks leaves no ghost glyphs.
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

void OpenGLCore::writeTextIndexes(const TextRun &run)
{
    if(run.capacity == 0)
        return;

    static const uint32_t c_quad[6]={ 0, 1, 2, 0, 2, 3 };

    for(size_t i=0; i<run.capacity; ++i)
    {
        const size_t slot=run.start+i;
        uint32_t *indexes=&m_textIndexes[slot*6];

        if(run.hidden || (i >= run.glyphs))
        {
            // Degenerate triangles: every index the same vertex, so the slot
            // rasterises nothing whatever its vertices still hold. This is what
            // hides a run and what keeps a freed or over-long run off the screen.
            for(size_t j=0; j<6; ++j)
                indexes[j]=0;
        }
        else
        {
            const uint32_t base=static_cast<uint32_t>(slot*4);

            for(size_t j=0; j<6; ++j)
                indexes[j]=base+c_quad[j];
        }

        m_dirtyTextIndexSlots.mark(slot);
    }
}

void OpenGLCore::markTextVertexes(const TextRun &run)
{
    for(size_t i=0; i<run.glyphs; ++i)
        m_dirtyTextVertexSlots.mark(run.start+i);
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
        run=TextRun();
    }

    releaseTextId(primitiveId);
}

void OpenGLCore::renderText(size_t textId, const gweni::Font &font, gweni::Point pos, int zIndex,
    const gweni::String &text)
{
    if(!ensureFont(font))
        return;

    if(m_textRuns.size() < textId+1)
        m_textRuns.resize(textId+1);

    TextRun &run=m_textRuns[textId];

    translate(pos.x, pos.y);

    if(run.text == text)
    {
        // String hasn't changed: at most offset the run that is already there.
        // A zIndex that changed on its own is not acted on, as it was not before
        // this rewrite either; the run keeps the depth it was last laid out at.
        if((pos.x != run.position.x) || (pos.y != run.position.y))
        {
            const gweni::Point delta=pos-run.position;
            const float zDelta=run.glyphs?1.0f/run.glyphs:0.0f;
            float zOffset=0.0f;

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
        }
        return;
    }

    GLFontData &fontData=m_lastFont->second;

    m_textTextureId=fontData.atlas->id;

    ftgl::text_buffer_t *textBuffer=ftgl::text_buffer_new();
    ftgl::vec2 pen={ 0.0f, 0.0f };

    // Bake the current draw color (set by the control's text renderer from its
    // text color) into the glyphs, so per-control text colors actually apply
    // instead of the static black the markup was initialised with.
    fontData.markup.foreground_color.r=m_color.r/255.0f;
    fontData.markup.foreground_color.g=m_color.g/255.0f;
    fontData.markup.foreground_color.b=m_color.b/255.0f;
    fontData.markup.foreground_color.a=1.0f;

    //the length is in codepoints, not bytes: text.length() overran the string for
    //anything non-ASCII and drew whatever followed it in memory as glyphs
    ftgl::text_buffer_add_text(textBuffer, &pen, &fontData.markup, text.c_str(), ftgl::utf8_strlen(text.c_str()));

    ftgl::vector_t *indexes=textBuffer->buffer->indices;
    ftgl::vector_t *vertexes=textBuffer->buffer->vertices;

    //with every markup decoration off, freetype-gl emits exactly one quad per glyph
    const size_t glyphCount=indexes->size/6;

    run.text=text;
    run.position=pos;
    allocateTextRun(run, glyphCount);

    ftgl::glyph_vertex_t *vertexBuffer=(ftgl::glyph_vertex_t *)vertexes->items;

    const float zDelta=glyphCount?1.0f/glyphCount:0.0f;
    float zOffset=0.0f;

#ifdef DEBUG_RENDER
    std::cout<<"Regenerating text run: ("<<pos.x<<", "<<pos.y<<" : "<<zIndex<<") "<<text<<"\n";
#endif//DEBUG_RENDER

    for(size_t i=0; i<glyphCount; ++i)
    {
        const size_t vertIndex=(run.start+i)*4;
        // Each glyph of a run sits a fraction of a z unit in front of the one
        // before it, so the GL_LESS depth test cannot reject the overlap between
        // two neighbouring glyph boxes. Accumulated rather than i*zDelta, which
        // rounds differently and would move a glyph by an ulp of depth.
        const float z=zIndex+zOffset;

        zOffset+=zDelta;

        for(size_t j=0; j<4; ++j)
        {
            const ftgl::glyph_vertex_t &source=vertexBuffer[i*4+j];
            ftgl::glyph_vertex_t &vertex=m_textVertexes[vertIndex+j];

            vertex=source;
            vertex.x=pos.x+source.x;
            vertex.y=pos.y-source.y;
            vertex.z=z;
        }
    }

    writeTextIndexes(run);
    markTextVertexes(run);

    if(textDebugMatches(text))
    {
        size_t freeSlots=0, liveGlyphs=0;

        for(const auto &block:m_freeTextRuns)
            freeSlots+=block.second;
        for(const TextRun &textRun:m_textRuns)
            liveGlyphs+=textRun.glyphs;

        ::printf("[mirror] textId=%zu \"%s\" chars=%zu run=[%zu,%zu) capacity=%zu "
            "ftgl vertices=%zu indices=%zu\n",
            textId, text.c_str(), text.length(), run.start, run.start+run.glyphs,
            run.capacity, vertexes->size, indexes->size);
        ::printf("[mirror]   pos=(%d,%d) z=%d hidden=%s atlas=%zux%zu depth=%zu "
            "mirror verts=%zu idx=%zu slots used=%zu free runs=%zu free slots=%zu "
            "live glyphs=%zu\n",
            pos.x, pos.y, zIndex, run.hidden?"yes":"no",
            fontData.atlas->width, fontData.atlas->height, fontData.atlas->depth,
            m_textVertexes.size(), m_textIndexes.size(), m_textSlotsUsed,
            m_freeTextRuns.size(), freeSlots, liveGlyphs);
        for(size_t i=0; i<run.capacity; ++i)
        {
            const size_t slot=run.start+i;
            const size_t vertIndex=slot*4;
            const size_t index=slot*6;
            float minX=m_textVertexes[vertIndex].x, maxX=minX;
            float minY=m_textVertexes[vertIndex].y, maxY=minY;

            ::printf("[mirror]   slot %zu (glyph %zu, char '%c')%s verts %zu..%zu idx %zu..%zu\n",
                slot, i, i<text.length()?text[i]:'?', i<run.glyphs?"":" [unused tail]",
                vertIndex, vertIndex+3, index, index+5);
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
            //a drawn slot must index only its own run; a blanked one, only one vertex
            const bool drawn=!run.hidden && (i < run.glyphs);
            bool ok=true;
            ::printf("[mirror]     idx =");
            for(size_t j=0; j<6; ++j)
            {
                uint32_t e=m_textIndexes[index+j];
                if(drawn?((e < run.start*4) || (e >= (run.start+run.capacity)*4))
                    :(e != m_textIndexes[index]))
                    ok=false;
                ::printf(" %u", (unsigned)e);
            }
            ::printf("  (%s: %s)\n", drawn?"all inside run":"degenerate", ok?"yes":"NO");
            ::printf("[mirror]     box w=%.3f h=%.3f  x=[%.3f..%.3f] y=[%.3f..%.3f]\n",
                (double)(maxX-minX), (double)(maxY-minY),
                (double)minX, (double)maxX, (double)minY, (double)maxY);
        }
    }

    text_buffer_delete(textBuffer);
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
    writeTextIndexes(run);
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
    writeTextIndexes(run);
}

gweni::Point OpenGLCore::measureText(const gweni::Font &font, const gweni::String &text)
{
    if(!ensureFont(font))
        return gweni::Point(0, 0);

    GLFontData &fontData=m_lastFont->second;

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
