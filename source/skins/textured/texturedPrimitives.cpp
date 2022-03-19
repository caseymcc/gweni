#include <gweni/skins/textured/texturedPrimitives.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <gweni/objectIdBank.h>

namespace gweni
{
namespace skin
{
namespace textured
{

Primitive::Primitive():
    m_texture(nullptr)
{
}

Primitive::~Primitive()
{
}

void Primitive::init(Texture *texture, const TextureData &texData, float x, float y, float w, float h)
{
    m_texture=texture;
    
    float const texw=texData.width;
    float const texh=texData.height;

    m_uv[0]=x / texw;
    m_uv[1]=y / texh;
    m_uv[2]=(x + w) / texw;
    m_uv[3]=(y + h) / texh;
    m_width=w;
    m_height=h;
}

//void Primitive::generate(gweni::renderer::Base *render, controls::Base *control)
//{
//    PrimitiveIds &primitiveIds=control->getPrimitiveIds();
//
//    if(primitiveIds.size() < 1)
//        enlargePrimitiveIds(control, primitiveIds, 1);
//
//    render->generatePrimitive(primitiveIds[0]);
//}

void Primitive::draw(gweni::renderer::Base *render, controls::Base *control, gweni::Rect r, int zIndex, const gweni::Color &col)
{
    if(!m_texture)
        return;

//    PrimitiveIds &primitiveIds=control->getPrimitiveIds();
//    if(primitiveIds.size() < 1)
//        enlargePrimitiveIds(control, primitiveIds, 1);

    render->setDrawColor(col);
//    render->drawTexturedRect(*m_texture, primitiveIds[0], r, zIndex, m_uv[0], m_uv[1], m_uv[2], m_uv[3]);
}

void Primitive::drawCenter(gweni::renderer::Base *render, controls::Base *control, gweni::Rect r, int zIndex, const gweni::Color &col)
{
    if(!m_texture)
        return;

    r.x+=(r.w - m_width)/2;
    r.y+=(r.h - m_height)/2;
    r.w=m_width;
    r.h=m_height;
    draw(render, control, r, zIndex, col);
}

//void Primitive::remove(gweni::renderer::Base *render, controls::Base *control)
//{
//    PrimitiveIds &primitiveIds=control->getPrimitiveIds();
//
//    if(primitiveIds.size() < 1)
//        return;
//
//    render->removePrimitive(primitiveIds[0]);
//}

BorderedPrimitive::BorderedPrimitive():
    m_texture(nullptr)
{
}

BorderedPrimitive::~BorderedPrimitive()
{
}

void BorderedPrimitive::init(TexturedSkin *skin, size_t id, float drawMarginScale)
{
    if(id >= skin->m_textureEntries.size())
        return;

    TextureEntry &textureEntry=skin->m_textureEntries[id];

    Rect boundingBox=textureEntry.boundingBox;
    BoundingBox margin=textureEntry.margin;

    init(&skin->m_texture, boundingBox, margin, drawMarginScale);
}

void BorderedPrimitive::init(Texture *texture, Rect &rect, BoundingBox &margin, float drawMarginScale)
{
    m_texture=texture;
    m_margin=margin;

    setRect(0, rect.x, rect.y, m_margin.left, m_margin.top);
    setRect(1, rect.x + m_margin.left, rect.y, rect.w - m_margin.left - m_margin.right, m_margin.top);
    setRect(2, (rect.x + rect.w) - m_margin.right, rect.y, m_margin.right, m_margin.top);
    setRect(3, rect.x, rect.y + m_margin.top, m_margin.left, rect.h - m_margin.top - m_margin.bottom);
    setRect(4, rect.x + m_margin.left, rect.y + m_margin.top, rect.w - m_margin.left - m_margin.right, rect.h - m_margin.top - m_margin.bottom);
    setRect(5, (rect.x + rect.w) - m_margin.right, rect.y + m_margin.top, m_margin.right, rect.h - m_margin.top - m_margin.bottom);
    setRect(6, rect.x, (rect.y + rect.h) - m_margin.bottom, m_margin.left, m_margin.bottom);
    setRect(7, rect.x + m_margin.left, (rect.y + rect.h) - m_margin.bottom, rect.w - m_margin.left - m_margin.right, m_margin.bottom);
    setRect(8, (rect.x + rect.w) - m_margin.right, (rect.y + rect.h) - m_margin.bottom, m_margin.right, m_margin.bottom);

    m_margin.left=(int)(((float)m_margin.left)*drawMarginScale);
    m_margin.right=(int)(((float)m_margin.right)*drawMarginScale);
    m_margin.top=(int)(((float)m_margin.top)*drawMarginScale);
    m_margin.bottom=(int)(((float)m_margin.bottom)*drawMarginScale);

    m_width=rect.w - rect.x;
    m_height=rect.h - rect.y;
}

void BorderedPrimitive::setRect(int num, float x, float y, float w, float h)
{
//    float const texw=m_texture.width;
//    float const texh=m_texture.height;
//    m_rects[num].m_uv[0]=x / texw;
//    m_rects[num].m_uv[1]=y / texh;
//    m_rects[num].m_uv[2]=(x + w) / texw;
//    m_rects[num].m_uv[3]=(y + h) / texh;
    m_rects[num].m_uv[0]=x;
    m_rects[num].m_uv[1]=y;
    m_rects[num].m_uv[2]=(x + w);
    m_rects[num].m_uv[3]=(y + h);
}

void BorderedPrimitive::show(renderer::Base *render, size_t *primitiveIds, bool visible)
{
    if(visible)
    {
        for(size_t i=0; i<9; i++)
        {
            render->showPrimitive(2, primitiveIds[i]);
        }
    }
    else
    {
        for(size_t i=0; i<9; i++)
        {
            render->hidePrimitive(primitiveIds[i]);
        }
    }
}   

void BorderedPrimitive::draw(renderer::Base *render, size_t *primitiveIds, const Rect &r, int zIndex,
    const Color &col, unsigned int draw)
{
    if(!m_texture)
        return;

    render->setDrawColor(col);

    if(r.w < m_width && r.h < m_height)
    {
        render->drawTexturedRect(*m_texture, primitiveIds[0], r,
            m_rects[0].m_uv[0], m_rects[0].m_uv[1], m_rects[8].m_uv[2], m_rects[8].m_uv[3]);
        return;
    }

    if(draw & (1<<0))
        drawRect(render, primitiveIds[0], 0, r.x, r.y, m_margin.left, m_margin.top, zIndex);

    if(draw & (1<<1))
        drawRect(render, primitiveIds[1], 1, r.x + m_margin.left, r.y, r.w - m_margin.left - m_margin.right, m_margin.top, zIndex);

    if(draw & (1<<2))
        drawRect(render, primitiveIds[2], 2, (r.x + r.w) - m_margin.right, r.y, m_margin.right, m_margin.top, zIndex);

    if(draw & (1<<3))
        drawRect(render, primitiveIds[3], 3, r.x, r.y + m_margin.top, m_margin.left, r.h - m_margin.top - m_margin.bottom, zIndex);

    if(draw & (1<<4))
        drawRect(render, primitiveIds[4], 4, r.x + m_margin.left, r.y + m_margin.top,
            r.w - m_margin.left - m_margin.right, r.h - m_margin.top - m_margin.bottom, zIndex);

    if(draw & (1<<5))
        drawRect(render, primitiveIds[5], 5, (r.x + r.w) - m_margin.right, r.y + m_margin.top, m_margin.right, r.h - m_margin.top - m_margin.bottom, zIndex);

    if(draw & (1<<6))
        drawRect(render, primitiveIds[6], 6, r.x, (r.y + r.h) - m_margin.bottom, m_margin.left, m_margin.bottom, zIndex);

    if(draw & (1<<7))
        drawRect(render, primitiveIds[7], 7, r.x + m_margin.left, (r.y + r.h) - m_margin.bottom, r.w - m_margin.left - m_margin.right, m_margin.bottom, zIndex);

    if(draw & (1<<8))
        drawRect(render, primitiveIds[8], 8, (r.x + r.w) - m_margin.right, (r.y + r.h) - m_margin.bottom, m_margin.right, m_margin.bottom, zIndex);
}

void BorderedPrimitive::drawRect(gweni::renderer::Base *render, size_t primitiveId, size_t i, int x, int y, int w, int h, int zIndex)
{
    render->drawTexturedRect(*m_texture, primitiveId,
        gweni::Rect(x, y, w, h), zIndex,
        m_rects[i].m_uv[0], m_rects[i].m_uv[1], m_rects[i].m_uv[2], m_rects[i].m_uv[3]);
}

//void BorderedPrimitive::remove(gweni::renderer::Base *render, controls::Base *control)
//{
//    PrimitiveIds &primitiveIds=control->getPrimitiveIds();
//
//    if(primitiveIds.size() < 9)
//        return;
//
//    for(size_t i=0; i<primitiveIds.size(); ++i)
//        render->removePrimitive(primitiveIds[i]);
//}

}//namespace textured
}//namespace skin
}//namespace gweni

