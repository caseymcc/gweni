/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Typed characters are codepoints, and a TextBox edits its UTF-8 text by
 *  whole codepoints. Keys and characters go in through the GLFW translator
 *  (input::GLFW3, the same object the examples' callbacks feed) to a real
 *  Canvas: what is typed is stored as exactly its UTF-8 bytes, Backspace,
 *  Delete, Left, Right, Shift+arrow selections, cut/copy/paste, the length
 *  limit, mouse placement and multiline Up/Down never stop inside a sequence,
 *  and codepoints that are not text (C0, DEL, C1, surrogates, past U+10FFFF)
 *  are dropped. Also the utf8 helpers of utility.h on their own.
 *
 *  It takes GLFW's headers only, not its library. A 4-byte codepoint is stored
 *  as its 4 bytes; positions set from outside snap to a boundary, as the
 *  length limit does; the codepoints that are not text are dropped by
 *  Canvas::inputCharacter; TextBoxNumeric refuses ٣ (Arabic-Indic three);
 *  Text::getClosestCharacter and a mouse click land only on boundaries; and
 *  multiline Up/Down keeps the column in codepoints, with wrapped words never
 *  broken inside a sequence. The helpers are utf8Encode, utf8Next, utf8Prev
 *  and utf8Snap.
 *
 *  The translator's modifiers: AltGr as GLFW reports it on Windows (a Right
 *  Alt with no LEFT_CONTROL event, and Ctrl|Alt in the mods of the keys under
 *  it), replayed with every plausible set of mods on the Right Alt events,
 *  types its character (@, €, µ), fires no CTRL+<letter> accelerator and
 *  leaves Ctrl up, by the next key's press at the latest. Real Ctrl shortcuts
 *  work with X11's and Windows' mods, and a Ctrl held across a keyboard-focus
 *  change is pressed again by the next key's mods.
 */
#include "headlessFixture.h"

#include <gweni/controls/textBox.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/input/glfw3.h>
#include <gweni/platforms/platform.h>
#include <gweni/utility.h>

#include <doctest/doctest.h>

#include <string>

using namespace gweni;

namespace
{

const String eAcute="\xC3\xA9";             // U+00E9, 2 bytes
const String euro="\xE2\x82\xAC";           // U+20AC, 3 bytes
const String grin="\xF0\x9F\x98\x80";       // U+1F600, 4 bytes

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
    void type(std::u32string text)
    {
        for(char32_t c : text)
            glfw.processCharEvent(static_cast<unsigned int>(c));
        frame();
    }

    controls::TextBox *focusedBox(const char *name="box")
    {
        controls::TextBox *box=canvas->newChild<controls::TextBox>(name);
        box->setText("");
        frame();
        box->focus();
        return box;
    }
};

//! Every byte offset of str from 0 to str.size() that utf8Snap leaves alone.
bool isBoundary(const String &str, size_t pos)
{
    return pos == utf8Snap(str, pos);
}

bool isValidUtf8(const String &str)
{
    size_t i=0;
    while(i < str.size())
    {
        const unsigned char lead=static_cast<unsigned char>(str[i]);
        size_t length;
        if(lead < 0x80)
            length=1;
        else if(lead >= 0xC2 && lead <= 0xDF)
            length=2;
        else if(lead >= 0xE0 && lead <= 0xEF)
            length=3;
        else if(lead >= 0xF0 && lead <= 0xF4)
            length=4;
        else
            return false;

        if(i+length > str.size())
            return false;
        for(size_t k=1; k < length; ++k)
            if((static_cast<unsigned char>(str[i+k]) & 0xC0) != 0x80)
                return false;
        i+=length;
    }
    return true;
}

} // namespace

