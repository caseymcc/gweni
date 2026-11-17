/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  An input method's composition in a TextBox (TextBox::setComposition,
 *  commitComposition, cancelComposition, through Canvas::inputComposition*),
 *  on a real Canvas with the headless stub. The composing text shows in the
 *  box's Text at the caret, the text after it moved along, or in place of the
 *  selection, with the caret where the input method puts it and an
 *  underline (getCompositionBounds()) under exactly the composing glyphs; it
 *  is not in getText() and fires no onTextChanged, and an empty box's
 *  placeholder stays hidden under it. A commit leaves the text, caret, Text
 *  and undo stack typing the same text would (one onTextChanged, one undo
 *  step), the selection replaced as typing replaces it, with its events. A cancel, an empty
 *  update and Escape give back the text, caret and selection it began from,
 *  with no event. A key (Left) or a character during a composition commits
 *  it first, the focus leaving commits it, and the app's setText() drops it.
 *  A PasswordTextBox shows none and types the commit. A focus that takes no
 *  composition (a plain control) gets a commit as characters. The platform's
 *  text input (platform::setTextInputRect) is asked for as a box takes the
 *  focus, follows its caret in device px at the canvas's scale, and is let
 *  go as the focus leaves or the focused box is deleted; a box that takes no
 *  edits asks for none, and setEditable(false) commits what was composed and
 *  lets it go. A composition the box ends itself (a key, a caret move, the
 *  focus leaving, through unfocus() or another control's focus(), setText)
 *  ends the platform's too, once,
 *  (platform::endTextComposition), one the platform ended does not; a
 *  modifier key, the release of a key pressed before the composition and a
 *  held key's repeat commit nothing. A
 *  finger down on the canvas (its touch gesture holding the input) keeps no
 *  composition from the focused box.
 */
#include "headlessFixture.h"

#include <gweni/controls/textBox.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/inputEvent.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace gweni;

namespace
{

const String nihongo="\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E";     // 日本語, 9 bytes

//! A box whose placeholder shows while it is focused (a CommandPalette's field).
class FocusedHintBox: public controls::TextBox
{
public:
    GWENI_CONTROL_INLINE(FocusedHintBox, controls::TextBox) {}

protected:
    bool placeholderWhileFocused() const override { return true; }
};

//! Writes down the characters it gets.
class CharTarget: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(CharTarget, controls::Base)
    {
        setKeyboardInputEnabled(true);
    }

    bool onChar(UnicodeChar c) override
    {
        chars.push_back(c);
        return true;
    }

    std::vector<UnicodeChar> chars;
};

struct TextInput
{
    bool active=false;
    Rect rect;
    platform::TextInputKind kind=platform::TextInputKind::Text;
    int calls=0;
};

struct Scene: public event::Handler
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::TextBox *box=nullptr;
    int changes=0;
    TextInput input;
    //! platform::endTextComposition() calls
    int ends=0;

    Scene()
    {
        platform::setTextInputHandler([this](bool active, const Rect &rect, platform::TextInputKind kind)
            {
                input.active=active;
                input.rect=rect;
                input.kind=kind;
                ++input.calls;
            });
        platform::setTextCompositionEndHandler([this]() { ++ends; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);

        box=canvas->newChild<controls::TextBox>();
        box->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        box->setBounds(10, 10, 300, 24);
        box->setText("abcdef");
        box->onTextChangedCaller.add(this, &Scene::changed);
        frame();
        box->focus();
        box->setCursorPos(3);
        box->setCursorend(3);
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
        platform::setTextCompositionEndHandler(nullptr);
    }

    void changed(event::Info) { ++changes; }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    const String &shown() const { return box->getTextControl()->getText(); }
};

}//namespace

