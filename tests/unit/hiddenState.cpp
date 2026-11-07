/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A control's own hidden flag and the hidden state it inherits, on a real
 *  Canvas with the StubSkin. hidden() is what setHidden/hide/show last set on
 *  that control, and hiding or showing an ancestor never changes it; visible()
 *  is false when the control or any ancestor (inner panels included) is
 *  hidden. So an ancestor hidden and shown again gives every descendant back
 *  the state it had, at depth 3 and more: a ComboBox's closed list, a
 *  TabControl's other pages, a collapsed CollapsibleCategory and TreeNode, a
 *  ScrollControl's unneeded bars, a setClosable(false) close button, a
 *  VirtualList's spare rows, a PageControl's Finish button and a TextBox's
 *  placeholder, with the live ids as they were and a settled frame after it
 *  that changes nothing. Each of them built into a hidden host and then shown
 *  lays out and shows as the same control built shown.
 *
 *  A hide or a show marks StateChange_Visibility on exactly the controls whose
 *  visible() changed, and showing a parent does not enter a subtree a child
 *  hid itself: a collapsed node holding 10 000 controls costs a few. A child
 *  added to a hidden parent (newChild, setParent) is not visible until the
 *  parent is shown, unless it hid itself; one moved out of a hidden parent
 *  into a shown one, or detached, is visible. A layout change made inside a
 *  hidden subtree (a child added, a size set) is laid out in the frame it is
 *  shown, in one pass. A WindowControl is brought to the front by its own
 *  show only, not when an ancestor is shown.
 */
#include "headlessFixture.h"

#include <gweni/controls/collapsibleCategory.h>
#include <gweni/controls/comboBox.h>
#include <gweni/controls/pageControl.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/tabControl.h>
#include <gweni/controls/textBox.h>
#include <gweni/controls/treeControl.h>
#include <gweni/controls/virtualList.h>
#include <gweni/controls/windowControl.h>
#include <gweni/inputHandler.h>

#include <doctest/doctest.h>

#include <functional>
#include <string>
#include <vector>

using namespace gweni;

namespace
{

//! Calls \p func on \p control and every control below it, inner panels
//! included (the tree the layout and render walks follow).
template<typename Func>
void forEachActual(controls::Base *control, Func &&func)
{
    func(control);
    for(controls::Base *child : control->getActualChildren())
        forEachActual(child, func);
}

size_t countControls(controls::Base *root)
{
    size_t count=0;
    forEachActual(root, [&](controls::Base *) { ++count; });
    return count;
}

//! One control of a subtree, in pre-order.
struct Entry
{
    controls::Base *control;
    std::string type;
    bool hidden;
    bool visible;
    Rect bounds;
};

std::vector<Entry> snapshot(controls::Base *root)
{
    std::vector<Entry> entries;
    forEachActual(root, [&](controls::Base *control)
        {
            entries.push_back({control, control->getTypeName(), control->hidden(),
                control->visible(), control->getBounds()});
        });
    return entries;
}

//! Rect has no doctest stringifier: compared through this.
bool sameRect(const Rect &a, const Rect &b)
{
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

//! What a check prints for an entry that differs.
std::string describe(const Entry &entry)
{
    return entry.type+" hidden="+(entry.hidden?"yes":"no")+" visible="+(entry.visible?"yes":"no")
        +" bounds=("+std::to_string(entry.bounds.x)+", "+std::to_string(entry.bounds.y)+", "
        +std::to_string(entry.bounds.w)+", "+std::to_string(entry.bounds.h)+")";
}

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;

    //! Over every frame: the most update-phase passes one took, and whether
    //! a pass re-dirtied layout or the render phase laid out again.
    int maxPasses=0;
    bool redirtied=false;
    bool renderLayout=false;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(800, 600);
        frame();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();

        const controls::LayoutStats &layout=canvas->getLayoutStats();

        maxPasses=std::max(maxPasses, layout.passes);
        redirtied=redirtied || (layout.redirtiedType != nullptr);
        renderLayout=renderLayout || layout.renderLayout;
    }

    //! Frames until one lays nothing out, and its counters cleared: a control
    //! is laid out in more than one pass the first time (a Text's first
    //! measure resizes it), which this is not about.
    void settle()
    {
        for(int i=0; i<10; ++i)
        {
            frame();
            if(canvas->getLayoutStats().passes == 0)
                break;
        }
        frame();
        maxPasses=0;
        redirtied=false;
        renderLayout=false;
    }

