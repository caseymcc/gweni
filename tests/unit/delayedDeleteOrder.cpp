/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Canvas::processDelayedDeletes deletes each queued control once, whatever
 *  order the queue holds them in (#518). A control queued after an ancestor
 *  (a parent, then its child and grandchild, all delayedDelete()d before the
 *  frame) is deleted with the ancestor and never again; a control queued and
 *  then deleted outright before the frame is not deleted again; a control
 *  that a queued control's destructor queues goes in the same frame. The
 *  double deletes are real checks only in the ASAN tree; the counts hold in
 *  every tree.
 */
#include "headlessFixture.h"

#include <doctest/doctest.h>

using namespace gweni;

namespace
{

//! Counts its deletes, and may queue another control from its destructor.
class Tracked: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Tracked, controls::Base)
    {}

    ~Tracked() override
    {
        if(m_deletes)
            ++*m_deletes;
        if(m_queueOnDelete)
            m_queueOnDelete->delayedDelete();
    }

    int *m_deletes=nullptr;
    controls::Base *m_queueOnDelete=nullptr;
};

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        delete canvas;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    Tracked *add(controls::Base *parent, int *deletes)
    {
        Tracked *control=parent->newChild<Tracked>();
        control->m_deletes=deletes;
        return control;
    }
};

}//namespace

TEST_CASE("a control queued after its ancestor is deleted once, with the ancestor")
{
    Scene scene;
    scene.frame();
    const size_t baseline=scene.canvas->getActualChildren().size();

    int parentDeletes=0, childDeletes=0, grandchildDeletes=0;
    Tracked *parent=scene.add(scene.canvas, &parentDeletes);
    Tracked *child=scene.add(parent, &childDeletes);
    Tracked *grandchild=scene.add(child, &grandchildDeletes);
    scene.frame();

    SUBCASE("parent, child, grandchild")
    {
        parent->delayedDelete();
        child->delayedDelete();
        grandchild->delayedDelete();
    }
    SUBCASE("parent, grandchild")
    {
        parent->delayedDelete();
        grandchild->delayedDelete();
    }
    SUBCASE("grandchild, parent, child")
    {
        grandchild->delayedDelete();
        parent->delayedDelete();
        child->delayedDelete();
    }
    scene.frame();

    CHECK(parentDeletes == 1);
    CHECK(childDeletes == 1);
    CHECK(grandchildDeletes == 1);
    CHECK(scene.canvas->getActualChildren().size() == baseline);
}

TEST_CASE("a queued control deleted outright before the frame is not deleted again")
{
    Scene scene;
    int deletes=0;
    Tracked *control=scene.add(scene.canvas, &deletes);
    scene.frame();

    control->delayedDelete();
    delete static_cast<controls::Base *>(control);
    CHECK(deletes == 1);
    scene.frame();
    CHECK(deletes == 1);
}

TEST_CASE("a control a queued control's destructor queues goes in the same frame")
{
    Scene scene;
    scene.frame();
    const size_t baseline=scene.canvas->getActualChildren().size();

    int firstDeletes=0, secondDeletes=0, thirdDeletes=0;
    Tracked *first=scene.add(scene.canvas, &firstDeletes);
    Tracked *second=scene.add(scene.canvas, &secondDeletes);
    Tracked *third=scene.add(scene.canvas, &thirdDeletes);
    first->m_queueOnDelete=second;
    second->m_queueOnDelete=third;
    scene.frame();

    first->delayedDelete();
    scene.frame();

    CHECK(firstDeletes == 1);
    CHECK(secondDeletes == 1);
    CHECK(thirdDeletes == 1);
    CHECK(scene.canvas->getActualChildren().size() == baseline);
}
