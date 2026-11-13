/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  controls::SearchComboBox in plain mode, on a real Canvas with the stub
 *  skin, clicked through input::onMouseClicked and keyed through
 *  input::onKeyEvent.
 *
 *  Options are data: the first one added is selected with no event, as are
 *  setOptions()' first, clearOptions() leaves none, setSelectedIndex() fires
 *  nothing and refuses an index out of range, and getValue() is the selected
 *  option's name, or its label without one. Up and Down on the closed box
 *  step the selection, one onSelection per step, nothing past either end.
 *
 *  The list: a press opens a Menu on the canvas below the box, as wide as
 *  it, its rows the options' labels, the box's look marked open; a second
 *  press, a press outside and Escape close it. Open, Up and Down move the
 *  highlight from the selection through the options, wrapping at both ends,
 *  the highlighted option on a shown row; Return picks it (onSelection once,
 *  the list closed, the box focused), a click on a row picks that row's
 *  option, and a real mouse move over a row highlights its option. The pool,
 *  the controls and the live ids the open list takes are the same at 30 and
 *  at 3 000 options. A sibling grown above the box moves it with no layout of
 *  its own, and the open list is at the box's new bottom edge in that frame,
 *  in one pass, with layout verification on and off. In a Popup the list
 *  opens over it, a click on a row picks without closing the popup, and
 *  Escape closes the list before the popup. A box deleted with its list open
 *  takes the list with it, every id back.
 */
#include "headlessFixture.h"

#include <gweni/controls/button.h>
#include <gweni/controls/popup.h>
#include <gweni/controls/searchComboBox.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <string>

using namespace gweni;
using controls::SearchComboBox;

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
        // a second apart per press, so two presses are never a double click
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 600);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        input::onMouseClicked(canvas, 0, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    //! A box of \p count options "Option 0".. at a fixed place (in \p parent).
    SearchComboBox *box(size_t count, controls::Base *parent=nullptr)
    {
        SearchComboBox *box=(parent ? parent : canvas)->newChild<SearchComboBox>();

        box->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        box->setBounds(20, 20, 160, 20);

        std::vector<SearchComboBox::Option> options;

        for(size_t i=0; i < count; ++i)
            options.push_back({"Option "+std::to_string(i), "o"+std::to_string(i)});
        box->setOptions(std::move(options));
        return box;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void click(int x, int y)
    {
        clock+=1.0f;
        canvas->inputMouseMoved(x, y, 0, 0);
        input::onMouseClicked(canvas, 0, true);
        input::onMouseClicked(canvas, 0, false);
        frame();
    }

    void clickOn(controls::Base *control)
    {
        const Rect at=canvasRect(control);
        click(at.x+at.w/2, at.y+at.h/2);
    }

    void key(int key)
    {
        input::onKeyEvent(canvas, key, true);
        input::onKeyEvent(canvas, key, false);
        frame();
    }

    static Rect canvasRect(controls::Base *control)
    {
        const Point at=control->localPosToCanvas();
        return Rect(at.x, at.y, control->getWidth(), control->getHeight());
    }
};

class Counter: public event::Handler
{
public:
    void count(event::Info)
    {
        ++calls;
    }

    void listen(event::Caller &caller)
    {
        caller.add(this, &Counter::count);
    }

    int calls=0;
};

//! The row of the open list showing option \p option, or null.
controls::internal::SearchComboBoxRow *rowFor(SearchComboBox *box, int option)
{
    controls::internal::SearchComboBoxOptions *rows=box->getOptionsList();
    const std::vector<size_t> &shown=box->getShownOptions();

    for(size_t i=0; i < shown.size(); ++i)
    {
        if(static_cast<int>(shown[i]) == option)
            return gweni_cast<controls::internal::SearchComboBoxRow>(rows->rowForItem(i));
    }
    return nullptr;
}

//! The rows of the open list that are shown and highlighted.
int highlightedRows(SearchComboBox *box)
{
    int count=0;

    for(controls::Base *control : box->getOptionsList()->getRows())
    {
        auto *row=gweni_cast<controls::internal::SearchComboBoxRow>(control);

        if(row && row->visible() && row->isHighlighted())
            ++count;
    }
    return count;
}

//! Every control under \p control, itself included.
size_t subtreeSize(controls::Base *control)
{
    size_t count=1;

    for(controls::Base *child : control->getChildren())
        count+=subtreeSize(child);
    return count;
}

}//namespace

