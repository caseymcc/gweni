/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Reloading a Texture that loaded before, when the new load fails, must leave
 *  it unloaded (dataIndex max). If it kept its old slot index, drawing it bound
 *  a texture the slot never made, and freeing it pushed the slot onto the free
 *  list a second time and destroyed that texture handle, which could destroy a
 *  live texture (the skin or font atlas).
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <limits>

namespace
{

constexpr size_t unloaded=std::numeric_limits<size_t>::max();

}//namespace

TEST_CASE("SlotTextureData and TextureData start zeroed")
{
    gweni::renderer::SlotTextureData slotData;
    gweni::TextureData data;

    CHECK(slotData.m_handle == 0);
    CHECK(slotData.m_index == unloaded);
    CHECK(slotData.readable == false);
    CHECK(data.readable == false);
}

TEST_CASE("A failed texture reload leaves the texture unloaded and the other textures alive")
{
    gweni::renderer::SlotRenderer &renderer=gweni::test::gpuRenderer();

    // stands in for the skin atlas, which main.cpp does not load
    gweni::Texture sentinel;
    sentinel.name="DefaultSkin.png";
    REQUIRE(renderer.loadTexture(sentinel) == gweni::Texture::Status::Loaded);

    const gweni::renderer::TextureHandle sentinelName=renderer.getTextureHandle(sentinel);
    const gweni::renderer::TextureHandle fontAtlasName=renderer.getFontAtlasHandle(gweni::test::openSans());

    REQUIRE(sentinelName != 0);
    REQUIRE(fontAtlasName != 0);

    gweni::Texture t;
    t.name="test16.png";
    REQUIRE(renderer.loadTexture(t) == gweni::Texture::Status::Loaded);
    REQUIRE(t.dataIndex != unloaded);

    t.name="no-such-texture.png";
    CHECK(renderer.loadTexture(t) == gweni::Texture::Status::ErrorFileNotFound);
    CHECK(t.dataIndex == unloaded);
    CHECK(renderer.getTextureHandle(t) == 0);
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

    CHECK(renderer.textureExists(sentinelName));
    CHECK(renderer.textureExists(fontAtlasName));
    CHECK(renderer.getTextureHandle(sentinel) == sentinelName);

    renderer.freeTexture(b);
    renderer.freeTexture(a);
    renderer.freeTexture(sentinel);
}

TEST_CASE("A texture that never loaded stays unloaded when its load fails")
{
    gweni::renderer::SlotRenderer &renderer=gweni::test::gpuRenderer();

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
