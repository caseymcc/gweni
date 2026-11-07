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

public:
    //! An item's padding above and below its caption, which makes the strip's
    //! height: 29 for 15-unit text.
    static constexpr int StripItemInset=7;

    //void render(skin::Base *skin) override;
    void renderUnder(skin::Base * /*skin*/) override {}

    void layout(skin::Base *skin) override;

protected:

    void onAddItem(MenuItem *item) override;
    //! A strip is as wide and as high as its items and never scrolls them: its
    //! size request (200x22) is only the least it is given.
    bool sizedByRequest(Dim /*dim*/) const override { return false; }
    bool shouldHoverOpenMenu() override;
    void close() override {}

};


}

}
#endif//_gweni_controls_menustrip_h_