TEST_CASE("SearchComboBox: options are data, the first selected with no event, setSelectedIndex fires nothing")
{
    Scene scene;
    SearchComboBox *box=scene.canvas->newChild<SearchComboBox>();
    Counter selections;

    selections.listen(box->onSelection);
    CHECK(box->getSelectedIndex() == SearchComboBox::NoOption);
    CHECK(box->getValue() == "");

    box->addOption("Alpha", "a");
    box->addOption("Beta");
    CHECK(box->getOptionCount() == 2);
    CHECK(box->getSelectedIndex() == 0);
    CHECK(box->getText() == "Alpha");
    CHECK(box->getValue() == "a");

    box->setSelectedIndex(1);
    CHECK(box->getSelectedIndex() == 1);
    CHECK(box->getText() == "Beta");
    // no name: the label
    CHECK(box->getValue() == "Beta");

    // out of range: nothing selected
    box->setSelectedIndex(5);
    CHECK(box->getSelectedIndex() == SearchComboBox::NoOption);
    CHECK(box->getText() == "");

    box->setOptions({{"One", "1"}, {"Two", "2"}, {"Three", "3"}});
    CHECK(box->getSelectedIndex() == 0);
    CHECK(box->getValue() == "1");

    box->clearOptions();
    CHECK(box->getOptionCount() == 0);
    CHECK(box->getSelectedIndex() == SearchComboBox::NoOption);
    CHECK(box->getText() == "");
    CHECK(selections.calls == 0);
}

TEST_CASE("SearchComboBox: Up and Down on the closed box step the selection, one onSelection per step")
{
    Scene scene;
    SearchComboBox *box=scene.box(3);
    Counter selections;

    selections.listen(box->onSelection);
    scene.frame();
    box->focus();

    scene.key(keys::Down);
    CHECK(box->getSelectedIndex() == 1);
    CHECK(selections.calls == 1);
    scene.key(keys::Down);
    scene.key(keys::Down);
    CHECK(box->getSelectedIndex() == 2);
    CHECK(selections.calls == 2);
    scene.key(keys::Up);
    scene.key(keys::Up);
    scene.key(keys::Up);
    CHECK(box->getSelectedIndex() == 0);
    CHECK(selections.calls == 4);
    CHECK(box->getText() == "Option 0");
    CHECK(!box->isListOpen());
}

TEST_CASE("SearchComboBox: a press opens the list below the box, as wide, and a press or Escape closes it")
{
    Scene scene;
    SearchComboBox *box=scene.box(5);

    scene.frame();
    CHECK(box->getList() == nullptr);

    scene.clickOn(box);
    REQUIRE(box->isListOpen());

    controls::internal::SearchComboBoxList *list=box->getList();

    CHECK((list->getParent() == scene.canvas));
    CHECK(list->getAnchor() == box);
    CHECK(list->getX() == 20);
    CHECK(list->getY() == 40);
    CHECK(list->getWidth() == 160);
    CHECK(box->getShownOptions().size() == 5);
    // as high as its rows, every option on one
    CHECK(box->getOptionsList()->getHeight() == 5*box->getOptionsList()->getRowHeight());

    for(int i=0; i < 5; ++i)
    {
        auto *row=rowFor(box, i);

        REQUIRE(row);
        CHECK(row->visible());
        CHECK(row->getText() == "Option "+std::to_string(i));
    }

    // the box itself toggles it
    scene.clickOn(box);
    CHECK(!box->isListOpen());

    scene.clickOn(box);
    REQUIRE(box->isListOpen());
    scene.click(390, 590);
    CHECK(!box->isListOpen());

    scene.clickOn(box);
    REQUIRE(box->isListOpen());
    scene.key(keys::Escape);
    CHECK(!box->isListOpen());
    CHECK(box->getSelectedIndex() == 0);
}

