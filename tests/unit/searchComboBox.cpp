/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  controls::SearchComboBox, plain, filterable, free text and multi-select, on
 *  a real Canvas with the stub skin, clicked through input::onMouseClicked and
 *  keyed through input::onKeyEvent.
 *
 *  Options are data: the first one added is selected with no event, as are
 *  setOptions()' first, clearOptions() holds the selection (still the value
 *  and the text, at no index) until setSelectedIndex(NoOption) drops it,
 *  setSelectedIndex() fires nothing and refuses an index out of range, and
 *  getValue() is the selected option's name, or its label without one. Up and Down on the closed box
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
 *
 *  Filterable: the field shows the selection (the box's own text empty) and
 *  has the focus; typing (onTextChangedCaller once per edit, typing over a
 *  selection included) opens the list on the options whose label contains
 *  the query, case-insensitively and anywhere in it, the first highlighted,
 *  every option for an empty query. Up and Down wrap through the shown
 *  options without editing the field, and Return picks (onSelection once)
 *  with the field keeping the focus through the key's release. Escape, a
 *  press on another control and Tab away each close the list and give the
 *  field back the selection's label. No match shows one disabled "No
 *  matches" row, which Return and a click leave alone. A press and release
 *  on a row keep the field focused (its edit kept between them) and pick. A
 *  press on the box's chevron column focuses the field, opens the list on
 *  every option with the selection highlighted and its label selected in the
 *  field, and a second one closes it. A keystroke binds again only the rows
 *  whose option changed (getRowBindCount), makes and deletes no control, lays
 *  out in one pass, and a shown row keeping its option and highlight takes
 *  no Text or Background change; at 30 and 3 000 options it keeps the same
 *  pool and makes and frees no control or id.
 *
 *  Free text: a text that is no option's label shows the Add “<text>” row
 *  after the matches (Up wraps onto it), alone and highlighted when nothing
 *  matches, enabled, and none for an option's label in another case. Return
 *  on it and a click on it fire onCustomValue once with the text, which
 *  becomes the value (getValue() the text, getSelectedIndex() NoOption, no
 *  onSelection), until an option is picked. A refused text stays in the field
 *  in Colors.SearchComboRefusedText with the value and the list as they were;
 *  an edit gives the field its colour back, a refusal on Tab leaves the text
 *  refused with the list closed, and Escape gives the value back. A press
 *  elsewhere takes a typed text, Tab with an option's label (case aside)
 *  picks that option, an emptied field gets the value back, and Return takes
 *  the text with the list closed by the box's chevron, the focus staying.
 *  An option the handler adds, one added later and setOptions() leave a
 *  free-text value the value. A pick of the selected option (Return, a row
 *  click, its label in another case and Tab) shows its label in the field
 *  and is not taken as a free-text value when the focus moves on, a refused
 *  text included; the value taken is the text typed, whatever the handler
 *  sets the field to. A disabled box closes its list, and a press where a
 *  row was picks nothing; a filterable box's font reaches its field. Options
 *  refilled from onTextChangedCaller (setOptions(), or clearOptions() and
 *  addOption()) leave the typed text and the query in the field, the list
 *  showing the new options that match it; a selection gone from them is
 *  held, so Escape and a lost focus give its label back (no onSelection) and
 *  the pick ending the edit fires onSelection, and a refill after it, or
 *  after a free-text field typed back to the value's own text loses the
 *  focus, holds the value too. setOptions() with the list closed leaves no
 *  shown option or highlight of the old ones, and keeps the selection and
 *  multi-select picks on options of the same label and name, holding the
 *  others (their chips and values kept, out of the indices, with no event)
 *  until an equal option comes back through setOptions() or addOption(); a
 *  held pick's x fires onSelectionChange with NoOption, and
 *  setSelectedIndices() drops held picks. A multi-select box refilled while
 *  the user types keeps its picks the results leave out. A free-text value
 *  wider than every option widens a size-to-content box, and an option picked
 *  again narrows it back.
 *
 *  Multi-select: no selection and no text on the box; a row click and Return
 *  each add the option as a chip, in pick order, the list staying open with
 *  the row checked and highlighted, and a picked option picked again goes
 *  (onSelectionChange once each, onSelection never). A chip's x and Backspace
 *  in the empty field take a pick away (the chip deleted with the frame),
 *  Backspace in a field with text edits it, and on a plain box it takes the
 *  last pick; Up and Down on the closed box open the list. setSelectedIndices
 *  fires nothing and leaves out repeated and out-of-range indices, and
 *  getSelectedValues gives names (labels without one). Two chips and the
 *  field's least width fit a box made that wide; a third wraps to a second
 *  row with the field after it, the box taller and the open list at its new
 *  bottom in that frame, in one pass, and removing it gives the one row back,
 *  the list following again, with layout verification off and on. A free-text
 *  value is a chip of its own, once, the field emptied and the list on every
 *  option again; an option's label typed in full and a lost focus pick it.
 *  Multi-select off gives the box back the height flag it had.
 */
#include "headlessFixture.h"

#include <gweni/controls/button.h>
#include <gweni/controls/chip.h>
#include <gweni/controls/popup.h>
#include <gweni/controls/searchComboBox.h>
#include <gweni/controls/textBox.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <set>
#include <string>

using namespace gweni;
using controls::SearchComboBox;
using controls::Chip;

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
    void count(event::Info info)
    {
        ++calls;
        last=info.integer;
    }

    void listen(event::Caller &caller)
    {
        caller.add(this, &Counter::count);
    }

    int calls=0;
    //! The last call's integer.
    size_t last=0;
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

//! How many rows high the open list (the Menu) is, its 2-unit inner padding
//! above and below them left out; -1 for a height that is no whole rows.
int listRows(SearchComboBox *box)
{
    const int rows=box->getList()->getHeight()-4;
    const int row=box->getOptionsList()->getRowHeight();

    return (rows%row == 0)?rows/row:-1;
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

    // no options: the selection is held, still the value
    box->clearOptions();
    CHECK(box->getOptionCount() == 0);
    CHECK(box->getSelectedIndex() == SearchComboBox::NoOption);
    CHECK(box->isSelectionHeld());
    CHECK(box->getText() == "One");
    CHECK(box->getValue() == "1");

    // and dropped by setSelectedIndex
    box->setSelectedIndex(SearchComboBox::NoOption);
    CHECK(!box->isSelectionHeld());
    CHECK(box->getText() == "");
    CHECK(box->getValue() == "");
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
    CHECK(listRows(box) == 5);

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

namespace
{

//! A filterable box over \p labels at a fixed place, its field focused.
SearchComboBox *filterable(Scene &scene, const std::vector<String> &labels)
{
    SearchComboBox *box=scene.box(0);
    std::vector<SearchComboBox::Option> options;

    for(const String &label : labels)
        options.push_back({label, ""});
    box->setOptions(std::move(options));
    box->setFilterable(true);
    scene.frame();
    box->getField()->focus();
    scene.frame();
    return box;
}

void type(Scene &scene, const char *text)
{
    for(const char *c=text; *c; ++c)
    {
        scene.canvas->inputCharacter(static_cast<UnicodeChar>(*c));
        scene.frame();
    }
}

std::vector<size_t> shown(SearchComboBox *box)
{
    return box->getShownOptions();
}

//! Every control under \p control (itself included) with a state change
//! pending, between updateCanvas() and renderCanvas().
void changed(controls::Base *control, std::set<controls::Base *> &out)
{
    if(control->getStateChange() != controls::StateChange_Nothing)
        out.insert(control);

    for(controls::Base *child : control->getChildren())
        changed(child, out);
}

bool within(controls::Base *control, controls::Base *root)
{
    for(; control; control=control->getParent())
    {
        if(control == root)
            return true;
    }
    return false;
}

}//namespace

TEST_CASE("SearchComboBox: filterable, typing opens the list on the options containing the query, case-insensitively")
{
    Scene scene;
    SearchComboBox *box=filterable(scene, {"Alpha", "beta", "ALPHABET", "Gamma", "delta"});
    Counter edits;
    String lastText;

    edits.listen(box->onTextChangedCaller);
    REQUIRE(box->getField());
    // the field shows the selection, the box's own text is empty
    CHECK(box->getField()->getText() == "Alpha");
    CHECK(box->getText() == "");
    CHECK(KeyboardFocus == box->getField());
    CHECK(!box->isListOpen());

    // typing replaces the selected text
    box->getField()->onSelectAll(box->getField());
    type(scene, "aLp");
    REQUIRE(box->isListOpen());
    CHECK(box->getQuery() == "aLp");
    CHECK(shown(box) == std::vector<size_t>{0, 2});
    CHECK(box->getHighlightedIndex() == 0);
    CHECK(edits.calls == 3);

    REQUIRE(rowFor(box, 2));
    CHECK(rowFor(box, 2)->getText() == "ALPHABET");
    CHECK(listRows(box) == 2);

    // a substring anywhere: "ta" is in beta and delta
    for(int i=0; i < 3; ++i)
        scene.key(keys::Backspace);
    type(scene, "TA");
    CHECK(shown(box) == std::vector<size_t>{1, 4});
    CHECK(box->getHighlightedIndex() == 1);

    // an empty query shows every option
    scene.key(keys::Backspace);
    scene.key(keys::Backspace);
    CHECK(box->getField()->getText() == "");
    CHECK(box->isListOpen());
    CHECK(shown(box).size() == 5);
    CHECK(edits.calls == 10);
    CHECK(box->getSelectedIndex() == 0);
}

TEST_CASE("SearchComboBox: filterable, Up and Down wrap through the shown options and Return picks")
{
    Scene scene;
    SearchComboBox *box=filterable(scene, {"red", "green", "blue", "grey", "gold"});
    Counter selections;

    selections.listen(box->onSelection);
    box->getField()->onSelectAll(box->getField());
    type(scene, "g");
    REQUIRE(shown(box) == (std::vector<size_t>{1, 3, 4}));
    CHECK(box->getHighlightedIndex() == 1);

    scene.key(keys::Up);
    CHECK(box->getHighlightedIndex() == 4);
    scene.key(keys::Down);
    CHECK(box->getHighlightedIndex() == 1);
    scene.key(keys::Down);
    CHECK(box->getHighlightedIndex() == 3);
    REQUIRE(rowFor(box, 3));
    CHECK(rowFor(box, 3)->isHighlighted());
    CHECK(highlightedRows(box) == 1);
    // the keys did not edit the field
    CHECK(box->getField()->getText() == "g");

    scene.key(keys::Return);
    CHECK(!box->isListOpen());
    CHECK(box->getSelectedIndex() == 3);
    CHECK(box->getField()->getText() == "grey");
    CHECK(selections.calls == 1);
    // the Return's release did not move the focus on
    CHECK(KeyboardFocus == box->getField());
}

TEST_CASE("SearchComboBox: filterable, Escape, a press elsewhere and Tab give back the text shown before the edit")
{
    Scene scene;
    SearchComboBox *box=filterable(scene, {"red", "green", "blue"});
    controls::TextBox *other=scene.canvas->newChild<controls::TextBox>();
    Counter selections;

    other->setBounds(20, 300, 100, 20);
    selections.listen(box->onSelection);
    box->setSelectedIndex(2);
    scene.frame();
    CHECK(box->getField()->getText() == "blue");

    box->getField()->onSelectAll(box->getField());
    type(scene, "re");
    REQUIRE(box->isListOpen());
    scene.key(keys::Escape);
    CHECK(!box->isListOpen());
    CHECK(box->getField()->getText() == "blue");
    CHECK(box->getSelectedIndex() == 2);
    CHECK(KeyboardFocus == box->getField());

    // edited again, then a click on another control
    box->getField()->onSelectAll(box->getField());
    type(scene, "gr");
    REQUIRE(box->isListOpen());
    scene.clickOn(other);
    CHECK(KeyboardFocus == other);
    CHECK(!box->isListOpen());
    CHECK(box->getField()->getText() == "blue");
    CHECK(box->getSelectedIndex() == 2);

    // Tab away, with no press closing the list first
    box->getField()->focus();
    box->getField()->onSelectAll(box->getField());
    type(scene, "gr");
    REQUIRE(box->isListOpen());
    scene.key(keys::Tab);
    CHECK(KeyboardFocus == other);
    CHECK(!box->isListOpen());
    CHECK(box->getField()->getText() == "blue");
    CHECK(selections.calls == 0);
}

TEST_CASE("SearchComboBox: filterable, no match shows one disabled No matches row, which picks nothing")
{
    Scene scene;
    SearchComboBox *box=filterable(scene, {"red", "green", "blue"});
    Counter selections;

    selections.listen(box->onSelection);
    box->getField()->onSelectAll(box->getField());
    type(scene, "zz");
    REQUIRE(box->isListOpen());
    CHECK(shown(box).empty());
    CHECK(box->getHighlightedIndex() == SearchComboBox::NoOption);
    CHECK(box->getOptionsList()->getItemCount() == 1);
    CHECK(listRows(box) == 1);

    auto *row=gweni_cast<controls::internal::SearchComboBoxRow>(box->getOptionsList()->rowForItem(0));

    REQUIRE(row);
    CHECK(row->getText() == SearchComboBox::noMatchesText());
    CHECK(row->isDisabled());
    CHECK(!row->isHighlighted());

    scene.key(keys::Down);
    scene.key(keys::Return);
    scene.clickOn(row);
    CHECK(box->isListOpen());
    CHECK(selections.calls == 0);
    CHECK(box->getSelectedIndex() == 0);

    // a match again: an enabled option row
    scene.key(keys::Backspace);
    scene.key(keys::Backspace);
    type(scene, "b");
    REQUIRE(rowFor(box, 2));
    CHECK(rowFor(box, 2)->getText() == "blue");
    CHECK(!rowFor(box, 2)->isDisabled());
}

TEST_CASE("SearchComboBox: filterable, a click on a row picks with the field keeping the focus")
{
    Scene scene;
    SearchComboBox *box=filterable(scene, {"red", "green", "blue"});
    Counter lostFocus, selections;

    lostFocus.listen(box->getField()->onLostFocusCaller);
    selections.listen(box->onSelection);
    box->getField()->onSelectAll(box->getField());
    type(scene, "e");
    REQUIRE(box->isListOpen());
    REQUIRE(rowFor(box, 2));

    const Rect row=Scene::canvasRect(rowFor(box, 2));

    scene.canvas->inputMouseMoved(row.x+5, row.y+row.h/2, 0, 0);
    input::onMouseClicked(scene.canvas, 0, true);
    scene.frame();
    // between the press and the release: still the field's, the edit kept
    CHECK(KeyboardFocus == box->getField());
    CHECK(box->getField()->getText() == "e");
    CHECK(box->isListOpen());

    input::onMouseClicked(scene.canvas, 0, false);
    scene.frame();
    CHECK(!box->isListOpen());
    CHECK(box->getSelectedIndex() == 2);
    CHECK(box->getField()->getText() == "blue");
    CHECK(KeyboardFocus == box->getField());
    CHECK(lostFocus.calls == 0);
    CHECK(selections.calls == 1);
}

TEST_CASE("SearchComboBox: filterable, a press on the box focuses the field and opens the list on every option")
{
    Scene scene;
    SearchComboBox *box=filterable(scene, {"red", "green", "blue"});

    box->getField()->unfocus();
    scene.frame();
    box->setSelectedIndex(1);

    // the chevron's column, right of the field
    const Rect rect=Scene::canvasRect(box);

    scene.click(rect.x+rect.w-6, rect.y+rect.h/2);
    REQUIRE(box->isListOpen());
    CHECK(KeyboardFocus == box->getField());
    CHECK(box->getQuery() == "");
    CHECK(shown(box).size() == 3);
    CHECK(box->getHighlightedIndex() == 1);
    // the shown text selected: typing replaces it
    CHECK(box->getField()->getSelection() == "green");

    // and closes it, the field keeping the focus
    scene.click(rect.x+rect.w-6, rect.y+rect.h/2);
    CHECK(!box->isListOpen());
    CHECK(KeyboardFocus == box->getField());
}

TEST_CASE("SearchComboBox: a keystroke binds only the rows whose option changed and makes no control")
{
    Scene scene;
    SearchComboBox *box=filterable(scene, {"xa", "xb", "xc", "ya", "yb"});

    box->getField()->onSelectAll(box->getField());
    type(scene, "x");
    REQUIRE(shown(box) == (std::vector<size_t>{0, 1, 2}));

    controls::Base *kept[3]={rowFor(box, 0), rowFor(box, 1), rowFor(box, 2)};
    const size_t controlsBefore=subtreeSize(scene.canvas);
    size_t binds=box->getRowBindCount();

    // the query emptied: the three rows keep their options, two more show
    scene.key(keys::Backspace);
    REQUIRE(shown(box).size() == 5);
    CHECK(scene.canvas->getLayoutStats().passes == 1);
    CHECK(box->getRowBindCount()-binds == 2);
    CHECK(subtreeSize(scene.canvas) == controlsBefore);

    for(int i=0; i < 3; ++i)
        CHECK(rowFor(box, i) == kept[i]);

    // "y": every shown row shows another option, two of them
    binds=box->getRowBindCount();
    type(scene, "y");
    REQUIRE(shown(box) == (std::vector<size_t>{3, 4}));
    CHECK(box->getRowBindCount()-binds == 2);

    // "ya": the first row keeps its option, the second is cut off
    binds=box->getRowBindCount();
    type(scene, "a");
    REQUIRE(shown(box) == std::vector<size_t>{3});
    CHECK(box->getRowBindCount()-binds == 0);
    CHECK(subtreeSize(scene.canvas) == controlsBefore);

    // shown rows that keep their option and highlight change no look: only
    // the clip box the list's new height gives them (Geometry)
    binds=box->getRowBindCount();
    scene.key(keys::Backspace);
    scene.key(keys::Backspace);
    type(scene, "x");
    REQUIRE(shown(box) == (std::vector<size_t>{0, 1, 2}));
    scene.key(keys::Backspace);
    scene.canvas->inputCharacter('x');
    scene.canvas->updateCanvas();

    std::set<controls::Base *> marked;

    changed(box->getList(), marked);
    for(controls::Base *control : marked)
    {
        // the two rows the query cut off are hidden, which marks them
        if(!control->visible())
            continue;

        const std::string type=control->getTypeName();
        const int change=control->getStateChange();

        CAPTURE(type);
        CAPTURE(change);
        CHECK((change & (controls::StateChange_Text|controls::StateChange_Background)) == 0);
    }
    scene.canvas->renderCanvas();
}

TEST_CASE("SearchComboBox: a keystroke's pool, controls and live ids are the same at 30 and 3 000 options")
{
    struct Stroke
    {
        size_t rows;
        size_t controls;
        size_t primitives;
        size_t texts;
        size_t passes;
        size_t shown;
    };

    auto stroke=[](size_t count) -> Stroke
        {
            Scene scene;
            std::vector<String> labels;

            for(size_t i=0; i < count; ++i)
                labels.push_back("Option "+std::to_string(i));

            SearchComboBox *box=filterable(scene, labels);

            box->getField()->onSelectAll(box->getField());
            type(scene, "Option 1");
            REQUIRE(box->isListOpen());
            scene.frame();

            const test::IdBankCounts before=test::IdBankCounts::now();
            const size_t controlsBefore=subtreeSize(scene.canvas);

            // "Option 12": 12, 120-129 and on at 3 000; 12 alone at 30
            type(scene, "2");

            const test::IdBankCounts after=test::IdBankCounts::now();

            return {box->getOptionsList()->getRows().size(), subtreeSize(scene.canvas)-controlsBefore,
                after.livePrimitives()-before.livePrimitives(), after.liveTexts()-before.liveTexts(),
                scene.canvas->getLayoutStats().passes, std::min<size_t>(shown(box).size(), SearchComboBox::MaxShownRows)};
        };

    const Stroke small=stroke(30);
    const Stroke large=stroke(3000);

    CHECK(small.shown == 1);
    CHECK(large.shown == SearchComboBox::MaxShownRows);
    CHECK(large.rows == small.rows);
    CHECK(small.controls == 0);
    CHECK(large.controls == 0);
    CHECK(small.primitives == 0);
    CHECK(large.primitives == 0);
    CHECK(small.texts == 0);
    CHECK(large.texts == 0);
    CHECK(small.passes == 1);
    CHECK(large.passes == 1);
}

namespace
{

//! An onCustomValue handler: records each text, refusing while `refuse` is
//! on, and adds an option for the text it takes while `add` is on.
class CustomValues: public event::Handler
{
public:
    void custom(event::Info info)
    {
        texts.push_back(info.string);

        if(refuse)
            box->refuseCustomValue();
        else if(add)
            box->addOption(info.string);
    }

    void listen(SearchComboBox *of)
    {
        box=of;
        box->onCustomValue.add(this, &CustomValues::custom);
    }

    SearchComboBox *box=nullptr;
    std::vector<String> texts;
    bool refuse=false;
    bool add=false;
};

SearchComboBox *freeText(Scene &scene, const std::vector<String> &labels)
{
    SearchComboBox *box=filterable(scene, labels);

    box->setAllowCustomValue(true);
    scene.frame();
    box->getField()->focus();
    box->getField()->onSelectAll(box->getField());
    return box;
}

String rowText(SearchComboBox *box, size_t position)
{
    controls::Base *row=box->getOptionsList()->rowForItem(position);

    return row?gweni_cast<controls::internal::SearchComboBoxRow>(row)->getText():String("<none>");
}

}//namespace

TEST_CASE("SearchComboBox: free text, a text that is no option's label shows the Add row after the matches")
{
    Scene scene;
    SearchComboBox *box=freeText(scene, {"web-1", "web-2", "db"});

    CHECK(box->isFilterable());
    type(scene, "web");
    REQUIRE(box->isListOpen());
    CHECK(shown(box) == (std::vector<size_t>{0, 1}));
    CHECK(box->isAddRowShown());
    CHECK(box->shownRowCount() == 3);
    CHECK(rowText(box, 2) == "Add \xe2\x80\x9cweb\xe2\x80\x9d");
    CHECK(SearchComboBox::addRowText("web") == "Add \xe2\x80\x9cweb\xe2\x80\x9d");
    CHECK(listRows(box) == 3);
    // the first match takes the highlight; Up wraps to the Add row
    CHECK(box->getHighlightedIndex() == 0);
    scene.key(keys::Up);
    CHECK(box->isAddRowHighlighted());
    CHECK(box->getHighlightedIndex() == SearchComboBox::NoOption);

    // nothing matches: the Add row alone, highlighted, never "No matches"
    type(scene, "-9");
    CHECK(shown(box).empty());
    CHECK(box->shownRowCount() == 1);
    CHECK(box->isAddRowHighlighted());
    CHECK(rowText(box, 0) == "Add \xe2\x80\x9cweb-9\xe2\x80\x9d");
    auto *addRow=gweni_cast<controls::internal::SearchComboBoxRow>(box->getOptionsList()->rowForItem(0));

    REQUIRE(addRow);
    CHECK(!addRow->isDisabled());

    // an option's label, case aside: no Add row
    for(int i=0; i < 5; ++i)
        scene.key(keys::Backspace);
    type(scene, "DB");
    CHECK(shown(box) == std::vector<size_t>{2});
    CHECK(!box->isAddRowShown());
    CHECK(box->shownRowCount() == 1);
}

TEST_CASE("SearchComboBox: free text, Return on the Add row takes the text as the value")
{
    Scene scene;
    SearchComboBox *box=freeText(scene, {"web-1", "web-2"});
    CustomValues customs;
    Counter selections;

    customs.listen(box);
    selections.listen(box->onSelection);
    type(scene, "web-9");
    REQUIRE(box->isAddRowHighlighted());

    scene.key(keys::Return);
    CHECK(customs.texts == std::vector<String>{"web-9"});
    CHECK(!box->isListOpen());
    CHECK(box->hasCustomValue());
    CHECK(box->getValue() == "web-9");
    CHECK(box->getSelectedIndex() == SearchComboBox::NoOption);
    CHECK(box->getField()->getText() == "web-9");
    CHECK(!box->isCustomValueRefused());
    CHECK(KeyboardFocus == box->getField());
    CHECK(selections.calls == 0);

    // a click on the Add row takes it too
    box->getField()->onSelectAll(box->getField());
    type(scene, "web-7");
    REQUIRE(box->isListOpen());
    REQUIRE(box->getOptionsList()->rowForItem(0));
    scene.clickOn(box->getOptionsList()->rowForItem(0));
    CHECK(customs.texts == (std::vector<String>{"web-9", "web-7"}));
    CHECK(box->getValue() == "web-7");
    CHECK(!box->isListOpen());

    // picking an option drops the free-text value
    box->getField()->onSelectAll(box->getField());
    type(scene, "web-2");
    scene.key(keys::Return);
    CHECK(!box->hasCustomValue());
    CHECK(box->getSelectedIndex() == 1);
    CHECK(box->getValue() == "web-2");
    CHECK(selections.calls == 1);
    CHECK(customs.texts.size() == 2);
}

TEST_CASE("SearchComboBox: free text, a refused text stays in the field in the refused colour")
{
    Scene scene;
    SearchComboBox *box=freeText(scene, {"web-1", "web-2"});
    controls::TextBox *other=scene.canvas->newChild<controls::TextBox>();
    CustomValues customs;

    other->setBounds(20, 300, 100, 20);
    customs.listen(box);
    customs.refuse=true;
    type(scene, "bad host");
    scene.key(keys::Return);
    CHECK(customs.texts == std::vector<String>{"bad host"});
    // the value stays, the text and the list too
    CHECK(box->getSelectedIndex() == 0);
    CHECK(!box->hasCustomValue());
    CHECK(box->getValue() == "web-1");
    CHECK(box->getField()->getText() == "bad host");
    CHECK(box->isListOpen());
    CHECK(box->isCustomValueRefused());
    CHECK((box->getField()->getTextColor() == scene.skin.Colors.SearchComboRefusedText));

    // an edit is no longer the refused text
    scene.key(keys::Backspace);
    CHECK(!box->isCustomValueRefused());
    CHECK((box->getField()->getTextColor() == scene.skin.Colors.Label.Default));

    // refused on a lost focus: the list closes, the text stays refused
    type(scene, "t");
    scene.key(keys::Tab);
    CHECK(KeyboardFocus == other);
    CHECK(customs.texts == (std::vector<String>{"bad host", "bad host"}));
    CHECK(!box->isListOpen());
    CHECK(box->getField()->getText() == "bad host");
    CHECK(box->isCustomValueRefused());
    CHECK(box->getValue() == "web-1");

    // Escape gives the value back
    box->getField()->focus();
    scene.key(keys::Escape);
    CHECK(box->getField()->getText() == "web-1");
    CHECK(!box->isCustomValueRefused());
    CHECK(customs.texts.size() == 2);
}

TEST_CASE("SearchComboBox: free text, a lost focus or Return with the list closed resolves what was typed")
{
    Scene scene;
    SearchComboBox *box=freeText(scene, {"web-1", "web-2"});
    controls::TextBox *other=scene.canvas->newChild<controls::TextBox>();
    CustomValues customs;
    Counter selections;

    other->setBounds(20, 300, 100, 20);
    customs.listen(box);
    selections.listen(box->onSelection);

    // a text that is no option, the focus moved by a press elsewhere
    type(scene, "newhost");
    scene.clickOn(other);
    CHECK(customs.texts == std::vector<String>{"newhost"});
    CHECK(box->getValue() == "newhost");
    CHECK(box->getField()->getText() == "newhost");

    // an option's label, case aside, picks that option
    box->getField()->focus();
    box->getField()->onSelectAll(box->getField());
    type(scene, "WEB-2");
    scene.key(keys::Tab);
    CHECK(box->getSelectedIndex() == 1);
    CHECK(!box->hasCustomValue());
    CHECK(box->getField()->getText() == "web-2");
    CHECK(selections.calls == 1);
    CHECK(customs.texts.size() == 1);

    // emptied: the value comes back
    box->getField()->focus();
    box->getField()->onSelectAll(box->getField());
    scene.key(keys::Backspace);
    scene.key(keys::Tab);
    CHECK(box->getField()->getText() == "web-2");
    CHECK(box->getSelectedIndex() == 1);

    // the list closed by the box's chevron keeps the text, and Return takes it
    box->getField()->focus();
    box->getField()->onSelectAll(box->getField());
    type(scene, "other");
    REQUIRE(box->isListOpen());

    const Rect rect=Scene::canvasRect(box);

    scene.click(rect.x+rect.w-6, rect.y+rect.h/2);
    REQUIRE(!box->isListOpen());
    CHECK(box->getField()->getText() == "other");
    scene.key(keys::Return);
    CHECK(customs.texts == (std::vector<String>{"newhost", "other"}));
    CHECK(box->getValue() == "other");
    // the Return's release did not move the focus on
    CHECK(KeyboardFocus == box->getField());
}

TEST_CASE("SearchComboBox: free text, an option the handler adds leaves the value the text")
{
    Scene scene;
    SearchComboBox *box=freeText(scene, {"web-1"});
    CustomValues customs;

    customs.listen(box);
    customs.add=true;
    // nothing selected: an option added then would be the default
    box->setSelectedIndex(SearchComboBox::NoOption);
    box->getField()->onSelectAll(box->getField());
    type(scene, "web-3");
    scene.key(keys::Return);
    REQUIRE(box->getOptionCount() == 2);
    CHECK(box->getOption(1).label == "web-3");
    CHECK(box->hasCustomValue());
    CHECK(box->getSelectedIndex() == SearchComboBox::NoOption);
    CHECK(box->getValue() == "web-3");

    // nor does one the app adds later take its place
    box->addOption("later");
    CHECK(box->getValue() == "web-3");
    CHECK(box->getSelectedIndex() == SearchComboBox::NoOption);

    // and setOptions keeps it
    box->setOptions({{"a", ""}, {"b", ""}});
    CHECK(box->getValue() == "web-3");
    CHECK(box->getSelectedIndex() == SearchComboBox::NoOption);
    CHECK(box->getField()->getText() == "web-3");
}

TEST_CASE("SearchComboBox: free text, a pick of the selected option shows its label and takes no free-text value")
{
    Scene scene;
    SearchComboBox *box=freeText(scene, {"web-1", "web-2"});
    controls::TextBox *other=scene.canvas->newChild<controls::TextBox>();
    CustomValues customs;

    other->setBounds(20, 300, 100, 20);
    customs.listen(box);
    REQUIRE(box->getSelectedIndex() == 0);

    // Return on the highlighted selected option
    type(scene, "web");
    REQUIRE(box->getHighlightedIndex() == 0);
    scene.key(keys::Return);
    CHECK(!box->isListOpen());
    CHECK(box->getField()->getText() == "web-1");
    scene.clickOn(other);
    CHECK(customs.texts.empty());
    CHECK(!box->hasCustomValue());
    CHECK(box->getValue() == "web-1");

    // a click on its row
    box->getField()->focus();
    box->getField()->onSelectAll(box->getField());
    type(scene, "web");
    REQUIRE(rowFor(box, 0));
    scene.clickOn(rowFor(box, 0));
    CHECK(!box->isListOpen());
    CHECK(box->getField()->getText() == "web-1");
    scene.clickOn(other);
    CHECK(customs.texts.empty());
    CHECK(box->getValue() == "web-1");

    // its label in another case, the focus moved on
    box->getField()->focus();
    box->getField()->onSelectAll(box->getField());
    type(scene, "WEB-1");
    scene.key(keys::Tab);
    CHECK(box->getField()->getText() == "web-1");
    CHECK(box->getSelectedIndex() == 0);
    CHECK(customs.texts.empty());
}

TEST_CASE("SearchComboBox: free text, a refused text gives way to a pick of the selected option")
{
    Scene scene;
    SearchComboBox *box=freeText(scene, {"web-1", "web-2"});
    CustomValues customs;

    customs.listen(box);
    customs.refuse=true;
    // the Add row after the two matches, then refused
    type(scene, "web");
    scene.key(keys::Up);
    REQUIRE(box->getHighlightedIndex() == SearchComboBox::NoOption);
    scene.key(keys::Return);
    REQUIRE(box->isCustomValueRefused());
    REQUIRE(box->isListOpen());

    // Down wraps onto web-1, the selected option, and Return picks it
    scene.key(keys::Down);
    REQUIRE(box->getHighlightedIndex() == 0);
    scene.key(keys::Return);
    CHECK(!box->isListOpen());
    CHECK(!box->isCustomValueRefused());
    CHECK(box->getField()->getText() == "web-1");
    CHECK((box->getField()->getTextColor() == scene.skin.Colors.Label.Default));
    CHECK(customs.texts.size() == 1);
}

TEST_CASE("SearchComboBox: free text, the value is the text typed, whatever the handler sets the field to")
{
    Scene scene;
    SearchComboBox *box=freeText(scene, {"web-1"});

    // a handler writing the field while the box takes the text
    struct Rewriter: event::Handler
    {
        void custom(event::Info)
        {
            box->getField()->setText("rewritten");
        }
        SearchComboBox *box=nullptr;
    } rewriter;

    rewriter.box=box;
    box->onCustomValue.add(&rewriter, &Rewriter::custom);
    type(scene, "newhost");
    REQUIRE(rowText(box, 0) == SearchComboBox::addRowText("newhost"));
    scene.clickOn(box->getOptionsList()->rowForItem(0));
    CHECK(box->hasCustomValue());
    CHECK(box->getValue() == "newhost");
}

TEST_CASE("SearchComboBox: disabled, the open list closes and a row picks nothing")
{
    Scene scene;
    SearchComboBox *box=scene.box(5);
    Counter selections;

    selections.listen(box->onSelection);
    scene.clickOn(box);
    REQUIRE(box->isListOpen());

    controls::Base *row=rowFor(box, 3);

    REQUIRE(row);

    const Rect at=Scene::canvasRect(row);

    box->setDisabled(true);
    scene.frame();
    CHECK(!box->isListOpen());
    scene.click(at.x+at.w/2, at.y+at.h/2);
    CHECK(selections.calls == 0);
    CHECK(box->getSelectedIndex() == 0);
}

TEST_CASE("SearchComboBox: filterable, the box's font reaches its field")
{
    Scene scene;
    SearchComboBox *box=filterable(scene, {"a", "b"});
    gweni::Font font=box->getFont();

    font.size+=7;
    box->setFont(font);
    CHECK(box->getField()->getFont().size == font.size);
}

namespace
{

//! An app's search: each edit of the field refills the options with the
//! labels containing the query, through setOptions() or clearOptions() and
//! addOption() one by one.
class Refill: public event::Handler
{
public:
    void edited(event::Info info)
    {
        std::vector<SearchComboBox::Option> results;

        for(const String &label : labels)
        {
            if(label.find(info.string) != String::npos)
                results.push_back({label, ""});
        }

        if(oneByOne)
        {
            box->clearOptions();

            for(const SearchComboBox::Option &option : results)
                box->addOption(option.label);
        }
        else
            box->setOptions(std::move(results));
        ++calls;
    }

    void listen(SearchComboBox *of)
    {
        box=of;
        box->onTextChangedCaller.add(this, &Refill::edited);
    }

    SearchComboBox *box=nullptr;
    std::vector<String> labels;
    bool oneByOne=false;
    int calls=0;
};

}//namespace

TEST_CASE("SearchComboBox: filterable, options refilled from onTextChangedCaller keep what is typed")
{
    Scene scene;
    const std::vector<String> labels{"web-1", "web-2", "db-1"};
    SearchComboBox *box=filterable(scene, labels);
    Refill refill;
    Counter selections;

    refill.labels=labels;
    SUBCASE("setOptions") {}
    SUBCASE("clearOptions and addOption") { refill.oneByOne=true; }

    refill.listen(box);
    selections.listen(box->onSelection);
    box->getField()->onSelectAll(box->getField());
    type(scene, "we");
    CHECK(refill.calls == 2);
    // the field holds what was typed, the list the new options matching it
    CHECK(box->getField()->getText() == "we");
    CHECK(box->getQuery() == "we");
    REQUIRE(box->isListOpen());
    REQUIRE(box->getOptionCount() == 2);
    CHECK(shown(box) == (std::vector<size_t>{0, 1}));
    REQUIRE(rowFor(box, 1));
    CHECK(rowFor(box, 1)->getText() == "web-2");
    CHECK(box->getHighlightedIndex() == 0);

    // an emptied field shows every option again, and a new query its own
    scene.key(keys::Backspace);
    scene.key(keys::Backspace);
    CHECK(box->getField()->getText() == "");
    CHECK(shown(box).size() == 3);
    type(scene, "db");
    CHECK(box->getField()->getText() == "db");
    REQUIRE(box->getOptionCount() == 1);
    CHECK(box->getOption(0).label == "db-1");
    CHECK(shown(box) == std::vector<size_t>{0});
    CHECK(selections.calls == 0);

    // a pick ends the edit: the field shows the option picked
    scene.key(keys::Return);
    CHECK(!box->isListOpen());
    CHECK(box->getSelectedIndex() == 0);
    CHECK(box->getField()->getText() == "db-1");
    CHECK(selections.calls == 1);

    // over: the app's own refill holds the value, as on any box
    box->setOptions({{"x", ""}});
    CHECK(box->getSelectedIndex() == SearchComboBox::NoOption);
    CHECK(box->getValue() == "db-1");
    CHECK(box->getField()->getText() == "db-1");
}

TEST_CASE("SearchComboBox: free text, the value's own text typed and Tab end the edit an app refill keeps")
{
    Scene scene;
    SearchComboBox *box=freeText(scene, {"web-1", "web-2"});
    controls::TextBox *other=scene.canvas->newChild<controls::TextBox>();

    other->setBounds(20, 300, 100, 20);
    REQUIRE(box->getSelectedIndex() == 0);
    // typed back to the value's own text, then away
    type(scene, "web-1");
    scene.key(keys::Tab);
    REQUIRE(KeyboardFocus != box->getField());
    CHECK(box->getField()->getText() == "web-1");
    CHECK(!box->hasCustomValue());

    // the app's refill then holds the value
    box->setOptions({{"z", ""}});
    CHECK(box->getSelectedIndex() == SearchComboBox::NoOption);
    CHECK(box->getValue() == "web-1");
    CHECK(box->getField()->getText() == "web-1");
    CHECK(!box->hasCustomValue());
}

TEST_CASE("SearchComboBox: setOptions with the list closed leaves no shown option of the old ones")
{
    Scene scene;
    SearchComboBox *box=scene.box(5);

    scene.clickOn(box);
    REQUIRE(box->isListOpen());
    REQUIRE(shown(box).size() == 5);
    scene.key(keys::Escape);
    REQUIRE(!box->isListOpen());

    box->setOptions({{"a", ""}, {"b", ""}});
    CHECK(shown(box).empty());
    // no option 4 to highlight
    box->setHighlightedIndex(4);
    CHECK(box->getHighlightedIndex() == SearchComboBox::NoOption);

    // opened, it shows the new ones
    scene.clickOn(box);
    CHECK(shown(box) == (std::vector<size_t>{0, 1}));
}

TEST_CASE("SearchComboBox: free text, a value wider than every option widens the box's preferred width")
{
    Scene scene;
    SearchComboBox *wide=filterable(scene, {"ab", "cd", "a-much-longer-host.example.org"});
    SearchComboBox *box=freeText(scene, {"ab", "cd"});

    // sized to their content
    for(SearchComboBox *each : {box, wide})
        each->setSizeFlags({SizeFlag::Elastic, SizeFlag::Fixed});
    wide->setPos(20, 100);
    scene.frame();
    const int narrow=box->getPreferredSize().width;

    REQUIRE(wide->getPreferredSize().width > narrow);
    box->getField()->focus();
    box->getField()->onSelectAll(box->getField());
    type(scene, "a-much-longer-host.example.org");
    scene.key(keys::Return);
    REQUIRE(box->getValue() == "a-much-longer-host.example.org");
    scene.frame();
    CHECK(box->getPreferredSize().width == wide->getPreferredSize().width);

    // an option picked again gives the width back
    box->setSelectedIndex(0);
    scene.frame();
    CHECK(box->getPreferredSize().width == narrow);
}

namespace
{

std::vector<String> chipTexts(SearchComboBox *box)
{
    std::vector<String> texts;

    for(Chip *chip : box->getChips())
        texts.push_back(chip->getText());
    return texts;
}

SearchComboBox *multi(Scene &scene, const std::vector<String> &labels, bool filterable)
{
    SearchComboBox *box=filterable?::filterable(scene, labels):scene.box(0);

    if(!filterable)
    {
        std::vector<SearchComboBox::Option> options;

        for(const String &label : labels)
            options.push_back({label, ""});
        box->setOptions(std::move(options));
    }
    box->setMultiSelect(true);
    scene.frame();
    return box;
}

}//namespace

TEST_CASE("SearchComboBox: multi-select, each pick is a chip in pick order and a second pick takes it away")
{
    Scene scene;
    SearchComboBox *box=multi(scene, {"red", "green", "blue", "grey", "gold"}, false);
    Counter changes, selections;

    changes.listen(box->onSelectionChange);
    selections.listen(box->onSelection);
    CHECK(box->isMultiSelect());
    CHECK(box->getSelectedIndices().empty());
    CHECK(box->getSelectedIndex() == SearchComboBox::NoOption);
    CHECK(box->getText() == "");

    scene.clickOn(box);
    REQUIRE(box->isListOpen());
    REQUIRE(rowFor(box, 2));
    scene.clickOn(rowFor(box, 2));
    // the list stays open, the picked row checked and highlighted
    CHECK(box->isListOpen());
    CHECK(box->getSelectedIndices() == std::vector<int>{2});
    CHECK(chipTexts(box) == std::vector<String>{"blue"});
    CHECK(rowFor(box, 2)->isChecked());
    CHECK(!rowFor(box, 1)->isChecked());
    CHECK(box->getHighlightedIndex() == 2);
    CHECK(changes.calls == 1);

    // Return picks the highlighted row: the order is the picks'
    scene.key(keys::Up);
    scene.key(keys::Up);
    scene.key(keys::Return);
    CHECK(box->getSelectedIndices() == (std::vector<int>{2, 0}));
    CHECK(chipTexts(box) == (std::vector<String>{"blue", "red"}));
    CHECK(rowFor(box, 0)->isChecked());
    CHECK(box->isListOpen());

    // a picked one picked again goes
    scene.clickOn(rowFor(box, 2));
    CHECK(box->getSelectedIndices() == std::vector<int>{0});
    CHECK(chipTexts(box) == std::vector<String>{"red"});
    CHECK(!rowFor(box, 2)->isChecked());
    CHECK(changes.calls == 3);
    CHECK(selections.calls == 0);
    CHECK(box->getValue() == "");
    CHECK(box->getSelectedValues() == std::vector<String>{"red"});
}

TEST_CASE("SearchComboBox: multi-select, the chip's x and Backspace in an empty field take a pick away")
{
    Scene scene;
    SearchComboBox *box=multi(scene, {"red", "green", "blue", "grey", "gold"}, true);
    Counter changes;

    changes.listen(box->onSelectionChange);
    box->setSelectedIndices({0, 2, 4});
    scene.frame();
    REQUIRE(box->getChips().size() == 3);

    // the x of the middle chip
    Chip *middle=box->getChips()[1];

    REQUIRE(middle->getRemoveButton());
    scene.clickOn(middle->getRemoveButton());
    CHECK(box->getSelectedIndices() == (std::vector<int>{0, 4}));
    CHECK(changes.calls == 1);
    // the click's frame deleted it
    CHECK(box->getChips().size() == 2);
    CHECK(chipTexts(box) == (std::vector<String>{"red", "gold"}));

    // Backspace in the empty field: the last pick
    box->getField()->focus();
    scene.key(keys::Backspace);
    CHECK(box->getSelectedIndices() == std::vector<int>{0});
    CHECK(changes.calls == 2);

    // with text in the field it edits the text
    type(scene, "gr");
    scene.key(keys::Backspace);
    CHECK(box->getField()->getText() == "g");
    CHECK(box->getSelectedIndices() == std::vector<int>{0});
    CHECK(changes.calls == 2);
}

TEST_CASE("SearchComboBox: multi-select, setSelectedIndices fires nothing and leaves out repeated and out-of-range indices")
{
    Scene scene;
    SearchComboBox *box=multi(scene, {"red", "green", "blue"}, false);
    Counter changes;

    changes.listen(box->onSelectionChange);
    box->setOptions({{"red", "r"}, {"green", ""}, {"blue", "b"}});
    box->setSelectedIndices({2, 0, 2, 7, -1});
    scene.frame();
    CHECK(box->getSelectedIndices() == (std::vector<int>{2, 0}));
    CHECK(box->getSelectedValues() == (std::vector<String>{"b", "r"}));
    CHECK(chipTexts(box) == (std::vector<String>{"blue", "red"}));
    CHECK(box->isPicked(0));
    CHECK(!box->isPicked(1));

    box->setSelectedIndices({1});
    scene.frame();
    CHECK(box->getSelectedIndices() == std::vector<int>{1});
    CHECK(box->getSelectedValues() == std::vector<String>{"green"});
    CHECK(chipTexts(box) == std::vector<String>{"green"});
    CHECK(changes.calls == 0);
}

TEST_CASE("SearchComboBox: multi-select, chips wrap onto a new row and the open list follows the box's new bottom in that frame")
{
    const bool verify=controls::layoutVerifyEnabled();
    const size_t verifyFailures=controls::layoutVerifyFailures();

    for(bool verifying : {false, true})
    {
        CAPTURE(verifying);
        controls::setLayoutVerify(verifying);

        Scene scene;
        SearchComboBox *box=multi(scene, {"alpha-one", "beta-two", "gamma-three", "delta"}, true);
        controls::Base *below=scene.canvas->newChild<controls::Base>();

        below->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        below->setBounds(20, 200, 50, 20);
        scene.frame();

        const int oneRow=box->getHeight();

        box->getField()->focus();
        scene.key(keys::Down);
        REQUIRE(box->isListOpen());
        REQUIRE(rowFor(box, 0));
        scene.clickOn(rowFor(box, 0));
        scene.clickOn(rowFor(box, 1));
        REQUIRE(box->getChips().size() == 2);

        // as wide as the two chips and the field's least width beside them
        const int chipsWidth=box->getChips()[0]->getPreferredSize().width+SearchComboBox::ChipGap
            +box->getChips()[1]->getPreferredSize().width+SearchComboBox::ChipGap;
        const int inset=box->getPadding().left+box->getTextPadding().left;
        const int rightInset=box->getPadding().right+box->getTextPadding().right;

        box->setWidth(inset+chipsWidth+SearchComboBox::MinFieldWidth+rightInset);
        scene.frame();

        const Rect first=box->getChips()[0]->getBounds();
        const Rect second=box->getChips()[1]->getBounds();

        REQUIRE(box->getField()->getBounds().y == first.y);
        CHECK(box->getField()->getBounds().w == SearchComboBox::MinFieldWidth);

        // side by side on the first row
        CHECK(second.y == first.y);
        CHECK(second.x == first.x+first.w+SearchComboBox::ChipGap);

        const int beforeWrap=box->getHeight();

        // the third does not fit beside them: a second row, the box taller,
        // the list at its new bottom, in the frame of the pick and one pass
        REQUIRE(rowFor(box, 2));
        scene.clickOn(rowFor(box, 2));
        REQUIRE(box->getChips().size() == 3);

        const Rect third=box->getChips()[2]->getBounds();

        CHECK(scene.canvas->getLayoutStats().passes == 1);
        CHECK(third.y > first.y);
        CHECK(third.x == first.x);
        CHECK(box->getHeight() > beforeWrap);
        CHECK(beforeWrap >= oneRow);
        CHECK(box->isListOpen());
        CHECK(box->getList()->getY() == Scene::canvasRect(box).y+box->getHeight());
        // the field after the last chip, inside the box
        const Rect field=box->getField()->getBounds();

        CHECK(field.y == third.y);
        CHECK(field.x > third.x+third.w);
        CHECK(field.x+field.w <= box->getWidth());

        // and back to one row when it goes, the list following again
        scene.clickOn(box->getChips()[2]->getRemoveButton());
        CHECK(box->getHeight() == beforeWrap);
        CHECK(box->getList()->getY() == Scene::canvasRect(box).y+box->getHeight());
        CHECK(scene.canvas->getLayoutStats().passes == 1);

        scene.frame();
        CHECK(scene.canvas->getLayoutStats().passes == 0);
    }
    CHECK(controls::layoutVerifyFailures() == verifyFailures);
    controls::setLayoutVerify(verify);
}

TEST_CASE("SearchComboBox: multi-select, a free-text value becomes a chip of its own, once")
{
    Scene scene;
    SearchComboBox *box=multi(scene, {"web-1", "web-2"}, true);
    CustomValues customs;
    Counter changes;

    box->setAllowCustomValue(true);
    customs.listen(box);
    changes.listen(box->onSelectionChange);
    box->getField()->focus();
    type(scene, "newtag");
    REQUIRE(box->isAddRowHighlighted());
    scene.key(keys::Return);
    CHECK(customs.texts == std::vector<String>{"newtag"});
    CHECK(chipTexts(box) == std::vector<String>{"newtag"});
    CHECK(box->getSelectedValues() == std::vector<String>{"newtag"});
    CHECK(box->getSelectedIndices().empty());
    CHECK(!box->hasCustomValue());
    // the field is empty for the next one, the list open on every option
    CHECK(box->getField()->getText() == "");
    CHECK(box->isListOpen());
    CHECK(shown(box).size() == 2);
    CHECK(changes.calls == 1);

    // the same text again adds no second chip
    type(scene, "newtag");
    scene.key(keys::Return);
    CHECK(chipTexts(box) == std::vector<String>{"newtag"});
    CHECK(changes.calls == 1);

    // an option typed in full and a lost focus pick it
    type(scene, "WEB-2");
    box->getField()->unfocus();
    scene.frame();
    CHECK(box->getSelectedIndices() == std::vector<int>{1});
    CHECK(chipTexts(box) == (std::vector<String>{"newtag", "web-2"}));
    CHECK(box->getField()->getText() == "");
    CHECK(changes.calls == 2);
}

TEST_CASE("SearchComboBox: multi-select off gives the box back its height flag")
{
    Scene scene;
    SearchComboBox *box=scene.box(3);

    REQUIRE((box->getSizeFlags().vertical == SizeFlag::Fixed));
    box->setMultiSelect(true);
    CHECK((box->getSizeFlags().vertical == SizeFlag::Expand));
    box->setMultiSelect(false);
    CHECK((box->getSizeFlags().vertical == SizeFlag::Fixed));
    CHECK((box->getSizeFlags().horizontal == SizeFlag::Fixed));
}

TEST_CASE("SearchComboBox: multi-select, Up and Down on the closed box open the list, Backspace on a plain box takes the last pick")
{
    Scene scene;
    SearchComboBox *box=multi(scene, {"red", "green", "blue"}, false);
    Counter changes;

    changes.listen(box->onSelectionChange);
    box->setSelectedIndices({0, 1});
    box->focus();
    scene.frame();

    scene.key(keys::Down);
    CHECK(box->isListOpen());
    CHECK(box->getSelectedIndices() == (std::vector<int>{0, 1}));
    scene.key(keys::Escape);
    CHECK(!box->isListOpen());
    scene.key(keys::Up);
    CHECK(box->isListOpen());
    scene.key(keys::Escape);

    scene.key(keys::Backspace);
    CHECK(box->getSelectedIndices() == std::vector<int>{0});
    CHECK(changes.calls == 1);
}

TEST_CASE("SearchComboBox: setOptions keeps the selection and the picks on equal options")
{
    Scene scene;
    SearchComboBox *box=scene.box(0);
    Counter selections;

    box->setOptions({{"A", "a"}, {"B", "b"}, {"C", "c"}});
    selections.listen(box->onSelection);
    box->setSelectedIndex(2);

    // the same label and name, at another index
    box->setOptions({{"X", "x"}, {"C", "c"}, {"A", "a"}});
    CHECK(box->getSelectedIndex() == 1);
    CHECK(box->getValue() == "c");
    CHECK(box->getText() == "C");

    // the same label and another name is another option: the selection is
    // held, at no index
    box->setOptions({{"Y", "y"}, {"C", "other"}});
    CHECK(box->getSelectedIndex() == SearchComboBox::NoOption);
    CHECK(box->isSelectionHeld());
    CHECK(box->getText() == "C");
    CHECK(box->getValue() == "c");

    // an equal option back, through setOptions or addOption, is its again
    box->setOptions({{"Z", "z"}, {"C", "c"}});
    CHECK(box->getSelectedIndex() == 1);
    CHECK(!box->isSelectionHeld());
    box->clearOptions();
    box->addOption("Q", "q");
    CHECK(box->getSelectedIndex() == SearchComboBox::NoOption);
    CHECK(box->getText() == "C");
    box->addOption("C", "c");
    CHECK(box->getSelectedIndex() == 1);
    CHECK(!box->isSelectionHeld());
    CHECK(box->getText() == "C");
    CHECK(selections.calls == 0);

    // multi-select: the picks of options still there stay on them, in pick
    // order, the rest held with their chips, none of them in the indices
    SearchComboBox *picks=multi(scene, {"red", "green", "blue"}, false);
    Counter changes;

    changes.listen(picks->onSelectionChange);
    picks->setSelectedIndices({2, 0, 1});
    REQUIRE(chipTexts(picks) == (std::vector<String>{"blue", "red", "green"}));
    picks->setOptions({{"green", ""}, {"grey", ""}, {"blue", ""}});
    CHECK(picks->getSelectedIndices() == (std::vector<int>{2, 0}));
    CHECK(chipTexts(picks) == (std::vector<String>{"blue", "red", "green"}));
    CHECK(picks->getSelectedValues() == (std::vector<String>{"blue", "red", "green"}));
    CHECK(picks->isPicked(0));
    CHECK(!picks->isPicked(1));
    CHECK(picks->isPicked(2));
    CHECK(changes.calls == 0);

    // red back is picked again, at its new index
    picks->addOption("red");
    CHECK(picks->getSelectedIndices() == (std::vector<int>{2, 3, 0}));
    CHECK(picks->isPicked(3));
    picks->setOptions({{"grey", ""}});
    CHECK(picks->getSelectedIndices().empty());
    CHECK(chipTexts(picks) == (std::vector<String>{"blue", "red", "green"}));
    CHECK(changes.calls == 0);

    // a held pick's x takes it away, onSelectionChange with NoOption
    scene.frame();
    REQUIRE(picks->getChips().size() == 3);
    REQUIRE(picks->getChips()[1]->getRemoveButton());
    scene.clickOn(picks->getChips()[1]->getRemoveButton());
    CHECK(chipTexts(picks) == (std::vector<String>{"blue", "green"}));
    CHECK(changes.calls == 1);
    CHECK(changes.last == static_cast<size_t>(SearchComboBox::NoOption));
    // and setSelectedIndices drops the rest
    picks->setSelectedIndices({0});
    CHECK(chipTexts(picks) == std::vector<String>{"grey"});
    CHECK(changes.calls == 1);
}

TEST_CASE("SearchComboBox: refilled from onTextChangedCaller, Escape and a lost focus give the held value back")
{
    Scene scene;
    const std::vector<String> labels{"web-1", "web-2", "db-1"};
    SearchComboBox *box=filterable(scene, labels);
    controls::TextBox *other=scene.canvas->newChild<controls::TextBox>();
    Refill refill;
    Counter selections;

    other->setBounds(20, 300, 100, 20);
    refill.labels=labels;
    refill.listen(box);
    selections.listen(box->onSelection);
    REQUIRE(box->getSelectedIndex() == 0);

    box->getField()->onSelectAll(box->getField());
    type(scene, "db");
    REQUIRE(box->getOptionCount() == 1);
    // the results hold no web-1: held, still the value
    CHECK(box->isSelectionHeld());
    CHECK(box->getValue() == "web-1");

    SUBCASE("Escape") { scene.key(keys::Escape); }
    SUBCASE("a lost focus") { scene.clickOn(other); }

    CHECK(!box->isListOpen());
    CHECK(box->getField()->getText() == "web-1");
    CHECK(box->getValue() == "web-1");
    CHECK(box->getSelectedIndex() == SearchComboBox::NoOption);
    CHECK(selections.calls == 0);
}

TEST_CASE("SearchComboBox: multi-select refilled from onTextChangedCaller keeps the picks the results leave out")
{
    Scene scene;
    const std::vector<String> labels{"red", "green", "blue", "grey"};
    SearchComboBox *box=multi(scene, labels, true);
    Refill refill;
    Counter changes;

    refill.labels=labels;
    box->setSelectedIndices({0});
    scene.frame();
    refill.listen(box);
    changes.listen(box->onSelectionChange);
    box->getField()->focus();
    type(scene, "gr");
    REQUIRE(box->getOptionCount() == 2);
    CHECK(box->getSelectedValues() == std::vector<String>{"red"});
    CHECK(chipTexts(box) == std::vector<String>{"red"});
    CHECK(box->getSelectedIndices().empty());

    // a second pick from the results adds to it
    scene.key(keys::Return);
    CHECK(box->getSelectedValues() == (std::vector<String>{"red", "green"}));
    CHECK(changes.calls == 1);
    // the app's next results bring red back, picked
    box->setOptions({{"red", ""}, {"green", ""}, {"blue", ""}, {"grey", ""}});
    CHECK(box->isPicked(0));
    CHECK(box->getSelectedIndices() == (std::vector<int>{0, 1}));
    CHECK(changes.calls == 1);
}

TEST_CASE("SearchComboBox: a click on the field's text with the list closed selects it all, so typing replaces it")
{
    Scene scene;
    SearchComboBox *box=filterable(scene, {"feature/search-0001", "beta"});
    KeyboardFocus=nullptr;
    scene.frame();
    REQUIRE(!box->isListOpen());

    controls::internal::SearchComboBoxField *field=box->getField();
    const Rect at=Scene::canvasRect(field);

    // a few characters into the text, press and release through the input
    scene.click(at.x+field->getTextPadding().left+12, at.y+at.h/2);
    CHECK(box->isListOpen());
    CHECK(field->getSelection() == "feature/search-0001");

    type(scene, "x");
    CHECK(field->getText() == "x");
}

TEST_CASE("SearchComboBox: a disabled filterable box opens no list from a click on its field or typing, and picks nothing")
{
    Scene scene;
    SearchComboBox *box=filterable(scene, {"alpha", "beta"});
    Counter selections;
    selections.listen(box->onSelection);

    box->getField()->focus();
    box->setDisabled(true);
    scene.frame();

    scene.clickOn(box->getField());
    CHECK(!box->isListOpen());

    type(scene, "b");
    CHECK(!box->isListOpen());

    scene.key(keys::Return);
    CHECK(selections.calls == 0);
    CHECK(box->getSelectedIndex() == 0);
}