    //! A panel at a fixed place on the canvas, placed by nothing else.
    controls::Base *panel(controls::Base *parent, const Rect &bounds)
    {
        controls::Base *panel=parent->newChild<controls::Base>();
        panel->setBounds(bounds);
        return panel;
    }

    //! A panel filling its parent.
    controls::Base *filling(controls::Base *parent)
    {
        controls::Base *panel=parent->newChild<controls::Base>();
        panel->setDock(DockPosition::Center);
        return panel;
    }
};

//! The controls marked StateChange_Visibility, in the canvas's tree.
std::vector<controls::Base *> markedVisibility(controls::Canvas *canvas)
{
    std::vector<controls::Base *> marked;
    forEachActual(canvas, [&](controls::Base *control)
        {
            if(control->getStateChange() & controls::StateChange_Visibility)
                marked.push_back(control);
        });
    return marked;
}

//! The controls of \p before that were visible: what hiding their root marks,
//! and what showing it again marks.
std::vector<controls::Base *> visibleIn(const std::vector<Entry> &before)
{
    std::vector<controls::Base *> visible;
    for(const Entry &entry : before)
    {
        if(entry.visible)
            visible.push_back(entry.control);
    }
    return visible;
}

//! Builds one control, with a part it hides itself, into \p host, and returns
//! it. The part is what \p selfHidden finds hidden in the control's subtree.
struct Case
{
    const char *name;
    std::function<controls::Base *(controls::Base *host)> build;
    //! The type name of a control in the subtree that the control hid itself.
    const char *selfHidden;
};

std::vector<Case> cases()
{
    return {
        {"ComboBox", [](controls::Base *host)
            {
                auto *combo=host->newChild<controls::ComboBox>();
                combo->setBounds(10, 10, 150, 20);
                combo->addItem("One");
                combo->addItem("Two");
                combo->addItem("Three");
                return combo;
            }, "Menu"},
        {"TabControl", [](controls::Base *host)
            {
                auto *tabs=host->newChild<controls::TabControl>();
                tabs->setDock(DockPosition::Center);
                for(const char *name : {"First", "Second", "Third"})
                    tabs->addPage(name)->getPage()->newChild<controls::Base>()->setDock(DockPosition::Center);
                return tabs;
            }, "Base"},
        {"CollapsibleCategory", [](controls::Base *host)
            {
                auto *category=host->newChild<controls::CollapsibleCategory>();
                category->setDock(DockPosition::Top);
                category->setText("Category");
                category->add("One");
                category->add("Two");
                category->setExpanded(false);
                return category;
            }, "CategoryInner"},
        {"TreeNode", [](controls::Base *host)
            {
                auto *tree=host->newChild<controls::TreeControl>();
                tree->setDock(DockPosition::Center);
                controls::TreeNode *open=tree->addNode("Open");
                open->addNode("Leaf");
                controls::TreeNode *closed=tree->addNode("Closed");
                closed->addNode("Leaf")->addNode("Deeper");
                open->open();
                return tree;
            }, "Base"},
        {"ScrollControl", [](controls::Base *host)
            {
                auto *scroll=host->newChild<controls::ScrollControl>();
                scroll->setDock(DockPosition::Center);
                scroll->setScroll(true, true);
                scroll->newChild<controls::Base>()->setBounds(0, 0, 20, 20);
                return scroll;
            }, "VerticalScrollBar"},
        {"WindowControl", [](controls::Base *host)
            {
                auto *window=host->newChild<controls::WindowControl>();
                window->setBounds(20, 20, 200, 150);
                window->setTitle("Window");
                window->setClosable(false);
                return window;
            }, "WindowCloseButton"},
        {"VirtualList", [](controls::Base *host)
            {
                auto *list=host->newChild<controls::VirtualList>();
                list->setBounds(0, 0, 200, 100);
                list->setRowHeight(20);
                list->setRowBinder([](controls::Base *, size_t) {});
                list->setItemCount(2);
                return list;
            }, "VirtualListRow"},
        {"PageControl", [](controls::Base *host)
            {
                auto *pages=host->newChild<controls::PageControl>();
                pages->setDock(DockPosition::Center);
                pages->addPages(3);
                return pages;
            }, "Button"},
        {"TextBox", [](controls::Base *host)
            {
                // the first box shows its hint and hides nothing; the second,
                // with text in it, hides its hint
                auto *panel=host->newChild<controls::Base>();
                panel->setDock(DockPosition::Center);
                auto *empty=panel->newChild<controls::TextBox>();
                empty->setBounds(0, 0, 150, 20);
                empty->setPlaceholder("Search");
                auto *typed=panel->newChild<controls::TextBox>();
                typed->setBounds(0, 30, 150, 20);
                typed->setPlaceholder("Search");
                typed->setText("typed");
                return panel;
            }, "Text"},
    };
}

}//namespace

