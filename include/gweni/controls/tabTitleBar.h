/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_tabtitlebar_h_
#define _gweni_controls_tabtitlebar_h_

#include <gweni/controls/base.h>
#include <gweni/controls/label.h>
#include <gweni/controls/tabButton.h>
#include <gweni/dragAndDrop.h>
#include <gweni/skin.h>

namespace gweni
{
namespace controls
{
class GWENI_EXPORT TabTitleBar: public Label
{
public:

    GWENI_CONTROL_INLINE(TabTitleBar, Label)
    {
        setMouseInputEnabled(true);
        setTextPadding(gweni::Padding(5, 2, 5, 2));
        setPadding(gweni::Padding(1, 2, 1, 2));
        dragAndDrop_setPackage(true, "TabWindowMove");
    }

    void render(skin::Base *skin) override
    {
//        skin->drawTabTitleBar(this, skin::Generate);
        skin->drawControl(this);
    }

    void dragAndDrop_startDragging(gweni::draganddrop::Package *package, int x, int y) override
    {
        draganddrop::SourceControl=getParent();
        draganddrop::SourceControl->dragAndDrop_startDragging(package, x, y);
    }

    void updateFromTab(TabButton *button)
    {
        setText(button->getText());
        sizeToContents();
    }

private:

};


}
}
#endif//_gweni_controls_tabtitlebar_h_
