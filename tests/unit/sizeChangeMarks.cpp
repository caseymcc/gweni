/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  What a control's new bounds mark for the render walk (Base::
 *  onBoundsChanged), on a real Canvas with the StubSkin. A move marks the
 *  control's whole subtree (every absolute position changed). A new size at
 *  the same origin marks the control itself and, below it, only the children
 *  its arrange gave new bounds (each marked by its own setBounds), when the
 *  region its children are clipped to, its bounds within its parent's region,
 *  stays: a control that does not clip, or one whose moving edge is outside
 *  its parent's region before and after. When that region changes (the
 *  control clips and its moving edge is inside the parent's region, or
 *  crosses its edge) the whole subtree is marked, so every slot below it is
 *  stamped with the new clip box. A control that narrows its children's
 *  clip itself (getChildClipRect: ScrollControl, TabControl, DataTableHeader)
 *  marks its subtree on any size change. So an overflowing ScrollControl or
 *  ListBox given one more item marks the new item, its inner panel and the
 *  bar's thumb, not the items already there (on main before #511: 64 of 77
 *  and 96 of 109 controls).
 */
#include "headlessFixture.h"

#include <gweni/controls/dataTable.h>
#include <gweni/controls/label.h>
#include <gweni/controls/listBox.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/tabButton.h>
#include <gweni/controls/tabControl.h>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace gweni;

namespace
{

//! A plain control whose clipping is a flag.
class Container: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Container, controls::Base)
    {}

    bool clips=true;

    bool shouldClip() override { return clips; }
};

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    //! a clipping panel at (20, 20), 200x100: the region the container is in
    controls::Base *parent=nullptr;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);

        parent=canvas->newChild<controls::Base>();
        parent->setBounds(20, 20, 200, 100);
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

    void settle()
    {
        frame();
        frame();
    }
};

bool geometry(controls::Base *control)
{
    return (control->getStateChange() & controls::StateChange_Geometry) != 0;
}

//! A container docking a 20-unit child to its left and one to its right, each
//! holding a small child of its own: a wider container moves the right one
//! only.
struct Docked
{
    Container *container=nullptr;
    controls::Base *left=nullptr;
    controls::Base *leftInner=nullptr;
    controls::Base *right=nullptr;
    controls::Base *rightInner=nullptr;

    Docked(controls::Base *parent, const Rect &bounds, bool clips)
    {
        container=parent->newChild<Container>();
        container->clips=clips;
        container->setBounds(bounds);

        left=container->newChild<controls::Base>();
        left->setWidth(20);
        left->setDock(DockPosition::Left);
        leftInner=left->newChild<controls::Base>();
        leftInner->setBounds(2, 2, 5, 5);

        right=container->newChild<controls::Base>();
        right->setWidth(20);
        right->setDock(DockPosition::Right);
        rightInner=right->newChild<controls::Base>();
        rightInner->setBounds(2, 2, 5, 5);
    }

    void widen(int by)
    {
        const Rect bounds=container->getBounds();

        container->setBounds(bounds.x, bounds.y, bounds.w+by, bounds.h);
    }
};

}//namespace

TEST_CASE("Size change: a clipping control whose region stays marks only what its arrange moved")
{
    Scene scene;
    // its right edge past the parent's (at 200) before and after
    Docked docked(scene.parent, Rect(150, 10, 100, 50), true);

    scene.settle();
    const Rect leftBefore=docked.left->getBounds();
    const Rect rightBefore=docked.right->getBounds();

    docked.widen(50);
    scene.canvas->updateCanvas();

    REQUIRE(docked.container->getWidth() == 150);
    REQUIRE((docked.left->getBounds() == leftBefore));
    REQUIRE(docked.right->getX() == rightBefore.x+50);

    CHECK(geometry(docked.container));
    CHECK(geometry(docked.right));
    CHECK(geometry(docked.rightInner));
    CHECK_FALSE(geometry(docked.left));
    CHECK_FALSE(geometry(docked.leftInner));
    CHECK_FALSE(geometry(scene.parent));

    scene.canvas->renderCanvas();
}

