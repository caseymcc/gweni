/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_modal_h_
#define _gweni_controls_modal_h_

#include <gweni/controls/base.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>
#include <gweni/controls/canvas.h>


namespace gweni
{
namespace controls
{
namespace internal
{

class Modal: public controls::Base
{
    GWENI_CONTROL_INLINE(Modal, controls::Base)
    {
        setKeyboardInputEnabled(true);
        setMouseInputEnabled(true);
        setShouldDrawBackground(true);
        setBounds(0, 0, getParent()->getWidth(), getParent()->getHeight());

        // recurseLayout() may not be called before Position is set.
        m_innerBounds=m_bounds;
    }

    void layout(skin::Base * /*skin*/) override
    {
        setBounds(0, 0, getCanvas()->getWidth(), getCanvas()->getHeight());
    }

    void render(skin::Base *skin) override
    {
//        skin->drawModalControl(this, skin::Generate);
        skin->drawControl(this);
    }

};


}//namespace internal
}//namespace controls
}//namespace gweni}

#endif//_gweni_controls_modal_h_
