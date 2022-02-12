/*
  * Gweni
  * Copyright (c) 2011 Facepunch Studios

  * See license in gweni.h
 */


#ifndef _gweni_controls_toolbar_h_
#define _gweni_controls_toolbar_h_

#include <gweni/platforms/baseRender.h>
#include <gweni/controls/base.h>
#include <gweni/skin.h>

namespace gweni
{
namespace controls
{
class ToolBarButton: public gweni::controls::Button
{
    GWENI_CONTROL_INLINE(ToolBarButton, gweni::controls::Button)
    {
        setSize(20, 20);
        dock(Position::Left);
    }

public:
    bool shouldDrawBackground() override
    {
        return isHovered();
    }

};


class ToolBarStrip: public Base
{
    GWENI_CONTROL_INLINE(ToolBarStrip, Base)
    {
        setSize(25, 25);
        setPadding(Padding(2, 2, 2, 2));
    }

public:
    virtual ToolBarButton *add(const String &text, const String &icon)
    {
        ToolBarButton *button=new ToolBarButton(this);
        button->setToolTipText(text);
        button->setImage(icon);
        return button;
    }

};


}

}
#endif//_gweni_controls_toolbar_h_
