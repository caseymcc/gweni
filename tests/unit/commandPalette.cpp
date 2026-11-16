/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  controls::CommandPalette on a real Canvas with the stub skin, keyed
 *  through input::onKeyEvent and input::handleAccelerator, clicked through
 *  input::onMouseClicked.
 *
 *  Ctrl+Shift+P, registered on the canvas by attach(), opens it (a Popup at
 *  the canvas's top centre, CanvasTop, inside the canvas) with the keyboard
 *  focus in its field, and a second Ctrl+Shift+P closes it, the focus back on
 *  the TextBox that had it; with no focus before, none after. A query lists
 *  the commands whose "category: label" holds its characters in order, best
 *  first (a prefix above a word start, ties by the order added); an empty
 *  query lists every enabled command by category and label, and after runs
 *  the run ones first, latest first (setRecentFirst(false) leaves them in
 *  place). A disabled command (its flag, or a predicate asked at each match)
 *  is not listed, and with nothing listed one disabled "No matching
 *  commands" row shows, which Return runs nothing on, its press and release
 *  leaving the field focused. A press in the popup off the field (its
 *  padding, the list's bar, that row) keeps the field focused, and the
 *  field's "Type a command" hint shows while it is empty and focused (a
 *  plain TextBox's only while unfocused). Tab and Shift+Tab are the field's
 *  and move no focus out of the open palette. Up and Down move the
 *  highlight and wrap at both ends, PageUp and PageDown move MaxShownRows and
 *  stop at the ends, Return runs the highlighted command once (onCommand with
 *  its id, after the palette closed and the focus came back), Escape closes
 *  it running nothing, and a press outside it closes it. A keystroke binds
 *  again only the rows whose command changed and makes no control; the
 *  pool, the popup's controls, the live ids and a keystroke's binds are the
 *  same with 50 and with 5 000 commands; opening a second time makes no
 *  control; deleting the palette takes its accelerator off the canvas, and
 *  deleting it open closes it first, the focus back on the TextBox.
 */
#include "headlessFixture.h"

#include <gweni/controls/commandPalette.h>
#include <gweni/controls/textBox.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/primitiveIds.h>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace gweni;
using controls::CommandPalette;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::TextBox *box=nullptr;
    CommandPalette *palette=nullptr;
    float clock=10.0f;

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(800, 600);
        box=canvas->newChild<controls::TextBox>();
        box->setBounds(10, 560, 100, 20);
        palette=canvas->newChild<CommandPalette>();
        palette->attach(canvas, "Ctrl+Shift+P");
        frame();
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

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void key(int key)
    {
        input::onKeyEvent(canvas, key, true);
        input::onKeyEvent(canvas, key, false);
        frame();
    }

    //! Ctrl+Shift+P, as input::GLFW3 delivers it.
    bool accelerator()
    {
        input::onKeyEvent(canvas, keys::Control, true);
        input::onKeyEvent(canvas, keys::Shift, true);
        const bool taken=input::handleAccelerator(canvas, 'P');
        input::onKeyEvent(canvas, keys::Shift, false);
        input::onKeyEvent(canvas, keys::Control, false);
        frame();
        return taken;
    }

    void type(const String &text)
    {
        for(char c : text)
            palette->getField()->insertText(String(1, c));
        frame();
    }

    void click(int x, int y)
    {
        clock+=1.0f;
        canvas->inputMouseMoved(x, y, 0, 0);
        input::onMouseClicked(canvas, 0, true);
        input::onMouseClicked(canvas, 0, false);
        frame();
    }

    void add(const String &id, const String &label, const String &category, bool enabled=true)
    {
        CommandPalette::Command command;

        command.id=id;
        command.label=label;
        command.category=category;
        command.enabled=enabled;
        palette->addCommand(command);
    }

    //! "category: label" of every shown command, in order.
    std::vector<String> shown() const
    {
        std::vector<String> texts;

        for(size_t index : palette->getShownCommands())
            texts.push_back(CommandPalette::matchText(palette->getCommand(index)));
        return texts;
    }

    String highlighted() const
    {
        const int index=palette->getHighlightedIndex();

        return (index == CommandPalette::NoCommand)?String():palette->getCommand(index).id;
    }
};

class Recorder: public event::Handler
{
public:
    void run(event::Info info)
    {
        ids.push_back(info.string);
        paletteOpen=palette && palette->isOpen();
        focus=KeyboardFocus;
    }

    std::vector<String> ids;
    CommandPalette *palette=nullptr;
    bool paletteOpen=false;
    controls::Base *focus=nullptr;
};

size_t liveIds()
{
    return primitiveIdCount()-releasedPrimitiveIdCount()+textIdCount()-releasedTextIdCount();
}

size_t subtreeSize(controls::Base *control)
{
    size_t count=1;

    for(controls::Base *child : control->getChildren())
        count+=subtreeSize(child);
    return count;
}

void addNumbered(Scene &scene, size_t count)
{
    static const char *const categories[]={"File", "Edit", "Selection", "View", "Go", "Run", "Terminal", "Git",
        "Debug", "Help"};
    static const char *const verbs[]={"Open", "Close", "Toggle", "Show", "Reload", "Find"};
    static const char *const nouns[]={"File", "Editor", "Panel", "Terminal", "Window", "Branch"};

    // each category's "Open File" first; past 360 a numbered round
    for(size_t i=0; i < count; ++i)
    {
        String label=String(verbs[(i/10)%6])+" "+nouns[(i/60)%6];

        if(i >= 360)
            label+=" "+std::to_string(i/360);
        scene.add("c"+std::to_string(i), label, categories[i%10]);
    }
}

}//namespace

