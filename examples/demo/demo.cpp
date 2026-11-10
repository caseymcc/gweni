/*
	GWEN
	Copyright (c) 2010 Facepunch Studios
	See license in Gwen.h
*/

#include "demo/demo.h"

#include <algorithm>
#include <limits>

#include "gweni/controls/dockedTabControl.h"
#include "gweni/controls/tabControl.h"
#include "gweni/controls/verticalScrollBar.h"
#include "gweni/controls/windowControl.h"
#include "gweni/controls/layout/position.h"
#include "gweni/platforms/platform.h"
#include "gweni/tools/statsOverlay.h"

#include "demo/button.h"
#include "demo/checkBox.h"
#include "demo/collapsibleList.h"
#include "demo/colorEdit.h"
#include "demo/colorPicker.h"
#include "demo/comboBox.h"
#include "demo/contextMenu.h"
#include "demo/plotHistogram.h"
#include "demo/plotLines.h"
#include "demo/dragNumber.h"
#include "demo/vectorField.h"
#include "demo/popup.h"
#include "demo/crossSplitter.h"
#include "demo/dataTable.h"
#include "demo/dataTableVirtual.h"
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
#include "demo/showcaseButtons.h"
#include "demo/showcaseContainers.h"
#include "demo/showcaseInput.h"
#include "demo/showcaseDisplay.h"
#include "demo/showcaseWindows.h"
#include "demo/slider.h"
#include "demo/sliderText.h"
#include "demo/statusBar.h"
#include "demo/tabClose.h"
#include "demo/tabControl.h"
#include "demo/tabOverflow.h"
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
    m_list=list;
    
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

    // The stats overlay's toggle, on while the overlay is open however it was
    // opened (F10, --stats-overlay, before this shell was built included), and
    // the overlay kept above the bar (arrange), so it never covers the button.
    m_statsCanvas=getCanvas();
    m_statsButton=m_statusBar->newChild<controls::Button>("StatsButton");
    m_statsButton->setText("Stats");
    // room around the word, which the bar's height leaves none above or below
    m_statsButton->setTextPadding(Padding(8, 0, 8, 0));
    m_statsButton->setIsToggle(true);
    m_statsButton->setToolTipText("Stats overlay (F10)");
    m_statsButton->setToggleState(tools::StatsOverlay::find(m_statsCanvas) != nullptr);
    m_statsButton->onToggleOnCaller.add(this, &Demo::onStatsToggleOn);
    m_statsButton->onToggleOffCaller.add(this, &Demo::onStatsToggleOff);
    m_statusBar->addControl(m_statsButton, true);
    tools::StatsOverlay::onOpenChangedCaller.add(this, &Demo::onStatsOverlayChanged);

    // First, so the selected page shows at the top of the list (the list runs
    // past the dock's bottom edge).
    {
        controls::CollapsibleCategory *cat=list->add("Showcase");

        cat->setName("Showcase");
        addDemo<ShowcaseContainersDemo>(cat, "ShowcaseContainersDemo", "Containers & data");
        addDemo<ShowcaseButtonsDemo>(cat, "ShowcaseButtonsDemo", "Buttons & toggles");
        addDemo<ShowcaseInputDemo>(cat, "ShowcaseInputDemo", "Text input");
        addDemo<ShowcaseDisplayDemo>(cat, "ShowcaseDisplayDemo", "Display & feedback");
        addDemo<ShowcaseWindowsDemo>(cat, "ShowcaseWindowsDemo", "Windows & menus");
    }
    {
        controls::CollapsibleCategory *cat=list->add("Basic");

        cat->setName("Basic");
        addDemo<ButtonDemo>(cat, "ButtonDemo", "Button");
        addDemo<LabelDemo>(cat, "LabelDemo", "Label");
        addDemo<LabelMultilineDemo>(cat, "LabelMultilineDemo", "Multiline label");
    }
    {
        controls::CollapsibleCategory *cat=list->add("Non-Interactive");
        cat->setName("Non-Interactive");

        addDemo<ProgressBarDemo>(cat, "ProgressBarDemo", "Progress bar");
        addDemo<GroupBoxDemo>(cat, "GroupBoxDemo", "Group box");
        addDemo<ImagePanelDemo>(cat, "ImagePanelDemo", "Image panel");
        addDemo<StatusBarDemo>(cat, "StatusBarDemo", "Status bar");
        addDemo<SeparatorDemo>(cat, "SeparatorDemo", "Separator");
        addDemo<PlotHistogramDemo>(cat, "PlotHistogramDemo", "Plot histogram");
        addDemo<PlotLinesDemo>(cat, "PlotLinesDemo", "Plot lines");
    }
    {
        controls::CollapsibleCategory *cat=list->add("Controls");
        cat->setName("Controls");

        addDemo<ComboBoxDemo>(cat, "ComboBoxDemo", "Combo box");
        addDemo<TextBoxDemo>(cat, "TextBoxDemo", "Text box");
        addDemo<ListBoxDemo>(cat, "ListBoxDemo", "List box");
        addDemo<SelectableDemo>(cat, "SelectableDemo", "Selectable");
        addDemo<VirtualListDemo>(cat, "VirtualListDemo", "Virtual list");
        addDemo<DataTableDemo>(cat, "DataTableDemo", "Data table");
        addDemo<DataTableVirtualDemo>(cat, "DataTableVirtualDemo", "Data table, 100k");
        addDemo<ContextMenuDemo>(cat, "ContextMenuDemo", "Context menu");
        addDemo<PopupDemo>(cat, "PopupDemo", "Popup");
        addDemo<UnicodeTextDemo>(cat, "UnicodeTextDemo", "Unicode text");
        addDemo<TextSizesDemo>(cat, "TextSizesDemo", "Text sizes");
        addDemo<TextDemo>(cat, "TextDemo", "Text");
        addDemo<CrossSplitterDemo>(cat, "CrossSplitterDemo", "Cross splitter");
        addDemo<RadioButtonDemo>(cat, "RadioButtonDemo", "Radio button");
        addDemo<CheckBoxDemo>(cat, "CheckBoxDemo", "Check box");
        addDemo<NumericDemo>(cat, "NumericDemo", "Numeric");
        addDemo<SliderDemo>(cat, "SliderDemo", "Slider");
        addDemo<SliderTextDemo>(cat, "SliderTextDemo", "Slider text");
        addDemo<DragNumberDemo>(cat, "DragNumberDemo", "Drag number");
        addDemo<VectorFieldDemo>(cat, "VectorFieldDemo", "Vector field");
        addDemo<MenuStripDemo>(cat, "MenuStripDemo", "Menu strip");
    }
    {
        controls::CollapsibleCategory *cat=list->add("Containers");
        cat->setName("Containers");

        addDemo<WindowDemo>(cat, "WindowDemo", "Window");
        addDemo<WindowButtonsDemo>(cat, "WindowButtonsDemo", "Window buttons");
        addDemo<TreeControlDemo>(cat, "TreeControlDemo", "Tree control");
        addDemo<PropertiesDemo>(cat, "PropertiesDemo", "Properties");
        addDemo<TabControlDemo>(cat, "TabControlDemo", "Tab control");
        addDemo<TabCloseDemo>(cat, "TabCloseDemo", "Closable tabs");
        addDemo<TabOverflowDemo>(cat, "TabOverflowDemo", "Tab overflow");
        addDemo<ScrollControlDemo>(cat, "ScrollControlDemo", "Scroll control");
        addDemo<PageControlDemo>(cat, "PageControlDemo", "Page control");
    }
    {
        controls::CollapsibleCategory *cat=list->add("Non-Standard");
        cat->setName("Non-Standard");

        addDemo<CollapsibleListDemo>(cat, "CollapsibleListDemo", "Collapsible list");
        addDemo<ColorPickerDemo>(cat, "ColorPickerDemo", "Color picker");
        addDemo<ColorEditDemo>(cat, "ColorEditDemo", "Color edit");
    }