TEST_CASE("Size change: a control that does not clip marks only what its arrange moved")
{
    Scene scene;
    // its right edge inside the parent's region before and after
    Docked docked(scene.parent, Rect(10, 10, 100, 50), false);

    scene.settle();

    docked.widen(20);
    scene.canvas->updateCanvas();

    REQUIRE(docked.container->getWidth() == 120);

    CHECK(geometry(docked.container));
    CHECK(geometry(docked.right));
    CHECK(geometry(docked.rightInner));
    CHECK_FALSE(geometry(docked.left));
    CHECK_FALSE(geometry(docked.leftInner));

    scene.canvas->renderCanvas();
}

TEST_CASE("Size change: a clipping control whose region changes marks its subtree")
{
    SUBCASE("the moving edge inside the parent's region")
    {
        Scene scene;
        Docked docked(scene.parent, Rect(10, 10, 100, 50), true);

        scene.settle();
        docked.widen(20);
        scene.canvas->updateCanvas();

        CHECK(geometry(docked.container));
        CHECK(geometry(docked.left));
        CHECK(geometry(docked.leftInner));
        CHECK(geometry(docked.right));
        CHECK(geometry(docked.rightInner));
        scene.canvas->renderCanvas();
    }

    SUBCASE("the moving edge crossing the parent's edge")
    {
        Scene scene;
        // right edge from 190 to 210, the parent's at 200
        Docked docked(scene.parent, Rect(150, 10, 40, 50), true);

        scene.settle();
        docked.widen(20);
        scene.canvas->updateCanvas();

        CHECK(geometry(docked.left));
        CHECK(geometry(docked.leftInner));
        scene.canvas->renderCanvas();
    }

    SUBCASE("a shorter control, its bottom edge inside the parent's region")
    {
        Scene scene;
        Docked docked(scene.parent, Rect(150, 10, 100, 50), true);

        scene.settle();
        docked.container->setBounds(150, 10, 100, 40);
        scene.canvas->updateCanvas();

        CHECK(geometry(docked.left));
        CHECK(geometry(docked.leftInner));
        scene.canvas->renderCanvas();
    }
}

TEST_CASE("Size change: a move marks the subtree")
{
    Scene scene;
    Docked docked(scene.parent, Rect(150, 10, 100, 50), true);

    scene.settle();
    docked.container->setBounds(140, 10, 110, 50);
    scene.canvas->updateCanvas();

    CHECK(geometry(docked.left));
    CHECK(geometry(docked.leftInner));
    CHECK(geometry(docked.rightInner));
    scene.canvas->renderCanvas();
}

TEST_CASE("Size change: a control with child clip rects marks its subtree")
{
    SUBCASE("ScrollControl")
    {
        Scene scene;
        controls::ScrollControl *scroll=scene.parent->newChild<controls::ScrollControl>();
        // past the parent's right edge before and after
        scroll->setBounds(150, 10, 100, 80);

        controls::Base *content=scroll->newChild<controls::Base>();
        content->setBounds(0, 0, 30, 30);
        controls::Base *inner=content->newChild<controls::Base>();
        inner->setBounds(2, 2, 5, 5);

        scene.settle();
        scroll->setBounds(150, 10, 150, 80);
        scene.canvas->updateCanvas();

        REQUIRE((content->getBounds() == Rect(0, 0, 30, 30)));
        CHECK(geometry(content));
        CHECK(geometry(inner));
        scene.canvas->renderCanvas();
    }

    SUBCASE("TabControl")
    {
        Scene scene;
        controls::TabControl *tabs=scene.parent->newChild<controls::TabControl>();
        tabs->setBounds(150, 10, 100, 80);

        controls::TabButton *button=tabs->addPage("One");
        tabs->addPage("Two");
        controls::Base *content=button->getPage()->newChild<controls::Base>();
        content->setBounds(0, 0, 10, 10);
        controls::Base *inner=content->newChild<controls::Base>();
        inner->setBounds(2, 2, 5, 5);

        scene.settle();
        const Rect contentBefore=content->getBounds();

        tabs->setBounds(150, 10, 150, 80);
        scene.canvas->updateCanvas();

        REQUIRE((content->getBounds() == contentBefore));
        CHECK(geometry(button));
        CHECK(geometry(content));
        CHECK(geometry(inner));
        scene.canvas->renderCanvas();
    }

    SUBCASE("DataTableHeader")
    {
        Scene scene;
        controls::DataTable *table=scene.parent->newChild<controls::DataTable>();
        table->setBounds(150, 10, 100, 80);
        table->setColumnCount(2);
        table->setColumnWidth(0, 30);
        table->setColumnWidth(1, 30);
        table->setLastColumnStretch(false);
        table->addRow();

        scene.settle();
        controls::internal::DataTableHeader *header=table->getHeader();
        controls::Base *cell=header->getCells().at(0);
        const Rect cellBefore=cell->getBounds();
        const int headerWidth=header->getWidth();

        table->setBounds(150, 10, 150, 80);
        scene.canvas->updateCanvas();

        REQUIRE(header->getWidth() == headerWidth+50);
        REQUIRE((cell->getBounds() == cellBefore));
        CHECK(geometry(cell));
        for(controls::Base *child : cell->getActualChildren())
            CHECK(geometry(child));
        scene.canvas->renderCanvas();
    }
}

