/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A renderer releases what it owns: freeFont destroys the font's atlas texture
 *  (SlotRenderer::textureExists) and its freetype-gl manager, loading a font
 *  twice makes nothing new and takes two frees to go, a font with a run laid out
 *  in it stays (the run drawing) until that run leaves it, and deleting a
 *  renderer leaves another renderer on the same device and target drawing.
 *
 *  The skin and Label hold the fonts they free: a skin set to test::openSans()'s
 *  font gives back only its own load when it is deleted, so the fixture keeps
 *  its atlas; deleting one of two Labels set to one font, or a Label set to the
 *  font other Texts draw in, leaves the others drawing the same pixels from the
 *  same atlas with no glyph rewritten, and the last Label to go takes its font.
 *
 *  Every font here is a fresh Font (another size than test::openSans()), so
 *  freeing it leaves the shared fixture loaded. A case skips itself when
 *  GWENI_SDF_TEXT_MIN puts its font and the fixture in the face's one SDF atlas.
 *
 *  On GL it also checks which atlas the text draw binds, and the case
 *  "Deleting a renderer deletes its GL objects ..." is in the test suite
 *  "gl_only": it checks the GL objects themselves (glIsTexture, glIsBuffer,
 *  getGLObjectNames), which only GL has. On Vulkan a freed atlas's image is
 *  destroyed once no frame in flight samples it, and the validation layer,
 *  which every Vulkan run has, aborts on any use of it after that.
 */
#include <doctest/doctest.h>

#include "glRenderer.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/label.h>
#include <gweni/platforms/commonPlatform.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <vector>

namespace
{

using gweni::Color;
using gweni::Font;
using gweni::Point;
using gweni::String;
using gweni::renderer::OpenGLCore;
using gweni::renderer::SlotRenderer;

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
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    if((renderer.getTextMode(font) == gweni::renderer::TextMode::Coverage)
        || (renderer.getTextMode(gweni::test::openSans()) == gweni::renderer::TextMode::Coverage))
        return true;
    MESSAGE("skipped: GWENI_SDF_TEXT_MIN puts it in the shared fixture's SDF atlas");
    return false;
}

//! The offscreen target, top-down RGBA8.
std::vector<uint8_t> readFrame()
{
    std::vector<uint8_t> pixels;

    gweni::test::readView(pixels);
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
    gweni::test::clearView(background);
}

//! Draws `text` alone, white at (8, 40), in one frame of `renderer`, and reads it back.
std::vector<uint8_t> drawText(SlotRenderer &renderer, const Font &font, const String &text)
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

//! The shared renderer if it is OpenGLCore, for the checks only GL can make
OpenGLCore *glOrNull()
{
    return dynamic_cast<OpenGLCore *>(&gweni::test::gpuRenderer());
}

//! The 2D texture bound to unit 0, on GL
GLuint boundTexture2D()
{
    GLint name=0;

    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &name);
    return (GLuint)name;
}

//! The pixels of `rect` (device px, the canvas at scale 1) in a frame readFrame() read.
std::vector<uint8_t> region(const std::vector<uint8_t> &pixels, const gweni::Rect &rect)
{
    const Point size=gweni::test::viewSize();
    std::vector<uint8_t> out;

    for(int y=rect.y; y < rect.y+rect.h; ++y)
    {
        const size_t row=(static_cast<size_t>(y)*size.x+rect.x)*4;

        out.insert(out.end(), pixels.begin()+row, pixels.begin()+row+static_cast<size_t>(rect.w)*4);
    }
    return out;
}

//! A textured skin set to test::openSans()'s font and a canvas on the shared
//! renderer; deleting it deletes the canvas, then the skin.
struct SkinScene
{
    SkinScene()
    {
        const Point viewSize=gweni::test::viewSize();

        skin=new gweni::skin::textured::TexturedSkin(&gweni::test::gpuRenderer());
        REQUIRE(skin->init("DefaultSkin.json"));
        skin->setDefaultFont("OpenSans.ttf", 11);

        canvas=new gweni::controls::Canvas(skin);
        canvas->setSize(viewSize.x, viewSize.y);
    }

    ~SkinScene()
    {
        delete canvas;
        delete skin;

        SlotRenderer &renderer=gweni::test::gpuRenderer();

        renderer.begin();
        renderer.end();
    }

