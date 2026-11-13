/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The canvas's modal stack (Canvas::pushModal, popModal, topModal) on a real
 *  Canvas with a StubRenderer, driven through the canvas's input calls.
 *
 *  Input under a modal: a move over an app Button leaves it unhovered (the
 *  canvas is hovered), a click on it presses nothing, the wheel reaches no
 *  ScrollControl under it and a right press opens no context menu, while a
 *  Button inside the modal hovers and presses; a press made before the push
 *  still ends with its release. The hover under the mouse when the modal
 *  comes up is hit-tested again at the end of that update phase, and the app
 *  control under the mouse is hovered again once the modal is popped. A
 *  canvas accelerator of the app's does not fire while a modal is up.
 *
 *  The keyboard: the push takes the focus from an app TextBox and the pop
 *  gives it back; focus() on an app control does nothing while the modal is
 *  up; Tab cycles through the modal's tabable controls alone, forwards and
 *  backwards, also when nothing had the focus; Escape with a Popup opened from
 *  the modal closes the Popup and leaves the modal up, and the next Escape
 *  closes a modal WindowControl (onWindowClosed once), the focus back on the
 *  app TextBox. A TextBox inside a modal still takes typed text.
 *
 *  Stacked: a second modal blocks the first; popping it hands the input and
 *  the focus back to the first. Order: the modals stay right after the app's
 *  children in stack order, below a Popup and a ComboBox's list opened from
 *  them, the overlays (addOverlay) and the ToolTip box, also after an app
 *  child was added to the canvas while they are up (they move after it),
 *  and a tool's canvas child (setExcludedFromStats, an Inspector's window)
 *  above them takes the mouse and the focus.
 *
 *  Leaving: a deleted modal leaves the stack and the focus comes back in the
 *  next update phase; a hidden one is popped by the next update phase; a
 *  canvas deleted with modals up deletes them with no report. WindowDemo's
 *  path: WindowControl::makeModal() / destroyModal() / setHidden(true) / its
 *  close button, no Modal control created.
 */
#include "headlessFixture.h"

#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/comboBox.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/popup.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/textBox.h>
#include <gweni/controls/toolTip.h>
#include <gweni/controls/windowControl.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <iterator>

using namespace gweni;

namespace
{

struct Scene: public event::Handler
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    //! the app: a Button and a TextBox
    controls::Button *appButton=nullptr;
    controls::TextBox *appText=nullptr;
    int appPresses=0;
    int modalPresses=0;
    float clock=10.0f;

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);

        appButton=canvas->newChild<controls::Button>();
        appButton->setText("App");
        place(appButton, 10, 10, 80, 20);
        appButton->onPressCaller.add(this, &Scene::appPressed);

        appText=canvas->newChild<controls::TextBox>();
        appText->setText("app");
        place(appText, 10, 250, 120, 20);
        frame();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        canvas->inputMouseButton(0, false);
        canvas->inputMouseButton(1, false);
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

    void appPressed(event::Info) { ++appPresses; }
    void modalPressed(event::Info) { ++modalPresses; }

    //! A modal panel at (100, 50, 200, 150) holding a Button and two
    //! TextBoxes, pushed.
    struct Modal
    {
        controls::Base *panel;
        controls::Button *button;
        controls::TextBox *first;
        controls::TextBox *second;
    };

    Modal modal(int x=100, int y=50)
    {
        Modal made;

        made.panel=canvas->newChild<controls::Base>();
        place(made.panel, x, y, 200, 150);
        made.panel->setMouseInputEnabled(true);
        made.button=made.panel->newChild<controls::Button>();
        made.button->setText("OK");
        place(made.button, 10, 10, 60, 20);
        made.button->onPressCaller.add(this, &Scene::modalPressed);
        made.first=made.panel->newChild<controls::TextBox>();
        place(made.first, 10, 40, 100, 20);
        made.second=made.panel->newChild<controls::TextBox>();
        place(made.second, 10, 70, 100, 20);
        canvas->pushModal(made.panel);
        frame();
        return made;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void move(int x, int y)
    {
        canvas->inputMouseMoved(x, y, 0, 0);
        frame();
    }

    void click(int x, int y, int button=0)
    {
        // a second apart per press, so two presses are never a double click
        clock+=1.0f;
        canvas->inputMouseMoved(x, y, 0, 0);
        canvas->inputMouseButton(button, true);
        canvas->inputMouseButton(button, false);
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

    //! The index of \p control among the canvas's children.
    int position(controls::Base *control) const
    {
        const auto &children=canvas->getActualChildren();
        auto found=std::find(children.begin(), children.end(), control);
        REQUIRE(found != children.end());
        return int(std::distance(children.begin(), found));
    }
};

//! Counts a Caller's calls.
struct Counter: public event::Handler
{
    void count(event::Info) { ++calls; }
    int calls=0;
};

}//namespace

