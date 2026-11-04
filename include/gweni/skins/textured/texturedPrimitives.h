/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios

 *  See license in Gweni.h
 */


#ifndef _gweni_skins_texturing_h_
#define _gweni_skins_texturing_h_

#include "gweni/gweni.h"


namespace gweni
{
namespace skin
{
namespace textured
{

class TexturedSkin;

//! Handle drawing a single textured rectangle.
//
struct Primitive
{
    Primitive();
    ~Primitive();

    static size_t requiredPrimitives() { return 1; }

    void init(Texture *texture, const TextureData &texData, float x, float y, float w, float h);

//    void generate(renderer::Base *render, controls::Base *control);

    void draw(renderer::Base *render, controls::Base *control, Rect r, int zIndex, const Color &col=colors::White);

    void drawCenter(renderer::Base *render, controls::Base *control, Rect r, int zIndex, const Color &col=colors::White);

//    void remove(renderer::Base *render, controls::Base *control);

    Texture *m_texture;  //!< Texture to use.
    float m_uv[4];      //!< Rectangle texture UVs: (TL.xy, BR.xy).
    int m_width;     //!< Width of draw rectangle.
    int m_height;    //!< Height of draw rectangle.
};

//! Handle drawing a skinned textured rectangle. This consists of nine areas:
//!
//!  || Areas: ||
//!  | :-: | :-: | :-: |
//!  |  0  |  1  |  2  |
//!  |  3  |  4  |  5  |
//!  |  6  |  7  |  8  |
//!
//
struct BorderedPrimitive
{
    BorderedPrimitive();
    ~BorderedPrimitive();

    static size_t requiredPrimitives() { return 9; }

    void init(TexturedSkin *skin, size_t id, float drawMarginScale=1.0f);
    void init(Texture *texture, Rect &rect, BoundingBox &margin, float drawMarginScale=1.0f);

    void setRect(int num, float x, float y, float w, float h);

    void show(renderer::Base *render, size_t *primitiveIds, bool visible);
//    void generate(renderer::Base *render, controls::Base *control);
    //
    //! Draw a segmented, textured rectangle for skinning.
    //!
    //! \param render - Renderer to use.
    //! \param r - Rectangle to draw; outer edge.
    //! \param col - Color. Tint?
    //! \param draw - Bitfield specifying segments to draw.
    //
    void draw(renderer::Base *render, size_t *primitiveIds, const Rect &r, int zIndex,
        const Color &col=colors::White, unsigned int draw=~0);

    void drawRect(renderer::Base *render, size_t primitiveId, size_t i, int x, int y, int w, int h, int zIndex);
//    void remove(renderer::Base *render, controls::Base *control);

    Texture *m_texture;
//    TextureData m_texData;

    struct SubRect
    {
        float m_uv[4];
    };

    SubRect m_rects[9];
    BoundingBox m_margin;

    float m_width;
    float m_height;
};

//! Handle drawing a single anti-aliased rounded rectangle with a vertical fill
//! gradient and an optional border, evaluated in the fragment shader (SDF). One
//! primitive id == one rounded rect (a single quad), unlike the 9-slice
//! BorderedPrimitive. Backed by renderer::Base::drawRoundedRect.
struct RoundedPrimitive
{
    static size_t requiredPrimitives() { return 1; }

    float m_radius=6.0f;   //!< Corner radius in px.
    float m_border=1.0f;   //!< Border thickness in px (0 == no border).

    void set(float radius, float border) { m_radius=radius; m_border=border; }

    //! Flat fill (no gradient, no border-color difference): pass one color.
    void draw(renderer::Base *render, size_t *primitiveIds, const Rect &r, int zIndex,
        const Color &fill, const Color &border)
    {
        draw(render, primitiveIds, r, zIndex, fill, fill, border);
    }

    //! Gradient fill from \a fillTop (top) to \a fillBottom (bottom) with \a border.
    void draw(renderer::Base *render, size_t *primitiveIds, const Rect &r, int zIndex,
        const Color &fillTop, const Color &fillBottom, const Color &border)
    {
        if(!primitiveIds)
            return;
        render->drawRoundedRect(primitiveIds[0], r, m_radius, m_border, fillTop, fillBottom, border, zIndex);
    }

    void show(renderer::Base *render, size_t *primitiveIds, bool visible)
    {
        if(!primitiveIds)
            return;
        render->showPrimitive(visible?4:0, primitiveIds[0]);
    }
};

}//namespace textured
}//namespace skin
}//namespace gweni

#endif
