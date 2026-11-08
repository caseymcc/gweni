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

public:
    void calculateSize(skin::Base *skin, Dim dim) override;

    bool dragAndDrop_handleDrop(gweni::draganddrop::Package *package, int x, int y) override;
    bool dragAndDrop_canAcceptPackage(gweni::draganddrop::Package *package) override;

    void dragAndDrop_hoverEnter(gweni::draganddrop::Package *package, int x, int y) override;
    void dragAndDrop_hoverLeave(gweni::draganddrop::Package *package) override;
    void dragAndDrop_hover(gweni::draganddrop::Package *package, int x, int y) override;

    virtual void setTabPosition(DockPosition pos);

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

    //! Units the row of tabs is scrolled along a Top or Bottom strip: the tabs
    //! are placed that much left of where they would be, so the first ones
    //! leave the strip and later ones come into it (TabControl's overflow
    //! buttons). It moves the tabs only: the strip's preferred size is the
    //! whole row's, at any offset. Set by its TabControl as it lays the strip
    //! out; a strip on the left or right never scrolls.
    void setScrollOffset(int offset);
    int getScrollOffset() const { return m_scrollOffset; }

protected:

    //! Places the row \p m_scrollOffset units left of the padding.
    void arrangeHorizontal(skin::Base *skin) override;

private:

    //! Docks each TabButton and sets its margin for its place in the row.
    void placeButtons();

    Base *m_tabDragControl;
    bool m_allowReorder;
    int m_scrollOffset=0;

};


}
}
#endif//_gweni_controls_tabstrip_h_