TEST_CASE("hidden() is a control's own flag, and visible() takes in every ancestor")
{
    Scene s;
    controls::Base *a=s.panel(s.canvas, Rect(0, 0, 400, 300));
    controls::Base *b=s.filling(a);
    controls::Base *c=s.filling(b);
    controls::Base *d=s.filling(c);
    s.settle();

    CHECK(d->visible());

    b->hide();
    CHECK(b->hidden());
    CHECK_FALSE(c->hidden());
    CHECK_FALSE(d->hidden());
    CHECK(a->visible());
    CHECK_FALSE(b->visible());
    CHECK_FALSE(c->visible());
    CHECK_FALSE(d->visible());

    // set under a hidden ancestor: the own flag changes, visible() does not
    d->hide();
    CHECK(d->hidden());
    CHECK_FALSE(d->visible());

    b->show();
    CHECK_FALSE(b->hidden());
    CHECK(c->visible());
    CHECK(d->hidden());
    CHECK_FALSE(d->visible());

    a->hide();
    d->show();
    CHECK_FALSE(d->hidden());
    CHECK_FALSE(d->visible());

    a->show();
    CHECK(b->visible());
    CHECK(c->visible());
    CHECK(d->visible());

    // the inner panel is on the chain too: a collapsed TreeNode's children
    auto *tree=a->newChild<controls::TreeControl>();
    controls::TreeNode *node=tree->addNode("Node");
    controls::TreeNode *leaf=node->addNode("Leaf");
    node->close();
    CHECK_FALSE(leaf->hidden());
    CHECK_FALSE(leaf->visible());
    node->open();
    CHECK(leaf->visible());
}

TEST_CASE("an ancestor hidden and shown again gives every descendant back its hidden state")
{
    for(const Case &test : cases())
    {
        const std::string name=test.name;
        CAPTURE(name);

        Scene s;
        controls::Base *top=s.panel(s.canvas, Rect(0, 0, 600, 400));
        controls::Base *host=s.filling(s.filling(top));
        controls::Base *control=test.build(host);
        s.settle();

        const std::vector<Entry> before=snapshot(top);
        const test::IdBankCounts idsBefore=test::IdBankCounts::now();

        // the part the control hid itself is there, under a shown control
        bool selfHidden=false;
        forEachActual(control, [&](controls::Base *at)
            {
                if(at != control && at->hidden() && at->getParent() && at->getParent()->visible()
                    && test.selfHidden == std::string(at->getTypeName()))
                    selfHidden=true;
            });
        CHECK(selfHidden);

        // hiding the top marks exactly the controls that were visible
        top->hide();
        CHECK(markedVisibility(s.canvas) == visibleIn(before));
        s.frame();

        for(const Entry &entry : before)
        {
            if(entry.control == top)
                continue;

            INFO(describe(entry));
            CHECK(entry.control->hidden() == entry.hidden);
            CHECK_FALSE(entry.control->visible());
        }

        // showing it marks them again, and gives every flag back
        top->show();
        CHECK(markedVisibility(s.canvas) == visibleIn(before));
        s.frame();

        const std::vector<Entry> after=snapshot(top);
        REQUIRE(after.size() == before.size());
        for(size_t i=0; i<before.size(); ++i)
        {
            INFO(describe(before[i]) << " -> " << describe(after[i]));
            CHECK(after[i].control == before[i].control);
            CHECK(after[i].hidden == before[i].hidden);
            CHECK(after[i].visible == before[i].visible);
            CHECK(sameRect(after[i].bounds, before[i].bounds));
        }

        const test::IdBankCounts idsAfter=test::IdBankCounts::now();
        CHECK(idsAfter.livePrimitives() == idsBefore.livePrimitives());
        CHECK(idsAfter.liveTexts() == idsBefore.liveTexts());

        // the frame after it settles: nothing pending, nothing laid out
        controls::resetChangeStats();
        s.frame();
        CHECK(controls::getChangeStats().controls == 0);
        CHECK(s.canvas->getLayoutStats().passes == 0);
        CHECK(s.maxPasses <= 1);
        CHECK_FALSE(s.redirtied);
        CHECK_FALSE(s.renderLayout);
    }
}

