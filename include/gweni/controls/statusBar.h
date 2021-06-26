/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_statusbar_h_
#define _gweni_controls_statusbar_h_

#include <gweni/gweni.h>
#include <gweni/controls/label.h>

namespace gweni
{
namespace controls
{
class StatusBar: public controls::Label
{
public:

    GWENI_CONTROL_INLINE(StatusBar, controls::Label)
    {
        setHeight(22);
        dock(Position::Bottom);
        setPadding(Padding(2, 2, 2, 2));
        setText("");
        setAlignment(Position::Left|Position::CenterV);
    }

    virtual void addControl(controls::Base *ctrl, bool right)
    {
        ctrl->setParent(this);
        ctrl->dock(right?Position::Right:Position::Left);
    }

    void render(skin::Base *skin) override
    {
//        skin->drawStatusBar(this, skin::Generate);
        skin->drawControl(this);
    }

};


}
}
#endif//_gweni_controls_statusbar_h_
