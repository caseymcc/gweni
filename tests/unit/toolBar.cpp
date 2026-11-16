/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  ToolBar's overflow on the StubRenderer. A toolbar whose container is wide
 *  enough shows every item and no overflow button; its preferred width is
 *  the width that shows every item but the AlwaysOverflow ones (with the
 *  button they force) and its minimumWidth() the AlwaysShow items' and the button's.
 *  As the container narrows, the Normal items overflow from the end one at a
 *  time, hidden, the overflow button shown at the right end with the row
 *  keeping its room; AlwaysShow items and an addControl item with no overflow
 *  text never go, AlwaysOverflow items are always gone (the button showing
 *  however wide the bar is). A separator never starts or ends the shown row
 *  nor follows another shown one. As it widens the items come back in order,
 *  the button going once all fit. Every one of those width changes lays out
 *  in one update-phase pass, with layout verification finding nothing, and
 *  changes no preferred size of the toolbar.
 *
 *  Below minimumWidth() the overflow button stays at the toolbar's right
 *  edge, where the hit test finds it and a click opens the menu. The width
 *  that shows every item counts the button AlwaysOverflow items force. A
 *  tool button's text is its tooltip only in IconOnly, a tooltip the app
 *  set put aside meanwhile (or set meanwhile) and given back after.
 *
 *  The overflow menu, built as the overflow button opens it: an entry per
 *  overflowed item in order, a tool button its text and icon, a toggle a checkable
 *  entry checked as it is, a separator a divider (never first), a drop-down
 *  a submenu of copies of its menu's items (submenus copied too), a
 *  SplitButton one with its main action, a divider and its menu's items (no
 *  divider for an empty menu), a
 *  disabled item a disabled entry, an addControl item its overflow text
 *  whatever control it is (a Button or DropDownButton added that way is
 *  neither copied nor pressed: the toolbar's onOverflowItemChosen is its only
 *  event). A copied submenu's entry follows its original (its text, disabled
 *  state and check), and getMenuEntry finds a drop-down's own entry and an
 *  original menu item's copy. A rebuilt menu is as wide as its entries, not
 *  the widest set it held. A copied menu item keeps its icon, and an
 *  overflow button's Menu made again (the old one deleted) is built anew.
 *  getMenuEntry(nullptr) is null. An item overflowed from under a resting
 *  mouse is no longer the hovered control. addControl of a control that is already an
 *  item, or of the overflow button, adds nothing, so deleting it leaves no
 *  item behind.
 *  Choosing an entry fires its item's own press once (a tool button's
 *  onPress, a toggle's onToggle, a drop-down item's onMenuItemSelected, a
 *  SplitButton's onPress) and then the toolbar's onOverflowItemChosen; the
 *  toggle's state shows in its entry on the next open, from the menu or from
 *  the app. The menu is built again only when the overflowed set changed;
 *  each open brings a drop-down's, a split button's and its main action's
 *  text up to date. A tool button or SplitButton main action changed to a
 *  toggle becomes checkable; one no longer toggling loses its check. Changed
 *  labels grow or shrink their cached menus without replacing entries,
 *  including copied and nested submenus, in one pass verified headless.
 *
 *  The keys: the tool items are one Tab stop, the last focused; Left, Right,
 *  Home and End move the focus over the shown, enabled tool items, the
 *  overflow button last, passing over separators, disabled items and an
 *  addControl item (whose keys are its own); Space and Return press; the
 *  overflow button's Space opens its menu navigating by keyboard.
 */
#include "headlessFixture.h"

#include <gweni/controls/menu.h>
#include <gweni/controls/menuItem.h>
#include <gweni/controls/separator.h>
#include <gweni/controls/splitButton.h>
#include <gweni/controls/textBox.h>
#include <gweni/controls/toolBar.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace gweni;
using controls::OverflowPriority;
using controls::ToolBar;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    controls::Base *box;
    ToolBar *bar;

    float clock=nextClock();

    //! The input handler keeps the last click's time and place across cases.
    static float nextClock()
    {
        static float start=0.0f;

        start+=1000.0f;
        return start;
    }

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        controls::setLayoutVerify(true);
        canvas=new controls::Canvas(&skin);
        canvas->setSize(1200, 300);
        box=canvas->newChild<controls::Base>();
        box->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        box->setBounds(0, 0, 1100, 100);
        bar=box->newChild<ToolBar>();
    }

    ~Scene()
    {
        for(int key : {keys::Shift, keys::Tab})
            input::onKeyEvent(canvas, key, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        controls::setLayoutVerify(false);
        platform::setTimeSource(nullptr);
    }

    void key(int key)
    {
        input::onKeyEvent(canvas, key, true);
        input::onKeyEvent(canvas, key, false);
        frame();
    }

    //! The overflow menu, opened and laid out.
    controls::Menu *openMenu()
    {
        bar->getOverflowButton()->openMenu(controls::MenuFocus::None);
        frame();
        return bar->getOverflowButton()->getMenu();
    }

    void closeMenu()
    {
        bar->getOverflowButton()->closeMenu();
        frame();
    }

    const controls::LayoutStats &frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
        return canvas->getLayoutStats();
    }

    void settle()
    {
        for(int i=0; i < 10 && frame().passes != 0; ++i)
            ;
    }

    //! The shown items' names, in order, "|" for a separator and "+" for
    //! the overflow button.
    std::string shown() const
    {
        std::string s;

        for(controls::Base *item : bar->getItems())
        {
            if(item->hidden())
                continue;
            if(!s.empty())
                s+=" ";
            s+=gweni_cast<controls::Separator>(item)?"|":item->getName();
        }
        if(!bar->getOverflowButton()->hidden())
            s+=s.empty()?"+":" +";
        return s;
    }

    //! Widens the bar a unit at a time from its minimum width until it shows
    //! \p row.
    void widenUntil(const std::string &row)
    {
        for(int width=bar->minimumWidth(); width < 1100 && shown() != row; ++width)
        {
            setWidth(width);
            settle();
        }
    }

    //! A container width that gives the bar exactly \p barWidth.
    void setWidth(int barWidth)
    {
        box->setBounds(0, 0, barWidth, 100);
    }
};

