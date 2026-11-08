/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  renderer::imageMipChain, the mip levels every backend uploads for an image
 *  (style 2) instead of its API's mipmap generation, so the browser and native
 *  sample the same bytes: level n is max(1, size >> n) on each axis down to 1x1,
 *  each texel the 2x2 box of the level above with every channel (a+b+c+d+2)/4,
 *  alpha straight; a 1-texel side takes its texel twice, and an odd side's last
 *  column or row is in no box. Built only with the OpenGL core renderer, which
 *  SlotRenderer is part of.
 */
#if __has_include(<freetype-gl/freetype-gl.h>)

#include <doctest/doctest.h>

#include <gweni/platforms/renderers/slotRenderer.h>

#include <cstdint>
#include <vector>

using gweni::renderer::ImageMipLevel;
using gweni::renderer::imageMipChain;

TEST_CASE("imageMipChain: the levels are GL's sizes down to 1x1")
{
    const std::vector<uint8_t> image(896*7*3, 0);
    const std::vector<ImageMipLevel> levels=imageMipChain(image.data(), 896, 7, 3);
    const int widths[]={ 448, 224, 112, 56, 28, 14, 7, 3, 1 };
    const int heights[]={ 3, 1, 1, 1, 1, 1, 1, 1, 1 };

    REQUIRE(levels.size() == 9);
    for(size_t i=0; i<levels.size(); ++i)
    {
        INFO("level ", i+1);
        CHECK(levels[i].width == widths[i]);
        CHECK(levels[i].height == heights[i]);
        CHECK(levels[i].texels.size() == (size_t)widths[i]*heights[i]*3);
    }
    CHECK(imageMipChain(image.data(), 1, 1, 3).empty());
}

TEST_CASE("imageMipChain: a 2x2 box per texel, rounded to nearest, alpha straight")
{
    // 4x2 RGBA: two boxes, the first's channels summing to 2 (0.5, rounds up),
    // 1 (0.25, rounds down), 6 (1.5, rounds up) and 1020 (255)
    const std::vector<uint8_t> image=
    {
        0, 1, 0, 255,   1, 0, 0, 255,   10, 20, 30, 0,   10, 20, 30, 0,
        1, 0, 3, 255,   0, 0, 3, 255,   10, 20, 30, 0,   14, 20, 30, 0,
    };
    const std::vector<ImageMipLevel> levels=imageMipChain(image.data(), 4, 2, 4);

    REQUIRE(levels.size() == 2);
    REQUIRE(levels[0].width == 2);
    REQUIRE(levels[0].height == 1);
    CHECK(levels[0].texels == std::vector<uint8_t>{ 1, 0, 2, 255,  11, 20, 30, 0 });
    // the 1x1 level averages that row's two texels, the 1-texel column taken twice
    CHECK(levels[1].texels == std::vector<uint8_t>{ 6, 10, 16, 128 });
}

TEST_CASE("imageMipChain: an odd side's last column is in no box")
{
    // 3x1 RGB: the level is 1x1, the box (0..1, 0) twice, texel 2 left out
    const std::vector<uint8_t> image={ 100, 0, 0,   200, 0, 0,   0, 0, 255 };
    const std::vector<ImageMipLevel> levels=imageMipChain(image.data(), 3, 1, 3);

    REQUIRE(levels.size() == 1);
    CHECK(levels[0].texels == std::vector<uint8_t>{ 150, 0, 0 });
}

#endif