TEST_CASE("a control built into a hidden host and shown is the same control built shown")
{
    for(const Case &test : cases())
    {
        const std::string name=test.name;
        CAPTURE(name);

        Scene s;
        controls::Base *shownHost=s.panel(s.canvas, Rect(0, 0, 390, 300));
        controls::Base *hiddenHost=s.panel(s.canvas, Rect(400, 0, 390, 300));
        // the hidden host is never laid out, so the control's parent has its
        // size from the start: a WindowControl's setBounds keeps it inside a
        // parent's current bounds, which a dock that has not been laid out yet
        // does not have
        controls::Base *hiddenInner=s.panel(hiddenHost, Rect(0, 0, 390, 300));
        hiddenHost->hide();

        controls::Base *shown=test.build(shownHost);
        controls::Base *built=test.build(hiddenInner);
        CHECK_FALSE(built->visible());
        s.settle();

        forEachActual(built, [](controls::Base *at) { CHECK_FALSE(at->visible()); });

        hiddenHost->show();
        s.frame();
        CHECK(s.maxPasses <= 1);
        CHECK_FALSE(s.redirtied);
        s.settle();

        const std::vector<Entry> a=snapshot(shown);
        const std::vector<Entry> b=snapshot(built);
        REQUIRE(a.size() == b.size());
        for(size_t i=0; i<a.size(); ++i)
        {
            INFO(describe(a[i]) << " vs " << describe(b[i]));
            CHECK(a[i].type == b[i].type);
            CHECK(a[i].hidden == b[i].hidden);
            CHECK(a[i].visible == b[i].visible);
            CHECK(sameRect(a[i].bounds, b[i].bounds));
        }
    }
}

TEST_CASE("a child added to a hidden parent is visible once the parent is shown, unless it hid itself")
{
    Scene s;
    controls::Base *shownParent=s.panel(s.canvas, Rect(0, 0, 200, 200));
    controls::Base *hiddenParent=s.panel(s.canvas, Rect(200, 0, 200, 200));
    hiddenParent->hide();
    s.settle();

    // newChild, with children of its own made in its init
    auto *made=hiddenParent->newChild<controls::ComboBox>();
    made->addItem("One");
    CHECK_FALSE(made->hidden());
    CHECK_FALSE(made->visible());
    forEachActual(made, [](controls::Base *at) { CHECK_FALSE(at->visible()); });

    // setParent from a shown parent, one that hid itself and its child
    controls::Base *moved=s.panel(shownParent, Rect(0, 0, 50, 50));
    controls::Base *movedChild=s.filling(moved);
    controls::Base *selfHidden=s.panel(shownParent, Rect(0, 60, 50, 50));
    controls::Base *selfHiddenChild=s.filling(selfHidden);
    selfHidden->hide();
    s.settle();

    moved->setParent(hiddenParent);
    selfHidden->setParent(hiddenParent);
    CHECK_FALSE(moved->visible());
    CHECK_FALSE(movedChild->visible());
    CHECK_FALSE(movedChild->hidden());
    CHECK(selfHidden->hidden());
    CHECK_FALSE(selfHiddenChild->hidden());

    hiddenParent->show();
    CHECK(made->visible());
    CHECK(moved->visible());
    CHECK(movedChild->visible());
    CHECK(selfHidden->hidden());
    CHECK_FALSE(selfHiddenChild->visible());

    // the ComboBox's list stays closed
    bool menuVisible=false;
    forEachActual(made, [&](controls::Base *at)
        {
            if(std::string(at->getTypeName()) == "Menu" && at->visible())
                menuVisible=true;
        });
    CHECK_FALSE(menuVisible);
    s.settle();

    // moved out of a hidden parent into a shown one: visible
    hiddenParent->hide();
    s.frame();
    controls::resetChangeStats();
    moved->setParent(shownParent);
    CHECK(moved->visible());
    CHECK(movedChild->visible());
    CHECK((moved->getStateChange() & controls::StateChange_Visibility) != 0);
    CHECK((movedChild->getStateChange() & controls::StateChange_Visibility) != 0);
    s.frame();

    // detached, it is visible unless it hid itself
    movedChild->setParent(nullptr);
    CHECK(movedChild->visible());
    hiddenParent->show();
    selfHidden->setParent(nullptr);
    CHECK_FALSE(selfHidden->visible());
    CHECK_FALSE(selfHiddenChild->visible());
    delete movedChild;
    delete selfHidden;
    s.frame();
}

