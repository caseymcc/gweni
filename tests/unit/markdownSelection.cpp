/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A MarkdownDocument's selection across its blocks, through the GLFW
 *  translator's keys and the canvas's mouse on the StubRenderer. It pins:
 *  - every text is on the document's SelectionModel at its path in reading
 *    order: a paragraph {i}, a list item's marker {i, k, 0} and its blocks
 *    {i, k, 1+j}, a code block's text {i}, a table's cell {i, row, column};
 *  - Ctrl+A selects everything and Ctrl+C copies it as shown: a newline
 *    between texts, a space after a marker, a tab between a row's cells (an
 *    empty one included), a code block's text with its own newlines;
 *  - a press in the first paragraph and a drag over a list, a code block
 *    and into a table select from the press to the mouse in reading order,
 *    each block drawing its part, and Ctrl+C copies exactly that; a drag
 *    into the gap between blocks lands at the next block's start;
 *  - a drag step re-marks only the texts whose part changed;
 *  - a double-click selects a word, Escape clears the selection;
 *  - the selection survives a setMarkdown that edits a block outside it,
 *    and one that inserts a block above it (the ends follow their texts to
 *    their new paths); a setMarkdown that removes the block an end is in
 *    clears it.
 */
#include "headlessFixture.h"

#include <gweni/controls/markdownDocument.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/input/glfw3.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace gweni;
using controls::MarkdownDocument;
using controls::internal::MarkdownCodeBlock;
using controls::internal::MarkdownList;
using controls::internal::MarkdownListItem;
using controls::internal::MarkdownTable;
using controls::internal::MarkdownText;
using Kind=MarkdownDocument::Selectable::Kind;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    MarkdownDocument *document=nullptr;
    input::GLFW3 glfw;
    double clock=500.0;

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(600, 800);
        glfw.initialize(canvas);
        document=canvas->newChild<MarkdownDocument>();
        frame();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        glfw.processFocus(false);
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

    void show(const std::string &markdown)
    {
        document->setMarkdown(markdown);
        for(int i=0; i < 4; ++i)
            frame();
    }

    void move(const Point &at)
    {
        canvas->inputMouseMoved(at.x, at.y, 0, 0);
        frame();
    }

    void press(bool down)
    {
        canvas->inputMouseButton(0, down);
        frame();
    }

    void click(const Point &at)
    {
        clock+=1.0;
        move(at);
        press(true);
        press(false);
    }

    void doubleClick(const Point &at)
    {
        click(at);
        canvas->inputMouseButton(0, true);
        canvas->inputMouseButton(0, false);
        frame();
    }

    void key(int key, int mods=0)
    {
        if(mods&GLFW_MOD_CONTROL)
            glfw.processKeyEvent(input::GLFW3::KeyEvent{GLFW_KEY_LEFT_CONTROL, 0, GLFW_PRESS, GLFW_MOD_CONTROL});
        glfw.processKeyEvent(input::GLFW3::KeyEvent{key, 0, GLFW_PRESS, mods});
        glfw.processKeyEvent(input::GLFW3::KeyEvent{key, 0, GLFW_RELEASE, mods});
        if(mods&GLFW_MOD_CONTROL)
            glfw.processKeyEvent(input::GLFW3::KeyEvent{GLFW_KEY_LEFT_CONTROL, 0, GLFW_RELEASE, 0});
        frame();
    }

    template<typename T>
    T *block(size_t index)
    {
        T *control=dynamic_cast<T *>(document->getBlockControl(index));

        REQUIRE(control);
        return control;
    }
};

//! The canvas point at caret \p caret of piece \p piece of \p text, halfway down its line
Point caretPoint(MarkdownText *text, size_t piece, size_t caret)
{
    const text::RichPiece &p=text->getLayout().pieces().at(piece);

    return text->localPosToCanvas(Point(text->contentOrigin().x+p.pos.x+p.carets.at(caret),
        text->contentOrigin().y+p.pos.y+p.metrics.lineHeight/2));
}

const char *reference=
    "Intro paragraph here.\n"
    "\n"
    "- first item\n"
    "- second item\n"
    "\n"
    "```\n"
    "code line one\n"
    "code line two\n"
    "```\n"
    "\n"
    "| Name | Value |\n"
    "|------|-------|\n"
    "| alpha | 1 |\n"
    "| beta |  |\n"
    "\n"
    "Closing paragraph.\n";