void checkOnePass(const controls::LayoutStats &stats)
{
    INFO("re-dirtied by " << std::string(stats.redirtiedType?stats.redirtiedType:"none")
        << "#" << stats.redirtiedId);
    CHECK(stats.passes == 1);
    CHECK(stats.redirtiedType == nullptr);
    CHECK_FALSE(stats.renderLayout);
}

controls::ToolButton *button(ToolBar *bar, const char *name)
{
    controls::ToolButton *b=bar->addButton(std::string(name)+" button");

    b->setName(name);
    return b;
}

//! a b | c d | e f: six buttons of the same width.
void fill(ToolBar *bar)
{
    button(bar, "a1");
    button(bar, "b1");
    bar->addSeparator();
    button(bar, "c1");
    button(bar, "d1");
    bar->addSeparator();
    button(bar, "e1");
    button(bar, "f1");
}

}//namespace

TEST_CASE("ToolBar: Normal items overflow from the end one at a time, in one pass, and come back in order")
{
    Scene scene;

    fill(scene.bar);
    scene.settle();

    const size_t failures=controls::layoutVerifyFailures();
    const int full=scene.bar->getPreferredSize().width;
    const int item=scene.bar->getItems()[0]->getWidth();
    const int button=scene.bar->getOverflowButton()->getPreferredSize().width;
    const int height=scene.bar->getPreferredSize().height;

    CHECK(item > 0);
    CHECK(full == 2*ToolBar::Padding+6*item+2*ToolBar::SeparatorSlot+7*ToolBar::Spacing);
    CHECK(scene.shown() == "a1 b1 | c1 d1 | e1 f1");
    CHECK(scene.bar->getOverflowedItems().empty());

    scene.setWidth(full);
    scene.settle();
    CHECK(scene.shown() == "a1 b1 | c1 d1 | e1 f1");

    // each width the bar of the first k items and the button needs
    const auto rowOf=[&](int items, int separators)
        {
            return 2*ToolBar::Padding+items*item+separators*ToolBar::SeparatorSlot
                +(items+separators)*ToolBar::Spacing+button;
        };
    const std::vector<std::pair<int, std::string>> steps=
    {
        {full-1, "a1 b1 | c1 d1 | e1 +"},
        {rowOf(5, 2)-1, "a1 b1 | c1 d1 +"},
        {rowOf(4, 1)-1, "a1 b1 | c1 +"},
        {rowOf(3, 1)-1, "a1 b1 +"},
        {rowOf(2, 0)-1, "a1 +"},
        {rowOf(1, 0)-1, "+"},
    };
    std::vector<size_t> serials;

    for(const auto &step : steps)
    {
        INFO("width " << step.first);
        scene.setWidth(step.first);
        checkOnePass(scene.frame());
        CHECK(scene.shown() == step.second);
        CHECK(scene.bar->getPreferredSize().width == full);
        CHECK(scene.bar->getPreferredSize().height == height);
        CHECK(controls::layoutVerifyFailures() == failures);
        CHECK(scene.frame().passes == 0);
        serials.push_back(scene.bar->getOverflowSerial());
    }
    CHECK(scene.bar->getOverflowedItems().size() == 8);
    for(size_t i=1; i < serials.size(); ++i)
        CHECK(serials[i] != serials[i-1]);

    // and back, in order
    for(size_t n=steps.size()-1; n > 0; --n)
    {
        INFO("width " << steps[n-1].first);
        scene.setWidth(steps[n-1].first);
        checkOnePass(scene.frame());
        CHECK(scene.shown() == steps[n-1].second);
        CHECK(controls::layoutVerifyFailures() == failures);
    }
    scene.setWidth(full);
    checkOnePass(scene.frame());
    CHECK(scene.shown() == "a1 b1 | c1 d1 | e1 f1");
    CHECK(scene.bar->getOverflowedItems().empty());
    CHECK(controls::layoutVerifyFailures() == failures);
}

