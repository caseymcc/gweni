/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include "demo/splitButton.h"
#include "gweni/controls/dropDownButton.h"
#include "gweni/controls/label.h"
#include "gweni/controls/menu.h"
#include "gweni/controls/menuItem.h"
#include "gweni/controls/splitButton.h"

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

// A row docked Top in the column; its buttons are docked Left in it, so each
// keeps its own width.
controls::Base *row(controls::Base *column)
{
    controls::Base *row=column->newChild<controls::Base>();

    row->setDock(DockPosition::Top);
    row->setSizeFlags({SizeFlag::Expand, SizeFlag::Shrink});
    return row;
}

// A button docked Left in the row, ButtonHeight high, its text 8 units off
// its sides (a DropDownButton's chevron room kept on its right).
constexpr int ButtonHeight=24;

void style(controls::DropDownButton *button)
{
    button->setTextPadding(Padding(8, 0, 8+controls::DropDownButton::ChevronRoom, 0));
}

void style(controls::SplitButton *button)
{
    button->getMainPart()->setTextPadding(Padding(8, 0, 8, 0));
}

template<typename T>
T *inRow(controls::Base *row)
{
    T *control=row->newChild<T>();

    control->setDock(DockPosition::Left);
    control->setMargin(Margin(0, 0, 8, 0));
    control->setHeight(ButtonHeight);
    control->setSizeFlags({SizeFlag::Shrink, SizeFlag::Fixed});
    style(control);
    return control;
}

}//namespace

// A column of captioned rows, then the label.
GWENI_CONTROL_CONSTRUCTOR(SplitButtonDemo)
{
    setDock(DockPosition::Center);

    controls::Base *column=newChild<controls::Base>();

    column->setSizeFlags({SizeFlag::Fixed, SizeFlag::Shrink});
    column->setPos(10, 4);
    column->setWidth(520);

    caption(column, "Drop-down: items, a divider, a disabled item, a check and a submenu");
    controls::Base *dropDowns=row(column);

    m_options=inRow<controls::DropDownButton>(dropDowns);
    m_options->setText("Options");
    {
        controls::Menu *menu=m_options->getMenu();

        menu->addItem("New")->onMenuItemSelected.add(this, &SplitButtonDemo::onItemSelected);
        menu->addItem("Open")->onMenuItemSelected.add(this, &SplitButtonDemo::onItemSelected);
        menu->addDivider();

        controls::MenuItem *save=menu->addItem("Save");

        save->setDisabled(true);

        controls::MenuItem *wrap=menu->addItem("Word wrap");

        wrap->setCheckable(true);
        wrap->onMenuItemSelected.add(this, &SplitButtonDemo::onItemSelected);

        m_recent=menu->addItem("Recent");
        for(const char *file : {"notes.txt", "todo.md", "build.log"})
            m_recent->getMenu()->addItem(file)->onMenuItemSelected.add(this, &SplitButtonDemo::onItemSelected);
    }

    m_iconDropDown=inRow<controls::DropDownButton>(dropDowns);
    m_iconDropDown->setImage("test16.png");
    m_iconDropDown->setToolTipText("More");
    for(const char *text : {"Share", "Export", "Print"})
        m_iconDropDown->getMenu()->addItem(text)->onMenuItemSelected.add(this, &SplitButtonDemo::onItemSelected);

    caption(column, "Split: an action and a menu of its variants");
    controls::Base *splits=row(column);

    m_paste=inRow<controls::SplitButton>(splits);
    m_paste->setText("Paste");
    m_paste->onPress.add(this, &SplitButtonDemo::onSplitPressed);
    for(const char *text : {"Paste", "Paste special", "Paste as text"})
        m_paste->getMenu()->addItem(text)->onMenuItemSelected.add(this, &SplitButtonDemo::onItemSelected);

    // the last item chosen becomes the main action
    m_run=inRow<controls::SplitButton>(splits);
    for(const char *text : {"Run", "Debug", "Profile"})
    {
        controls::MenuItem *item=m_run->getMenu()->addItem(text);

        item->onMenuItemSelected.add(this, &SplitButtonDemo::onRunItemSelected);
        if(!m_run->getMainItem())
            m_run->setMainItem(item);
    }
    // wide enough for its longest item, so a new main item lays nothing out
    m_run->getMainPart()->setWidth(70);
    m_run->getMainPart()->setSizeFlags({SizeFlag::Fixed, SizeFlag::Shrink});

    caption(column, "Toggle split and icons alone");
    controls::Base *toggles=row(column);

    m_toggle=inRow<controls::SplitButton>(toggles);
    m_toggle->setText("Wrap");
    m_toggle->setIsToggle(true);
    m_toggle->onPress.add(this, &SplitButtonDemo::onSplitPressed);
    for(const char *text : {"At the window", "At 80 columns", "At 100 columns"})
        m_toggle->getMenu()->addItem(text)->onMenuItemSelected.add(this, &SplitButtonDemo::onItemSelected);

    controls::SplitButton *iconSplit=inRow<controls::SplitButton>(toggles);

    iconSplit->setImage("test16.png");
    iconSplit->setToolTipText("Insert image");
    iconSplit->onPress.add(this, &SplitButtonDemo::onSplitPressed);
    for(const char *text : {"From file", "From clipboard"})
        iconSplit->getMenu()->addItem(text)->onMenuItemSelected.add(this, &SplitButtonDemo::onItemSelected);

    caption(column, "Disabled");
    controls::Base *disabled=row(column);
    controls::DropDownButton *disabledDropDown=inRow<controls::DropDownButton>(disabled);

    disabledDropDown->setText("Options");
    disabledDropDown->getMenu()->addItem("Nothing");
    disabledDropDown->setDisabled(true);

    controls::SplitButton *disabledSplit=inRow<controls::SplitButton>(disabled);

    disabledSplit->setText("Paste");
    disabledSplit->getMenu()->addItem("Paste");
    disabledSplit->setDisabled(true);

    m_action=column->newChild<controls::Label>();
    m_action->setDock(DockPosition::Top);
    m_action->setMargin(Margin(0, 12, 0, 0));
    m_action->setText("Last action: -");
}

void SplitButtonDemo::setLabelUpdates(bool updates)
{
    m_labelUpdates=updates;
}

void SplitButtonDemo::showAction(const String &action)
{
    if(m_labelUpdates)
        m_action->setText("Last action: "+action);
}

void SplitButtonDemo::onItemSelected(event::Info info)
{
    if(auto *item=gweni_cast<controls::MenuItem>(info.controlCaller))
        showAction(item->getText());
}

// Chosen from the menu or pressed as the main action: the main action from now on.
void SplitButtonDemo::onRunItemSelected(event::Info info)
{
    auto *item=gweni_cast<controls::MenuItem>(info.controlCaller);

    if(!item)
        return;
    if(m_run->getMainItem() != item)
        m_run->setMainItem(item);
    showAction(item->getText());
}

void SplitButtonDemo::onSplitPressed(event::Info info)
{
    auto *split=gweni_cast<controls::SplitButton>(info.controlCaller);

    if(!split)
        return;

    String action=split->getText();

    if(action.empty())
        action=split->getToolTipText();
    if(split->isToggle())
        action+=split->getToggleState()?" on":" off";
    showAction(action);
}

}//namespace gweni
