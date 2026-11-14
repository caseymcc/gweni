/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#ifndef _gweni_splitbutton_example_h_
#define _gweni_splitbutton_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class DropDownButton;
class Label;
class MenuItem;
class SplitButton;
}

//! Buttons that open menus: a DropDownButton whose menu has items, a divider,
//! a disabled item, a checkable item and a submenu, and an icon-only one; a
//! SplitButton "Paste" (Paste / Paste special / Paste as text), a "Run" one
//! whose last chosen item becomes its main action (setMainItem), a toggle one
//! and an icon-only one; a disabled DropDownButton and SplitButton; and a
//! label naming the last action.
class SplitButtonDemo: public controls::Base
{
public:
    GWENI_CONTROL(SplitButtonDemo, controls::Base);

public:
    //! The controls a test drives.
    controls::DropDownButton *optionsButton() const { return m_options; }
    controls::DropDownButton *iconDropDown() const { return m_iconDropDown; }
    controls::SplitButton *pasteButton() const { return m_paste; }
    controls::SplitButton *runButton() const { return m_run; }
    controls::SplitButton *toggleButton() const { return m_toggle; }
    controls::MenuItem *recentItem() const { return m_recent; }
    controls::Label *actionLabel() const { return m_action; }

    //! Whether an action sets the label's text (on by default): a test
    //! weighing a button's or a menu's own cost turns it off, since the
    //! label's new text is laid out.
    void setLabelUpdates(bool updates);

private:
    void onItemSelected(event::Info info);
    void onRunItemSelected(event::Info info);
    void onSplitPressed(event::Info info);
    void showAction(const String &action);

    controls::DropDownButton *m_options=nullptr;
    controls::DropDownButton *m_iconDropDown=nullptr;
    controls::SplitButton *m_paste=nullptr;
    controls::SplitButton *m_run=nullptr;
    controls::SplitButton *m_toggle=nullptr;
    controls::MenuItem *m_recent=nullptr;
    controls::Label *m_action=nullptr;
    bool m_labelUpdates=true;
};

}//namespace gweni

#endif//_gweni_splitbutton_example_h_
