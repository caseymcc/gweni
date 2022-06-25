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

class GWENI_EXPORT ImagePanel: public controls::Base
{
public:

    GWENI_CONTROL(ImagePanel, controls::Base);
    
public:
    virtual ~ImagePanel();

    virtual void setUV(float u1, float v1, float u2, float v2);

    virtual void setImage(const String &imageName);

    virtual String &getImage();

    virtual int textureWidth();

    virtual int textureHeight();

    virtual const String &getImageName();

    void render(skin::Base *skin) override;

    virtual void sizeToContents();

    virtual void setDrawColor(gweni::Color color);

    virtual bool failedToLoad();

    virtual bool getStretch();

    virtual void setStretch(bool b);

protected:

    Texture m_texture;
    size_t m_primitiveId;

    float m_uv[4];
    gweni::Color m_drawColor;

    bool m_stretch;
    float m_texWidth, m_texHeight;
    Texture::Status m_status;

};

}
}
#endif//_gweni_controls_imagepanel_h_
