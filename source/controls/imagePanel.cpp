/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/imagePanel.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace gweni
{
namespace controls
{

namespace
{

uint64_t fnv1a(uint64_t hash, const uint8_t *bytes, size_t size)
{
    for(size_t i=0; i<size; ++i)
    {
        hash^=bytes[i];
        hash*=1099511628211ull;
    }
    return hash;
}

uint64_t fnv1a(uint64_t hash, uint64_t value)
{
    uint8_t bytes[8];

    for(int i=0; i<8; ++i)
        bytes[i]=(uint8_t)(value >> (i*8));
    return fnv1a(hash, bytes, 8);
}

const uint64_t fnvBasis=14695981039346656037ull;

}//namespace

GWENI_CONTROL_CONSTRUCTOR(ImagePanel)
{
    // taken on the first render, after the parent's skin took its ids, so the image
    // is drawn after the background it sits on (see render()); an empty panel takes
    // it too, so an image set later keeps that order
    m_primitiveId=std::numeric_limits<size_t>::max();
    m_status=Texture::Status::Unloaded;

    m_uv[0]=0.0f;
    m_uv[1]=0.0f;
    m_uv[2]=1.0f;
    m_uv[3]=1.0f;
    setMouseInputEnabled(false);
    m_drawColor=colors::White;
    m_scaleMode=ImageScaleMode::Stretch;
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
    if(m_uv[0] == u1 && m_uv[1] == v1 && m_uv[2] == u2 && m_uv[3] == v2)
        return;

    m_uv[0]=u1;
    m_uv[1]=v1;
    m_uv[2]=u2;
    m_uv[3]=v2;
    redraw();
}

void ImagePanel::freeImage()
{
    getSkin()->getRenderer()->getLoader().freeTexture(m_texture);
    m_texture=Texture();
    m_texWidth=0.0f;
    m_texHeight=0.0f;
    m_status=Texture::Status::Unloaded;
}

void ImagePanel::loaded(Texture::Status status)
{
    m_status=status;
    if(m_status == Texture::Status::Loaded)
    {
        TextureData texData=getSkin()->getRenderer()->getLoader().getTextureData(m_texture);

        m_texWidth=texData.width;
        m_texHeight=texData.height;
    }
    else
    {
        m_texWidth=0.0f;
        m_texHeight=0.0f;
    }
    redraw();
}

void ImagePanel::setImage(const String &imageName)
{
    if(imageName.empty())
        return clearImage();
    if((m_source == Source::File) && (m_texture.name == imageName))
        return;

    freeImage();
    m_source=Source::File;
    m_texture.name=imageName;
    loaded(getSkin()->getRenderer()->getLoader().loadTexture(m_texture));
}

void ImagePanel::setImageData(const uint8_t *bytes, size_t size, int maxSize)
{
    const uint64_t hash=fnv1a(fnv1a(fnv1a(fnvBasis, bytes?bytes:(const uint8_t *)"", bytes?size:0),
        (uint64_t)size), (uint64_t)(int64_t)maxSize);

    if((m_source == Source::Data) && (m_sourceHash == hash))
        return;

    freeImage();
    m_source=Source::Data;
    m_sourceHash=hash;
    loaded(getSkin()->getRenderer()->getLoader().loadTexture(m_texture, bytes, size, maxSize));
}

void ImagePanel::setImagePixels(int width, int height, size_t stride, const uint8_t *rgba, int maxSize)
{
    uint64_t hash=fnv1a(fnv1a(fnv1a(fnvBasis, (uint64_t)(int64_t)width), (uint64_t)(int64_t)height),
        (uint64_t)(int64_t)maxSize);

    // the rows' texels, not the stride's padding
    if(rgba && width > 0 && height > 0 && stride >= (size_t)width*4)
    {
        for(int y=0; y<height; ++y)
            hash=fnv1a(hash, rgba+(size_t)y*stride, (size_t)width*4);
    }
    else
        hash=fnv1a(hash, (uint64_t)stride);

    if((m_source == Source::Pixels) && (m_sourceHash == hash))
        return;

    freeImage();
    m_source=Source::Pixels;
    m_sourceHash=hash;
    loaded(getSkin()->getRenderer()->getLoader().loadTexturePixels(m_texture, width, height, stride, rgba,
        maxSize));
}

void ImagePanel::clearImage()
{
    if(m_source == Source::None)
        return;

    freeImage();
    m_source=Source::None;
    m_sourceHash=0;
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

bool ImagePanel::placeImage(ImageScaleMode mode, const gweni::Rect &bounds, float textureWidth, float textureHeight,
    const float uv[4], gweni::Rect &rect, float drawnUv[4])
{
    // the part of the image the UVs select, in texels
    const float imageW=textureWidth*std::fabs(uv[2]-uv[0]);
    const float imageH=textureHeight*std::fabs(uv[3]-uv[1]);

    if(bounds.w <= 0 || bounds.h <= 0)
        return false;

    // where the whole selected image goes, in float units: the bounds for
    // Stretch, else its aspect ratio at a scale, centred
    float x=(float)bounds.x;
    float y=(float)bounds.y;
    float w=(float)bounds.w;
    float h=(float)bounds.h;

    if(mode != ImageScaleMode::Stretch)
    {
        if(imageW <= 0.0f || imageH <= 0.0f)
            return false;

        float scale=1.0f;

        if(mode == ImageScaleMode::Fit)
            scale=std::min(w/imageW, h/imageH);
        else if(mode == ImageScaleMode::Fill)
            scale=std::max(w/imageW, h/imageH);
        x+=(w-imageW*scale)*0.5f;
        y+=(h-imageH*scale)*0.5f;
        w=imageW*scale;
        h=imageH*scale;
    }

    // that cut to the bounds and rounded to whole units, the UVs following the
    // cut, so Fill and None crop through the UVs rather than a clip. Half up on
    // both sides of 0 (lround goes away from it), so an image is its own width
    // wherever its bounds start
    auto whole=[](float v) { return (int)std::floor((double)v+0.5); };
    const int left=std::max(bounds.x, whole(x));
    const int top=std::max(bounds.y, whole(y));
    const int right=std::min(bounds.x+bounds.w, whole(x+w));
    const int bottom=std::min(bounds.y+bounds.h, whole(y+h));

    if(right <= left || bottom <= top)
        return false;

    rect=gweni::Rect(left, top, right-left, bottom-top);

    auto lerp=[](float a, float b, float t) { return a+(b-a)*t; };

    // a rect that is the whole placed image (Stretch, and Fit) keeps the UVs as set
    if((float)left <= x+0.5f && (float)right >= x+w-0.5f)
    {
        drawnUv[0]=uv[0];
        drawnUv[2]=uv[2];
    }
    else
    {
        drawnUv[0]=lerp(uv[0], uv[2], ((float)left-x)/w);
        drawnUv[2]=lerp(uv[0], uv[2], ((float)right-x)/w);
    }
    if((float)top <= y+0.5f && (float)bottom >= y+h-0.5f)
    {
        drawnUv[1]=uv[1];
        drawnUv[3]=uv[3];
    }
    else
    {
        drawnUv[1]=lerp(uv[1], uv[3], ((float)top-y)/h);
        drawnUv[3]=lerp(uv[1], uv[3], ((float)bottom-y)/h);
    }
    return true;
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
    // directly rather than through a skin renderer. With no image the slot is
    // hidden too: an empty panel draws nothing.
    if(!visible() || (m_source == Source::None))
    {
        render->hidePrimitive(m_primitiveId);
        return;
    }

    render->setDrawColor(m_drawColor);

    // render bounds are in the parent's space, the renderer's offset at this point;
    // the z index is above the parent's, so its background does not hide the image.
    // A texture that did not load draws the renderer's missing-image placeholder
    // over the whole bounds.
    const gweni::Rect bounds=getRenderBounds();
    gweni::Rect rect=bounds;
    float uv[4]={ m_uv[0], m_uv[1], m_uv[2], m_uv[3] };

    if((m_status == Texture::Status::Loaded) &&
        !placeImage(m_scaleMode, bounds, m_texWidth, m_texHeight, m_uv, rect, uv))
    {
        render->hidePrimitive(m_primitiveId);
        return;
    }

    render->drawTexturedRect(m_texture, m_primitiveId, rect, getZIndex(), uv[0], uv[1], uv[2], uv[3]);
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
    return (m_source != Source::None) && (m_status != Texture::Status::Loaded);
}

void ImagePanel::setScaleMode(ImageScaleMode mode)
{
    if(m_scaleMode == mode)
        return;

    m_scaleMode=mode;
    redraw();
}

bool ImagePanel::getStretch()
{
    return m_scaleMode == ImageScaleMode::Stretch;
}

void ImagePanel::setStretch(bool b)
{
    setScaleMode(b?ImageScaleMode::Stretch:ImageScaleMode::None);
}

}//namespace controls
}//namespace gweni
