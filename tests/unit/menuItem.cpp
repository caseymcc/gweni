/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  MenuItem's checked state: a new item is unchecked (its members have in-class
 *  initialisers, so its constructor reads no indeterminate bool), and
 *  setChecked fires onCheckChange plus onChecked or onUnChecked only when the
 *  state actually changes.
 *
 *  The uninitialised m_checked this guards against (#251) was harmless at -O0
 *  and showed only under -DGWENI_SANITIZE=undefined.
 *
 *  A click (doAction, which is onPress) toggles only a checkable item; any
 *  other leaf item stays unchecked and fires no check event, and both fire
 *  onMenuItemSelected (#464). A real change of setChecked or setCheckable marks
 *  the item StateChange_Background, so a shown menu draws it on the next frame,
 *  and a call that changes nothing marks nothing. A checkable item's image is
 *  hidden while it is checked and shown otherwise, whatever order setImage,
 *  setCheckable and setChecked come in.
 */
#include "headlessFixture.h"

#include <gweni/controls/menuItem.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <array>

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

    //! Both phases, so every pending state change is drawn and cleared.
    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }
};

//! The item's ImagePanel, or null.
controls::Base *imageOf(controls::MenuItem *item)
{
    for(controls::Base *child : item->getChildren())
    {
        if(child->getTypeName() == std::string("ImagePanel"))
            return child;
    }
    return nullptr;
}

//! Counts an item's onMenuItemSelected calls.
struct Selections
{
    int count=0;

    explicit Selections(controls::MenuItem *item)
    {
        item->onMenuItemSelected.addCallback(item,
            [this](event::Handler &, event::Info const &) {++count;});
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
    CHECK_FALSE(item->isChecked());
}

TEST_CASE("MenuItem::setChecked fires its events only when the state changes")
{
    Scene scene;
    controls::MenuItem *item=scene.canvas->newChild<controls::MenuItem>("item");
    CheckEvents events(item);

    item->setChecked(false);
    CHECK_FALSE(item->isChecked());
    CHECK(events.are(0, 0, 0));

    item->setChecked(true);
    CHECK(item->isChecked());
    CHECK(events.are(1, 1, 0));

    item->setChecked(true);
    CHECK(item->isChecked());
    CHECK(events.are(1, 1, 0));

    item->setChecked(false);
    CHECK_FALSE(item->isChecked());
    CHECK(events.are(2, 1, 1));
}

TEST_CASE("A click on a MenuItem that is not checkable leaves it unchecked and fires no check event")
{
    Scene scene;
    controls::Menu *menu=scene.canvas->newChild<controls::Menu>();
    controls::MenuItem *item=menu->addItem("Save");
    CheckEvents events(item);
    Selections selections(item);

    CHECK_FALSE(item->isCheckable());
    item->doAction();
    CHECK_FALSE(item->isChecked());
    CHECK(events.are(0, 0, 0));
    CHECK(selections.count == 1);

    item->doAction();
    CHECK_FALSE(item->isChecked());
    CHECK(events.are(0, 0, 0));
    CHECK(selections.count == 2);
}

TEST_CASE("A click on a checkable MenuItem toggles it and fires onMenuItemSelected")
{
    Scene scene;
    controls::Menu *menu=scene.canvas->newChild<controls::Menu>();
    controls::MenuItem *item=menu->addItem("Checkable");
    item->setCheckable(true);
    CHECK(item->isCheckable());
    CheckEvents events(item);
    Selections selections(item);

    item->doAction();
    CHECK(item->isChecked());
    CHECK(events.are(1, 1, 0));
    CHECK(selections.count == 1);

    item->doAction();
    CHECK_FALSE(item->isChecked());
    CHECK(events.are(2, 1, 1));
    CHECK(selections.count == 2);
}

TEST_CASE("MenuItem::setChecked and setCheckable mark the item only on a real change")
{
    Scene scene;
    controls::Menu *menu=scene.canvas->newChild<controls::Menu>();
    controls::MenuItem *item=menu->addItem("Item");
    menu->setHidden(false);
    scene.frame();
    REQUIRE(item->getStateChange() == controls::StateChange_Nothing);

    item->setCheckable(false);
    item->setChecked(false);
    CHECK(item->getStateChange() == controls::StateChange_Nothing);

    item->setCheckable(true);
    CHECK((item->getStateChange()&controls::StateChange_Background) != 0);
    scene.frame();
    REQUIRE(item->getStateChange() == controls::StateChange_Nothing);

    item->setCheckable(true);
    CHECK(item->getStateChange() == controls::StateChange_Nothing);

    item->setChecked(true);
    CHECK((item->getStateChange()&controls::StateChange_Background) != 0);
    scene.frame();
    REQUIRE(item->getStateChange() == controls::StateChange_Nothing);

    item->setChecked(true);
    CHECK(item->getStateChange() == controls::StateChange_Nothing);

    item->setChecked(false);
    CHECK((item->getStateChange()&controls::StateChange_Background) != 0);
}

TEST_CASE("A checkable MenuItem's image is hidden while it is checked, in every call order")
{
    enum Call { Image, Checkable, Checked };
    std::array<Call, 3> order{Image, Checkable, Checked};

    do
    {
        CAPTURE(order[0]);
        CAPTURE(order[1]);
        CAPTURE(order[2]);
        Scene scene;
        controls::MenuItem *item=scene.canvas->newChild<controls::MenuItem>("item");

        for(Call call : order)
        {
            controls::Base *image=imageOf(item);

            // before the last call, the image (if there is one) still shows
            if(image)
                CHECK_FALSE(image->hidden());

            switch(call)
            {
            case Image: item->setImage("icon.png"); break;
            case Checkable: item->setCheckable(true); break;
            case Checked: item->setChecked(true); break;
            }
        }

        controls::Base *image=imageOf(item);
        REQUIRE(image != nullptr);
        CHECK(image->hidden());

        item->setChecked(false);
        CHECK_FALSE(image->hidden());
        item->setChecked(true);
        CHECK(image->hidden());
        item->setCheckable(false);
        CHECK_FALSE(image->hidden());
        item->setCheckable(true);
        CHECK(image->hidden());

        // a new image keeps the state's visibility
        item->setImage("other.png");
        CHECK(imageOf(item) == image);
        CHECK(image->hidden());
    } while(std::next_permutation(order.begin(), order.end()));
}