TEST_CASE("Modal stack: the mouse reaches the top modal alone")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    // the app Button hovers and presses before
    scene.move(30, 20);
    CHECK(HoveredControl == scene.appButton);
    scene.click(30, 20);
    CHECK(scene.appPresses == 1);

    // a ScrollControl and a context menu owner in the app
    controls::ScrollControl *scroll=canvas->newChild<controls::ScrollControl>();
    Scene::place(scroll, 300, 200, 80, 60);
    controls::Base *content=scroll->newChild<controls::Base>();
    Scene::place(content, 0, 0, 80, 600);
    scroll->createContextMenu()->addItem("Item");
    scene.frame();
    scene.move(330, 220);
    CHECK(canvas->modalAllows(HoveredControl));

    // resting on the app Button as the modal comes up: the hover leaves it
    // by the end of that update phase
    scene.move(30, 20);
    REQUIRE(HoveredControl == scene.appButton);

    Scene::Modal modal=scene.modal();

    CHECK(canvas->topModal() == modal.panel);
    CHECK(canvas->modalCount() == 1);
    CHECK(HoveredControl == static_cast<controls::Base *>(canvas));
    CHECK_FALSE(canvas->modalAllows(scene.appButton));
    CHECK_FALSE(canvas->modalAllows(canvas));
    CHECK(canvas->modalAllows(modal.button));

    // a move and a click over the app Button: nothing
    scene.move(31, 21);
    CHECK(HoveredControl == static_cast<controls::Base *>(canvas));
    CHECK(canvas->getControlAt(30, 20) == static_cast<controls::Base *>(canvas));
    scene.click(30, 20);
    CHECK(scene.appPresses == 1);

    // the wheel over the ScrollControl scrolls nothing, a right press opens
    // no context menu
    scene.move(330, 220);
    CHECK(HoveredControl == static_cast<controls::Base *>(canvas));
    CHECK_FALSE(canvas->inputMouseWheel(-60));
    scene.click(330, 220, 1);
    CHECK(canvas->shownContextMenu() == nullptr);

    // inside the modal: its Button hovers and presses
    scene.move(140, 70);
    CHECK(HoveredControl == modal.button);
    scene.click(140, 70);
    CHECK(scene.modalPresses == 1);

    // popped: the app Button under a resting mouse is hovered again
    scene.move(30, 20);
    CHECK(HoveredControl == static_cast<controls::Base *>(canvas));
    canvas->popModal(modal.panel);
    scene.frame();
    CHECK(canvas->topModal() == nullptr);
    CHECK(HoveredControl == scene.appButton);
    scene.click(30, 20);
    CHECK(scene.appPresses == 2);
}

TEST_CASE("Modal stack: a press made before the push ends with its release")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    // the app Button pressed, then a modal comes up over it
    canvas->inputMouseMoved(30, 20, 0, 0);
    canvas->inputMouseButton(0, true);
    scene.frame();
    REQUIRE(scene.appButton->isDepressed());

    Scene::Modal modal=scene.modal(150, 100);

    canvas->inputMouseButton(0, false);
    scene.frame();
    CHECK_FALSE(scene.appButton->isDepressed());
    CHECK(MouseFocus == nullptr);
    CHECK(canvas->topModal() == modal.panel);
}

