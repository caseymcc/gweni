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

//! Handle drawing a single textured rectangle.
//
struct Primitive
{
    Primitive();
    ~Primitive();

    static size_t requiredPrimitives() { return 1; }

    void init(Texture *texture, const TextureData &texData, float x, float y, float w, float h);

    void generate(renderer::Base *render, controls::Base *control);

    void draw(renderer::Base *render, controls::Base *control, Rect r, const Color &col=colors::White);

    void drawCenter(renderer::Base *render, controls::Base *control, Rect r, const Color &col=colors::White);

    void remove(renderer::Base *render, controls::Base *control);

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

    void init(Texture *texture, const TextureData &texData, float x, float y, float w, float h, Margin in_margin, float drawMarginScale=1.0f);

    void setRect(int num, float x, float y, float w, float h);

    void generate(renderer::Base *render, controls::Base *control);
    //
    //! Draw a segmented, textured rectangle for skinning.
    //!
    //! \param render - Renderer to use.
    //! \param r - Rectangle to draw; outer edge.
    //! \param col - Color. Tint?
    //! \param draw - Bitfield specifying segments to draw.
    //
    void draw(renderer::Base *render, controls::Base *control, Rect r,
        const Color &col=colors::White, unsigned int draw=~0);

    void drawRect(renderer::Base *render, PrimitiveIds &primitiveIds, int i, int x, int y, int w, int h);

    void remove(renderer::Base *render, controls::Base *control);

    Texture *m_texture;
    TextureData m_texData;

    struct SubRect
    {
        float m_uv[4];
    };

    SubRect m_rects[9];
    Margin m_margin;
    float m_width;
    float m_height;
};

}//namespace textured
}//namespace skin
}//namespace gweni

#endif