TEST_CASE("utf8Encode writes each codepoint as its UTF-8 bytes and nothing for a non-codepoint")
{
    CHECK(utf8Encode(U'A') == "A");
    CHECK(utf8Encode(0x7F) == "\x7F");
    CHECK(utf8Encode(0x80) == "\xC2\x80");
    CHECK(utf8Encode(U'é') == eAcute);
    CHECK(utf8Encode(0x7FF) == "\xDF\xBF");
    CHECK(utf8Encode(0x800) == "\xE0\xA0\x80");
    CHECK(utf8Encode(U'€') == euro);
    CHECK(utf8Encode(0xFFFF) == "\xEF\xBF\xBF");
    CHECK(utf8Encode(U'\U0001F600') == grin);
    CHECK(utf8Encode(0x10FFFF) == "\xF4\x8F\xBF\xBF");

    CHECK(utf8Encode(0xD800).empty());
    CHECK(utf8Encode(0xDFFF).empty());
    CHECK(utf8Encode(0x110000).empty());
}

TEST_CASE("utf8Next, utf8Prev and utf8Snap step over and snap to whole codepoints")
{
    // a=0, é=1..2, €=3..5, 😀=6..9, end=10
    const String str="a"+eAcute+euro+grin;
    REQUIRE(str.size() == 10);

    const size_t snapped[]={0, 1, 1, 3, 3, 3, 6, 6, 6, 6, 10, 10};
    const size_t next[]={1, 3, 3, 6, 6, 6, 10, 10, 10, 10, 10, 10};
    const size_t prev[]={0, 0, 1, 1, 3, 3, 3, 6, 6, 6, 6, 6};

    for(size_t pos=0; pos <= 11; ++pos)
    {
        CAPTURE(pos);
        CHECK(utf8Snap(str, pos) == snapped[pos]);
        CHECK(utf8Next(str, pos) == next[pos]);
        CHECK(utf8Prev(str, pos) == prev[pos]);
    }

    CHECK(utf8Next(String(), 0) == 0);
    CHECK(utf8Prev(String(), 3) == 0);
    CHECK(utf8Snap(String(), 3) == 0);
}

TEST_CASE("Typed non-ASCII text is stored as its UTF-8 and edited by whole codepoints")
{
    Scene scene;
    controls::TextBox *box=scene.focusedBox();

    scene.type(U"héllo€");
    CHECK(box->getText() == "h"+eAcute+"llo"+euro);

    scene.key(GLFW_KEY_BACKSPACE);              // the whole €
    CHECK(box->getText() == "h"+eAcute+"llo");

    scene.key(GLFW_KEY_LEFT);
    scene.key(GLFW_KEY_LEFT);                   // h é l | l o
    scene.key(GLFW_KEY_BACKSPACE);              // h é | l o
    CHECK(box->getText() == "h"+eAcute+"lo");

    scene.key(GLFW_KEY_BACKSPACE);              // the whole é
    CHECK(box->getText() == "hlo");

    scene.type(U"ñ");
    CHECK(box->getText() == "h\xC3\xB1lo");

    scene.key(GLFW_KEY_LEFT);                   // h | ñ l o
    scene.key(GLFW_KEY_DELETE);                 // the whole ñ
    CHECK(box->getText() == "hlo");

    scene.key(GLFW_KEY_A, GLFW_MOD_CONTROL);
    scene.type(U"x");
    CHECK(box->getText() == "x");
}

TEST_CASE("A 4-byte codepoint is stored as exactly its 4 bytes and removed by one Backspace")
{
    Scene scene;
    controls::TextBox *box=scene.focusedBox();

    scene.type(U"a\U0001F600");
    CHECK(box->getText() == "a"+grin);
    CHECK(box->getText().size() == 5);

    scene.key(GLFW_KEY_BACKSPACE);
    CHECK(box->getText() == "a");

    // Left and Right step over all four bytes; Delete takes them together.
    scene.type(U"\U0001F600b");
    scene.key(GLFW_KEY_LEFT);
    scene.key(GLFW_KEY_LEFT);                   // a | 😀 b
    scene.key(GLFW_KEY_RIGHT);
    scene.key(GLFW_KEY_RIGHT);                  // a 😀 b |
    scene.key(GLFW_KEY_LEFT);
    scene.key(GLFW_KEY_LEFT);
    scene.key(GLFW_KEY_DELETE);
    CHECK(box->getText() == "ab");
}