TEST_CASE("TextBox composition: the composing text shows at the caret, not in getText(), with no event")
{
    Scene scene;
    controls::TextBox *box=scene.box;

    CHECK(scene.canvas->inputCompositionUpdate("xy", 1));
    CHECK(box->isComposing());
    CHECK(box->getComposition() == "xy");
    CHECK(scene.shown() == "abcxydef");
    CHECK(box->getText() == "abcdef");
    CHECK(box->getCursorPos() == 4);
    CHECK(box->getCursorEnd() == 4);
    CHECK(scene.changes == 0);
    CHECK(!box->getUndoStack().canUndo());
    scene.frame();

    // the underline is under the composing glyphs, on the line's last row
    const Rect start=box->getCharacterPosition(3);
    const Rect end=box->getCharacterPosition(5);
    const Rect &underline=box->getCompositionBounds();

    CHECK(underline.x == start.x);
    CHECK(underline.w == end.x-start.x);
    CHECK(underline.y == start.y+start.h-1);
    CHECK(underline.h == 1);

    // a longer update moves the text after it along
    scene.canvas->inputCompositionUpdate(nihongo, 9);
    CHECK(scene.shown() == "abc"+nihongo+"def");
    CHECK(box->getCursorPos() == 12);
    CHECK(box->getText() == "abcdef");
    CHECK(scene.changes == 0);
}

TEST_CASE("TextBox composition: a commit is what typing the text gives, one change and one undo step")
{
    Scene scene;
    controls::TextBox *box=scene.box;

    scene.canvas->inputCompositionUpdate("x", 1);
    scene.canvas->inputCompositionUpdate(nihongo, 9);
    CHECK(scene.canvas->inputCompositionCommit(nihongo));
    CHECK(!box->isComposing());
    CHECK(box->getText() == "abc"+nihongo+"def");
    CHECK(scene.shown() == box->getText());
    CHECK(box->getCursorPos() == 12);
    CHECK(box->getCursorEnd() == 12);
    CHECK(scene.changes == 1);
    CHECK(box->getCompositionBounds().w == 0);

    // the same as typed
    Scene typed;

    typed.box->insertText(nihongo);
    CHECK(typed.box->getText() == box->getText());
    CHECK(typed.box->getCursorPos() == box->getCursorPos());
    CHECK(typed.box->getCaretBounds().x == box->getCaretBounds().x);

    // one step, back to the text it began from
    CHECK(box->undo());
    CHECK(box->getText() == "abcdef");
    CHECK(box->getCursorPos() == 3);
}

TEST_CASE("TextBox composition: it replaces the selection, and a cancel gives the selection back")
{
    Scene scene;
    controls::TextBox *box=scene.box;

    box->setCursorPos(4);
    box->setCursorend(1);
    scene.frame();
    scene.canvas->inputCompositionUpdate("Z", 1);
    CHECK(scene.shown() == "aZef");
    CHECK(box->getText() == "abcdef");
    CHECK(box->getCursorPos() == 2);

    CHECK(scene.canvas->inputCompositionCancel());
    CHECK(!box->isComposing());
    CHECK(scene.shown() == "abcdef");
    CHECK(box->getCursorPos() == 4);
    CHECK(box->getCursorEnd() == 1);
    CHECK(scene.changes == 0);

    // committed, it replaces the selection as typing does, with the same
    // events (the erase and the insert)
    scene.canvas->inputCompositionUpdate("Z", 1);
    scene.canvas->inputCompositionCommit("Z");
    CHECK(box->getText() == "aZef");

    Scene typed;

    typed.box->setCursorPos(4);
    typed.box->setCursorend(1);
    typed.canvas->inputCharacter('Z');
    CHECK(typed.box->getText() == "aZef");
    CHECK(scene.changes == typed.changes);
    CHECK(box->getCursorPos() == typed.box->getCursorPos());
}

TEST_CASE("TextBox composition: Escape and an empty update cancel it")
{
    Scene scene;
    controls::TextBox *box=scene.box;

    scene.canvas->inputCompositionUpdate("xy", 2);
    CHECK(scene.canvas->inputModifierKey(keys::Escape, true));
    scene.canvas->inputModifierKey(keys::Escape, false);
    CHECK(!box->isComposing());
    CHECK(scene.shown() == "abcdef");
    CHECK(box->getCursorPos() == 3);
    // the box still has the focus: Escape went to the composition alone
    CHECK(KeyboardFocus == box);

    scene.canvas->inputCompositionUpdate("xy", 2);
    scene.canvas->inputCompositionUpdate("", 0);
    CHECK(!box->isComposing());
    CHECK(scene.shown() == "abcdef");
    CHECK(scene.changes == 0);
}

