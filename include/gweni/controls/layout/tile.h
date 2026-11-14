/*
  * Gweni
  * Copyright (c) 2012 Facepunch Studios

  * See license in gweni.h
 */


#ifndef _gweni_controls_layout_tile_h_
#define _gweni_controls_layout_tile_h_

#include <gweni/controls/base.h>

#include <algorithm>

namespace gweni
{
namespace controls
{
namespace layout
{
class GWENI_EXPORT Tile: public Base
{
public:

    GWENI_CONTROL_INLINE(Tile, Base)
    {
        setDock(DockPosition::Center);
        setTileSize(22, 22);
    }

public:
    //! Layout only: it positions its children.
    GWENI_DRAWS_NOTHING

    //! Its children are its own to place (Placement::Owner), each at its own
    //! size, centred in a cell of the tile size: left to right from the padded
    //! content box's corner, a new row once a cell would pass its width.
    //! Hidden children take no cell. Placed along each axis before the
    //! children are arranged, so a move leaves nothing dirty after the pass.
    //! The width is final in the X arrange, and the rows depend on it alone.
    void arrange(skin::Base *skin, Dim dim) override
    {
        const Padding &padding=getPadding();
        const int right=getWidth()-padding.right;
        gweni::Point pos=Point(padding.left, padding.top);

        for(auto &&child : getChildren())
        {
            if(child->hidden())
                continue;

            const Rect &now=child->getBounds();

            if(dim == Dim::X)
                placeChildPos(child, pos.x+m_tileSize.x/2-now.w/2, now.y);
            else
                placeChildPos(child, now.x, pos.y+m_tileSize.y/2-now.h/2);
            pos.x=pos.x+m_tileSize.x;

            if(pos.x+m_tileSize.x > right)
            {
                pos.x=padding.left;
                pos.y+=m_tileSize.y;
            }
        }

        ParentClass::arrange(skin, dim);
    }

    //! X: one tile and the padding. Y: the rows at the width the X arrange
    //! gave it (the cells a row holds, columns(), over the shown children)
    //! times the tile height, and the padding, so a ScrollControl around it
    //! scrolls to its last row. A Y measure before this pass's X arrange (a
    //! ScrollControl's, at the old width) keeps the last height.
    void calculateSize(skin::Base *skin, Dim dim) override
    {
        const Padding &padding=getPadding();

        if(dim == Dim::X)
        {
            ParentClass::calculateSize(skin, dim);
            m_preferredSize.width=preferredFromContent(m_tileSize.x+padding.left+padding.right, Dim::X);
            return;
        }
        // before the parent's measure, which would set the height to the
        // padding alone (as Flow)
        if(inLayoutPass() && !laidOutThisPass(Dim::X))
            return;
        ParentClass::calculateSize(skin, dim);

        int shown=0;

        for(auto &&child : getChildren())
        {
            if(!child->hidden())
                ++shown;
        }

        const int columns=this->columns();
        const int rows=(shown+columns-1)/columns;

        m_preferredSize.height=preferredFromContent(rows*m_tileSize.y+padding.top+padding.bottom, Dim::Y);
    }

    //! The cells a row holds at the width now, as arrange() wraps them: a new
    //! row once a cell would pass the content box, at least one a row
    int columns() const
    {
        const Padding &padding=getPadding();
        const int content=getWidth()-padding.left-padding.right;

        if(m_tileSize.x <= 0)
            return 1;
        return std::max(1, content/m_tileSize.x);
    }

    void onChildAdded(Base *child) override
    {
        child->setPlacement(Placement::Owner);
        ParentClass::onChildAdded(child);
    }

    void onChildRemoved(Base *child) override
    {
        child->setPlacement(Placement::Parent);
        ParentClass::onChildRemoved(child);
    }

    void setTileSize(int x, int y)
    {
        if(m_tileSize.x == x && m_tileSize.y == y)
            return;
        m_tileSize=Point(x, y);
        invalidate();
    }

private:

    Point m_tileSize{0, 0};
};


}
}
}

#endif//_gweni_controls_layout_tile_h_
