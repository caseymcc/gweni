/*
 *  Gweni - OpenGL Core renderer.
 *  Copyright (c) 2013-18 Nick Trout
 *  See license in Gweni.h
 */

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
#include <algorithm>
#include <fstream>
#include <cassert>

 ////#define STBI_ASSERT(x)  // comment in for no asserts
 //#define STB_IMAGE_IMPLEMENTATION
 //#include <gweni/External/stb_image.h>
 //
 //#define STB_TRUETYPE_IMPLEMENTATION
 //#define STBTT_STATIC
 //#include <gweni/External/stb_truetype.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <freetype-gl/freetype-gl.h>
#include <freetype-gl/text-buffer.h>

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

// See "Font Size in Pixels or Points" in "stb_truetype.h"
static constexpr float c_pointsToPixels=1.333f;
// Arbitrary size chosen for texture cache target.
static constexpr int c_texsz=800; // Texture size too small for wchar_t characters but stbtt_BakeFontBitmap crashes on larger sizes


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

    //    FT_Library fontLibrary;
    //
    //    freeFont(font);
    m_lastFont=nullptr;

    const String filename=getResourcePaths().getPath(ResourcePaths::Type::Font, font.facename);

    vec4 black={{0.0, 0.0, 0.0, 1.0}};
    vec4 white={{1.0, 1.0, 1.0, 1.0}};
    vec4 none={{1.0, 1.0, 1.0, 0.0}};

    fontData.faceName=font.facename;
    fontData.markup.family=(char *)filename.c_str();
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

    size_t cacheLength=strlen(cache);
    fontData.fontManager->cache=(char *)malloc(sizeof(char)*(cacheLength+1));
    strncpy(fontData.fontManager->cache, cache, cacheLength+1);

    fontData.markup.font=ftgl::font_manager_get_from_markup(fontData.fontManager, &fontData.markup);
    fontData.textureFont=fontData.markup.font;

    fontData.atlas=fontData.fontManager->atlas;

//    texture_font_load_glyphs(fontData.textureFont, cache);

    glGenTextures(1, &fontData.atlas->id);
    glBindTexture(GL_TEXTURE_2D, fontData.atlas->id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, fontData.atlas->width,
        fontData.atlas->height, 0, GL_RGB, GL_UNSIGNED_BYTE,
        fontData.atlas->data);

//    fontData.atlas=ftgl::texture_atlas_new(1024, 1024, 1);
//    fontData.textureFont=ftgl::texture_font_new_from_file(fontData.atlas, font.size, filename.c_str());
//    fontData.markup.font=fontData.textureFont;
//
//    texture_font_load_glyphs(fontData.textureFont, cache);

    //    std::ifstream inFile(filename, std::ifstream::in | std::ifstream::binary);
    //
    //    if (!inFile.good())
    //    {
    //        gweni::log::write(log::Level::Error, "Font file not found: %s", filename.c_str());
    //        return Font::Status::ErrorFileNotFound;
    //    }
    //
    //    std::streampos begin = inFile.tellg();
    //    inFile.seekg(0, std::ios::end);
    //    const size_t fsz = inFile.tellg() - begin;
    //    inFile.seekg(0, std::ios::beg);
    //    assert(fsz > 0);
    //
    //    std::unique_ptr<unsigned char[]> ttfdata = std::unique_ptr<unsigned char[]>(new unsigned char[fsz]);
    //    inFile.read(reinterpret_cast<char*>(ttfdata.get()), fsz);
    //    inFile.close();
    //
    //    std::unique_ptr<unsigned char[]> font_bmp = std::unique_ptr<unsigned char[]>(new unsigned char[c_texsz * c_texsz]);
    //
    //    GLFontData fontData;
    //    const float realsize = font.size * scale();
    //    fontData.baked_chars.resize(LastCharacter - BeginCharacter + 1);
    //
    //    stbtt_BakeFontBitmap(ttfdata.get(), 0,
    //                    realsize * c_pointsToPixels, // height
    //                    font_bmp.get(),
    //                    c_texsz, c_texsz,
    //                    BeginCharacter, LastCharacter,             // range to bake
    //                    reinterpret_cast<stbtt_bakedchar*>(fontData.baked_chars.data()));
    //
    //    glGenTextures(1, &fontData.m_textureId);
    //    setTexture(fontData.m_textureId);
    //    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    //    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
    //        c_texsz, c_texsz, 0,
    //        GL_RED, GL_UNSIGNED_BYTE,
    //        font_bmp.get());
    //
    //    fontData.width = c_texsz;
    //    fontData.height = c_texsz;

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

