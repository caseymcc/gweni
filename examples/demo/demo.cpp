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
#include "demo/slider.h"
#include "demo/statusBar.h"
#include "demo/tabControl.h"
#include "demo/treeControl.h"
#include "demo/textBox.h"
#include "demo/window.h"

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

void Demo::init(const String &name)
{
    m_lastControl=NULL;

    setDock(DockPosition::Center);
    setSize(1024, 768);

    m_dockBase=newChild<controls::DockBase>("DemoDockBase");
    m_dockBase->setDock(DockPosition::Center);

//    controls::TabControl *centerDock=m_dockBase->getTabControl();

////    m_center=newChild<controls::layout::Center>("Center");
    m_center=m_dockBase->newChild<controls::PageControl>("DemoCenter");
    m_center->setDock(DockPosition::Center);
//    m_center->showControls(false);

    controls::CollapsibleList *list=m_dockBase->newChild<controls::CollapsibleList>("DemoList");
    
    DockBase *leftDock=m_dockBase->getLeft();
    DockBase *bottomDock=m_dockBase->getBottom();

    leftDock->getTabControl()->setName("ControlsTab");
    leftDock->getTabControl()->addPage("Demos", list);
    leftDock->setSizeFlags({SizeFlag::Fixed, SizeFlag::Expand});
    leftDock->setWidth(150);
    
    m_textOutput=bottomDock->newChild<controls::ListBox>();

    bottomDock->getTabControl()->addPage("Output", m_textOutput);
    bottomDock->setSizeFlags({SizeFlag::Expand, SizeFlag::Fixed});
    bottomDock->setHeight(200);

    m_statusBar=newChild<controls::StatusBar>();

    {
        controls::CollapsibleCategory *cat=list->add("Basic");

        cat->setName("Basic");
        addDemo<ButtonDemo>(cat, "ButtonDemo");
//        addDemo<LabelDemo>(cat, "LabelDemo");
//        addDemo<LabelMultilineDemo>(cat, "LabelMultilineDemo");
//        ADD_DEMO(ButtonDemo);
//        ButtonDemo *buttonDemo=new ButtonDemo(this);
//        LabelDemo *labelDemo=new LabelDemo(this);
//        LabelMultilineDemo *labelMultilineDemo=new LabelMultilineDemo(this);
    }
    {
        controls::CollapsibleCategory *cat=list->add("Non-Interactive");

//        addDemo<ProgressBarDemo>(cat, "ProgressBarDemo");
//        addDemo<GroupBoxDemo>(cat, "GroupBoxDemo");
//        addDemo<ImagePanelDemo>(cat, "ImagePanelDemo");
//        addDemo<StatusBarDemo>(cat, "StatusBarDemo");

//        ProgressBarDemo *progressBarDemo=new ProgressBarDemo(this);
//        GroupBoxDemo *groupBoxDemo=new GroupBoxDemo(this);
//        ImagePanelDemo *imagePanelDemo=new ImagePanelDemo(this);
//        StatusBarDemo *statusBarDemo=new StatusBarDemo(this);
    }
    {
        controls::CollapsibleCategory *cat=list->add("Controls");

//        ComboBoxDemo *comboBoxDemo=new ComboBoxDemo(this);
//        TextBoxDemo *textBoxDemo=new TextBoxDemo(this);
//        ListBoxDemo *listBoxDemo=new ListBoxDemo(this);
//        CrossSplitterDemo *crossSplitterDemo=new CrossSplitterDemo(this);
//        RadioButtonDemo *radioButtonDemo=new RadioButtonDemo(this);
//        CheckBoxDemo *checkboxDemo=new CheckBoxDemo(this);
//        NumericDemo *numericDemo=new NumericDemo(this);
//        SliderDemo *sliderDemo=new SliderDemo(this);
//        MenuStripDemo *menuStripDemo=new MenuStripDemo(this);
    }
    {
        controls::CollapsibleCategory *cat=list->add("Containers");

//        WindowDemo *windowDemo=new WindowDemo(this);
//        TreeControlDemo *treeControlDemo=new TreeControlDemo(this);
//        PropertiesDemo *propertiesDemo=new PropertiesDemo(this);
//        TabControlDemo *tabControlDemo=new TabControlDemo(this);
//        ScrollControlDemo *scrollControlDemo=new ScrollControlDemo(this);
//        PageControlDemo *pageControlDemo=new PageControlDemo(this);
    }
    {
        controls::CollapsibleCategory *cat=list->add("Non-Standard");

//        CollapsibleListDemo *collapsibleListDemo=new CollapsibleListDemo(this);
//        ColorPickerDemo *colorPickerDemo=new ColorPickerDemo(this);
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
}

void Demo::printText(const String &str)
{
    m_textOutput->addItem(str);
    m_textOutput->scrollToBottom();
}

void Demo::think()
{
    m_frames++;

    if(m_lastSecond<platform::getTimeInSeconds())
    {
//        m_statusBar->setText(format("GWEN Unit Test - %i fps", m_frames*2));
        m_lastSecond=platform::getTimeInSeconds() + 0.5f;
        m_frames=0;
    }
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
