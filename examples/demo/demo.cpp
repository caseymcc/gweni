/*
	GWEN
	Copyright (c) 2010 Facepunch Studios
	See license in Gwen.h
*/

#include "demo/demo.h"
#include "gweni/controls/dockedTabControl.h"
#include "gweni/controls/tabControl.h"
#include "gweni/controls/windowControl.h"
#include "gweni/controls/layout/position.h"
#include "gweni/platforms/platform.h"

#include "demo/button.h"
#include "demo/checkBox.h"
#include "demo/collapsibleList.h"
#include "demo/colorPicker.h"
#include "demo/comboBox.h"
#include "demo/contextMenu.h"
#include "demo/popup.h"
#include "demo/crossSplitter.h"
#include "demo/groupBox.h"
#include "demo/imagePanel.h"
#include "demo/label.h"
#include "demo/labelMultiline.h"
#include "demo/listBox.h"
#include "demo/menuStrip.h"
#include "demo/numeric.h"
#include "demo/pageControl.h"
#include "demo/progressBar.h"
#include "demo/properties.h"
#include "demo/radioButton.h"
#include "demo/scrollControl.h"
#include "demo/selectable.h"
#include "demo/separator.h"
#include "demo/showcaseContainers.h"
#include "demo/slider.h"
#include "demo/statusBar.h"
#include "demo/tabControl.h"
#include "demo/treeControl.h"
#include "demo/textBox.h"
#include "demo/textDemo.h"
#include "demo/textSizes.h"
#include "demo/unicodeText.h"
#include "demo/virtualList.h"
#include "demo/window.h"
#include "demo/windowButtons.h"

