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
 *  that a queued control's destructor queues goes in the same frame, and one
 *  queued after it that the destructor deletes outright (not its descendant)
 *  is deleted once and never touched again (#532). A ComboBox whose list was
 *  never opened, delayedDelete()d and then cleared (clearItems() queues every
 *  item of the list, the box's child, behind the box) leaves no Menu on the
 *  canvas and gives back every primitive and text id it took (#532). The
 *  double deletes are real checks only in the ASAN tree; the counts hold in
 *  every tree.
 */
#include "headlessFixture.h"

#include <gweni/controls/comboBox.h>
#include <gweni/controls/menu.h>

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
        if(m_deleteOnDelete)
            delete m_deleteOnDelete;
    }

    int *m_deletes=nullptr;
    controls::Base *m_queueOnDelete=nullptr;
    controls::Base *m_deleteOnDelete=nullptr;
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

TEST_CASE("a queued control a queued control's destructor deletes outright is deleted once")
{
    Scene scene;
    scene.frame();
    const size_t baseline=scene.canvas->getActualChildren().size();

    int firstDeletes=0, secondDeletes=0, thirdDeletes=0;
    Tracked *first=scene.add(scene.canvas, &firstDeletes);
    Tracked *second=scene.add(scene.canvas, &secondDeletes);
    Tracked *third=scene.add(scene.canvas, &thirdDeletes);
    scene.frame();

    SUBCASE("a sibling queued after it")
    {
        first->m_deleteOnDelete=second;
        first->delayedDelete();
        second->delayedDelete();
        third->delayedDelete();
    }
    SUBCASE("a sibling queued after it, the sibling deleting the next")
    {
        first->m_deleteOnDelete=second;
        second->m_deleteOnDelete=third;
        first->delayedDelete();
        third->delayedDelete();
        second->delayedDelete();
    }
    SUBCASE("a child of another control, queued after it")
    {
        int childDeletes=0;
        Tracked *child=scene.add(third, &childDeletes);
        scene.frame();

        first->m_deleteOnDelete=child;
        first->delayedDelete();
        second->delayedDelete();
        child->delayedDelete();
        third->delayedDelete();
        scene.frame();
        CHECK(childDeletes == 1);
    }
    scene.frame();

    CHECK(firstDeletes == 1);
    CHECK(secondDeletes == 1);
    CHECK(thirdDeletes == 1);
    CHECK(scene.canvas->getActualChildren().size() == baseline);
}

TEST_CASE("a ComboBox delayedDelete()d and then cleared, its list never opened, goes once with its items")
{
    Scene scene;
    scene.frame();
    const size_t baseline=scene.canvas->getActualChildren().size();
    const test::IdBankCounts before=test::IdBankCounts::now();

    controls::ComboBox *combo=scene.canvas->newChild<controls::ComboBox>();
    combo->addItem("One");
    combo->addItem("Two");
    combo->addItem("Three");
    scene.frame();
    CHECK(test::IdBankCounts::now().livePrimitives() > before.livePrimitives());

    combo->delayedDelete();
    combo->clearItems();
    scene.frame();

    size_t menus=0;

    for(auto &&child : scene.canvas->getActualChildren())
    {
        if(gweni_cast<controls::Menu>(child))
            ++menus;
    }
    CHECK(menus == 0);
    CHECK(scene.canvas->getActualChildren().size() == baseline);

    const test::IdBankCounts after=test::IdBankCounts::now();
    CHECK(after.livePrimitives() == before.livePrimitives());
    CHECK(after.liveTexts() == before.liveTexts());
}