namespace
{

//! The controls in \p control's subtree, \p control included.
size_t subtreeSize(controls::Base *control)
{
    size_t count=1;

    for(controls::Base *child : control->getActualChildren())
        count+=subtreeSize(child);
    return count;
}

//! Whether anything in \p control's subtree, \p control included, is marked.
bool subtreeMarked(controls::Base *control)
{
    if(control->getStateChange() != controls::StateChange_Nothing)
        return true;
    for(controls::Base *child : control->getActualChildren())
        if(subtreeMarked(child))
            return true;
    return false;
}

}//namespace

TEST_CASE("Size change: an overflowing scroll area given one more item marks that item, not the others")
{
    SUBCASE("ScrollControl")
    {
        Scene scene;
        controls::ScrollControl *scroll=scene.canvas->newChild<controls::ScrollControl>();
        scroll->setBounds(10, 10, 300, 200);

        std::vector<controls::Label *> labels;
        for(int i=0; i<30; ++i)
        {
            controls::Label *label=scroll->newChild<controls::Label>();
            label->setText("Label "+std::to_string(i));
            label->setDock(DockPosition::Top);
            labels.push_back(label);
        }
        scene.settle();
        controls::Base *inner=labels.front()->getActualParent();
        const int innerHeight=inner->getHeight();

        REQUIRE(innerHeight > 200);

        controls::Label *added=scroll->newChild<controls::Label>();
        added->setText("Added");
        added->setDock(DockPosition::Top);
        controls::resetChangeStats();
        scene.canvas->updateCanvas();

        // the inner panel grew at the same origin, its region the viewport's
        REQUIRE(inner->getHeight() > innerHeight);
        CHECK(geometry(inner));
        for(controls::Label *label : labels)
            CHECK_FALSE(subtreeMarked(label));

        scene.canvas->renderCanvas();
        // the added Label's subtree, the inner panel and the thumb
        CHECK(controls::getChangeStats().controls <= subtreeSize(added)+2);
        CHECK(controls::getChangeStats().visited <= subtreeSize(added)+2);
    }

    SUBCASE("ListBox")
    {
        Scene scene;
        controls::ListBox *list=scene.canvas->newChild<controls::ListBox>();
        list->setBounds(10, 10, 300, 200);

        std::vector<controls::Base *> rows;
        for(int i=0; i<30; ++i)
            rows.push_back(list->addItem("Item "+std::to_string(i)));
        scene.settle();

        controls::Base *added=list->addItem("Added");
        controls::resetChangeStats();
        scene.canvas->updateCanvas();

        for(controls::Base *row : rows)
            CHECK_FALSE(subtreeMarked(row));

        scene.canvas->renderCanvas();
        // the added row's subtree, the table, the inner panel and the thumb
        CHECK(controls::getChangeStats().controls <= subtreeSize(added)+3);
        CHECK(controls::getChangeStats().visited <= subtreeSize(added)+3);
    }
}
