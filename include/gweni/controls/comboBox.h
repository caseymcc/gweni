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

#include <vector>


namespace gweni
{
namespace controls
{
class GWENI_EXPORT ComboBox: public Button
{
public:

    GWENI_CONTROL(ComboBox, Button);

public:
    //! The down chevron the skin paints on the right, ChevronInset from the
    //! edge: the box keeps that column as its right padding, so it is part of
    //! the content size and the text is never laid out under it.
    static constexpr int ChevronSize=12;
    static constexpr int ChevronInset=6;
    //! Above and below the text line.
    static constexpr int VerticalPadding=2;

    void render(skin::Base *skin) override;
    void layout(skin::Base *skin) override;
    //! In X, the widest item's text in this box's font as well as the
    //! selected text, with the text padding and the chevron column: the width
    //! fits every item, so choosing another one does not change it. The item
    //! texts are measured directly, not through the hidden list, and only when
    //! one of them or the font changed. With no items the width is the
    //! Button's, from the shown text.
    void calculateSize(skin::Base *skin, Dim dim) override;
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

    //! The widest of the items' texts in this box's font, measured again only
    //! for the texts (or the font, the renderer's text layout) that changed
    //! since the last call.
    int itemsTextWidth();

    Menu *m_menu=nullptr;
    MenuItem *m_selectedItem;

    //! What itemsTextWidth() measured: each item's text and width, in order,
    //! and the font and text layout they were measured in.
    std::vector<String> m_itemTexts;
    std::vector<int> m_itemWidths;
    gweni::Font m_itemsFont;
    const renderer::Base *m_itemsRenderer=nullptr;
    unsigned m_itemsSerial=0;

    controls::Base *m_button;

};


}
}
#endif//_gweni_controls_combobox_h_
