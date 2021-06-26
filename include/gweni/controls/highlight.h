/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_highlight_h_
#define _gweni_controls_highlight_h_

#include <gweni/platforms/baseRender.h>
#include <gweni/controls/base.h>
#include <gweni/skin.h>

namespace gweni
{
namespace controls
{
namespace internal
{

class GWENI_EXPORT Highlight: public controls::Base
{
public:

    GWENI_CONTROL_INLINE(Highlight, controls::Base)
    {
    }

    void render(skin::Base *skin) override
    {
//        skin->drawHighlight(this, skin::Generate);
        skin->drawControl(this);
    }

};


}//namespace internal
}//namespace controls
}//namespace gweni}

#endif//_gweni_controls_highlight_h_