Texture::Status OpenGLCore::loadTexture(const Texture &texture)
{
    freeTexture(texture);
    m_lastTexture=nullptr;

    const String filename=getResourcePaths().getPath(ResourcePaths::Type::Texture, texture.name);

    GLTextureData texData;

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
    setTexture(texData.m_textureId);
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

    m_lastTexture=&(*m_textures.insert(std::make_pair(texture, std::move(texData))).first);
    return Texture::Status::Loaded;
}

void OpenGLCore::freeTexture(const gweni::Texture &texture)
{
    if(m_lastTexture != nullptr && m_lastTexture->first == texture)
        m_lastTexture=nullptr;

    m_textures.erase(texture); // calls GLTextureData destructor
}

TextureData OpenGLCore::getTextureData(const Texture &texture) const
{
    if(m_lastTexture != nullptr && m_lastTexture->first == texture)
        return m_lastTexture->second;

    auto it=m_textures.find(texture);
    if(it != m_textures.cend())
    {
        return it->second;
    }
    // Texture not loaded :(
    return TextureData();
}

bool OpenGLCore::ensureTexture(const gweni::Texture &texture)
{
    if(m_lastTexture != nullptr)
    {
        if(m_lastTexture->first == texture)
            return true;
    }

    // Was it loaded before?
    auto it=m_textures.find(texture);
    if(it != m_textures.end())
    {
        m_lastTexture=&(*it);
        return true;
    }

    // No, try load to it

    // LoadTexture sets m_lastTexture, if exist
    return loadTexture(texture) == Texture::Status::Loaded;
}

//-------------------------------------------------------------------------------

OpenGLCore::OpenGLCore(ResourcePaths &paths, const Rect &viewRect)
    : Base(paths)
    , m_context(nullptr)
    , m_viewRect(viewRect)
    , m_currentTexture(0)
    , m_indexes(minIndexSize)
    , m_vertexes(minVertexSize)
    , m_clipping(false)
    , m_clippingBoxes(minClippingSize)
    , m_lastFont(nullptr)
    , m_lastTexture(nullptr)
{}

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
            std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }
    else
    {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if(!success)
        {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
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

    // Creating buffers
    glCreateVertexArrays(1, &m_vertexArray);
    glCreateBuffers(1, &m_vertexBuffer);
    glCreateBuffers(1, &m_indexBuffer);
    glCreateBuffers(1, &m_clipBuffer);

    //setup vertex array
    glBindVertexArray(m_vertexArray);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);

    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
    glEnableVertexAttribArray(0); // Attrib '0' is render type
    glVertexAttribIPointer(0, 1, GL_UNSIGNED_BYTE, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, type)));
    glEnableVertexAttribArray(1); // Attrib '1' is vertex pos
    glVertexAttribIPointer(1, 3, GL_UNSIGNED_SHORT, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, x)));
    glEnableVertexAttribArray(2); // Attrib '2' is texture coord or packeted color
    glVertexAttribIPointer(2, 2, GL_UNSIGNED_SHORT, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, tx)));

    glBindBuffer(GL_ARRAY_BUFFER, m_clipBuffer);
    glEnableVertexAttribArray(3); // Attrib '3' is show
    glVertexAttribIPointer(3, 1, GL_UNSIGNED_BYTE, sizeof(Box), (GLvoid*)(offsetof(Box, state)));
    glVertexAttribDivisor(3, 6);
    glEnableVertexAttribArray(4); // Attrib '4' is clipping
    glVertexAttribIPointer(4, 4, GL_UNSIGNED_SHORT, sizeof(Box), (GLvoid*)(offsetof(Box, x0)));
    glVertexAttribDivisor(4, 6);

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
    glEnableVertexAttribArray(3); // Attrib '3' is shift
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(glyph_vertex_t), (GLvoid*)(offsetof(glyph_vertex_t, shift)));
    glEnableVertexAttribArray(4); // Attrib '4' is gamma
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(glyph_vertex_t), (GLvoid*)(offsetof(glyph_vertex_t, gamma)));

    glBindVertexArray(0);

    // Loading shaders
    auto vertexSource=R"(
