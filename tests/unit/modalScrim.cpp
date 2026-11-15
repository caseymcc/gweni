/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Canvas::setModalScrim on a real Canvas with a StubRenderer: off by default,
 *  and then a pushed modal makes no scrim and leaves the canvas's children as
 *  they were. On, the scrim (one internal::ModalScrim child) is hidden while
 *  no modal is up; with one up it is shown over the whole canvas, its size
 *  following the canvas's, right before the top modal in the children (so
 *  numbered just under it): after the app's children and the modals below
 *  the top one, before the top one, and it moves as a second modal is pushed
 *  and popped, hiding when the last goes. It takes no mouse: the hit test
 *  outside the top modal never finds it. Turned off with a modal up, it is
 *  deleted by the next update and the modals stay right after the app.
 */
#include "headlessFixture.h"

#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/modalScrim.h>

#include <doctest/doctest.h>

#include <vector>

using namespace gweni;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::Button *app=nullptr;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
        app=canvas->newChild<controls::Button>();
        place(app, 10, 10, 80, 20);
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
    }

    static void place(controls::Base *control, int x, int y, int w, int h)
    {
        control->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        control->setBounds(x, y, w, h);
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    controls::Base *modal(int x)
    {
        controls::Base *control=canvas->newChild<controls::Base>();

        place(control, x, 100, 100, 80);
        return control;
    }

    //! the canvas's shown children, in order
    std::vector<controls::Base *> shown() const
    {
        std::vector<controls::Base *> children;

        for(controls::Base *child : canvas->getChildren())
        {
            if(!child->hidden())
                children.push_back(child);
        }
        return children;
    }

    controls::Base *scrim() const
    {
        for(controls::Base *child : canvas->getChildren())
        {
            if(gweni_cast<controls::internal::ModalScrim>(child))
                return child;
        }
        return nullptr;
    }
};

}//namespace

TEST_CASE("Modal scrim: off by default, a modal makes none")
{
    Scene scene;
    controls::Base *a=scene.modal(100);

    CHECK_FALSE(scene.canvas->modalScrim());
    scene.canvas->pushModal(a);
    scene.frame();
    CHECK(scene.scrim() == nullptr);
    CHECK((scene.shown() == std::vector<controls::Base *>{scene.app, a}));
}

TEST_CASE("Modal scrim: just under the top modal, over the whole canvas, as modals stack and pop")
{
    Scene scene;
    controls::Base *a=scene.modal(100);
    controls::Base *b=scene.modal(200);

    scene.canvas->setModalScrim(true);
    scene.frame();
    REQUIRE(scene.scrim() != nullptr);
    controls::Base *scrim=scene.scrim();

    // no modal: hidden
    CHECK(scrim->hidden());
    CHECK_FALSE(scrim->getMouseInputEnabled());

    scene.canvas->pushModal(a);
    scene.frame();
    CHECK((scene.shown() == std::vector<controls::Base *>{scene.app, b, scrim, a}));
    CHECK((scrim->getBounds() == Rect(0, 0, 400, 300)));
    CHECK(scrim->getZIndex() < a->getZIndex());
    CHECK(scrim->getZIndex() > b->getZIndex());

    // the hit test outside the modal finds no scrim
    CHECK(scene.canvas->getControlAt(5, 5) != scrim);
    CHECK(scene.canvas->getControlAt(50, 15) != scrim);

    // the canvas resized: the scrim follows
    scene.canvas->setSize(500, 320);
    scene.frame();
    CHECK((scrim->getBounds() == Rect(0, 0, 500, 320)));

    scene.canvas->pushModal(b);
    scene.frame();
    CHECK((scene.shown() == std::vector<controls::Base *>{scene.app, a, scrim, b}));

    scene.canvas->popModal(b);
    scene.frame();
    // b is an app child now, which the modal goes after
    CHECK((scene.shown() == std::vector<controls::Base *>{scene.app, b, scrim, a}));

    scene.canvas->popModal(a);
    scene.frame();
    CHECK(scrim->hidden());
}

TEST_CASE("Modal scrim: turned off with a modal up, it goes")
{
    Scene scene;
    controls::Base *a=scene.modal(100);

    scene.canvas->setModalScrim(true);
    scene.canvas->pushModal(a);
    scene.frame();
    REQUIRE(scene.scrim() != nullptr);

    scene.canvas->setModalScrim(false);
    CHECK_FALSE(scene.canvas->modalScrim());
    scene.frame();
    CHECK(scene.scrim() == nullptr);
    CHECK((scene.shown() == std::vector<controls::Base *>{scene.app, a}));
}