TEST_CASE("TextBox composition: a key, a character or the focus leaving commits it, setText drops it")
{
    Scene scene;
    controls::TextBox *box=scene.box;

    scene.canvas->inputCompositionUpdate("xy", 2);
    scene.canvas->inputModifierKey(keys::Left, true);
    scene.canvas->inputModifierKey(keys::Left, false);
    CHECK(!box->isComposing());
    CHECK(box->getText() == "abcxydef");
    CHECK(box->getCursorPos() == 4);

    scene.canvas->inputCompositionUpdate("q", 1);
    scene.canvas->inputCharacter('!');
    CHECK(box->getText() == "abcxq!ydef");

    scene.canvas->inputCompositionUpdate("w", 1);
    box->unfocus();
    CHECK(!box->isComposing());
    CHECK(box->getText() == "abcxq!wydef");

    box->focus();
    scene.canvas->inputCompositionUpdate("v", 1);
    box->setText("new");
    CHECK(!box->isComposing());
    CHECK(box->getText() == "new");
    CHECK(scene.shown() == "new");
}

TEST_CASE("TextBox composition: an empty box's placeholder stays hidden under it")
{
    Scene scene;
    auto *box=scene.canvas->newChild<FocusedHintBox>();

    box->setBounds(10, 50, 200, 24);
    box->setPlaceholder("hint");
    scene.frame();
    box->focus();
    scene.frame();
    REQUIRE(box->isPlaceholderShown());

    scene.canvas->inputCompositionUpdate("k", 1);
    CHECK(box->getText().empty());
    CHECK(box->getTextControl()->getText() == "k");
    CHECK(!box->isPlaceholderShown());

    scene.canvas->inputCompositionCancel();
    CHECK(box->isPlaceholderShown());
}

TEST_CASE("TextBox composition: a password shows none and types the commit")
{
    Scene scene;
    auto *password=scene.canvas->newChild<controls::PasswordTextBox>();

    password->setBounds(10, 50, 200, 24);
    scene.frame();
    password->focus();
    scene.canvas->inputCompositionUpdate("xy", 2);
    CHECK(!password->isComposing());
    CHECK(password->getText().empty());
    scene.canvas->inputCompositionCommit("xy");
    CHECK(password->getText() == "xy");
}

TEST_CASE("TextBox composition: a focus that takes no composition gets the commit as characters")
{
    Scene scene;
    auto *target=scene.canvas->newChild<CharTarget>();

    target->setBounds(10, 80, 50, 20);
    scene.frame();
    target->focus();
    scene.canvas->inputCompositionUpdate(nihongo, 9);
    scene.canvas->inputCompositionCommit(nihongo);

    const std::vector<UnicodeChar> expected{0x65E5, 0x672C, 0x8A9E};

    CHECK(target->chars == expected);
}

TEST_CASE("TextBox composition: the platform's text input follows the focus and the caret")
{
    Scene scene;
    controls::TextBox *box=scene.box;

    REQUIRE(scene.input.active);

    const Point origin=box->localPosToCanvas(Point(box->getCaretBounds().x, box->getCaretBounds().y));

    CHECK(scene.input.rect.x == origin.x);
    CHECK(scene.input.rect.y == origin.y);
    CHECK(scene.input.rect.h == box->getCaretBounds().h);

    // the caret moved: the rect with it
    const int x=scene.input.rect.x;

    box->setCursorPos(5);
    CHECK(scene.input.active);
    CHECK(scene.input.rect.x > x);

    box->unfocus();
    CHECK(!scene.input.active);

    // in device px at the canvas's scale
    scene.canvas->setScale(2.0f);
    scene.frame();
    box->focus();
    scene.frame();
    REQUIRE(scene.input.active);

    const Point at=box->localPosToCanvas(Point(box->getCaretBounds().x, box->getCaretBounds().y));

    CHECK(scene.input.rect.x == 2*at.x);
    CHECK(scene.input.rect.y == 2*at.y);
    CHECK(scene.input.rect.h == 2*box->getCaretBounds().h);

    // deleted with the focus, it lets the input go
    box->delayedDelete();
    scene.frame();
    CHECK(!scene.input.active);
}