TEST_CASE("Modal stack: the keyboard focus stays inside the top modal")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    scene.clickOn(scene.appText);
    REQUIRE(KeyboardFocus == scene.appText);

    Scene::Modal modal=scene.modal();

    // the push took the focus; the app cannot take it back
    CHECK(KeyboardFocus == nullptr);
    scene.appText->focus();
    CHECK(KeyboardFocus == nullptr);
    scene.clickOn(scene.appText);
    CHECK(KeyboardFocus == nullptr);

    // Tab with no focus goes to the modal and cycles inside it
    scene.key(keys::Tab);
    controls::Base *first=KeyboardFocus;
    REQUIRE(first != nullptr);
    CHECK(canvas->modalAllows(first));

    std::vector<controls::Base *> seen{first};

    for(int i=0; i < 6; ++i)
    {
        scene.key(keys::Tab);
        REQUIRE(KeyboardFocus != nullptr);
        CHECK(canvas->modalAllows(KeyboardFocus));
        CHECK(KeyboardFocus != scene.appText);
        CHECK(KeyboardFocus != scene.appButton);
        seen.push_back(KeyboardFocus);
    }
    // the two text boxes, round and round
    CHECK(seen[0] != seen[1]);
    for(size_t i=2; i < seen.size(); ++i)
        CHECK(seen[i] == seen[i-2]);
    CHECK(((seen[0] == modal.first) || (seen[0] == modal.second)));
    CHECK(((seen[1] == modal.first) || (seen[1] == modal.second)));

    // backwards too
    controls::Base *at=KeyboardFocus;
    scene.key(keys::Tab, true);
    CHECK(canvas->modalAllows(KeyboardFocus));
    scene.key(keys::Tab);
    CHECK(KeyboardFocus == at);

    // a TextBox in the modal takes text
    scene.clickOn(modal.first);
    REQUIRE(KeyboardFocus == modal.first);
    canvas->inputCharacter('x');
    CHECK(modal.first->getText() == "x");
    CHECK(scene.appText->getText() == "app");

    // the pop gives the focus back
    canvas->popModal(modal.panel);
    CHECK(KeyboardFocus == scene.appText);
}

TEST_CASE("Modal stack: Escape closes a Popup opened from the modal first, then the modal window")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    scene.clickOn(scene.appText);
    REQUIRE(KeyboardFocus == scene.appText);

    controls::WindowControl *window=canvas->newChild<controls::WindowControl>();
    window->setTitle("Modal");
    Scene::place(window, 80, 40, 240, 200);
    Counter closed;
    window->onWindowClosed.add(&closed, &Counter::count);
    controls::Button *opener=window->newChild<controls::Button>();
    opener->setDock(DockPosition::Top);
    window->makeModal();
    scene.frame();
    CHECK(window->isModal());
    CHECK(canvas->topModal() == window);

    controls::Popup *popup=window->createPopup();
    controls::Base *inPopup=popup->newChild<controls::Base>();
    Scene::place(inPopup, 0, 0, 60, 30);
    popup->open(opener);
    scene.frame();
    REQUIRE(canvas->shownPopup() == popup);
    // above the modal, and it takes the mouse there
    CHECK(scene.position(popup) > scene.position(window));
    CHECK(canvas->modalAllows(inPopup));

    scene.key(keys::Escape);
    CHECK(canvas->shownPopup() == nullptr);
    CHECK(canvas->topModal() == window);
    CHECK(closed.calls == 0);

    scene.key(keys::Escape);
    CHECK(closed.calls == 1);
    CHECK(canvas->topModal() == nullptr);
    CHECK(window->hidden());
    CHECK(KeyboardFocus == scene.appText);

    // the app is back
    scene.click(30, 20);
    CHECK(scene.appPresses == 1);
}