TEST_CASE("Codepoints that are not text are not inserted")
{
    Scene scene;
    controls::TextBox *box=scene.focusedBox();

    scene.type(U"[");
    for(char32_t c : {char32_t(0x00), char32_t(0x01), char32_t(0x08), char32_t(0x09), char32_t(0x0A),
            char32_t(0x1F), char32_t(0x7F), char32_t(0x80), char32_t(0x9F), char32_t(0xD800),
            char32_t(0xDBFF), char32_t(0xDC00), char32_t(0xDFFF), char32_t(0x110000), char32_t(0xFFFFFFFF)})
    {
        CAPTURE(static_cast<unsigned>(c));
        CHECK_FALSE(scene.canvas->inputCharacter(c));
    }
    // just past each rejected range
    scene.type(U" ~ \U0010FFFF]");

    CHECK(box->getText() == "[ ~"+utf8Encode(0xA0)+utf8Encode(0xE000)+utf8Encode(0x10FFFF)+"]");
    CHECK(isValidUtf8(box->getText()));
}

TEST_CASE("TextBoxNumeric still takes only ASCII digits")
{
    Scene scene;
    controls::TextBoxNumeric *box=scene.canvas->newChild<controls::TextBoxNumeric>("numeric");
    box->setText("");
    scene.frame();
    box->focus();

    // U+0663 ARABIC-INDIC DIGIT THREE and U+FF11 FULLWIDTH DIGIT ONE are digits,
    // but not the ones a numeric box reads.
    scene.type(U"1٣a１2é");
    CHECK(box->getText() == "12");
}

TEST_CASE("Shift+arrow selections and cut, copy and paste never split a sequence")
{
    Scene scene;
    controls::TextBox *box=scene.focusedBox();

    scene.type(U"aé€");               // a é € |
    scene.key(GLFW_KEY_LEFT, GLFW_MOD_SHIFT);
    scene.key(GLFW_KEY_LEFT, GLFW_MOD_SHIFT);   // a [é €]
    CHECK(box->getSelection() == eAcute+euro);

    scene.key(GLFW_KEY_C, GLFW_MOD_CONTROL);
    CHECK(platform::getClipboardText() == eAcute+euro);

    scene.key(GLFW_KEY_RIGHT, GLFW_MOD_SHIFT);  // a é [€]: the caret end moved
    scene.key(GLFW_KEY_X, GLFW_MOD_CONTROL);
    CHECK(platform::getClipboardText() == euro);
    CHECK(box->getText() == "a"+eAcute);

    scene.key(GLFW_KEY_HOME);
    scene.key(GLFW_KEY_V, GLFW_MOD_CONTROL);
    CHECK(box->getText() == euro+"a"+eAcute);

    scene.key(GLFW_KEY_END);

    // Typing over a selection that ends in a 4-byte codepoint.
    scene.type(U"\U0001F600");
    scene.key(GLFW_KEY_LEFT, GLFW_MOD_SHIFT);
    CHECK(box->getSelection() == grin);
    scene.type(U"z");
    CHECK(box->getText() == euro+"a"+eAcute+"z");
    CHECK(isValidUtf8(box->getText()));
}

TEST_CASE("Positions set from outside snap to a codepoint boundary")
{
    Scene scene;
    controls::TextBox *box=scene.focusedBox();
    box->setText("a"+euro+"b");                 // a=0, €=1..3, b=4
    scene.frame();

    box->setCursorPos(2);
    box->setCursorend(2);
    scene.type(U"x");
    CHECK(box->getText() == "ax"+euro+"b");

    // A range whose ends fall inside sequences deletes whole codepoints.
    box->deleteText(3, 2);                      // from inside € to inside nothing: €
    CHECK(box->getText() == "axb");

    // Replacing the text keeps the caret on a boundary of the new string.
    box->setText(grin);
    scene.frame();
    scene.type(U"!");
    CHECK(isValidUtf8(box->getText()));
    CHECK(box->getText().size() == 5);
}