    gweni::controls::Label *addLabel(const String &text, int y)
    {
        gweni::controls::Label *label=canvas->newChild<gweni::controls::Label>();

        label->setText(text);
        label->setBounds(10, y, 200, 24);
        return label;
    }

    //! One frame, read back.
    std::vector<uint8_t> frame()
    {
        clearFrame();
        canvas->updateCanvas();
        canvas->renderCanvas();
        return readFrame();
    }

    gweni::skin::textured::TexturedSkin *skin=nullptr;
    gweni::controls::Canvas *canvas=nullptr;
};

}//namespace

TEST_CASE("freeFont deletes the font's atlas texture")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const Font font=makeFont(13);

    if(!ownAtlas(font))
        return;
    const size_t fontsBefore=renderer.getLoadedFontCount();

    REQUIRE((renderer.loadFont(font) == Font::Status::Loaded));

    const unsigned int atlas=renderer.getFontAtlasHandle(font);

    REQUIRE(atlas != 0);
    CHECK(renderer.textureExists(atlas));
    CHECK(renderer.getLoadedFontCount() == fontsBefore+1);

    renderer.freeFont(font);

    CHECK_FALSE(renderer.textureExists(atlas));
    CHECK(renderer.getFontAtlasHandle(font) == 0);
    CHECK(renderer.getLoadedFontCount() == fontsBefore);

    // freeing it again, or a font never loaded, does nothing
    renderer.freeFont(font);
    renderer.freeFont(makeFont(17));
    CHECK(renderer.getLoadedFontCount() == fontsBefore);
    CHECK(renderer.textureExists(renderer.getFontAtlasHandle(gweni::test::openSans())));
}

TEST_CASE("loadFont of a font already loaded returns Loaded, makes nothing new, and takes a second freeFont")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const Font font=makeFont(13);

    if(!ownAtlas(font))
        return;
    const size_t fontsBefore=renderer.getLoadedFontCount();

    REQUIRE((renderer.loadFont(font) == Font::Status::Loaded));

    const unsigned int atlas=renderer.getFontAtlasHandle(font);
    const unsigned int sharedAtlas=renderer.getFontAtlasHandle(gweni::test::openSans());

    // on GL a load that makes an atlas binds it (and freeing a duplicate would
    // leave 0 bound), so an unchanged binding shows no texture was made
    if(glOrNull())
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sharedAtlas);
    }

    CHECK((renderer.loadFont(font) == Font::Status::Loaded));
    if(glOrNull())
        CHECK(boundTexture2D() == sharedAtlas);
    CHECK(renderer.getFontAtlasHandle(font) == atlas);
    CHECK(renderer.getLoadedFontCount() == fontsBefore+1);
    // still usable through the entry it returned
    CHECK(renderer.measureText(font, "Hello").x > 0);

    // two owners: the first free leaves it to the other
    renderer.freeFont(font);
    CHECK(renderer.getFontAtlasHandle(font) == atlas);
    CHECK(renderer.textureExists(atlas));
    CHECK(renderer.getLoadedFontCount() == fontsBefore+1);

    // the second, with no run laid out in it, deletes it
    renderer.freeFont(font);
    CHECK_FALSE(renderer.textureExists(atlas));
    CHECK(renderer.getFontAtlasHandle(font) == 0);
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
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const size_t fontsBefore=renderer.getLoadedFontCount();

    REQUIRE((renderer.loadFont(font) == Font::Status::Loaded));

    const unsigned int atlas=renderer.getFontAtlasHandle(font);
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
    if(glOrNull())
        CHECK(boundTexture2D() == atlas);

    // its one owner frees it, and the run holds it
    renderer.deviceError();
    renderer.freeFont(font);
    CHECK(renderer.getFontAtlasHandle(font) == atlas);
    CHECK(renderer.textureExists(atlas));
    CHECK(renderer.getLoadedFontCount() == fontsBefore+1);

    // a frame with no new text draws the run as it was, from the same atlas
    clearFrame();
    renderer.begin();
    renderer.end();
    CHECK_FALSE(renderer.deviceError());
    if(glOrNull())
        CHECK(boundTexture2D() == atlas);
    CHECK(readFrame() == drawn);
    CHECK(renderer.getFrameStats().atlasUploadCalls == 0);

    // a freeFont with no load to match it is deferred the same way
    renderer.freeFont(font);
    CHECK(renderer.getFontAtlasHandle(font) == atlas);
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

    CHECK(renderer.getFontAtlasHandle(font) == 0);
    CHECK_FALSE(renderer.textureExists(atlas));
    CHECK(renderer.getLoadedFontCount() == fontsBefore);

    // and nothing is drawn with the deleted atlas, or with whatever took its name
    clearFrame();
    renderer.begin();
    renderer.end();
    CHECK_FALSE(renderer.deviceError());
    if(glOrNull())
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
    SlotRenderer &renderer=gweni::test::gpuRenderer();
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
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const Font font=makeFont(12);

    if(!ownAtlas(font))
        return;
    const String text="Hello gweni 0123";

    REQUIRE((renderer.loadFont(font) == Font::Status::Loaded));

    const std::vector<uint8_t> first=drawText(renderer, font, text);

    renderer.freeFont(font);
    REQUIRE(renderer.getFontAtlasHandle(font) == 0);
    REQUIRE((renderer.loadFont(font) == Font::Status::Loaded));

    const std::vector<uint8_t> second=drawText(renderer, font, text);

    CHECK(drawnPixels(first) > 20);
    CHECK(first == second);

    renderer.freeFont(font);
}

