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
 *
 *  The validator: set, it runs nothing and reserves the line; on Change it
 *  runs in each text change, on the text after a TextBoxNumeric's filtering
 *  (a refused letter never reaches it); on FocusLoss typing does not run it
 *  and losing the focus, Tab included, does; Return runs it in either mode,
 *  before onReturnPressedCaller; validate() runs it on demand and says whether
 *  the field is valid; a null one removes it and keeps the error.
 *
 *  The look, through the textured skin's TextBox renderer on a recording
 *  StubRenderer: an error frame is TextBoxError (the theme's red border),
 *  TextBoxErrorFocus while focused and TextBoxErrorDisabled while disabled,
 *  and the plain surfaces come back with the error cleared; the frame is the
 *  field part's height, never over the line; the message is in the skin's
 *  ErrorText colour, the helper text in HelperText, and both follow a theme
 *  edit (ThemeEditor::applyToCanvas).
 */
#include "headlessFixture.h"

#include <gweni/controls/textBox.h>
#include <gweni/controls/numericUpDown.h>
#include <gweni/inputHandler.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/tokyoNight.h>
#include <gweni/tools/themeEditor.h>

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

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

    void type(const char *text)
    {
        for(const char *c=text; *c; ++c)
            canvas->inputCharacter(static_cast<UnicodeChar>(*c));
        frame();
    }

    void key(int k)
    {
        input::onKeyEvent(canvas, k, true);
        input::onKeyEvent(canvas, k, false);
        frame();
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

// Digits work in every field, a TextBoxNumeric's included: two at most.
struct Rule
{
    int calls=0;
    std::vector<String> seen;

    controls::TextBox::Validator make()
    {
        return [this](const String &text) -> String
        {
            ++calls;
            seen.push_back(text);
            if(text.empty())
                return "required";
            return text.size() > 2?"two digits at most":"";
        };
    }
};

template<typename T>
void checkValidateOnChange()
{
    Scene scene;
    T *box=scene.template field<T>("box", 10);
    Rule rule;

    box->setText("");
    box->setValidator(rule.make(), controls::ValidateOn::Change);
    // setting it runs nothing, and reserves the line
    CHECK(rule.calls == 0);
    CHECK(box->reservesMessageLine());
    CHECK_FALSE(box->hasError());
    box->focus();
    scene.settle();

    scene.type("1");
    CHECK_FALSE(box->hasError());
    scene.type("23");
    CHECK(box->getError() == "two digits at most");
    CHECK(box->getMessageText()->getText() == "two digits at most");
    scene.key(keys::Backspace);
    CHECK_FALSE(box->hasError());
    REQUIRE_FALSE(rule.seen.empty());
    CHECK(rule.seen.back() == "12");

    // a letter a TextBoxNumeric refuses never reaches the validator
    const int calls=rule.calls;
    scene.type("x");
    if(std::is_base_of<controls::TextBoxNumeric, T>::value)
        CHECK(rule.calls == calls);
    else
        CHECK(rule.seen.back() == "12x");
}

template<typename T>
void checkValidateOnFocusLoss()
{
    Scene scene;
    T *box=scene.template field<T>("box", 10);
    T *other=scene.template field<T>("other", 60);
    Rule rule;

    box->setText("");
    box->setValidator(rule.make());
    box->focus();
    scene.settle();

    // typing does not run it, losing the focus does
    scene.type("123");
    CHECK(rule.calls == 0);
    CHECK_FALSE(box->hasError());
    other->focus();
    scene.frame();
    CHECK(rule.calls == 1);
    CHECK(box->getError() == "two digits at most");

    // Tab off an emptied box
    box->focus();
    box->setText("");
    scene.frame();
    CHECK(rule.calls == 1);
    scene.key(keys::Tab);
    CHECK_FALSE(box->isFocussed());
    CHECK(box->getError() == "required");

    // validate() on demand
    box->setText("7");
    CHECK(box->hasError());
    CHECK(box->validate());
    CHECK_FALSE(box->hasError());
    box->setText("");
    CHECK_FALSE(box->validate());
    CHECK(box->getError() == "required");

    // removed, the error stays and nothing runs it again
    const int calls=rule.calls;
    box->setValidator(nullptr);
    CHECK_FALSE(box->hasValidator());
    CHECK(box->getError() == "required");
    box->setText("5");
    box->focus();
    other->focus();
    scene.frame();
    CHECK(rule.calls == calls);
    CHECK(box->getError() == "required");
    CHECK_FALSE(box->validate());
}

// Return runs it whether it is set for Change or for FocusLoss, before
// onReturnPressedCaller fires.
template<typename T>
void checkValidateOnReturn(controls::ValidateOn when)
{
    Scene scene;
    T *box=scene.template field<T>("box", 10);
    Rule rule;

    box->setText("");
    box->setValidator(rule.make(), when);
    box->focus();
    scene.settle();
    box->setText("12345", false);
    rule.calls=0;

    struct Enter: event::Handler
    {
        controls::TextBox *box;
        bool hadError=false;
        void entered(event::Info) { hadError=box->hasError(); }
    } enter;
    enter.box=box;
    box->onReturnPressedCaller.add(&enter, &Enter::entered);

    scene.key(keys::Return);
    CHECK(rule.calls >= 1);
    CHECK(box->hasError());
    CHECK(enter.hadError);
}

} // namespace

TEST_CASE_TEMPLATE("Field validation: a Change validator runs on each text change", T,
    controls::TextBox, controls::PasswordTextBox, controls::TextBoxNumeric, controls::NumericUpDown)
{
    checkValidateOnChange<T>();
}

TEST_CASE_TEMPLATE("Field validation: a FocusLoss validator runs as the focus goes, and validate()", T,
    controls::TextBox, controls::PasswordTextBox, controls::TextBoxNumeric, controls::NumericUpDown)
{
    checkValidateOnFocusLoss<T>();
}

TEST_CASE_TEMPLATE("Field validation: Return runs the validator in either mode", T,
    controls::TextBox, controls::PasswordTextBox, controls::TextBoxNumeric, controls::NumericUpDown)
{
    checkValidateOnReturn<T>(controls::ValidateOn::Change);
    checkValidateOnReturn<T>(controls::ValidateOn::FocusLoss);
}

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

namespace
{

namespace tn=gweni::skin::textured::tokyonight;

//! Records the frames the TextBox renderer draws (its slot 0 among them).
class FrameRecorder: public test::StubRenderer
{
public:
    struct Draw
    {
        Rect rect;
        float borderWidth;
        Color border;
    };

    void drawRoundedRect(size_t, Rect rect, float, float borderWidth, Color, Color, Color border, int) override
    {
        draws.push_back({rect, borderWidth, border});
    }

    std::vector<Draw> draws;
};

//! The textured skin's TextBox renderer, a StubControl for the rest.
class TextBoxSkin: public test::StubSkin
{
public:
    explicit TextBoxSkin(test::StubRenderer *renderer): StubSkin(renderer) {}

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        // the textured skin has no renderer of their own: they draw as a TextBox
        if(typeName == "TextBox" || typeName == "PasswordTextBox" || typeName == "TextBoxNumeric"
            || typeName == "NumericUpDown")
            return std::make_unique<skin::textured::TextBox>();
        return StubSkin::getControlRenderer(typeName);
    }
};

struct LookScene
{
    FrameRecorder renderer;
    TextBoxSkin skin{&renderer};
    controls::Canvas *canvas;

    LookScene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
    }

    ~LookScene()
    {
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
    }

    //! The frame a frame drew at (10, 10), the box's: exactly one.
    FrameRecorder::Draw frame()
    {
        renderer.draws.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();

        std::vector<FrameRecorder::Draw> mine;

        for(const FrameRecorder::Draw &draw:renderer.draws)
        {
            if(draw.rect.x == 10 && draw.rect.y == 10)
                mine.push_back(draw);
        }
        REQUIRE(mine.size() == 1);
        return mine.back();
    }
};

