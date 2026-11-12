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

#include <functional>

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

    //! Called each time a page is shown, with its key. \a picked is true for
    //! a press on its list entry (the user's choice) and false for
    //! showDemo() (the app's: --demo at start-up, the URL after back or
    //! forward), so an app keeping the page in its URL pushes the first and
    //! replaces for the second.
    typedef std::function<void(const String &name, bool picked)> PageShownHandler;
    void setPageShownHandler(PageShownHandler handler) { m_pageShown=std::move(handler); }

    //! The key of the page shown: the one last shown through its list entry
    //! or showDemo(), else the one the shell is built showing (its first).
    const String &shownDemo() const { return m_shownDemo; }

    //! The demo list's entry for the page \a name, null if there is none.
    controls::Button *demoEntry(const String &name) const;

    //! What the status bar's link opens (platform::openUrl: a new tab on the
    //! web, the system's browser elsewhere).
    static const char *const LinkUrl;
    //! The status bar's link, at its left end.
    controls::Button *link() const { return m_link; }

    //! One entry of the demo list as last laid out: its key and label, the
    //! width of the label's Text, and the room it has, from the Text's left
    //! edge to where the space it is drawn in ends. That is the button's inner
    //! rect (its bounds less its padding), cut by each clipping control
    //! between the button and the list, and by the list's vertical scrollbar
    //! while it is shown. A label wider than its room is clipped.
    struct ListEntryFit
    {
        String key;
        String label;
        int textWidth;
        int room;
    };

    //! Every entry of the demo list, in list order (ControlTest's
    //! --scenario demo-labels fails on any whose textWidth exceeds its room).
    std::vector<ListEntryFit> listEntryFits();

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

    virtual ~Demo();

    //! Places its children, then keeps the canvas's stats overlay above the
    //! status bar (tools::StatsOverlay::setBottomReserve).
    void arrange(skin::Base *skin, Dim dim) override;

    //! The status bar's "Stats" toggle, on exactly while the canvas's
    //! tools::StatsOverlay is open.
    controls::Button *statsButton() const { return m_statsButton; }
    //! The status bar's "Depth" toggle, left of Stats, on exactly while the
    //! canvas's tools::DepthView is open.
    controls::Button *depthButton() const { return m_depthButton; }
    controls::StatusBar *statusBar() const { return m_statusBar; }

private:
    void onCategorySelect(event::Info info);
    //! The Stats button turned on or off: opens or closes the overlay, unless
    //! it already is so (the button following the overlay).
    void onStatsToggleOn(event::Info info);
    void onStatsToggleOff(event::Info info);
    //! tools::StatsOverlay::onOpenChangedCaller: the button follows its canvas's
    //! overlay, however it was opened or closed (F10, --stats-overlay).
    void onStatsOverlayChanged(event::Info info);
    //! The same for the Depth button and the canvas's depth view, opened on
    //! the canvas skin's renderer. While the view is open it takes the mouse,
    //! so the way out is Escape or F9, which the button follows.
    void onDepthToggleOn(event::Info info);
    void onDepthToggleOff(event::Info info);
    void onDepthViewChanged(event::Info info);
    //! The status bar's link pressed: LinkUrl opened.
    void onLinkPress(event::Info info);

    controls::TabControl *m_tabControl;
    controls::ListBox *m_textOutput;
    controls::StatusBar *m_statusBar;
    controls::Button *m_statsButton=nullptr;
    controls::Button *m_depthButton=nullptr;
    controls::Button *m_link=nullptr;
    PageShownHandler m_pageShown;
    String m_shownDemo;
    //! showDemo() is pressing a list entry: the page is not the user's pick
    bool m_showingByName=false;
    //! the canvas the Stats and Depth buttons and the overlay's bottom reserve
    //! are for
    controls::Canvas *m_statsCanvas=nullptr;
    controls::Base *m_lastControl;
//    controls::layout::Center *m_center;
    controls::PageControl *m_center;
    controls::DockBase *m_dockBase;
    controls::CollapsibleList *m_list;

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
