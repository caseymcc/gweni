/*
  * Gweni demo
  * See license in gweni.h
 */

#include "demo/showcaseWindows.h"
#include "gweni/controls/bullet.h"
#include "gweni/controls/button.h"
#include "gweni/controls/checkBox.h"
#include "gweni/controls/colorPicker.h"
#include "gweni/controls/groupBox.h"
#include "gweni/controls/horizontalSlider.h"
#include "gweni/controls/label.h"
#include "gweni/controls/menu.h"
#include "gweni/controls/menuStrip.h"
#include "gweni/controls/windowControl.h"

namespace gweni
{

// A MenuStrip across the top; under it three columns of titled sections
// filling the rest of the page (the shell's centre pane, 865x545, or
// ControlTest's canvas); and two windows over the middle and right-hand
// columns, placed by their alignment in the page, so they keep their place in
// it at any size.
// Every state is set here; the context menu is opened by a right-click in its
// box (tools/capture_samples.sh's SAMPLE_STEPS).
GWENI_CONTROL_CONSTRUCTOR(ShowcaseWindowsDemo)
{
    // Docked, so it fills the shell's page and ControlTest's canvas alike; the
    // strip and the body dock in it, the windows are placed in it.
    setDock(DockPosition::Center);

    {
        controls::MenuStrip *strip=newChild<controls::MenuStrip>();

        {
            controls::Menu *file=strip->addItem("File")->getMenu();

            file->addItem("New", "test16.png", "Ctrl+N");
            file->addItem("Open...", "", "Ctrl+O");
            file->addItem("Save", "", "Ctrl+S");
            file->addDivider();
            file->addItem("Quit", "", "Ctrl+Q");
        }
        {
            controls::Menu *edit=strip->addItem("Edit")->getMenu();

            edit->addItem("Undo", "", "Ctrl+Z");
            edit->addItem("Redo", "", "Ctrl+Y")->setDisabled(true);
        }
        {
            controls::Menu *view=strip->addItem("View")->getMenu();

            view->addItem("Zoom in", "", "Ctrl++");
            view->addItem("Zoom out", "", "Ctrl+-");
        }
        strip->addItem("Window")->getMenu()->addItem("Cascade");
        // a menu that is not available yet
        strip->addItem("Plugins")->setDisabled(true);
        strip->addItem("Help")->getMenu()->addItem("About gweni");
        m_notes.push_back("MenuStrip: six menus, \"Plugins\" disabled");
    }

    controls::Base *body=newChild<controls::Base>();

    showcase::initPage(body);

    controls::Base *left=showcase::addColumn(body);
    controls::Base *middle=showcase::addColumn(body);
    controls::Base *right=showcase::addColumn(body);

    {
        controls::GroupBox *section=showcase::addSection(left, "ColorPicker", false);
        controls::ColorPicker *picker=showcase::addRow<controls::ColorPicker>(section);

        picker->setColor(Color(122, 162, 247, 200));
        m_notes.push_back("ColorPicker: red 122, green 162, blue 247, alpha 200");
    }
    {
        controls::GroupBox *section=showcase::addSection(left, "Context menu");
        controls::Label *hint=showcase::addRow<controls::Label>(section);

        hint->setText("Right-click in this box");

        if(controls::Menu *menu=section->createContextMenu())
        {
            menu->addItem("Cut", "", "Ctrl+X");
            menu->addItem("Copy", "", "Ctrl+C");
            menu->addItem("Paste", "", "Ctrl+V");
            menu->addDivider();
            // a MenuItem's check is not drawn yet (issue #414), so there is no
            // checkable item
            menu->addItem("Select all", "", "Ctrl+A");
            menu->addItem("Find...", "", "Ctrl+F");
            menu->addItem("Delete")->setDisabled(true);
        }
        m_notes.push_back("Context menu: accelerators, a divider, \"Delete\" disabled");
    }
    {
        // the content the windows float over
        controls::GroupBox *section=showcase::addSection(middle, "Document");

        for(const char *text :
            {
                "gweni lays its controls out in an update phase and draws them in a "
                "render phase, and both phases enter only what changed.",
                "A WindowControl is a control like any other: it is laid out with the "
                "page and drawn over the controls before it.",
                "Drag a window by its title bar, resize it by an edge and close it with "
                "the button on its right.",
                "A context menu opens at the cursor, flipped and kept inside the canvas "
                "where it would not fit, and closes on a click outside it or Escape.",
                "A menu strip opens its menus on a click, and moving along the strip "
                "opens the next one; a disabled item never highlights.",
                "Each control keeps its primitives in a persistent buffer, so a frame in "
                "which nothing changed uploads nothing at all.",
            })
        {
            controls::Label *paragraph=showcase::addRow<controls::Label>(section);

            paragraph->setWrap(true);
            paragraph->setText(text);
            paragraph->setMargin(Margin(0, 0, 0, 8));
        }
    }
    {
        controls::GroupBox *section=showcase::addSection(right, "Outline");

        for(const char *text : {"Update phase", "Render phase", "Layout", "Z order",
            "Primitives", "Clipping", "Coverage text", "SDF text", "Skins", "Themes",
            "Input", "Tooltips", "Menus", "Popups", "Windows"})
            showcase::addRow<controls::Bullet>(section)->setText(text);
    }

    {
        // behind the other one; it has no close button
        controls::WindowControl *window=newChild<controls::WindowControl>();

        window->setTitle("Inspector");
        window->setClosable(false);
        window->setSize(220, 130);
        window->setAlignment(Alignment::Right|Alignment::Top);
        window->setMargin(Margin(0, 110, 24, 0));

        for(const char *text : {"Position  120, 48", "Size  220 x 130", "Opacity  85 %"})
            showcase::addRow<controls::Label>(window)->setText(text);
    }
    {
        controls::WindowControl *window=newChild<controls::WindowControl>();

        window->setTitle("Preferences");
        window->setSize(240, 165);
        window->setAlignment(Alignment::Right|Alignment::Bottom);
        window->setMargin(Margin(0, 0, 170, 60));

        controls::CheckBoxWithLabel *autosave=showcase::addRow<controls::CheckBoxWithLabel>(window);

        autosave->label()->setText("Save automatically");
        autosave->checkBox()->setChecked(true);
        showcase::addRow<controls::CheckBoxWithLabel>(window)->label()->setText("Show hidden files");
        showcase::addRow<controls::Label>(window)->setText("Font size");

        controls::HorizontalSlider *size=showcase::addRow<controls::HorizontalSlider>(window);

        size->setHeight(20);
        size->setRange(8, 24);
        size->setFloatValue(14);

        controls::Button *ok=window->newChild<controls::Button>();

        ok->setText("OK");
        ok->setDock(DockPosition::Bottom);
        m_notes.push_back("WindowControl: \"Preferences\" over \"Inspector\", which has no close button");
    }
}

}//namespace gweni
