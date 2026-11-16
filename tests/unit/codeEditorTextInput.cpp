/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A CodeEditor asks for the platform's text input (platform::
 *  setTextInputRect), as a TextBox does, so a phone opens its keyboard over
 *  it in a browser page: active as it takes the keyboard focus, at its caret
 *  in device px at the canvas's scale, following the caret as it moves, and
 *  let go as the focus leaves, as it turns read-only while focused and as
 *  it is deleted with the focus. A read-only editor (a CodeView's) never
 *  asks for it, focused or not, and asks again when it takes edits again
 *  while focused.
 */
#include "headlessFixture.h"

#include <gweni/controls/codeEditor.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <doctest/doctest.h>

using namespace gweni;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::CodeEditor *editor=nullptr;
    bool active=false;
    Rect rect;
    int calls=0;

    Scene()
    {
        platform::setTextInputHandler([this](bool on, const Rect &caret)
            {
                active=on;
                rect=caret;
                ++calls;
            });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(600, 400);

        editor=canvas->newChild<controls::CodeEditor>();
        editor->setPos(20, 10);
        editor->setSize(300, 200);
        editor->setText("first line\nsecond line\nthird line");
        frame();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTextInputHandler(nullptr);
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }
};

}//namespace

TEST_CASE("CodeEditor text input: asked for at the caret while focused, let go as the focus leaves")
{
    Scene scene;
    controls::CodeEditor *editor=scene.editor;

    CHECK(scene.calls == 0);
    editor->focus();
    CHECK(scene.active);
    const Rect caret=editor->getCaretBounds();
    const Point origin=editor->localPosToCanvas(Point(caret.x, caret.y));
    CHECK(scene.rect.x == origin.x);
    CHECK(scene.rect.y == origin.y);
    CHECK(scene.rect.h == caret.h);

    // the caret's move to the next line moves it a line down
    const int top=scene.rect.y;
    editor->setCaret(text::Position{1, 3});
    scene.frame();
    CHECK(scene.active);
    CHECK(scene.rect.y == top+caret.h);

    editor->unfocus();
    CHECK(!scene.active);
}

TEST_CASE("CodeEditor text input: at the canvas's scale, in device px")
{
    Scene scene;

    scene.canvas->setScale(2.0f);
    scene.frame();
    scene.editor->focus();
    REQUIRE(scene.active);
    const Rect caret=scene.editor->getCaretBounds();
    const Point origin=scene.editor->localPosToCanvas(Point(caret.x, caret.y));
    CHECK(scene.rect.x == origin.x*2);
    CHECK(scene.rect.y == origin.y*2);
    CHECK(scene.rect.h == caret.h*2);
}

TEST_CASE("CodeEditor text input: read-only takes none, and gives it back when it turns read-only")
{
    Scene scene;
    controls::CodeEditor *editor=scene.editor;

    editor->setReadOnly(true);
    editor->focus();
    CHECK(scene.calls == 0);

    editor->setReadOnly(false);
    CHECK(scene.active);
    editor->setReadOnly(true);
    CHECK(!scene.active);
}

TEST_CASE("CodeEditor text input: deleted with the focus, it gives it back")
{
    Scene scene;

    scene.editor->focus();
    REQUIRE(scene.active);
    delete scene.editor;
    scene.editor=nullptr;
    CHECK(!scene.active);
}
