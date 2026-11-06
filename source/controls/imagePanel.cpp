/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/imagePanel.h>

#include <limits>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(ImagePanel)
{
    // taken on the first render, after the parent's skin took its ids, so the image
    // is drawn after the background it sits on (see render())
    m_primitiveId=std::numeric_limits<size_t>::max();
    m_status=Texture::Status::Unloaded;

    m_uv[0]=0.0f;
    m_uv[1]=0.0f;
    m_uv[2]=1.0f;
    m_uv[3]=1.0f;
    setMouseInputEnabled(false);
    m_drawColor=colors::White;
    m_stretch=true;
    m_texWidth=0.0f;
    m_texHeight=0.0f;
}

ImagePanel::~ImagePanel()
{
    getSkin()->getRenderer()->getLoader().freeTexture(m_texture);

    if(m_primitiveId != std::numeric_limits<size_t>::max())
        getSkin()->getRenderer()->releasePrimitive(m_primitiveId);
}

void ImagePanel::setUV(float u1, float v1, float u2, float v2)
{
    m_uv[0]=u1;
    m_uv[1]=v1;
    m_uv[2]=u2;
    m_uv[3]=v2;
    redraw();
}

void ImagePanel::setImage(const String &imageName)
{
    m_texture.name=imageName;
    IResourceLoader &loader=getSkin()->getRenderer()->getLoader();
    m_status=loader.loadTexture(m_texture);
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
        m_texWidth=0.0f;
        m_texHeight=0.0f;
    }
    redraw();
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
    // as Base::render: an unchanged control leaves its slot as it is
    if(getStateChange() == StateChange_Nothing)
        return;

    // counts the change in getChangeStats() (there is no skin Control to update)
    ParentClass::render(skin);

    renderer::Base *render=skin->getRenderer();

    if(m_primitiveId == std::numeric_limits<size_t>::max())
        m_primitiveId=getPrimitiveId((void *)this);

    // Hide the image when the control isn't visible (this control or a parent
    // hidden), otherwise it keeps drawing and bleeds across other pages, since
    // rendering doesn't skip hidden subtrees and this control draws its texture
    // directly rather than through a skin renderer.
    if(!visible())
    {
        render->hidePrimitive(m_primitiveId);
        return;
    }

    render->setDrawColor(m_drawColor);

    // render bounds are in the parent's space, the renderer's offset at this point;
    // the z index is above the parent's, so its background does not hide the image.
    // A texture that did not load draws the renderer's missing-image placeholder.
    gweni::Rect rect=getRenderBounds();

    if(!m_stretch)
    {
        rect.w=m_texWidth;
        rect.h=m_texHeight;
    }

    render->drawTexturedRect(m_texture, m_primitiveId, rect, getZIndex(),
        m_uv[0], m_uv[1], m_uv[2], m_uv[3]);
}

void ImagePanel::sizeToContents()
{
    setSize(m_texWidth, m_texHeight);
}

void ImagePanel::setDrawColor(gweni::Color color)
{
    if(m_drawColor == color)
        return;

    m_drawColor=color;
    redraw();
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
    redraw();
}

}//namespace controls
}//namespace gweni