TEST_CASE("SearchComboBox: open, Up and Down move the highlight, wrapping, Return picks it and a row click picks its own")
{
    Scene scene;
    SearchComboBox *box=scene.box(30);
    Counter selections;

    selections.listen(box->onSelection);
    box->setSelectedIndex(2);
    scene.frame();

    scene.clickOn(box);
    REQUIRE(box->isListOpen());
    // from the selection
    CHECK(box->getHighlightedIndex() == 2);
    CHECK(highlightedRows(box) == 1);
    REQUIRE(rowFor(box, 2));
    CHECK(rowFor(box, 2)->isHighlighted());

    scene.key(keys::Up);
    scene.key(keys::Up);
    CHECK(box->getHighlightedIndex() == 0);
    // wraps to the last, which is scrolled into view
    scene.key(keys::Up);
    CHECK(box->getHighlightedIndex() == 29);
    REQUIRE(rowFor(box, 29));
    CHECK(rowFor(box, 29)->isHighlighted());
    CHECK(highlightedRows(box) == 1);

    const Rect viewport=Scene::canvasRect(box->getOptionsList());
    const Rect last=Scene::canvasRect(rowFor(box, 29));

    CHECK(last.y >= viewport.y);
    CHECK(last.y+last.h <= viewport.y+viewport.h);

    // and back to the first
    scene.key(keys::Down);
    CHECK(box->getHighlightedIndex() == 0);
    REQUIRE(rowFor(box, 0));
    CHECK(rowFor(box, 0)->isHighlighted());
    CHECK(highlightedRows(box) == 1);
    CHECK(selections.calls == 0);

    scene.key(keys::Down);
    scene.key(keys::Return);
    CHECK(!box->isListOpen());
    CHECK(box->getSelectedIndex() == 1);
    CHECK(box->getText() == "Option 1");
    CHECK(selections.calls == 1);
    CHECK(KeyboardFocus == box);

    // a click on a row
    scene.clickOn(box);
    REQUIRE(box->isListOpen());
    REQUIRE(rowFor(box, 4));
    scene.clickOn(rowFor(box, 4));
    CHECK(!box->isListOpen());
    CHECK(box->getSelectedIndex() == 4);
    CHECK(selections.calls == 2);
    CHECK(KeyboardFocus == box);
}

TEST_CASE("SearchComboBox: a mouse move over a row highlights its option")
{
    Scene scene;
    SearchComboBox *box=scene.box(5);

    scene.frame();
    scene.clickOn(box);
    REQUIRE(box->isListOpen());
    CHECK(box->getHighlightedIndex() == 0);

    REQUIRE(rowFor(box, 3));
    const Rect row=Scene::canvasRect(rowFor(box, 3));

    scene.canvas->inputMouseMoved(row.x+5, row.y+row.h/2, 0, 0);
    scene.frame();
    CHECK(box->getHighlightedIndex() == 3);
    CHECK(rowFor(box, 3)->isHighlighted());
    CHECK(!rowFor(box, 0)->isHighlighted());
    CHECK(highlightedRows(box) == 1);
}

TEST_CASE("SearchComboBox: the open list's pool, controls and live ids are the same at 30 and 3 000 options")
{
    struct Open
    {
        size_t rows;
        size_t controls;
        size_t primitives;
        size_t texts;
    };

    auto open=[](size_t count) -> Open
        {
            Scene scene;
            SearchComboBox *box=scene.box(count);

            scene.frame();
            scene.frame();

            const test::IdBankCounts before=test::IdBankCounts::now();
            const size_t controlsBefore=subtreeSize(scene.canvas);

            scene.clickOn(box);
            REQUIRE(box->isListOpen());
            CHECK(box->getShownOptions().size() == count);

            const test::IdBankCounts after=test::IdBankCounts::now();

            return {box->getOptionsList()->getRows().size(), subtreeSize(scene.canvas)-controlsBefore,
                after.livePrimitives()-before.livePrimitives(), after.liveTexts()-before.liveTexts()};
        };

    const Open small=open(30);
    const Open large=open(3000);

    CHECK(small.rows == SearchComboBox::MaxShownRows+1);
    CHECK(large.rows == small.rows);
    CHECK(large.controls == small.controls);
    CHECK(large.primitives == small.primitives);
    CHECK(large.texts == small.texts);
}

