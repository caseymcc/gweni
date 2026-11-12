/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A PasswordTextBox shows one mask character per codepoint of its password,
 *  not per byte (#617), and its caret, selection, clicks and scroll follow
 *  that mask: TextBox's displayPos/realPos map a byte offset into getText()
 *  to one into the mask and back. Pinned here, on the headless stub (which
 *  measures every byte, so a mask a byte per byte would be visibly wider):
 *  "é", "€", "a€b" and a lone invalid byte give 1, 1, 3 and 1 masks; typing
 *  "é€" (by insertText and by GLFW char events), Left, Shift+Left/Right,
 *  Ctrl+A and Backspace give exactly the caret and selection rects, and the
 *  mask, that an ASCII password of as many codepoints gets; a click and a
 *  drag at mask boundary k land after real codepoint k; a password too long
 *  for its box scrolls as the ASCII one does; getText(), textLength() and the
 *  length limit stay in bytes. setPasswordChar takes one valid UTF-8
 *  codepoint ("•") and rebuilds the mask and the caret at once, and refuses
 *  an empty string, two codepoints and invalid UTF-8 (a lone lead or
 *  continuation byte, an overlong form, a surrogate, past U+10FFFF), keeping
 *  its character; the char overload takes ASCII only; "*" is the default.
 *  And Ctrl+C and Ctrl+X leave the clipboard (and Ctrl+X the text) as they
 *  were while Ctrl+V and Ctrl+A work, where a plain TextBox copies and cuts.
 */
#include "headlessFixture.h"

#include <gweni/controls/textBox.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/input/glfw3.h>
#include <gweni/platforms/platform.h>
#include <gweni/utility.h>

#include <doctest/doctest.h>

#include <functional>
#include <string>
#include <vector>

using namespace gweni;

namespace
{

const String eAcute="\xC3\xA9";             // U+00E9, 2 bytes
const String euro="\xE2\x82\xAC";           // U+20AC, 3 bytes
const String grin="\xF0\x9F\x98\x80";       // U+1F600, 4 bytes
const String bullet="\xE2\x80\xA2";         // U+2022, 3 bytes

struct Scene
{
    test::StubRenderer renderer;
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

    //! A key pressed and released, with the modifiers in mods held around it
    //! the way GLFW reports them: their own presses first, released last.
    void key(int key, int mods=0)
    {
        int held=0;
        if(mods & GLFW_MOD_CONTROL)
            glfwKey(GLFW_KEY_LEFT_CONTROL, GLFW_PRESS, held|=GLFW_MOD_CONTROL);
        if(mods & GLFW_MOD_SHIFT)
            glfwKey(GLFW_KEY_LEFT_SHIFT, GLFW_PRESS, held|=GLFW_MOD_SHIFT);

        glfwKey(key, GLFW_PRESS, mods);
        glfwKey(key, GLFW_RELEASE, mods);

        if(mods & GLFW_MOD_SHIFT)
            glfwKey(GLFW_KEY_LEFT_SHIFT, GLFW_RELEASE, held&=~GLFW_MOD_SHIFT);
        if(mods & GLFW_MOD_CONTROL)
            glfwKey(GLFW_KEY_LEFT_CONTROL, GLFW_RELEASE, held&=~GLFW_MOD_CONTROL);
        frame();
    }

    //! One char event per codepoint, as glfwSetCharCallback sends them.
    void type(const std::u32string &text)
    {
        for(char32_t c : text)
            glfw.processCharEvent(static_cast<unsigned int>(c));
        frame();
    }

    template<typename T=controls::PasswordTextBox>
    T *focusedBox(int width=200)
    {
        T *box=canvas->newChild<T>("box");
        box->setBounds(10, 10, width, 20);
        box->setText("");
        frame();
        box->focus();
        frame();
        return box;
    }
};

//! What the box shows after a step: its mask, caret and selection rects.
struct Look
{
    String mask;
    Rect caret;
    Rect selection;

    bool operator==(const Look &o) const
    {
        return mask == o.mask && caret == o.caret && selection == o.selection;
    }
};

doctest::String rectString(const Rect &r)
{
    return doctest::String(format("(%d,%d %dx%d)", r.x, r.y, r.w, r.h).c_str());
}

}

namespace doctest
{
template<>
struct StringMaker<Look>
{
    static String convert(const Look &look)
    {
        return String(("\""+look.mask+"\"").c_str())+" caret "+rectString(look.caret)
            +" selection "+rectString(look.selection);
    }
};
}

namespace
{

Look lookOf(controls::PasswordTextBox *box)
{
    return Look{box->Label::getText(), box->getCaretBounds(), box->getSelectionBounds()};
}

//! The box's caret position, found the only way the API has: a "|" inserted
//! there (and taken out again).
size_t caretOffset(Scene &scene, controls::TextBox *box)
{
    const String before=box->getText();
    box->insertText("|");
    const size_t at=box->getText().find('|');
    scene.key(GLFW_KEY_BACKSPACE);
    REQUIRE(box->getText() == before);
    return at;
}

//! A step on a focused box; each returns after the frame it ran in.
using Step=std::function<void(Scene &, controls::PasswordTextBox *)>;

//! Runs \p steps on a new focused box and records its Look after each.
std::vector<Look> run(const std::vector<Step> &steps, const String &passwordChar="*")
{
    Scene scene;
    controls::PasswordTextBox *box=scene.focusedBox();
    REQUIRE(box->setPasswordChar(passwordChar));

    std::vector<Look> looks;
    for(const Step &step : steps)
    {
        step(scene, box);
        looks.push_back(lookOf(box));
    }
    return looks;
}

//! The two runs step by step, so a failure names the step and both looks.
void checkSame(const std::vector<Look> &password, const std::vector<Look> &ascii)
{
    REQUIRE(password.size() == ascii.size());
    for(size_t i=0; i < password.size(); ++i)
    {
        CAPTURE(i);
        CHECK(password[i] == ascii[i]);
    }
}

Step insert(const String &text)
{
    return [text](Scene &scene, controls::PasswordTextBox *box) { box->insertText(text); scene.frame(); };
}

Step typeChars(const std::u32string &text)
{
    return [text](Scene &scene, controls::PasswordTextBox *) { scene.type(text); };
}

Step press(int key, int mods=0)
{
    return [key, mods](Scene &scene, controls::PasswordTextBox *) { scene.key(key, mods); };
}

//! The edits and moves of criteria 2 and 3, after the text is typed.
std::vector<Step> editsAfter(const Step &typed)
{
    return {
        typed,                                  // caret after the 2nd mask character
        press(GLFW_KEY_LEFT),                   // one mask character back
        press(GLFW_KEY_RIGHT, GLFW_MOD_SHIFT),  // selects the last one
        press(GLFW_KEY_LEFT, GLFW_MOD_SHIFT),   // and nothing again
        press(GLFW_KEY_LEFT, GLFW_MOD_SHIFT),   // the first one
        press(GLFW_KEY_A, GLFW_MOD_CONTROL),    // both
        press(GLFW_KEY_END),
        press(GLFW_KEY_BACKSPACE),              // one mask character, one codepoint
    };
}

}

TEST_CASE("A PasswordTextBox shows one mask character per codepoint")
{
    struct Case
    {
        String text;
        size_t masks;
    };
    const Case cases[]={
        {"", 0},
        {eAcute, 1},
        {euro, 1},
        {"a"+euro+"b", 3},
        {grin, 1},
        {"\x80", 1},                    // a lone continuation byte
        {"\xFF", 1},                    // never a UTF-8 byte
        {"\xC3", 1},                    // a lead byte with nothing after it
        {"\xFF" "b", 2},
        {eAcute+euro+grin+"o", 4},
    };

    Scene scene;
    controls::PasswordTextBox *box=scene.focusedBox();
    CHECK(box->getPasswordChar() == "*");

    for(const Case &c : cases)
    {
        CAPTURE(c.text);
        box->setText(c.text);
        scene.frame();

        CHECK(box->Label::getText() == String(c.masks, '*'));
        CHECK(box->getText() == c.text);
        CHECK(box->textLength() == static_cast<int>(c.text.size()));
    }
}

TEST_CASE("Typing a non-ASCII password moves, selects and erases as an ASCII one of as many codepoints")
{
    SUBCASE("insertText")
    {
        const std::vector<Look> password=run(editsAfter(insert(eAcute+euro)));
        const std::vector<Look> ascii=run(editsAfter(insert("ab")));
        checkSame(password, ascii);
    }
    SUBCASE("GLFW char events")
    {
        const std::vector<Look> password=run(editsAfter(typeChars(U"é€")));
        const std::vector<Look> ascii=run(editsAfter(typeChars(U"ab")));
        checkSame(password, ascii);
    }
    SUBCASE("a bullet mask")
    {
        const std::vector<Look> password=run(editsAfter(insert(eAcute+euro)), bullet);
        const std::vector<Look> ascii=run(editsAfter(insert("ab")), bullet);
        checkSame(password, ascii);
    }

    // What those steps did to the password itself, and where the caret was.
    Scene scene;
    controls::PasswordTextBox *box=scene.focusedBox();
    scene.type(U"é€");
    CHECK(box->getText() == eAcute+euro);
    CHECK(box->Label::getText() == "**");
    CHECK(caretOffset(scene, box) == eAcute.size()+euro.size());

    // after the 2nd mask character: as wide as the whole mask
    const Rect caret=box->getCaretBounds();
    CHECK(caret.x == box->getCharacterPosition(2).x);

    scene.key(GLFW_KEY_LEFT);
    CHECK(caretOffset(scene, box) == eAcute.size());
    CHECK(box->getCaretBounds().x == box->getCharacterPosition(1).x);

    scene.key(GLFW_KEY_LEFT, GLFW_MOD_SHIFT);
    CHECK(box->getSelection() == eAcute);
    const Rect selection=box->getSelectionBounds();
    CHECK(selection.x == box->getCharacterPosition(0).x);
    CHECK(selection.w == box->getCharacterPosition(1).x-box->getCharacterPosition(0).x);

    scene.key(GLFW_KEY_A, GLFW_MOD_CONTROL);
    CHECK(box->getSelection() == eAcute+euro);
    CHECK(box->getSelectionBounds().w == box->getCharacterPosition(2).x-box->getCharacterPosition(0).x);

    scene.key(GLFW_KEY_END);
    scene.key(GLFW_KEY_BACKSPACE);
    CHECK(box->getText() == eAcute);
    CHECK(box->Label::getText() == "*");
}

TEST_CASE("A click or a drag at mask boundary k lands after real codepoint k")
{
    const String password="a"+eAcute+euro+grin+"z";
    const size_t after[]={0, 1, 3, 6, 10, 11};      // the byte after each codepoint

    for(const String &maskChar : {String("*"), bullet})
    {
        CAPTURE(maskChar);
        Scene scene;
        controls::PasswordTextBox *box=scene.focusedBox();
        REQUIRE(box->setPasswordChar(maskChar));
        box->setText(password);
        scene.frame();

        const int maskBytes=static_cast<int>(maskChar.size());
        const Point origin=box->localPosToCanvas(Point(0, 0));
        auto at=[&](int k) {
            return Point(origin.x+box->getCharacterPosition(k*maskBytes).x, origin.y+box->getHeight()/2);
        };

        for(int k=0; k <= 5; ++k)
        {
            CAPTURE(k);
            const Point p=at(k);
            scene.canvas->inputMouseMoved(p.x, p.y, 0, 0);
            scene.canvas->inputMouseButton(0, true);
            scene.canvas->inputMouseButton(0, false);
            scene.frame();
            CHECK(caretOffset(scene, box) == after[k]);
        }

        // pressed at boundary 1, dragged to 4 and let go: the 2nd to 4th codepoints
        const Point from=at(1);
        const Point to=at(4);
        scene.canvas->inputMouseMoved(from.x, from.y, 0, 0);
        scene.canvas->inputMouseButton(0, true);
        scene.canvas->inputMouseMoved(to.x, to.y, to.x-from.x, 0);
        scene.canvas->inputMouseButton(0, false);
        scene.frame();
        CHECK(box->getSelection() == eAcute+euro+grin);
        CHECK(box->getSelectionBounds().x == box->getCharacterPosition(1*maskBytes).x);
        CHECK(box->getSelectionBounds().w
            == box->getCharacterPosition(4*maskBytes).x-box->getCharacterPosition(1*maskBytes).x);
    }
}

TEST_CASE("A password too long for its box scrolls as an ASCII one of as many codepoints")
{
    struct Scroll
    {
        int textX;
        Rect caret;
    };
    auto scrolls=[](const String &character) {
        Scene scene;
        controls::PasswordTextBox *box=scene.focusedBox(60);
        String text;
        for(int i=0; i < 30; ++i)
            text+=character;

        std::vector<Scroll> result;
        auto record=[&]() {
            const controls::internal::Text *shown=nullptr;
            for(controls::Base *child : box->getActualChildren())
                if(auto *t=dynamic_cast<controls::internal::Text *>(child))
                    if(t->getText() == box->Label::getText())
                        shown=t;
            REQUIRE(shown);
            result.push_back(Scroll{shown->getX(), box->getCaretBounds()});
        };

        box->insertText(text);                  // the caret at the end, scrolled to it
        scene.frame();
        record();
        for(int i=0; i < 12; ++i)
            scene.key(GLFW_KEY_LEFT);
        record();
        scene.key(GLFW_KEY_HOME);
        record();
        for(int i=0; i < 20; ++i)
            scene.key(GLFW_KEY_RIGHT);
        record();
        return result;
    };

    const std::vector<Scroll> password=scrolls(eAcute);
    const std::vector<Scroll> ascii=scrolls("a");
    REQUIRE(password.size() == ascii.size());

    CHECK(ascii[0].textX < 0);                  // it did scroll
    for(size_t i=0; i < ascii.size(); ++i)
    {
        CAPTURE(i);
        CHECK(password[i].textX == ascii[i].textX);
        CHECK(rectString(password[i].caret) == rectString(ascii[i].caret));
    }
}

TEST_CASE("A PasswordTextBox's text, length and length limit are in bytes")
{
    Scene scene;
    controls::PasswordTextBox *box=scene.focusedBox();
    box->setMaxTextLength(5);

    scene.type(U"é€é");          // 2 + 3 bytes fit, the last é does not
    CHECK(box->getText() == eAcute+euro);
    CHECK(box->textLength() == 5);
    CHECK(box->Label::getText() == "**");
}

TEST_CASE("setPasswordChar takes one UTF-8 codepoint and rebuilds the mask at once")
{
    Scene scene;
    controls::PasswordTextBox *box=scene.focusedBox();
    box->setText("a"+euro+"b");
    box->moveCaretToend();
    scene.frame();
    REQUIRE(box->Label::getText() == "***");
    REQUIRE(box->getCaretBounds().x == box->getCharacterPosition(3).x);
    const Rect starCaret=box->getCaretBounds();

    CHECK(box->setPasswordChar(bullet));
    CHECK(box->getPasswordChar() == bullet);
    CHECK(box->Label::getText() == bullet+bullet+bullet);     // no edit, no frame
    CHECK(box->getText() == "a"+euro+"b");

    // the caret follows the new mask, at the same codepoint
    scene.frame();
    CHECK(box->getCaretBounds().x == box->getCharacterPosition(3*3).x);
    CHECK(box->getCaretBounds().x != starCaret.x);
    CHECK(caretOffset(scene, box) == 5);

    const String refused[]={
        "",
        "ab",
        bullet+"*",
        "\x80",                         // a lone continuation byte
        "\xC3",                         // a lead byte alone
        "\xC3\xA9\x80",                 // é and a stray continuation byte
        "\xC0\xAF",                     // an overlong '/'
        "\xE0\x80\xAF",                 // and another
        "\xED\xA0\x80",                 // a surrogate, U+D800
        "\xF4\x90\x80\x80",             // past U+10FFFF
        "\xFF",
    };
    for(const String &c : refused)
    {
        CAPTURE(c);
        CHECK_FALSE(box->setPasswordChar(c));
        CHECK(box->getPasswordChar() == bullet);
        CHECK(box->Label::getText() == bullet+bullet+bullet);
    }

    CHECK_FALSE(box->setPasswordChar('\xE9'));
    CHECK(box->getPasswordChar() == bullet);
    CHECK(box->setPasswordChar('#'));
    CHECK(box->getPasswordChar() == "#");
    CHECK(box->Label::getText() == "###");
    CHECK(box->setPasswordChar(grin));
    CHECK(box->Label::getText() == grin+grin+grin);
}

TEST_CASE("Ctrl+C and Ctrl+X never put a PasswordTextBox's text on the clipboard")
{
    const String password="a"+euro+"b";

    SUBCASE("a PasswordTextBox")
    {
        Scene scene;
        controls::PasswordTextBox *box=scene.focusedBox();
        box->setText(password);
        scene.frame();

        scene.key(GLFW_KEY_A, GLFW_MOD_CONTROL);
        REQUIRE(box->getSelection() == password);
        const Rect selection=box->getSelectionBounds();

        platform::setClipboardText("kept");
        scene.key(GLFW_KEY_C, GLFW_MOD_CONTROL);
        CHECK(platform::getClipboardText() == "kept");

        scene.key(GLFW_KEY_X, GLFW_MOD_CONTROL);
        CHECK(platform::getClipboardText() == "kept");
        CHECK(box->getText() == password);
        CHECK(box->Label::getText() == "***");
        CHECK(box->getSelection() == password);
        CHECK(rectString(box->getSelectionBounds()) == rectString(selection));

        // paste replaces the selection, select-all selects it
        platform::setClipboardText(eAcute+"z");
        scene.key(GLFW_KEY_V, GLFW_MOD_CONTROL);
        CHECK(box->getText() == eAcute+"z");
        CHECK(box->Label::getText() == "**");
        CHECK_FALSE(box->hasSelection());

        scene.key(GLFW_KEY_A, GLFW_MOD_CONTROL);
        CHECK(box->getSelection() == eAcute+"z");
        CHECK(box->getSelectionBounds().w == box->getCharacterPosition(2).x-box->getCharacterPosition(0).x);
    }
    SUBCASE("a plain TextBox copies and cuts")
    {
        Scene scene;
        controls::TextBox *box=scene.focusedBox<controls::TextBox>();
        box->setText(password);
        scene.frame();

        scene.key(GLFW_KEY_A, GLFW_MOD_CONTROL);
        platform::setClipboardText("kept");
        scene.key(GLFW_KEY_C, GLFW_MOD_CONTROL);
        CHECK(platform::getClipboardText() == password);

        platform::setClipboardText("kept");
        scene.key(GLFW_KEY_X, GLFW_MOD_CONTROL);
        CHECK(platform::getClipboardText() == password);
        CHECK(box->getText().empty());
    }
}