TEST_CASE("showing a parent does not enter the subtree of a child that hid itself")
{
    Scene s;
    controls::Base *page=s.panel(s.canvas, Rect(0, 0, 600, 400));
    auto *tree=page->newChild<controls::TreeControl>();
    tree->setDock(DockPosition::Center);
    controls::TreeNode *big=tree->addNode("Big");

    for(int i=0; i<50; ++i)
    {
        controls::TreeNode *node=big->addNode("Node");

        for(int j=0; j<50; ++j)
            node->addNode("Leaf");
    }
    big->close();
    s.settle();

    REQUIRE(countControls(big) >= 10000);

    page->hide();
    s.frame();

    // shown: marks the controls above the collapsed node's inner panel only
    page->show();
    const std::vector<controls::Base *> marked=markedVisibility(s.canvas);
    CHECK(marked.size() < 40);
    for(controls::Base *control : marked)
        CHECK(control->visible());

    controls::resetChangeStats();
    s.frame();
    CHECK(controls::getChangeStats().visited < 50);

    // and hidden again, the same
    page->hide();
    CHECK(markedVisibility(s.canvas).size() == marked.size());
    controls::resetChangeStats();
    s.frame();
    CHECK(controls::getChangeStats().visited < 50);

    // the frames after a show and a hide write nothing
    controls::resetChangeStats();
    s.frame();
    CHECK(controls::getChangeStats().controls == 0);
    page->show();
    s.frame();
    controls::resetChangeStats();
    s.frame();
    CHECK(controls::getChangeStats().controls == 0);
}

TEST_CASE("a layout change inside a hidden subtree is laid out in the frame it is shown")
{
    // the same panel built shown, and built shown, hidden, changed and shown
    // again, have the same bounds
    Scene s;
    controls::Base *hosts[2]={s.panel(s.canvas, Rect(0, 0, 300, 300)), s.panel(s.canvas, Rect(400, 0, 300, 300))};
    controls::Base *panels[2];
    controls::Base *placed[2];

    for(int i=0; i<2; ++i)
    {
        panels[i]=s.filling(s.filling(hosts[i]));
        controls::Base *top=panels[i]->newChild<controls::Base>();
        top->setDock(DockPosition::Top);
        top->setHeight(20);
        placed[i]=s.panel(panels[i], Rect(10, 40, 50, 50));
    }
    s.settle();

    hosts[1]->hide();
    s.frame();

    for(int i=0; i<2; ++i)
    {
        controls::Base *added=panels[i]->newChild<controls::Base>();
        added->setDock(DockPosition::Top);
        added->setHeight(30);
        placed[i]->setBounds(20, 100, 80, 40);
        panels[i]->setPadding(Padding(4, 4, 4, 4));
    }
    s.frame();
    s.maxPasses=0;

    hosts[1]->show();
    s.frame();
    CHECK(s.maxPasses == 1);
    CHECK_FALSE(s.redirtied);
    CHECK_FALSE(s.renderLayout);

    const std::vector<Entry> a=snapshot(panels[0]);
    const std::vector<Entry> b=snapshot(panels[1]);
    REQUIRE(a.size() == b.size());
    for(size_t i=0; i<a.size(); ++i)
    {
        INFO(describe(a[i]) << " vs " << describe(b[i]));
        CHECK(sameRect(a[i].bounds, b[i].bounds));
    }
    CHECK(sameRect(placed[1]->getBounds(), Rect(20, 100, 80, 40)));
}

TEST_CASE("a WindowControl comes to the front for its own show, not an ancestor's")
{
    Scene s;
    controls::Base *page=s.panel(s.canvas, Rect(0, 0, 600, 400));
    auto *back=page->newChild<controls::WindowControl>();
    back->setBounds(10, 10, 200, 150);
    auto *front=page->newChild<controls::WindowControl>();
    front->setBounds(50, 50, 200, 150);
    s.settle();

    const controls::Base::List order=page->getActualChildren();
    REQUIRE(order.back() == front);

    page->hide();
    s.frame();
    page->show();
    s.frame();
    CHECK(page->getActualChildren() == order);
    CHECK(s.canvas->getUpdateStats().zVisited == 0);

    back->hide();
    s.frame();
    back->show();
    s.frame();
    CHECK(page->getActualChildren().back() == back);
}
