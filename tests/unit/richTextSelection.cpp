/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  RichText's selection, through the GLFW translator's keys and the canvas's
 *  mouse, drawn by the textured skin's real RichText renderer on a recording
 *  StubRenderer (richTextFixture.h). It checks:
 *  - a press and a drag select from the press's byte to the mouse's, one
 *    RichTextSelection rect per line at the run's z (getZIndex()+1, behind
 *    its glyphs, over a code background at getZIndex()), each drag frame
 *    drawing the selection's slot alone and no run, a settled frame nothing,
 *    and a press without a drag clearing it, giving the slot back;
 *  - a double-click selects the word under it (a punctuation run apart),
 *    Ctrl+A every byte, Ctrl+C puts the selected text on the clipboard,
 *    Escape clears it, and the press gives the paragraph the keyboard focus;
 *  - wrapped, the selection is a rect per line, a selected line break
 *    reaching breakSelectionWidth() past the line's last piece, an empty
 *    line's rect that width at its left, and the last line's rect ending at
 *    its last glyph;
 *  - a press on a link that drags to another byte selects and is no click;
 *  - two paragraphs on one container's SelectionModel each draw and copy
 *    their block's part of it, and clearing it from one leaves the caret at
 *    the focus in the other's block;
 *  - new spans clear the paragraph's own selection;
 *  - before the new spans are laid out, a selection is clamped to their
 *    text, not the laid-out one: past a shorter text it is empty, inside a
 *    longer one its text, and Ctrl+C copies that.
 */
#include "richTextFixture.h"

#include <gweni/controls/richText.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/input/glfw3.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <vector>

using namespace gweni;
using controls::RichSpan;
using controls::RichText;
using test::DrawRecorder;