TEST_CASE("CommandPalette: Ctrl+Shift+P opens it at the canvas's top centre with the field focused, a second press closes it")
{
    Scene scene;

    scene.add("save", "Save", "File");
    scene.box->focus();
    REQUIRE(KeyboardFocus == scene.box);

    CHECK(scene.accelerator());
    REQUIRE(scene.palette->isOpen());
    CHECK(KeyboardFocus == scene.palette->getField());
    CHECK(scene.palette->getField()->getPlaceholder() == CommandPalette::placeholderText());

    const controls::Base *popup=scene.palette->getPopup();

    CHECK(popup->getWidth() == CommandPalette::Width);
    CHECK(popup->getX() == (800-CommandPalette::Width)/2);
    CHECK(popup->getY() == controls::Popup::CanvasTopInset);

    CHECK(scene.accelerator());
    CHECK_FALSE(scene.palette->isOpen());
    CHECK(KeyboardFocus == scene.box);

    // no focus before, none after
    scene.box->unfocus();
    scene.accelerator();
    REQUIRE(scene.palette->isOpen());
    scene.accelerator();
    CHECK(KeyboardFocus == nullptr);
}

TEST_CASE("CommandPalette: a narrow canvas makes it narrower, inside the canvas")
{
    Scene scene;

    scene.add("save", "Save", "File");
    scene.canvas->setSize(400, 300);
    scene.frame();
    scene.palette->open();
    scene.frame();

    const controls::Base *popup=scene.palette->getPopup();

    CHECK(popup->getWidth() == 400-2*CommandPalette::CanvasInset);
    CHECK(popup->getX() == CommandPalette::CanvasInset);
}

TEST_CASE("CommandPalette: a query lists the commands holding its characters in order, best first, ties in the order added")
{
    Scene scene;

    scene.add("a", "Toggle Terminal", "View");
    scene.add("b", "Close Editor", "File");
    scene.add("c", "Open Terminal", "Terminal");
    scene.add("d", "Toggle Sidebar", "View");
    scene.add("e", "Save", "File");
    scene.palette->open();
    scene.frame();

    scene.type("term");
    // "Terminal: ..." starts with it; the others have it at a word start
    CHECK(scene.shown() == std::vector<String>({"Terminal: Open Terminal", "View: Toggle Terminal"}));
    CHECK(scene.highlighted() == "c");

    scene.palette->getField()->setText("");
    scene.type("vt");
    // equal scores: the order they were added
    CHECK(scene.shown() == std::vector<String>({"View: Toggle Terminal", "View: Toggle Sidebar"}));

    scene.palette->getField()->setText("");
    scene.frame();
    scene.type("xyz");
    CHECK(scene.shown().empty());
    CHECK(scene.palette->shownRowCount() == 1);
    CHECK(scene.highlighted() == "");
}