TEST_CASE("A skin set to test::openSans()'s font gives back only its own load: the fixture keeps its atlas")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const Font &shared=gweni::test::openSans();
    const unsigned int atlas=renderer.getFontAtlasHandle(shared);
    const String text="Shared font 0123";

    REQUIRE(atlas != 0);

    const std::vector<uint8_t> before=drawText(renderer, shared, text);

    REQUIRE(drawnPixels(before) > 20);

    {
        SkinScene scene;
        gweni::controls::Label *label=scene.addLabel("In the skin's font", 10);

        scene.frame();

        const std::vector<uint8_t> pixels=scene.frame();

        CHECK((label->getFont() == shared));
        CHECK(drawnPixels(region(pixels, label->getBounds())) > 20);
        // the skin's font is the fixture's entry, not one of its own
        CHECK(renderer.getFontAtlasHandle(shared) == atlas);
    }

    CHECK(renderer.getFontAtlasHandle(shared) == atlas);
    CHECK(renderer.textureExists(atlas));
    CHECK(drawText(renderer, shared, text) == before);
    CHECK(renderer.getFrameStats().atlasUploadCalls == 0);
}

TEST_CASE("Deleting one of two Labels set to one font leaves the other drawing, and the last takes the font")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const Font font=makeFont(13);

    if(!ownAtlas(font))
        return;

    SkinScene scene;
    const size_t fontsBefore=renderer.getLoadedFontCount();
    gweni::controls::Label *first=scene.addLabel("First label", 10);
    gweni::controls::Label *second=scene.addLabel("Second label", 40);

    first->setFont(font.facename, static_cast<int>(font.size), false);
    second->setFont(font.facename, static_cast<int>(font.size), false);
    scene.frame();

    const std::vector<uint8_t> drawn=scene.frame();
    const unsigned int atlas=renderer.getFontAtlasHandle(font);
    const gweni::Rect firstBounds=first->getBounds();
    const std::vector<uint8_t> secondPixels=region(drawn, second->getBounds());

    REQUIRE(atlas != 0);
    REQUIRE(drawnPixels(region(drawn, firstBounds)) > 20);
    REQUIRE(drawnPixels(secondPixels) > 20);

    first->delayedDelete();

    const std::vector<uint8_t> afterDelete=scene.frame();

    CHECK(renderer.getFontAtlasHandle(font) == atlas);
    CHECK(renderer.getFrameStats().atlasUploadCalls == 0);
    CHECK(drawnPixels(region(afterDelete, firstBounds)) == 0);
    CHECK(region(afterDelete, second->getBounds()) == secondPixels);

    for(int frame=0; frame < 2; ++frame)
    {
        CAPTURE(frame);

        const std::vector<uint8_t> later=scene.frame();

        CHECK(renderer.getFrameStats().textSlotsWritten == 0);
        CHECK(renderer.getFontAtlasHandle(font) == atlas);
        CHECK(region(later, second->getBounds()) == secondPixels);
    }

    // its load and the first Label's were each given back once
    second->delayedDelete();
    scene.frame();
    CHECK(renderer.getFontAtlasHandle(font) == 0);
    CHECK_FALSE(renderer.textureExists(atlas));
    CHECK(renderer.getLoadedFontCount() == fontsBefore);
}

