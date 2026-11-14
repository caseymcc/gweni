/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  CodeView's copy button, on the textured skin over a StubRenderer (every
 *  draw and hide a write), with a stepped clock and an
 *  in-test clipboard (platform::setClipboardBackend). A view has none until
 *  setCopyButton(true), which makes it hidden: the view's own child, after
 *  its rows (a higher z than every row's), CopyInset in from the viewport's
 *  top-right corner, the same there however far the content is scrolled.
 *  It shows while the mouse is over the view or anything in it (the button,
 *  a bar) and hides when the mouse leaves it, each of those frames writing
 *  the button's own slots and nothing else, and it stays shown while it has
 *  the keyboard focus (Tab from the view while hovered), hiding once the
 *  focus leaves it with the mouse gone. With the mouse away, Tab from the
 *  focused view shows the button and focuses it (writing its slots and the
 *  view's own focus look, no row's), Space copies, Shift+Tab goes back to the
 *  view and hides it, and Tab twice goes on to the next control; a click leaves the focus on the view
 *  (focused by Tab or not), so the button hides once the mouse leaves after
 *  it. On a one-line view narrower than its line it sits inside the view and
 *  above the horizontal bar. A click puts the whole text on the
 *  clipboard, or the selection when there is one, and the button shows
 *  "Copied" for CopiedSeconds on the canvas clock, then "Copy" again; a
 *  second click meanwhile starts the time again. setCopyButton(false) hides
 *  it for good, hovered or not, and gives a focus it held back to the view.
 *  Its showing, hiding or resizing under a mouse at rest hit-tests the hover
 *  again: one move straight onto it and a press with no move copies (and
 *  keeps the selection), and Copied shrinking back to Copy under the mouse
 *  gives the hover back to what is under it.
 */
#include "headlessFixture.h"

#include <gweni/controls/codeView.h>
#include <gweni/controls/textBox.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/input/glfw3.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

using namespace gweni;
using controls::CodeView;

namespace
{

String numberedLines(size_t count)
{
    String text;

    for(size_t i=0; i < count; ++i)
    {
        if(i > 0)
            text+='\n';
        text+="line "+std::to_string(i);
    }
    return text;
}

struct Scene
{
    test::StubRenderer renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    CodeView *view=nullptr;
    input::GLFW3 glfw;
    double clock=100.0;
    String clipboard;
    int clipboardWrites=0;

    explicit Scene(const String &text, int width=0, int height=0)
    {
        platform::setTimeSource([this]() { return clock; });
        platform::setClipboardBackend([this]() { return clipboard; },
            [this](const String &copied) { clipboard=copied; ++clipboardWrites; return true; });

        canvas=new controls::Canvas(&skin);
        canvas->setSize(800, 600);
        glfw.initialize(canvas);

        view=canvas->newChild<CodeView>();
        view->setPos(20, 20);
        if(width > 0)
            view->setSize(width, height);
        view->setText(text);
        view->setCopyButton(true);
        // the mouse an earlier case left is off the view
        canvas->inputMouseMoved(790, 590, 0, 0);
        settle();
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
        platform::setClipboardBackend(nullptr, nullptr, nullptr);
        platform::setTimeSource(nullptr);
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void settle()
    {
        for(int i=0; i < 3; ++i)
            frame();
    }

    //! The clock \p seconds on, and a frame.
    void wait(double seconds)
    {
        clock+=seconds;
        frame();
    }

    void moveCanvas(int x, int y)
    {
        canvas->inputMouseMoved(x, y, 0, 0);
        frame();
    }

    //! The mouse to a point in \p control's coordinates.
    void move(controls::Base *control, int x, int y)
    {
        const Point at=control->localPosToCanvas(Point(x, y));

        moveCanvas(at.x, at.y);
    }

    void moveOff() { moveCanvas(790, 590); }

    void clickButton()
    {
        controls::Base *button=view->getCopyButton();

        // onto the view (which shows the button), then onto the button
        move(view, 2, view->getHeight()-2);
        move(button, button->getWidth()/2, button->getHeight()/2);
        canvas->inputMouseButton(0, true);
        frame();
        canvas->inputMouseButton(0, false);
        frame();
    }

    void key(int key, int mods=0)
    {
        glfw.processKeyEvent(input::GLFW3::KeyEvent{key, 0, GLFW_PRESS, mods});
        glfw.processKeyEvent(input::GLFW3::KeyEvent{key, 0, GLFW_RELEASE, mods});
        frame();
    }

    //! The primitive ids of \p control and its subtree.
    static void subtreeIds(controls::Base *control, std::vector<size_t> &ids)
    {
        std::vector<size_t> texts;

        if(control->getSkinControl())
            control->getSkinControl()->appendIds(ids, texts);
        for(controls::Base *child : control->getChildren())
            subtreeIds(child, ids);
    }

    //! Every primitive slot the last frame wrote is the button's or its
    //! text's, and it wrote at least one.
    void checkWroteButtonAlone()
    {
        std::vector<size_t> button;

        subtreeIds(view->getCopyButton(), button);

        const std::vector<size_t> &written=renderer.writtenPrimitiveIds();

        CHECK(!written.empty());
        for(size_t id : written)
        {
            CAPTURE(id);
            CHECK(std::find(button.begin(), button.end(), id) != button.end());
        }
        for(const renderer::WrittenText &text : renderer.writtenTextIds())
        {
            std::vector<size_t> primitives, texts;

            for(controls::Base *child : view->getCopyButton()->getChildren())
                child->getSkinControl()->appendIds(primitives, texts);
            CAPTURE(text.textId);
            CHECK(std::find(texts.begin(), texts.end(), text.textId) != texts.end());
        }
    }

    //! The button's rect in the view's coordinates.
    Rect buttonRect() const
    {
        return view->getCopyButton()->getBounds();
    }
};

}//namespace

TEST_CASE("CodeView copy button: none by default, then the view's child over the viewport's top-right, hidden until hovered")
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas canvas(&skin);
    CodeView *plain=canvas.newChild<CodeView>();

    CHECK(!plain->hasCopyButton());
    CHECK(plain->getCopyButton() == nullptr);

    Scene scene("echo hello\nnpm install gweni");
    controls::Base *button=scene.view->getCopyButton();

    REQUIRE(button);
    CHECK(scene.view->hasCopyButton());
    CHECK(button->getParent() == scene.view);
    CHECK(button->hidden());
    CHECK(button->getZIndex() > 0);

    scene.move(scene.view, 5, 5);
    CHECK(button->visible());

    const Rect &viewport=scene.view->getViewport();
    const Rect rect=scene.buttonRect();

    CHECK(rect.x+rect.w == viewport.x+viewport.w-CodeView::CopyInset);
    CHECK(rect.y == viewport.y+CodeView::CopyInset);
    CHECK(static_cast<controls::Label *>(button)->getText() == CodeView::CopyLabel);

    // numbered above every row
    for(controls::Base *row : scene.view->getRows())
        CHECK(button->getZIndex() > row->getZIndex());
}

TEST_CASE("CodeView copy button: shown while the view or anything in it is hovered, each frame writing its slots alone")
{
    Scene scene(numberedLines(3));
    controls::Base *button=scene.view->getCopyButton();

    // onto the view: shown, its slots alone
    scene.move(scene.view, 5, scene.view->getHeight()-5);
    CHECK(button->visible());
    scene.checkWroteButtonAlone();

    // a settled frame writes nothing
    scene.frame();
    CHECK(scene.renderer.writtenPrimitiveIds().empty());

    // onto the button itself: still shown, nothing written
    scene.move(button, 2, 2);
    CHECK(button->visible());
    CHECK(HoveredControl == button);

    // off: hidden, its slots alone
    scene.moveOff();
    CHECK(button->hidden());
    scene.checkWroteButtonAlone();
    scene.frame();
    CHECK(scene.renderer.writtenPrimitiveIds().empty());

    // turned off: hidden while hovered
    scene.move(scene.view, 5, 5);
    REQUIRE(button->visible());
    scene.view->setCopyButton(false);
    scene.frame();
    CHECK(!scene.view->hasCopyButton());
    CHECK(button->hidden());
    scene.moveOff();
    scene.move(scene.view, 5, 5);
    CHECK(button->hidden());
}

TEST_CASE("CodeView copy button: stays put while the content scrolls, and shows over a bar")
{
    Scene scene(numberedLines(1000), 300, 200);
    controls::Base *button=scene.view->getCopyButton();

    scene.move(scene.view, 5, 5);
    REQUIRE(button->visible());

    const Rect before=scene.buttonRect();

    scene.view->setVerticalScrollOffset(40*scene.view->getLineHeight());
    scene.frame();
    REQUIRE(scene.view->getVerticalScrollOffset() > 0);

    const Rect after=scene.buttonRect();

    CHECK(after.x == before.x);
    CHECK(after.y == before.y);
    CHECK(button->visible());

    // the vertical bar, right of the viewport, is in the view
    const Rect &viewport=scene.view->getViewport();

    scene.move(scene.view, viewport.x+viewport.w+3, viewport.y+viewport.h/2);
    CHECK(HoveredControl != scene.view);
    CHECK(button->visible());
}

TEST_CASE("CodeView copy button: shown while it has the focus")
{
    Scene scene(numberedLines(3));
    controls::Base *button=scene.view->getCopyButton();

    scene.view->focus();
    scene.move(scene.view, 5, 5);
    REQUIRE(button->visible());

    scene.key(GLFW_KEY_TAB);
    REQUIRE(KeyboardFocus == button);

    scene.moveOff();
    CHECK(button->visible());

    // the focus moved back to the view, the mouse gone: hidden
    scene.view->focus();
    scene.frame();
    CHECK(button->hidden());
}

TEST_CASE("CodeView copy button: Tab from the focused view reaches it with the mouse away")
{
    Scene scene(numberedLines(3));
    controls::TextBox *after=scene.canvas->newChild<controls::TextBox>();
    controls::Base *button=scene.view->getCopyButton();

    after->setPos(20, 300);
    scene.settle();
    scene.view->focus();
    scene.frame();
    REQUIRE(button->hidden());

    // Tab shows it and gives it the focus, writing its slots and the view's
    // own (its frame loses the focus look), no row's
    scene.key(GLFW_KEY_TAB);
    CHECK(KeyboardFocus == button);
    CHECK(button->visible());

    std::vector<size_t> allowed, texts;

    Scene::subtreeIds(button, allowed);
    scene.view->getSkinControl()->appendIds(allowed, texts);
    CHECK(!scene.renderer.writtenPrimitiveIds().empty());
    for(size_t id : scene.renderer.writtenPrimitiveIds())
    {
        CAPTURE(id);
        CHECK(std::find(allowed.begin(), allowed.end(), id) != allowed.end());
    }

    // Space presses it
    scene.key(GLFW_KEY_SPACE);
    CHECK(scene.clipboard == numberedLines(3));

    // Shift+Tab back to the view hides it again
    scene.glfw.processKeyEvent(input::GLFW3::KeyEvent{GLFW_KEY_LEFT_SHIFT, 0, GLFW_PRESS, GLFW_MOD_SHIFT});
    scene.key(GLFW_KEY_TAB, GLFW_MOD_SHIFT);
    scene.glfw.processKeyEvent(input::GLFW3::KeyEvent{GLFW_KEY_LEFT_SHIFT, 0, GLFW_RELEASE, 0});
    scene.frame();
    CHECK(KeyboardFocus == scene.view);
    CHECK(button->hidden());

    // and Tab twice goes on past it to the next control
    scene.key(GLFW_KEY_TAB);
    REQUIRE(KeyboardFocus == button);
    scene.key(GLFW_KEY_TAB);
    CHECK(KeyboardFocus == after);
    CHECK(button->hidden());
}

TEST_CASE("CodeView copy button: a click copies the whole text, or the selection, and shows Copied")
{
    const String text="pip install gweni\n# then\ngweni --demo";
    Scene scene(text);
    auto *button=static_cast<controls::Label *>(scene.view->getCopyButton());

    scene.clickButton();
    CHECK(scene.clipboard == text);
    CHECK(scene.clipboardWrites == 1);
    CHECK(scene.view->isCopied());
    CHECK(button->getText() == CodeView::CopiedLabel);

    // "install gweni" on the first line selected
    scene.view->setCaret(text::Position{0, 4});
    scene.view->setCaret(text::Position{0, 17}, true);
    scene.frame();
    REQUIRE(scene.view->getSelectedText() == "install gweni");

    scene.clickButton();
    CHECK(scene.clipboard == "install gweni");
    CHECK(scene.clipboardWrites == 2);
}

TEST_CASE("CodeView copy button: Copied ends CopiedSeconds on the canvas clock, a second click starting it again")
{
    Scene scene("curl -fsSL https://example.com/install.sh | sh");
    auto *button=static_cast<controls::Label *>(scene.view->getCopyButton());

    scene.clickButton();
    REQUIRE(scene.view->isCopied());

    scene.wait(CodeView::CopiedSeconds-0.25);
    CHECK(scene.view->isCopied());
    CHECK(button->getText() == CodeView::CopiedLabel);

    scene.wait(0.5);
    CHECK(!scene.view->isCopied());
    CHECK(button->getText() == CodeView::CopyLabel);

    // a second click within the time: Copied for CopiedSeconds from it
    scene.clickButton();
    scene.wait(CodeView::CopiedSeconds-0.5);
    scene.clickButton();
    scene.wait(0.75);
    CHECK(scene.view->isCopied());
    scene.wait(CodeView::CopiedSeconds);
    CHECK(!scene.view->isCopied());
    CHECK(button->getText() == CodeView::CopyLabel);

    // a frozen clock never ends it
    scene.clickButton();
    for(int i=0; i < 10; ++i)
        scene.frame();
    CHECK(scene.view->isCopied());
}

TEST_CASE("CodeView copy button: a click leaves the focus on the view, and moving off hides it")
{
    Scene scene(numberedLines(3));
    controls::Base *button=scene.view->getCopyButton();

    scene.clickButton();
    REQUIRE(scene.view->isCopied());
    CHECK(KeyboardFocus == scene.view);
    CHECK(!button->isFocussed());

    // off: hidden, its slots alone, then nothing
    scene.moveOff();
    CHECK(button->hidden());
    scene.checkWroteButtonAlone();
    scene.frame();
    CHECK(scene.renderer.writtenPrimitiveIds().empty());

    // focused by Tab, then clicked: the focus goes back to the view too
    scene.move(scene.view, 5, 5);
    scene.key(GLFW_KEY_TAB);
    REQUIRE(KeyboardFocus == button);
    scene.clickButton();
    CHECK(KeyboardFocus == scene.view);
    scene.moveOff();
    CHECK(button->hidden());
}

TEST_CASE("CodeView copy button: on a one-line view narrower than its line, inside the view above the horizontal bar")
{
    Scene scene("./run.sh -b controltest --control CodeViewDemo");
    controls::Base *panel=scene.canvas->newChild<controls::Base>();
    controls::Base *button=scene.view->getCopyButton();

    panel->setBounds(20, 20, 90, 200);
    panel->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    scene.view->setParent(panel);
    scene.view->setDock(DockPosition::Top);
    scene.settle();
    REQUIRE(scene.view->getMaxHorizontalScrollOffset() > 0);

    controls::Base *bar=nullptr;

    for(controls::Base *child : scene.view->getActualChildren())
    {
        if(dynamic_cast<controls::HorizontalScrollBar *>(child))
            bar=child;
    }
    REQUIRE(bar);
    REQUIRE(bar->visible());

    scene.move(scene.view, 5, 3);
    REQUIRE(button->visible());

    const Rect rect=scene.buttonRect();

    const Rect barRect=bar->getBounds();

    CAPTURE(rect.x);
    CAPTURE(rect.y);
    CAPTURE(rect.w);
    CAPTURE(rect.h);
    CAPTURE(barRect.y);
    CHECK(rect.x >= 0);
    CHECK(rect.x+rect.w <= scene.view->getWidth());
    CHECK(rect.y >= 0);
    CHECK(rect.y+rect.h <= barRect.y);

    // narrower than the button: at the view's left edge, not left of it
    panel->setWidth(20);
    scene.settle();
    scene.move(scene.view, 2, 3);
    REQUIRE(button->visible());
    REQUIRE(scene.view->getWidth() == 20);
    CHECK(scene.buttonRect().x == 0);
}

TEST_CASE("CodeView copy button: one move straight onto it and a press with no move copies, keeping the selection")
{
    const String text="pip install gweni\n# then\ngweni --demo";
    Scene scene(text);
    controls::Base *button=scene.view->getCopyButton();

    // where it shows, from a hover elsewhere in the view
    scene.move(scene.view, 2, scene.view->getHeight()-2);
    REQUIRE(button->visible());

    const Rect rect=button->getBounds();
    const Point centre=scene.view->localPosToCanvas(Point(rect.x+rect.w/2, rect.y+rect.h/2));

    scene.moveOff();
    REQUIRE(button->hidden());

    scene.view->setCaret(text::Position{0, 4});
    scene.view->setCaret(text::Position{0, 17}, true);
    scene.frame();
    REQUIRE(scene.view->getSelectedText() == "install gweni");

    // one move from off the view onto where the button shows: the row under
    // it is hovered first, the button once it has shown
    scene.moveCanvas(centre.x, centre.y);
    scene.frame();
    REQUIRE(button->visible());
    CHECK(HoveredControl == button);

    // the press and release with no move between
    scene.canvas->inputMouseButton(0, true);
    scene.frame();
    scene.canvas->inputMouseButton(0, false);
    scene.frame();
    CHECK(scene.clipboardWrites == 1);
    CHECK(scene.clipboard == "install gweni");
    CHECK(scene.view->getSelectedText() == "install gweni");
}

TEST_CASE("CodeView copy button: Copied shrinking back to Copy under a resting mouse gives the hover back to the row")
{
    Scene scene(numberedLines(3));
    controls::Base *button=scene.view->getCopyButton();

    scene.clickButton();
    REQUIRE(scene.view->isCopied());
    scene.frame();

    // the mouse inside Copied's rect but left of where Copy's will be (the
    // button is right-aligned, and Copied is the wider)
    const Rect copied=button->getBounds();

    scene.move(button, 1, copied.h/2);
    REQUIRE(HoveredControl == button);

    scene.wait(CodeView::CopiedSeconds+0.25);
    scene.frame();
    REQUIRE(!scene.view->isCopied());

    const Rect copy=button->getBounds();

    REQUIRE(copy.x > copied.x);
    CHECK(HoveredControl != button);
    CHECK(button->visible());
}

TEST_CASE("CodeView copy button: turned off while it has the focus gives the focus back to the view")
{
    Scene scene(numberedLines(3));
    controls::Base *button=scene.view->getCopyButton();

    scene.view->focus();
    scene.key(GLFW_KEY_TAB);
    REQUIRE(KeyboardFocus == button);

    scene.view->setCopyButton(false);
    scene.frame();
    CHECK(button->hidden());
    CHECK(KeyboardFocus == scene.view);
}