TEST_CASE("TextBox composition: one the box ends itself ends the platform's, one the platform ends does not")
{
    Scene scene;
    controls::TextBox *box=scene.box;

    // the platform's own end, commit, cancel or empty update: nothing back
    scene.canvas->inputCompositionUpdate("xy", 2);
    scene.canvas->inputCompositionCommit("xy");
    scene.canvas->inputCompositionUpdate("q", 1);
    scene.canvas->inputCompositionCancel();
    scene.canvas->inputCompositionUpdate("q", 1);
    scene.canvas->inputCompositionUpdate("", 0);
    CHECK(box->getText() == "abcxydef");
    CHECK(scene.ends == 0);

    // a key
    scene.canvas->inputCompositionUpdate("q", 1);
    scene.canvas->inputModifierKey(keys::Left, true);
    scene.canvas->inputModifierKey(keys::Left, false);
    CHECK(!box->isComposing());
    CHECK(scene.ends == 1);

    // the caret moved (a tap elsewhere in the box)
    scene.canvas->inputCompositionUpdate("w", 1);
    box->setCursorPos(0);
    CHECK(!box->isComposing());
    CHECK(scene.ends == 2);

    // the focus leaving
    scene.canvas->inputCompositionUpdate("v", 1);
    box->unfocus();
    CHECK(scene.ends == 3);

    // the app's setText
    box->focus();
    scene.canvas->inputCompositionUpdate("u", 1);
    box->setText("new");
    CHECK(scene.ends == 4);

    // nothing composing: nothing to end
    box->setCursorPos(1);
    box->setText("newer");
    CHECK(scene.ends == 4);
}

TEST_CASE("TextBox composition: another control taking the focus ends the platform's once and lets the keyboard go")
{
    Scene scene;
    controls::TextBox *box=scene.box;
    controls::Base *other=scene.canvas->newChild<controls::Base>();

    other->setKeyboardInputEnabled(true);
    scene.canvas->inputCompositionUpdate("v", 1);
    // Base::focus() tells the box while the focus is still its own
    other->focus();
    CHECK(KeyboardFocus == other);
    CHECK(!box->isComposing());
    CHECK(box->getText() == "abcvdef");
    CHECK(scene.ends == 1);
    CHECK(!scene.input.active);
}

TEST_CASE("TextBox composition: a modifier, a release and a held key's repeat commit nothing")
{
    Scene scene;
    controls::TextBox *box=scene.box;
    double clock=100.0;

    platform::setTimeSource([&clock]() { return clock; });
    // a key pressed before the composition, released during it (at once:
    // held past the repeat delay, its repeat would commit)
    scene.canvas->inputModifierKey(keys::Left, true);
    scene.canvas->inputCompositionUpdate("xy", 2);
    scene.canvas->inputModifierKey(keys::Left, false);
    CHECK(box->isComposing());
    for(const int key: {keys::Shift, keys::Control, keys::Alt, keys::Command})
    {
        scene.canvas->inputModifierKey(key, true);
        CHECK(box->isComposing());
    }
    // Shift held: past the repeat delay it repeats, every 30 ms
    for(int i=0; i < 40; ++i)
    {
        clock+=0.03;
        scene.frame();
    }
    CHECK(box->isComposing());
    for(const int key: {keys::Shift, keys::Control, keys::Alt, keys::Command})
        scene.canvas->inputModifierKey(key, false);
    CHECK(box->isComposing());
    CHECK(box->getComposition() == "xy");
    CHECK(scene.ends == 0);
    platform::setTimeSource(nullptr);
}

TEST_CASE("TextBox composition: a box that takes no edits asks for no keyboard")
{
    Scene scene;
    controls::TextBox *box=scene.box;

    REQUIRE(scene.input.active);
    box->unfocus();
    box->setEditable(false);
    box->focus();
    CHECK(!scene.input.active);
    box->setCursorPos(1);
    CHECK(!scene.input.active);

    // editable again, focused: the keyboard comes back
    box->setEditable(true);
    CHECK(scene.input.active);

    // made read-only mid-composition: what was composed goes in (over the
    // selection, 1..3, as typing), the composition and the keyboard go
    scene.canvas->inputCompositionUpdate("xy", 2);
    box->setEditable(false);
    CHECK(!box->isComposing());
    CHECK(box->getText() == "axydef");
    CHECK(!scene.input.active);
    CHECK(scene.ends == 1);
    scene.canvas->inputCompositionUpdate("z", 1);
    CHECK(!box->isComposing());
}

