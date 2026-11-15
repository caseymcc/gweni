/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  OpenGLCore::setPreserveAppState(true) gives an app that draws its own GL
 *  scene its state back (#687). In the doctest test suite "gl_only": it reads
 *  and sets GL state through glGet* and the GL calls themselves, which only the
 *  GL backend has (Vulkan has no such global state to give back).
 *
 *  The app's state is made unlike every default a frame relies on: its own
 *  program, vertex array, array buffer, active unit 5, 2D and 2D-array
 *  textures and sampler objects on gweni's units, blend on with other factors
 *  and a reverse-subtract equation, the depth test off with GL_GREATER, no
 *  depth writes and a 0.2..0.8 depth range, cull face, scissor and stencil on,
 *  a scissor box, a colour mask, a viewport, pack and unpack alignment, row
 *  length and skips, a pixel unpack buffer, and on desktop GL a line polygon
 *  mode, GL_FRAMEBUFFER_SRGB and primitive restart at index 7.
 *
 *    state    a frame that lays out new text (its glyphs uploaded to the atlas)
 *             with preserve on leaves every one of those glGet* values as it
 *             was; with preserve off some differ (what the frame sets).
 *    pixels   with preserve on, a frame of a settled canvas over that state
 *             draws byte for byte what it draws over gweni's own state.
 *    update   with preserve on, an update phase whose layout loads a new font
 *             (its atlas texture made where it is first measured, outside a
 *             frame) leaves every one of those values as it was, and makes no
 *             GL error under the app's pixel unpack buffer.
 */
#include <doctest/doctest.h>

#include "glRenderer.h"

#include <gweni/platforms/renderers/glApi.h>

#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/label.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <map>
#include <string>
#include <vector>

TEST_SUITE_BEGIN("gl_only");

namespace
{

using gweni::Color;

//! The units the frame uses: 0 (atlas), 1 (clip boxes), 2..6 (image pages)
constexpr int Units=7;

//! Every value the app's state is checked by, by name.
std::map<std::string, std::vector<GLint>> snapshot()
{
    std::map<std::string, std::vector<GLint>> state;
    auto integers=[&state](const char *name, GLenum pname, int count) {
        std::vector<GLint> values((size_t)count, 0);
        glGetIntegerv(pname, values.data());
        state[name]=values;
    };
    auto booleans=[&state](const char *name, GLenum pname, int count) {
        std::vector<GLboolean> values((size_t)count, 0);
        glGetBooleanv(pname, values.data());
        state[name]=std::vector<GLint>(values.begin(), values.end());
    };
    auto enabled=[&state](const char *name, GLenum cap) {
        state[name]={ glIsEnabled(cap) ? 1 : 0 };
    };

    integers("program", GL_CURRENT_PROGRAM, 1);
    integers("vertexArray", GL_VERTEX_ARRAY_BINDING, 1);
    integers("arrayBuffer", GL_ARRAY_BUFFER_BINDING, 1);
    integers("activeTexture", GL_ACTIVE_TEXTURE, 1);
    GLint active=0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    for(int unit=0; unit < Units; ++unit)
    {
        const std::string u=std::to_string(unit);

        glActiveTexture(GL_TEXTURE0+(GLenum)unit);
        integers(("texture2D"+u).c_str(), GL_TEXTURE_BINDING_2D, 1);
        integers(("texture2DArray"+u).c_str(), GL_TEXTURE_BINDING_2D_ARRAY, 1);
        integers(("sampler"+u).c_str(), GL_SAMPLER_BINDING, 1);
    }
    glActiveTexture((GLenum)active);
    enabled("blend", GL_BLEND);
    integers("blendSrcRgb", GL_BLEND_SRC_RGB, 1);
    integers("blendDstRgb", GL_BLEND_DST_RGB, 1);
    integers("blendSrcAlpha", GL_BLEND_SRC_ALPHA, 1);
    integers("blendDstAlpha", GL_BLEND_DST_ALPHA, 1);
    integers("blendEquationRgb", GL_BLEND_EQUATION_RGB, 1);
    integers("blendEquationAlpha", GL_BLEND_EQUATION_ALPHA, 1);
    enabled("depthTest", GL_DEPTH_TEST);
    integers("depthFunc", GL_DEPTH_FUNC, 1);
    booleans("depthMask", GL_DEPTH_WRITEMASK, 1);
    GLfloat range[2]={};
    glGetFloatv(GL_DEPTH_RANGE, range);
    state["depthRange"]={ (GLint)(range[0]*1000.0f+0.5f), (GLint)(range[1]*1000.0f+0.5f) };
    enabled("cullFace", GL_CULL_FACE);
    enabled("scissorTest", GL_SCISSOR_TEST);
    integers("scissorBox", GL_SCISSOR_BOX, 4);
    enabled("stencilTest", GL_STENCIL_TEST);
    booleans("colorMask", GL_COLOR_WRITEMASK, 4);
    integers("viewport", GL_VIEWPORT, 4);
    integers("packAlignment", GL_PACK_ALIGNMENT, 1);
    integers("unpackAlignment", GL_UNPACK_ALIGNMENT, 1);
    integers("unpackRowLength", GL_UNPACK_ROW_LENGTH, 1);
    integers("unpackSkipRows", GL_UNPACK_SKIP_ROWS, 1);
    integers("unpackSkipPixels", GL_UNPACK_SKIP_PIXELS, 1);
    integers("pixelUnpackBuffer", GL_PIXEL_UNPACK_BUFFER_BINDING, 1);
#ifndef GWENI_GL_ES
    integers("polygonMode", GL_POLYGON_MODE, 2);
    enabled("framebufferSrgb", GL_FRAMEBUFFER_SRGB);
    enabled("primitiveRestart", GL_PRIMITIVE_RESTART);
    integers("primitiveRestartIndex", GL_PRIMITIVE_RESTART_INDEX, 1);
#endif
    return state;
}

GLuint compile(GLenum stage, const char *body)
{
    const bool es=gweni::test::glRenderer().glCapabilities().dialect == gweni::renderer::GLSLDialect::Es300;
    const std::string source=std::string(es ? "#version 300 es\nprecision highp float;\n" : "#version 330 core\n")
        +body;
    const char *text=source.c_str();
    const GLuint shader=glCreateShader(stage);

    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);
    return shader;
}

//! The app's objects and the state made of them, set and taken down.
struct AppState
{
    GLuint program=0;
    GLuint vertexArray=0;
    GLuint buffer=0;
    GLuint pixelBuffer=0;
    GLuint textures[3]={};
    GLuint samplers[2]={};
    std::map<std::string, std::vector<GLint>> before;

    AppState()
    {
        before=snapshot();

        const GLuint vertex=compile(GL_VERTEX_SHADER, "void main() { gl_Position=vec4(0.0); }\n");
        const GLuint fragment=compile(GL_FRAGMENT_SHADER, "out vec4 color;\nvoid main() { color=vec4(1.0); }\n");
        program=glCreateProgram();
        glAttachShader(program, vertex);
        glAttachShader(program, fragment);
        glLinkProgram(program);
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        GLint linked=0;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        REQUIRE(linked == GL_TRUE);

        glGenVertexArrays(1, &vertexArray);
        glGenBuffers(1, &buffer);
        glGenBuffers(1, &pixelBuffer);
        glGenTextures(3, textures);
        glGenSamplers(2, samplers);
        glSamplerParameteri(samplers[0], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glSamplerParameteri(samplers[0], GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glUseProgram(program);
        glBindVertexArray(vertexArray);
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        // too small for any upload gweni makes: one read from it is an error
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pixelBuffer);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, 16, nullptr, GL_STREAM_DRAW);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textures[0]);
        glBindSampler(0, samplers[0]);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D_ARRAY, textures[1]);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, textures[2]);
        glBindSampler(4, samplers[1]);
        glActiveTexture(GL_TEXTURE5);
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ZERO, GL_ONE);
        glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_SUBTRACT);
        glDisable(GL_DEPTH_TEST);
        glDepthFunc(GL_GREATER);
        glDepthMask(GL_FALSE);
