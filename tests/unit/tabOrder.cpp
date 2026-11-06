/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Tab and Shift+Tab move the keyboard focus through Base::nextTabable: the
 *  tabable, enabled, visible controls of the focus holder's canvas in child
 *  order, wrapping at either end. A hidden or disabled parent takes its
 *  subtree out of the order, a control on another canvas is never chosen, and
 *  with no other tabable control the focus stays put. Return in a single-line
 *  TextBox moves the focus the same way before it fires onReturnPressed.
 */
#include "headlessFixture.h"

#include <gweni/controls/textBox.h>
#include <gweni/inputHandler.h>

#include <doctest/doctest.h>

#include <string>
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
        // The focus, the key state and the repeat target are process-global.
        input::onKeyEvent(canvas, keys::Shift, false);
        input::onKeyEvent(canvas, keys::Tab, false);
        input::onKeyEvent(canvas, keys::Return, false);
        KeyboardFocus=nullptr;
        delete canvas;
    }

    controls::Base *tabable(controls::Base *parent, const char *name)
    {
        controls::Base *control=parent->newChild<controls::Base>(name);
        control->setTabable(true);
        return control;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void key(int key)
    {
        input::onKeyEvent(canvas, key, true);
        input::onKeyEvent(canvas, key, false);
    }

    //! The focus holder's name after each of `count` Tab presses.
    std::vector<std::string> tabs(int count)
    {
        std::vector<std::string> names;
        for(int i=0; i < count; ++i)
        {
            key(keys::Tab);
            names.push_back(KeyboardFocus ? KeyboardFocus->getName() : "(none)");
        }
        return names;
    }
};

//! A tree with every case the walk has to skip:
//!   a
//!   group           (not tabable)
//!     b
//!     plain         (not tabable)
//!   off             (tabable, disabled)
//!   hiddenParent    (hidden)
//!     underHidden   (tabable)
//!   disabledParent  (disabled)
//!     underDisabled (tabable)
//!   c
//!     d             (a tabable control's child)
struct TabTree: Scene
{
    controls::Base *a, *b, *c, *d;

    TabTree()
    {
        a=tabable(canvas, "a");
        controls::Base *group=canvas->newChild<controls::Base>("group");
        b=tabable(group, "b");
        group->newChild<controls::Base>("plain");
        tabable(canvas, "off")->setDisabled(true);
        controls::Base *hiddenParent=canvas->newChild<controls::Base>("hiddenParent");
        tabable(hiddenParent, "underHidden");
        hiddenParent->hide();
        controls::Base *disabledParent=canvas->newChild<controls::Base>("disabledParent");
        tabable(disabledParent, "underDisabled");
        disabledParent->setDisabled(true);
        c=tabable(canvas, "c");
        d=tabable(c, "d");
        frame();
    }
};

using Names=std::vector<std::string>;

} // namespace

TEST_CASE("Tab walks the tabable controls in child order and wraps to the first")
{
    TabTree tree;
    tree.a->focus();

    CHECK(tree.tabs(5) == Names{"b", "c", "d", "a", "b"});
}

TEST_CASE("Shift+Tab walks the same order backwards and wraps to the last")
{
    TabTree tree;
    tree.a->focus();

    input::onKeyEvent(tree.canvas, keys::Shift, true);
    REQUIRE(input::isShiftDown());
    CHECK(tree.tabs(5) == Names{"d", "c", "b", "a", "d"});
    input::onKeyEvent(tree.canvas, keys::Shift, false);

    // and forward again once Shift is up
    CHECK(tree.tabs(1) == Names{"a"});
}

TEST_CASE("nextTabable skips controls that are hidden, disabled, or under a hidden or disabled parent")
{
    TabTree tree;
    controls::Base *off=tree.canvas->findChildByName("off", true);
    controls::Base *underHidden=tree.canvas->findChildByName("underHidden", true);
    controls::Base *underDisabled=tree.canvas->findChildByName("underDisabled", true);
    REQUIRE(off);
    REQUIRE(underHidden);
    REQUIRE(underDisabled);

    CHECK(tree.b->nextTabable() == tree.c);
    CHECK(tree.c->nextTabable(true) == tree.b);

    // From a control outside the order, the walk goes on from its place.
    CHECK(off->nextTabable() == tree.c);
    CHECK(underHidden->nextTabable() == tree.c);
    CHECK(underDisabled->nextTabable(true) == tree.b);

    // Showing the parent puts its child back in the order.
    underHidden->getParent()->show();
    CHECK(tree.b->nextTabable() == underHidden);

    // A hidden control is out of the order and so is its subtree.
    tree.c->hide();
    tree.a->focus();
    CHECK(tree.tabs(3) == Names{"b", "underHidden", "a"});
}

TEST_CASE("Tab with no other tabable control leaves the focus where it is")
{
    Scene scene;
    controls::Base *only=scene.tabable(scene.canvas, "only");
    scene.canvas->newChild<controls::Base>("plain");
    scene.tabable(scene.canvas, "off")->setDisabled(true);
    scene.frame();

    CHECK(only->nextTabable() == nullptr);
    CHECK(only->nextTabable(true) == nullptr);

    only->focus();
    CHECK(scene.tabs(2) == Names{"only", "only"});

    input::onKeyEvent(scene.canvas, keys::Shift, true);
    CHECK(scene.tabs(1) == Names{"only"});
}

TEST_CASE("Tab never moves the focus to a control on another canvas")
{
    Scene first;
    Scene second;
    controls::Base *a=first.tabable(first.canvas, "a");
    controls::Base *b=first.tabable(first.canvas, "b");
    second.tabable(second.canvas, "elsewhere1");
    second.tabable(second.canvas, "elsewhere2");
    first.frame();
    second.frame();

    CHECK(a->nextTabable() == b);
    CHECK(b->nextTabable() == a);
    CHECK(b->nextTabable(true) == a);

    a->focus();
    CHECK(first.tabs(3) == Names{"b", "a", "b"});
}

TEST_CASE("Return in a TextBox moves the focus to the next control, then fires onReturnPressed")
{
    Scene scene;
    controls::TextBox *box=scene.canvas->newChild<controls::TextBox>("box");
    controls::Base *next=scene.tabable(scene.canvas, "next");
    scene.frame();

    controls::Base *focusAtReturn=box;
    int returns=0;
    box->onReturnPressedCaller.addCallback(scene.canvas,
        [&](event::Handler &, event::Info const &)
        {
            ++returns;
            focusAtReturn=KeyboardFocus;
        });

    box->focus();
    scene.key(keys::Return);
    CHECK(returns == 1);
    CHECK(focusAtReturn == next);
    CHECK(KeyboardFocus == next);

    // With nowhere to go, Return unfocuses the box and still fires.
    next->setDisabled(true);
    box->focus();
    scene.key(keys::Return);
    CHECK(returns == 2);
    CHECK(focusAtReturn == nullptr);
    CHECK(KeyboardFocus == nullptr);
}