TEST_CASE("TextBox composition: a finger down on the canvas keeps no composition from the box")
{
    Scene scene;
    controls::TextBox *box=scene.box;

    // a finger on the bare canvas: its touch gesture holds the input
    scene.canvas->inputTouch(0, TouchPhase::Down, 350.0f, 250.0f);
    REQUIRE(scene.canvas->inputCapture() != nullptr);
    scene.canvas->inputCompositionUpdate("xy", 2);
    CHECK(box->getComposition() == "xy");
    scene.canvas->inputCompositionCancel();
    CHECK(!box->isComposing());
    scene.canvas->inputCompositionUpdate("q", 1);
    scene.canvas->inputCompositionCommit(nihongo);
    CHECK(box->getText() == "abc"+nihongo+"def");
    CHECK(!box->isComposing());
    scene.canvas->inputTouch(0, TouchPhase::Up, 350.0f, 250.0f);
    CHECK(scene.ends == 0);
}

TEST_CASE("TextBox composition: a password asks for a password's input, a box for text")
{
    Scene scene;
    auto *password=scene.canvas->newChild<controls::PasswordTextBox>();

    password->setBounds(10, 50, 200, 24);
    scene.frame();
    CHECK(scene.input.active);
    CHECK(scene.input.kind == platform::TextInputKind::Text);
    password->focus();
    CHECK(scene.input.active);
    CHECK(scene.input.kind == platform::TextInputKind::Password);
    scene.box->focus();
    CHECK(scene.input.kind == platform::TextInputKind::Text);
}

// A phone's keyboard composes every word, and a password box shows none of
// it: the canvas types the word still composing as the focus leaves, and
// ends the platform's composition, which would give it to the next control.
TEST_CASE("TextBox composition: a password's word still composing goes in as another control takes the focus")
{
    Scene scene;
    auto *password=scene.canvas->newChild<controls::PasswordTextBox>();
    controls::Base *other=scene.canvas->newChild<controls::Base>();

    other->setKeyboardInputEnabled(true);
    password->setBounds(10, 50, 200, 24);
    scene.frame();
    password->focus();
    scene.canvas->inputCompositionUpdate("se", 2);
    scene.canvas->inputCompositionUpdate("secret", 6);
    CHECK(password->getText().empty());
    other->focus();
    CHECK(KeyboardFocus == other);
    CHECK(password->getText() == "secret");
    CHECK(scene.ends == 1);
    // the platform's late commit goes nowhere it would be typed again
    scene.canvas->inputCompositionCommit("secret");
    CHECK(password->getText() == "secret");
}

TEST_CASE("TextBox composition: a password's word still composing goes in as it is unfocused")
{
    Scene scene;
    auto *password=scene.canvas->newChild<controls::PasswordTextBox>();

    password->setBounds(10, 50, 200, 24);
    scene.frame();
    password->focus();
    scene.canvas->inputCompositionUpdate("secret", 6);
    password->unfocus();
    CHECK(password->getText() == "secret");
    CHECK(scene.ends == 1);
}

TEST_CASE("TextBox composition: a word the platform committed or cancelled is not typed again as the focus leaves")
{
    Scene scene;
    auto *password=scene.canvas->newChild<controls::PasswordTextBox>();

    password->setBounds(10, 50, 200, 24);
    scene.frame();
    password->focus();
    scene.canvas->inputCompositionUpdate("se", 2);
    scene.canvas->inputCompositionCommit("secret");
    scene.canvas->inputCompositionUpdate("x", 1);
    scene.canvas->inputCompositionCancel();
    password->unfocus();
    CHECK(password->getText() == "secret");
    CHECK(scene.ends == 0);
}

TEST_CASE("TextBox composition: a control deleted with a word composing for it ends the platform's")
{
    Scene scene;
    auto *box=scene.canvas->newChild<controls::TextBox>();
    auto *target=scene.canvas->newChild<CharTarget>();

    // a box deleted mid-composition
    box->setBounds(10, 50, 200, 24);
    scene.frame();
    box->focus();
    scene.canvas->inputCompositionUpdate("v", 1);
    REQUIRE(box->isComposing());
    delete box;
    CHECK(scene.ends == 1);
    CHECK(KeyboardFocus == nullptr);

    // a control that showed none of it: nothing is typed, the platform's ends
    target->setBounds(10, 80, 50, 20);
    scene.frame();
    target->focus();
    scene.canvas->inputCompositionUpdate("ab", 2);
    delete target;
    CHECK(scene.ends == 2);
    CHECK(KeyboardFocus == nullptr);
}
