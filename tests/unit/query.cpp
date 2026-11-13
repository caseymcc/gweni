/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The standard dialogs (dialogs::message, confirm, prompt; query.h) on a
 *  real Canvas with a StubRenderer, driven through the canvas's input calls.
 *
 *  Results: each button's click gives its result (message OK -> Ok; confirm
 *  YesNo Yes -> Yes, No -> No; OkCancel OK -> Ok, Cancel -> Cancel; prompt
 *  OK -> Ok with the field's text, Cancel -> Cancel with ""), and Escape, the
 *  close button and close() give the cancel result (Ok, No, Cancel, Cancel
 *  with ""), each handler firing exactly once however often the dialog is
 *  closed or Escape is pressed after. Enter chooses the default button (OK,
 *  Yes), also from the prompt's single-line field, and the focused footer
 *  button when one is (Tab to No, Enter -> No); in a multiline TextBox in a
 *  dialog it is the box's newline and the dialog stays open.
 *
 *  Focus: a confirm opens with its default button focused, a prompt with its
 *  field focused and its whole initial text selected (so typing replaces it);
 *  Tab cycles through the field and the two buttons alone, forwards and
 *  backwards; closing gives the focus back to the app's TextBox before the
 *  handler runs (the handler sees it).
 *
 *  Escape goes to a Popup opened from the dialog first. A handler may open
 *  another dialog, which is then the canvas's top modal. A confirm opened from
 *  a large Dialog is modal to it (the large one's controls take no click); its
 *  close hands the input and the focus back to the large one's field. A
 *  dialog deleted while open, or with its canvas, calls no handler, and every
 *  dialog closed gives every primitive and text id back.
 */
#include "headlessFixture.h"

#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/dialogs/query.h>
#include <gweni/controls/popup.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/textBox.h>
#include <gweni/controls/windowButtons.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/primitiveIds.h>

#include <doctest/doctest.h>

#include <functional>
#include <string>
#include <vector>

using namespace gweni;
using dialogs::Buttons;
using dialogs::Dialog;
using dialogs::Result;

namespace
{

size_t livePrimitives()
{
    return primitiveIdCount()-releasedPrimitiveIdCount();
}

size_t liveTexts()
{
    return textIdCount()-releasedTextIdCount();
}

const char *name(Result result)
{
    switch(result)
    {
    case Result::Ok: return "Ok";
    case Result::Yes: return "Yes";
    case Result::No: return "No";
    case Result::Cancel: return "Cancel";
    }
    return "?";
}

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::TextBox *appText=nullptr;
    float clock=10.0f;
    std::vector<std::string> results;
    //! KeyboardFocus as each handler ran
    std::vector<controls::Base *> focusInHandler;

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(800, 600);

        appText=canvas->newChild<controls::TextBox>();
        appText->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        appText->setBounds(10, 560, 120, 20);
        frame();
        clickOn(appText);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        canvas->inputMouseButton(0, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    dialogs::ResultHandler record()
    {
        return [this](Result result, const String &text)
        {
            results.push_back(std::string(name(result))+":"+text);
            focusInHandler.push_back(KeyboardFocus);
        };
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void click(int x, int y)
    {
        clock+=1.0f;
        canvas->inputMouseMoved(x, y, 0, 0);
        canvas->inputMouseButton(0, true);
        canvas->inputMouseButton(0, false);
        frame();
    }

    void clickOn(controls::Base *control)
    {
        const Point at=control->localPosToCanvas();
        click(at.x+control->getWidth()/2, at.y+control->getHeight()/2);
    }

    void key(int code, bool shift=false)
    {
        if(shift)
            canvas->inputModifierKey(keys::Shift, true);
        canvas->inputModifierKey(code, true);
        canvas->inputModifierKey(code, false);
        if(shift)
            canvas->inputModifierKey(keys::Shift, false);
        frame();
    }

    void type(const char *text)
    {
        for(const char *c=text; *c; ++c)
            canvas->inputCharacter(static_cast<unsigned char>(*c));
        frame();
    }

    //! The footer button labelled \p label.
    static controls::Button *button(Dialog *dialog, const String &label)
    {
        for(controls::Base *child : dialog->getFooter()->getRow()->getChildren())
        {
            controls::Button *found=gweni_cast<controls::Button>(child);

            if(found && (found->getText() == label))
                return found;
        }
        return nullptr;
    }
};

using Opener=std::function<Dialog *(Scene &)>;
using Closer=std::function<void(Scene &, Dialog *)>;

Opener messageDialog() { return [](Scene &s) { return dialogs::message(s.canvas, "M", "Done.", s.record()); }; }
Opener yesNo() { return [](Scene &s) { return dialogs::confirm(s.canvas, "C", "Delete it?", Buttons::YesNo, s.record()); }; }
Opener okCancel() { return [](Scene &s) { return dialogs::confirm(s.canvas, "C", "Release it?", Buttons::OkCancel, s.record()); }; }
Opener promptDialog() { return [](Scene &s) { return dialogs::prompt(s.canvas, "P", "Name:", "initial", s.record()); }; }

Closer press(const char *label)
{
    return [label](Scene &s, Dialog *d)
    {
        controls::Button *button=Scene::button(d, label);
        REQUIRE(button != nullptr);
        s.clickOn(button);
    };
}
Closer escape() { return [](Scene &s, Dialog *) { s.key(keys::Escape); }; }
Closer enter() { return [](Scene &s, Dialog *) { s.key(keys::Return); }; }
Closer closeButton() { return [](Scene &s, Dialog *d) { s.clickOn(d->getCloseButton()); }; }
Closer closeCall() { return [](Scene &, Dialog *d) { d->close(); }; }

//! Opens with \p open, closes with \p close and checks the handler got
//! \p expected once, after the pop, with the focus back on the app's TextBox,
//! and that every id came back.
void check(const char *what, Opener open, Closer close, const std::string &expected)
{
    CAPTURE(what);
    Scene scene;
    REQUIRE(KeyboardFocus == scene.appText);
    const size_t primitives=livePrimitives();
    const size_t texts=liveTexts();

    Dialog *dialog=open(scene);
    scene.frame();
    REQUIRE(scene.canvas->topModal() == dialog);
    const size_t id=dialog->getControlId();

    close(scene, dialog);
    CHECK(scene.results == std::vector<std::string>{expected});

    // once: closing again and Escape do nothing
    if(controls::Base::getControlById(id))
        dialog->close();
    scene.key(keys::Escape);
    scene.frame();
    CHECK(scene.results == std::vector<std::string>{expected});
    REQUIRE(scene.focusInHandler.size() == 1);
    CHECK(scene.focusInHandler[0] == scene.appText);
    CHECK(scene.canvas->topModal() == nullptr);
    CHECK(controls::Base::getControlById(id) == nullptr);
    CHECK(livePrimitives() == primitives);
    CHECK(liveTexts() == texts);
}

}//namespace

TEST_CASE("Query: each button gives its result")
{
    check("message OK", messageDialog(), press("OK"), "Ok:");
    check("Yes", yesNo(), press("Yes"), "Yes:");
    check("No", yesNo(), press("No"), "No:");
    check("OK", okCancel(), press("OK"), "Ok:");
    check("Cancel", okCancel(), press("Cancel"), "Cancel:");
    check("prompt OK", promptDialog(), press("OK"), "Ok:initial");
    check("prompt Cancel", promptDialog(), press("Cancel"), "Cancel:");
}

TEST_CASE("Query: Escape, the close button and close() give the cancel result")
{
    for(const auto &way : std::vector<std::pair<const char *, Closer>>{{"escape", escape()},
        {"close button", closeButton()}, {"close()", closeCall()}})
    {
        CAPTURE(way.first);
        check("message", messageDialog(), way.second, "Ok:");
        check("YesNo", yesNo(), way.second, "No:");
        check("OkCancel", okCancel(), way.second, "Cancel:");
        check("prompt", promptDialog(), way.second, "Cancel:");
    }
}

TEST_CASE("Query: Enter chooses the default button")
{
    check("message", messageDialog(), enter(), "Ok:");
    check("YesNo", yesNo(), enter(), "Yes:");
    check("OkCancel", okCancel(), enter(), "Ok:");
    // from the prompt's focused field
    check("prompt", promptDialog(), enter(), "Ok:initial");
    // the focused footer button is the one Enter chooses: Tab from Yes wraps
    // to No
    check("focused No", yesNo(), [](Scene &s, Dialog *d)
    {
        REQUIRE(KeyboardFocus == Scene::button(d, "Yes"));
        s.key(keys::Tab);
        REQUIRE(KeyboardFocus == Scene::button(d, "No"));
        s.key(keys::Return);
    }, "No:");
}

TEST_CASE("Query: Enter in a multiline TextBox is the box's newline")
{
    Scene scene;
    Dialog *dialog=dialogs::confirm(scene.canvas, "C", "Notes:", Buttons::OkCancel, scene.record());
    controls::TextBoxMultiline *notes=dialog->getContent()->newChild<controls::TextBoxMultiline>();
    notes->setDock(DockPosition::Top);
    notes->setSizeFlags({SizeFlag::Expand, SizeFlag::Fixed});
    notes->setHeight(60);
    scene.frame();

    scene.clickOn(notes);
    REQUIRE(KeyboardFocus == notes);
    scene.type("a");
    scene.key(keys::Return);
    scene.type("b");
    CHECK(notes->getText() == "a\nb");
    CHECK(dialog->isOpen());
    CHECK(scene.results.empty());

    // out of the box, Enter is the dialog's again: Tab wraps to the first
    // footer button, Cancel, which it chooses
    scene.key(keys::Tab);
    REQUIRE(KeyboardFocus == Scene::button(dialog, "Cancel"));
    scene.key(keys::Return);
    CHECK(scene.results == std::vector<std::string>{"Cancel:"});
}

TEST_CASE("Query: the prompt opens with its field focused and its text selected")
{
    Scene scene;
    dialogs::PromptDialog *dialog=dialogs::prompt(scene.canvas, "P", "Name:", "initial", scene.record());
    scene.frame();

    controls::TextBox *field=dialog->getField();
    CHECK(KeyboardFocus == field);
    CHECK(field->getText() == "initial");
    CHECK(field->getSelection() == "initial");

    // typing replaces it, Enter gives it
    scene.type("new");
    CHECK(field->getText() == "new");
    scene.key(keys::Return);
    CHECK(scene.results == std::vector<std::string>{"Ok:new"});
    CHECK(KeyboardFocus == scene.appText);
}

TEST_CASE("Query: Tab stays inside the dialog, and the focus comes back on close")
{
    Scene scene;
    dialogs::PromptDialog *dialog=dialogs::prompt(scene.canvas, "P", "Name:", "x", scene.record());
    scene.frame();

    controls::Base *field=dialog->getField();
    controls::Base *cancel=Scene::button(dialog, "Cancel");
    controls::Base *ok=Scene::button(dialog, "OK");
    REQUIRE(KeyboardFocus == field);

    scene.key(keys::Tab);
    CHECK(KeyboardFocus == cancel);
    scene.key(keys::Tab);
    CHECK(KeyboardFocus == ok);
    scene.key(keys::Tab);
    CHECK(KeyboardFocus == field);
    scene.key(keys::Tab, true);
    CHECK(KeyboardFocus == ok);

    // a focused button: Space presses it
    scene.key(keys::Space);
    CHECK(scene.results == std::vector<std::string>{"Ok:x"});
    CHECK(KeyboardFocus == scene.appText);
}

TEST_CASE("Query: a confirm opens with its default button focused")
{
    Scene scene;
    Dialog *dialog=dialogs::confirm(scene.canvas, "C", "Sure?", Buttons::YesNo, scene.record());
    scene.frame();
    CHECK(KeyboardFocus == Scene::button(dialog, "Yes"));
    CHECK(KeyboardFocus == dialog->getDefaultButton());
    // and keeps it through a frame (it holds keyboard input)
    scene.frame();
    CHECK(KeyboardFocus == dialog->getDefaultButton());
}

TEST_CASE("Query: Escape goes to a Popup opened from the dialog first")
{
    Scene scene;
    Dialog *dialog=dialogs::confirm(scene.canvas, "C", "Sure?", Buttons::OkCancel, scene.record());
    scene.frame();

    controls::Popup *popup=dialog->createPopup();
    controls::Base *inside=popup->newChild<controls::Base>();
    inside->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    inside->setBounds(0, 0, 60, 30);
    popup->open(dialog->getDefaultButton());
    scene.frame();
    REQUIRE(scene.canvas->shownPopup() == popup);

    scene.key(keys::Escape);
    CHECK(scene.canvas->shownPopup() == nullptr);
    CHECK(dialog->isOpen());
    CHECK(scene.results.empty());

    scene.key(keys::Escape);
    CHECK(scene.results == std::vector<std::string>{"Cancel:"});
}

TEST_CASE("Query: a handler may open another dialog")
{
    Scene scene;
    Dialog *second=nullptr;

    dialogs::confirm(scene.canvas, "C", "Delete?", Buttons::YesNo, [&](Result result, const String &)
    {
        scene.results.push_back(name(result));
        second=dialogs::message(scene.canvas, "M", "Deleted.", scene.record());
    });
    scene.frame();
    scene.key(keys::Return);

    REQUIRE(second != nullptr);
    CHECK(scene.results == std::vector<std::string>{"Yes"});
    CHECK(scene.canvas->topModal() == second);
    CHECK(scene.canvas->modalCount() == 1);
    CHECK(KeyboardFocus == second->getDefaultButton());

    scene.key(keys::Return);
    CHECK(scene.results == std::vector<std::string>{"Yes", "Ok:"});
    CHECK(scene.canvas->topModal() == nullptr);
    CHECK(KeyboardFocus == scene.appText);
}

TEST_CASE("Query: a confirm over a large dialog is modal to it and hands it back")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    Dialog *large=canvas->newChild<Dialog>();
    large->setTitle("Task");
    large->setDialogSize(dialogs::Extent::units(1200), dialogs::Extent::fraction(0.92f));
    controls::TextBox *field=large->getContent()->newChild<controls::TextBox>();
    field->setDock(DockPosition::Top);
    int largePresses=0;
    controls::Button *forceRelease=large->addButton("Force release");
    struct Presses: public event::Handler
    {
        int *count;
        void pressed(event::Info) { ++*count; }
    } presses;
    presses.count=&largePresses;
    forceRelease->onPressCaller.add(&presses, &Presses::pressed);
    large->setResultHandler(scene.record());
    large->open();
    scene.frame();
    REQUIRE(canvas->topModal() == large);
    CHECK(large->getWidth() == 800);
    scene.clickOn(field);
    REQUIRE(KeyboardFocus == field);

    Dialog *sure=dialogs::confirm(canvas, "Force release", "Sure?", Buttons::YesNo, scene.record());
    scene.frame();
    CHECK(canvas->topModal() == sure);
    CHECK(canvas->modalCount() == 2);
    CHECK(sure->getZIndex() > large->getZIndex());

    // the large one is out of reach
    scene.clickOn(forceRelease);
    CHECK(largePresses == 0);
    field->focus();
    CHECK(KeyboardFocus == sure->getDefaultButton());

    // No: the large one is the top again, with its field focused
    scene.clickOn(Scene::button(sure, "No"));
    CHECK(scene.results == std::vector<std::string>{"No:"});
    CHECK(scene.focusInHandler.back() == field);
    CHECK(canvas->topModal() == large);
    CHECK(KeyboardFocus == field);
    scene.clickOn(forceRelease);
    CHECK(largePresses == 1);

    // and its Escape is its own
    scene.key(keys::Escape);
    CHECK(scene.results == std::vector<std::string>{"No:", "Cancel:"});
    CHECK(canvas->topModal() == nullptr);
    CHECK(KeyboardFocus == scene.appText);
}

TEST_CASE("Query: deleted while open, or with its canvas, no handler fires")
{
    int calls=0;
    auto count=[&](Result, const String &) { ++calls; };
    {
        Scene scene;
        Dialog *dialog=dialogs::confirm(scene.canvas, "C", "Sure?", Buttons::YesNo, count);
        scene.frame();
        delete dialog;
        CHECK(scene.canvas->topModal() == nullptr);
        scene.frame();
        CHECK(KeyboardFocus == scene.appText);

        dialogs::prompt(scene.canvas, "P", "Name:", "x", count)->delayedDelete();
        scene.frame();
        CHECK(scene.canvas->topModal() == nullptr);

        // the scene deletes its canvas with these open
        dialogs::message(scene.canvas, "M", "One", count);
        dialogs::confirm(scene.canvas, "C", "Two", Buttons::OkCancel, count);
        scene.frame();
        CHECK(scene.canvas->modalCount() == 2);
    }
    CHECK(calls == 0);
}
