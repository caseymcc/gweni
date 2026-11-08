/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  OpenGLCore releases what it owns: freeFont deletes the font's atlas texture
 *  and its freetype-gl manager, loading a font twice makes nothing new and takes
 *  two frees to go, a font with a run laid out in it stays (the run drawing)
 *  until that run leaves it, and deleting a renderer deletes every GL object it
 *  made without disturbing another renderer in the same context.
 *
 *  Every font here is a fresh Font (another size than test::openSans()), so
 *  freeing it leaves the shared fixture loaded. A case skips itself when
 *  GWENI_SDF_TEXT_MIN puts its font and the fixture in the face's one SDF atlas.
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

//! Whether font has an atlas of its own, apart from the shared fixture's.
bool ownAtlas(const Font &font)
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();

    if((renderer.getTextMode(font) == gweni::renderer::GLTextMode::Coverage)
        || (renderer.getTextMode(gweni::test::openSans()) == gweni::renderer::GLTextMode::Coverage))
        return true;
    MESSAGE("skipped: GWENI_SDF_TEXT_MIN puts it in the shared fixture's SDF atlas");
    return false;
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

    if(!ownAtlas(font))
        return;
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
    renderer.freeFont(makeFont(17));
    CHECK(renderer.getLoadedFontCount() == fontsBefore);
    CHECK(glIsTexture(renderer.getFontAtlasName(gweni::test::openSans())) == GL_TRUE);
}

TEST_CASE("loadFont of a font already loaded returns Loaded, makes nothing new, and takes a second freeFont")
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    const Font font=makeFont(13);

    if(!ownAtlas(font))
        return;
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

    // two owners: the first free leaves it to the other
    renderer.freeFont(font);
    CHECK(renderer.getFontAtlasName(font) == atlas);
    CHECK(glIsTexture(atlas) == GL_TRUE);
    CHECK(renderer.getLoadedFontCount() == fontsBefore+1);

    // the second, with no run laid out in it, deletes it
    renderer.freeFont(font);
    CHECK(glIsTexture(atlas) == GL_FALSE);
    CHECK(renderer.getFontAtlasName(font) == 0);
    CHECK(renderer.getLoadedFontCount() == fontsBefore);
}

namespace
{

//! How a run leaves the font it was laid out in.
enum class Leave
{
    Released, //!< releaseTextPrimitive
    OtherFont //!< laid out again in test::openSans()
};

//! Lays a run out in font (loaded once), frees the font, and checks that the run
//! keeps drawing from the same atlas until it leaves the font, which then goes.
void freeWhileARunUsesIt(const Font &font, Leave leave)
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    const size_t fontsBefore=renderer.getLoadedFontCount();

    REQUIRE(renderer.loadFont(font) == Font::Status::Loaded);

    const unsigned int atlas=renderer.getFontAtlasName(font);
    size_t textId=0;

    REQUIRE(atlas != 0);
    renderer.generateTextPrimitive(textId);

    // the only run, so the text draw binds this font's atlas
    clearFrame();
    renderer.begin();
    renderer.setDrawColor(white);
    renderer.renderText(textId, font, Point(8, 40), 1, "Freed");
    renderer.end();

    const std::vector<uint8_t> drawn=readFrame();

    REQUIRE(drawnPixels(drawn) > 20);
    CHECK(boundTexture2D() == atlas);

    // its one owner frees it, and the run holds it
    clearGlErrors();
    renderer.freeFont(font);
    CHECK(renderer.getFontAtlasName(font) == atlas);
    CHECK(glIsTexture(atlas) == GL_TRUE);
    CHECK(renderer.getLoadedFontCount() == fontsBefore+1);

    // a frame with no new text draws the run as it was, from the same atlas
    clearFrame();
    renderer.begin();
    renderer.end();
    CHECK(glGetError() == GL_NO_ERROR);
    CHECK(boundTexture2D() == atlas);
    CHECK(readFrame() == drawn);
    CHECK(renderer.getFrameStats().atlasUploadCalls == 0);

    // a freeFont with no load to match it is deferred the same way
    renderer.freeFont(font);
    CHECK(renderer.getFontAtlasName(font) == atlas);
    clearFrame();
    renderer.begin();
    renderer.end();
    CHECK(readFrame() == drawn);

    // the run leaving it takes the font with it
    if(leave == Leave::Released)
        renderer.releaseTextPrimitive(textId);
    else
    {
        clearFrame();
        renderer.begin();
        renderer.setDrawColor(white);
        renderer.renderText(textId, gweni::test::openSans(), Point(8, 40), 1, "Freed");
        renderer.end();
        CHECK(drawnPixels(readFrame()) > 20);
    }

    CHECK(renderer.getFontAtlasName(font) == 0);
    CHECK(glIsTexture(atlas) == GL_FALSE);
    CHECK(renderer.getLoadedFontCount() == fontsBefore);

    // and nothing is drawn with the deleted atlas, or with whatever took its name
    clearFrame();
    renderer.begin();
    renderer.end();
    CHECK(glGetError() == GL_NO_ERROR);
    CHECK(boundTexture2D() != atlas);

    if(leave == Leave::OtherFont)
        renderer.releaseTextPrimitive(textId);
    renderer.begin();
    renderer.end();
}

//! Puts the shared renderer's text AA mode back when a case ends.
struct RestoreTextAA
{
    RestoreTextAA(): aa(gweni::test::gpuRenderer().textAA()) {}
    ~RestoreTextAA() { gweni::test::gpuRenderer().setTextAA(aa); }

    gweni::renderer::TextAA aa;
};

}//namespace

TEST_CASE("Freeing a font with a run laid out in it keeps its atlas, the run drawing, until the run leaves it")
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    const Font font=makeFont(15);

    if(!ownAtlas(font))
        return;

    RestoreTextAA restore;

    for(gweni::renderer::TextAA aa:{ gweni::renderer::TextAA::Grayscale, gweni::renderer::TextAA::LcdRGB })
    {
        for(Leave leave:{ Leave::Released, Leave::OtherFont })
        {
            CAPTURE(static_cast<int>(aa));
            CAPTURE(static_cast<int>(leave));
            renderer.setTextAA(aa);
            freeWhileARunUsesIt(font, leave);
        }
    }
}

TEST_CASE("Load, free and load the same face again: the same text draws the same pixels")
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    const Font font=makeFont(12);

    if(!ownAtlas(font))
        return;
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