TEST_CASE("ToolBar: AlwaysShow and AlwaysOverflow items, and a control with no menu form, keep their places")
{
    Scene scene;
    ToolBar *bar=scene.bar;

    controls::ToolButton *a=button(bar, "a1");
    button(bar, "b1");
    controls::ToolButton *c=button(bar, "c1");
    controls::TextBox *search=bar->newChild<controls::TextBox>();

    search->setName("s1");
    search->setWidth(80);
    bar->addControl(search);
    controls::ToolButton *d=button(bar, "d1");

    bar->setOverflowPriority(c, OverflowPriority::AlwaysShow);
    bar->setOverflowPriority(d, OverflowPriority::AlwaysOverflow);
    scene.settle();

    // d is in the menu however wide the bar is
    CHECK(scene.shown() == "a1 b1 c1 s1 +");
    CHECK(bar->isOverflowed(d));
    CHECK_FALSE(bar->canOverflow(search));
    CHECK(bar->getPreferredSize().width > bar->minimumWidth());

    scene.setWidth(bar->minimumWidth());
    checkOnePass(scene.frame());
    CHECK(scene.shown() == "c1 s1 +");

    // narrower still: the AlwaysShow items stay, running on past the
    // toolbar's edge, but the overflow button stays at its right edge,
    // inside it and in reach of the mouse
    scene.setWidth(bar->minimumWidth()/2);
    checkOnePass(scene.frame());
    CHECK(scene.shown() == "c1 s1 +");

    controls::Base *more=bar->getOverflowButton();
    const Point centre=more->localPosToCanvas(Point(more->getWidth()/2, more->getHeight()/2));

    CHECK(more->getBounds().x+more->getWidth() == bar->getWidth()-bar->getPadding().right);
    CHECK(scene.canvas->getControlAt(centre.x, centre.y) == more);
    // and a click there opens the menu
    scene.canvas->inputMouseMoved(centre.x, centre.y, 0, 0);
    scene.canvas->inputMouseButton(0, true);
    scene.canvas->inputMouseButton(0, false);
    scene.frame();
    CHECK(bar->getOverflowButton()->isMenuOpen());
    scene.closeMenu();

    bar->setOverflowPriority(d, OverflowPriority::Normal);
    bar->setOverflowPriority(a, OverflowPriority::AlwaysShow);
    scene.setWidth(1100);
    checkOnePass(scene.frame());
    CHECK(scene.shown() == "a1 b1 c1 s1 d1");

    // with an overflow text it may go
    bar->setOverflowText(search, "Search");
    CHECK(bar->canOverflow(search));
}

TEST_CASE("ToolBar: a separator never starts or ends the shown row, nor follows another")
{
    Scene scene;
    ToolBar *bar=scene.bar;

    bar->addSeparator();
    controls::ToolButton *a=button(bar, "a1");
    bar->addSeparator();
    controls::ToolButton *b=button(bar, "b1");
    bar->addSeparator();
    button(bar, "c1");
    bar->addSeparator();

    scene.settle();
    CHECK(scene.shown() == "a1 | b1 | c1");

    // b gone: its separators would meet
    bar->setOverflowPriority(b, OverflowPriority::AlwaysOverflow);
    checkOnePass(scene.frame());
    CHECK(scene.shown() == "a1 | c1 +");

    bar->setOverflowPriority(a, OverflowPriority::AlwaysOverflow);
    checkOnePass(scene.frame());
    CHECK(scene.shown() == "c1 +");
}

TEST_CASE("ToolBar: an item changed while overflowed comes back at its new size")
{
    Scene scene;

    fill(scene.bar);
    scene.settle();

    const size_t failures=controls::layoutVerifyFailures();
    const int full=scene.bar->getPreferredSize().width;
    controls::ToolButton *f=gweni_cast<controls::ToolButton>(scene.bar->getItems().back());
    const int width=f->getWidth();

    scene.setWidth(full-1);
    scene.settle();
    REQUIRE(f->hidden());

    // its new text is measured though it is hidden: the bar's preferred
    // width follows it
    f->setText("a much longer caption");
    checkOnePass(scene.frame());
    CHECK(scene.bar->getPreferredSize().width > full);
    CHECK(f->getPreferredSize().width > width);

    scene.setWidth(scene.bar->getPreferredSize().width);
    checkOnePass(scene.frame());
    CHECK_FALSE(f->hidden());
    CHECK(f->getWidth() == f->getPreferredSize().width);
    CHECK(f->getWidth() > width);
    CHECK(controls::layoutVerifyFailures() == failures);
}

namespace
{

//! A menu's shown entries: "text" for an item, "[x] text" / "[ ] text" for a
//! checkable one, "(off) " before a disabled one, "text >" for a submenu, "-"
//! for a divider.
std::vector<std::string> entries(controls::Menu *menu)
{
    std::vector<std::string> out;

    for(controls::Base *child : menu->getChildren())
    {
        if(!child || child->hidden())
            continue;
        if(gweni_cast<controls::MenuDivider>(child))
        {
            out.push_back("-");
            continue;
        }

        controls::MenuItem *item=gweni_cast<controls::MenuItem>(child);

        if(!item)
            continue;

        std::string text=item->isDisabled()?"(off) ":"";

        if(item->isCheckable())
            text+=item->isChecked()?"[x] ":"[ ] ";
        text+=item->getText();
        if(item->hasMenu())
            text+=" >";
        out.push_back(text);
    }
    return out;
}

controls::MenuItem *entry(controls::Menu *menu, const std::string &text)
{
    for(controls::Base *child : menu->getChildren())
    {
        controls::MenuItem *item=gweni_cast<controls::MenuItem>(child);

        if(item && !item->hidden() && item->getText() == text)
            return item;
    }
    return nullptr;
}

struct Counter
{
    int calls=0;
    controls::Base *last=nullptr;

    void hit(event::Info info)
    {
        ++calls;
        last=info.control;
    }
};

struct Handler: public event::Handler
{
    Counter press, toggle, dropItem, split, chosen;