const char *everything=
    "Intro paragraph here.\n"
    "\xE2\x80\xA2 first item\n"
    "\xE2\x80\xA2 second item\n"
    "code line one\ncode line two\n"
    "Name\tValue\n"
    "alpha\t1\n"
    "beta\t\n"
    "Closing paragraph.";

}//namespace

TEST_CASE("Markdown selection: every text on the document's model at its path in reading order")
{
    Scene scene;

    scene.show(reference);

    const std::vector<MarkdownDocument::Selectable> &selectables=scene.document->getSelectables();
    using Path=std::vector<size_t>;
    const std::vector<std::pair<Path, Kind>> expected=
    {
        { Path{ 0 }, Kind::Text },
        { Path{ 1, 0, 0 }, Kind::Marker }, { Path{ 1, 0, 1 }, Kind::Text },
        { Path{ 1, 1, 0 }, Kind::Marker }, { Path{ 1, 1, 1 }, Kind::Text },
        { Path{ 2 }, Kind::Text },
        { Path{ 3, 0, 0 }, Kind::Cell }, { Path{ 3, 0, 1 }, Kind::Cell },
        { Path{ 3, 1, 0 }, Kind::Cell }, { Path{ 3, 1, 1 }, Kind::Cell },
        { Path{ 3, 2, 0 }, Kind::Cell }, { Path{ 3, 2, 1 }, Kind::Cell },
        { Path{ 4 }, Kind::Text },
    };

    REQUIRE(selectables.size() == expected.size());
    for(size_t i=0; i < expected.size(); ++i)
    {
        INFO(i);
        CHECK(selectables[i].path == expected[i].first);
        CHECK(selectables[i].kind == expected[i].second);
        CHECK(&selectables[i].text->selectionModel() == &scene.document->selectionModel());
        CHECK(selectables[i].text->selectionBlock() == expected[i].first);
    }
    CHECK(selectables[5].text == scene.block<MarkdownCodeBlock>(2)->getText());
    CHECK(selectables[1].text == dynamic_cast<MarkdownListItem *>(
        scene.block<MarkdownList>(1)->getBlockControl(0))->getMarker());
}

TEST_CASE("Markdown selection: Ctrl+A and Ctrl+C copy everything as shown")
{
    Scene scene;

    scene.show(reference);
    platform::setClipboardText("unchanged");

    MarkdownText *intro=scene.block<MarkdownText>(0);

    scene.click(caretPoint(intro, 0, 2));
    REQUIRE(KeyboardFocus == intro);
    scene.key(GLFW_KEY_A, GLFW_MOD_CONTROL);
    CHECK(scene.document->hasSelection());
    CHECK(scene.document->getSelectedText() == everything);
    scene.key(GLFW_KEY_C, GLFW_MOD_CONTROL);
    CHECK(platform::getClipboardText() == everything);

    // every text draws its part: all of it
    for(const MarkdownDocument::Selectable &selectable:scene.document->getSelectables())
    {
        INFO(selectable.text->getText());
        CHECK((selectable.text->getText().empty() || selectable.text->hasSelection()));
    }

    // Escape clears it
    scene.key(GLFW_KEY_ESCAPE);
    CHECK(!scene.document->hasSelection());
    for(const MarkdownDocument::Selectable &selectable:scene.document->getSelectables())
        CHECK(!selectable.text->hasSelection());
}

TEST_CASE("Markdown selection: a drag across a paragraph, a list, a code block and a table")
{
    Scene scene;

    scene.show(reference);

    MarkdownText *intro=scene.block<MarkdownText>(0);
    MarkdownTable *table=scene.block<MarkdownTable>(3);
    MarkdownText *alpha=table->getCell(1, 0);
    MarkdownText *closing=scene.block<MarkdownText>(4);

    // "paragraph here." from "Intro |paragraph" to "al|pha"
    scene.clock+=1.0;
    scene.move(caretPoint(intro, 0, 6));
    scene.press(true);
    scene.move(Point(caretPoint(intro, 0, 6).x, caretPoint(intro, 0, 6).y+40));
    scene.move(caretPoint(alpha, 0, 2));
    scene.press(false);

    CHECK(scene.document->getSelectedText() ==
        "paragraph here.\n"
        "\xE2\x80\xA2 first item\n"
        "\xE2\x80\xA2 second item\n"
        "code line one\ncode line two\n"
        "Name\tValue\n"
        "al");
    CHECK(intro->hasSelection());
    CHECK(alpha->hasSelection());
    CHECK(!table->getCell(1, 1)->hasSelection());
    CHECK(!closing->hasSelection());

    platform::setClipboardText("unchanged");
    scene.key(GLFW_KEY_C, GLFW_MOD_CONTROL);
    CHECK(platform::getClipboardText() == scene.document->getSelectedText());

    // a drag into the gap above the closing paragraph: its start
    scene.clock+=1.0;
    scene.move(caretPoint(intro, 0, 0));
    scene.press(true);
    const Rect gap=closing->getCanvasBounds();
    scene.move(Point(gap.x+20, gap.y-2));
    scene.press(false);
    CHECK(scene.document->selectionModel().focus() == text::SelectionPosition({ 4 }, 0));
    CHECK(!closing->hasSelection());
    CHECK(table->getCell(2, 0)->hasSelection());
}

