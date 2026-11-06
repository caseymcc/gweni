/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  OpenGLCore releases what it owns: freeFont deletes the font's atlas texture
 *  and its freetype-gl manager, loading a font twice makes nothing new, the text
 *  draw never binds a freed atlas, and deleting a renderer deletes every GL
 *  object it made without disturbing another renderer in the same context.
 *
 *  Every font here is a fresh Font (another size than test::openSans()), so
 *  freeing it leaves the shared fixture loaded.
 */
#include <doctest/doctest.h>

#include "capture.h"
#include "gpuRenderer.h"

#include <gweni/platforms/commonPlatform.h>

#include <vector>

namespace
{

using gweni::Color;
using gweni::Font;
using gweni::Point;
using gweni::String;
using gweni::renderer::OpenGLCore;

const Color background(0, 0, 255, 255);
const Color white(255, 255, 255, 255);

Font makeFont(int size)
{
    Font font;

    font.facename="OpenSans";
    font.size=size;
    return font;
}

//! The offscreen framebuffer, top-down RGBA8.
std::vector<uint8_t> readFrame()
{
    const Point size=gweni::test::viewSize();
    std::vector<uint8_t> pixels;

    gweni::capture::readFramebufferRgba(size.x, size.y, pixels);
    return pixels;
}

//! Pixels that are not the background colour.
size_t drawnPixels(const std::vector<uint8_t> &pixels)
{
    size_t drawn=0;

    for(size_t i=0; i+3 < pixels.size(); i+=4)
    {
        if((pixels[i] != background.r) || (pixels[i+1] != background.g) || (pixels[i+2] != background.b))
            ++drawn;
    }
    return drawn;
}

void clearFrame()
{
    glClearColor(background.r/255.0f, background.g/255.0f, background.b/255.0f, 1.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
}

//! Draws `text` alone, white at (8, 40), in one frame of `renderer`, and reads it back.
std::vector<uint8_t> drawText(OpenGLCore &renderer, const Font &font, const String &text)
{
    size_t textId=0;

    clearFrame();
    renderer.generateTextPrimitive(textId);
    renderer.begin();
    renderer.setDrawColor(white);
    renderer.renderText(textId, font, Point(8, 40), 1, text);
    renderer.end();

    std::vector<uint8_t> pixels=readFrame();

    renderer.releaseTextPrimitive(textId);
    renderer.begin();
    renderer.end();
    return pixels;
}

GLuint boundTexture2D()
{
    GLint name=0;

    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &name);
    return (GLuint)name;
}

void clearGlErrors()
{
    while(glGetError() != GL_NO_ERROR)
    {}
}

}//namespace

TEST_CASE("freeFont deletes the font's atlas texture")
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    const Font font=makeFont(13);
    const size_t fontsBefore=renderer.getLoadedFontCount();

    REQUIRE(renderer.loadFont(font) == Font::Status::Loaded);

    const unsigned int atlas=renderer.getFontAtlasName(font);

    REQUIRE(atlas != 0);
    CHECK(glIsTexture(atlas) == GL_TRUE);
    CHECK(renderer.getLoadedFontCount() == fontsBefore+1);

    renderer.freeFont(font);

    CHECK(glIsTexture(atlas) == GL_FALSE);
    CHECK(renderer.getFontAtlasName(font) == 0);
    CHECK(renderer.getLoadedFontCount() == fontsBefore);

    // freeing it again, or a font never loaded, does nothing
    renderer.freeFont(font);
    renderer.freeFont(makeFont(77));
    CHECK(renderer.getLoadedFontCount() == fontsBefore);
    CHECK(glIsTexture(renderer.getFontAtlasName(gweni::test::openSans())) == GL_TRUE);
}

TEST_CASE("loadFont of a font already loaded returns Loaded and makes nothing new")
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    const Font font=makeFont(13);
    const size_t fontsBefore=renderer.getLoadedFontCount();

    REQUIRE(renderer.loadFont(font) == Font::Status::Loaded);

    const unsigned int atlas=renderer.getFontAtlasName(font);
    const unsigned int sharedAtlas=renderer.getFontAtlasName(gweni::test::openSans());

    // a load that makes an atlas binds it (and freeing a duplicate would leave 0
    // bound), so an unchanged binding shows no texture was made
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sharedAtlas);

    CHECK(renderer.loadFont(font) == Font::Status::Loaded);
    CHECK(boundTexture2D() == sharedAtlas);
    CHECK(renderer.getFontAtlasName(font) == atlas);
    CHECK(renderer.getLoadedFontCount() == fontsBefore+1);
    // still usable through the entry it returned
    CHECK(renderer.measureText(font, "Hello").x > 0);

    renderer.freeFont(font);
    CHECK(glIsTexture(atlas) == GL_FALSE);
    CHECK(renderer.getLoadedFontCount() == fontsBefore);
}

