/*
  * Gweni demo
  * See license in gweni.h
 */

#include "demo/commandPalette.h"
#include "gweni/controls/button.h"
#include "gweni/controls/canvas.h"
#include "gweni/controls/commandPalette.h"
#include "gweni/controls/label.h"

#include <string>

namespace gweni
{

namespace
{
const char *const g_categories[]={"File", "Edit", "Selection", "View", "Go", "Run", "Terminal", "Git", "Debug",
    "Help"};
const char *const g_verbs[]={"Open", "Close", "Toggle", "Show", "Hide", "Reload", "Rename", "Copy", "Move", "Find",
    "Format", "Reset"};
const char *const g_nouns[]={"File", "Folder", "Editor", "Panel", "Terminal", "Sidebar", "Workspace", "Window",
    "Selection", "Breakpoint", "Branch", "Settings"};
constexpr size_t Categories=sizeof(g_categories)/sizeof(g_categories[0]);
constexpr size_t Verbs=sizeof(g_verbs)/sizeof(g_verbs[0]);
constexpr size_t Nouns=sizeof(g_nouns)/sizeof(g_nouns[0]);
// distinct (category, verb, noun) for the first Combinations commands
constexpr size_t Combinations=Categories*Verbs*Nouns;
}

String CommandPaletteDemo::commandCategory(size_t index)
{
    return g_categories[index%Categories];
}

// The verb changes every Categories commands and the noun every
// Categories*Verbs, so the first ones are each category's "Open File"; a
// second round of the combinations is numbered.
String CommandPaletteDemo::commandLabel(size_t index)
{
    String label=String(g_verbs[(index/Categories)%Verbs])+" "+g_nouns[(index/(Categories*Verbs))%Nouns];

    if(index >= Combinations)
        label+=" ("+std::to_string(index/Combinations+1)+")";
    return label;
}

String CommandPaletteDemo::commandShortcut(size_t index)
{
    if(index%4 != 0)
        return String();

    if(index%8 == 0)
        return "Ctrl+Shift+"+String(1, static_cast<char>('A'+(index/8)%26));
    return "Ctrl+K Ctrl+"+std::to_string((index/4)%10);
}

bool CommandPaletteDemo::commandEnabled(size_t index)
{
    return index%13 != 12;
}

String CommandPaletteDemo::commandId(size_t index)
{
    return "cmd."+std::to_string(index);
}

// A caption, the button and the label in a column; the palette, hidden, on
// the canvas's Ctrl+Shift+P.
GWENI_CONTROL_CONSTRUCTOR(CommandPaletteDemo)
{
    setDock(DockPosition::Center);

    controls::Base *column=newChild<controls::Base>();

    column->setSizeFlags({SizeFlag::Fixed, SizeFlag::Shrink});
    column->setPos(10, 4);
    column->setWidth(420);

    controls::Label *caption=column->newChild<controls::Label>();

    caption->setText("Ctrl+Shift+P or the button opens the palette; type to filter.");
    caption->setDock(DockPosition::Top);
    caption->setMargin(Margin(0, 8, 0, 8));

    m_open=column->newChild<controls::Button>();
    m_open->setText("Open command palette");
    m_open->setDock(DockPosition::Top);
    m_open->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    m_open->setSize(200, 24);
    m_open->onPressCaller.add(this, &CommandPaletteDemo::onOpen);

    m_last=column->newChild<controls::Label>();
    m_last->setDock(DockPosition::Top);
    m_last->setMargin(Margin(0, 10, 0, 0));
    m_last->setText("No command run yet.");

    m_palette=newChild<controls::CommandPalette>();
    m_palette->onCommand.add(this, &CommandPaletteDemo::onCommand);
    setCommandCount(CommandCount);

    if(controls::Canvas *canvas=getCanvas())
        m_palette->attach(canvas, "Ctrl+Shift+P");
}

void CommandPaletteDemo::setCommandCount(size_t count)
{
    m_palette->clearCommands();

    for(size_t i=0; i < count; ++i)
    {
        controls::CommandPalette::Command command;

        command.id=commandId(i);
        command.label=commandLabel(i);
        command.category=commandCategory(i);
        command.shortcut=commandShortcut(i);
        command.enabled=commandEnabled(i);
        m_palette->addCommand(command);
    }
}

const String &CommandPaletteDemo::lastCommand() const
{
    return m_last->getText();
}

void CommandPaletteDemo::onOpen(event::Info)
{
    m_palette->open();
}

void CommandPaletteDemo::onCommand(event::Info info)
{
    const int index=m_palette->findCommand(info.string);

    if(index == controls::CommandPalette::NoCommand)
        return;

    const controls::CommandPalette::Command &command=m_palette->getCommand(static_cast<size_t>(index));

    m_last->setText("Ran: "+command.category+": "+command.label);
}

}//namespace gweni