bool sameColor(const Color &a, const Color &b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

template<typename T>
void checkLook()
{
    LookScene scene;
    T *box=scene.canvas->newChild<T>("box");
    T *other=scene.canvas->newChild<T>("other");
    box->setPos(10, 10);
    other->setPos(10, 80);
    box->setHelperText("help");
    scene.canvas->updateCanvas();
    scene.canvas->renderCanvas();

    box->setError("bad");
    FrameRecorder::Draw draw=scene.frame();
    CHECK(sameColor(draw.border, tn::TextBoxError.border));
    CHECK(sameColor(draw.border, tn::Red));
    CHECK(draw.borderWidth == tn::TextBoxError.borderWidth);
    // the field part, above the message line
    CHECK(draw.rect.h == box->getFieldHeight());
    CHECK(box->getFieldHeight() < box->getHeight());

    box->focus();
    draw=scene.frame();
    CHECK(sameColor(draw.border, tn::TextBoxErrorFocus.border));
    CHECK(draw.borderWidth == tn::TextBoxErrorFocus.borderWidth);
    other->focus();
    draw=scene.frame();
    CHECK(sameColor(draw.border, tn::TextBoxError.border));

    box->setDisabled(true);
    draw=scene.frame();
    CHECK(sameColor(draw.border, tn::TextBoxErrorDisabled.border));
    box->setDisabled(false);
    scene.frame();

    box->clearError();
    draw=scene.frame();
    CHECK(sameColor(draw.border, tn::TextBox.border));
    CHECK(draw.rect.h == box->getFieldHeight());
}

} // namespace

TEST_CASE_TEMPLATE("Field validation: the error surfaces", T,
    controls::TextBox, controls::PasswordTextBox, controls::TextBoxNumeric, controls::NumericUpDown)
{
    checkLook<T>();
}

TEST_CASE("Field validation: the message follows the ErrorText and HelperText colours")
{
    LookScene scene;
    controls::TextBox *box=scene.canvas->newChild<controls::TextBox>("box");
    const Color error=scene.skin.Colors.ErrorText;
    const Color helper=scene.skin.Colors.HelperText;

    box->setHelperText("help");
    scene.canvas->updateCanvas();
    scene.canvas->renderCanvas();
    CHECK(sameColor(box->getMessageText()->TextColor(), helper));

    box->setError("bad");
    scene.canvas->updateCanvas();
    CHECK(sameColor(box->getMessageText()->TextColor(), error));

    // a theme edit reaches it
    scene.skin.Colors.ErrorText=Color(1, 2, 3);
    tools::ThemeEditor::applyToCanvas(scene.canvas);
    CHECK(sameColor(box->getMessageText()->TextColor(), Color(1, 2, 3)));

    scene.skin.Colors.HelperText=Color(4, 5, 6);
    box->clearError();
    CHECK(sameColor(box->getMessageText()->TextColor(), Color(4, 5, 6)));

    scene.skin.Colors.ErrorText=error;
    scene.skin.Colors.HelperText=helper;
}
