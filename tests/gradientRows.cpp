/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Style 4's vertical gradient (drawRoundedRect) is an exact 8-bit level per
 *  row, the same on every backend: row r (counted from the top colour's edge)
 *  of an H-device-px rect has, in each channel, alpha included,
 *
 *      floor((top*2H + (bottom-top)*(2r+1) + H) / (2H))
 *
 *  the row centre's exact value, (2r+1)/(2H) of the way from top to bottom,
 *  rounded half up. A float gradient rounded by the framebuffer's UNORM
 *  conversion lands exactly on x.5 of a level on some rows (odd difference and
 *  odd H on the middle row, difference 3 at H 3 or 16 at H 16 on every row), and
 *  there one ulp decides the level: llvmpipe and lavapipe rounded such rows of
 *  #289's ButtonNormal (#3D59A1 -> #2C3D6B, 15 units high) a level apart, and
 *  backend_pixels failed (#492).
 *
 *  The rects are radius 0 with no border, so every pixel of them is fully
 *  covered; the leftmost and rightmost columns of each are left out, away from
 *  the edge. They cover odd and even differences, both directions, the colours
 *  of #289, every height from 1 to 40 and some larger ones, at many y positions
 *  (a rect's place moves the interpolation's float error, not the level), as
 *  narrow rects and as rects as wide as the view, at scales 1 and 1.5. The
 *  report counts the rows with a tie and splits the wrong values into ties and
 *  the rest: the float gradient only ever missed a tie, by 1. Opaque pairs are
 *  drawn over white; an alpha gradient is white over black, so the blend
 *  (1*a + 0*(1-a)) gives each channel the alpha level itself, with no blend
 *  rounding of its own.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Point;
using gweni::Rect;
using gweni::renderer::SlotRenderer;

//! One gradient, top colour to bottom, and what it is drawn over.
struct Gradient
{
    const char *name;
    Color top;
    Color bottom;
    //! drawn white with these alphas over black, and read back as the alpha
    bool alpha;
};

const Color white(255, 255, 255, 255);
const Color black(0, 0, 0, 255);

const Gradient gradients[]=
{
    {"#289 ButtonNormal", Color(0x3d, 0x59, 0xa1, 255), Color(0x2c, 0x3d, 0x6b, 255), false},
    {"#289 reversed", Color(0x2c, 0x3d, 0x6b, 255), Color(0x3d, 0x59, 0xa1, 255), false},
    {"differences 3, 16, 1", Color(10, 20, 30, 255), Color(13, 36, 31, 255), false},
    {"differences 15, 17, -15", Color(100, 100, 100, 255), Color(115, 117, 85, 255), false},
    {"differences 255, -255, 2", Color(0, 255, 7, 255), Color(255, 0, 9, 255), false},
    {"alpha difference 17", Color(255, 255, 255, 40), Color(255, 255, 255, 57), true},
    {"alpha difference -3", Color(255, 255, 255, 203), Color(255, 255, 255, 200), true},
    {"alpha difference 16", Color(255, 255, 255, 100), Color(255, 255, 255, 116), true},
};

const float scales[]={1.0f, 1.5f};

//! The level of one channel at row r of an h-row gradient, round half up.
int expectedLevel(int top, int bottom, int r, int h)
{
    return (top*2*h+(bottom-top)*(2*r+1)+h)/(2*h);
}

//! Whether that level's exact value is a tie, x.5 of a level.
bool isTie(int top, int bottom, int r, int h)
{
    const int twice=((bottom-top)*(2*r+1))%(2*h);

    return twice == h || twice == -h;
}

const int narrowWidth=6;
const int narrowStride=7;

//! The frames to draw, each a list of rects in logical units that fit the view at
//! `scale`: every height from 1 to 40 and some larger ones, each at several y
//! positions, as narrow rects side by side; and rects as wide as the view (a long
//! row of a surface, where the interpolation runs across the whole quad) of a few
//! heights, stacked one unit apart, at every y.
std::vector<std::vector<Rect>> frames(float scale)
{
    const Point view=gweni::test::viewSize();
    const int viewWidth=static_cast<int>(view.x/scale);
    const int viewHeight=static_cast<int>(view.y/scale);
    std::vector<std::vector<Rect>> result;

    std::vector<int> heights;

    for(int h=1; h <= 40; ++h)
        heights.push_back(h);
    for(int h:{45, 47, 48, 63, 64, 65, 85, 99, 100})
        heights.push_back(h);

    const int ys[]={0, 1, 2, 3, 5, 7, 8, 11, 13, 16, 19, 23, 31, 37, 50, 61};
    const size_t perFrame=static_cast<size_t>(viewWidth/narrowStride);
    std::vector<Rect> narrow;

    for(int h:heights)
    {
        for(int y:ys)
        {
            if(y+h > viewHeight)
                continue;
            if(narrow.size() == perFrame)
            {
                result.push_back(narrow);
                narrow.clear();
            }
            narrow.push_back(Rect(static_cast<int>(narrow.size())*narrowStride, y, narrowWidth, h));
        }
    }
    result.push_back(narrow);

    for(int h:{1, 2, 3, 5, 15, 16, 17, 31})
    {
        for(int start=0; start <= h; ++start)
        {
            std::vector<Rect> wide;

            for(int y=start; y+h <= viewHeight; y+=h+1)
                wide.push_back(Rect(0, y, viewWidth, h));
            result.push_back(wide);
        }
    }
    return result;
}

}//namespace

TEST_CASE("Gradient rows: style 4's level on each row is the exact one, rounded half up")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const Point view=gweni::test::viewSize();

    for(float scale:scales)
    {
        CAPTURE(scale);
        renderer.setScale(scale);

        const std::vector<std::vector<Rect>> all=frames(scale);

        for(const Gradient &gradient:gradients)
        {
            const std::string gradientName=gradient.name;

            CAPTURE(gradientName);

            int rowsChecked=0;
            int rowsWrong=0;
            int tieRows=0;
            int wrongTieValues=0;
            int wrongOtherValues=0;
            int maxDelta=0;
            std::ostringstream report;

            for(const std::vector<Rect> &rects:all)
            {
                const size_t count=rects.size();
                std::vector<size_t> ids(count);

                for(size_t &id:ids)
                    renderer.generatePrimitive(id);

                gweni::test::clearView(gradient.alpha?black:white);
                renderer.begin();
                for(size_t i=0; i < count; ++i)
                {
                    renderer.drawRoundedRect(ids[i], rects[i], 0.0f, 0.0f, gradient.top, gradient.bottom,
                        black, 1);
                }
                renderer.end();

                std::vector<uint8_t> pixels;

                gweni::test::readView(pixels);

                for(size_t i=0; i < count; ++i)
                {
                    const Rect &p=rects[i];
                    const int left=SlotRenderer::snap(float(p.x), scale);
                    const int right=SlotRenderer::snap(float(p.x+p.w), scale);
                    const int top=SlotRenderer::snap(float(p.y), scale);
                    const int bottom=SlotRenderer::snap(float(p.y+p.h), scale);
                    const int h=bottom-top;

                    for(int r=0; r < h; ++r)
                    {
                        // each channel's top and bottom: an alpha gradient's alpha in all three
                        int ends[3][2];

                        for(int c=0; c < 3; ++c)
                        {
                            const Color *from[2]={&gradient.top, &gradient.bottom};

                            for(int e=0; e < 2; ++e)
                            {
                                const Color &color=*from[e];

                                ends[c][e]=gradient.alpha?color.a:(c == 0?color.r:c == 1?color.g:color.b);
                            }
                        }

                        bool rowTie=false;
                        bool rowWrong=false;

                        for(int c=0; c < 3; ++c)
                            rowTie=rowTie || isTie(ends[c][0], ends[c][1], r, h);

                        for(int px=left+1; px < right-1; ++px)
                        {
                            const uint8_t *got=&pixels[(static_cast<size_t>(top+r)*view.x+px)*4];

                            for(int c=0; c < 3; ++c)
                            {
                                const int expected=expectedLevel(ends[c][0], ends[c][1], r, h);
                                const int delta=int(got[c])-expected;

                                if(delta == 0)
                                    continue;
                                maxDelta=std::max(maxDelta, std::abs(delta));
                                if(isTie(ends[c][0], ends[c][1], r, h))
                                    ++wrongTieValues;
                                else
                                    ++wrongOtherValues;
                                if(!rowWrong && rowsWrong < 12)
                                {
                                    report<<"  x "<<p.x<<" y "<<p.y<<" w "<<p.w<<" h "<<p.h<<" (device rows "<<top<<".."<<bottom
                                        <<", H "<<h<<") row "<<r<<" x "<<px<<" channel "<<c<<": "
                                        <<int(got[c])<<", expected "<<expected
                                        <<(isTie(ends[c][0], ends[c][1], r, h)?" (a tie)":"")<<"\n";
                                }
                                rowWrong=true;
                            }
                        }
                        ++rowsChecked;
                        if(rowTie)
                            ++tieRows;
                        if(rowWrong)
                            ++rowsWrong;
                    }
                }

                for(size_t id:ids)
                    renderer.releasePrimitive(id);
            }

            INFO("rows checked: "<<rowsChecked<<" ("<<tieRows<<" with a tie), wrong: "<<rowsWrong
                <<"; channel values wrong on a tie: "<<wrongTieValues<<", elsewhere: "<<wrongOtherValues
                <<", by at most "<<maxDelta<<"\n"<<report.str());
            CHECK(rowsChecked > 0);
            CHECK(rowsWrong == 0);
        }
    }

    // leave the renderer as the next case expects: 1x, every slot released
    renderer.setScale(1.0f);
    renderer.begin();
    renderer.end();
}
