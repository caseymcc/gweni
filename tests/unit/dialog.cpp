/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  dialogs::Dialog on a real Canvas with a StubRenderer, driven through the
 *  canvas's input calls.
 *
 *  Size and place: units and a fraction of the canvas (400 x 0.5 of 800x600
 *  is 400x300 at (200, 150)), its content's size by default, never larger
 *  than the canvas (1200 units on 800 is 800 at x 0), and centred and
 *  clamped again by the canvas's next layout after a resize and after a
 *  setScale with the canvas resized to match, with no request recorded by
 *  any pass. Its title bar drags it by the mouse's motion, a drag past the
 *  canvas's edge keeps it inside, and once dragged it stays where it was put
 *  (kept inside) across a resize instead of centring again; a drag frame
 *  lays out one pass that enters the canvas and the dialog alone. The footer is hidden until addButton() and shows the
 *  buttons left to right in the order added.
 *
 *  Opening shows it, makes it the canvas's child and its top modal; the app
 *  under it takes no click. Closing (its close button, Escape, setHidden(true),
 *  close() and close(result)) hides it, takes it off the modal stack, gives
 *  the focus back to the app's TextBox before the handler runs, calls the
 *  handler once with the result (the cancel result for all but close(result),
 *  setCancelResult's when set), then onClose, and deletes it with every
 *  primitive and text id back; setDeleteOnClose(false) keeps it to open
 *  again. A handler may open another dialog, which is then the top modal, or
 *  its own again, which is then not deleted.
 *  setClosable(false) hides the close button and Escape leaves it open.
 *  Deleting it while open, or deleting the canvas, calls no handler.
 */
#include "headlessFixture.h"

#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/dialogs/dialog.h>
#include <gweni/controls/label.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/textBox.h>
#include <gweni/controls/windowButtons.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/primitiveIds.h>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace gweni;
using dialogs::Dialog;
using dialogs::Extent;
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

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::Button *appButton=nullptr;
    controls::TextBox *appText=nullptr;
    int appPresses=0;
    float clock=10.0f;
    //! what the handlers saw
    std::vector<std::string> results;

    struct Presses: public event::Handler
    {
        int *count;
        void pressed(event::Info) { ++*count; }
    } presses;

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(800, 600);

        appButton=canvas->newChild<controls::Button>();
        appButton->setText("App");
        place(appButton, 10, 10, 80, 20);
        presses.count=&appPresses;
        appButton->onPressCaller.add(&presses, &Presses::pressed);

        appText=canvas->newChild<controls::TextBox>();
        place(appText, 10, 560, 120, 20);
        frame();
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

    static void place(controls::Base *control, int x, int y, int w, int h)
    {
        control->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        control->setBounds(x, y, w, h);
    }

    //! A dialog of \p width x \p height with a few Labels in its content,
    //! recording its result in results.
    Dialog *dialog(Extent width, Extent height, const char *name="d")
    {
        Dialog *made=canvas->newChild<Dialog>();

        made->setTitle(name);
        for(int i=0; i < 3; ++i)
        {
            controls::Label *label=made->getContent()->newChild<controls::Label>();
            label->setText("line");
            label->setDock(DockPosition::Top);
        }
        made->setDialogSize(width, height);
        made->setResultHandler([this, name](Result result, const String &text)
        {
            results.push_back(std::string(name)+":"+std::to_string(int(result))+":"+text);
        });
        return made;
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

    void key(int code)
    {
        canvas->inputModifierKey(code, true);
        canvas->inputModifierKey(code, false);
        frame();
    }

    static Rect bounds(controls::Base *control) { return control->getBounds(); }
};

bool same(const Rect &a, const Rect &b)
{
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

std::string rectText(const Rect &r)
{
    return std::to_string(r.x)+","+std::to_string(r.y)+" "+std::to_string(r.w)+"x"+std::to_string(r.h);
}

}//namespace

TEST_CASE("Dialog: sized in units, a fraction or its content, centred and kept inside the canvas")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    Dialog *dialog=scene.dialog(Extent::units(400), Extent::fraction(0.5f));
    dialog->setDeleteOnClose(false);
    dialog->open();
    scene.frame();
    CHECK(rectText(dialog->getBounds()) == "200,150 400x300");
    CHECK(canvas->getLayoutStats().requests == 0);

    // larger than the canvas: clamped to it
    dialog->setDialogSize(Extent::units(1200), Extent::fraction(0.92f));
    scene.frame();
    CHECK(rectText(dialog->getBounds()) == "0,24 800x552");

    // its content's size: the preferred size, centred
    dialog->setDialogSize(Extent::content(), Extent::content());
    scene.frame();
    const Size preferred=dialog->getPreferredSize();
    CHECK(dialog->getWidth() == preferred.width);
    CHECK(dialog->getHeight() == preferred.height);
    CHECK(dialog->getX() == (800-preferred.width)/2);
    CHECK(dialog->getY() == (600-preferred.height)/2);

    // a resize centres and clamps it again, the fraction following the canvas
    dialog->setDialogSize(Extent::units(400), Extent::fraction(0.5f));
    canvas->setSize(1000, 800);
    scene.frame();
    CHECK(rectText(dialog->getBounds()) == "300,200 400x400");
    canvas->setSize(300, 200);
    scene.frame();
    CHECK(rectText(dialog->getBounds()) == "0,50 300x100");
    CHECK(canvas->getLayoutStats().requests == 0);

    // a scale with the canvas resized to match, as a window's does
    canvas->setScale(2.0f);
    canvas->setSize(400, 300);
    scene.frame();
    CHECK(rectText(dialog->getBounds()) == "0,75 400x150");
    canvas->setScale(1.0f);
    canvas->setSize(800, 600);
    scene.frame();
    CHECK(rectText(dialog->getBounds()) == "200,150 400x300");

    // the canvas's next frame lays out nothing
    scene.frame();
    CHECK(canvas->getLayoutStats().passes == 0);
}

