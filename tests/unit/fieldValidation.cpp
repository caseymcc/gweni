/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The field validation state of TextBox, PasswordTextBox, TextBoxNumeric and
 *  NumericUpDown: setError/clearError with onErrorChanged once per change of
 *  the error text, the helper text shown while there is no error and given
 *  back after it, and the message line under the field part.
 *
 *  The line's room: reserved by setHelperText or setReserveMessageLine(true),
 *  a set, change or clear then lays out the box alone (its subtree, and the
 *  canvas above it, LayoutStats::visited) and keeps its height; on a box with
 *  none of those the first setError grows it by the room once, and it keeps
 *  the room after. The field part (getFieldHeight, the frame the textured
 *  renderer draws, the text, a NumericUpDown's buttons) stays the height the
 *  box was asked for, above the line. The message is not content (not
 *  getText(), a press on it leaves the caret and the selection), and a
 *  PasswordTextBox shows it in clear text.
 */
#include "headlessFixture.h"

#include <gweni/controls/textBox.h>
#include <gweni/controls/numericUpDown.h>
#include <gweni/inputHandler.h>

#include <doctest/doctest.h>

#include <string>

using namespace gweni;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
    }

    ~Scene()
    {
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
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

    template<typename T>
    T *field(const char *name, int y)
    {
        T *box=canvas->newChild<T>(name);
        box->setPos(10, y);
        return box;
    }

    // a press and a release at a point in canvas coordinates
    void click(int x, int y)
    {
        canvas->inputMouseMoved(x, y, 0, 0);
        canvas->inputMouseButton(0, true);
        canvas->inputMouseButton(0, false);
        frame();
    }
};

// The four fields, each checked by the same cases.
template<typename T>
void checkSetAndClear()
{
    Scene scene;
    T *box=scene.template field<T>("box", 10);
    int changes=0;

    struct Counter: event::Handler
    {
        int *count;
        void changed(event::Info) { ++*count; }
    } counter;
    counter.count=&changes;
    box->onErrorChanged.add(&counter, &Counter::changed);
    scene.settle();

    CHECK_FALSE(box->hasError());
    CHECK(box->getMessageText() == nullptr);
    // clearing a box with no error makes nothing
    box->clearError();
    CHECK(box->getMessageText() == nullptr);
    CHECK(changes == 0);

    box->setError("bad value");
    scene.frame();
    CHECK(box->hasError());
    CHECK(box->getError() == "bad value");
    CHECK(box->getMessage() == "bad value");
    REQUIRE(box->getMessageText() != nullptr);
    CHECK(box->getMessageText()->getText() == "bad value");
    CHECK(changes == 1);

    // the same error again is no change
    box->setError("bad value");
    CHECK(changes == 1);
    box->setError("worse");
    CHECK(changes == 2);
    box->setError("");
    scene.frame();
    CHECK_FALSE(box->hasError());
    CHECK(box->getMessageText()->getText().empty());
    CHECK(changes == 3);
    box->clearError();
    CHECK(changes == 3);
}

template<typename T>
void checkHelperText()
{
    Scene scene;
    T *box=scene.template field<T>("box", 10);

    box->setHelperText("a host name");
    scene.settle();
    CHECK(box->getMessage() == "a host name");
    CHECK(box->getMessageText()->getText() == "a host name");

    box->setError("required");
    scene.frame();
    CHECK(box->getMessageText()->getText() == "required");
    CHECK(box->getHelperText() == "a host name");

    box->clearError();
    scene.frame();
    CHECK(box->getMessageText()->getText() == "a host name");
}

// With the room reserved, a set, a change and a clear lay out the box alone
// at a constant height: the canvas's and the box's four entries, and the
// message Text's.
template<typename T>
void checkReservedLine()
{
    Scene scene;
    T *box=scene.template field<T>("box", 10);
    T *below=scene.template field<T>("below", 60);
    const int asked=box->getHeight();

    box->setReserveMessageLine(true);
    scene.settle();
    CHECK(box->reservesMessageLine());
    const int height=box->getHeight();
    CHECK(height > asked);
    CHECK(box->getFieldHeight() == asked);
    CHECK(box->getMessageRoom() == height-asked);

    for(const char *error:{"required", "too long, far too long for this", ""})
    {
        CAPTURE(error);
        box->setError(error);
        scene.frame();
        const controls::LayoutStats &stats=scene.canvas->getLayoutStats();
        CHECK(stats.passes == 1);
        // the canvas, the box and its message Text, four entries each
        CHECK(stats.visited <= 3*4);
        CHECK(box->getHeight() == height);
        CHECK(below->getHeight() == asked);
        scene.frame();
        CHECK(scene.canvas->getLayoutStats().passes == 0);
    }
}