#ifdef GWENI_GL_ES
        glDepthRangef(0.2f, 0.8f);
#else
        glDepthRange(0.2, 0.8);
#endif
        glEnable(GL_CULL_FACE);
        glEnable(GL_SCISSOR_TEST);
        glScissor(3, 4, 50, 60);
        glEnable(GL_STENCIL_TEST);
        glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE);
        glViewport(5, 6, 100, 80);
        glPixelStorei(GL_PACK_ALIGNMENT, 2);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 7);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 1);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 2);
#ifndef GWENI_GL_ES
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glEnable(GL_FRAMEBUFFER_SRGB);
        glEnable(GL_PRIMITIVE_RESTART);
        glPrimitiveRestartIndex(7);
#endif
        REQUIRE(glGetError() == GL_NO_ERROR);
    }

    //! Back to the state before it, as the next case expects it.
    ~AppState()
    {
        glUseProgram((GLuint)before["program"][0]);
        glBindVertexArray((GLuint)before["vertexArray"][0]);
        glBindBuffer(GL_ARRAY_BUFFER, (GLuint)before["arrayBuffer"][0]);
        for(int unit=0; unit < Units; ++unit)
        {
            glActiveTexture(GL_TEXTURE0+(GLenum)unit);
            glBindTexture(GL_TEXTURE_2D, (GLuint)before["texture2D"+std::to_string(unit)][0]);
            glBindTexture(GL_TEXTURE_2D_ARRAY, (GLuint)before["texture2DArray"+std::to_string(unit)][0]);
            glBindSampler((GLuint)unit, 0);
        }
        glActiveTexture((GLenum)before["activeTexture"][0]);
        glDeleteProgram(program);
        glDeleteVertexArrays(1, &vertexArray);
        glDeleteBuffers(1, &buffer);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glDeleteBuffers(1, &pixelBuffer);
        glDeleteTextures(3, textures);
        glDeleteSamplers(2, samplers);
        glBlendFuncSeparate((GLenum)before["blendSrcRgb"][0], (GLenum)before["blendDstRgb"][0],
            (GLenum)before["blendSrcAlpha"][0], (GLenum)before["blendDstAlpha"][0]);
        glBlendEquation(GL_FUNC_ADD);
        if(before["blend"][0])
            glEnable(GL_BLEND);
        else
            glDisable(GL_BLEND);
        if(before["depthTest"][0])
            glEnable(GL_DEPTH_TEST);
        glDepthFunc((GLenum)before["depthFunc"][0]);
        glDepthMask(GL_TRUE);
#ifdef GWENI_GL_ES
        glDepthRangef(0.0f, 1.0f);
#else
        glDepthRange(0.0, 1.0);
#endif
        glDisable(GL_CULL_FACE);
        glDisable(GL_SCISSOR_TEST);
        const std::vector<GLint> &box=before["scissorBox"];
        glScissor(box[0], box[1], box[2], box[3]);
        glDisable(GL_STENCIL_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        const std::vector<GLint> &view=before["viewport"];
        glViewport(view[0], view[1], view[2], view[3]);
        glPixelStorei(GL_PACK_ALIGNMENT, before["packAlignment"][0]);
        glPixelStorei(GL_UNPACK_ALIGNMENT, before["unpackAlignment"][0]);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
#ifndef GWENI_GL_ES
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDisable(GL_FRAMEBUFFER_SRGB);
        glDisable(GL_PRIMITIVE_RESTART);
        glPrimitiveRestartIndex((GLuint)before["primitiveRestartIndex"][0]);
#endif
        gweni::test::glRenderer().setPreserveAppState(false);
        CHECK(glGetError() == GL_NO_ERROR);
        CHECK((snapshot() == before));
    }
};