TEST_CASE("Deleting a Label set to the skin's default font leaves the Texts drawn in it drawing")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const Font &shared=gweni::test::openSans();
    const unsigned int atlas=renderer.getFontAtlasHandle(shared);

    REQUIRE(atlas != 0);

    {
        SkinScene scene;
        gweni::controls::Label *set=scene.addLabel("Set to the default", 10);
        gweni::controls::Label *other=scene.addLabel("In the default", 40);

        set->setFont(shared.facename, static_cast<int>(shared.size), false);
        scene.frame();

        const std::vector<uint8_t> drawn=scene.frame();
        const std::vector<uint8_t> otherPixels=region(drawn, other->getBounds());

        REQUIRE((other->getFont() == shared));
        REQUIRE(drawnPixels(region(drawn, set->getBounds())) > 20);
        REQUIRE(drawnPixels(otherPixels) > 20);

        set->delayedDelete();

        const std::vector<uint8_t> afterDelete=scene.frame();

        CHECK(renderer.getFontAtlasHandle(shared) == atlas);
        CHECK(renderer.getFrameStats().atlasUploadCalls == 0);
        CHECK(region(afterDelete, other->getBounds()) == otherPixels);

        for(int frame=0; frame < 2; ++frame)
        {
            CAPTURE(frame);

            const std::vector<uint8_t> later=scene.frame();

            CHECK(renderer.getFrameStats().textSlotsWritten == 0);
            CHECK(region(later, other->getBounds()) == otherPixels);
        }
    }

    // the Label, the skin and the fixture each gave back only their own load
    CHECK(renderer.getFontAtlasHandle(shared) == atlas);
    CHECK(renderer.textureExists(atlas));
}

TEST_CASE("Deleting a renderer leaves another renderer drawing")
{
    SlotRenderer &shared=gweni::test::gpuRenderer();
    const unsigned int sharedAtlas=shared.getFontAtlasHandle(gweni::test::openSans());
    const std::vector<uint8_t> before=drawText(shared, gweni::test::openSans(), "Hello");

    REQUIRE(sharedAtlas != 0);
    REQUIRE(drawnPixels(before) > 20);

    gweni::platform::CurrentPath paths("resources");
    SlotRenderer *renderer=gweni::test::newRenderer(paths, gweni::Rect(Point(0, 0), gweni::test::viewSize()));

    renderer->init();

    const Font font=makeFont(14);
    gweni::Texture texture;

    texture.name="test16.png";
    REQUIRE((renderer->loadFont(font) == Font::Status::Loaded));
    REQUIRE((renderer->loadTexture(texture) == gweni::Texture::Status::Loaded));

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
    CHECK(renderer->textureExists(renderer->getFontAtlasHandle(font)));
    CHECK(renderer->textureExists(renderer->getTextureHandle(texture)));

    // the ids come from the process-wide banks, not the renderer
    renderer->releasePrimitive(primitiveId);
    renderer->releaseTextPrimitive(textId);
    delete renderer;

    // the shared renderer is untouched and draws as it did
    CHECK(shared.textureExists(sharedAtlas));
    CHECK(drawText(shared, gweni::test::openSans(), "Hello") == before);
    CHECK_FALSE(shared.deviceError());
}

// gl_only: the GL objects are GL's (see the header)
TEST_CASE("Deleting a renderer deletes its GL objects and leaves another renderer drawing"
    * doctest::test_suite("gl_only"))
{
    OpenGLCore &shared=gweni::test::glRenderer();
    const unsigned int sharedAtlas=shared.getFontAtlasHandle(gweni::test::openSans());

    REQUIRE(sharedAtlas != 0);

    gweni::platform::CurrentPath paths("resources");
    OpenGLCore *renderer=new OpenGLCore(paths, gweni::Rect(Point(0, 0), gweni::test::viewSize()));

    renderer->init();

    const Font font=makeFont(14);
    gweni::Texture texture;

    texture.name="test16.png";
    REQUIRE((renderer->loadFont(font) == Font::Status::Loaded));
    REQUIRE((renderer->loadTexture(texture) == gweni::Texture::Status::Loaded));

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

    const unsigned int atlas=renderer->getFontAtlasHandle(font);
    const unsigned int textureName=renderer->getTextureHandle(texture);
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

    shared.deviceError();
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