template<typename T>
void checkFirstErrorGrows()
{
    Scene scene;
    T *box=scene.template field<T>("box", 10);
    scene.settle();
    const int asked=box->getHeight();

    CHECK(box->getMessageRoom() == 0);
    CHECK_FALSE(box->reservesMessageLine());

    box->setError("required");
    scene.frame();
    const int grown=box->getHeight();
    CHECK(grown > asked);
    CHECK(box->getFieldHeight() == asked);

    // it keeps the room from then on
    box->clearError();
    scene.frame();
    CHECK(box->getHeight() == grown);
    box->setError("again");
    scene.frame();
    CHECK(box->getHeight() == grown);
    CHECK(scene.canvas->getLayoutStats().visited <= 3*4);
}

// The message is not content: it is not in getText(), and a press on its line
// moves no caret and takes no selection away, a double-click selects nothing.
template<typename T>
void checkNotContent()
{
    Scene scene;
    T *box=scene.template field<T>("box", 10);
    box->setText("12345");
    box->setError("bad");
    scene.settle();
    CHECK(box->getText() == "12345");

    box->focus();
    box->setCursorPos(1);
    box->setCursorend(3);
    scene.frame();

    const Rect bounds=box->getBounds();
    const int lineY=bounds.y+box->getFieldHeight()+box->getMessageRoom()/2;

    scene.click(bounds.x+bounds.w-4, lineY);
    CHECK(box->getCursorPos() == 1);
    CHECK(box->getCursorEnd() == 3);
    scene.canvas->inputMouseButton(0, true);
    scene.canvas->inputMouseButton(0, false);
    scene.canvas->inputMouseButton(0, true);
    scene.canvas->inputMouseButton(0, false);
    scene.frame();
    CHECK(box->getCursorPos() == 1);
    CHECK(box->getCursorEnd() == 3);
    CHECK(box->getText() == "12345");

    // a press in the field part still places the caret
    scene.click(bounds.x+bounds.w-4, bounds.y+box->getFieldHeight()/2);
    CHECK(box->getCursorPos() == 5);
}

} // namespace

TEST_CASE_TEMPLATE("Field validation: set and clear, onErrorChanged once per change", T,
    controls::TextBox, controls::PasswordTextBox, controls::TextBoxNumeric, controls::NumericUpDown)
{
    checkSetAndClear<T>();
}

TEST_CASE_TEMPLATE("Field validation: the helper text while there is no error", T,
    controls::TextBox, controls::PasswordTextBox, controls::TextBoxNumeric, controls::NumericUpDown)
{
    checkHelperText<T>();
}

TEST_CASE_TEMPLATE("Field validation: a reserved line lays out the field alone", T,
    controls::TextBox, controls::PasswordTextBox, controls::TextBoxNumeric, controls::NumericUpDown)
{
    checkReservedLine<T>();
}

TEST_CASE_TEMPLATE("Field validation: the first error grows an unreserved field once", T,
    controls::TextBox, controls::PasswordTextBox, controls::TextBoxNumeric, controls::NumericUpDown)
{
    checkFirstErrorGrows<T>();
}

TEST_CASE_TEMPLATE("Field validation: the message is not content", T,
    controls::TextBox, controls::PasswordTextBox, controls::TextBoxNumeric, controls::NumericUpDown)
{
    checkNotContent<T>();
}

TEST_CASE("Field validation: a NumericUpDown's buttons stay in the field part")
{
    Scene scene;
    controls::NumericUpDown *box=scene.field<controls::NumericUpDown>("box", 10);
    scene.settle();
    std::vector<Rect> before;

    for(controls::Base *child:box->getChildren())
        before.push_back(child->getBounds());

    box->setHelperText("0 to 100");
    scene.settle();
    CHECK(box->getFieldHeight() == 20);

    // the splitter docked Right: where it was, inside the field part
    size_t i=0;
    for(controls::Base *child:box->getChildren())
    {
        if(i < before.size())
        {
            CAPTURE(child->getTypeName());
            CHECK((child->getBounds() == before[i]));
            CHECK(child->getBounds().y+child->getBounds().h <= box->getFieldHeight());
        }
        ++i;
    }
}

TEST_CASE("Field validation: a PasswordTextBox shows its message unmasked")
{
    Scene scene;
    controls::PasswordTextBox *box=scene.field<controls::PasswordTextBox>("box", 10);
    box->setText("secret");
    box->setError("at least 8 characters");
    scene.settle();
    CHECK(box->getMessageText()->getText() == "at least 8 characters");
    CHECK(box->getText() == "secret");
}
