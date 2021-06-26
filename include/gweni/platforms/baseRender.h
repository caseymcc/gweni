/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios

 *  See license in Gweni.h
 */


#ifndef _gweni_baserender_h_
#define _gweni_baserender_h_

#include <gweni/platforms/platformTypes.h>

namespace gweni
{
struct Font;
struct Texture;
class WindowProvider;

namespace renderer
{
class Base;

class ICacheToTexture
{
public:

    typedef void *CacheHandle;

    virtual ~ICacheToTexture() {}
    virtual void initialize()=0;
    virtual void shutDown()=0;
    virtual void setupCacheTexture(CacheHandle control)=0;
    virtual void finishCacheTexture(CacheHandle control)=0;
    virtual void drawCachedControlTexture(CacheHandle control)=0;
    virtual void createControlCacheTexture(CacheHandle control, const Point &size)=0;
    virtual void updateControlCacheTexture(CacheHandle control)=0;
    virtual void setRenderer(gweni::renderer::Base *renderer)=0;
};

//
//! \brief Base class for all renderer implementations.
//!
//! Each renderer implements this functionality so that the skin can draw the GUI.
//! \note We never instance this directly, only the derived implementations.
//
class GWENI_EXPORT Base: public IResourceLoader
{
protected:

    //! Constructor. Not public as we only instance derived implementations.
    Base(ResourcePaths &paths);

public:

    virtual ~Base();

    virtual void init() {}
    virtual void begin() {}
    virtual void end() {}

    //! Get ResourceLoader we are using for the renderer.
    IResourceLoader &getLoader() { return *reinterpret_cast<IResourceLoader *>(this); }
    ResourcePaths &getResourcePaths() { return m_paths; }

    //! Set the current drawing color.
    virtual void setDrawColor(Color color) {}

    virtual void startClip() {}     //!< Start clipping the drawing. \see setClipRegion().
    virtual void endClip() {}       //!< Stop clipping the drawing.

    void    setClipRegion(gweni::Rect const &rect);
    void    addClipRegion(gweni::Rect rect);
    bool    clipRegionVisible();
    const gweni::Rect &clipRegion() const;

    virtual void generatePrimitive(size_t primitiveId) {}
    virtual void removePrimitive(size_t primitiveId) {}

    //! Draw a filled rectangle using the current color.
    virtual void drawFilledRect(size_t primitiveId, gweni::Rect rect) {}

    virtual void drawTexturedRect(const gweni::Texture &texture, size_t primitiveId, gweni::Rect targetRect,
        float u1=0.0f, float v1=0.0f,
        float u2=1.0f, float v2=1.0f)
    {}

    virtual void showPrimitive(size_t primitiveId) {}
    virtual void hidePrimitive(size_t primitiveId) {}

    virtual void drawMissingImage(size_t primitiveId,gweni::Rect targetRect);

    virtual gweni::Color pixelColor(const gweni::Texture &texture,
        unsigned int x, unsigned int y,
        const gweni::Color &col_default=gweni::colors::White)
    {
        return col_default;
    }

    virtual ICacheToTexture *getCTT()
    {
        return m_RTT;
    }

    virtual void renderText(size_t textId, const gweni::Font &font,
        gweni::Point pos,
        const gweni::String &text);

    virtual gweni::Point measureText(const gweni::Font &font,
        const gweni::String &text);

    //! \sect{Render Specialisation}
    //!     No need to implement these functions in your derived class, but
    //!     if you can do them faster than the default implementation it's a
    //!     good idea to.
    virtual void drawLinedRect(size_t *primitiveIds, gweni::Rect rect);
    virtual void drawPixel(size_t *primitiveIds, int x, int y);
    virtual void drawShavedCornerRect(size_t *primitiveIds, gweni::Rect rect, bool bSlight=false);
    //! \}

    //! \sect{Translate}
    //!     Translate a panel's local drawing coordinate
    //!     into view space, taking Offsets into account.
    void translate(int &x, int &y);
    void translate(gweni::Rect &rect);
    //! \}

    //! Set the rendering offset. You shouldn't have to touch these, ever.
    void setRenderOffset(const gweni::Point &offset)
    {
        m_renderOffset=offset;
    }

    void addRenderOffset(const gweni::Rect &offset)
    {
        m_renderOffset.x+=offset.x;
        m_renderOffset.y+=offset.y;
    }

    const gweni::Point &getRenderOffset() const
    {
        return m_renderOffset;
    }


    void setScale(float fScale) { m_scale=fScale; }
    float scale() const { return m_scale; }

    //
    // Self Initialization, shutdown
    //
    virtual bool initializeContext(gweni::WindowProvider *window)
    {
        return false;
    }

    virtual bool shutdownContext(gweni::WindowProvider *window)
    {
        return false;
    }

    virtual bool resizedContext(gweni::WindowProvider *window, int w, int h)
    {
        return false;
    }

    virtual bool beginContext(gweni::WindowProvider *window)
    {
        return false;
    }

    virtual bool endContext(gweni::WindowProvider *window)
    {
        return false;
    }

    virtual bool presentContext(gweni::WindowProvider *window)
    {
        return false;
    }

protected:

    virtual bool ensureFont(const gweni::Font &font) { return false; }
    virtual bool ensureTexture(const gweni::Texture &texture) { return false; }

    float m_scale;

private:

    ResourcePaths &m_paths;
    gweni::Point m_renderOffset;
    gweni::Rect m_rectClipRegion;
    ICacheToTexture *m_RTT;
};

}
}
#endif //_gweni_baserender_h_
