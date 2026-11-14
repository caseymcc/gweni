/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  ImagePanel::placeImage's whole-unit rect, with no renderer: each edge of the
 *  placed image is rounded half up, the same way on either side of 0, so a None
 *  image (one image px a logical unit) and a Fit one whose centre falls on a
 *  half unit are their own width wherever their bounds start, bounds that start
 *  left of or above 0 and end right of or below it included, and keep the UVs
 *  as set (nothing is cut). A rounding that went half away from 0 drew such an
 *  image one unit wider than itself.
 */
#include <gweni/controls/imagePanel.h>

#include <doctest/doctest.h>

using namespace gweni;
using controls::ImagePanel;
using controls::ImageScaleMode;

TEST_CASE("ImagePanel placement: a None or Fit image is its own size wherever its bounds start, across 0 too")
{
    const float uv[4]={ 0.0f, 0.0f, 1.0f, 1.0f };

    for(ImageScaleMode mode : { ImageScaleMode::None, ImageScaleMode::Fit })
    {
        for(int spare : { 4, 5 })
        {
            for(int at=-20; at<=20; ++at)
            {
                CAPTURE(int(mode));
                CAPTURE(spare);
                CAPTURE(at);

                // a 10x10 image, the bounds 10+spare along x and y for None; for Fit
                // a 20x10 image in bounds 20+spare by 10, drawn at scale 1
                const bool none=(mode == ImageScaleMode::None);
                const Rect bounds=none?Rect(at, at, 10+spare, 10+spare):Rect(at, at, 20+spare, 10);
                const float width=none?10.0f:20.0f;
                Rect rect;
                float drawn[4];

                REQUIRE(ImagePanel::placeImage(mode, bounds, width, 10.0f, uv, rect, drawn));
                // the centre on a half unit (an odd spare) goes half up
                CHECK(rect.x == at+(spare+1)/2);
                CHECK(rect.w == (int)width);
                CHECK(rect.h == 10);
                if(none)
                    CHECK(rect.y == at+(spare+1)/2);
                CHECK(drawn[0] == uv[0]);
                CHECK(drawn[1] == uv[1]);
                CHECK(drawn[2] == uv[2]);
                CHECK(drawn[3] == uv[3]);
            }
        }
    }
}
