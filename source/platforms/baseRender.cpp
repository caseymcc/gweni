/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/platforms/baseRender.h>
#include <gweni/utility.h>
#include <gweni/platforms/platform.h>

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

void Base::translate(int &x, int &y)
{
    x+=m_renderOffset.x;
    y+=m_renderOffset.y;
    x=std::ceil(float(x)  *m_scale);
    y=std::ceil(float(y)  *m_scale);
}

void Base::translate(gweni::Rect &rect)
{
    translate(rect.x, rect.y);
    rect.w=std::ceil(float(rect.w)  *m_scale);
    rect.h=std::ceil(float(rect.h)  *m_scale);
}

void gweni::renderer::Base::setClipRegion(gweni::Rect const &rect)
{
    m_rectClipRegion=rect;
}

void Base::addClipRegion(gweni::Rect rect)
{
    rect.x=m_renderOffset.x;
    rect.y=m_renderOffset.y;
    gweni::Rect out=rect;

    if(rect.x < m_rectClipRegion.x)
    {
        out.w-=m_rectClipRegion.x - out.x;
        out.x=m_rectClipRegion.x;
    }

    if(rect.y < m_rectClipRegion.y)
    {
        out.h-=m_rectClipRegion.y - out.y;
        out.y=m_rectClipRegion.y;
    }

    if(rect.x+rect.w > m_rectClipRegion.x+m_rectClipRegion.w)
        out.w=(m_rectClipRegion.x + m_rectClipRegion.w) - out.x;

    if(rect.y+rect.h > m_rectClipRegion.y+m_rectClipRegion.h)
        out.h=(m_rectClipRegion.y + m_rectClipRegion.h) - out.y;

    m_rectClipRegion=out;
}

const gweni::Rect &Base::clipRegion() const
{
    return m_rectClipRegion;
}

bool Base::clipRegionVisible()
{
    if(m_rectClipRegion.w <= 0 || m_rectClipRegion.h <= 0)
        return false;

    return true;
}

void Base::drawMissingImage(size_t primitiveId, gweni::Rect targetRect, int zIndex)
{
    setDrawColor(colors::Red);
    drawFilledRect(primitiveId, targetRect, zIndex);
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
    gweni::Point p;
    p.x=font.size  *scale()  *text.length()  *0.4f;
    p.y=font.size  *scale();
    return p;
}

} // namespace renderer
} // namespace gweni
