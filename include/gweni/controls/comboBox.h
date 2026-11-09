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
class ComboBox;

namespace internal
{

//! A ComboBox's list: a Menu that tells its box when an item leaves it
//! (deleted outright or by the canvas, cleared, or given another parent), so
//! the box never holds an item that is gone. Drawn by Menu's renderer, the
//! skin's lookup walking up the parent types.
class GWENI_EXPORT ComboBoxList: public Menu
{
public:

    GWENI_CONTROL(ComboBoxList, Menu);

public:
    //! Clears its box's pointer to it: a canvas tearing down can delete the
    //! list before the box.
    ~ComboBoxList() override;

    void setComboBox(ComboBox *comboBox)
    {
        m_comboBox=comboBox;
    }

protected:

    //! Runs from inside the item's ~Base when it is deleted, so the item is
    //! only compared, never read. Not reached while the list itself is
    //! deleted: its items then leave from its ~Base, past this override.
    void onChildRemoved(controls::Base *child) override;

    ComboBox *m_comboBox=nullptr;
};

}//namespace internal

class GWENI_EXPORT ComboBox: public Button
{
public:

    GWENI_CONTROL(ComboBox, Button);

public:
    //! The list is not its child once it has been opened: clears the list's
    //! pointer back to it.
    ~ComboBox() override;

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

    friend class internal::ComboBoxList;

    //! \p child left the list (it may be inside its ~Base): the selection is
    //! dropped if it was the selected item, with no onSelection, as
    //! clearItems() drops it, and the box is measured again.
    void itemRemoved(controls::Base *child);
    //! The list is being deleted (before this box): it is no longer held.
    void listDeleted();

    //! The widest of the items' texts in this box's font, measured again only
    //! for the texts (or the font, the renderer's text layout) that changed
    //! since the last call.
    int itemsTextWidth();

    internal::ComboBoxList *m_menu=nullptr;
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
