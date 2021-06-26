/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_menustrip_h_
#define _gweni_controls_menustrip_h_

#include <gweni/platforms/baseRender.h>
#include <gweni/controls/base.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/menuItem.h>

namespace gweni
{
namespace controls
{
class GWENI_EXPORT MenuStrip: public Menu
{
    GWENI_CONTROL(MenuStrip, Menu);

    void render(skin::Base *skin) override;
    void renderUnder(skin::Base * /*skin*/) override {}

    void layout(skin::Base *skin) override;

protected:

    void onAddItem(MenuItem *item) override;
    bool shouldHoverOpenMenu() override;
    void close() override {}

};


}

}
#endif//_gweni_controls_menustrip_h_
