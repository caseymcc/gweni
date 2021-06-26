/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_tabstrip_h_
#define _gweni_controls_tabstrip_h_

#include <gweni/controls/base.h>
#include <gweni/controls/button.h>
#include <gweni/controls/tabButton.h>

namespace gweni
{
namespace controls
{
// class TabControl;

class GWENI_EXPORT TabStrip: public Base
{
public:

    GWENI_CONTROL(TabStrip, Base);

    void layout(skin::Base *skin) override;

    bool dragAndDrop_handleDrop(gweni::draganddrop::Package *package, int x, int y) override;
    bool dragAndDrop_canAcceptPackage(gweni::draganddrop::Package *package) override;

    void dragAndDrop_hoverEnter(gweni::draganddrop::Package *package, int x, int y) override;
    void dragAndDrop_hoverLeave(gweni::draganddrop::Package *package) override;
    void dragAndDrop_hover(gweni::draganddrop::Package *package, int x, int y) override;

    virtual void setTabPosition(Position pos);

    virtual void setAllowReorder(bool b)
    {
        m_allowReorder=b;
    }

    virtual bool allowTabReorder()
    {
        return m_allowReorder;
    }

    bool shouldClip() override
    {
        return false;
    }

private:

    Base *m_tabDragControl;
    bool m_allowReorder;

};


}
}
#endif//_gweni_controls_tabstrip_h_
