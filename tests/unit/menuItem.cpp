/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  MenuItem's checked state: a new item is unchecked (its members have in-class
 *  initialisers, so its constructor reads no indeterminate bool), and
 *  setChecked fires onCheckChange plus onChecked or onUnChecked only when the
 *  state actually changes.
 */
#include "headlessFixture.h"

#include <gweni/controls/menuItem.h>

#include <doctest/doctest.h>

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
};

//! Counts the check events one item fires.
struct CheckEvents
{
    int changes=0;
    int checks=0;
    int unchecks=0;

    explicit CheckEvents(controls::MenuItem *item)
    {
        item->onCheckChange.addCallback(item,
            [this](event::Handler &, event::Info const &) {++changes;});
        item->onChecked.addCallback(item,
            [this](event::Handler &, event::Info const &) {++checks;});
        item->onUnChecked.addCallback(item,
            [this](event::Handler &, event::Info const &) {++unchecks;});
    }

    bool are(int expectedChanges, int expectedChecks, int expectedUnchecks) const
    {
        return changes == expectedChanges && checks == expectedChecks && unchecks == expectedUnchecks;
    }
};

}//namespace

TEST_CASE("A new MenuItem is unchecked")
{
    Scene scene;
    controls::MenuItem *item=scene.canvas->newChild<controls::MenuItem>("item");
    CHECK_FALSE(item->cetChecked());
}

TEST_CASE("MenuItem::setChecked fires its events only when the state changes")
{
    Scene scene;
    controls::MenuItem *item=scene.canvas->newChild<controls::MenuItem>("item");
    CheckEvents events(item);

    item->setChecked(false);
    CHECK_FALSE(item->cetChecked());
    CHECK(events.are(0, 0, 0));

    item->setChecked(true);
    CHECK(item->cetChecked());
    CHECK(events.are(1, 1, 0));

    item->setChecked(true);
    CHECK(item->cetChecked());
    CHECK(events.are(1, 1, 0));

    item->setChecked(false);
    CHECK_FALSE(item->cetChecked());
    CHECK(events.are(2, 1, 1));
}
