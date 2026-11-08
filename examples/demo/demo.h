#include <cstdio>
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

    //! Show the page of the demo named \a name, as pressing its list button does.
    //! \return false if no demo has that name.
    bool showDemo(const String &name);

    //! The demo names showDemo accepts, in list order.
    std::vector<String> getDemoNames() const;

    void render(skin::Base *skin);

    //! Add the page of \a _Type, with an entry in \a category of the demo list.
    //! \a name is the page's key, its class name: what --demo, HERO_DEMO in
    //! tools/capture_samples.sh and ControlTest's list use. \a label is what the
    //! list entry and the Output pane show (the key if empty). Every page in
    //! Demo::init gets a short label with no "Demo" suffix ("Window buttons"),
    //! since the list is the demo list and its category headers group the
    //! pages, and a class name is too wide for the list's rows.
    template<typename _Type>
    void addDemo(controls::CollapsibleCategory *category, const std::string &name,
        const std::string &label=std::string());

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
    //! the key of each page, in m_pages' order
    std::vector<String> m_names;
    //! the demo control on each page, in m_pages' order
    std::vector<controls::Base *> m_demoControls;
};

template<typename _Type>
void Demo::addDemo(controls::CollapsibleCategory *category, const std::string &name,
    const std::string &label)
{
    controls::Button *button = category->add(label.empty() ? name : label);

    size_t index=m_pages.size();

//    m_center->setPageCount(index+1);
//    controls::Base *page=m_center->getPage(index);
    controls::Base *page=m_center->addPage();

    page->setDock(DockPosition::Center);
//    page->setAlignment(Alignment::Center);
    _Type *control=page->newChild<_Type>();

    control->setAlignment(Alignment::Center);
    m_pages.push_back(button);
    m_names.push_back(name);
    m_demoControls.push_back(control);

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