#version 330 core

//layout (location = 0) in vec3 inPosition;
//layout (location = 1) in vec2 inTexCoords;
//layout (location = 2) in vec4 inColor;

layout (location = 0) in uint type;
layout (location = 1) in uvec3 inPosition;
layout (location = 2) in uvec2 inTexCoords;
layout (location = 3) in uint inState;
layout (location = 4) in ivec4 inClippingBox;

out vec2 TexCoords;
out vec4 VertexColor;
flat out uint ShaderType;
out vec4 clippingBox;

uniform mat4 projection;
uniform uvec2 textureSize;

void main()
{
//    ShaderType = int(inPosition.z);
    ShaderType = type;
    clippingBox = vec4(inClippingBox);

    TexCoords = vec2(inTexCoords)/vec2(textureSize);
    VertexColor.x = float((inTexCoords.x >> 8) & 0xffu);
    VertexColor.y = float(inTexCoords.x & 0xffu);
    VertexColor.z = float((inTexCoords.y >> 8) & 0xffu);
    VertexColor.w = float(inTexCoords.y & 0xffu);

    if(inState == 0u)//item has been hidden, move all coords to 0.0f
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
in vec4 clippingBox;

uniform sampler2D texture1;
void main()
{
    //check if frag should be clipped
    if(gl_FragCoord.x < clippingBox.x)
        discard;
    else if(gl_FragCoord.y < clippingBox.y)
        discard;
    else if(gl_FragCoord.x > clippingBox.z)
        discard;
    else if(gl_FragCoord.y > clippingBox.w)
        discard;

    if (ShaderType == 0u)
        FragColor = VertexColor;
    else if (ShaderType == 1u)
        FragColor = texture(texture1, TexCoords);// * VertexColor;
    else if (ShaderType == 2u)
    {
        float color = texture(texture1, TexCoords).r;
        FragColor = vec4(VertexColor.rgb, color);
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
layout (location = 3) in float vertShift;
layout (location = 4) in float vertGamma;

uniform mat4 projection;

out vec2 texCoord;
out vec4 color;
out float shift;
out float gamma;

void main()
{
    texCoord=vertTexCoord;
    color=vertColor;
    shift=vertShift;
    gamma=vertGamma;
    gl_Position = projection * vec4(float(vertPosition.x), float(vertPosition.y), float(vertPosition.z), 1.0f);
//    gl_Position = projection * vec4(float(vertPosition.x), float(vertPosition.y), 1000.0f, 1.0f);
}
    )";

    auto textFragSource=R"(
#version 330 core

in vec2 texCoord;
in vec4 color;
in float shift;
in float gamma;

uniform sampler2D texture1;
void main()
{
    float a = texture2D(texture1, texCoord).r;

    gl_FragColor = color * pow( a, 1.0/gamma );
}
    )";

    m_textProgram=loadShaders(textVertexSource, textFragSource);
    m_textProjectionUniform=glGetUniformLocation(m_textProgram, "projection");

    glViewport(m_viewRect.x, m_viewRect.y, m_viewRect.w, m_viewRect.h);
}

void OpenGLCore::begin()
{
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

void OpenGLCore::flush()
{
    if(!m_vertexes.empty())
    {
        //copy data to gpu
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indexes.size()*sizeof(uint16_t), m_indexes.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, m_vertexes.size()*sizeof(Vertex), m_vertexes.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, m_clipBuffer);
        glBufferData(GL_ARRAY_BUFFER, m_clippingBoxes.size()*sizeof(Box), m_clippingBoxes.data(), GL_STATIC_DRAW);

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
        glBindVertexArray(0);
    }

    if(!m_textIndexes.empty())
    {
        //copy text data to gpu
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_textIndexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_textIndexes.size()*sizeof(uint16_t), m_textIndexes.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, m_textVertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, m_textVertexes.size()*sizeof(glyph_vertex_t), m_textVertexes.data(), GL_STATIC_DRAW);

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

        glDrawElements(GL_TRIANGLES, m_textIndexes.size(), (GLenum)GL_UNSIGNED_SHORT, 0);
        glBindVertexArray(0);
    }

    glBindVertexArray(0);

    glFlush();

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

