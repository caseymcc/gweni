#include <gweni/skins/texturedPrimitives.h>

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

void Primitive::generate(gweni::renderer::Base *render, controls::Base *control)
{
    PrimitiveIds &primitiveIds=control->getPrimitiveIds();

    if(primitiveIds.size() < 1)
        enlargePrimitiveIds(control, primitiveIds, 1);

    render->generatePrimitive(primitiveIds[0]);
}

void Primitive::draw(gweni::renderer::Base *render, controls::Base *control, gweni::Rect r, const gweni::Color &col)
{
    if(!m_texture)
        return;

    PrimitiveIds &primitiveIds=control->getPrimitiveIds();
//    if(primitiveIds.size() < 1)
//        enlargePrimitiveIds(control, primitiveIds, 1);

    render->setDrawColor(col);
    render->drawTexturedRect(*m_texture, primitiveIds[0], r, m_uv[0], m_uv[1], m_uv[2], m_uv[3]);
}

void Primitive::drawCenter(gweni::renderer::Base *render, controls::Base *control, gweni::Rect r, const gweni::Color &col)
{
    if(!m_texture)
        return;

    r.x+=(r.w - m_width)/2;
    r.y+=(r.h - m_height)/2;
    r.w=m_width;
    r.h=m_height;
    draw(render, control, r, col);
}

void Primitive::remove(gweni::renderer::Base *render, controls::Base *control)
{
    PrimitiveIds &primitiveIds=control->getPrimitiveIds();

    if(primitiveIds.size() < 1)
        return;

    render->removePrimitive(primitiveIds[0]);
}

BorderedPrimitive::BorderedPrimitive():
    m_texture(nullptr)
{
}

BorderedPrimitive::~BorderedPrimitive()
{
}

void BorderedPrimitive::init(Texture *texture, const TextureData &texData, float x, float y, float w, float h, Margin in_margin, float drawMarginScale)
{
    m_texture=texture;
    m_texData=texData;
    m_margin=in_margin;

    setRect(0, x, y, m_margin.left, m_margin.top);
    setRect(1, x + m_margin.left, y, w - m_margin.left - m_margin.right, m_margin.top);
    setRect(2, (x + w) - m_margin.right, y, m_margin.right, m_margin.top);
    setRect(3, x, y + m_margin.top, m_margin.left, h - m_margin.top - m_margin.bottom);
    setRect(4, x + m_margin.left, y + m_margin.top, w - m_margin.left - m_margin.right, h - m_margin.top - m_margin.bottom);
    setRect(5, (x + w) - m_margin.right, y + m_margin.top, m_margin.right, h - m_margin.top - m_margin.bottom);
    setRect(6, x, (y + h) - m_margin.bottom, m_margin.left, m_margin.bottom);
    setRect(7, x + m_margin.left, (y + h) - m_margin.bottom, w - m_margin.left - m_margin.right, m_margin.bottom);
    setRect(8, (x + w) - m_margin.right, (y + h) - m_margin.bottom, m_margin.right, m_margin.bottom);
    m_margin.left*=drawMarginScale;
    m_margin.right*=drawMarginScale;
    m_margin.top*=drawMarginScale;
    m_margin.bottom*=drawMarginScale;
    m_width=w - x;
    m_height=h - y;
}

void BorderedPrimitive::setRect(int num, float x, float y, float w, float h)
{
    float const texw=m_texData.width;
    float const texh=m_texData.height;
//    m_rects[num].m_uv[0]=x / texw;
//    m_rects[num].m_uv[1]=y / texh;
//    m_rects[num].m_uv[2]=(x + w) / texw;
//    m_rects[num].m_uv[3]=(y + h) / texh;
    m_rects[num].m_uv[0]=x;
    m_rects[num].m_uv[1]=y;
    m_rects[num].m_uv[2]=(x + w);
    m_rects[num].m_uv[3]=(y + h);
}

void BorderedPrimitive::generate(gweni::renderer::Base *render, controls::Base *control)
{
    PrimitiveIds &primitiveIds=control->getPrimitiveIds();
    if(primitiveIds.size() < 9)
        enlargePrimitiveIds(control, primitiveIds, 9);

    for(size_t i=0; i<9; ++i)
    {
        render->generatePrimitive(primitiveIds[i]);
    }
}

void BorderedPrimitive::draw(gweni::renderer::Base *render, controls::Base *control, gweni::Rect r,
    const gweni::Color &col, unsigned int draw)
{
    if(!m_texture)
        return;

    PrimitiveIds &primitiveIds=control->getPrimitiveIds();
//    if(primitiveIds.size() < 9)
//        enlargePrimitiveIds(control, primitiveIds, 9);

    render->setDrawColor(col);

    if(r.w < m_width && r.h < m_height)
    {
        render->drawTexturedRect(*m_texture, primitiveIds[0], r,
            m_rects[0].m_uv[0], m_rects[0].m_uv[1], m_rects[8].m_uv[2], m_rects[8].m_uv[3]);
        return;
    }

    if(draw & (1<<0))
        drawRect(render, primitiveIds, 0, r.x, r.y, m_margin.left, m_margin.top);

    if(draw & (1<<1))
        drawRect(render, primitiveIds, 1, r.x + m_margin.left, r.y, r.w - m_margin.left - m_margin.right, m_margin.top);

    if(draw & (1<<2))
        drawRect(render, primitiveIds, 2, (r.x + r.w) - m_margin.right, r.y, m_margin.right, m_margin.top);

    if(draw & (1<<3))
        drawRect(render, primitiveIds, 3, r.x, r.y + m_margin.top, m_margin.left, r.h - m_margin.top - m_margin.bottom);

    if(draw & (1<<4))
        drawRect(render, primitiveIds, 4, r.x + m_margin.left, r.y + m_margin.top,
            r.w - m_margin.left - m_margin.right, r.h - m_margin.top - m_margin.bottom);

    if(draw & (1<<5))
        drawRect(render, primitiveIds, 5, (r.x + r.w) - m_margin.right, r.y + m_margin.top, m_margin.right, r.h - m_margin.top - m_margin.bottom);

    if(draw & (1<<6))
        drawRect(render, primitiveIds, 6, r.x, (r.y + r.h) - m_margin.bottom, m_margin.left, m_margin.bottom);

    if(draw & (1<<7))
        drawRect(render, primitiveIds, 7, r.x + m_margin.left, (r.y + r.h) - m_margin.bottom, r.w - m_margin.left - m_margin.right, m_margin.bottom);

    if(draw & (1<<8))
        drawRect(render, primitiveIds, 8, (r.x + r.w) - m_margin.right, (r.y + r.h) - m_margin.bottom, m_margin.right, m_margin.bottom);
}

void BorderedPrimitive::drawRect(gweni::renderer::Base *render, PrimitiveIds &primitiveIds, int i, int x, int y, int w, int h)
{
    render->drawTexturedRect(*m_texture, primitiveIds[i],
        gweni::Rect(x, y, w, h),
        m_rects[i].m_uv[0], m_rects[i].m_uv[1], m_rects[i].m_uv[2], m_rects[i].m_uv[3]);
}

void BorderedPrimitive::remove(gweni::renderer::Base *render, controls::Base *control)
{
    PrimitiveIds &primitiveIds=control->getPrimitiveIds();

    if(primitiveIds.size() < 9)
        return;

    for(size_t i=0; i<primitiveIds.size(); ++i)
        render->removePrimitive(primitiveIds[i]);
}

}//namespace textured
}//namespace skin
}//namespace gweni

