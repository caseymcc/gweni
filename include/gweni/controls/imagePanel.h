/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_imagepanel_h_
#define _gweni_controls_imagepanel_h_

#include <gweni/gweni.h>
#include <gweni/controls/base.h>
#include <gweni/platforms/baseRender.h>

namespace gweni
{
namespace controls
{

//! How an ImagePanel places its image in its bounds (setScaleMode)
enum class ImageScaleMode
{
    Stretch,    //!< fills the bounds, the aspect ratio ignored
    Fit,        //!< the largest rect of the image's aspect ratio inside the bounds, centred
    Fill,       //!< covers the bounds with the image's aspect ratio, centred, cropped through the UVs
    None        //!< 1 image px a logical unit, centred, cropped through the UVs to the bounds
};

//! A control that draws one image: a file under the resources (setImage), encoded
//! bytes (setImageData) or RGBA pixels (setImagePixels). With no image it draws
//! nothing (its slot hidden); an image that failed to load draws the renderer's
//! missing-image placeholder. Each set frees the image before it, and setting
//! the same image again (the same name, the same bytes or pixels and max size)
//! changes and draws nothing.
class GWENI_EXPORT ImagePanel: public controls::Base
{
public:

    GWENI_CONTROL(ImagePanel, controls::Base);
    
public:
    //! Draws its texture itself in render(), not through the skin (kept after #169).
    GWENI_DRAWS_NOTHING

    virtual ~ImagePanel();

    //! The part of the image drawn, 0..1 on each axis
    virtual void setUV(float u1, float v1, float u2, float v2);

    //! A file under the resources; an empty name is clearImage()
    virtual void setImage(const String &imageName);
    //! A PNG or JPEG in memory, the format found from its first bytes, kept at a
    //! longest side of at most maxSize image px (0: as it is, which fails past
    //! the renderer's largest image). The bytes are not kept.
    virtual void setImageData(const uint8_t *bytes, size_t size, int maxSize=0);
    //! Rows of width RGBA8 texels stride bytes apart, kept as setImageData keeps them
    virtual void setImagePixels(int width, int height, size_t stride, const uint8_t *rgba, int maxSize=0);
    //! Frees the image: the panel draws nothing until the next set
    virtual void clearImage();
    //! Whether there is no image (nothing set, or cleared); a failed load is not empty
    bool isEmpty() const { return m_source == Source::None; }
    //! The primitive slot it draws in, taken on its first render (max until then)
    size_t primitiveId() const { return m_primitiveId; }

    virtual String &getImage();

    virtual int textureWidth();

    virtual int textureHeight();

    virtual const String &getImageName();

    void render(skin::Base *skin) override;

    virtual void sizeToContents();

    virtual void setDrawColor(gweni::Color color);

    //! Whether an image was set and did not load
    virtual bool failedToLoad();

    void setScaleMode(ImageScaleMode mode);
    ImageScaleMode getScaleMode() const { return m_scaleMode; }

    //! setScaleMode(Stretch) or (None)
    virtual bool getStretch();
    virtual void setStretch(bool b);

    //! The rect (in the bounds' space, whole units) and UVs the image is drawn
    //! with for a texture of textureWidth x textureHeight in bounds, for the mode
    //! and the UVs set: what render() draws. False when nothing shows (no texture
    //! size, or empty bounds).
    static bool placeImage(ImageScaleMode mode, const gweni::Rect &bounds, float textureWidth, float textureHeight,
        const float uv[4], gweni::Rect &rect, float drawnUv[4]);

protected:

    //! Where the image came from, for telling a set that changes nothing
    enum class Source
    {
        None,
        File,
        Data,
        Pixels
    };

    //! After a load: the size, the status, a redraw
    void loaded(Texture::Status status);
    //! Frees the texture, keeping m_source as it is
    void freeImage();

    Texture m_texture;
    size_t m_primitiveId;

    float m_uv[4];
    gweni::Color m_drawColor;

    ImageScaleMode m_scaleMode;
    float m_texWidth, m_texHeight;
    Texture::Status m_status;

    Source m_source=Source::None;
    //! setImageData's or setImagePixels' content: an FNV-1a hash of the bytes
    //! (the pixels' rows), their size and the max size
    uint64_t m_sourceHash=0;

};

}
}
#endif//_gweni_controls_imagepanel_h_