TEST_CASE("Markdown selection: a drag step re-marks only the texts whose part changed")
{
    Scene scene;

    scene.show(reference);

    MarkdownText *intro=scene.block<MarkdownText>(0);
    MarkdownText *closing=scene.block<MarkdownText>(4);
    MarkdownText *code=scene.block<MarkdownCodeBlock>(2)->getText();

    scene.clock+=1.0;
    scene.move(caretPoint(intro, 0, 1));
    scene.press(true);
    scene.move(caretPoint(intro, 0, 4));

    // within the paragraph: the paragraph alone
    scene.canvas->inputMouseMoved(caretPoint(intro, 0, 8).x, caretPoint(intro, 0, 8).y, 0, 0);
    CHECK((intro->getStateChange()&controls::StateChange_Background) != 0);
    CHECK((code->getStateChange()&controls::StateChange_Background) == 0);
    CHECK((closing->getStateChange()&controls::StateChange_Background) == 0);
    scene.frame();

    // on into the code block: the paragraph's part stays, every text from the
    // list to the code is marked, the closing paragraph is not
    scene.canvas->inputMouseMoved(caretPoint(code, 0, 3).x, caretPoint(code, 0, 3).y, 0, 0);
    CHECK((intro->getStateChange()&controls::StateChange_Background) != 0);
    CHECK((code->getStateChange()&controls::StateChange_Background) != 0);
    CHECK((closing->getStateChange()&controls::StateChange_Background) == 0);
    scene.frame();
    scene.press(false);
}

TEST_CASE("Markdown selection: a double-click selects a word")
{
    Scene scene;

    scene.show(reference);

    MarkdownText *closing=scene.block<MarkdownText>(4);

    scene.doubleClick(caretPoint(closing, 0, 2));
    CHECK(scene.document->getSelectedText() == "Closing");
}

TEST_CASE("Markdown selection: kept across a setMarkdown of other blocks, cleared when its block goes")
{
    Scene scene;

    scene.show(reference);

    MarkdownText *intro=scene.block<MarkdownText>(0);
    MarkdownText *closing=scene.block<MarkdownText>(4);

    // the intro's "paragraph" and the list
    scene.document->select(text::SelectionPosition({ 0 }, 6), text::SelectionPosition({ 1, 1, 1 }, 6));
    const String selected=scene.document->getSelectedText();

    CHECK(selected == "paragraph here.\n\xE2\x80\xA2 first item\n\xE2\x80\xA2 second");

    // the closing paragraph edited
    std::string edited=reference;

    edited.replace(edited.find("Closing paragraph."), 18, "Closing, edited.");
    scene.show(edited);
    CHECK(closing->getText() == "Closing, edited.");
    CHECK(scene.document->getSelectedText() == selected);

    // a block inserted above it: the ends follow their texts
    scene.show("# New heading\n\n"+edited);
    CHECK(scene.document->getBlockControl(1) == intro);
    CHECK(scene.document->selectionModel().anchor() == text::SelectionPosition({ 1 }, 6));
    CHECK(scene.document->selectionModel().focus() == text::SelectionPosition({ 2, 1, 1 }, 6));
    CHECK(scene.document->getSelectedText() == selected);
    CHECK(intro->hasSelection());

    // the intro removed: the selection is cleared
    edited.replace(0, std::string("Intro paragraph here.\n\n").size(), "");
    scene.show("# New heading\n\n"+edited);
    CHECK(!scene.document->hasSelection());
}
