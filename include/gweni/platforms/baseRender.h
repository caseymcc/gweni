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

    //! Update the viewport/projection when the output surface is resized.
    virtual void setViewRect(const gweni::Rect &viewRect) {}

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

    virtual void generatePrimitive(size_t &primitiveId) {}
    virtual void releasePrimitive(size_t primitiveId) {}

    //! Draw a filled rectangle using the current color.
    virtual void drawFilledRect(size_t primitiveId, gweni::Rect rect, int zIndex) {}

    //! Draw an anti-aliased rounded rectangle with a vertical fill gradient and an
    //! optional border, evaluated analytically in the fragment shader (SDF). Backends
    //! that don't support it can leave the default no-op; callers should treat absence
    //! as "falls back to a plain filled rect" at the skin level. \a radius and \a border
    //! are in pixels; \a fillTop/\a fillBottom are the gradient endpoints (pass equal
    //! colors for a flat fill); \a borderColor is ignored when \a border<=0.
    virtual void drawRoundedRect(size_t primitiveId, gweni::Rect rect, float radius, float border,
        gweni::Color fillTop, gweni::Color fillBottom, gweni::Color borderColor, int zIndex) {}

    //! Draw a rounded rectangle like drawRoundedRect, but filled with a four-corner
    //! bilinear blend: \a c00 top-left, \a c10 top-right, \a c01 bottom-left, \a c11
    //! bottom-right (exact, with no crease along the quad's diagonal). \a radius,
    //! \a border and \a borderColor are as for drawRoundedRect. The default
    //! approximates it with drawRoundedRect, top = mix(c00,c10), bottom = mix(c01,c11),
    //! so a backend without it still shows the average colours.
    virtual void drawBilinearRect(size_t primitiveId, gweni::Rect rect, int zIndex,
        gweni::Color c00, gweni::Color c10, gweni::Color c01, gweni::Color c11,
        float radius, float border, gweni::Color borderColor);

    //! Draw a rounded rectangle like drawRoundedRect, filled with \a fill composited
    //! over a checkerboard of \a light and \a dark cells, so a translucent colour can be
    //! previewed. The result is opaque (only the rounded edge is blended), so it does not
    //! depend on what is drawn behind it. \a radius, \a border and \a borderColor are as
    //! for drawRoundedRect. The default approximates it with a flat drawRoundedRect of
    //! \a fill over mix(light,dark).
    virtual void drawCheckerRect(size_t primitiveId, gweni::Rect rect, int zIndex,
        gweni::Color fill, gweni::Color light, gweni::Color dark,
        float radius, float border, gweni::Color borderColor);

    //! Draw a stroked icon glyph (anti-aliased, SDF) in \a color: \a kind 0=checkmark,
    //! 1=chevron; \a direction 0=down 1=up 2=left 3=right (chevron only).
    virtual void drawIcon(size_t primitiveId, gweni::Rect rect, int kind, int direction,
        gweni::Color color, int zIndex) {}

    virtual void drawTexturedRect(const gweni::Texture &texture, size_t primitiveId, gweni::Rect targetRect, int zIndex,
        float u1=0.0f, float v1=0.0f,float u2=1.0f, float v2=1.0f)
    {}

    virtual void showPrimitive(int style, size_t primitiveId) {}
    virtual void hidePrimitive(size_t primitiveId) {}

    virtual void drawMissingImage(size_t primitiveId, gweni::Rect targetRect, int zIndex);

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

    virtual void generateTextPrimitive(size_t &primitiveId) {}
    virtual void releaseTextPrimitive(size_t primitiveId) {}

    virtual void renderText(size_t textId, const gweni::Font &font,
        gweni::Point pos, int zIndex,
        const gweni::String &text);

    virtual void showText(size_t textId) {}
    virtual void hideText(size_t textId) {}

    virtual gweni::Point measureText(const gweni::Font &font,
        const gweni::String &text);

    //! \sect{Render Specialisation}
    //!     No need to implement these functions in your derived class, but
    //!     if you can do them faster than the default implementation it's a
    //!     good idea to.
    virtual void drawLinedRect(size_t *primitiveIds, gweni::Rect rect, int zIndex);
    virtual void drawPixel(size_t *primitiveIds, int x, int y, int zIndex);
    virtual void drawShavedCornerRect(size_t *primitiveIds, gweni::Rect rect, int zIndex, bool bSlight=false);
    //! \}

    //! \sect{Translate}
    //!     Translate a panel's local drawing coordinate
    //!     into view space, taking Offsets into account.
    //!     A point becomes snap(x+offset.x); a rect has both of its edges
    //!     snapped, and its size is the distance between them.
    void translate(int &x, int &y);
    void translate(gweni::Rect &rect);
    //! A canvas coordinate (or length) in device pixels: value*scale(),
    //! rounded half up. Every scaled edge goes through it, so edges that meet
    //! in canvas units meet on the device-pixel grid too.
    int snap(float value) const;
    //! snap() at a given scale, for code that sizes a framebuffer to match.
    static int snap(float value, float scale);
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
