/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/platforms/baseRender.h>
#include <gweni/utility.h>
#include <gweni/platforms/platform.h>

#include <algorithm>
#include <cmath>

namespace gweni
{
namespace renderer
{

Base::Base(ResourcePaths &paths)
    : m_scale(1.0f)
    , m_paths(paths)
    , m_renderOffset(gweni::Point(0, 0))
    , m_RTT(nullptr)
{}

Base::~Base()
{
    if(getCTT())
        getCTT()->shutDown();
}

void Base::drawLinedRect(size_t *primitiveIds, gweni::Rect rect, int zIndex)
{
    drawFilledRect(primitiveIds[0], gweni::Rect(rect.x, rect.y, rect.w, 1), zIndex);
    drawFilledRect(primitiveIds[1], gweni::Rect(rect.x, rect.y+rect.h-1, rect.w, 1), zIndex);
    drawFilledRect(primitiveIds[2], gweni::Rect(rect.x, rect.y, 1, rect.h), zIndex);
    drawFilledRect(primitiveIds[3], gweni::Rect(rect.x+rect.w-1, rect.y, 1, rect.h), zIndex);
}

void Base::drawPixel(size_t *primitiveIds, int x, int y, int zIndex)
{
    drawFilledRect(primitiveIds[0], gweni::Rect(x, y, 1, 1), zIndex);
}

void Base::drawShavedCornerRect(size_t *primitiveIds, gweni::Rect rect, int zIndex, bool bSlight)
{
    // Draw INSIDE the w/h.
    rect.w-=1;
    rect.h-=1;

    if(bSlight)
    {
        drawFilledRect(primitiveIds[0], gweni::Rect(rect.x+1, rect.y, rect.w-1, 1), zIndex);
        drawFilledRect(primitiveIds[1], gweni::Rect(rect.x+1, rect.y+rect.h, rect.w-1, 1), zIndex);
        drawFilledRect(primitiveIds[2], gweni::Rect(rect.x, rect.y+1, 1, rect.h-1), zIndex);
        drawFilledRect(primitiveIds[3], gweni::Rect(rect.x+rect.w, rect.y+1, 1, rect.h-1), zIndex);
    }
    else
    {
        drawPixel(&primitiveIds[0], rect.x+1, rect.y+1, zIndex);
        drawPixel(&primitiveIds[1], rect.x+rect.w-1, rect.y+1, zIndex);
        drawPixel(&primitiveIds[2], rect.x+1, rect.y+rect.h-1, zIndex);
        drawPixel(&primitiveIds[3], rect.x+rect.w-1, rect.y+rect.h-1, zIndex);
        drawFilledRect(primitiveIds[4], gweni::Rect(rect.x+2, rect.y, rect.w-3, 1), zIndex);
        drawFilledRect(primitiveIds[5], gweni::Rect(rect.x+2, rect.y+rect.h, rect.w-3, 1), zIndex);
        drawFilledRect(primitiveIds[6], gweni::Rect(rect.x, rect.y+2, 1, rect.h-3), zIndex);
        drawFilledRect(primitiveIds[7], gweni::Rect(rect.x+rect.w, rect.y+2, 1, rect.h-3), zIndex);
    }
}

int Base::snap(float value) const
{
    return snap(value, m_scale);
}

int Base::snap(float value, float scale)
{
    return static_cast<int>(std::floor(value*scale+0.5f));
}

void Base::translate(int &x, int &y)
{
    x=snap(float(x+m_renderOffset.x));
    y=snap(float(y+m_renderOffset.y));
}

// Both edges are snapped and the size is what lies between them, so a rect that
// ends where the next one starts shares its device-pixel edge at any scale.
void Base::translate(gweni::Rect &rect)
{
    const int x=rect.x+m_renderOffset.x;
    const int y=rect.y+m_renderOffset.y;
    const int x0=snap(float(x));
    const int y0=snap(float(y));

    rect.w=snap(float(x+rect.w))-x0;
    rect.h=snap(float(y+rect.h))-y0;
    rect.x=x0;
    rect.y=y0;
}

// The rect's size at the current render offset (its position is the offset,
// i.e. the control's absolute origin), intersected with the current region. No
// overlap gives an empty region at the overlap's corner, never a negative size.
void Base::addClipRegion(gweni::Rect rect)
{
    const gweni::Rect &clip=m_rectClipRegion;
    const int x0=std::max(m_renderOffset.x, clip.x);
    const int y0=std::max(m_renderOffset.y, clip.y);
    const int x1=std::min(m_renderOffset.x+rect.w, clip.x+clip.w);
    const int y1=std::min(m_renderOffset.y+rect.h, clip.y+clip.h);

    m_rectClipRegion=gweni::Rect(x0, y0, std::max(0, x1-x0), std::max(0, y1-y0));
}

bool Base::clipRegionVisible()
{
    if(m_rectClipRegion.w <= 0 || m_rectClipRegion.h <= 0)
        return false;

    return true;
}

void Base::drawMissingImage(size_t primitiveId, gweni::Rect targetRect, int zIndex)
{
    setDrawColor(colors::Missing);
    drawFilledRect(primitiveId, targetRect, zIndex);
}

//! a + (b-a)*t per channel, rounded; t in [0,1].
static gweni::Color mixColor(const gweni::Color &a, const gweni::Color &b, float t)
{
    auto channel=[t](unsigned char x, unsigned char y)
    {
        return static_cast<unsigned char>(std::lround(x+(y-x)*t));
    };

    return gweni::Color(channel(a.r, b.r), channel(a.g, b.g), channel(a.b, b.b), channel(a.a, b.a));
}

void Base::drawBilinearRect(size_t primitiveId, gweni::Rect rect, int zIndex,
    gweni::Color c00, gweni::Color c10, gweni::Color c01, gweni::Color c11,
    float radius, float border, gweni::Color borderColor)
{
    drawRoundedRect(primitiveId, rect, radius, border,
        mixColor(c00, c10, 0.5f), mixColor(c01, c11, 0.5f), borderColor, zIndex);
}

// A rect along the longer axis, from the nearer whole unit of one end to the other's,
// and round(thickness) units across, starting where OpenGLCore's stroke of that many
// device px does at scale 1.
void Base::drawLine(size_t primitiveId, gweni::PointF p0, gweni::PointF p1,
    float thickness, int zIndex)
{
    const int across=std::max(1, static_cast<int>(std::lround(thickness)));
    const bool horizontal=std::abs(p1.x-p0.x) >= std::abs(p1.y-p0.y);
    const float lo=horizontal ? std::min(p0.x, p1.x) : std::min(p0.y, p1.y);
    const float hi=horizontal ? std::max(p0.x, p1.x) : std::max(p0.y, p1.y);
    const float middle=horizontal ? 0.5f*(p0.y+p1.y) : 0.5f*(p0.x+p1.x);
    const int start=static_cast<int>(std::lround(lo));
    const int length=std::max(1, static_cast<int>(std::lround(hi))-start);
    const int top=static_cast<int>(std::floor(middle-0.5f*across+0.5f));

    drawFilledRect(primitiveId, horizontal ? gweni::Rect(start, top, length, across)
        : gweni::Rect(top, start, across, length), zIndex);
}

void Base::drawCheckerRect(size_t primitiveId, gweni::Rect rect, int zIndex,
    gweni::Color fill, gweni::Color light, gweni::Color dark,
    float radius, float border, gweni::Color borderColor)
{
    gweni::Color flat=mixColor(mixColor(light, dark, 0.5f), fill, fill.a/255.0f);

    flat.a=255;
    drawRoundedRect(primitiveId, rect, radius, border, flat, flat, borderColor, zIndex);
}

///  If they haven't defined these font functions in their renderer code
///  we just draw some rects where the letters would be to give them an
///  idea.
void Base::renderText(size_t textId, const gweni::Font &font, gweni::Point pos, int zIndex, const gweni::String &text)
{
//    const float fSize=font.size  *scale();
//
//    for(unsigned int i=0; i < text.length(); i++)
//    {
//        UnicodeChar chr=text[i];
//
//        if(chr == ' ')
//            continue;
//
//        gweni::Rect r(pos.x + i*fSize*0.4f, pos.y, fSize*0.4f - 1.0f, fSize);
//
//        // This isn't important, it's just me messing around changing
//        // the shape of the rect based on the letter.. just for fun.
//        if(chr == 'l' || chr == 'i' || chr == '!' || chr == 't')
//        {
//            r.w=1;
//        }
//        else if(chr >= 'a' && chr <= 'z')
//        {
//            r.y+=fSize*0.5f;
//            r.h-=fSize*0.4f;
//        }
//        else if(chr == '.' || chr == ',')
//        {
//            r.x+=2;
//            r.y+=r.h-2;
//            r.w=2;
//            r.h=2;
//        }
//        else if(chr == '\'' || chr == '`'  || chr == '"')
//        {
//            r.x+=3;
//            r.w=2;
//            r.h=2;
//        }
//
//        if(chr == 'o' || chr == 'O' || chr == '0')
//            drawLinedRect(r);
//        else
//            drawFilledRect(r);
//    }
}

gweni::Point Base::measureText(const gweni::Font &font, const gweni::String &text)
{
    // in canvas units, whatever the scale (measureText's contract)
    gweni::Point p;
    p.x=font.size*text.length()*0.4f;
    p.y=font.size;
    return p;
}

int Base::lineHeight(const gweni::Font &font)
{
    return measureText(font, " ").y;
}

void Base::measureCarets(const gweni::Font &font, const gweni::String &text,
    std::vector<int> &xs)
{
    xs.clear();
    xs.push_back(0);

    for(size_t i=utf8Next(text, 0); !text.empty(); i=utf8Next(text, i))
    {
        xs.push_back(measureText(font, text.substr(0, i)).x);

        if(i >= text.size())
            break;
    }
}

} // namespace renderer
} // namespace gweni