TEST_CASE("CommandPalette: an empty query lists by category and label, the recently run first, latest first")
{
    Scene scene;
    Recorder ran;

    scene.palette->onCommand.add(&ran, &Recorder::run);
    scene.add("v2", "Zoom In", "View");
    scene.add("f1", "Save", "File");
    scene.add("v1", "Toggle Sidebar", "View");
    scene.add("e1", "Undo", "Edit");

    scene.palette->open();
    scene.frame();
    CHECK(scene.shown() == std::vector<String>({"Edit: Undo", "File: Save", "View: Toggle Sidebar", "View: Zoom In"}));

    CHECK(scene.palette->runCommand("v2"));
    CHECK(scene.palette->runCommand("f1"));
    CHECK(ran.ids == std::vector<String>({"v2", "f1"}));
    CHECK(scene.palette->getRecent() == std::vector<String>({"f1", "v2"}));

    scene.palette->open();
    scene.frame();
    CHECK(scene.shown() == std::vector<String>({"File: Save", "View: Zoom In", "Edit: Undo", "View: Toggle Sidebar"}));

    scene.palette->setRecentFirst(false);
    CHECK(scene.shown() == std::vector<String>({"Edit: Undo", "File: Save", "View: Toggle Sidebar", "View: Zoom In"}));
}

TEST_CASE("CommandPalette: disabled commands, by flag or predicate, are not listed and nothing listed shows one disabled row")
{
    Scene scene;
    bool enabled=false;

    scene.add("a", "Save", "File");
    scene.add("b", "Save All", "File", false);

    CommandPalette::Command guarded;

    guarded.id="c";
    guarded.label="Save As";
    guarded.category="File";
    guarded.isEnabled=[&enabled]() { return enabled; };
    scene.palette->addCommand(guarded);

    scene.palette->open();
    scene.frame();
    CHECK(scene.shown() == std::vector<String>({"File: Save"}));
    CHECK_FALSE(scene.palette->runCommand("b"));

    scene.palette->close();
    enabled=true;
    scene.palette->open();
    scene.frame();
    CHECK(scene.shown() == std::vector<String>({"File: Save", "File: Save As"}));

    scene.type("all");
    CHECK(scene.shown().empty());
    REQUIRE(scene.palette->shownRowCount() == 1);

    auto *row=gweni_cast<controls::internal::CommandPaletteRow>(scene.palette->getList()->rowForItem(0));

    REQUIRE(row);
    CHECK(row->isDisabled());
    CHECK(row->getLabelText() == CommandPalette::noMatchesText());

    Recorder ran;

    scene.palette->onCommand.add(&ran, &Recorder::run);
    scene.key(keys::Return);
    CHECK(ran.ids.empty());
    CHECK(scene.palette->isOpen());
    // the press and the release are the field's: no Tab on, no unfocus
    CHECK(KeyboardFocus == scene.palette->getField());

    scene.canvas->inputCharacter('x');
    scene.frame();
    CHECK(scene.palette->getQuery() == "allx");
}

TEST_CASE("CommandPalette: Up and Down wrap, PageUp and PageDown move a page and stop, Return runs once and closes, Escape closes")
{
    Scene scene;
    Recorder ran;

    ran.palette=scene.palette;
    scene.palette->onCommand.add(&ran, &Recorder::run);
    addNumbered(scene, 25);
    scene.box->focus();
    scene.accelerator();
    REQUIRE(scene.palette->isOpen());
    REQUIRE(scene.palette->getShownCommands().size() == 25);
    CHECK(scene.palette->getHighlightedPosition() == 0);

    scene.key(keys::Up);
    CHECK(scene.palette->getHighlightedPosition() == 24);
    scene.key(keys::Down);
    CHECK(scene.palette->getHighlightedPosition() == 0);
    scene.key(keys::Down);
    CHECK(scene.palette->getHighlightedPosition() == 1);

    scene.key(keys::PageDown);
    CHECK(scene.palette->getHighlightedPosition() == 1+CommandPalette::MaxShownRows);
    scene.key(keys::PageDown);
    scene.key(keys::PageDown);
    CHECK(scene.palette->getHighlightedPosition() == 24);
    scene.key(keys::PageUp);
    CHECK(scene.palette->getHighlightedPosition() == 24-CommandPalette::MaxShownRows);
    scene.key(keys::PageUp);
    scene.key(keys::PageUp);
    CHECK(scene.palette->getHighlightedPosition() == 0);

    scene.key(keys::Down);
    scene.key(keys::Down);

    const String expected=scene.highlighted();

    // the highlighted row shows it
    auto *row=gweni_cast<controls::internal::CommandPaletteRow>(scene.palette->getList()->rowForItem(2));

    REQUIRE(row);
    CHECK(row->isHighlighted());

    scene.key(keys::Return);
    CHECK(ran.ids == std::vector<String>({expected}));
    CHECK_FALSE(ran.paletteOpen);
    CHECK(ran.focus == scene.box);
    CHECK_FALSE(scene.palette->isOpen());
    CHECK(KeyboardFocus == scene.box);

    scene.accelerator();
    REQUIRE(scene.palette->isOpen());
    scene.key(keys::Escape);
    CHECK_FALSE(scene.palette->isOpen());
    CHECK(ran.ids.size() == 1);
    CHECK(KeyboardFocus == scene.box);
}

