/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A ComboBox's items leaving its list, and the list's lifetime (#515), on a
 *  real Canvas drawn by the StubSkin on a StubRenderer.
 *
 *  The selected item leaving the list, deleted outright, delayedDelete()d (once
 *  the canvas deletes it), cleared with Menu::clearItems() on the list (once
 *  processed) or given another parent, drops the selection, whether or not the
 *  list was ever opened: getSelectedItem() is null, the box shows "" (its Text
 *  marked StateChange_Text in that frame's update phase), its preferred width
 *  no longer counts the item, a property::ComboBox's value is "", Up and Down
 *  select nothing, onSelection does not fire, and the next item added is the
 *  default selection again. Until the canvas deletes a delayedDelete()d item it
 *  is still selected and shown. An item that is not selected leaving keeps the
 *  selection and its text, and the width is measured again, opened list or not.
 *  An item that left the list, clicked in another box's list it was given to,
 *  is not selected, nor is another box's item handed to selectItem(), so no
 *  selection outlives an item deleted outside the box's list.
 *
 *  Deleting a ComboBox deletes its list, whether the list was never opened (it
 *  is the box's child), opened and closed or open (it is the canvas's child):
 *  once the delayed deletes have run no Menu is left on the canvas and the live
 *  primitive and text ids are back where they were before the box was made. An
 *  item of a list waiting for its delete may still be deleted without reaching
 *  the dead box, and a box delayedDelete()d from its own onSelection handler
 *  (a click on an item of its open list) takes its list with it. A canvas
 *  deleted with the list opened deletes each once, whichever of the list and
 *  the box comes first among the canvas's children. An opened list taken off
 *  every canvas is deleted with the box at once.
 *
 *  A list deleted from outside takes its items, the selection and the shown
 *  text with it; the box then opens, steps and selects nothing until an item
 *  is added, which makes it a new list.
 */
#include "headlessFixture.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/comboBox.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/menuItem.h>
#include <gweni/controls/property/comboBox.h>
#include <gweni/controls/text.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <iterator>

using namespace gweni;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    float clock=10.0f;

    Scene()
    {
        // a second apart per click, so two clicks are never a double click
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        deleteCanvas();
        platform::setTimeSource(nullptr);
    }

    void deleteCanvas()
    {
        if(!canvas)
            return;

        // the hovered control, the focus and the button state are process-global
        input::onMouseClicked(canvas, 0, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        canvas=nullptr;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void settle()
    {
        for(int i=0; i < 5; ++i)
            frame();
    }

    //! A whole left click at \p at, without a frame.
    void click(Point at)
    {
        clock+=1.0f;
        canvas->inputMouseMoved(at.x, at.y, 0, 0);
        canvas->inputMouseButton(0, true);
        canvas->inputMouseButton(0, false);
    }

    int width(controls::ComboBox *combo, const String &widest)
    {
        const Padding &textPadding=combo->getTextPadding();
        const Padding &padding=combo->getPadding();

        return renderer.measureText(combo->getFont(), widest).x+
            textPadding.left+textPadding.right+padding.left+padding.right;
    }

    size_t menusOnCanvas()
    {
        size_t menus=0;

        for(auto &&child : canvas->getChildren())
        {
            if(gweni_cast<controls::Menu>(child))
                ++menus;
        }
        return menus;
    }
};

//! Counts onSelection calls.
struct Listener: event::Handler
{
    int calls=0;
    controls::ComboBox *deleteOnSelection=nullptr;

    void onSelection(event::Info)
    {
        ++calls;
        if(deleteOnSelection)
            deleteOnSelection->delayedDelete();
    }
};

//! The list an item of a ComboBox is in: its logical parent.
controls::Menu *listOf(controls::MenuItem *item)
{
    return gweni_cast<controls::Menu>(item->getParent());
}

//! The Text a ComboBox shows its selection in.
controls::internal::Text *shownText(controls::ComboBox *combo)
{
    for(auto &&child : combo->getChildren())
    {
        if(auto *text=gweni_cast<controls::internal::Text>(child))
            return text;
    }
    return nullptr;
}

//! A ComboBox in a plain box on the canvas with "One", "The longest item" and
//! "Middle one", the first selected, settled; with \p opened its list opened
//! and closed once, which makes it the canvas's child.
struct Box
{
    controls::Base *host;
    controls::ComboBox *combo;
    controls::MenuItem *one;
    controls::MenuItem *longest;
    controls::MenuItem *middle;
    Listener listener;

    Box(Scene &scene, bool opened)
    {
        host=scene.canvas->newChild<controls::Base>("host");
        host->setBounds(10, 10, 300, 100);
        combo=host->newChild<controls::ComboBox>();
        one=combo->addItem("One", "one");
        longest=combo->addItem("The longest item", "longest");
        middle=combo->addItem("Middle one", "middle");
        combo->onSelection.add(&listener, &Listener::onSelection);
        scene.settle();

        if(opened)
        {
            combo->openList();
            scene.settle();
            combo->closeList();
            scene.settle();
        }
    }
};

//! How the selected item leaves the list.
enum class Removal
{
    Delete,
    DelayedDelete,
    MenuClearItems,
    Reparent
};

//! Removes \p box's selected item ("One") by \p how (Reparent moves it to
//! \p other).
void removeSelected(Box &box, Removal how, controls::Base *other)
{
    switch(how)
    {
    case Removal::Delete:
        delete box.one;
        break;
    case Removal::DelayedDelete:
        box.one->delayedDelete();
        break;
    case Removal::MenuClearItems:
        listOf(box.one)->clearItems();
        break;
    case Removal::Reparent:
        box.one->setParent(other);
        break;
    }
}

//! After the update phase that removed the selected item. \p empty is a
//! ComboBox with no items, for the width a box left with none has.
void checkSelectionDropped(Scene &scene, Box &box, Removal how, controls::ComboBox *empty)
{
    controls::ComboBox *combo=box.combo;

    CHECK(combo->getSelectedItem() == nullptr);
    CHECK(combo->getText() == "");
    // marked in the update phase that removed it, so this frame draws it
    REQUIRE(shownText(combo));
    CHECK((shownText(combo)->getStateChange() & controls::StateChange_Text) != 0);
    scene.canvas->renderCanvas();
    scene.settle();

    CHECK(combo->getSelectedItem() == nullptr);
    CHECK(combo->getText() == "");

    if(how == Removal::MenuClearItems)
    {
        // every item went, so the width is the empty box's
        CHECK(combo->getPreferredSize().width == empty->getPreferredSize().width);
    }
    else
    {
        CHECK(combo->getPreferredSize().width == scene.width(combo, "The longest item"));

        // Up and Down have no selection to move from.
        combo->onKeyDown(true);
        CHECK(combo->getSelectedItem() == nullptr);
        combo->onKeyUp(true);
        CHECK(combo->getSelectedItem() == nullptr);
        CHECK(combo->getText() == "");
    }

    CHECK(box.listener.calls == 0);

    // The next item added is the default again, with no onSelection.
    controls::MenuItem *again=combo->addItem("Again", "again");
    scene.settle();
    CHECK(combo->getSelectedItem() == again);
    CHECK(combo->getText() == "Again");
    CHECK(box.listener.calls == 0);
}

void runRemoval(Removal how, bool opened)
{
    Scene scene;
    controls::Base *other=scene.canvas->newChild<controls::Base>("other");
    controls::ComboBox *empty=other->newChild<controls::ComboBox>();
    Box box(scene, opened);

    REQUIRE(box.combo->getSelectedItem() == box.one);
    REQUIRE(listOf(box.one));
    REQUIRE(box.combo->getPreferredSize().width == scene.width(box.combo, "The longest item"));

    removeSelected(box, how, other);

    if((how == Removal::DelayedDelete) || (how == Removal::MenuClearItems))
    {
        // alive until the canvas deletes it, at the top of the next update
        // phase: still selected and shown
        CHECK(box.combo->getSelectedItem() == box.one);
        CHECK(box.combo->getText() == "One");
    }

    scene.canvas->updateCanvas();
    checkSelectionDropped(scene, box, how, empty);
}

} // namespace

TEST_CASE("a ComboBox's selected item deleted outright drops the selection")
{
    SUBCASE("list never opened") { runRemoval(Removal::Delete, false); }
    SUBCASE("list opened and closed") { runRemoval(Removal::Delete, true); }
}

TEST_CASE("a ComboBox's selected item delayedDelete()d drops the selection once deleted")
{
    SUBCASE("list never opened") { runRemoval(Removal::DelayedDelete, false); }
    SUBCASE("list opened and closed") { runRemoval(Removal::DelayedDelete, true); }
}

TEST_CASE("Menu::clearItems() on a ComboBox's list drops the selection once processed")
{
    SUBCASE("list never opened") { runRemoval(Removal::MenuClearItems, false); }
    SUBCASE("list opened and closed") { runRemoval(Removal::MenuClearItems, true); }
}

TEST_CASE("a ComboBox's selected item given another parent drops the selection")
{
    SUBCASE("list never opened") { runRemoval(Removal::Reparent, false); }
    SUBCASE("list opened and closed") { runRemoval(Removal::Reparent, true); }
}

TEST_CASE("a ComboBox selects no item outside its own list")
{
    Scene scene;
    Box box(scene, false);
    controls::ComboBox *second=box.host->newChild<controls::ComboBox>();

    second->setPos(0, 40);
    controls::MenuItem *secondsOwn=second->addItem("Second's own", "own");
    scene.settle();

    // The first box's selected item given to the second box's list: still
    // connected to the first box, whose selection it left.
    box.one->setParent(listOf(secondsOwn));
    scene.settle();
    REQUIRE(box.combo->getSelectedItem() == nullptr);

    second->openList();
    scene.settle();
    REQUIRE(second->isMenuOpen());

    const Point at=box.one->localPosToCanvas(
        Point(box.one->getWidth()/2, box.one->getHeight()/2));

    // a click on it there reaches the first box, which refuses it
    scene.click(at);
    scene.settle();
    CHECK(box.combo->getSelectedItem() == nullptr);
    CHECK(box.combo->getText() == "");
    CHECK(box.listener.calls == 0);

    // and so it does another box's item, or none, handed in directly
    box.combo->selectItem(secondsOwn);
    box.combo->selectItem(nullptr);
    scene.settle();
    CHECK(box.combo->getSelectedItem() == nullptr);
    CHECK(box.listener.calls == 0);
    CHECK(second->getSelectedItem() == secondsOwn);

    // deleted there, it leaves the first box nothing to read (ASAN)
    delete box.one;
    scene.settle();
    CHECK(box.combo->getSelectedItem() == nullptr);
    box.combo->onKeyDown(true);
    box.combo->onKeyUp(true);
    CHECK(box.combo->getSelectedItem() == nullptr);

    // its own items it still selects
    box.combo->selectItem(box.middle);
    CHECK(box.combo->getSelectedItem() == box.middle);
    CHECK(box.combo->getText() == "Middle one");
    CHECK(box.listener.calls == 1);
}

TEST_CASE("a property::ComboBox's value is empty once its selected item is deleted")
{
    Scene scene;
    auto *property=scene.canvas->newChild<controls::property::ComboBox>();
    controls::ComboBox *combo=property->getComboBox();
    controls::MenuItem *first=combo->addItem("First", "first");

    combo->addItem("Second", "second");
    scene.settle();
    REQUIRE(property->getPropertyValue() == "first");

    delete first;
    scene.settle();

    // checked first: getPropertyValue() reads the selected item's name
    REQUIRE(combo->getSelectedItem() == nullptr);
    CHECK(property->getPropertyValue() == "");
}

TEST_CASE("an item that is not selected leaving a ComboBox keeps the selection and is measured out")
{
    bool opened=false;

    SUBCASE("list never opened") { opened=false; }
    SUBCASE("list opened and closed") { opened=true; }

    Scene scene;
    Box box(scene, opened);
    controls::ComboBox *combo=box.combo;

    REQUIRE(combo->getPreferredSize().width == scene.width(combo, "The longest item"));

    delete box.longest;
    scene.settle();

    CHECK(combo->getSelectedItem() == box.one);
    CHECK(combo->getText() == "One");
    CHECK(combo->getPreferredSize().width == scene.width(combo, "Middle one"));

    box.middle->delayedDelete();
    scene.settle();

    CHECK(combo->getSelectedItem() == box.one);
    CHECK(combo->getText() == "One");
    CHECK(combo->getPreferredSize().width == scene.width(combo, "One"));
    CHECK(box.listener.calls == 0);
}

TEST_CASE("deleting a ComboBox deletes its list, opened or not")
{
    enum class State { NeverOpened, OpenedAndClosed, Open };
    State state=State::NeverOpened;

    SUBCASE("list never opened") { state=State::NeverOpened; }
    SUBCASE("list opened and closed") { state=State::OpenedAndClosed; }
    SUBCASE("list open") { state=State::Open; }

    Scene scene;

    scene.settle();

    const size_t menus=scene.menusOnCanvas();
    const size_t children=scene.canvas->getChildren().size();
    const test::IdBankCounts baseline=test::IdBankCounts::now();

    controls::ComboBox *combo=scene.canvas->newChild<controls::ComboBox>();

    combo->setPos(10, 10);
    combo->addItem("One", "one");
    controls::MenuItem *two=combo->addItem("Two", "two");
    combo->addItem("Three", "three");
    scene.settle();

    if(state != State::NeverOpened)
    {
        combo->openList();
        scene.settle();
        // the list is drawn, so it holds ids of its own
        CHECK(scene.menusOnCanvas() == menus+1);
    }
    if(state == State::OpenedAndClosed)
    {
        combo->closeList();
        scene.settle();
    }

    delete combo;

    // An item of a list waiting for its delete may still go, and must not
    // reach the box.
    if(state != State::NeverOpened)
        delete two;

    scene.settle();

    CHECK(scene.menusOnCanvas() == menus);
    CHECK(scene.canvas->getChildren().size() == children);

    const test::IdBankCounts after=test::IdBankCounts::now();

    CHECK(after.livePrimitives() == baseline.livePrimitives());
    CHECK(after.liveTexts() == baseline.liveTexts());
}

TEST_CASE("a ComboBox delayedDelete()d from its own onSelection handler takes its list with it")
{
    Scene scene;

    scene.settle();

    const size_t menus=scene.menusOnCanvas();
    const size_t children=scene.canvas->getChildren().size();
    const test::IdBankCounts baseline=test::IdBankCounts::now();

    Listener listener;
    controls::ComboBox *combo=scene.canvas->newChild<controls::ComboBox>();

    combo->setPos(10, 10);
    combo->addItem("One", "one");
    controls::MenuItem *two=combo->addItem("Two", "two");
    combo->onSelection.add(&listener, &Listener::onSelection);
    listener.deleteOnSelection=combo;
    scene.settle();

    combo->openList();
    scene.settle();
    REQUIRE(combo->isMenuOpen());

    const Point at=two->localPosToCanvas(Point(two->getWidth()/2, two->getHeight()/2));

    scene.click(at);
    CHECK(listener.calls == 1);
    scene.settle();

    CHECK(scene.menusOnCanvas() == menus);
    CHECK(scene.canvas->getChildren().size() == children);

    const test::IdBankCounts after=test::IdBankCounts::now();

    CHECK(after.livePrimitives() == baseline.livePrimitives());
    CHECK(after.liveTexts() == baseline.liveTexts());
}

TEST_CASE("a canvas deleted with a ComboBox's list opened deletes each once")
{
    bool listFirst=false;
    bool open=false;

    SUBCASE("box before list") { listFirst=false; }
    SUBCASE("list before box") { listFirst=true; }
    SUBCASE("box before open list") { listFirst=false; open=true; }
    SUBCASE("open list before box") { listFirst=true; open=true; }

    Scene scene;
    Box box(scene, true);

    if(open)
    {
        box.combo->openList();
        scene.settle();
    }

    // the list went to the end of the canvas's children when it was opened
    if(listFirst)
    {
        box.host->bringToFront();
        scene.settle();
    }

    controls::Base::List &children=scene.canvas->getChildren();
    controls::Menu *list=listOf(box.middle);

    REQUIRE(list);
    REQUIRE(list->getParent() == static_cast<controls::Base *>(scene.canvas));

    const auto at=[&](controls::Base *control)
    {
        return std::distance(children.begin(), std::find(children.begin(), children.end(), control));
    };

    CHECK((at(list) < at(box.host)) == listFirst);

    // ASAN (in that tree) reports a control deleted twice or touched after
    scene.deleteCanvas();
}

TEST_CASE("a ComboBox's opened list taken off the canvas is deleted with the box")
{
    Scene scene;

    scene.settle();

    const test::IdBankCounts baseline=test::IdBankCounts::now();
    controls::ComboBox *combo=scene.canvas->newChild<controls::ComboBox>();
    controls::MenuItem *one=combo->addItem("One", "one");

    combo->addItem("Two", "two");
    combo->openList();
    scene.settle();

    controls::Menu *list=listOf(one);

    REQUIRE(list);
    // off every canvas, so no canvas could delete it later
    list->setParent(nullptr);
    const bool offCanvas=list->getCanvas() == nullptr;

    REQUIRE(offCanvas);

    delete combo;
    scene.settle();

    const test::IdBankCounts after=test::IdBankCounts::now();

    CHECK(scene.menusOnCanvas() == 0);
    CHECK(after.livePrimitives() == baseline.livePrimitives());
    CHECK(after.liveTexts() == baseline.liveTexts());
}

TEST_CASE("a ComboBox whose list is deleted from outside holds nothing of it and makes a new one")
{
    bool opened=false;

    SUBCASE("list never opened") { opened=false; }
    SUBCASE("list opened and closed") { opened=true; }

    Scene scene;

    scene.settle();

    const size_t menus=scene.menusOnCanvas();
    const test::IdBankCounts baseline=test::IdBankCounts::now();
    Box box(scene, opened);
    controls::ComboBox *combo=box.combo;

    delete static_cast<controls::Base *>(listOf(box.one));
    scene.settle();

    CHECK(combo->getSelectedItem() == nullptr);
    CHECK(combo->getText() == "");
    CHECK(combo->isMenuOpen() == false);

    // with no list, nothing to open, step through or select
    combo->openList();
    combo->closeList();
    combo->onKeyDown(true);
    combo->onKeyUp(true);
    combo->selectItemByName("middle");
    scene.settle();
    CHECK(combo->getSelectedItem() == nullptr);
    CHECK(box.listener.calls == 0);

    controls::MenuItem *again=combo->addItem("Again", "again");

    scene.settle();
    CHECK(combo->getSelectedItem() == again);
    CHECK(combo->getText() == "Again");
    REQUIRE(listOf(again));
    CHECK(listOf(again)->getParent() == static_cast<controls::Base *>(combo));

    combo->openList();
    scene.settle();
    CHECK(combo->isMenuOpen());

    delete box.host;
    scene.settle();

    const test::IdBankCounts after=test::IdBankCounts::now();

    CHECK(scene.menusOnCanvas() == menus);
    CHECK(after.livePrimitives() == baseline.livePrimitives());
    CHECK(after.liveTexts() == baseline.liveTexts());
}
