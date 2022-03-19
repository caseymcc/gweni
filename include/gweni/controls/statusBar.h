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
        setDock(DockPosition::Bottom);
        setPadding(Padding(2, 2, 2, 2));
        setText("");
        setAlignment(Alignment::Left|Alignment::CenterV);
    }

public:
    virtual void addControl(controls::Base *ctrl, bool right)
    {
        ctrl->setParent(this);
        ctrl->setDock(right?DockPosition::Right:DockPosition::Left);
    }
};


}
}
#endif//_gweni_controls_statusbar_h_
