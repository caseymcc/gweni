/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/imagePanel.h>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(ImagePanel)
{
    m_primitiveId=getPrimitiveId((void *)this);

    setUV(0, 0, 1, 1);
    setMouseInputEnabled(false);
    m_drawColor=colors::White;
    setStretch(true);
    m_texWidth=0.0f;
    m_texHeight=0.0f;
}

ImagePanel::~ImagePanel()
{
    getSkin()->getRender()->getLoader().freeTexture(m_texture);

    releasePrimitiveId(m_primitiveId);
}

void ImagePanel::setUV(float u1, float v1, float u2, float v2)
{
    m_uv[0]=u1;
    m_uv[1]=v1;
    m_uv[2]=u2;
    m_uv[3]=v2;
}

void ImagePanel::setImage(const String &imageName)
{
    m_texture.name=imageName;
    IResourceLoader &loader=getSkin()->getRender()->getLoader();
    Texture::Status m_status=loader.loadTexture(m_texture);
    switch(m_status)
    {
    case Texture::Status::Loaded:
    {
        TextureData texData=loader.getTextureData(m_texture);
        m_texWidth=texData.width;
        m_texHeight=texData.height;
        break;
    }
    default:
        ;
    }
}

String &ImagePanel::getImage()
{
    return m_texture.name;
}

int ImagePanel::textureWidth()
{
    return m_texWidth;
}

int ImagePanel::textureHeight()
{
    return m_texHeight;
}

const String &ImagePanel::getImageName()
{
    return m_texture.name;
}

void ImagePanel::render(skin::Base *skin)
{
    renderer::Base *render=skin->getRender();

    render->setDrawColor(m_drawColor);

    if(m_stretch)
    {
        render->drawTexturedRect(m_texture, m_primitiveId,
            getRenderBounds(),
            m_uv[0], m_uv[1], m_uv[2], m_uv[3]);
    }
    else
    {
        render->drawTexturedRect(m_texture, m_primitiveId,
            gweni::Rect(0, 0, m_texWidth, m_texHeight),
            m_uv[0], m_uv[1], m_uv[2], m_uv[3]);
    }
}

void ImagePanel::sizeToContents()
{
    setSize(m_texWidth, m_texHeight);
}

void ImagePanel::setDrawColor(gweni::Color color)
{
    m_drawColor=color;
}

bool ImagePanel::failedToLoad()
{
    return m_status != Texture::Status::Loaded;
}

bool ImagePanel::getStretch()
{
    return m_stretch;
}

void ImagePanel::setStretch(bool b)
{
    m_stretch=b;
}

}//namespace controls
}//namespace gweni