//
//    m_statusBar->sendToBack();
//    printText("Unit Test Started.\n");
//    m_lastSecond=platform::getTimeInSeconds();
//    m_frames=0;
//    list->getNamedChildren("MenuStrip").doAction();
}

Demo::~Demo()
{
    // the bar the overlay kept clear of goes with this shell
    tools::StatsOverlay::setBottomReserve(m_statsCanvas, 0);
}

void Demo::arrange(skin::Base *skin, Dim dim)
{
    ParentClass::arrange(skin, dim);

    // The bar has its place now: the canvas's stats overlay keeps clear of
    // the canvas's bottom up to the bar's top edge. It is the same at every
    // canvas size and scale, so only the first pass changes it, before the
    // overlay (after this shell among the canvas's children) is arranged.
    if((dim == Dim::Y) && m_statsCanvas && m_statusBar->visible())
    {
        const int barTop=m_statusBar->localPosToCanvas().y;

        tools::StatsOverlay::setBottomReserve(m_statsCanvas, m_statsCanvas->getHeight()-barTop);
    }
}

void Demo::onStatsToggleOn(event::Info /*info*/)
{
    if(!tools::StatsOverlay::find(m_statsCanvas))
        tools::StatsOverlay::open(m_statsCanvas);
}

void Demo::onStatsToggleOff(event::Info /*info*/)
{
    if(tools::StatsOverlay::find(m_statsCanvas))
        tools::StatsOverlay::close(m_statsCanvas);
}

void Demo::onStatsOverlayChanged(event::Info info)
{
    if(info.control != m_statsCanvas)
        return;
    m_statsButton->setToggleState(tools::StatsOverlay::find(m_statsCanvas) != nullptr);
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

    if(ShowcaseNotes *notes=dynamic_cast<ShowcaseNotes *>(m_demoControls[pageIndex]))
    {
        for(const String &line : notes->showcaseNotes())
            printText("  "+line);

        notes->showcaseShown();
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

std::vector<Demo::ListEntryFit> Demo::listEntryFits()
{
    // The list's vertical scrollbar is drawn over the right of its inner
    // panel, so a label running under it is as hidden as a clipped one.
    int barLeft=std::numeric_limits<int>::max();

    for(controls::Base *child : m_list->getActualChildren())
    {
        if(gweni_cast<controls::VerticalScrollBar>(child) && child->visible())
            barLeft=child->localPosToCanvas().x;
    }

    std::vector<ListEntryFit> fits;

    for(size_t i=0; i<m_pages.size(); ++i)
    {
        controls::Button *button=gweni_cast<controls::Button>(m_pages[i]);

        if(!button)
            continue;

        const int buttonLeft=button->localPosToCanvas().x;
        const int textLeft=buttonLeft+button->textX();
        int right=std::min(barLeft, buttonLeft+button->getWidth()-button->getPadding().right);

        // Each clipping control up to the list cuts the region the button's
        // children are drawn in (Base::drainRenderQueue).
        for(controls::Base *parent=button->getParent(); parent; parent=parent->getParent())
        {
            if(parent->shouldClip())
                right=std::min(right, parent->localPosToCanvas().x+parent->getWidth());
            if(parent == m_list)
                break;
        }

        fits.push_back({m_names[i], button->getText(), button->textWidth(), right-textLeft});
    }
    return fits;
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
