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

namespace gweni
{

class Demo: public controls::DockBase
{
public:
    GWENI_CONTROL(Demo, controls::DockBase);

    void printText(const String &str);

    void render(skin::Base *skin);

    template<typename _Type>
    void addDemo(controls::CollapsibleCategory *category, const std::string &name);

private:
    void onCategorySelect(event::Info info);

    controls::TabControl *m_tabControl;
    controls::ListBox *m_textOutput;
    controls::StatusBar *m_statusBar;
    controls::Base *m_lastControl;

    unsigned int m_frames;
    float m_lastSecond;
};

template<typename _Type>
void Demo::addDemo(controls::CollapsibleCategory *category, const std::string &name)
{
    controls::Button *button = category->add(name);

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