namespace
{

struct Scene
{
    DrawRecorder renderer;
    test::RichSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    RichText *rich=nullptr;
    input::GLFW3 glfw;
    double clock=100.0;

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(600, 300);
        glfw.initialize(canvas);
        rich=canvas->newChild<RichText>();
        rich->setSizeFlags({ SizeFlag::Fixed, SizeFlag::Elastic });
        rich->setPos(10, 10);
        rich->setWidth(500);
        rich->setSpans({ RichSpan::plain("Some "), RichSpan::plain("bold", RichSpan::Bold),
            RichSpan::plain(" and "), RichSpan::plain("code()", RichSpan::Code), RichSpan::plain(" a "),
            RichSpan::link("link", "https://example.com/"), RichSpan::plain(" end.") });
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
        renderer.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    //! The canvas point at caret \p caret of piece \p piece, halfway down its line
    Point caretPoint(size_t piece, size_t caret) const
    {
        const text::RichPiece &p=rich->getLayout().pieces().at(piece);

        return rich->localPosToCanvas(Point(rich->contentOrigin().x+p.pos.x+p.carets.at(caret),
            rich->contentOrigin().y+p.pos.y+p.metrics.lineHeight/2));
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

    //! A click a second after the last, so it is never a double-click
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

    void glfwKey(int key, int action, int mods)
    {
        glfw.processKeyEvent(input::GLFW3::KeyEvent{key, 0, action, mods});
    }

    void key(int key, int mods=0)
    {
        if(mods&GLFW_MOD_CONTROL)
            glfwKey(GLFW_KEY_LEFT_CONTROL, GLFW_PRESS, GLFW_MOD_CONTROL);
        glfwKey(key, GLFW_PRESS, mods);
        glfwKey(key, GLFW_RELEASE, mods);
        if(mods&GLFW_MOD_CONTROL)
            glfwKey(GLFW_KEY_LEFT_CONTROL, GLFW_RELEASE, 0);
        frame();
    }

    std::vector<size_t> slotIds() const
    {
        std::vector<size_t> primitives, texts;

        rich->getSkinControl()->appendIds(primitives, texts);
        return primitives;
    }

    //! The slots this frame drew in the selection's surface
    std::vector<DrawRecorder::SlotDraw> selectionDraws() const
    {
        std::vector<DrawRecorder::SlotDraw> draws;

        for(const DrawRecorder::SlotDraw &draw:renderer.slots)
        {
            if(!draw.rule && (draw.color == skin::textured::tokyonight::RichTextSelection.top))
                draws.push_back(draw);
        }
        return draws;
    }
};

}//namespace

TEST_CASE("RichText selection: a drag selects, drawn behind the glyphs in the selection's own slot alone")
{
    Scene scene;
    RichText *rich=scene.rich;
    const std::vector<size_t> before=scene.slotIds();
    const std::vector<text::RichPiece> &pieces=rich->getLayout().pieces();

    REQUIRE(rich->getLayout().lines().size() == 1);
    REQUIRE(pieces.size() == 7);

    // from caret 1 of "Some " to caret 3 of "code()"
    scene.click(scene.caretPoint(0, 1));
    CHECK_FALSE(rich->hasSelection());
    CHECK(rich->isFocussed());

    scene.clock+=1.0;
    scene.press(true);
    CHECK(gweni::MouseFocus == rich);
    scene.move(scene.caretPoint(3, 3));
    CHECK(rich->getSelectedText() == "ome bold and cod");
    CHECK(rich->selectionModel().anchor().offset == 1);
    CHECK(rich->selectionModel().focus().offset == pieces[3].offset+3);

    // one rect, over the line, at the runs' z; no run drawn again
    const std::vector<DrawRecorder::SlotDraw> drawn=scene.selectionDraws();

    REQUIRE(drawn.size() == 1);
    CHECK(scene.renderer.slots.size() == 1);
    CHECK(scene.renderer.texts.empty());
    CHECK(drawn[0].zIndex == rich->getZIndex()+1);

    const Point origin=rich->localPosToCanvas(rich->contentOrigin());
    const text::RichLine &line=rich->getLayout().lines()[0];

    CHECK(drawn[0].rect.x == origin.x+pieces[0].pos.x+pieces[0].carets[1]);
    CHECK(drawn[0].rect.x+drawn[0].rect.w == origin.x+pieces[3].pos.x+pieces[3].carets[3]);
    CHECK(drawn[0].rect.y == origin.y+line.top);
    CHECK(drawn[0].rect.h == line.height);

    const std::vector<size_t> withSelection=scene.slotIds();

    REQUIRE(withSelection.size() == before.size()+1);
    CHECK(std::vector<size_t>(withSelection.begin(), withSelection.end()-1) == before);
    CHECK(drawn[0].id == withSelection.back());

    // a settled frame draws nothing
    scene.frame();
    CHECK(scene.renderer.slots.empty());
    CHECK(scene.renderer.texts.empty());

    // further: the same slot alone, wider
    scene.move(scene.caretPoint(4, 2));
    REQUIRE(scene.renderer.slots.size() == 1);
    CHECK(scene.renderer.slots[0].id == withSelection.back());
    CHECK(rich->getSelectedText() == "ome bold and code() a");

    scene.press(false);
    CHECK(gweni::MouseFocus == nullptr);
    CHECK(rich->getSelectedText() == "ome bold and code() a");

    // a press with no drag clears it, and its slot is given back
    scene.click(scene.caretPoint(1, 2));
    CHECK_FALSE(rich->hasSelection());
    CHECK(rich->selectionRects().empty());
    CHECK(scene.slotIds() == before);
}

TEST_CASE("RichText selection: a double-click selects a word, Ctrl+A everything, Ctrl+C copies, Escape clears")
{
    Scene scene;
    RichText *rich=scene.rich;

    scene.doubleClick(scene.caretPoint(1, 2));
    CHECK(rich->getSelectedText() == "bold");
    CHECK(rich->isFocussed());

    platform::setClipboardText("");
    scene.key(GLFW_KEY_C, GLFW_MOD_CONTROL);
    CHECK(platform::getClipboardText() == "bold");

    // "code()": the word stops at its punctuation
    scene.doubleClick(scene.caretPoint(3, 1));
    CHECK(rich->getSelectedText() == "code");

    scene.key(GLFW_KEY_A, GLFW_MOD_CONTROL);
    CHECK(rich->getSelectedText() == rich->getText());
    scene.key(GLFW_KEY_C, GLFW_MOD_CONTROL);
    CHECK(platform::getClipboardText() == "Some bold and code() a link end.");

    scene.key(GLFW_KEY_ESCAPE);
    CHECK_FALSE(rich->hasSelection());
    CHECK(rich->selectionRects().empty());

    // nothing selected: Ctrl+C leaves the clipboard as it is
    scene.key(GLFW_KEY_C, GLFW_MOD_CONTROL);
    CHECK(platform::getClipboardText() == "Some bold and code() a link end.");
}

TEST_CASE("RichText selection: wrapped, a rect per line, a selected break reaching past the line's end")
{
    Scene scene;
    RichText *rich=scene.rich;

    rich->setSpans({ RichSpan::plain("first words here\n\nthe last line") });
    rich->setWidth(50);
    scene.frame();

    const std::vector<text::RichLine> &lines=rich->getLayout().lines();
    const std::vector<text::RichPiece> &pieces=rich->getLayout().pieces();

    REQUIRE(lines.size() >= 4);
    rich->selectAll();
    scene.frame();

    const std::vector<Rect> rects=rich->selectionRects();

    REQUIRE(rects.size() == lines.size());
    REQUIRE(scene.selectionDraws().size() == lines.size());
    for(size_t i=0; i < lines.size(); ++i)
    {
        const text::RichLine &line=lines[i];

        CHECK(rects[i].y == line.top);
        CHECK(rects[i].h == line.height);
        if(line.pieceCount == 0)
        {
            // the empty line between the two '\n's
            CHECK(rects[i].x == 0);
            CHECK(rects[i].w == RichText::breakSelectionWidth(line));
            continue;
        }

        const Rect last=pieces[line.firstPiece+line.pieceCount-1].box();
        const int breakWidth=(i+1 < lines.size())?RichText::breakSelectionWidth(line):0;

        CHECK(rects[i].x == pieces[line.firstPiece].pos.x);
        CHECK(rects[i].x+rects[i].w == last.x+last.w+breakWidth);
    }

    // the first line alone, its break not selected: no wider than its glyphs
    rich->select(0, pieces[lines[0].firstPiece].text.size());
    scene.frame();
    REQUIRE(rich->selectionRects().size() == 1);
    CHECK(rich->selectionRects()[0].w == pieces[lines[0].firstPiece].width);
}

TEST_CASE("RichText selection: a press on a link dragged to another byte selects and is no click")
{
    Scene scene;
    RichText *rich=scene.rich;
    int clicks=0;

    rich->onLinkClicked.addCallback(scene.canvas, [&](event::Handler &, event::Info) { ++clicks; });

    // a press and a release on one byte of it: the link's click
    scene.click(scene.caretPoint(5, 1));
    CHECK(clicks == 1);
    CHECK_FALSE(rich->hasSelection());

    scene.clock+=1.0;
    scene.press(true);
    scene.move(scene.caretPoint(5, 3));
    scene.press(false);
    CHECK(rich->getSelectedText() == "in");
    CHECK(clicks == 1);
}

TEST_CASE("RichText selection: paragraphs on a container's model draw and copy their own block's part")
{
    Scene scene;
    RichText *first=scene.rich;
    RichText *second=scene.canvas->newChild<RichText>();
    text::SelectionModel model;

    second->setSizeFlags({ SizeFlag::Fixed, SizeFlag::Elastic });
    second->setPos(10, 100);
    second->setWidth(500);
    second->setSpans({ RichSpan::plain("Another paragraph") });
    first->setSelectionModel(&model, { 0 });
    second->setSelectionModel(&model, { 1 });
    scene.frame();

    model.select(text::SelectionPosition({ 0 }, 5), text::SelectionPosition({ 1 }, 7));
    first->selectionChanged();
    second->selectionChanged();
    scene.frame();
    CHECK(first->getSelectedText() == "bold and code() a link end.");
    CHECK(second->getSelectedText() == "Another");
    CHECK(scene.selectionDraws().size() == 2);
    CHECK(&first->selectionModel() == &model);

    // cleared from the first, the caret is the focus, in the second's block
    first->clearSelection();
    CHECK(model.empty());
    CHECK((model.focus() == text::SelectionPosition({ 1 }, 7)));
    model.select(text::SelectionPosition({ 0 }, 5), text::SelectionPosition({ 1 }, 7));
    first->selectionChanged();
    second->selectionChanged();
    scene.frame();

    // its own model back: nothing selected there
    first->setSelectionModel(nullptr);
    scene.frame();
    CHECK_FALSE(first->hasSelection());
    CHECK(first->selectionBlock().empty());
    CHECK(second->getSelectedText() == "Another");
}

TEST_CASE("RichText selection: new spans clear the paragraph's own selection")
{
    Scene scene;
    RichText *rich=scene.rich;

    rich->selectAll();
    scene.frame();
    REQUIRE(rich->hasSelection());
    rich->setSpanText(0, "Other ");
    scene.frame();
    CHECK_FALSE(rich->hasSelection());
    CHECK(rich->selectionRects().empty());
}

TEST_CASE("RichText selection: before new spans are laid out, a selection is clamped to their text")
{
    Scene scene;
    RichText *rich=scene.rich;

    rich->setSpans({ RichSpan::plain("hello world") });
    scene.frame();

    // shorter, not laid out yet: past its end is nothing, in it its bytes
    rich->setSpans({ RichSpan::plain("hi") });
    rich->select(5, 9);
    CHECK_FALSE(rich->hasSelection());
    CHECK_NOTHROW(CHECK(rich->getSelectedText() == ""));
    rich->select(1, 9);
    CHECK(rich->getSelectedText() == "i");
    platform::setClipboardText("unchanged");
    CHECK_NOTHROW(rich->copySelection());
    CHECK(platform::getClipboardText() == "i");
    scene.frame();

    // longer, not laid out yet: its new bytes are selectable
    rich->setSpans({ RichSpan::plain("hello world") });
    rich->select(6, 11);
    CHECK(rich->getSelectedText() == "world");
    scene.frame();
    CHECK(rich->getSelectedText() == "world");
}