TEST_CASE("CommandPalette: a press outside it closes it, a press on a row runs that row's command")
{
    Scene scene;
    Recorder ran;

    scene.palette->onCommand.add(&ran, &Recorder::run);
    addNumbered(scene, 5);
    scene.palette->open();
    scene.frame();
    REQUIRE(scene.palette->isOpen());

    // the canvas's bottom-left, away from the popup and the box
    scene.click(400, 590);
    CHECK_FALSE(scene.palette->isOpen());
    CHECK(ran.ids.empty());

    scene.palette->open();
    scene.frame();

    controls::Base *row=scene.palette->getList()->rowForItem(1);

    REQUIRE(row);

    const String second=scene.palette->getCommand(scene.palette->getShownCommands()[1]).id;
    const Point at=row->localPosToCanvas();

    scene.click(at.x+row->getWidth()/2, at.y+row->getHeight()/2);
    CHECK(ran.ids == std::vector<String>({second}));
    CHECK_FALSE(scene.palette->isOpen());
}

TEST_CASE("CommandPalette: a press in the popup off the field, on its padding, the list's bar or the no-matches row, keeps the field focused")
{
    Scene scene;

    addNumbered(scene, 50);
    scene.palette->open();
    scene.frame();
    REQUIRE(KeyboardFocus == scene.palette->getField());

    controls::Base *popup=scene.palette->getPopup();
    const Point corner=popup->localPosToCanvas();

    // the popup's own padding, left of the field
    REQUIRE(scene.canvas->getControlAt(corner.x+1, corner.y+1) == popup);
    scene.click(corner.x+1, corner.y+1);
    CHECK(scene.palette->isOpen());
    CHECK(KeyboardFocus == scene.palette->getField());

    // the list's scroll bar (50 commands, 10 shown)
    controls::VirtualList *list=scene.palette->getList();
    const Point listAt=list->localPosToCanvas();
    const int barX=listAt.x+list->getWidth()-2;
    const int barY=listAt.y+list->getHeight()/2;
    controls::Base *bar=scene.canvas->getControlAt(barX, barY);

    REQUIRE(bar);
    CHECK(bar != list->rowForItem(0));
    scene.click(barX, barY);
    CHECK(scene.palette->isOpen());
    CHECK(KeyboardFocus == scene.palette->getField());

    // the disabled "No matching commands" row
    scene.type("zzzz");
    REQUIRE(scene.palette->shownRowCount() == 1);

    controls::Base *row=list->rowForItem(0);
    const Point rowAt=row->localPosToCanvas();

    scene.click(rowAt.x+row->getWidth()/2, rowAt.y+row->getHeight()/2);
    CHECK(scene.palette->isOpen());
    CHECK(KeyboardFocus == scene.palette->getField());

    scene.canvas->inputCharacter('q');
    scene.frame();
    CHECK(scene.palette->getQuery() == "zzzzq");
}

TEST_CASE("CommandPalette: its field shows the placeholder while empty and focused, a plain TextBox's only unfocused")
{
    Scene scene;

    scene.add("save", "Save", "File");
    scene.box->setPlaceholder("hint");
    scene.frame();
    CHECK(scene.box->isPlaceholderShown());
    scene.box->focus();
    scene.frame();
    CHECK_FALSE(scene.box->isPlaceholderShown());

    scene.accelerator();
    REQUIRE(KeyboardFocus == scene.palette->getField());
    CHECK(scene.palette->getField()->isPlaceholderShown());

    scene.type("s");
    CHECK_FALSE(scene.palette->getField()->isPlaceholderShown());

    scene.key(keys::Backspace);
    REQUIRE(scene.palette->getQuery().empty());
    CHECK(scene.palette->getField()->isPlaceholderShown());
}