    void onPress(event::Info info) { press.hit(info); }
    void onToggle(event::Info info) { toggle.hit(info); }
    void onDropItem(event::Info info) { dropItem.hit(info); }
    void onSplit(event::Info info) { split.hit(info); }
    void onChosen(event::Info info) { chosen.hit(info); }
};

//! a | t (a toggle, on) dd (a drop-down) | sp (a split button) b (disabled)
//! search: everything but `keep` Normal.
struct MenuBar
{
    Scene scene;
    Handler handler;
    controls::ToolButton *a, *t, *b, *keep;
    controls::DropDownButton *dd;
    controls::SplitButton *sp;
    controls::TextBox *search;
    controls::MenuItem *ddSave, *ddAs;

    MenuBar()
    {
        ToolBar *bar=scene.bar;

        keep=button(bar, "keep");
        bar->setOverflowPriority(keep, OverflowPriority::AlwaysShow);
        bar->addSeparator();
        a=button(bar, "a1");
        a->onPressCaller.add(&handler, &Handler::onPress);
        bar->addSeparator();
        t=bar->addToggle("Bold", "", true);
        t->setName("Bold");
        t->onToggleCaller.add(&handler, &Handler::onToggle);
        dd=bar->addDropDown("Export");
        dd->setName("Export");
        ddSave=dd->getMenu()->addItem("Save");
        ddSave->onMenuItemSelected.add(&handler, &Handler::onDropItem);
        dd->getMenu()->addDivider();
        ddAs=dd->getMenu()->addItem("Save as");
        ddAs->setCheckable(true);
        ddAs->getMenu()->addItem("Deep");
        bar->addSeparator();
        sp=bar->addSplitButton("Paste");
        sp->setName("Paste");
        sp->onPress.add(&handler, &Handler::onSplit);
        sp->getMenu()->addItem("Paste plain");
        b=button(bar, "b1");
        b->setText("Off");
        b->setDisabled(true);
        search=bar->newChild<controls::TextBox>();
        search->setWidth(80);
        search->setName("Search");
        bar->addControl(search, "Search");
        bar->onOverflowItemChosen.add(&handler, &Handler::onChosen);
        scene.settle();
        scene.setWidth(bar->minimumWidth());
        scene.settle();
    }
};

}//namespace

TEST_CASE("ToolBar: the overflow menu holds every overflowed item in its menu form")
{
    MenuBar m;

    CHECK(m.scene.shown() == "keep +");

    controls::Menu *menu=m.scene.openMenu();

    REQUIRE(menu);
    CHECK_FALSE(menu->hidden());
    // the separator after keep would lead the menu: not shown
    CHECK(entries(menu) == std::vector<std::string>{"a1 button", "-", "[x] Bold", "Export >", "-",
        "Paste >", "(off) Off", "Search"});

    controls::Menu *exportMenu=entry(menu, "Export")->getMenu();

    CHECK(entries(exportMenu) == std::vector<std::string>{"Save", "-", "[ ] Save as >"});
    CHECK(entries(entry(exportMenu, "Save as")->getMenu()) == std::vector<std::string>{"Deep"});
    CHECK(entries(entry(menu, "Paste")->getMenu()) == std::vector<std::string>{"Paste", "-", "Paste plain"});
    CHECK(m.scene.bar->getMenuEntrySource(entry(menu, "a1 button")) == m.a);
    CHECK(m.scene.bar->getMenuEntry(m.t) == entry(menu, "Bold"));
}

TEST_CASE("ToolBar: each open brings a drop-down's, a split button's and its main action's text up to date")
{
    MenuBar m;
    controls::Menu *menu=m.scene.openMenu();

    REQUIRE(entry(menu, "Export"));
    m.scene.closeMenu();
    m.dd->setText("Share");
    m.sp->setText("Insert");
    menu=m.scene.openMenu();
    // the overflowed set did not change: the same entries, new texts
    CHECK(m.scene.bar->getMenuBuildCount() == 1);
    CHECK(entries(menu) == std::vector<std::string>{"a1 button", "-", "[x] Bold", "Share >", "-",
        "Insert >", "(off) Off", "Search"});
    REQUIRE(entry(menu, "Insert"));
    CHECK(entries(entry(menu, "Insert")->getMenu()) == std::vector<std::string>{"Insert", "-", "Paste plain"});
}

TEST_CASE("ToolBar: a copied submenu's entry in a drop-down's overflow submenu follows its original")
{
    MenuBar m;
    controls::Menu *menu=m.scene.openMenu();

    REQUIRE(entry(menu, "Export"));
    // the drop-down's own entry, and the copy of its submenu item
    CHECK(m.scene.bar->getMenuEntry(m.dd) == entry(menu, "Export"));
    REQUIRE(entry(entry(menu, "Export")->getMenu(), "Save as"));
    CHECK(m.scene.bar->getMenuEntry(m.ddAs) == entry(entry(menu, "Export")->getMenu(), "Save as"));
    CHECK(m.scene.bar->getMenuEntrySource(m.scene.bar->getMenuEntry(m.ddAs)) == m.ddAs);

    m.scene.closeMenu();
    m.ddAs->setText("Save copy");
    m.ddAs->setDisabled(true);
    menu=m.scene.openMenu();
    CHECK(m.scene.bar->getMenuBuildCount() == 1);
    CHECK(entries(entry(menu, "Export")->getMenu()) == std::vector<std::string>{"Save", "-", "(off) [ ] Save copy >"});
}