TEST_CASE("Dialog: the title bar drags it, never off the canvas, and it stays where it was put")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    Dialog *dialog=scene.dialog(Extent::units(400), Extent::units(300));
    dialog->open();
    scene.frame();
    REQUIRE(rectText(dialog->getBounds()) == "200,150 400x300");
    CHECK_FALSE(dialog->isUserPlaced());

    // pressed on the title bar, left of the close button
    const Point grab(dialog->getX()+50, dialog->getY()+10);
    canvas->inputMouseMoved(grab.x, grab.y, 0, 0);
    scene.frame();
    canvas->inputMouseButton(0, true);
    scene.frame();

    canvas->inputMouseMoved(grab.x+50, grab.y+30, 50, 30);
    scene.frame();
    CHECK(rectText(dialog->getBounds()) == "250,180 400x300");
    // a pure move: one pass entering the canvas and the dialog alone
    CHECK(canvas->getLayoutStats().passes == 1);
    CHECK(canvas->getLayoutStats().visited <= 8);

    // past the canvas's bottom right: kept inside
    canvas->inputMouseMoved(grab.x+900, grab.y+900, 850, 870);
    scene.frame();
    CHECK(rectText(dialog->getBounds()) == "400,300 400x300");
    // and past its top left
    canvas->inputMouseMoved(-500, -500, 0, 0);
    scene.frame();
    CHECK(rectText(dialog->getBounds()) == "0,0 400x300");

    canvas->inputMouseMoved(grab.x-100, grab.y-50, 0, 0);
    scene.frame();
    canvas->inputMouseButton(0, false);
    scene.frame();
    CHECK(dialog->isUserPlaced());
    CHECK(rectText(dialog->getBounds()) == "100,100 400x300");

    // a resize keeps it where it was put, inside the canvas
    canvas->setSize(1000, 800);
    scene.frame();
    CHECK(rectText(dialog->getBounds()) == "100,100 400x300");
    canvas->setSize(450, 350);
    scene.frame();
    CHECK(rectText(dialog->getBounds()) == "50,50 400x300");
    canvas->setSize(800, 600);
    scene.frame();
    CHECK(rectText(dialog->getBounds()) == "100,100 400x300");
}

