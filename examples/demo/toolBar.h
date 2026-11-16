/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#ifndef _gweni_toolbar_example_h_
#define _gweni_toolbar_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class HorizontalSlider;
class Label;
class SplitButton;
class TextBox;
class ToolBar;
class ToolButton;
}

//! Toolbars: an editor's toolbar of a dozen items (New, Open and Save with
//! icons, AlwaysShow; a search box, which has no menu form and so stays in the
//! bar; Cut, Copy, a Paste SplitButton, the Bold, Italic and Underline
//! toggles, Undo and Redo; separators between the groups) whose width a
//! slider sets, so its items from Redo back go into the
//! overflow menu and come back; an icon-only toolbar; and a label naming the
//! last action, from the bar or the menu.
class ToolBarDemo: public controls::Base
{
public:
    GWENI_CONTROL(ToolBarDemo, controls::Base);

public:
    //! The editor toolbar's width, in units (the slider's value).
    void setBarWidth(int width);
    int getBarWidth() const;

    //! The controls a test drives.
    controls::ToolBar *editorBar() const { return m_editor; }
    controls::ToolBar *iconBar() const { return m_icons; }
    controls::ToolButton *boldToggle() const { return m_bold; }
    controls::SplitButton *pasteButton() const { return m_paste; }
    controls::TextBox *searchBox() const { return m_search; }
    controls::Label *actionLabel() const { return m_action; }

    //! Whether an action sets the label's text (on by default): a test
    //! weighing the toolbar's own cost turns it off, since the label's new
    //! text is laid out.
    void setLabelUpdates(bool updates);

    static constexpr int DefaultWidth=600;

private:
    void onAction(event::Info info);
    void onMenuItem(event::Info info);
    void onWidthChanged(event::Info info);
    void showAction(const String &action);

    controls::ToolBar *m_editor=nullptr;
    controls::ToolBar *m_icons=nullptr;
    controls::ToolButton *m_bold=nullptr;
    controls::SplitButton *m_paste=nullptr;
    controls::TextBox *m_search=nullptr;
    controls::HorizontalSlider *m_width=nullptr;
    controls::Label *m_action=nullptr;
    bool m_labelUpdates=true;
};

}//namespace gweni

#endif//_gweni_toolbar_example_h_