TEST_CASE("ToolBar: a rebuilt overflow menu is as wide as its entries, not the widest set it held")
{
    Scene scene;
    ToolBar *bar=scene.bar;

    button(bar, "a1");
    bar->addButton("a much longer button name", "")->setName("wide");
    button(bar, "b1");
    scene.settle();

    const int full=bar->fullWidth();

    // only b1 overflowed: the menu a fresh one's width
    scene.setWidth(full-10);
    scene.settle();
    REQUIRE(scene.shown() == "a1 wide +");

    int fresh=scene.openMenu()->getWidth();

    scene.closeMenu();

    // the long name overflowed too, widening the menu
    scene.setWidth(bar->minimumWidth());
    scene.settle();
    CHECK(scene.openMenu()->getWidth() > fresh);
    scene.closeMenu();

    // b1 alone again: back to the fresh width
    scene.setWidth(full-10);
    scene.settle();
    CHECK(scene.openMenu()->getWidth() == fresh);
    CHECK(bar->getMenuBuildCount() == 3);
}

TEST_CASE("ToolBar: cached button entries follow changes of toggle mode in both directions")
{
    MenuBar m;
    const size_t failures=controls::layoutVerifyFailures();
    controls::Button *source=m.a;
    bool wasToggle=false;

    SUBCASE("tool button becomes a toggle") {}
    SUBCASE("tool button stops toggling") { wasToggle=true; }
    SUBCASE("SplitButton main action becomes a toggle") { source=m.sp->getMainPart(); }
    SUBCASE("SplitButton main action stops toggling")
    {
        source=m.sp->getMainPart();
        wasToggle=true;
    }

    const auto setToggle=[&](bool toggle)
    {
        if(source == m.sp->getMainPart())
            m.sp->setIsToggle(toggle);
        else
            source->setIsToggle(toggle);
    };
    const auto setState=[&](bool on)
    {
        if(source == m.sp->getMainPart())
            m.sp->setToggleState(on);
        else
            source->setToggleState(on);
    };
    setToggle(wasToggle);
    setState(wasToggle);
    m.scene.openMenu();
    controls::MenuItem *cached=m.scene.bar->getMenuEntry(source);

    REQUIRE(cached);
    CHECK(cached->isCheckable() == wasToggle);
    CHECK(cached->isChecked() == wasToggle);
    m.scene.closeMenu();
    setToggle(!wasToggle);
    setState(true);
    m.scene.openMenu();
    CHECK(m.scene.bar->getMenuEntry(source) == cached);
    CHECK(m.scene.bar->getMenuBuildCount() == 1);
    CHECK(cached->isCheckable() == !wasToggle);
    CHECK(cached->isChecked() == !wasToggle);

    // Choosing the reused entry follows the current toggle mode.
    cached->onPress(event::Info(cached));
    m.scene.frame();
    CHECK(source->getToggleState() == wasToggle);
    m.scene.openMenu();
    CHECK_FALSE(cached->isChecked());
    m.scene.closeMenu();
    CHECK(m.scene.frame().passes == 0);
    CHECK(controls::layoutVerifyFailures() == failures);
}

TEST_CASE("ToolBar: refreshed labels resize cached menus both ways without rebuilding entries")
{
    MenuBar m;
    const size_t failures=controls::layoutVerifyFailures();
    controls::Base *source=m.a;

    SUBCASE("tool button") {}
    SUBCASE("addControl overflow text") { source=m.search; }
    SUBCASE("drop-down parent") { source=m.dd; }
    SUBCASE("SplitButton parent and main action") { source=m.sp; }
    SUBCASE("copied drop-down item") { source=m.ddSave; }
    SUBCASE("copied submenu parent") { source=m.ddAs; }
    SUBCASE("nested copied submenu item") { source=entry(m.ddAs->getMenu(), "Deep"); }

    const auto textOf=[&]()
    {
        return source == m.search ? m.scene.bar->getOverflowText(source)
            : source == m.sp ? m.sp->getText() : gweni_cast<controls::Button>(source)->getText();
    };
    const auto setText=[&](const String &text)
    {
        if(source == m.search)
            m.scene.bar->setOverflowText(source, text);
        else if(source == m.sp)
            m.sp->setText(text);
        else
            gweni_cast<controls::Button>(source)->setText(text);
    };
    const auto open=[&](controls::Menu *measured)
    {
        m.scene.closeMenu();
        controls::Menu *top=m.scene.openMenu();

        checkOnePass(m.scene.canvas->getLayoutStats());
        if(measured != top)
        {
            // A hidden submenu has not been arranged. Assert its actual
            // width only after showing it, never on a stale preferred size.
            measured->openAt(Point(300, 0));
            checkOnePass(m.scene.frame());
        }
        CHECK(m.scene.frame().passes == 0);
    };

    m.scene.openMenu();
    controls::MenuItem *cached=m.scene.bar->getMenuEntry(source);

    REQUIRE(cached);
    controls::Menu *measured=cached->parentMenu();
    REQUIRE(measured);
    open(measured);
    const int originalWidth=measured->getWidth();
    const String originalText=textOf();
    controls::MenuItem *splitMain=m.scene.bar->getMenuEntry(m.sp->getMainPart());
    controls::Menu *splitMenu=splitMain->parentMenu();
    open(splitMenu);
    const int splitWidth=splitMenu->getWidth();

    m.scene.closeMenu();
    setText("An action renamed to a much much much much much longer label");
    open(measured);
    const int longWidth=measured->getWidth();

    CHECK(longWidth > originalWidth);
    CHECK(m.scene.bar->getMenuEntry(source) == cached);
    CHECK(m.scene.bar->getMenuBuildCount() == 1);
    if(source == m.sp)
    {
        open(splitMenu);
        CHECK(splitMenu->getWidth() > splitWidth);
        CHECK(m.scene.bar->getMenuEntry(m.sp->getMainPart()) == splitMain);
    }

    m.scene.closeMenu();
    setText(originalText);
    open(measured);
    CHECK(measured->getWidth() < longWidth);
    CHECK(measured->getWidth() == originalWidth);
    CHECK(m.scene.bar->getMenuEntry(source) == cached);
    CHECK(m.scene.bar->getMenuBuildCount() == 1);
    if(source == m.sp)
    {
        open(splitMenu);
        CHECK(splitMenu->getWidth() == splitWidth);
    }
    CHECK(controls::layoutVerifyFailures() == failures);
}

