/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  OpenGLCore's shaders keep to what GLSL 3.30 and GLSL ES 3.00 share: each
 *  stage is one body behind a version header (OpenGLCore::shaderSource), and
 *  init() compiles the dialect of its context. Here every stage is compiled and
 *  each program linked in the dialects this desktop context does not run: the
 *  ES 3.00 bodies (which a GLES 3.0 or WebGL 2 context would compile) and the
 *  text program without its dual-source output (which a context without
 *  dual-source blending gets). An implicit conversion, texture2D, a buffer
 *  texture or gl_PrimitiveID in a body fails here, without a GLES context.
 *
 *  A desktop context compiles `#version 300 es` through GL_ARB_ES3_compatibility
 *  (Mesa's llvmpipe has it); without it the ES cases are skipped with a message.
 *  The ES text program with its dual-source output needs
 *  GL_EXT_blend_func_extended, an ES extension that desktop contexts do not list,
 *  so it is compiled only where the context lists it.
 *
 *  GL only (the test suite "gl_only"): it compiles OpenGLCore's GLSL sources in
 *  the GL dialects.
 */
#include <doctest/doctest.h>

#include "glRenderer.h"

#include <string>
#include <vector>

TEST_SUITE_BEGIN("gl_only");

namespace
{

using gweni::renderer::GLSLDialect;
using gweni::renderer::GLShaderSource;
using gweni::renderer::OpenGLCore;

//! Compiles one stage; the log on failure.
GLuint compile(GLenum type, const std::string &source, std::string &log)
{
    const GLuint shader=glCreateShader(type);
    const char *text=source.c_str();
    GLint success=0;

    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if(!success)
    {
        char info[4096]={};

        glGetShaderInfoLog(shader, sizeof(info), nullptr, info);
        log+=info;
    }
    return shader;
}

//! Compiles both stages of a program in a dialect and links them: an empty
//! string, or what the compiler and linker said.
std::string build(GLShaderSource vertex, GLShaderSource fragment, GLSLDialect dialect, bool dualSource)
{
    std::string log;
    const GLuint vs=compile(GL_VERTEX_SHADER, OpenGLCore::shaderSource(vertex, dialect, dualSource), log);
    const GLuint fs=compile(GL_FRAGMENT_SHADER, OpenGLCore::shaderSource(fragment, dialect, dualSource), log);

    if(log.empty())
    {
        const GLuint program=glCreateProgram();
        GLint success=0;

        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if(!success)
        {
            char info[4096]={};

            glGetProgramInfoLog(program, sizeof(info), nullptr, info);
            log+=info;
        }
        glDeleteProgram(program);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return log;
}

void checkPrograms(GLSLDialect dialect, bool dualSource)
{
    const std::string primitive=build(GLShaderSource::PrimitiveVertex, GLShaderSource::PrimitiveFragment,
        dialect, dualSource);
    const std::string text=build(GLShaderSource::TextVertex, GLShaderSource::TextFragment, dialect, dualSource);

    CAPTURE(dualSource);
    INFO("primitive program: ", primitive);
    CHECK(primitive.empty());
    INFO("text program: ", text);
    CHECK(text.empty());
}

}//namespace

TEST_CASE("GLSL dialects: the headers in front of the shared bodies")
{
    const std::string desktop=OpenGLCore::shaderSource(GLShaderSource::TextFragment, GLSLDialect::Glsl330, true);
    const std::string es=OpenGLCore::shaderSource(GLShaderSource::TextFragment, GLSLDialect::Es300, false);
    const std::string esDual=OpenGLCore::shaderSource(GLShaderSource::TextFragment, GLSLDialect::Es300, true);

    CHECK(desktop.rfind("#version 330 core\n", 0) == 0);
    CHECK(desktop.find("#define GWENI_DUAL_SOURCE") != std::string::npos);
    CHECK(es.rfind("#version 300 es\n", 0) == 0);
    for(const char *precision:{ "precision highp float;", "precision highp int;",
        "precision highp sampler2D;", "precision highp usampler2D;" })
    {
        CAPTURE(precision);
        CHECK(es.find(precision) != std::string::npos);
    }
    CHECK(es.find("#define GWENI_DUAL_SOURCE") == std::string::npos);
    CHECK(es.find("GL_EXT_blend_func_extended") == std::string::npos);
    CHECK(esDual.find("#extension GL_EXT_blend_func_extended : require") != std::string::npos);
    // only the text fragment shader has the second output
    CHECK(OpenGLCore::shaderSource(GLShaderSource::PrimitiveFragment, GLSLDialect::Es300, true)
        .find("GL_EXT_blend_func_extended") == std::string::npos);
}

TEST_CASE("GLSL dialects: the desktop programs compile and link without dual-source blending too")
{
    gweni::test::glRenderer();
    checkPrograms(GLSLDialect::Glsl330, true);
    checkPrograms(GLSLDialect::Glsl330, false);
}

TEST_CASE("GLSL dialects: every program compiles and links as GLSL ES 3.00")
{
    gweni::test::glRenderer();

    if(!OpenGLCore::hasExtension("GL_ARB_ES3_compatibility"))
    {
        MESSAGE("skipped: this context has no GL_ARB_ES3_compatibility to compile #version 300 es with");
        return;
    }

    checkPrograms(GLSLDialect::Es300, false);
    if(OpenGLCore::hasExtension("GL_EXT_blend_func_extended"))
        checkPrograms(GLSLDialect::Es300, true);
    else
        MESSAGE("the ES text program's dual-source output is not compiled: no GL_EXT_blend_func_extended");
}

TEST_SUITE_END();
