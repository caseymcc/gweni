/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_dockBase_h_
#define _gweni_dockBase_h_

#include <gweni/controls/base.h>
#include <gweni/controls/button.h>

namespace gweni
{
namespace controls
{

class DockedTabControl;
class TabControl;

class GWENI_EXPORT DockBase: public Base
{
public:
    GWENI_CONTROL(DockBase, Base);

public:
    void render(skin::Base *skin) override;
    void renderOver(skin::Base *skin) override;
    virtual bool isEmpty();

    virtual TabControl *getTabControl();

    virtual DockBase *getRight() { return getChildDock(Position::Right); }
    virtual DockBase *getLeft() { return getChildDock(Position::Left); }
    virtual DockBase *getTop() { return getChildDock(Position::Top); }
    virtual DockBase *getBottom() { return getChildDock(Position::Bottom); }

    // No action on space (default button action is to press)
    bool onKeySpace(bool /*down*/) override { return false; }

private:
    // Drag n Drop
    bool dragAndDrop_handleDrop(gweni::draganddrop::Package *package, int x, int y) override;
    bool dragAndDrop_canAcceptPackage(gweni::draganddrop::Package *package) override;
    void dragAndDrop_hoverEnter(gweni::draganddrop::Package *package, int x, int y) override;
    void dragAndDrop_hoverLeave(gweni::draganddrop::Package *package) override;
    void dragAndDrop_hover(gweni::draganddrop::Package *package, int x, int y) override;

    virtual void setupChildDock(Position pos);

    virtual void doRedundancyCheck();
    virtual void doConsolidateCheck();
    virtual void onRedundantChildDock(DockBase *dockBase);

    virtual Position getDroppedTabDirection(int x, int y);
    virtual void onTabRemoved(event::Info);

    DockBase *getChildDock(Position pos);
    DockBase **getChildDockPtr(Position pos);

    DockBase *m_left;
    DockBase *m_right;
    DockBase *m_top;
    DockBase *m_bottom;

    // Only CHILD dockpanels have a tabcontrol.
    DockedTabControl *m_dockedTabControl;

    bool m_drawHover;
    bool m_dropFar;
    gweni::Rect m_hoverRect;

};


}//namespace controls
}//namespace gweni

#endif//_gweni_dockBase_h_
