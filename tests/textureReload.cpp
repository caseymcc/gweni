/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Reloading a Texture that loaded before, when the new load fails, must leave
 *  it unloaded (dataIndex max). If it kept its old slot index, drawing it bound
 *  a GL name the slot never generated, and freeing it pushed the slot onto the
 *  free list a second time and called glDeleteTextures on that name, which
 *  could delete a live texture (the skin or font atlas).
 */
#include <doctest/doctest.h>

#include "capture.h"
#include "gpuRenderer.h"

#include <limits>

namespace
{

constexpr size_t unloaded=std::numeric_limits<size_t>::max();

}//namespace

TEST_CASE("GLTextureData and TextureData start zeroed")
{
    gweni::renderer::GLTextureData glData;
    gweni::TextureData data;

    CHECK(glData.m_textureId == 0);
    CHECK(glData.m_index == unloaded);
    CHECK(glData.readable == false);
    CHECK(data.readable == false);
}

TEST_CASE("A failed texture reload leaves the texture unloaded and the other textures alive")
{
    gweni::renderer::OpenGLCore &renderer=gweni::test::gpuRenderer();

    // stands in for the skin atlas, which main.cpp does not load
    gweni::Texture sentinel;
    sentinel.name="DefaultSkin.png";
    REQUIRE(renderer.loadTexture(sentinel) == gweni::Texture::Status::Loaded);

    const unsigned int sentinelName=renderer.getTextureName(sentinel);
    const unsigned int fontAtlasName=renderer.getFontAtlasName(gweni::test::openSans());

    REQUIRE(sentinelName != 0);
    REQUIRE(fontAtlasName != 0);

    gweni::Texture t;
    t.name="test16.png";
    REQUIRE(renderer.loadTexture(t) == gweni::Texture::Status::Loaded);
    REQUIRE(t.dataIndex != unloaded);

    t.name="no-such-texture.png";
    CHECK(renderer.loadTexture(t) == gweni::Texture::Status::ErrorFileNotFound);
    CHECK(t.dataIndex == unloaded);
    CHECK(renderer.getTextureName(t) == 0);
    // drawTexturedRect draws it as a missing image
    CHECK_FALSE(renderer.ensureTexture(t));

    // freeing a failed texture does nothing, however often
    renderer.freeTexture(t);
    renderer.freeTexture(t);

    // a slot on the free list twice would hand both of these the same index
    gweni::Texture a;
    gweni::Texture b;
    a.name="test16.png";
    b.name="gwen.png";
    REQUIRE(renderer.loadTexture(a) == gweni::Texture::Status::Loaded);
    REQUIRE(renderer.loadTexture(b) == gweni::Texture::Status::Loaded);
    CHECK(a.dataIndex != b.dataIndex);
    CHECK(a.dataIndex != sentinel.dataIndex);
    CHECK(b.dataIndex != sentinel.dataIndex);

    CHECK(glIsTexture(sentinelName) == GL_TRUE);
    CHECK(glIsTexture(fontAtlasName) == GL_TRUE);
    CHECK(renderer.getTextureName(sentinel) == sentinelName);

    renderer.freeTexture(b);
    renderer.freeTexture(a);
    renderer.freeTexture(sentinel);
}

TEST_CASE("A texture that never loaded stays unloaded when its load fails")
{
    gweni::renderer::OpenGLCore &renderer=gweni::test::gpuRenderer();

    gweni::Texture t;
    t.name="no-such-texture.png";
    CHECK(renderer.loadTexture(t) == gweni::Texture::Status::ErrorFileNotFound);
    CHECK(t.dataIndex == unloaded);
    CHECK(renderer.loadTexture(t) == gweni::Texture::Status::ErrorFileNotFound);
    CHECK(t.dataIndex == unloaded);
    renderer.freeTexture(t);

    gweni::Texture a;
    gweni::Texture b;
    a.name="test16.png";
    b.name="gwen.png";
    REQUIRE(renderer.loadTexture(a) == gweni::Texture::Status::Loaded);
    REQUIRE(renderer.loadTexture(b) == gweni::Texture::Status::Loaded);
    CHECK(a.dataIndex != b.dataIndex);

    renderer.freeTexture(b);
    renderer.freeTexture(a);
}
