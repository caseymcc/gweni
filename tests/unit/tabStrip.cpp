/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  TabStrip/TabButton layout on a real Canvas: each tab is as wide as its
 *  caption plus its own padding from the first frame, the padding follows the
 *  side the strip is docked on, neighbours overlap only by their 1 px shared
 *  border along the row and stay inside the strip across it, and a tab moved
 *  into another TabControl takes that strip's padding.
 *
 *  In a Left/Right strip every tab fills the strip, none at x=-1. The tab is
 *  moved into the other TabControl through addPage.
 */
#include "headlessFixture.h"

#include <gweni/controls/tabControl.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <vector>

using namespace gweni;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
    }

    ~Scene()
    {
        delete canvas;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    controls::TabControl *tabControl(std::vector<String> captions)
    {
        controls::TabControl *control=canvas->newChild<controls::TabControl>();
        control->setDock(DockPosition::Center);

        for(auto &&caption : captions)
            control->addPage(caption);
        return control;
    }
};

std::vector<controls::TabButton *> buttons(controls::TabControl *control)
{
    std::vector<controls::TabButton *> result;

    for(auto &&child : control->getTabStrip()->getChildren())
    {
        if(controls::TabButton *button=gweni_cast<controls::TabButton>(child))
            result.push_back(button);
    }
    return result;
}

Padding expectedPadding(DockPosition dock)
{
    if(dock == DockPosition::Bottom)
        return Padding(3, 1, 5, 4);
    if(dock == DockPosition::Top)
        return Padding(3, 3, 5, 2);
    return Padding(3, 2, 5, 2);
}

void checkPadding(controls::TabButton *button, DockPosition dock)
{
    const Padding expected=expectedPadding(dock);
    const Padding &padding=button->getPadding();

    CHECK(padding.left == expected.left);
    CHECK(padding.top == expected.top);
    CHECK(padding.right == expected.right);
    CHECK(padding.bottom == expected.bottom);
}

//! The tabs of \p control as laid out with its strip docked on \p dock.
void checkStrip(controls::TabControl *control, DockPosition dock)
{
    controls::TabStrip *strip=control->getTabStrip();
    const std::vector<controls::TabButton *> tabs=buttons(control);
    const bool vertical=dock == DockPosition::Left || dock == DockPosition::Right;
    int widest=0;

    REQUIRE(String(getDockPositionName(strip->getDock())) == getDockPositionName(dock));
    REQUIRE(!tabs.empty());

    for(size_t i=0; i < tabs.size(); ++i)
    {
        controls::TabButton *tab=tabs[i];
        const Rect &bounds=tab->getBounds();
        const Padding &padding=tab->getPadding();
        // the caption, text padding included
        const int captionWidth=tab->textWidth();
        const int wanted=captionWidth+padding.left+padding.right;

        CAPTURE(i);
        checkPadding(tab, dock);
        widest=std::max(widest, wanted);

        if(vertical)
        {
            // across the row a tab fills the strip, and stays inside it
            CHECK(bounds.x == 0);
            CHECK(bounds.w == strip->getWidth());
            CHECK(bounds.w >= wanted);
        }
        else
        {
            CHECK(bounds.w == wanted);
            CHECK(bounds.y == 0);
            CHECK(bounds.h == strip->getHeight());
        }

        if(i == 0)
            continue;

        // each tab after the first shares the 1 px border of the one before
        const Rect &before=tabs[i-1]->getBounds();

        if(vertical)
            CHECK(bounds.y == before.y+before.h-1);
        else
            CHECK(bounds.x == before.x+before.w-1);
    }

    if(vertical)
        CHECK(strip->getWidth() == widest);
}

}//namespace

TEST_CASE("tabs are as wide as their caption and padding from the first frame")
{
    Scene scene;
    controls::TabControl *control=scene.tabControl({"You", "Can", "Reorder", "These", "Tabs"});
    scene.frame();

    checkStrip(control, DockPosition::Top);
}

TEST_CASE("the tab padding and placement follow the strip's position")
{
    Scene scene;
    controls::TabControl *control=scene.tabControl({"Controls", "Red", "Green", "Blue"});
    scene.frame();

    for(DockPosition dock : {DockPosition::Bottom, DockPosition::Left, DockPosition::Right,
            DockPosition::Top})
    {
        const String dockName=getDockPositionName(dock);
        CAPTURE(dockName);
        control->setTabStripPosition(dock);
        scene.frame();
        checkStrip(control, dock);
    }
}

TEST_CASE("a tab moved into another TabControl takes that strip's padding")
{
    Scene scene;
    controls::TabControl *from=scene.tabControl({"One", "Two"});
    controls::TabControl *to=scene.tabControl({"Three"});
    to->setTabStripPosition(DockPosition::Bottom);
    scene.frame();

    controls::TabButton *moved=buttons(from).back();
    checkPadding(moved, DockPosition::Top);

    to->addPage(moved);
    scene.frame();

    CHECK(moved->getTabControl() == to);
    CHECK(moved->getParent() == to->getTabStrip());
    checkStrip(to, DockPosition::Bottom);
    checkStrip(from, DockPosition::Top);
}
