/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios

 *  See license in Gweni.h
 */


#ifndef _gweni_baserender_h_
#define _gweni_baserender_h_

#include <gweni/platforms/platformTypes.h>

#include <cstdint>
#include <vector>

namespace gweni
{
struct Font;
struct Texture;
class WindowProvider;

namespace renderer
{
class Base;

//! What a renderer's last frame wrote and uploaded, in terms any backend can
//! give (Base::frameStats()). A retained renderer counts a slot as written when
//! the frame changed its bytes, so a settled frame writes 0; a renderer that
//! does not count reports counted=false and every field 0.
struct FrameCounts
{
    bool counted=false;             //!< false: this renderer does not count
    size_t frameIndex=0;            //!< frames ended so far
    size_t primitiveSlotsWritten=0; //!< primitive slots (ids) the frame wrote
    size_t textSlotsWritten=0;      //!< glyph slots the frame wrote, every text id's together
    size_t uploadCalls=0;           //!< transfers to the device, 0 for a renderer with none
    size_t uploadBytes=0;           //!< bytes those transfers sent
    size_t drawCalls=0;             //!< draws the frame issued, 0 for a renderer that does not count them
    //! how long its end() (a retained renderer's flush: the diff, the uploads,
    //! the draws and the submit) took, on platform::profileTimeMicros(); 0 for a
    //! renderer that does not time it
    uint64_t flushMicros=0;
};

//! A text id whose run had glyph slots written in the last frame, and how many
struct WrittenText
{
    size_t textId=0;
    size_t glyphSlots=0;

    bool operator==(const WrittenText &other) const
    {
        return (textId == other.textId) && (glyphSlots == other.glyphSlots);
    }
};

//! drawRoundedRect's edge mask: which corners are square and which sides have no
//! border. 0, the default, is a rect rounded at all four corners with a border on
//! all four sides. A square corner is the corner of the rect itself (its
//! quadrant's radius 0); a side without a border is the fill up to that edge, the
//! border of the sides next to it ending on it. A tab joined to its page is
//! square on the page's side and has no border there.
enum RoundedEdge: uint8_t
{
    SquareTopLeft=0x01,
    SquareTopRight=0x02,
    SquareBottomLeft=0x04,
    SquareBottomRight=0x08,
    NoBorderTop=0x10,
    NoBorderRight=0x20,
    NoBorderBottom=0x40,
    NoBorderLeft=0x80
};

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

    //! The largest z index a control may be given: the draw calls' \a zIndex
    //! (plus the control's depth and overlay layers) must stay in [0, maxZIndex()],
    //! where a higher z is drawn in front. The canvas numbers within it.
    virtual int maxZIndex() const { return 10000; }

    //! What the last frame (the last end()) wrote. The default does not count:
    //! counted is false and every field 0.
    virtual FrameCounts frameStats() const { return FrameCounts(); }
    //! The primitive ids whose slots the last frame wrote, ascending, each once:
    //! frameStats().primitiveSlotsWritten of them. A released id the frame
    //! cleared is among them. Empty when the renderer does not count.
    virtual const std::vector<size_t> &writtenPrimitiveIds() const;
    //! The text ids whose glyph slots the last frame wrote, ascending, each once,
    //! with how many of its slots: their glyphSlots add up to
    //! frameStats().textSlotsWritten. A slot is reported under the text id whose
    //! run held it last, a released one's included (one no run ever held, under
    //! ~size_t(0)). Empty when the renderer does not count.
    virtual const std::vector<WrittenText> &writtenTextIds() const;

    //! Get ResourceLoader we are using for the renderer.
    IResourceLoader &getLoader() { return *reinterpret_cast<IResourceLoader *>(this); }
    ResourcePaths &getResourcePaths() { return m_paths; }

    //! Set the current drawing color.
    virtual void setDrawColor(Color color) {}

    virtual void startClip() {}     //!< Start clipping the drawing. \see setClipRegion().
    virtual void endClip() {}       //!< Stop clipping the drawing.

    void    setClipRegion(gweni::Rect const &rect) { m_rectClipRegion=rect; }
    void    addClipRegion(gweni::Rect rect);
    bool    clipRegionVisible();
    const gweni::Rect &clipRegion() const { return m_rectClipRegion; }

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

    //! drawRoundedRect with an edge mask, \a edges a combination of RoundedEdge:
    //! the corners it names square, the sides it names without a border. The
    //! default ignores the mask and draws the rect drawRoundedRect does, so a
    //! backend without it still draws a rounded rect in the right place.
    virtual void drawRoundedRect(size_t primitiveId, gweni::Rect rect, float radius, float border,
        gweni::Color fillTop, gweni::Color fillBottom, gweni::Color borderColor, int zIndex,
        uint8_t edges)
    {
        drawRoundedRect(primitiveId, rect, radius, border, fillTop, fillBottom, borderColor, zIndex);
    }

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
    //! 1=chevron, 2=cross (x), 3=dash, 4=square outline, 5=restore (a square outline
    //! with a second one behind it, offset up and right), 6=rule (a separator line);
    //! \a direction 0=down 1=up 2=left 3=right (chevron only), and for the rule
    //! 0=horizontal 1=vertical. Kinds 2-5 fill the square centred in \a rect, and the
    //! rule runs the whole length of \a rect through its middle; all are stroked
    //! max(1, snap(1)) device px wide, and the strokes of 3-6 are whole rows and
    //! columns of fully covered pixels, so a rule is as thick wherever it lands on the
    //! pixel grid. A pixel the glyph does not cover writes nothing, depth included.
    virtual void drawIcon(size_t primitiveId, gweni::Rect rect, int kind, int direction,
        gweni::Color color, int zIndex) {}

