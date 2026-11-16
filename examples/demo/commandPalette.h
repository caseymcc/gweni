/*
  * Gweni demo
  * See license in gweni.h
 */

#ifndef _gweni_commandpalette_example_h_
#define _gweni_commandpalette_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class Button;
class CommandPalette;
class Label;
}

//! A CommandPalette over a few hundred generated commands in ten categories
//! (every fourth with a shortcut, every thirteenth disabled), opened by
//! Ctrl+Shift+P on the canvas or by its button, and a label naming the last
//! command run.
class CommandPaletteDemo: public controls::Base
{
public:
    GWENI_CONTROL(CommandPaletteDemo, controls::Base);

public:
    //! The commands the palette holds at first.
    static constexpr size_t CommandCount=300;

    controls::CommandPalette *palette() const { return m_palette; }
    controls::Button *openButton() const { return m_open; }
    controls::Label *lastLabel() const { return m_last; }
    //! The palette's commands: the first \p count generated ones.
    void setCommandCount(size_t count);
    //! Command \p index's category, label and shortcut, the same for an index
    //! at any count; whether it is enabled.
    static String commandCategory(size_t index);
    static String commandLabel(size_t index);
    static String commandShortcut(size_t index);
    static bool commandEnabled(size_t index);
    static String commandId(size_t index);
    //! What the label says: the last command run, or that none was.
    const String &lastCommand() const;

private:
    void onCommand(event::Info info);
    void onOpen(event::Info info);

    controls::CommandPalette *m_palette=nullptr;
    controls::Button *m_open=nullptr;
    controls::Label *m_last=nullptr;
};

}//namespace gweni

#endif//_gweni_commandpalette_example_h_
