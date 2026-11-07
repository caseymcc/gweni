/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/gweni.h>
#include <gweni/controls/splitterBar.h>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(SplitterBar)
{
    setTarget(this);
    setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    restrictToParent(true);
}

// Kept inside its parent, as a drag keeps it, which is computed, not asked
// for. A bar its owner places (Placement::Owner: CrossSplitter's, Properties')
// is where the owner put it.
void SplitterBar::layout(skin::Base * /*skin*/)
{
    if(getPlacement() == Placement::Owner)
        return;

    const Point pos=restrictedPos(getX(), getY());

    setArrangedBounds(pos.x, pos.y, getWidth(), getHeight());
}

}//namespace controls
}//namespace gweni
