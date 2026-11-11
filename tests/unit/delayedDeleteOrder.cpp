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
 *  canvas and gives back every primitive and text id it took (#532).
 *
 *  The queue survives a control leaving the canvas that queued it (#575): a
 *  queued control detached (or under a detached ancestor) and then deleted
 *  outright, or moved onto a second canvas and deleted there, is taken off
 *  the queuing canvas's queue and deleted once; one detached and left alone
 *  is still deleted by that canvas's next frame, and one moved to a second
 *  canvas by the canvas that queued it, never by the other. A control is
 *  queued once: delayedDelete() again after a move to a second canvas leaves
 *  it queued on the first only. A canvas deleted with queued controls off its
 *  tree (detached, or on another canvas, which then runs a frame and is
 *  deleted cleanly) deletes each of them once. delayedDelete() on a control
 *  with no canvas deletes it at once, without preDelete(), so a detached
 *  layout::Table shrunk, given a row less or cleared, a detached Properties
 *  cleared and a detached ComboBox cleared do not crash (clear() and
 *  clearItems() walk a copy of the list the deletes take their rows out of),
 *  and a MenuItem deleted with a deleteOnClose() submenu off any canvas
 *  deletes it once, in close(), and touches it no more. The double deletes
 *  and freed reads are real checks only in the ASAN tree; the counts hold in
 *  every tree.
 */
#include "headlessFixture.h"

#include <gweni/controls/comboBox.h>
#include <gweni/controls/layout/table.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/properties.h>

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

    void preDelete(skin::Base *skin) override
    {
        if(m_preDeletes)
            ++*m_preDeletes;
    }

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
    int *m_preDeletes=nullptr;
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

//! Two canvases on one skin, for a control moved from one to the other.
struct TwoCanvases
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *a;
    controls::Canvas *b;

    TwoCanvases()
    {
        a=new controls::Canvas(&skin);
        a->setSize(640, 480);
        b=new controls::Canvas(&skin);
        b->setSize(640, 480);
    }

    TwoCanvases(const TwoCanvases &)=delete;
    TwoCanvases &operator=(const TwoCanvases &)=delete;

    ~TwoCanvases()
    {
        delete a;
        delete b;
    }

    static void frame(controls::Canvas *canvas)
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void frames()
    {
        frame(a);
        frame(b);
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

TEST_CASE("a queued control detached and then deleted outright is deleted once")
{
    Scene scene;
    int deletes=0;
    Tracked *control=scene.add(scene.canvas, &deletes);
    scene.frame();

    control->delayedDelete();
    control->setParent(nullptr);
    delete static_cast<controls::Base *>(control);
    CHECK(deletes == 1);
    CHECK_FALSE(scene.canvas->isPendingDelete(control));
    scene.frame();
    CHECK(deletes == 1);
}

TEST_CASE("a queued control whose parent is detached and deleted outright goes once, with it")
{
    Scene scene;
    scene.frame();
    const size_t baseline=scene.canvas->getActualChildren().size();

    int parentDeletes=0, childDeletes=0;
    Tracked *parent=scene.add(scene.canvas, &parentDeletes);
    Tracked *child=scene.add(parent, &childDeletes);
    scene.frame();

    child->delayedDelete();
    parent->setParent(nullptr);
    delete static_cast<controls::Base *>(parent);
    CHECK(parentDeletes == 1);
    CHECK(childDeletes == 1);
    CHECK_FALSE(scene.canvas->isPendingDelete(child));
    scene.frame();
    CHECK(parentDeletes == 1);
    CHECK(childDeletes == 1);
    CHECK(scene.canvas->getActualChildren().size() == baseline);
}

TEST_CASE("a queued control moved to a second canvas is deleted once, by the canvas that queued it")
{
    TwoCanvases scene;
    controls::Base *host=scene.b->newChild<controls::Base>();
    int deletes=0;
    Tracked *control=scene.add(scene.a, &deletes);
    scene.frames();

    control->delayedDelete();
    control->setParent(host);
    CHECK(scene.a->isPendingDelete(control));
    CHECK_FALSE(scene.b->isPendingDelete(control));

    TwoCanvases::frame(scene.b);
    CHECK(deletes == 0);
    TwoCanvases::frame(scene.a);
    CHECK(deletes == 1);
    CHECK(host->numChildren() == 0);
    scene.frames();
    CHECK(deletes == 1);
}

TEST_CASE("a queued control moved to a second canvas and queued again stays queued on the first only")
{
    TwoCanvases scene;
    controls::Base *host=scene.b->newChild<controls::Base>();
    int deletes=0;
    Tracked *control=scene.add(scene.a, &deletes);
    scene.frames();

    control->delayedDelete();
    control->setParent(host);
    control->delayedDelete();
    CHECK(scene.a->isPendingDelete(control));
    CHECK_FALSE(scene.b->isPendingDelete(control));

    SUBCASE("the second canvas's frame first")
    {
        TwoCanvases::frame(scene.b);
        CHECK(deletes == 0);
        TwoCanvases::frame(scene.a);
    }
    SUBCASE("the first canvas's frame first")
    {
        TwoCanvases::frame(scene.a);
        CHECK(deletes == 1);
        TwoCanvases::frame(scene.b);
    }
    CHECK(deletes == 1);
    CHECK(host->numChildren() == 0);
    scene.frames();
    CHECK(deletes == 1);
}

TEST_CASE("a queued control detached and left alone is deleted by the queuing canvas's next frame")
{
    Scene scene;
    int deletes=0;
    Tracked *control=scene.add(scene.canvas, &deletes);
    scene.frame();

    control->delayedDelete();
    control->setParent(nullptr);
    CHECK(deletes == 0);
    scene.frame();
    CHECK(deletes == 1);
    scene.frame();
    CHECK(deletes == 1);
}

TEST_CASE("a canvas deleted with queued controls off its tree deletes each of them once")
{
    TwoCanvases scene;
    controls::Base *host=scene.b->newChild<controls::Base>();
    int detachedDeletes=0, movedDeletes=0, onTreeDeletes=0;
    Tracked *detached=scene.add(scene.a, &detachedDeletes);
    Tracked *moved=scene.add(scene.a, &movedDeletes);
    Tracked *onTree=scene.add(scene.a, &onTreeDeletes);
    scene.frames();

    detached->delayedDelete();
    detached->setParent(nullptr);
    moved->delayedDelete();
    moved->setParent(host);
    onTree->delayedDelete();

    delete scene.a;
    scene.a=nullptr;
    CHECK(detachedDeletes == 1);
    CHECK(movedDeletes == 1);
    CHECK(onTreeDeletes == 1);
    CHECK(host->numChildren() == 0);

    TwoCanvases::frame(scene.b);
    delete scene.b;
    scene.b=nullptr;
    CHECK(detachedDeletes == 1);
    CHECK(movedDeletes == 1);
    CHECK(onTreeDeletes == 1);
}

TEST_CASE("delayedDelete() on a control with no canvas deletes it at once, without preDelete()")
{
    Scene scene;
    int deletes=0, preDeletes=0;

    SUBCASE("a detached control")
    {
        Tracked *control=scene.add(scene.canvas, &deletes);
        control->m_preDeletes=&preDeletes;
        scene.frame();
        control->setParent(nullptr);

        control->delayedDelete();
        CHECK(deletes == 1);
    }
    SUBCASE("a control under a detached parent")
    {
        int parentDeletes=0;
        Tracked *parent=scene.add(scene.canvas, &parentDeletes);
        Tracked *control=scene.add(parent, &deletes);
        control->m_preDeletes=&preDeletes;
        scene.frame();
        parent->setParent(nullptr);

        control->delayedDelete();
        CHECK(deletes == 1);
        CHECK(parent->numChildren() == 0);
        delete static_cast<controls::Base *>(parent);
        CHECK(parentDeletes == 1);
    }
    CHECK(preDeletes == 0);
    scene.frame();
    CHECK(deletes == 1);
}

TEST_CASE("a queued control is given preDelete() once, by the frame that deletes it")
{
    Scene scene;
    int deletes=0, preDeletes=0;
    Tracked *control=scene.add(scene.canvas, &deletes);
    control->m_preDeletes=&preDeletes;
    scene.frame();

    control->delayedDelete();
    CHECK(preDeletes == 0);
    scene.frame();
    CHECK(preDeletes == 1);
    CHECK(deletes == 1);
}

TEST_CASE("a detached layout::Table shrunk or given a row less does not crash")
{
    Scene scene;
    controls::layout::Table *table=scene.canvas->newChild<controls::layout::Table>();
    table->setColumnCount(3);
    controls::layout::TableRow *first=table->addRow();
    table->addRow();
    scene.frame();
    REQUIRE(table->numChildren() == 2);

    table->setParent(nullptr);

    SUBCASE("fewer columns")
    {
        table->setColumnCount(1);
        CHECK(table->numChildren() == 2);
        CHECK(first->numChildren() == 1);
    }
    SUBCASE("a row removed")
    {
        table->remove(first);
        CHECK(table->numChildren() == 1);
    }
    SUBCASE("cleared")
    {
        // each row deleted at once, out of the list clear() walks
        table->addRow();
        table->clear();
        CHECK(table->numChildren() == 0);
    }
    delete table;
    scene.frame();
}

TEST_CASE("a detached ComboBox cleared does not crash")
{
    Scene scene;
    controls::ComboBox *combo=scene.canvas->newChild<controls::ComboBox>();
    combo->addItem("One");
    combo->addItem("Two");
    combo->addItem("Three");
    scene.frame();

    // its list, never opened, is the box's child, so it has no canvas either:
    // clearItems() deletes each item at once, out of the list it walks
    combo->setParent(nullptr);
    combo->clearItems();
    CHECK(combo->getSelectedItem() == nullptr);
    delete combo;
    scene.frame();
}

TEST_CASE("a MenuItem deleted with a deleteOnClose submenu off any canvas deletes it once")
{
    Scene scene;
    controls::Menu *menu=scene.canvas->newChild<controls::Menu>();
    controls::MenuItem *item=menu->addItem("Item");
    controls::Menu *submenu=item->getMenu();
    submenu->setDeleteOnClose(true);
    int deletes=0;
    scene.add(submenu, &deletes);
    scene.frame();

    // close() deletes the submenu at once with no canvas: ~MenuItem must not
    // touch or delete it after that
    submenu->setParent(nullptr);
    delete item;
    CHECK(deletes == 1);
    scene.frame();
    CHECK(deletes == 1);
}

TEST_CASE("a detached Properties cleared does not crash")
{
    Scene scene;
    controls::Properties *properties=scene.canvas->newChild<controls::Properties>();
    properties->add("One", "1");
    properties->add("Two", "2");
    scene.frame();

    properties->setParent(nullptr);
    properties->clear();

    size_t rows=0;

    for(auto &&child : properties->getChildren())
    {
        if(gweni_cast<controls::PropertyRow>(child))
            ++rows;
    }
    CHECK(rows == 0);
    delete properties;
    scene.frame();
}