TEST_CASE("CommandPalette: Tab and Shift+Tab in the open palette keep the focus in its field, and Escape gives it back")
{
    Scene scene;

    scene.add("save", "Save", "File");
    scene.add("open", "Open", "File");
    scene.box->focus();
    scene.accelerator();
    REQUIRE(KeyboardFocus == scene.palette->getField());

    scene.key(keys::Tab);
    CHECK(scene.palette->isOpen());
    CHECK(KeyboardFocus == scene.palette->getField());

    input::onKeyEvent(scene.canvas, keys::Shift, true);
    scene.key(keys::Tab);
    input::onKeyEvent(scene.canvas, keys::Shift, false);
    CHECK(scene.palette->isOpen());
    CHECK(KeyboardFocus == scene.palette->getField());

    // a character still reaches the query, not the app's box
    scene.type("o");
    CHECK(scene.palette->getQuery() == "o");
    CHECK(scene.box->getText().empty());

    scene.key(keys::Escape);
    CHECK_FALSE(scene.palette->isOpen());
    CHECK(KeyboardFocus == scene.box);
}

TEST_CASE("CommandPalette: deleting it while open closes it first, the focus given back")
{
    Scene scene;

    scene.add("save", "Save", "File");
    scene.box->focus();
    scene.accelerator();
    REQUIRE(scene.palette->isOpen());
    REQUIRE(KeyboardFocus == scene.palette->getField());

    controls::Base *popup=scene.palette->getPopup();

    delete scene.palette;
    scene.palette=nullptr;
    CHECK(KeyboardFocus == scene.box);
    CHECK(scene.canvas->shownPopup() != popup);
    scene.frame();
    scene.frame();
}

TEST_CASE("CommandPalette: a keystroke binds only the rows whose command changed, and opening again makes no control")
{
    Scene scene;

    addNumbered(scene, 60);
    scene.palette->open();
    scene.frame();

    const size_t controls=subtreeSize(scene.palette->getPopup());

    scene.type("open fil");

    const size_t binds=scene.palette->getRowBindCount();
    const std::vector<size_t> before=scene.palette->getShownCommands();

    // "open file": the same commands, nothing bound again
    scene.type("e");
    CHECK(scene.palette->getShownCommands() == before);
    CHECK(scene.palette->getRowBindCount() == binds);
    CHECK(subtreeSize(scene.palette->getPopup()) == controls);

    scene.palette->close();
    scene.frame();
    scene.palette->open();
    scene.frame();
    CHECK(subtreeSize(scene.palette->getPopup()) == controls);
}

TEST_CASE("CommandPalette: the pool, the popup's controls, the live ids and a keystroke's binds are the same at 50 and 5 000 commands")
{
    struct Counts
    {
        size_t pool=0;
        size_t controls=0;
        size_t ids=0;
        size_t binds=0;
        std::vector<size_t> shown;
    };

    auto measure=[](size_t count) -> Counts
        {
            Scene scene;
            const size_t baseline=liveIds();

            addNumbered(scene, count);
            scene.palette->open();
            scene.frame();
            // "Open File" is each category's first command: the same ten at
            // both counts (the later rounds are numbered and come after)
            scene.type("open fil");

            const size_t binds=scene.palette->getRowBindCount();

            scene.type("e");

            Counts counts;

            counts.pool=scene.palette->getList()->getRows().size();
            counts.controls=subtreeSize(scene.palette->getPopup());
            counts.ids=liveIds()-baseline;
            counts.binds=scene.palette->getRowBindCount()-binds;

            const std::vector<size_t> &shown=scene.palette->getShownCommands();

            counts.shown.assign(shown.begin(), shown.begin()+std::min<size_t>(shown.size(), 10));
            return counts;
        };

    const Counts small=measure(50);
    const Counts large=measure(5000);

    CHECK(small.pool == large.pool);
    CHECK(small.controls == large.controls);
    CHECK(small.ids == large.ids);
    CHECK(small.binds == large.binds);
    CHECK(small.shown == large.shown);
    CHECK(small.shown.size() == 10);
}

TEST_CASE("CommandPalette: deleting it takes its accelerator off the canvas")
{
    Scene scene;

    scene.add("save", "Save", "File");
    CHECK(scene.canvas->m_accelerators.count(controls::Base::acceleratorKey("Ctrl+Shift+P")) == 1);

    delete scene.palette;
    scene.palette=nullptr;
    CHECK(scene.canvas->m_accelerators.count(controls::Base::acceleratorKey("Ctrl+Shift+P")) == 0);
    scene.frame();
    CHECK_FALSE(scene.accelerator());
}
