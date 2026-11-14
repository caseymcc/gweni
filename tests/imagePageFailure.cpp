/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  OpenGLCore::allocateImagePage keeps its contract when GL refuses the new
 *  page: asked for more layers than the context has (GL_INVALID_VALUE, as an
 *  out-of-memory GL_OUT_OF_MEMORY is), it returns false, deletes the page it
 *  made and keeps the old one, so an image already in that page still draws
 *  its texels, a later image still loads into it, and no GL error is left
 *  behind. gl_only: the GL error path is GL's (Vulkan's is its own).
 */
#include <doctest/doctest.h>

#include "glRenderer.h"

#include <gweni/platforms/commonPlatform.h>

#include <cstdint>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Point;
using gweni::Rect;
using gweni::Texture;
using gweni::renderer::OpenGLCore;

const Color background(0, 0, 255, 255);

//! OpenGLCore with its page allocation callable from the test.
class PageProbe: public OpenGLCore
{
public:
    using OpenGLCore::OpenGLCore;
    using OpenGLCore::allocateImagePage;
    using OpenGLCore::maxImageLayers;
};

//! A width x height RGBA8 image of one colour.
std::vector<uint8_t> solid(const Color &color, int width, int height)
{
    std::vector<uint8_t> pixels;

    for(int i=0; i<width*height; ++i)
        pixels.insert(pixels.end(), { color.r, color.g, color.b, color.a });
    return pixels;
}

//! The colour at the centre of rect after one frame drawing texture there.
Color drawn(OpenGLCore &renderer, const Texture &texture, size_t id, const Rect &rect)
{
    gweni::test::clearView(background);
    renderer.begin();
    renderer.setDrawColor(Color(255, 255, 255, 255));
    renderer.drawTexturedRect(texture, id, rect, 1);
    renderer.end();

    std::vector<uint8_t> pixels;

    gweni::test::readView(pixels);

    const int x=rect.x+rect.w/2;
    const int y=rect.y+rect.h/2;
    const uint8_t *p=&pixels[((size_t)y*gweni::test::viewSize().x+x)*4];

    return Color(p[0], p[1], p[2], p[3]);
}

}//namespace

TEST_CASE("Image pages: a page GL refuses returns false and keeps the old page and its images"
    * doctest::test_suite("gl_only"))
{
    gweni::platform::CurrentPath paths("resources");
    PageProbe renderer(paths, Rect(Point(0, 0), gweni::test::viewSize()));

    renderer.init();

    const Color red(200, 40, 30, 255);
    const Color green(30, 180, 60, 255);
    const std::vector<uint8_t> redPixels=solid(red, 16, 16);
    const std::vector<uint8_t> greenPixels=solid(green, 16, 16);
    Texture first;
    Texture second;
    size_t id=0;

    REQUIRE((renderer.loadTexturePixels(first, 16, 16, 16*4, redPixels.data()) == Texture::Status::Loaded));
    renderer.generatePrimitive(id);
    REQUIRE(drawn(renderer, first, id, Rect(8, 8, 32, 32)) == red);

    const size_t layers=renderer.imagePageLayers(0);

    REQUIRE(layers > 0);
    // more layers than GL_MAX_ARRAY_TEXTURE_LAYERS: glTexImage3D fails
    CHECK_FALSE(renderer.allocateImagePage(0, renderer.maxImageLayers()+1, 1));
    CHECK_FALSE(renderer.deviceError());
    CHECK(renderer.imagePageLayers(0) == layers);
    // the old page is still the one sampled, its layer intact
    CHECK(drawn(renderer, first, id, Rect(8, 8, 32, 32)) == red);
    // and still takes images
    REQUIRE((renderer.loadTexturePixels(second, 16, 16, 16*4, greenPixels.data()) == Texture::Status::Loaded));
    CHECK(drawn(renderer, second, id, Rect(8, 8, 32, 32)) == green);
    CHECK(drawn(renderer, first, id, Rect(8, 8, 32, 32)) == red);

    renderer.releasePrimitive(id);
    renderer.freeTexture(first);
    renderer.freeTexture(second);
    renderer.begin();
    renderer.end();
    CHECK_FALSE(renderer.deviceError());
}