TEST_CASE("Modal stack: a modal window's close button, destroyModal and setHidden pop it")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;
    const size_t children=canvas->getActualChildren().size();

    controls::WindowControl *window=canvas->newChild<controls::WindowControl>();
    Scene::place(window, 80, 40, 240, 200);
    window->makeModal();
    window->makeModal();
    scene.frame();
    CHECK(canvas->modalCount() == 1);
    // no Modal control: the window is the canvas's child, and nothing else
    CHECK(window->getParent() == static_cast<controls::Base *>(canvas));
    CHECK(canvas->getActualChildren().size() == children+1);

    window->destroyModal();
    CHECK_FALSE(window->isModal());
    window->makeModal();
    window->setHidden(true);
    CHECK_FALSE(window->isModal());

    window->setHidden(false);
    window->makeModal();
    scene.frame();
    controls::Base *close=window->findChildByName("closeButton", true);
    REQUIRE(close != nullptr);
    scene.clickOn(close);
    CHECK_FALSE(window->isModal());
    CHECK(window->hidden());

    // closable(false): Escape does not close it
    window->setHidden(false);
    window->setClosable(false);
    window->makeModal();
    scene.frame();
    scene.key(keys::Escape);
    CHECK(window->isModal());
}

TEST_CASE("Modal stack: stacked modals hand the input and the focus back down")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    Scene::Modal lower=scene.modal(20, 20);

    scene.clickOn(lower.second);
    REQUIRE(KeyboardFocus == lower.second);

    Scene::Modal upper=scene.modal(180, 120);

    CHECK(canvas->topModal() == upper.panel);
    CHECK(canvas->modalCount() == 2);
    CHECK(KeyboardFocus == nullptr);
    CHECK(scene.position(upper.panel) == scene.position(lower.panel)+1);

    // the lower modal's Button is out of reach
    scene.move(lower.button->localPosToCanvas().x+5, lower.button->localPosToCanvas().y+5);
    CHECK(HoveredControl == static_cast<controls::Base *>(canvas));
    scene.clickOn(lower.button);
    CHECK(scene.modalPresses == 0);
    lower.first->focus();
    CHECK(KeyboardFocus == nullptr);

    scene.clickOn(upper.button);
    CHECK(scene.modalPresses == 1);

    // popped, and hidden as a dialog closing is: the lower one is the top
    upper.panel->hide();
    canvas->popModal(upper.panel);
    CHECK(canvas->topModal() == lower.panel);
    CHECK(KeyboardFocus == lower.second);
    scene.frame();
    scene.clickOn(lower.button);
    CHECK(scene.modalPresses == 2);
    // the app is still out of reach
    scene.click(30, 20);
    CHECK(scene.appPresses == 0);
}

