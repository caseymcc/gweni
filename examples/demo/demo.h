/*
	GWEN
	Copyright (c) 2011 Facepunch Studios
	See license in Gwen.h
*/
#ifndef _gweni_demo_example_h_
#define _gweni_demo_example_h_

#include "gweni/align.h"
//#include "gweni/utility.h"
#include "gweni/controls/windowControl.h"
#include "gweni/controls/tabControl.h"
#include "gweni/controls/listBox.h"
#include "gweni/controls/dockBase.h"
#include "gweni/controls/statusBar.h"
#include "gweni/controls/collapsibleList.h"
#include "gweni/controls/pageControl.h"
#include "gweni/controls/layout/position.h"

namespace gweni
{

class Demo: public controls::DockBase
{
public:
    GWENI_CONTROL(Demo, controls::DockBase);

public:
    void printText(const String &str);

    void think() override;
    void render(skin::Base *skin);

    template<typename _Type>
    void addDemo(controls::CollapsibleCategory *category, const std::string &name);

private:
    void onCategorySelect(event::Info info);

    controls::TabControl *m_tabControl;
    controls::ListBox *m_textOutput;
    controls::StatusBar *m_statusBar;
    controls::Base *m_lastControl;
//    controls::layout::Center *m_center;
    controls::PageControl *m_center;
    controls::DockBase *m_dockBase;

    std::vector<controls::Base *> m_pages;
    unsigned int m_frames;
    float m_lastSecond;
};

template<typename _Type>
void Demo::addDemo(controls::CollapsibleCategory *category, const std::string &name)
{
    controls::Button *button = category->add(name);

    size_t index=m_pages.size();

//    m_center->setPageCount(index+1);
//    controls::Base *page=m_center->getPage(index);
    controls::Base *page=m_center->addPage();

    page->setDock(DockPosition::Center);
//    page->setAlignment(Alignment::Center);
    _Type *control=page->newChild<_Type>();

    control->setAlignment(Alignment::Center);
    m_pages.push_back(button);

//    event::Packet packet(control);
    event::Packet packet(index);
    button->onPressCaller.add(this, &Demo::onCategorySelect, packet);

    

//    getTabControl()->addPage(name, control);

//    button->setName(name);

//    _Type *demo=new _Type(this);
//
//    demo->hide();
//
//    event::Packet packet(demo);
//
//    button->onPressCaller.add(this, &Demo::onCategorySelect, packet);
}

}//namespace gweni

#endif//_gweni_demo_example_h_