struct Scene
{
    gweni::skin::textured::TexturedSkin *skin;
    gweni::controls::Canvas *canvas;
    gweni::controls::Label *label;

    Scene()
    {
        const gweni::Point viewSize=gweni::test::viewSize();

        skin=new gweni::skin::textured::TexturedSkin(&gweni::test::gpuRenderer());
        REQUIRE(skin->init("DefaultSkin.json"));
        skin->setDefaultFont("OpenSans.ttf", 11);

        canvas=new gweni::controls::Canvas(skin);
        canvas->setSize(viewSize.x, viewSize.y);

        gweni::controls::Button *button=canvas->newChild<gweni::controls::Button>();
        button->setText("Button");
        button->setPos(10, 10);

        label=canvas->newChild<gweni::controls::Label>();
        label->setText("Label");
        label->setPos(10, 60);
        label->sizeToContents();
    }

    ~Scene()
    {
        delete canvas;
        delete skin;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    std::vector<uint8_t> settled()
    {
        std::vector<uint8_t> pixels;

        for(int i=0; i < 3; ++i)
        {
            gweni::test::clearView(Color(10, 20, 30, 255));
            frame();
        }
        gweni::test::readView(pixels);
        return pixels;
    }
};

//! The names whose values differ.
std::string differences(const std::map<std::string, std::vector<GLint>> &a,
    const std::map<std::string, std::vector<GLint>> &b)
{
    std::string names;

    for(const auto &entry:a)
    {
        auto other=b.find(entry.first);

        if((other == b.end()) || (other->second != entry.second))
            names+=entry.first+" ";
    }
    return names;
}

}//namespace