TEST_CASE("Modal stack: the modals stay between the app and what is above them")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    // a tooltip box and an overlay already on the canvas
    scene.appButton->setToolTipText("tip");
    scene.move(30, 20);
    scene.clock+=1.0f;
    scene.frame();
    REQUIRE(canvas->toolTipControl() != nullptr);
    controls::Base *overlay=canvas->newChild<controls::Base>();
    Scene::place(overlay, 0, 280, 40, 20);
    canvas->addOverlay(overlay);
    scene.frame();

    Scene::Modal modal=scene.modal();

    CHECK(canvas->toolTipControl()->hidden());
    CHECK(scene.position(modal.panel) > scene.position(scene.appText));
    CHECK(scene.position(modal.panel) < scene.position(overlay));
    CHECK(scene.position(modal.panel) < scene.position(canvas->toolTipControl()));

    // the app adds a child to the canvas: the modal goes over it again
    controls::Base *late=canvas->newChild<controls::Base>();
    Scene::place(late, 100, 50, 200, 150);
    late->setMouseInputEnabled(true);
    scene.frame();
    CHECK(scene.position(modal.panel) > scene.position(late));
    CHECK(scene.position(modal.panel) < scene.position(overlay));
    CHECK(modal.panel->getZIndex() > late->getZIndex());
    scene.move(140, 70);
    CHECK(HoveredControl == modal.button);

    // a ComboBox's list opened from the modal is above it
    controls::ComboBox *combo=modal.panel->newChild<controls::ComboBox>();
    Scene::place(combo, 10, 100, 100, 20);
    combo->addItem("one");
    combo->addItem("two");
    scene.frame();
    combo->openList();
    scene.frame();
    controls::Base *list=nullptr;
    for(controls::Base *child : canvas->getActualChildren())
    {
        if(gweni_cast<controls::Menu>(child) && !child->hidden())
            list=child;
    }
    REQUIRE(list != nullptr);
    CHECK(scene.position(list) > scene.position(modal.panel));
    CHECK(canvas->modalAllows(list));

    // a tool's canvas child above the modal takes the mouse and the focus
    controls::TextBox *tool=canvas->newChild<controls::TextBox>();
    Scene::place(tool, 300, 260, 90, 20);
    tool->setExcludedFromStats(true);
    scene.frame();
    CHECK(scene.position(tool) > scene.position(modal.panel));
    CHECK(canvas->modalAllows(tool));
    scene.clickOn(tool);
    CHECK(KeyboardFocus == tool);
}

TEST_CASE("Modal stack: a deleted modal gives the focus back, a hidden one is popped")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    scene.clickOn(scene.appText);
    REQUIRE(KeyboardFocus == scene.appText);

    Scene::Modal modal=scene.modal();
    scene.clickOn(modal.first);
    REQUIRE(KeyboardFocus == modal.first);

    delete modal.panel;
    CHECK(canvas->topModal() == nullptr);
    CHECK(KeyboardFocus == nullptr);
    CHECK(canvas->nextWorkTime() == controls::Canvas::WorkNow);
    scene.frame();
    CHECK(KeyboardFocus == scene.appText);

    Scene::Modal second=scene.modal();
    second.panel->delayedDelete();
    scene.frame();
    CHECK(canvas->topModal() == nullptr);
    CHECK(KeyboardFocus == scene.appText);

    Scene::Modal third=scene.modal();
    CHECK(KeyboardFocus == nullptr);
    third.panel->hide();
    scene.frame();
    CHECK(canvas->topModal() == nullptr);
    CHECK(KeyboardFocus == scene.appText);
}

TEST_CASE("Modal stack: the app's canvas accelerators do not fire under a modal")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;
    Counter fired;

    scene.appButton->addAccelerator("CTRL+K", &Counter::count, &fired);
    canvas->inputModifierKey(keys::Control, true);
    canvas->inputCharacter('k');
    canvas->inputModifierKey(keys::Control, false);
    REQUIRE(fired.calls == 1);

    Scene::Modal modal=scene.modal();
    Counter inModal;
    modal.button->addAccelerator("CTRL+K", &Counter::count, &inModal);

    canvas->inputModifierKey(keys::Control, true);
    canvas->inputCharacter('k');
    canvas->inputModifierKey(keys::Control, false);
    CHECK(fired.calls == 1);
    CHECK(inModal.calls == 1);
}

TEST_CASE("Modal stack: a canvas deleted with modals up")
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=new controls::Canvas(&skin);
    canvas->setSize(300, 200);

    controls::Base *lower=canvas->newChild<controls::Base>();
    controls::WindowControl *upper=canvas->newChild<controls::WindowControl>();
    canvas->pushModal(lower);
    upper->makeModal();
    canvas->updateCanvas();
    canvas->renderCanvas();
    CHECK(canvas->modalCount() == 2);

    delete canvas;
    HoveredControl=nullptr;
    KeyboardFocus=nullptr;
    MouseFocus=nullptr;
}
