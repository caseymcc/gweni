/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include "demo/toolBar.h"
#include "gweni/controls/horizontalSlider.h"
#include "gweni/controls/label.h"
#include "gweni/controls/menu.h"
#include "gweni/controls/menuItem.h"
#include "gweni/controls/splitButton.h"
#include "gweni/controls/textBox.h"
#include "gweni/controls/toolBar.h"

namespace gweni
{

namespace
{

// A section's caption, docked Top in the column.
void caption(controls::Base *column, const char *text)
{
    controls::Label *label=column->newChild<controls::Label>();

    label->setText(text);
    label->setDock(DockPosition::Top);
    label->setMargin(Margin(0, 8, 0, 3));
}

// A row docked Top in the column, as high as what it holds.
controls::Base *row(controls::Base *column)
{
    controls::Base *row=column->newChild<controls::Base>();

    row->setDock(DockPosition::Top);
    row->setSizeFlags({SizeFlag::Expand, SizeFlag::Shrink});
    return row;
}

}//namespace

// A column: the editor toolbar in its container, the width slider, the
// icon-only toolbar, then the label.
GWENI_CONTROL_CONSTRUCTOR(ToolBarDemo)
{
    setDock(DockPosition::Center);

    controls::Base *column=newChild<controls::Base>();

    column->setSizeFlags({SizeFlag::Fixed, SizeFlag::Shrink});
    column->setPos(10, 4);
    column->setWidth(780);

    caption(column, "An editor's toolbar: what does not fit goes into the menu at its end");

    // the slider sets the toolbar's width in its row: a clipping container
    // narrowed around it would give every item a new clip box, so it is the
    // toolbar (which does not clip) that is resized
    m_editor=row(column)->newChild<controls::ToolBar>();
    m_editor->setDock(DockPosition::Left);
    m_editor->setSizeFlags({SizeFlag::Fixed, SizeFlag::Shrink});
    m_editor->setWidth(DefaultWidth);
    // the file group always shows
    for(const char *text : {"New", "Open", "Save"})
    {
        controls::ToolButton *button=m_editor->addButton(text, "test16.png");

        button->onPressCaller.add(this, &ToolBarDemo::onAction);
        m_editor->setOverflowPriority(button, controls::OverflowPriority::AlwaysShow);
    }
    m_editor->addSeparator();

    // the search box has no menu form: it stays in the bar
    m_search=m_editor->newChild<controls::TextBox>();
    m_search->setWidth(110);
    m_search->setPlaceholder("Search");
    m_editor->addControl(m_search);
    m_editor->addSeparator();
    for(const char *text : {"Cut", "Copy"})
        m_editor->addButton(text)->onPressCaller.add(this, &ToolBarDemo::onAction);
    m_paste=m_editor->addSplitButton("Paste");
    m_paste->onPress.add(this, &ToolBarDemo::onAction);
    for(const char *text : {"Paste special", "Paste as text"})
        m_paste->getMenu()->addItem(text)->onMenuItemSelected.add(this, &ToolBarDemo::onMenuItem);
    m_editor->addSeparator();
    m_bold=m_editor->addToggle("Bold");
    m_bold->onPressCaller.add(this, &ToolBarDemo::onAction);
    for(const char *text : {"Italic", "Underline"})
        m_editor->addToggle(text)->onPressCaller.add(this, &ToolBarDemo::onAction);
    m_editor->addSeparator();
    for(const char *text : {"Undo", "Redo"})
        m_editor->addButton(text)->onPressCaller.add(this, &ToolBarDemo::onAction);


    caption(column, "Its width");
    m_width=row(column)->newChild<controls::HorizontalSlider>();
    m_width->setDock(DockPosition::Left);
    m_width->setSize(300, 20);
    m_width->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    m_width->setRange(120, 780);
    m_width->setFloatValue(DefaultWidth, false);
    m_width->onValueChanged.add(this, &ToolBarDemo::onWidthChanged);

    caption(column, "Icons alone (each text its tooltip)");
    controls::Base *iconRow=row(column);

    m_icons=iconRow->newChild<controls::ToolBar>();
    m_icons->setDock(DockPosition::Left);
    m_icons->setDisplay(controls::ToolBarDisplay::IconOnly);
    for(const char *text : {"Back", "Forward", "Reload"})
        m_icons->addButton(text, "test16.png")->onPressCaller.add(this, &ToolBarDemo::onAction);
    m_icons->addSeparator();
    m_icons->addToggle("Pin", "test16.png")->onPressCaller.add(this, &ToolBarDemo::onAction);

    m_action=column->newChild<controls::Label>();
    m_action->setDock(DockPosition::Top);
    m_action->setMargin(Margin(0, 12, 0, 0));
    m_action->setText("Last action: -");
}

void ToolBarDemo::setBarWidth(int width)
{
    m_editor->setWidth(width);
}

int ToolBarDemo::getBarWidth() const
{
    return m_editor->getWidth();
}

void ToolBarDemo::setLabelUpdates(bool updates)
{
    m_labelUpdates=updates;
}

void ToolBarDemo::showAction(const String &action)
{
    if(m_labelUpdates)
        m_action->setText("Last action: "+action);
}

void ToolBarDemo::onAction(event::Info info)
{
    if(auto *button=gweni_cast<controls::Button>(info.controlCaller))
    {
        String text=button->getText();

        if(button->isToggle())
            text+=button->getToggleState()?" on":" off";
        showAction(text);
    }
    else if(auto *split=gweni_cast<controls::SplitButton>(info.controlCaller))
        showAction(split->getText());
}

void ToolBarDemo::onMenuItem(event::Info info)
{
    if(auto *item=gweni_cast<controls::MenuItem>(info.controlCaller))
        showAction(item->getText());
}

void ToolBarDemo::onWidthChanged(event::Info)
{
    setBarWidth(static_cast<int>(m_width->getFloatValue()+0.5f));
}

}//namespace gweni