namespace gweni
{

#define ADD_DEMO(name)\
{\
    controls::Button* button = cat->add(#name);\
	button->setName(#name);\
    ##name *demo=new ##name (this);\
    demo->hide();\
    Packet packet(demo);\
	button->onPressCaller.add(this, &ThisClass::onCategorySelect, packet);\
}

Demo::Demo(const String &name):
    DockBase()
{}

void registerDemoControlRenderers();

void Demo::init(const String &name)
{
    // Pull in the demo's transparent control-renderer registrations (see
    // demoControlRenderers.cpp) so the demo wrappers don't hit the red fallback.
    registerDemoControlRenderers();

    m_lastControl=NULL;

    setDock(DockPosition::Center);
    setSize(1024, 768);

    m_dockBase=newChild<controls::DockBase>("DemoDockBase");
    m_dockBase->setDock(DockPosition::Center);

//    controls::TabControl *centerDock=m_dockBase->getTabControl();

////    m_center=newChild<controls::layout::Center>("Center");
    m_center=m_dockBase->newChild<controls::PageControl>("DemoCenter");
    m_center->setDock(DockPosition::Center);
    m_center->showControls(false);

    controls::CollapsibleList *list=m_dockBase->newChild<controls::CollapsibleList>("DemoList");
    
    DockBase *leftDock=m_dockBase->getLeft();
    DockBase *bottomDock=m_dockBase->getBottom();

    leftDock->getTabControl()->setName("ControlsTab");
    leftDock->getTabControl()->addPage("Demos", list);
    leftDock->setSizeFlags({SizeFlag::Fixed, SizeFlag::Expand});
    // The list's rows are 113 wide beside its scroll bar (12 in Tokyo Night).
    leftDock->setWidth(155);
    
    m_textOutput=bottomDock->newChild<controls::ListBox>();

    bottomDock->getTabControl()->addPage("Output", m_textOutput);
    bottomDock->setSizeFlags({SizeFlag::Expand, SizeFlag::Fixed});
    bottomDock->setHeight(200);

    m_statusBar=newChild<controls::StatusBar>();

    // First, so the selected page shows at the top of the list (the list runs
    // past the dock's bottom edge).
    {
        controls::CollapsibleCategory *cat=list->add("Showcase");

        cat->setName("Showcase");
        addDemo<ShowcaseContainersDemo>(cat, "ShowcaseContainersDemo", "Containers & data");
    }
    {
        controls::CollapsibleCategory *cat=list->add("Basic");

        cat->setName("Basic");
        addDemo<ButtonDemo>(cat, "ButtonDemo");
        addDemo<LabelDemo>(cat, "LabelDemo");
        addDemo<LabelMultilineDemo>(cat, "LabelMultilineDemo");
    }
    {
        controls::CollapsibleCategory *cat=list->add("Non-Interactive");
        cat->setName("Non-Interactive");

        addDemo<ProgressBarDemo>(cat, "ProgressBarDemo");
        addDemo<GroupBoxDemo>(cat, "GroupBoxDemo");
        addDemo<ImagePanelDemo>(cat, "ImagePanelDemo");
        addDemo<StatusBarDemo>(cat, "StatusBarDemo");
        addDemo<SeparatorDemo>(cat, "SeparatorDemo");
    }
    {
        controls::CollapsibleCategory *cat=list->add("Controls");
        cat->setName("Controls");

        addDemo<ComboBoxDemo>(cat, "ComboBoxDemo");
        addDemo<TextBoxDemo>(cat, "TextBoxDemo");
        addDemo<ListBoxDemo>(cat, "ListBoxDemo");
        addDemo<SelectableDemo>(cat, "SelectableDemo");
        addDemo<VirtualListDemo>(cat, "VirtualListDemo");
        addDemo<ContextMenuDemo>(cat, "ContextMenuDemo");
        addDemo<PopupDemo>(cat, "PopupDemo");
        addDemo<UnicodeTextDemo>(cat, "UnicodeTextDemo");
        addDemo<TextSizesDemo>(cat, "TextSizesDemo");
        addDemo<TextDemo>(cat, "TextDemo");
        addDemo<CrossSplitterDemo>(cat, "CrossSplitterDemo");
        addDemo<RadioButtonDemo>(cat, "RadioButtonDemo");
        addDemo<CheckBoxDemo>(cat, "CheckBoxDemo");
        addDemo<NumericDemo>(cat, "NumericDemo");
        addDemo<SliderDemo>(cat, "SliderDemo");
        addDemo<MenuStripDemo>(cat, "MenuStripDemo");
    }
    {
        controls::CollapsibleCategory *cat=list->add("Containers");
        cat->setName("Containers");

        addDemo<WindowDemo>(cat, "WindowDemo");
        addDemo<WindowButtonsDemo>(cat, "WindowButtonsDemo");
        addDemo<TreeControlDemo>(cat, "TreeControlDemo");
        addDemo<PropertiesDemo>(cat, "PropertiesDemo");
        addDemo<TabControlDemo>(cat, "TabControlDemo");
        addDemo<ScrollControlDemo>(cat, "ScrollControlDemo");
        addDemo<PageControlDemo>(cat, "PageControlDemo");
    }
    {
        controls::CollapsibleCategory *cat=list->add("Non-Standard");
        cat->setName("Non-Standard");

        addDemo<CollapsibleListDemo>(cat, "CollapsibleListDemo");
        addDemo<ColorPickerDemo>(cat, "ColorPickerDemo");
    }
//
//    m_statusBar->sendToBack();
//    printText("Unit Test Started.\n");
//    m_lastSecond=platform::getTimeInSeconds();
//    m_frames=0;
//    list->getNamedChildren("MenuStrip").doAction();
}

void Demo::onCategorySelect(event::Info info)
{
 //   if(!info.packet->isControl())
 //       return;
//
//    if(m_lastControl)
//    {
//        m_lastControl->hide();
//    }
//
//    controls::Base *control=info.packet->getControl();
//
//    control->show();
//    m_lastControl=control;
    
    if(!info.packet->isSizeT())
        return;

    size_t pageIndex=info.packet->get<size_t>();

    m_center->showPage(pageIndex);

    // The Output pane: the page shown and, for a showcase page, the states it
    // sets in its constructor.
    if(controls::Button *button=gweni_cast<controls::Button>(m_pages[pageIndex]))
        printText("Showing "+button->getText());

    if(const ShowcaseNotes *notes=dynamic_cast<const ShowcaseNotes *>(m_demoControls[pageIndex]))
    {
        for(const String &line : notes->showcaseNotes())
            printText("  "+line);
    }
}

bool Demo::showDemo(const String &name)
{
    for(size_t i=0; i<m_pages.size(); ++i)
    {
        controls::Button *button=gweni_cast<controls::Button>(m_pages[i]);

        if(!button || m_names[i] != name)
            continue;

        // The list button's own press path: it shows the page through
        // onCategorySelect and leaves the entry selected in the demo list.
        button->doAction();
        return true;
    }
    return false;
}

std::vector<String> Demo::getDemoNames() const
{
    return m_names;
}

void Demo::printText(const String &str)
{
    m_textOutput->addItem(str);
    m_textOutput->scrollToBottom();
}

void Demo::render(skin::Base *skin)
{
//    m_frames++;
//
//    if(m_lastSecond < platform::getTimeInSeconds())
//    {
//        m_statusBar->setText(format("GWEN Unit Test - %i fps", m_frames * 2));
////        m_lastSecond=platform::getTimeInSeconds() + 0.5f;
//        m_frames=0;
//    }
//
    DockBase::render(skin);
}

}//namespace gweni