TEST_CASE("Dialog: the footer shows its buttons in the order added")
{
    Scene scene;

    Dialog *dialog=scene.dialog(Extent::units(400), Extent::units(300));
    CHECK(dialog->getFooter()->hidden());

    controls::Button *first=dialog->addButton("First");
    controls::Button *second=dialog->addButton("Second");
    dialog->open();
    scene.frame();

    CHECK_FALSE(dialog->getFooter()->hidden());
    CHECK(first->localPosToCanvas().x < second->localPosToCanvas().x);
    // right-aligned inside the dialog, at its bottom
    const Point secondAt=second->localPosToCanvas();
    CHECK(secondAt.x+second->getWidth() > dialog->getX()+dialog->getWidth()-40);
    CHECK(secondAt.y > dialog->getY()+dialog->getHeight()-50);
    // the content is between the title bar and the footer
    controls::Base *content=dialog->getContent();
    CHECK(content->getY() >= dialogs::internal::DialogTitleBar::Height);
    CHECK(content->getY()+content->getHeight() <= dialog->getFooter()->getY());
}

TEST_CASE("Dialog: each way of closing calls the handler once, after the pop, and gives the ids back")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    scene.clickOn(scene.appText);
    REQUIRE(KeyboardFocus == scene.appText);
    const size_t primitives=livePrimitives();
    const size_t texts=liveTexts();

    struct Way
    {
        const char *name;
        std::function<void(Scene &, Dialog *)> close;
        int result;
    };
    const std::vector<Way> ways={
        {"button", [](Scene &s, Dialog *d) { s.clickOn(d->getCloseButton()); }, int(Result::Cancel)},
        {"escape", [](Scene &s, Dialog *) { s.key(keys::Escape); }, int(Result::Cancel)},
        {"hide", [](Scene &, Dialog *d) { d->setHidden(true); }, int(Result::Cancel)},
        {"close", [](Scene &, Dialog *d) { d->close(); }, int(Result::Cancel)},
        {"result", [](Scene &, Dialog *d) { d->close(Result::Yes); }, int(Result::Yes)},
    };

    for(const Way &way : ways)
    {
        CAPTURE(way.name);
        scene.results.clear();

        Dialog *dialog=scene.dialog(Extent::units(400), Extent::units(300), way.name);
        bool poppedFirst=false;
        bool focusBack=false;
        int closes=0;
        dialog->setResultHandler([&, dialog](Result result, const String &)
        {
            poppedFirst=(canvas->topModal() == nullptr) && dialog->hidden();
            focusBack=(KeyboardFocus == scene.appText);
            scene.results.push_back(std::string(way.name)+":"+std::to_string(int(result))+":");
        });
        struct Counter: public event::Handler
        {
            int *count;
            void closed(event::Info) { ++*count; }
        } counter;
        counter.count=&closes;
        dialog->onClose.add(&counter, &Counter::closed);

        dialog->open();
        scene.frame();
        REQUIRE(canvas->topModal() == dialog);
        CHECK(dialog->getParent() == static_cast<controls::Base *>(canvas));
        CHECK(KeyboardFocus == nullptr);

        // the app under it takes no click
        scene.click(30, 20);
        CHECK(scene.appPresses == 0);

        const size_t id=dialog->getControlId();
        const std::vector<std::string> expected{std::string(way.name)+":"+std::to_string(way.result)+":"};
        way.close(scene, dialog);
        CHECK(scene.results == expected);
        // a second close, and Escape, do nothing more
        if(controls::Base::getControlById(id))
            dialog->close();
        scene.key(keys::Escape);
        scene.frame();

        CHECK(scene.results == expected);
        CHECK(closes == 1);
        CHECK(poppedFirst);
        CHECK(focusBack);
        CHECK(canvas->topModal() == nullptr);
        CHECK(controls::Base::getControlById(id) == nullptr);
        CHECK(livePrimitives() == primitives);
        CHECK(liveTexts() == texts);
    }
}