TEST_CASE("SearchComboBox: the open list follows the box in the frame a sibling's layout moves it")
{
    const bool verify=controls::layoutVerifyEnabled();
    const size_t verifyFailures=controls::layoutVerifyFailures();

    for(bool verifying : {false, true})
    {
        CAPTURE(verifying);
        controls::setLayoutVerify(verifying);

        Scene scene;
        controls::Base *top=scene.canvas->newChild<controls::Base>();

        top->setDock(DockPosition::Top);
        top->setSizeFlags({SizeFlag::Expand, SizeFlag::Fixed});
        top->setHeight(20);

        controls::Base *panel=scene.canvas->newChild<controls::Base>();

        panel->setDock(DockPosition::Center);

        SearchComboBox *box=scene.box(5, panel);

        scene.frame();
        scene.clickOn(box);
        REQUIRE(box->isListOpen());

        for(int i=0; i < 3; ++i)
            scene.frame();
        REQUIRE(scene.canvas->getLayoutStats().passes == 0);

        controls::internal::SearchComboBoxList *list=box->getList();

        REQUIRE(Scene::canvasRect(box).y == 40);
        CHECK(list->getY() == 60);

        top->setHeight(60);
        scene.frame();
        REQUIRE(Scene::canvasRect(box).y == 80);
        CHECK(scene.canvas->getLayoutStats().passes == 1);
        CHECK_FALSE(scene.canvas->getLayoutStats().renderLayout);
        CHECK(list->getX() == Scene::canvasRect(box).x);
        CHECK(list->getY() == 100);
        CHECK(list->getWidth() == 160);

        scene.frame();
        CHECK(scene.canvas->getLayoutStats().passes == 0);
        CHECK(scene.renderer.frameStats().primitiveSlotsWritten == 0);
        CHECK(scene.renderer.frameStats().textSlotsWritten == 0);
    }
    CHECK(controls::layoutVerifyFailures() == verifyFailures);
    controls::setLayoutVerify(verify);
}

TEST_CASE("SearchComboBox: in a Popup the list opens over it, a row click keeps it open, Escape closes the list first")
{
    Scene scene;
    controls::Button *anchor=scene.canvas->newChild<controls::Button>();

    anchor->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    anchor->setBounds(10, 10, 60, 20);

    controls::Popup *popup=anchor->createPopup();
    SearchComboBox *box=scene.box(5, popup);

    scene.frame();
    popup->open(anchor);
    scene.frame();
    REQUIRE(popup->isOpen());

    scene.clickOn(box);
    REQUIRE(box->isListOpen());
    CHECK(popup->isOpen());
    CHECK(box->getList()->getZIndex() > popup->getZIndex());

    REQUIRE(rowFor(box, 3));
    scene.clickOn(rowFor(box, 3));
    CHECK(!box->isListOpen());
    CHECK(popup->isOpen());
    CHECK(box->getSelectedIndex() == 3);

    scene.clickOn(box);
    REQUIRE(box->isListOpen());
    scene.key(keys::Escape);
    CHECK(!box->isListOpen());
    CHECK(popup->isOpen());
    scene.key(keys::Escape);
    CHECK(!popup->isOpen());
}

TEST_CASE("SearchComboBox: deleted with its list open, the box takes the list with it, every id back")
{
    Scene scene;

    scene.frame();

    const test::IdBankCounts before=test::IdBankCounts::now();
    const size_t controlsBefore=subtreeSize(scene.canvas);
    SearchComboBox *box=scene.box(20);

    scene.frame();
    scene.clickOn(box);
    REQUIRE(box->isListOpen());

    box->delayedDelete();
    scene.frame();
    scene.frame();

    const test::IdBankCounts after=test::IdBankCounts::now();

    CHECK(subtreeSize(scene.canvas) == controlsBefore);
    CHECK(after.livePrimitives() == before.livePrimitives());
    CHECK(after.liveTexts() == before.liveTexts());
}