TEST_CASE("After freeing the font text was last drawn with, the text draw binds a live atlas")
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    const Font font=makeFont(15);

    REQUIRE(renderer.loadFont(font) == Font::Status::Loaded);

    const unsigned int freedAtlas=renderer.getFontAtlasName(font);

    // the last renderText, so flush() binds this font's atlas for the text draw
    CHECK(drawnPixels(drawText(renderer, font, "Freed")) > 0);
    CHECK(boundTexture2D() == freedAtlas);

    clearGlErrors();
    renderer.freeFont(font);
    REQUIRE(glIsTexture(freedAtlas) == GL_FALSE);

    // a frame with no new text still draws the text mirror
    clearFrame();
    renderer.begin();
    renderer.end();
    CHECK(glGetError() == GL_NO_ERROR);
    CHECK(boundTexture2D() != freedAtlas);

    const unsigned int liveAtlas=renderer.getFontAtlasName(gweni::test::openSans());
    const std::vector<uint8_t> pixels=drawText(renderer, gweni::test::openSans(), "Hello");

    CHECK(glGetError() == GL_NO_ERROR);
    CHECK(drawnPixels(pixels) > 20);
    CHECK(boundTexture2D() == liveAtlas);
    CHECK(glIsTexture(liveAtlas) == GL_TRUE);
}

TEST_CASE("Load, free and load the same face again: the same text draws the same pixels")
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    const Font font=makeFont(12);
    const String text="Hello gweni 0123";

    REQUIRE(renderer.loadFont(font) == Font::Status::Loaded);

    const std::vector<uint8_t> first=drawText(renderer, font, text);

    renderer.freeFont(font);
    REQUIRE(renderer.getFontAtlasName(font) == 0);
    REQUIRE(renderer.loadFont(font) == Font::Status::Loaded);

    const std::vector<uint8_t> second=drawText(renderer, font, text);

    CHECK(drawnPixels(first) > 20);
    CHECK(first == second);

    renderer.freeFont(font);
}

TEST_CASE("Deleting a renderer deletes its GL objects and leaves another renderer drawing")
{
    OpenGLCore &shared=gweni::test::gpuRenderer();
    const unsigned int sharedAtlas=shared.getFontAtlasName(gweni::test::openSans());

    REQUIRE(sharedAtlas != 0);

    gweni::platform::CurrentPath paths("resources");
    OpenGLCore *renderer=new OpenGLCore(paths, gweni::Rect(Point(0, 0), gweni::test::viewSize()));

    renderer->init();

    const Font font=makeFont(14);
    gweni::Texture texture;

    texture.name="test16.png";
    REQUIRE(renderer->loadFont(font) == Font::Status::Loaded);
    REQUIRE(renderer->loadTexture(texture) == gweni::Texture::Status::Loaded);

    size_t primitiveId=0;
    size_t textId=0;

    renderer->generatePrimitive(primitiveId);
    renderer->generateTextPrimitive(textId);

    clearFrame();
    renderer->begin();
    renderer->drawTexturedRect(texture, primitiveId, gweni::Rect(8, 8, 16, 16), 1);
    renderer->setDrawColor(white);
    renderer->renderText(textId, font, Point(32, 40), 1, "Second");
    renderer->end();
    CHECK(drawnPixels(readFrame()) > 20);

    const unsigned int atlas=renderer->getFontAtlasName(font);
    const unsigned int textureName=renderer->getTextureName(texture);
    const OpenGLCore::GLObjectNames names=renderer->getGLObjectNames();

    REQUIRE(glIsTexture(atlas) == GL_TRUE);
    REQUIRE(glIsTexture(textureName) == GL_TRUE);
    for(unsigned int name:names.buffers)
        REQUIRE(glIsBuffer(name) == GL_TRUE);
    for(unsigned int name:names.vertexArrays)
        REQUIRE(glIsVertexArray(name) == GL_TRUE);
    for(unsigned int name:names.textures)
        REQUIRE(glIsTexture(name) == GL_TRUE);
    for(unsigned int name:names.programs)
        REQUIRE(glIsProgram(name) == GL_TRUE);

    // the ids come from the process-wide banks, not the renderer
    renderer->releasePrimitive(primitiveId);
    renderer->releaseTextPrimitive(textId);

    clearGlErrors();
    delete renderer;
    CHECK(glGetError() == GL_NO_ERROR);

    CHECK(glIsTexture(atlas) == GL_FALSE);
    CHECK(glIsTexture(textureName) == GL_FALSE);
    for(unsigned int name:names.buffers)
        CHECK(glIsBuffer(name) == GL_FALSE);
    for(unsigned int name:names.vertexArrays)
        CHECK(glIsVertexArray(name) == GL_FALSE);
    for(unsigned int name:names.textures)
        CHECK(glIsTexture(name) == GL_FALSE);
    for(unsigned int name:names.programs)
        CHECK(glIsProgram(name) == GL_FALSE);

    // the shared renderer is untouched and still draws text
    CHECK(glIsTexture(sharedAtlas) == GL_TRUE);
    CHECK(drawnPixels(drawText(shared, gweni::test::openSans(), "Hello")) > 20);
    CHECK(glGetError() == GL_NO_ERROR);
}