void OpenGLCore::setVertex(Vertex &vertex, int x, int y, float u, float v)
{
    vertex.type=m_activeProgram;

    vertex.x=x;
//    vertex.y=m_viewRect.h - y;
    vertex.y=y;
    vertex.z=m_zIndex;// m_activeProgram;

    if(m_activeProgram == 1)
    {
        vertex.tx=u;
        vertex.ty=v;
    }
    else
    {
        vertex.tx=(m_color.r<<8)&m_color.g;
        vertex.ty=(m_color.b<<8)&m_color.a;
//        vertex.color=glm::vec4(
//            m_color.r / 255.0f,
//            m_color.g / 255.0f,
//            m_color.b / 255.0f,
//            m_color.a / 255.0f
//        );
    }
}

void OpenGLCore::addVert(int x, int y, float u, float v)
{
    // OpenGL origin is bottom-left. Gweni origin is top-left.

    Vertex vertex;

    setVertex(vertex, x, y, u, v);
    m_vertexes.emplace_back(vertex);
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

void OpenGLCore::drawFilledRect(size_t primitiveId, gweni::Rect rect)
{
    m_activeProgram=0;
    m_zIndex++;

    translate(rect);

    size_t index=primitiveId*6;
    size_t vertexIndex=primitiveId*4;

    if(m_clippingBoxes.size() < primitiveId+1)
        m_clippingBoxes.resize(primitiveId+1);

    Box &box=m_clippingBoxes[primitiveId];

    box.state=1;
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

    if(m_indexes.size() < index + 6)
        m_indexes.resize(index+6);

    m_indexes[index++]=vertexIndex;
    m_indexes[index++]=vertexIndex+1;
    m_indexes[index++]=vertexIndex+2;
    m_indexes[index++]=vertexIndex+1;
    m_indexes[index++]=vertexIndex+3;
    m_indexes[index++]=vertexIndex+2;

    if(m_vertexes.size() < vertexIndex+4)
        m_vertexes.resize(vertexIndex+4);

    setVertex(m_vertexes[vertexIndex++], rect.x, rect.y);
    setVertex(m_vertexes[vertexIndex++], rect.x+rect.w, rect.y);
    setVertex(m_vertexes[vertexIndex++], rect.x, rect.y+rect.h);
//    setVertex(m_vertexes[vertexIndex++], rect.x+rect.w, rect.y);
    setVertex(m_vertexes[vertexIndex++], rect.x+rect.w, rect.y+rect.h);
//    setVertex(m_vertexes[vertexIndex++], rect.x, rect.y+rect.h);

//    addVert(rect.x, rect.y);
//    addVert(rect.x+rect.w, rect.y);
//    addVert(rect.x, rect.y+rect.h);
//    addVert(rect.x+rect.w, rect.y);
//    addVert(rect.x+rect.w, rect.y+rect.h);
//    addVert(rect.x, rect.y+rect.h);
}

void OpenGLCore::setDrawColor(gweni::Color color)
{
    //            glColor4ubv((GLubyte*)&color);
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

void OpenGLCore::generatePrimitive(size_t primitiveId)
{

}

void OpenGLCore::removePrimitive(size_t primitiveId)
{

}

void OpenGLCore::drawTexturedRect(const gweni::Texture &texture, size_t primitiveId, gweni::Rect rect,
    float u1, float v1, float u2, float v2)
{
    if(!ensureTexture(texture))
        return drawMissingImage(primitiveId, rect);

    m_zIndex++;

    GLTextureData &texData=m_lastTexture->second;

    translate(rect);

    if(!m_currentTexture || texData.m_textureId != m_currentTexture)
    {
//        flush();
        setTexture(texData.m_textureId);
    }

    m_activeProgram=1;

    size_t index=primitiveId*6;
    size_t vertexIndex=primitiveId*4;

    if(m_clippingBoxes.size() < primitiveId+1)
        m_clippingBoxes.resize(primitiveId+1);

    Box &box=m_clippingBoxes[primitiveId];

    box.state=1;
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

    if(m_indexes.size() < index + 6)
        m_indexes.resize(index+6);

    m_indexes[index++]=vertexIndex;
    m_indexes[index++]=vertexIndex+1;
    m_indexes[index++]=vertexIndex+2;
    m_indexes[index++]=vertexIndex+1;
    m_indexes[index++]=vertexIndex+3;
    m_indexes[index++]=vertexIndex+2;

    if(m_vertexes.size() < vertexIndex+4)
        m_vertexes.resize(vertexIndex+4);

    setVertex(m_vertexes[vertexIndex++], rect.x, rect.y, u1, v1);
    setVertex(m_vertexes[vertexIndex++], rect.x + rect.w, rect.y, u2, v1);
    setVertex(m_vertexes[vertexIndex++], rect.x, rect.y + rect.h, u1, v2);
//    setVertex(m_vertexes[vertexIndex++], rect.x + rect.w, rect.y, u2, v1);
    setVertex(m_vertexes[vertexIndex++], rect.x + rect.w, rect.y + rect.h, u2, v2);
//    setVertex(m_vertexes[vertexIndex++], rect.x, rect.y + rect.h, u1, v2);

//    addVert(rect.x, rect.y, u1, v1);
//    addVert(rect.x + rect.w, rect.y, u2, v1);
//    addVert(rect.x, rect.y + rect.h, u1, v2);
//    addVert(rect.x + rect.w, rect.y, u2, v1);
//    addVert(rect.x + rect.w, rect.y + rect.h, u2, v2);
//    addVert(rect.x, rect.y + rect.h, u1, v2);
}

void OpenGLCore::showPrimitive(size_t primitiveId)
{
    if(m_clippingBoxes.size() < primitiveId+1)
        m_clippingBoxes.resize(primitiveId+1);

    Box &box=m_clippingBoxes[primitiveId];

    box.state=1;
}

void OpenGLCore::hidePrimitive(size_t primitiveId)
{
    if(m_clippingBoxes.size() < primitiveId+1)
        m_clippingBoxes.resize(primitiveId+1);

    Box &box=m_clippingBoxes[primitiveId];

    box.state=0;
}

gweni::Color OpenGLCore::pixelColor(const gweni::Texture &texture, unsigned int x, unsigned int y,
    const gweni::Color &col_default)
{
    if(!ensureTexture(texture))
        return col_default;

    GLTextureData &texData=m_lastTexture->second;

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

void OpenGLCore::renderText(size_t textId, const gweni::Font &font, gweni::Point pos,
    const gweni::String &text)
{
//    return;

    if(!ensureFont(font))
        return;

    if(m_textPrimitiveIds.size() < textId+1)
    {
        m_textPrimitiveIds.resize(textId+1);
        m_textStrings.resize(textId+1);
        m_textPosition.resize(textId+1);
    }

    std::vector<size_t> &textPrimitiveIds=m_textPrimitiveIds[textId];

    translate(pos.x, pos.y);
    if(m_textStrings[textId] == text)
    {
        if((pos.x != m_textPosition[textId].x) || (pos.y != m_textPosition[textId].y))
        {
            //move verts
            gweni::Point delta=pos-m_textPosition[textId];

            for(size_t i=0; i<textPrimitiveIds.size(); ++i)
            {
                size_t vertIndex=textPrimitiveIds[i]*4;

                for(size_t j=0; j<4; ++j)
                {
                    m_textVertexes[vertIndex+j].x=m_textVertexes[vertIndex+j].x+delta.x;
                    m_textVertexes[vertIndex+j].y=m_textVertexes[vertIndex+j].y+delta.y;
                    m_textVertexes[vertIndex+j].z=m_zIndex;
                }
                m_zIndex++;
            }
            m_textPosition[textId]=pos;
        }
//        m_zIndex++;
        return; //string hasn't changed
    }

    m_textStrings[textId]=text;
    m_textPosition[textId]=pos;

    GLFontData &fontData=m_lastFont->second;

    m_textTextureId=fontData.atlas->id;

    ftgl::text_buffer_t *textBuffer=ftgl::text_buffer_new();
    ftgl::texture_font_t *textFont=fontData.markup.font;
    
//    pos.y=pos.y+fontData.markup.font->ascender;
//    ftgl::vec2 pen={pos.x, pos.y};
    ftgl::vec2 pen={0.0f, 0.0f};

    ftgl::text_buffer_add_text(textBuffer, &pen, &fontData.markup, text.c_str(), text.length());

    ftgl::vector_t *indexes=textBuffer->buffer->indices;
    ftgl::vector_t *vertexes=textBuffer->buffer->vertices;

    size_t primitiveCount=indexes->size/6;

    if(textPrimitiveIds.size() != primitiveCount)
    {
        if(textPrimitiveIds.size() < primitiveCount)
        {
            for(size_t i=textPrimitiveIds.size(); i<primitiveCount; ++i)
            {
                textPrimitiveIds.push_back(m_textIds.getNextId(textId));
            }
        }
        else
        {
            for(size_t i=primitiveCount; i<textPrimitiveIds.size(); ++i)
            {
                m_textIds.releaseId(textPrimitiveIds[i]);
            }
            textPrimitiveIds.resize(primitiveCount);
        }
    }

    size_t textIdsSize=m_textIds.size();

    if(m_textIndexes.size() < textIdsSize*6)
        m_textIndexes.resize(textIdsSize*6);
    if(m_textVertexes.size() < textIdsSize*4)
        m_textVertexes.resize(textIdsSize*4);

//    textPrimitiveIds.push_back(m_textIds.getNextId(textId));

    GLuint *indexesBuffer=(GLuint *)indexes->items;
    ftgl::glyph_vertex_t *vertexBuffer=(ftgl::glyph_vertex_t *)vertexes->items;

    size_t indexPos=0;
    size_t vertexPos=0;
    for(size_t i=0; i<primitiveCount; ++i)
    {
        size_t index=textPrimitiveIds[i]*6;
        size_t vertIndex=textPrimitiveIds[i]*4;

        m_textIndexes[index++]=vertIndex+(indexesBuffer[indexPos++]%4);
        m_textIndexes[index++]=vertIndex+(indexesBuffer[indexPos++]%4);
        m_textIndexes[index++]=vertIndex+(indexesBuffer[indexPos++]%4);
        m_textIndexes[index++]=vertIndex+(indexesBuffer[indexPos++]%4);
        m_textIndexes[index++]=vertIndex+(indexesBuffer[indexPos++]%4);
        m_textIndexes[index++]=vertIndex+(indexesBuffer[indexPos++]%4);

        m_textVertexes[vertIndex]=vertexBuffer[vertexPos];
        m_textVertexes[vertIndex].x=pos.x+vertexBuffer[vertexPos].x;
//        m_textVertexes[vertIndex].y=pos.y+(textFont->height+vertexBuffer[vertexPos++].y);
        m_textVertexes[vertIndex].y=pos.y-vertexBuffer[vertexPos++].y;
        m_textVertexes[vertIndex++].z=m_zIndex;
        m_textVertexes[vertIndex]=vertexBuffer[vertexPos];
        m_textVertexes[vertIndex].x=pos.x+vertexBuffer[vertexPos].x;
        m_textVertexes[vertIndex].y=pos.y-vertexBuffer[vertexPos++].y;
        m_textVertexes[vertIndex++].z=m_zIndex;
        m_textVertexes[vertIndex]=vertexBuffer[vertexPos];
        m_textVertexes[vertIndex].x=pos.x+vertexBuffer[vertexPos].x;
        m_textVertexes[vertIndex].y=pos.y-vertexBuffer[vertexPos++].y;
        m_textVertexes[vertIndex++].z=m_zIndex;
        m_textVertexes[vertIndex]=vertexBuffer[vertexPos];
        m_textVertexes[vertIndex].x=pos.x+vertexBuffer[vertexPos].x;
        m_textVertexes[vertIndex].y=pos.y-vertexBuffer[vertexPos++].y;
        m_textVertexes[vertIndex++].z=m_zIndex;
        m_zIndex++;
    }
//    m_zIndex++;
//    //cant use, need to move vertexes to main vertex buffer
//    vertex_buffer_render(textBuffer->buffer, GL_TRIANGLES);

    text_buffer_delete(textBuffer);

    //    if (m_currentTexture != fontData.m_textureId)
    //    {
    //        flush();
    //        setTexture(fontData.m_textureId);
    //    }
    //    m_activeProgram = 2;
    //    float x = pos.x, y = pos.y;
    //    char* text_ptr = const_cast<char*>(text.c_str());
    //    // Height of font, allowing for descenders, because baseline is bottom of capitals.
    //    const float height = font.size * scale() * c_pointsToPixels * 0.8f;
    //
    //    while (const auto wide_char = utf8_to_wchart(text_ptr))
    //    {
    //        const auto c = wide_char - BeginCharacter;
    //
    //        if (wide_char < BeginCharacter || wide_char > LastCharacter)
    //            continue;
    //
    //        stbtt_aligned_quad q;
    //        stbtt_GetBakedQuad(reinterpret_cast<stbtt_bakedchar*>(fontData.baked_chars.data()),
    //            fontData.width, fontData.height,
    //            c,
    //            &x, &y, &q, 1); // 1=opengl & d3d10+,0=d3d9
    //
    //        Rect rect(q.x0, q.y0 + height, q.x1 - q.x0, q.y1 - q.y0);
    //
    //        translate(rect);
    //
    //        addVert(rect.x, rect.y, q.s0, q.t0);
    //        addVert(rect.x + rect.w, rect.y, q.s1, q.t0);
    //        addVert(rect.x, rect.y + rect.h, q.s0, q.t1);
    //        addVert(rect.x + rect.w, rect.y, q.s1, q.t0);
    //        addVert(rect.x + rect.w, rect.y + rect.h, q.s1, q.t1);
    //        addVert(rect.x, rect.y + rect.h, q.s0, q.t1);
    //    }
}

gweni::Point OpenGLCore::measureText(const gweni::Font &font, const gweni::String &text)
{
    if(!ensureFont(font))
        return gweni::Point(0, 0);

    GLFontData &fontData=m_lastFont->second;

    Point sz(0, font.size * scale() * c_pointsToPixels);

    float x=0.f, y=0.f;
    char *text_ptr=const_cast<char *>(text.c_str());

    ftgl::text_buffer_t *textBuffer=ftgl::text_buffer_new();
    ftgl::vec2 pen={0.f, 0.f};

    ftgl::text_buffer_add_text(textBuffer, &pen, &fontData.markup, text.c_str(), text.length());

    ftgl::vector_t *vertices=textBuffer->buffer->vertices;
    float *verticesBuffer=(float *)vertices->items;

//    assert(vertices->item_size == 4);
    assert(vertices->item_size == 44);

    size_t index=0;
    for(size_t i=0; i<vertices->size; ++i)
    {
        x=std::max(x, verticesBuffer[index]);
        y=std::max(y, verticesBuffer[index+1]);
        index+=11;
    }

    sz.x=(int)x;
    sz.y=(int)ceil(fontData.markup.font->height);

    text_buffer_delete(textBuffer);

    //while (const auto wide_char = utf8_to_wchart(text_ptr))
    //{
    //    const auto c = wide_char - BeginCharacter;
    //
    //    if (wide_char < BeginCharacter || wide_char > LastCharacter)
    //        continue;
    //
    //
    //    stbtt_aligned_quad q;
    //    stbtt_GetBakedQuad(reinterpret_cast<stbtt_bakedchar*>(fontData.baked_chars.data()),
    //        fontData.width, fontData.height,
    //        c,
    //        &x, &y, &q, 1); // 1=opengl & d3d10+,0=d3d9
    //
    //    sz.x = q.x1;
    //    sz.y = std::max(sz.y, int((q.y1 - q.y0) * c_pointsToPixels));
    //}

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
    std::cout << "ResizedContext w=" << w << ", h=" << h << std::endl;
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
    std::cout << "BeginContext" << std::endl;
    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    return true;
}

bool OpenGLCore::endContext(gweni::WindowProvider *window)
{
    std::cout << "EndContext" << std::endl;
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
