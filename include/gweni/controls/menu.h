/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_menu_h_
#define _gweni_controls_menu_h_

#include <gweni/platforms/baseRender.h>
#include <gweni/controls/base.h>
#include <gweni/controls/menuItem.h>
#include <gweni/controls/scrollControl.h>

namespace gweni
{
namespace controls
{
class MenuItem;

class GWENI_EXPORT Menu: public ScrollControl
{
public:

    GWENI_CONTROL(Menu, ScrollControl);

public:
    //void render(skin::Base *skin) override;
    void renderUnder(skin::Base *skin) override;

    void layout(skin::Base *skin) override;

    virtual MenuItem *addItem(const String &name,
        const String &strIconName="",
        const String &strAccelerator="");

    virtual void addDivider();

    void onHoverItem(event::Info info);
    void closeAll();
    bool isMenuOpen();
    void clearItems();

    virtual void open(Alignment pos);
    virtual void close();

    bool isMenuComponent() override
    {
        return true;
    }

    void closeMenus() override;

    bool iconMarginDisabled()
    {
        return m_disableIconMargin;
    }

    void setDisableIconMargin(bool bDisable)
    {
        m_disableIconMargin=bDisable;
    }

    bool deleteOnClose()
    {
        return m_deleteOnClose;
    }

    void setDeleteOnClose(bool b)
    {
        m_deleteOnClose=b;
    }

protected:

    virtual bool shouldHoverOpenMenu()
    {
        return true;
    }

    virtual void onAddItem(MenuItem *item);

    bool m_disableIconMargin;
    bool m_deleteOnClose;
};


class GWENI_EXPORT MenuDivider: public Base
{
public:

    GWENI_CONTROL_INLINE(MenuDivider, Base)
    {
        setHeight(1);
    }

public:
    //void render(gweni::skin::Base *skin) override;
};


}

}
#endif//_gweni_controls_menu_h_