TEST_CASE("The length limit leaves out a codepoint that does not fit whole")
{
    Scene scene;
    controls::TextBox *box=scene.focusedBox();
    box->setMaxTextLength(4);

    scene.type(U"ab€");                    // € needs 3 bytes, 2 are left
    CHECK(box->getText() == "ab");

    scene.type(U"é");                      // é needs 2: fits exactly
    CHECK(box->getText() == "ab"+eAcute);

    box->setText("a");
    scene.frame();
    box->moveCaretToend();
    box->insertText(eAcute+euro);               // é fits, € is cut off whole
    CHECK(box->getText() == "a"+eAcute);
}

TEST_CASE("A mouse position always maps to a codepoint boundary")
{
    Scene scene;
    controls::TextBox *box=scene.focusedBox();
    const String text="h"+eAcute+euro+grin+"o";
    box->setText(text);
    scene.frame();

    controls::Label *label=box;
    REQUIRE(label->textLength() == static_cast<int>(text.size()));

    // The stub measures every byte, so the old byte-by-byte search found
    // offsets inside the sequences; across the whole width none may be.
    const int width=label->getCharacterPosition(static_cast<int>(text.size())).x;
    const Point origin=box->localPosToCanvas(Point(0, 0));
    for(int x=-2; x <= width+2; ++x)
    {
        CAPTURE(x);
        const Point at(origin.x+label->getCharacterPosition(0).x+x, origin.y+box->getHeight()/2);

        scene.canvas->inputMouseMoved(at.x, at.y, 0, 0);
        scene.canvas->inputMouseButton(0, true);
        scene.canvas->inputMouseButton(0, false);
        scene.frame();

        box->insertText("|");
        const String &result=box->getText();
        CHECK(isValidUtf8(result));
        CHECK(result.size() == text.size()+1);
        box->setText(text);
        scene.frame();
    }
}

TEST_CASE("Text::getClosestCharacter only returns codepoint boundaries")
{
    Scene scene;
    auto *text=scene.canvas->newChild<controls::internal::Text>("text");
    const String str="h"+eAcute+euro+grin+"o";
    text->setText(str);
    scene.frame();

    const int width=text->getCharacterPosition(static_cast<unsigned>(str.size())).x;
    REQUIRE(width > 0);
    for(int x=-2; x <= width+2; ++x)
    {
        CAPTURE(x);
        const int pos=text->getClosestCharacter(Point(x, 0));
        CHECK(pos >= 0);
        CHECK(pos <= static_cast<int>(str.size()));
        CHECK(isBoundary(str, static_cast<size_t>(pos)));
    }
    CHECK(text->getClosestCharacter(Point(width+2, 0)) == static_cast<int>(str.size()));
}

TEST_CASE("TextBoxMultiline: Up and Down keep the codepoint column, and wrapped words stay whole")
{
    Scene scene;
    auto *box=scene.canvas->newChild<controls::TextBoxMultiline>("multi");
    box->setBounds(10, 10, 300, 200);
    box->setText(eAcute+eAcute+eAcute+"\nabcdef");
    scene.frame();
    box->focus();

    box->moveCaretToend();
    scene.key(GLFW_KEY_LEFT);
    scene.key(GLFW_KEY_LEFT);
    scene.key(GLFW_KEY_LEFT);
    scene.key(GLFW_KEY_LEFT);                   // ab | cdef
    scene.key(GLFW_KEY_UP);                     // éé | é: two codepoints in
    scene.type(U"x");
    CHECK(box->getText() == eAcute+eAcute+"x"+eAcute+"\nabcdef");

    scene.key(GLFW_KEY_DOWN);                   // three codepoints in: abc | def
    scene.type(U"y");
    CHECK(box->getText() == eAcute+eAcute+"x"+eAcute+"\nabcydef");

    scene.key(GLFW_KEY_UP);
    scene.key(GLFW_KEY_END);                    // before the '\n'
    scene.type(U"z");
    CHECK(box->getText() == eAcute+eAcute+"x"+eAcute+"z\nabcydef");

    // A word too wide for the box is broken between codepoints, never inside
    // one. The stub measures bytes, so the 30 wide text fits an odd 7 of them.
    box->setBounds(10, 10, 38, 200);
    String word;
    for(int i=0; i < 12; ++i)
        word+=eAcute;
    box->setText(word);
    scene.frame();

    controls::internal::Text *text=nullptr;
    for(controls::Base *child : box->getActualChildren())
        if(auto *t=dynamic_cast<controls::internal::Text *>(child))
            text=t;
    REQUIRE(text);
    REQUIRE(text->numLines() > 1);
    for(unsigned line=0; line < text->numLines(); ++line)
    {
        CAPTURE(line);
        CHECK(isValidUtf8(text->getLine(line)->getText()));
        CHECK(isBoundary(word, static_cast<size_t>(text->getStartCharFromLine(line))));
        CHECK(isBoundary(word, static_cast<size_t>(text->getLastCaretOnLine(line))));
    }

    // End on the first (wrapped) line puts the caret before its last é.
    box->moveCaretToStart();
    scene.key(GLFW_KEY_END);
    scene.type(U"|");
    CHECK(isValidUtf8(box->getText()));
}