    //! Draw a line segment from \a p0 to \a p1 in the current color (setDrawColor):
    //! a capsule (round caps) \a thickness wide, anti-aliased, in one primitive.
    //! The endpoints are in canvas units and may be fractional; the thickness is
    //! snapped like a border, max(1, snap(thickness)) whole device px, and the stroke
    //! is placed so a horizontal or vertical segment covers whole rows (columns)
    //! fully and nothing beside them. A pixel the line does not cover writes nothing,
    //! depth included. Segments that share an endpoint join without a gap (their caps
    //! overlap). p0 == p1 draws a disc.
    //!
    //! \a prev and \a next, when given, are the polyline's points before p0 and
    //! after p1: the segments prev-p0 and p1-next are its neighbours, drawn with the
    //! same thickness, z and colour. A backend that knows them blends each pixel
    //! where neighbours overlap once, so a translucent polyline is no darker at its
    //! joints; without them, or for segments farther apart, the overlap is blended
    //! as the depth test lets it (a translucent joint can be darker). A caller that
    //! hides or removes a neighbour draws this segment again without it. A segment
    //! drawn again with other neighbours is a changed slot.
    //!
    //! The default is an approximation for a backend without it: a filled rect along
    //! the segment's longer axis, its extent on that axis and \a thickness across,
    //! centred on the segment's middle; exact only for an axis-aligned segment
    //! (without the caps). It ignores the neighbours.
    virtual void drawLine(size_t primitiveId, gweni::PointF p0, gweni::PointF p1,
        float thickness, int zIndex, const gweni::PointF *prev=nullptr,
        const gweni::PointF *next=nullptr);

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

    //! Draw \a text into the run of \a textId, in front of \a zIndex and
    //! behind \a zIndex+1: a rect drawn at \a zIndex is behind every glyph.
    //! \a pos is the top-left of the first line's box, in canvas units like
    //! the rects: a line box is ceil(font->height) high (ascender-descender,
    //! no line gap), and the first baseline is at pos.y+ascender. Each '\n'
    //! starts a line one line step below the one before, at pos.x; the whole
    //! run is the box measureText(font, text) gives. A control puts \a pos
    //! where it wants the box; the renderer adds no ascent of its own.
    virtual void renderText(size_t textId, const gweni::Font &font,
        gweni::Point pos, int zIndex,
        const gweni::String &text);

    virtual void showText(size_t textId) {}
    virtual void hideText(size_t textId) {}

    //! The size \a text takes in \a font, in canvas units: the same at every
    //! scale(), as the layout that uses it is. A string with '\n's is measured
    //! as renderText draws it, on as many lines: its widest line's width,
    //! rounded up, and one line box plus one line step (the distance from one
    //! baseline to the next) per further line, rounded up; a trailing '\n'
    //! is an empty last line. A one-line string is lineHeight() high. The
    //! default here, a rough estimate, is one line whatever the string.
    //! A control keeps what it measured
    //! until its string or font changes or textLayoutSerial() is raised
    //! (internal::Text), so a measure that changes for any other reason must
    //! raise that serial.
    virtual gweni::Point measureText(const gweni::Font &font,
        const gweni::String &text);

    //! The height of one line box in \a font, in the units of measureText:
    //! its `y` for any one-line string, which does not depend on the string,
    //! and the same at every scale. The default measures " "; a renderer that
    //! can read the font's metric instead should.
    virtual int lineHeight(const gweni::Font &font);

    //! \sect{Render Specialisation}
    //!     No need to implement these functions in your derived class, but
    //!     if you can do them faster than the default implementation it's a
    //!     good idea to.

    //! The x of every caret position in \a text, in the units of measureText:
    //! \a xs gets one entry per codepoint boundary (utf8Next's walk), 0 and
    //! text.size() included, so codepoint count + 1 entries. xs[0] is 0 and
    //! xs[k] is measureText(font, text.substr(0, boundary k)).x. The default
    //! is that definition, one measureText per prefix; a renderer that can lay
    //! the string out once should.
    virtual void measureCarets(const gweni::Font &font, const gweni::String &text,
        std::vector<int> &xs);
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

    //! Raised whenever text drawn or measured before may draw or measure otherwise
    //! now, for a reason other than the scale (SlotRenderer::setTextAA). A canvas that
    //! sees it change lays out and renders its whole tree again, as for a new scale.
    unsigned textLayoutSerial() const { return m_textLayoutSerial; }

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

    //! Every text run laid out so far is stale: raises textLayoutSerial()
    void textLayoutChanged() { ++m_textLayoutSerial; }

    float m_scale;

private:

    unsigned m_textLayoutSerial=0;

    ResourcePaths &m_paths;
    gweni::Point m_renderOffset;
    gweni::Rect m_rectClipRegion;
    ICacheToTexture *m_RTT;
};

}
}
#endif //_gweni_baserender_h_
