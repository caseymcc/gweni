/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A ComboBox's items renamed in place (#531), on a real Canvas drawn by the
 *  StubSkin on a StubRenderer, whose Text renderer here draws its string as
 *  the textured skin's does (renderText on any state change), so a frame's
 *  glyph slots and each text run's string can be read.
 *
 *  The selected item renamed, by MenuItem::setText with or without events or
 *  through the Label getSelectedItem() returns, makes the box show the new
 *  string at once, in the phase the app renamed in (its Text marked
 *  StateChange_Text before any frame), and the next frame draws it in the box's
 *  text run, laid out in one update-phase pass with no request and no render-
 *  phase layout. An item that is not selected renamed leaves the box's Text
 *  unmarked and, while the box keeps its width, writes no glyph slot under the
 *  box's text id; the box's preferred width follows a rename of either, which
 *  lays out in one pass too. Renamed to the string it already has, the
 *  selected item marks nothing and the frame writes nothing. No rename fires onSelection, moves the keyboard focus, changes the
 *  selection or a property::ComboBox's value (its item's name). All of it with
 *  the list never opened (the box's child) and opened and closed (the
 *  canvas's).
 *
 *  An item that left the list, given another parent or another box's list, is
 *  renamed without reaching the box it left; renamed in another box's list it
 *  reaches that box, which shows it only if it is its selection. An item of an
 *  opened list renamed after its box was deleted (the list waiting for its
 *  delete) reaches no dead box (ASAN, in that tree).
 */
#include "headlessFixture.h"

#include <gweni/controls/button.h>
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
#include <map>

using namespace gweni;

namespace
{

//! A StubRenderer that also keeps each text run's last string.
class RunRenderer: public test::StubRenderer
{
public:
    void renderText(size_t textId, const gweni::Font &font, gweni::Point pos, int zIndex,
        const gweni::String &text) override
    {
        runs[textId]=text;
        StubRenderer::renderText(textId, font, pos, zIndex, text);
    }

    //! Glyph slots the last frame wrote under \p textId.
    size_t written(size_t textId) const
    {
        for(const renderer::WrittenText &entry : writtenTextIds())
        {
            if(entry.textId == textId)
                return entry.glyphSlots;
        }
        return 0;
    }

    std::map<size_t, gweni::String> runs;
};

//! An internal::Text's renderer as the textured skin's: one text id, its
//! string drawn on every state change.
class RunText: public skin::Control
{
public:
    std::string getTypeName() override { return "Text"; }
    size_t requiredPrimitives() override { return 0; }

    void generate(renderer::Base *renderer, controls::Base *) override
    {
        renderer->generateTextPrimitive(m_textId);
        m_hasText=true;
    }

    void update(renderer::Base *renderer, controls::Base *control) override
    {
        if(control->getStateChange() == controls::StateChange_Nothing)
            return;

        auto *text=dynamic_cast<controls::internal::Text *>(control);

        if(!text || !m_hasText)
            return;
        renderer->renderText(m_textId, text->getFont(), text->textOrigin(), text->getZIndex(),
            text->getText());
    }

    void remove(renderer::Base *renderer, controls::Base *) override
    {
        if(m_hasText)
            renderer->releaseTextPrimitive(m_textId);
        m_hasText=false;
    }

    void appendIds(std::vector<size_t> &primitiveIds, std::vector<size_t> &textIds) const override
    {
        (void)primitiveIds;
        if(m_hasText)
            textIds.push_back(m_textId);
    }

private:
    bool m_hasText=false;
    size_t m_textId=0;
};

//! The StubSkin, with RunText for internal::Text.
class RunSkin: public test::StubSkin
{
public:
    explicit RunSkin(test::StubRenderer *renderer): StubSkin(renderer) {}

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == "Text")
            return std::make_unique<RunText>();
        return StubSkin::getControlRenderer(std::move(typeName));
    }
};

struct Scene
{
    RunRenderer renderer;
    RunSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    float clock=10.0f;

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        // the hovered control, the focus and the button state are process-global
        input::onMouseClicked(canvas, 0, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
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
};

//! Counts onSelection calls.
struct Listener: event::Handler
{
    int calls=0;

