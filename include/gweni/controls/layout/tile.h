/*
  * Gweni
  * Copyright (c) 2012 Facepunch Studios

  * See license in gweni.h
 */


#ifndef _gweni_controls_layout_tile_h_
#define _gweni_controls_layout_tile_h_

#include <gweni/controls/base.h>

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
        m_tileSize=Point(x, y);
    }

private:

    Point m_tileSize;
};


}
}
}

#endif//_gweni_controls_layout_tile_h_