TEST_CASE("ToolBar: an overflow entry has its tool button's icon, and an empty split button no divider")
{
    Scene scene;
    ToolBar *bar=scene.bar;
    controls::ToolButton *icon=bar->addButton("", "test16.png");
    controls::SplitButton *solo=bar->addSplitButton("Solo");

    bar->setOverflowPriority(icon, OverflowPriority::AlwaysOverflow);
    bar->setOverflowPriority(solo, OverflowPriority::AlwaysOverflow);
    scene.settle();

    controls::Menu *menu=scene.openMenu();
    controls::MenuItem *iconEntry=bar->getMenuEntry(icon);

    REQUIRE(iconEntry);
    CHECK(iconEntry->getImageName() == "test16.png");
    REQUIRE(entry(menu, "Solo"));
    CHECK(entries(entry(menu, "Solo")->getMenu()) == std::vector<std::string>{"Solo"});
}

TEST_CASE("ToolBar: the width that shows every item counts the button AlwaysOverflow items force")
{
    Scene scene;
    ToolBar *bar=scene.bar;

    button(bar, "a1");
    button(bar, "b1");
    // a narrow item, always in the menu
    controls::Base *narrow=bar->addSeparator();
    controls::ToolButton *c=bar->addButton("", "");

    (void)narrow;
    bar->setOverflowPriority(c, OverflowPriority::AlwaysOverflow);
    scene.settle();
    scene.setWidth(bar->fullWidth());
    scene.settle();
    CHECK(scene.shown() == "a1 b1 +");
}

TEST_CASE("ToolBar: a tool button keeps a tooltip of the app's own")
{
    Scene scene;
    controls::ToolButton *b=button(scene.bar, "a1");

    b->setToolTipText("mine");
    b->setText("other");
    b->setImage("test16.png");
    CHECK(b->getToolTipText() == "mine");

    // in IconOnly the text is the tooltip, and back the app's comes back
    scene.bar->setDisplay(controls::ToolBarDisplay::IconOnly);
    CHECK(b->getToolTipText() == "other");
    scene.bar->setDisplay(controls::ToolBarDisplay::TextOnly);
    CHECK(b->getToolTipText() == "mine");

    // one the app sets meanwhile is the one given back
    scene.bar->setDisplay(controls::ToolBarDisplay::IconOnly);
    b->setToolTipText("newer");
    CHECK(b->getToolTipText() == "other");
    scene.bar->setDisplay(controls::ToolBarDisplay::TextOnly);
    CHECK(b->getToolTipText() == "newer");

    // with none of the app's, leaving IconOnly leaves none
    controls::ToolButton *c=button(scene.bar, "c1");

    c->setImage("test16.png");
    scene.bar->setDisplay(controls::ToolBarDisplay::IconOnly);
    CHECK(c->getToolTipText() == "c1 button");
    scene.bar->setDisplay(controls::ToolBarDisplay::IconAndText);
    CHECK(c->getToolTipText().empty());
}

TEST_CASE("ToolBar: choosing an overflow entry fires its item's event, toggles kept in step both ways")
{
    MenuBar m;
    controls::Menu *menu=m.scene.openMenu();

    entry(menu, "a1 button")->onPress(event::Info(entry(menu, "a1 button")));
    m.scene.frame();
    CHECK(m.handler.press.calls == 1);
    CHECK(m.handler.press.last == m.a);
    CHECK(m.handler.chosen.calls == 1);
    CHECK(m.handler.chosen.last == m.a);
    CHECK(menu->hidden());

    // the toggle, from the menu: off, and its entry shows it next time
    menu=m.scene.openMenu();
    controls::MenuItem *bold=entry(menu, "Bold");
    bold->onPress(event::Info(bold));
    m.scene.frame();
    CHECK_FALSE(m.t->getToggleState());
    CHECK(m.handler.toggle.calls == 1);
    menu=m.scene.openMenu();
    CHECK_FALSE(entry(menu, "Bold")->isChecked());
    m.scene.closeMenu();

    // and from the bar (the app's): the next open shows it
    m.t->setToggleState(true);
    menu=m.scene.openMenu();
    CHECK(entry(menu, "Bold")->isChecked());

    // a drop-down's item: its own onMenuItemSelected, once
    entry(entry(menu, "Export")->getMenu(), "Save")->onPress(event::Info(menu));
    m.scene.frame();
    CHECK(m.handler.dropItem.calls == 1);
    CHECK(m.handler.chosen.last == m.dd);

    // the split button's main action
    menu=m.scene.openMenu();
    controls::MenuItem *paste=entry(entry(menu, "Paste")->getMenu(), "Paste");
    paste->onPress(event::Info(paste));
    m.scene.frame();
    CHECK(m.handler.split.calls == 1);

    // a disabled item's entry is disabled; a control's fires the toolbar's
    // event alone
    menu=m.scene.openMenu();
    CHECK(entry(menu, "Off")->isDisabled());
    const int chosen=m.handler.chosen.calls;
    entry(menu, "Search")->onPress(event::Info(menu));
    CHECK(m.handler.chosen.calls == chosen+1);
    CHECK(m.handler.chosen.last == m.search);
    CHECK(m.handler.press.calls == 1);

    // enabled again: so is its entry
    m.b->setDisabled(false);
    menu=m.scene.openMenu();
    CHECK_FALSE(entry(menu, "Off")->isDisabled());
}

