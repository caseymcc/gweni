/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A single-line box's text never holds a line break or a tab, however it
 *  gets there (#569): TextBox, PasswordTextBox and TextBoxNumeric, by a paste
 *  (Ctrl+V through the GLFW translator), insertText and setText, drop the
 *  trailing line breaks and turn every other one ("\r\n", a lone "\r", "\n")
 *  and every "\t" into one space, as Firefox's single-line inputs do. For a
 *  PasswordTextBox that is its real text, with one mask character per
 *  codepoint of it. The rule runs before isTextAllowed and the length cap, so a numeric
 *  box takes a pasted "12\n" and the cap counts the converted text; a paste
 *  leaves the caret just after what it put in, with no selection and one
 *  onTextChanged, and one that converts to nothing changes nothing, a
 *  selection under it included. A TextBoxMultiline turns "\r\n" and a lone
 *  "\r" into "\n" and keeps its tabs. And after a multi-line paste a
 *  single-line box's Text is one line tall, measured by a renderer that, as
 *  SlotRenderer does, starts a line at each '\n'.
 */
#include "headlessFixture.h"

#include <gweni/controls/textBox.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/input/glfw3.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

using namespace gweni;

namespace
{

//! The stub's measure, one line high per '\n' the string holds plus one, as
//! SlotRenderer's layout (and freetype-gl's) starts a new line at each.
class LineBreakRenderer: public test::StubRenderer
{
public:
    gweni::Point measureText(const gweni::Font &font, const gweni::String &text) override
    {
        gweni::Point p=StubRenderer::measureText(font, text);
        p.y*=1+static_cast<int>(std::count(text.begin(), text.end(), '\n'));
        return p;
    }
};

struct Scene
{
    LineBreakRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    input::GLFW3 glfw;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
        glfw.initialize(canvas);
    }

    ~Scene()
    {
        // The focus and the key state are process-global.
        glfw.processFocus(false);
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void glfwKey(int key, int action, int mods)
    {
        glfw.processKeyEvent(input::GLFW3::KeyEvent{key, 0, action, mods});
    }

    //! A key pressed and released, Ctrl held around it when \p control.
    void key(int key, bool control=false)
    {
        const int mods=control?GLFW_MOD_CONTROL:0;

        if(control)
            glfwKey(GLFW_KEY_LEFT_CONTROL, GLFW_PRESS, GLFW_MOD_CONTROL);
        glfwKey(key, GLFW_PRESS, mods);
        glfwKey(key, GLFW_RELEASE, mods);
        if(control)
            glfwKey(GLFW_KEY_LEFT_CONTROL, GLFW_RELEASE, 0);
        frame();
    }

    void paste(controls::TextBox *box, const String &text)
    {
        platform::setClipboardText(text);
        box->focus();
        key(GLFW_KEY_V, true);
    }

    template<typename T>
    T *box(const String &text="")
    {
        T *box=canvas->newChild<T>("box");
        box->setWidth(200);
        box->setText(text);
        frame();
        return box;
    }
};

enum class Way
{
    Paste,
    Insert,
    Set
};

const char *wayName(Way way)
{
    switch(way)
    {
    case Way::Paste: return "paste";
    case Way::Insert: return "insertText";
    case Way::Set: return "setText";
    }
    return "?";
}

const Way kWays[]={Way::Paste, Way::Insert, Way::Set};

//! \p text put into the empty \p box the way \p way says.
void put(Scene &scene, controls::TextBox *box, Way way, const String &text)
{
    switch(way)
    {
    case Way::Paste:
        scene.paste(box, text);
        break;
    case Way::Insert:
        box->insertText(text);
        scene.frame();
        break;
    case Way::Set:
        box->setText(text);
        scene.frame();
        break;
    }
}

struct Case
{
    String in;
    String out;
};

//! The single-line rule's examples (#569's criterion 1).
const std::vector<Case> kSingleLine={
    {"a\nb", "a b"},
    {"a\r\nb", "a b"},
    {"a\rb", "a b"},
    {"a\tb", "a b"},
    {"12\n", "12"},
    {"12\r\n", "12"},
    {"12\r", "12"},
    {"12\n\r\n\r", "12"},
    {"\na", " a"},
    {"a\n\nb", "a  b"},
    {"a\r\n\r\nb", "a  b"},
    {"a\t", "a "},
    {"a\t\n", "a "},
};

bool hasBreakOrTab(const String &text)
{
    return text.find_first_of("\r\n\t") != String::npos;
}

//! Counts onTextChanged.
struct Counter: public event::Handler
{
    int count=0;
    void changed(event::Info) { ++count; }
};

}//namespace

TEST_CASE("A TextBox turns line breaks and tabs into spaces and drops the trailing breaks")
{
    for(Way way : kWays)
    {
        for(const Case &c : kSingleLine)
        {
            const String how=wayName(way);
            CAPTURE(how);
            CAPTURE(c.in);

            Scene scene;
            controls::TextBox *box=scene.box<controls::TextBox>();

            put(scene, box, way, c.in);
            CHECK(box->getText() == c.out);
            CHECK_FALSE(hasBreakOrTab(box->getText()));
        }
    }
}

TEST_CASE("A PasswordTextBox's real text has no line break or tab, and its mask a character per codepoint")
{
    for(Way way : kWays)
    {
        for(const Case &c : kSingleLine)
        {
            const String how=wayName(way);
            CAPTURE(how);
            CAPTURE(c.in);

            Scene scene;
            controls::PasswordTextBox *box=scene.box<controls::PasswordTextBox>();

            put(scene, box, way, c.in);
            CHECK(box->getText() == c.out);
            CHECK_FALSE(hasBreakOrTab(box->getText()));

            // the Text shows the mask, one '*' per codepoint of the real text
            // (every result here is ASCII, so one per byte)
            const String &mask=box->Label::getText();
            CHECK(mask == String(c.out.size(), '*'));
            CHECK(box->textLength() == static_cast<int>(c.out.size()));
        }
    }
}

TEST_CASE("A TextBoxNumeric converts before it checks: a pasted \"12\\n\" is 12")
{
    // setText checks nothing, so every example converts as in a TextBox
    for(const Case &c : kSingleLine)
    {
        CAPTURE(c.in);

        Scene scene;
        controls::TextBoxNumeric *box=scene.box<controls::TextBoxNumeric>();

        put(scene, box, Way::Set, c.in);
        CHECK(box->getText() == c.out);
    }

    // typed in, a trailing break goes and the number is taken; a break inside
    // one is a space, which a number refuses, so the box keeps what it had
    const std::vector<Case> typed={
        {"12\n", "12"},
        {"12\r\n", "12"},
        {"12\r", "12"},
        {"-1.5\n", "-1.5"},
        {"1\n2", ""},
        {"1\t", ""},
    };

    for(Way way : {Way::Paste, Way::Insert})
    {
        for(const Case &c : typed)
        {
            const String how=wayName(way);
            CAPTURE(how);
            CAPTURE(c.in);

            Scene scene;
            controls::TextBoxNumeric *box=scene.box<controls::TextBoxNumeric>();

            put(scene, box, way, c.in);
            CHECK(box->getText() == c.out);
        }
    }

    Scene scene;
    controls::TextBoxNumeric *box=scene.box<controls::TextBoxNumeric>();
    scene.paste(box, "12\n");
    CHECK(box->getText() == "12");
    CHECK(box->GetFloatFromText() == 12.0f);
}

TEST_CASE("The length cap counts a paste's converted text")
{
    Scene scene;

    controls::TextBox *hex=scene.box<controls::TextBox>();
    hex->setMaxTextLength(9);
    scene.paste(hex, "#FF8800\n");
    CHECK(hex->getText() == "#FF8800");

    // "a\r\nb" is 4 bytes, "a b" 3: it fits a cap of 3 whole
    controls::TextBox *box=scene.box<controls::TextBox>();
    box->setMaxTextLength(3);
    scene.paste(box, "a\r\nb");
    CHECK(box->getText() == "a b");
}

TEST_CASE("A converted paste leaves the caret after it, with one onTextChanged")
{
    Scene scene;
    controls::TextBox *box=scene.box<controls::TextBox>("xy");
    box->focus();
    scene.key(GLFW_KEY_HOME);
    scene.key(GLFW_KEY_RIGHT);          // x|y

    Counter counter;
    box->onTextChangedCaller.add(&counter, &Counter::changed);

    scene.paste(box, "a\r\nb\n");
    CHECK(box->getText() == "xa by");
    CHECK_FALSE(box->hasSelection());
    CHECK(counter.count == 1);

    // the caret is just after "a b"
    box->insertText("Z");
    CHECK(box->getText() == "xa bZy");
}

TEST_CASE("A paste that converts to nothing changes nothing, a selection under it included")
{
    for(const String &nothing : {String("\n"), String("\r\n"), String("\r"), String("\n\r\n")})
    {
        CAPTURE(nothing);

        Scene scene;
        controls::TextBox *box=scene.box<controls::TextBox>("abc");
        box->focus();
        scene.key(GLFW_KEY_A, true);    // select all
        REQUIRE(box->getSelection() == "abc");

        Counter counter;
        box->onTextChangedCaller.add(&counter, &Counter::changed);

        scene.paste(box, nothing);
        CHECK(box->getText() == "abc");
        CHECK(box->hasSelection());
        CHECK(box->getSelection() == "abc");
        CHECK(counter.count == 0);

        box->insertText(nothing);
        CHECK(box->getText() == "abc");
        CHECK(box->getSelection() == "abc");
        CHECK(counter.count == 0);
    }
}

TEST_CASE("A TextBoxMultiline turns \"\\r\\n\" and a lone \"\\r\" into \"\\n\" and keeps its tabs")
{
    const std::vector<Case> cases={
        {"a\r\nb\rc\nd", "a\nb\nc\nd"},
        {"a\tb", "a\tb"},
        {"a\r\r\nb", "a\n\nb"},
        {"a\n", "a\n"},
        {"a\r\n", "a\n"},
    };

    for(Way way : kWays)
    {
        for(const Case &c : cases)
        {
            const String how=wayName(way);
            CAPTURE(how);
            CAPTURE(c.in);

            Scene scene;
            controls::TextBoxMultiline *box=scene.box<controls::TextBoxMultiline>();

            put(scene, box, way, c.in);
            CHECK(box->getText() == c.out);
            CHECK(box->getText().find('\r') == String::npos);
        }
    }
}

TEST_CASE("After a multi-line paste a single-line box's text is one line tall")
{
    Scene scene;

    controls::TextBox *oneLine=scene.box<controls::TextBox>("a b c");
    const int lineHeight=oneLine->textHeight();
    REQUIRE(lineHeight > 0);

    controls::TextBox *box=scene.box<controls::TextBox>();
    scene.paste(box, "a\nb\r\nc\n");
    CHECK(box->getText() == "a b c");
    CHECK(box->textHeight() == lineHeight);

    // the measure does see line breaks: a multiline box holding them is taller
    controls::TextBoxMultiline *multi=scene.box<controls::TextBoxMultiline>();
    scene.paste(multi, "a\nb\r\nc");
    CHECK(multi->textHeight() == 3*lineHeight);
}
