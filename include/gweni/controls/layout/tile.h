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
        dock(Position::Fill);
        setTileSize(22, 22);
    }

public:
    void postLayout(skin::Base *skin) override
    {
        gweni::Rect bounds=getInnerBounds();
        gweni::Point pos=Point(bounds.x, bounds.y);

        for(auto &&child : getChildren())
        {
            if(child->getDock() != Position::None)
                continue;

            child->setPos(pos.x+m_tileSize.x/2 - child->getWidth()/2,
                pos.y+m_tileSize.y/2 - child->getHeight()/2);
            pos.x=pos.x+m_tileSize.x;

            if(pos.x+m_tileSize.x > bounds.x+bounds.w)
            {
                pos.x=bounds.x;
                pos.y+=m_tileSize.y;
            }
        }
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
