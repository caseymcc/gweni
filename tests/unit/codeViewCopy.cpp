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
 *  focus leaves it with the mouse gone. A click puts the whole text on the
 *  clipboard, or the selection when there is one, and the button shows
 *  "Copied" for CopiedSeconds on the canvas clock, then "Copy" again; a
 *  second click meanwhile starts the time again. setCopyButton(false) hides
 *  it for good, hovered or not.
 */
#include "headlessFixture.h"

#include <gweni/controls/codeView.h>
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