    void onSelection(event::Info)
    {
        ++calls;
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

//! The text id \p text's skin Control draws it in.
size_t textIdOf(controls::internal::Text *text)
{
    std::vector<size_t> primitives;
    std::vector<size_t> texts;

    REQUIRE(text);
    REQUIRE(text->getSkinControl());
    text->getSkinControl()->appendIds(primitives, texts);
    REQUIRE(texts.size() == 1);
    return texts[0];
}

bool marked(controls::Base *control, int flags)
{
    return (control->getStateChange() & flags) != 0;
}

//! A ComboBox in a plain box on the canvas with "One", "The longest item" and
//! "Middle one", the first selected, beside a Button holding the keyboard
//! focus, settled; with \p opened its list opened and closed once, which makes
//! it the canvas's child.
struct Box
{
    controls::Base *host;
    controls::ComboBox *combo;
    controls::Button *focused;
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
        focused=host->newChild<controls::Button>("focused");
        focused->setPos(0, 50);
        focused->setText("Focused");
        focused->setKeyboardInputEnabled(true);
        scene.settle();

        if(opened)
        {
            combo->openList();
            scene.settle();
            REQUIRE(combo->isMenuOpen());
            combo->closeList();
            scene.settle();
            REQUIRE(listOf(one)->getParent() == static_cast<controls::Base *>(scene.canvas));
        }
        else
            REQUIRE(listOf(one)->getParent() == static_cast<controls::Base *>(combo));

        focused->focus();
        scene.settle();
        REQUIRE(KeyboardFocus == static_cast<controls::Base *>(focused));
        REQUIRE(combo->getSelectedItem() == one);
        REQUIRE(combo->getText() == "One");
        REQUIRE(scene.renderer.runs[textIdOf(shownText(combo))] == "One");
    }

    //! Nothing about the selection, the focus or onSelection moved.
    void checkUntouched(controls::MenuItem *selected)
    {
        CHECK(combo->getSelectedItem() == static_cast<controls::Label *>(selected));
        CHECK(KeyboardFocus == static_cast<controls::Base *>(focused));
        CHECK(listener.calls == 0);
        CHECK(!combo->isMenuOpen());
        CHECK(!listOf(one)->visible());
    }
};

//! How the selected item is renamed.
enum class Rename
{
    SetText,
    SetTextNoEvents,
    SelectedItem
};

void rename(Box &box, Rename how, const String &text)
{
    switch(how)
    {
    case Rename::SetText:
        box.one->setText(text);
        break;
    case Rename::SetTextNoEvents:
        box.one->setText(text, false);
        break;
    case Rename::SelectedItem:
        box.combo->getSelectedItem()->setText(text);
        break;
    }
}

void runSelectedRename(Rename how, bool opened)
{
    Scene scene;
    Box box(scene, opened);
    controls::ComboBox *combo=box.combo;
    controls::internal::Text *text=shownText(combo);
    const size_t textId=textIdOf(text);
    const int width=combo->getPreferredSize().width;

    rename(box, how, "New");

    // pushed in the phase the app renamed in, not found by a later layout
    CHECK(box.one->getText() == "New");
    CHECK(combo->getText() == "New");
    CHECK(marked(text, controls::StateChange_Text));

    scene.canvas->updateCanvas();

    const controls::LayoutStats stats=scene.canvas->getLayoutStats();

    CHECK(stats.passes == 1);
    CHECK(!stats.capped);
    CHECK(stats.redirtiedType == nullptr);
    CHECK(stats.requests == 0);

    scene.canvas->renderCanvas();
    CHECK(!scene.canvas->getLayoutStats().renderLayout);
    CHECK(scene.renderer.runs[textId] == "New");
    CHECK(scene.renderer.written(textId) == 3);

    scene.settle();
    CHECK(combo->getText() == "New");
    CHECK(scene.renderer.runs[textId] == "New");
    // "The longest item" still decides the width
    CHECK(combo->getPreferredSize().width == width);
    box.checkUntouched(box.one);

    // and again, longer than any other item
    rename(box, how, "A much longer name than any");
    CHECK(combo->getText() == "A much longer name than any");
    scene.settle();
    CHECK(scene.renderer.runs[textId] == "A much longer name than any");
    CHECK(combo->getPreferredSize().width > width);
    box.checkUntouched(box.one);
}

void runUnselectedRename(bool opened)
{
    Scene scene;
    Box box(scene, opened);
    controls::ComboBox *combo=box.combo;
    controls::internal::Text *text=shownText(combo);
    const size_t textId=textIdOf(text);
    const int width=combo->getPreferredSize().width;

    // the same width, so the box's bounds stay as they are
    box.middle->setText("Middle two");
    CHECK(combo->getText() == "One");
    CHECK(!marked(text, controls::StateChange_Text));

    scene.canvas->updateCanvas();

    const controls::LayoutStats stats=scene.canvas->getLayoutStats();

    CHECK(stats.passes <= 1);
    CHECK(stats.redirtiedType == nullptr);
    CHECK(stats.requests == 0);
    CHECK(!marked(text, controls::StateChange_Text));

    scene.canvas->renderCanvas();
    CHECK(!scene.canvas->getLayoutStats().renderLayout);
    CHECK(scene.renderer.written(textId) == 0);
    CHECK(scene.renderer.runs[textId] == "One");

    scene.settle();
    CHECK(combo->getText() == "One");
    CHECK(combo->getPreferredSize().width == width);
    box.checkUntouched(box.one);

    // without events, and the widest renamed: only the width follows (the
    // narrower box gives its Text a new clip box, which redraws its run, with
    // the string it had)
    box.longest->setText("Short", false);
    CHECK(combo->getText() == "One");
    CHECK(!marked(text, controls::StateChange_Text));
    scene.canvas->updateCanvas();
    CHECK(scene.canvas->getLayoutStats().passes == 1);
    CHECK(scene.canvas->getLayoutStats().requests == 0);
    CHECK(!marked(text, controls::StateChange_Text));
    scene.canvas->renderCanvas();
    CHECK(scene.renderer.runs[textId] == "One");
    scene.settle();
    CHECK(combo->getText() == "One");
    CHECK(scene.renderer.runs[textId] == "One");
    CHECK(combo->getPreferredSize().width < width);
    box.checkUntouched(box.one);
}

void runSameStringRename(Rename how, bool opened)
{
    Scene scene;
    Box box(scene, opened);
    controls::ComboBox *combo=box.combo;
    controls::internal::Text *text=shownText(combo);

    // settled: a frame writes nothing
    scene.frame();
    REQUIRE(scene.renderer.frameStats().primitiveSlotsWritten == 0);
    REQUIRE(scene.renderer.frameStats().textSlotsWritten == 0);

    rename(box, how, "One");

    CHECK(combo->getText() == "One");
    CHECK(text->getStateChange() == controls::StateChange_Nothing);
    CHECK(combo->getStateChange() == controls::StateChange_Nothing);
    CHECK(box.one->getStateChange() == controls::StateChange_Nothing);

    scene.canvas->updateCanvas();
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    scene.canvas->renderCanvas();
    CHECK(scene.renderer.frameStats().primitiveSlotsWritten == 0);
    CHECK(scene.renderer.frameStats().textSlotsWritten == 0);
    box.checkUntouched(box.one);
}

} // namespace

TEST_CASE("a ComboBox's selected item renamed with setText shows the new text")
{
    SUBCASE("list never opened") { runSelectedRename(Rename::SetText, false); }
    SUBCASE("list opened and closed") { runSelectedRename(Rename::SetText, true); }
}

TEST_CASE("a ComboBox's selected item renamed with setText(text, false) shows the new text")
{
    SUBCASE("list never opened") { runSelectedRename(Rename::SetTextNoEvents, false); }
    SUBCASE("list opened and closed") { runSelectedRename(Rename::SetTextNoEvents, true); }
}

TEST_CASE("a ComboBox's selected item renamed through getSelectedItem() shows the new text")
{
    SUBCASE("list never opened") { runSelectedRename(Rename::SelectedItem, false); }
    SUBCASE("list opened and closed") { runSelectedRename(Rename::SelectedItem, true); }
}

TEST_CASE("a ComboBox's item that is not selected renamed leaves the box's text alone")
{
    SUBCASE("list never opened") { runUnselectedRename(false); }
    SUBCASE("list opened and closed") { runUnselectedRename(true); }
}

TEST_CASE("a ComboBox's selected item renamed to its own text marks nothing")
{
    SUBCASE("setText, list never opened") { runSameStringRename(Rename::SetText, false); }
    SUBCASE("setText, list opened and closed") { runSameStringRename(Rename::SetText, true); }
    SUBCASE("setText(text, false), list never opened") { runSameStringRename(Rename::SetTextNoEvents, false); }
    SUBCASE("setText(text, false), list opened and closed") { runSameStringRename(Rename::SetTextNoEvents, true); }
    SUBCASE("getSelectedItem(), list never opened") { runSameStringRename(Rename::SelectedItem, false); }
    SUBCASE("getSelectedItem(), list opened and closed") { runSameStringRename(Rename::SelectedItem, true); }
}

TEST_CASE("a property::ComboBox's value is its item's name, kept through a rename")
{
    Scene scene;
    auto *property=scene.canvas->newChild<controls::property::ComboBox>();
    controls::ComboBox *combo=property->getComboBox();
    controls::MenuItem *first=combo->addItem("First", "first");

    combo->addItem("Second", "second");
    scene.settle();
    REQUIRE(property->getPropertyValue() == "first");

    first->setText("Renamed");
    scene.settle();

    CHECK(combo->getText() == "Renamed");
    CHECK(combo->getSelectedItem() == static_cast<controls::Label *>(first));
    CHECK(property->getPropertyValue() == "first");
}

TEST_CASE("an item that left a ComboBox's list is renamed without reaching the box")
{
    bool opened=false;

    SUBCASE("list never opened") { opened=false; }
    SUBCASE("list opened and closed") { opened=true; }

    Scene scene;
    controls::Base *other=scene.canvas->newChild<controls::Base>("other");
    Box box(scene, opened);
    controls::ComboBox *combo=box.combo;
    const size_t textId=textIdOf(shownText(combo));

    // the selected item given another parent: the selection is dropped
    box.one->setParent(other);
    scene.settle();
    REQUIRE(combo->getSelectedItem() == nullptr);
    REQUIRE(combo->getText() == "");

    box.one->setText("Moved");
    CHECK(combo->getText() == "");
    CHECK(!marked(shownText(combo), controls::StateChange_Text));
    scene.settle();
    CHECK(combo->getText() == "");
    CHECK(scene.renderer.runs[textId] == "");
    CHECK(combo->getSelectedItem() == nullptr);
    CHECK(box.listener.calls == 0);

    // the box deleted, the item it left renamed again (ASAN)
    delete combo;
    scene.settle();
    box.one->setText("After the box");
    box.one->setText("After the box, no events", false);
    scene.settle();
    CHECK(box.one->getText() == "After the box, no events");
}

TEST_CASE("an item given to another ComboBox's list reaches that box when renamed")
{
    Scene scene;
    Box box(scene, false);
    controls::ComboBox *second=box.host->newChild<controls::ComboBox>();

    second->setPos(0, 25);
    controls::MenuItem *secondsOwn=second->addItem("Second's own", "own");
    scene.settle();
    REQUIRE(second->getSelectedItem() == static_cast<controls::Label *>(secondsOwn));

    box.one->setParent(listOf(secondsOwn));
    scene.settle();
    REQUIRE(box.combo->getSelectedItem() == nullptr);

    // not the second box's selection, nor the first's any more
    box.one->setText("Moved");
    scene.settle();
    CHECK(box.combo->getText() == "");
    CHECK(box.combo->getSelectedItem() == nullptr);
    CHECK(second->getText() == "Second's own");
    CHECK(second->getSelectedItem() == static_cast<controls::Label *>(secondsOwn));

    // the second box's own selection renamed
    secondsOwn->setText("Renamed own");
    CHECK(second->getText() == "Renamed own");
    scene.settle();
    CHECK(scene.renderer.runs[textIdOf(shownText(second))] == "Renamed own");
    CHECK(box.combo->getText() == "");
    CHECK(box.listener.calls == 0);
}

TEST_CASE("an item of an opened ComboBox list renamed after the box was deleted reaches no dead box")
{
    Scene scene;
    Box box(scene, true);
    controls::MenuItem *one=box.one;
    controls::MenuItem *middle=box.middle;
    controls::Menu *list=listOf(one);

    REQUIRE(list->getParent() == static_cast<controls::Base *>(scene.canvas));

    // the list is delayedDelete()d with the box: its items live until the next
    // update phase, and may be renamed meanwhile (ASAN, in that tree)
    delete box.host;
    one->setText("Selected, the box gone");
    middle->setText("Not selected, the box gone", false);
    one->setText("Selected, the box gone");
    scene.settle();
}