TEST_CASE("ToolBar: an addControl'd Button or DropDownButton is a plain entry, its only event the toolbar's")
{
    Scene scene;
    Handler handler;
    ToolBar *bar=scene.bar;

    controls::ToolButton *keep=button(bar, "keep");
    bar->setOverflowPriority(keep, OverflowPriority::AlwaysShow);

    controls::Button *plain=bar->newChild<controls::Button>();
    plain->setText("Gear");
    plain->setName("Gear");
    plain->onPressCaller.add(&handler, &Handler::onPress);
    bar->addControl(plain, "Settings");

    controls::DropDownButton *dd=bar->newChild<controls::DropDownButton>();
    dd->setText("Opts");
    dd->setName("Opts");
    dd->getMenu()->addItem("One");
    bar->addControl(dd, "Options");
    bar->onOverflowItemChosen.add(&handler, &Handler::onChosen);
    scene.settle();
    scene.setWidth(bar->minimumWidth());
    scene.settle();
    REQUIRE(scene.shown() == "keep +");

    controls::Menu *menu=scene.openMenu();

    // their overflow texts, no submenu copied
    CHECK(entries(menu) == std::vector<std::string>{"Settings", "Options"});
    CHECK(bar->getMenuEntry(plain) == entry(menu, "Settings"));
    CHECK(bar->getMenuEntrySource(entry(menu, "Options")) == dd);

    // choosing it presses nothing: the toolbar's event alone
    REQUIRE(entry(menu, "Settings"));
    entry(menu, "Settings")->onPress(event::Info(menu));
    scene.frame();
    CHECK(handler.chosen.calls == 1);
    CHECK(handler.chosen.last == plain);
    CHECK(handler.press.calls == 0);

    // its overflow text, set again, shows on the next open; a disabled
    // control's entry is disabled
    bar->setOverflowText(plain, "Preferences");
    plain->setDisabled(true);
    menu=scene.openMenu();
    CHECK(entries(menu) == std::vector<std::string>{"(off) Preferences", "Options"});
}

TEST_CASE("ToolBar: addControl of a control already an item, or of the overflow button, adds nothing")
{
    Scene scene;
    ToolBar *bar=scene.bar;

    controls::TextBox *search=bar->newChild<controls::TextBox>();
    CHECK(bar->addControl(search) == search);
    CHECK(bar->addControl(search, "Search") == search);
    CHECK(bar->itemCount() == 1);
    // the second call changed nothing: no overflow text, so still no menu form
    CHECK(bar->getOverflowText(search).empty());

    controls::ToolButton *tool=button(bar, "a1");
    CHECK(bar->addControl(tool) == tool);
    CHECK(bar->addControl(bar->getOverflowButton()) == bar->getOverflowButton());
    CHECK(bar->itemCount() == 2);
    scene.settle();

    // deleted, each leaves the list: no freed pointer stays behind
    delete search;
    delete tool;
    CHECK(bar->itemCount() == 0);
    scene.settle();
    CHECK(scene.shown().empty());
}

TEST_CASE("ToolBar: an item overflowed from under a resting mouse is no longer hovered")
{
    Scene scene;
    ToolBar *bar=scene.bar;

    fill(bar);
    scene.settle();
    controls::Base *last=bar->getItems().back();
    const Point at=last->localPosToCanvas();

    scene.canvas->inputMouseMoved(at.x+last->getWidth()/2, at.y+last->getHeight()/2, 0, 0);
    scene.frame();
    REQUIRE(HoveredControl == last);

    // narrowed until it overflows, the mouse resting where it was
    for(int width=bar->getWidth(); width > bar->minimumWidth() && !last->hidden(); --width)
    {
        scene.setWidth(width);
        scene.settle();
    }
    REQUIRE(last->hidden());
    scene.frame();
    CHECK(HoveredControl != last);
}

TEST_CASE("ToolBar: getMenuEntry of null is null")
{
    MenuBar m;

    m.scene.openMenu();
    CHECK(m.scene.bar->getMenuEntry(nullptr) == nullptr);
}