TEST_CASE("App state: with setPreserveAppState a frame gives every saved GL value back")
{
    Scene scene;
    scene.settled();
    {
        AppState app;
        const auto set=snapshot();

        gweni::test::glRenderer().setPreserveAppState(true);
        // new glyphs: the frame uploads atlas rows as well as drawing
        scene.label->setText("Ωμέγα ẞ ŋ");
        scene.frame();
        CHECK(glGetError() == GL_NO_ERROR);
        const auto after=snapshot();
        INFO("changed: ", differences(set, after));
        CHECK((after == set));
    }
    {
        AppState app;
        const auto set=snapshot();

        // what the frame changes without it, so the state above is a real test;
        // glyphs already in the atlas, so nothing is uploaded under the app's
        // unpack state, and no pixel buffer is bound for the clip-box upload
        // the new text makes (both of which are errors)
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        scene.label->setText("Label");
        scene.frame();
        CHECK_FALSE((snapshot() == set));
    }
}

TEST_CASE("App state: with setPreserveAppState an update phase that loads a font gives every saved GL value back")
{
    Scene scene;
    scene.settled();
    {
        AppState app;
        const size_t fonts=gweni::test::gpuRenderer().getLoadedFontCount();
        const auto set=snapshot();

        gweni::test::glRenderer().setPreserveAppState(true);
        // a size nothing else uses, so its atlas is made by this layout's measure
        scene.label->setFont("OpenSans.ttf", 15, true);
        scene.canvas->updateCanvas();
        REQUIRE(gweni::test::gpuRenderer().getLoadedFontCount() > fonts);
        CHECK(glGetError() == GL_NO_ERROR);
        const auto after=snapshot();
        INFO("changed: ", differences(set, after));
        CHECK((after == set));
        scene.canvas->renderCanvas();
        CHECK(glGetError() == GL_NO_ERROR);
        CHECK((snapshot() == set));
    }
}

TEST_CASE("App state: with setPreserveAppState a frame over the app's state draws as over gweni's")
{
    Scene scene;
    const std::vector<uint8_t> reference=scene.settled();
    std::vector<uint8_t> pixels;

    gweni::test::clearView(Color(10, 20, 30, 255));
    {
        AppState app;

        gweni::test::glRenderer().setPreserveAppState(true);
        scene.frame();
    }
    gweni::test::readView(pixels);

    REQUIRE(pixels.size() == reference.size());
    size_t differing=0;

    for(size_t i=0; i < pixels.size(); ++i)
        if(pixels[i] != reference[i])
            ++differing;
    CHECK(differing == 0);
}

TEST_SUITE_END();
