/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_combobox_h_
#define _gweni_controls_combobox_h_

#include <gweni/controls/base.h>
#include <gweni/controls/button.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>
#include <gweni/controls/textBox.h>
#include <gweni/controls/menu.h>


namespace gweni
{
namespace controls
{
class GWENI_EXPORT ComboBox: public Button
{
public:

    GWENI_CONTROL(ComboBox, Button);

    void render(skin::Base *skin) override;
    void layout(skin::Base *skin) override;
    void updateColors() override;

    virtual void selectItem(MenuItem *item, bool fireChangeEvents=
        true);
    virtual void selectItemByName(const gweni::String &name,
        bool fireChangeEvents=true);
    virtual gweni::controls::Label *getSelectedItem();

    void onPress(event::Info info) override;
    virtual void onItemSelected(event::Info info);
    virtual void openList();
    virtual void closeList();

    virtual void clearItems();

    virtual MenuItem *addItem(const String &label, const String &name="");
    bool onKeyUp(bool down) override;
    bool onKeyDown(bool down) override;

    void renderFocus(gweni::skin::Base *skin) override;
    void onLostKeyboardFocus() override;
    void onKeyboardFocus() override;

    virtual bool isMenuOpen();

    bool isMenuComponent() override
    {
        return true;
    }

    gweni::event::Caller onSelection;

protected:

    Menu *m_menu;
    MenuItem *m_selectedItem;

    controls::Base *m_button;

};


}
}
#endif//_gweni_controls_combobox_h_