TEST_CASE("ToolBar: a drop-down item's icon is copied into its overflow entry")
{
    MenuBar m;
    m.ddSave->setImage("save.png");
    controls::Menu *menu=m.scene.openMenu();
    controls::MenuItem *copy=m.scene.bar->getMenuEntry(m.ddSave);

    REQUIRE(menu);
    REQUIRE(copy);
    CHECK(copy->getImageName() == "save.png");
}

TEST_CASE("ToolBar: an overflow button's Menu made again is built anew")
{
    MenuBar m;
    ToolBar *bar=m.scene.bar;
    controls::Menu *menu=m.scene.openMenu();

    REQUIRE(entry(menu, "a1 button"));
    m.scene.closeMenu();
    const size_t builds=bar->getMenuBuildCount();
    menu->delayedDelete();
    m.scene.frame();

    menu=m.scene.openMenu();
    CHECK(bar->getMenuBuildCount() == builds+1);
    CHECK(entry(menu, "a1 button") != nullptr);
}

TEST_CASE("ToolBar: the overflow menu is built again only when the overflowed set changed")
{
    MenuBar m;
    ToolBar *bar=m.scene.bar;

    CHECK(bar->getMenuBuildCount() == 0);
    controls::Menu *menu=m.scene.openMenu();
    CHECK(bar->getMenuBuildCount() == 1);
    controls::MenuItem *first=entry(menu, "a1 button");

    m.scene.closeMenu();
    menu=m.scene.openMenu();
    m.scene.closeMenu();
    CHECK(bar->getMenuBuildCount() == 1);
    CHECK(entry(menu, "a1 button") == first);

    // a width that changes nothing in the set: still the same menu
    m.scene.setWidth(bar->minimumWidth()+1);
    m.scene.settle();
    menu=m.scene.openMenu();
    CHECK(bar->getMenuBuildCount() == 1);
    m.scene.closeMenu();

    // wide enough for a1: it leaves the menu
    m.scene.widenUntil("keep | a1 +");
    REQUIRE_FALSE(m.a->hidden());
    menu=m.scene.openMenu();
    CHECK(bar->getMenuBuildCount() == 2);
    CHECK(entry(menu, "a1 button") == nullptr);
    CHECK(entries(menu).front() == "[x] Bold");
}

TEST_CASE("ToolBar: Left and Right move the focus over the shown tool items, the overflow button last")
{
    MenuBar m;
    ToolBar *bar=m.scene.bar;

    // keep, a1, | Bold, Export, | Paste shown; Off and the search box gone
    m.scene.widenUntil("keep | a1 | Bold Export | Paste +");
    REQUIRE(m.scene.shown() == "keep | a1 | Bold Export | Paste +");
    CHECK(bar->getTabStop() == m.keep);

    m.keep->focus();
    m.scene.frame();
    const std::vector<controls::Base *> order{m.keep, m.a, m.t, m.dd, m.sp, bar->getOverflowButton()};

    for(size_t i=1; i < order.size(); ++i)
    {
        m.scene.key(keys::Right);
        CHECK(KeyboardFocus == order[i]);
    }
    // the end: it stays
    m.scene.key(keys::Right);
    CHECK(KeyboardFocus == bar->getOverflowButton());
    for(size_t i=order.size()-1; i > 0; --i)
    {
        m.scene.key(keys::Left);
        CHECK(KeyboardFocus == order[i-1]);
    }
    m.scene.key(keys::End);
    CHECK(KeyboardFocus == bar->getOverflowButton());
    m.scene.key(keys::Home);
    CHECK(KeyboardFocus == m.keep);

    // the toolbar is one Tab stop, the last focused tool item
    m.scene.key(keys::Right);
    m.scene.key(keys::Right);
    CHECK(KeyboardFocus == m.t);
    CHECK(bar->getTabStop() == m.t);
    int tabable=0;
    for(controls::Base *item : order)
        tabable+=item->isTabable()?1:0;
    CHECK(tabable == 1);
    CHECK(m.t->isTabable());

    controls::Base *outside=m.scene.canvas->newChild<controls::ToolButton>();
    outside->focus();
    m.scene.key(keys::Tab);
    CHECK(KeyboardFocus == m.t);

    // Space and Return press it
    const int toggles=m.handler.toggle.calls;
    m.scene.key(keys::Space);
    m.scene.key(keys::Return);
    CHECK(m.handler.toggle.calls == toggles+2);

    // the overflow button's Space opens its menu navigating by keyboard
    m.scene.key(keys::End);
    m.scene.key(keys::Space);
    controls::Menu *menu=bar->getOverflowButton()->getMenu();
    CHECK_FALSE(menu->hidden());
    CHECK(menu->isKeyboardNavigating());
    CHECK(entries(menu) == std::vector<std::string>{"(off) Off", "Search"});
}

TEST_CASE("ToolBar: Left and Right pass over a disabled item and an addControl one")
{
    MenuBar m;

    m.scene.setWidth(1100);
    m.scene.settle();
    REQUIRE(m.scene.shown() == "keep | a1 | Bold Export | Paste b1 Search");

    m.sp->focus();
    m.scene.frame();
    // Off is disabled and the search box keeps its own keys: Paste is last
    m.scene.key(keys::Right);
    CHECK(KeyboardFocus == m.sp);
    m.scene.key(keys::End);
    CHECK(KeyboardFocus == m.sp);

    // from the search box, Left is the box's own
    m.search->focus();
    m.scene.frame();
    m.scene.key(keys::Left);
    CHECK(KeyboardFocus == m.search);
}