namespace
{

//! Counts the calls of the accelerators it is hung on.
struct AcceleratorCount : event::Handler
{
    int calls=0;
    void hit(event::Info) { ++calls; }
};

//! One key typed as GLFW delivers it: its press, the char the layout makes of
//! it (none for 0), then its release, all with the same mods.
void typeKey(Scene &scene, int key, int mods, char32_t c)
{
    scene.glfwKey(key, GLFW_PRESS, mods);
    if(c)
        scene.glfw.processCharEvent(static_cast<unsigned int>(c));
    scene.glfwKey(key, GLFW_RELEASE, mods);
    scene.frame();
}

// The mods GLFW might report on the Right Alt press and release of AltGr on
// Windows. Every key under AltGr has Ctrl|Alt (GetKeyState(VK_CONTROL) sees
// the synthetic Left Ctrl, whose own events GLFW discards), but GLFW's source
// does not settle the bits on the Right Alt events themselves.
const int altGrPressMods[]={GLFW_MOD_CONTROL|GLFW_MOD_ALT, GLFW_MOD_CONTROL, 0};
const int altGrReleaseMods[]={GLFW_MOD_CONTROL|GLFW_MOD_ALT, GLFW_MOD_ALT, GLFW_MOD_CONTROL, 0};

}//namespace

TEST_CASE("AltGr on Windows types its character, fires no Ctrl accelerator and leaves Ctrl up")
{
    const struct { int key; char32_t c; String text; } altGrKeys[]=
    {
        {GLFW_KEY_Q, U'@', "@"},                    // German layout
        {GLFW_KEY_E, U'€', euro},
        {GLFW_KEY_M, U'µ', "\xC2\xB5"},
    };
    const int altGr=GLFW_MOD_CONTROL|GLFW_MOD_ALT;

    for(const auto &k : altGrKeys)
    for(int pressMods : altGrPressMods)
    for(int releaseMods : altGrReleaseMods)
    {
        CAPTURE(k.key);
        CAPTURE(pressMods);
        CAPTURE(releaseMods);

        AcceleratorCount accelerators;
        Scene scene;
        for(const char *accelerator : {"CTRL+Q", "CTRL+E", "CTRL+M", "Q", "E", "M"})
            scene.canvas->addAccelerator(accelerator, &AcceleratorCount::hit, &accelerators);
        controls::TextBox *box=scene.focusedBox();

        typeKey(scene, GLFW_KEY_A, 0, U'a');

        // AltGr down: Right Alt alone, no LEFT_CONTROL event.
        scene.glfwKey(GLFW_KEY_RIGHT_ALT, GLFW_PRESS, pressMods);
        typeKey(scene, k.key, altGr, k.c);
        scene.glfwKey(GLFW_KEY_RIGHT_ALT, GLFW_RELEASE, releaseMods);
        scene.frame();

        typeKey(scene, GLFW_KEY_B, 0, U'b');

        CHECK(box->getText() == "a"+k.text+"b");
        CHECK(accelerators.calls == 0);
        CHECK_FALSE(input::isControlDown());
        CHECK_FALSE(input::isKeyDown(keys::Alt));
    }
}