TEST_CASE("Dialog: the cancel result, setClosable and setDeleteOnClose")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    Dialog *dialog=scene.dialog(Extent::units(400), Extent::units(300));
    dialog->setCancelResult(Result::Ok);
    dialog->setDeleteOnClose(false);
    dialog->open();
    scene.frame();
    scene.key(keys::Escape);
    CHECK(scene.results == std::vector<std::string>{"d:0:"});
    CHECK_FALSE(dialog->isOpen());

    // kept, and opened again: a new handler, the close button hidden, Escape
    // does nothing
    scene.results.clear();
    dialog->setResultHandler([&](Result result, const String &)
    {
        scene.results.push_back("again:"+std::to_string(int(result)));
    });
    dialog->setClosable(false);
    dialog->open();
    scene.frame();
    CHECK(dialog->isOpen());
    CHECK(dialog->getCloseButton()->hidden());
    scene.key(keys::Escape);
    CHECK(dialog->isOpen());
    CHECK(canvas->topModal() == dialog);
    dialog->close(Result::No);
    CHECK(scene.results == std::vector<std::string>{"again:2"});
}

TEST_CASE("Dialog: a handler may open another dialog")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    Dialog *first=scene.dialog(Extent::units(400), Extent::units(300), "first");
    Dialog *second=nullptr;
    first->setResultHandler([&](Result, const String &)
    {
        second=scene.dialog(Extent::units(200), Extent::units(100), "second");
        second->open();
    });
    first->open();
    scene.frame();
    first->close(Result::Ok);
    scene.frame();

    REQUIRE(second != nullptr);
    CHECK(canvas->topModal() == second);
    CHECK(canvas->modalCount() == 1);
    CHECK(rectText(second->getBounds()) == "300,250 200x100");
    scene.key(keys::Escape);
    CHECK(scene.results == std::vector<std::string>{"second:3:"});
    CHECK(canvas->topModal() == nullptr);
}

TEST_CASE("Dialog: a handler may open its own dialog again, which then stays")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    Dialog *dialog=scene.dialog(Extent::units(400), Extent::units(300), "again");
    const size_t id=dialog->getControlId();
    int calls=0;

    // a prompt's validation, say: the first result sends it back
    dialog->setResultHandler([&](Result, const String &)
    {
        ++calls;
        dialog->setResultHandler([&](Result, const String &) { ++calls; });
        dialog->open();
    });
    dialog->open();
    scene.frame();
    dialog->close(Result::Ok);
    scene.frame();
    scene.frame();

    REQUIRE(controls::Base::getControlById(id) == dialog);
    CHECK(dialog->isOpen());
    CHECK(dialog->visible());
    CHECK(canvas->topModal() == dialog);
    dialog->close(Result::Ok);
    scene.frame();
    CHECK(calls == 2);
    CHECK(controls::Base::getControlById(id) == nullptr);
}

TEST_CASE("Dialog: deleted while open, or with its canvas, it calls no handler")
{
    int calls=0;
    {
        Scene scene;
        Dialog *dialog=scene.dialog(Extent::units(400), Extent::units(300));
        dialog->setResultHandler([&](Result, const String &) { ++calls; });
        dialog->open();
        scene.frame();
        delete dialog;
        CHECK(scene.canvas->topModal() == nullptr);
        scene.frame();

        Dialog *delayed=scene.dialog(Extent::units(400), Extent::units(300));
        delayed->setResultHandler([&](Result, const String &) { ++calls; });
        delayed->open();
        scene.frame();
        delayed->delayedDelete();
        scene.frame();
        CHECK(scene.canvas->topModal() == nullptr);

        // the scene deletes its canvas with this one open
        Dialog *open=scene.dialog(Extent::units(400), Extent::units(300));
        open->setResultHandler([&](Result, const String &) { ++calls; });
        open->open();
        scene.frame();
    }
    CHECK(calls == 0);
}