TEST_CASE("AltGr pressed and released alone leaves Ctrl up by the next key at the latest")
{
    // After the Right Alt release Ctrl is up, unless that release itself
    // reports Ctrl without Alt, which then counts as a held Ctrl. Either way
    // the next key's press (which GLFW delivers before its char) lets it go,
    // so the next character is typed.
    for(int pressMods : altGrPressMods)
    for(int releaseMods : altGrReleaseMods)
    {
        CAPTURE(pressMods);
        CAPTURE(releaseMods);

        Scene scene;
        controls::TextBox *box=scene.focusedBox();

        scene.glfwKey(GLFW_KEY_RIGHT_ALT, GLFW_PRESS, pressMods);
        scene.glfwKey(GLFW_KEY_RIGHT_ALT, GLFW_RELEASE, releaseMods);
        scene.frame();
        if(releaseMods != GLFW_MOD_CONTROL)
            CHECK_FALSE(input::isControlDown());

        scene.glfwKey(GLFW_KEY_B, GLFW_PRESS, 0);
        CHECK_FALSE(input::isControlDown());
        scene.glfw.processCharEvent('b');
        scene.glfwKey(GLFW_KEY_B, GLFW_RELEASE, 0);
        scene.frame();
        CHECK(box->getText() == "b");
    }
}

TEST_CASE("Real Ctrl shortcuts still work, with the mods X11 and Windows each report")
{
    // Windows and Wayland report a modifier's own bit on its press, X11 the
    // state before the event: without it on the press and with it on the
    // release. Ctrl+A, Ctrl+C, End, Ctrl+V doubles the text either way.
    for(bool x11 : {false, true})
    {
        CAPTURE(x11);
        Scene scene;
        controls::TextBox *box=scene.focusedBox();
        scene.type(U"x€");

        auto chord=[&](int key)
        {
            scene.glfwKey(GLFW_KEY_LEFT_CONTROL, GLFW_PRESS, x11 ? 0 : GLFW_MOD_CONTROL);
            CHECK(input::isControlDown());
            scene.glfwKey(key, GLFW_PRESS, GLFW_MOD_CONTROL);
            scene.glfwKey(key, GLFW_RELEASE, GLFW_MOD_CONTROL);
            scene.glfwKey(GLFW_KEY_LEFT_CONTROL, GLFW_RELEASE, x11 ? GLFW_MOD_CONTROL : 0);
            CHECK_FALSE(input::isControlDown());
            scene.frame();
        };

        chord(GLFW_KEY_A);
        chord(GLFW_KEY_C);
        CHECK(platform::getClipboardText() == "x"+euro);
        scene.key(GLFW_KEY_END);
        chord(GLFW_KEY_V);
        CHECK(box->getText() == "x"+euro+"x"+euro);

        typeKey(scene, GLFW_KEY_Y, 0, U'y');
        CHECK(box->getText() == "x"+euro+"x"+euro+"y");
    }
}

TEST_CASE("A Ctrl held across a keyboard-focus change is still held for the next shortcut")
{
    // The input handler lets go of every held key when the keyboard focus
    // moves; the next key's mods press Ctrl again.
    Scene scene;
    controls::TextBox *first=scene.focusedBox("first");
    controls::TextBox *second=scene.canvas->newChild<controls::TextBox>("second");
    second->setText("abc");
    scene.frame();

    scene.glfwKey(GLFW_KEY_LEFT_CONTROL, GLFW_PRESS, GLFW_MOD_CONTROL);
    second->focus();
    scene.frame();
    CHECK_FALSE(input::isControlDown());

    scene.glfwKey(GLFW_KEY_A, GLFW_PRESS, GLFW_MOD_CONTROL);
    CHECK(input::isControlDown());
    scene.glfwKey(GLFW_KEY_A, GLFW_RELEASE, GLFW_MOD_CONTROL);
    scene.glfwKey(GLFW_KEY_LEFT_CONTROL, GLFW_RELEASE, 0);
    scene.frame();
    CHECK(second->getSelection() == "abc");
    CHECK(first->getText().empty());
}
