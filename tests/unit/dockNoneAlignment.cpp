/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Where Base::arrange puts a dock-None child (docs/layout.md, "Placing a
 *  dock-None control"). Alignment is opt-in: a control has none by default
 *  and stays at the position it asked for (setPos, setBounds, moveTo), through
 *  later passes and a parent resize. An aligned child is placed in the
 *  parent's padded content box, its margins included, whatever docked
 *  siblings come before or after it. Also the Bottom dock's margin.left.
 */
#include "headlessFixture.h"

#include <gweni/controls/menu.h>

#include <doctest/doctest.h>

#include <string>

using namespace gweni;

namespace
{

//! Rect as text, so a failed check prints both sides.
std::string text(const Rect &r)
{
    return "("+std::to_string(r.x)+", "+std::to_string(r.y)+", "+std::to_string(r.w)+", "+std::to_string(r.h)+")";
}

//! Prefers the size it is told to.
class Block: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Block, controls::Base)
    {}

    Size want;

    void calculateSize(skin::Base * /*skin*/, Dim dim) override
    {
        if(dim == Dim::X)
            m_preferredSize.width=want.width;
        else
            m_preferredSize.height=want.height;
    }
};

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    controls::Base *parent;

    //! A 200x100 dock-None parent at (0, 0), padding 5, 6, 7, 8: its content
    //! box is (5, 6, 188, 86).
    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);

        parent=canvas->newChild<controls::Base>("parent");
        parent->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        parent->setBounds(0, 0, 200, 100);
        parent->setPadding(Padding(5, 6, 7, 8));
    }

    ~Scene()
    {
        delete canvas;
    }

    void settle()
    {
        for(int i=0; i<10; ++i)
        {
            canvas->updateCanvas();
            canvas->renderCanvas();
            if(canvas->getLayoutStats().passes == 0)
                break;
        }
    }

    //! A 40x20 child with margin 1, 2, 3, 4, asking for (11, 13).
    controls::Base *child(Alignment alignment)
    {
        controls::Base *control=parent->newChild<controls::Base>("child");
        control->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        control->setBounds(11, 13, 40, 20);
        control->setMargin(Margin(1, 2, 3, 4));
        control->setAlignment(alignment);
        return control;
    }

    //! A docked sibling 30 wide (Left/Right) or 10 high (Top/Bottom).
    Block *docked(DockPosition dock)
    {
        Block *block=parent->newChild<Block>("docked");
        block->want=Size(30, 10);
        block->setDock(dock);
        return block;
    }
};

}//namespace

TEST_CASE("dock None: a control has no alignment by default")
{
    Scene scene;
    CHECK((scene.parent->newChild<controls::Base>()->getAlignment() == Alignment::Absolute));
}

TEST_CASE("dock None: each alignment is placed in the padded content box, margins included")
{
    struct Case
    {
        const char *name;
        Alignment alignment;
        Point position;
    };
    // The other axis is unaligned, so it keeps the requested 11 or 13.
    const Case cases[]=
    {
        {"Left", Alignment::Left, Point(5+1, 13)},
        {"Right", Alignment::Right, Point(200-7-3-40, 13)},
        {"CenterH", Alignment::CenterH, Point(5+1+(188-40-1-3)/2, 13)},
        {"Top", Alignment::Top, Point(11, 6+2)},
        {"Bottom", Alignment::Bottom, Point(11, 100-8-4-20)},
        {"CenterV", Alignment::CenterV, Point(11, 6+2+(86-20-2-4)/2)},
    };

    for(const Case &c:cases)
    {
        CAPTURE(c.name);
        Scene scene;
        controls::Base *control=scene.child(c.alignment);
        scene.settle();
        CHECK(text(control->getBounds()) == text(Rect(c.position.x, c.position.y, 40, 20)));
    }

    Scene scene;
    controls::Base *control=scene.child(Alignment::Right|Alignment::Bottom);
    scene.settle();
    CHECK(text(control->getBounds()) == text(Rect(150, 68, 40, 20)));
    control->setAlignment(Alignment::Center);
    scene.settle();
    CHECK(text(control->getBounds()) == text(Rect(78, 38, 40, 20)));
}

TEST_CASE("dock None: an aligned child ignores the docked siblings and their order")
{
    const DockPosition docks[]={DockPosition::Left, DockPosition::Right, DockPosition::Top, DockPosition::Bottom};

    for(DockPosition dock:docks)
    {
        for(int before=0; before<2; ++before)
        {
            CAPTURE(getDockPositionName(dock));
            CAPTURE(before);
            Scene scene;
            if(before)
                scene.docked(dock);
            controls::Base *right=scene.child(Alignment::Right|Alignment::Bottom);
            if(!before)
                scene.docked(dock);
            scene.settle();
            CHECK(text(right->getBounds()) == text(Rect(150, 68, 40, 20)));
        }
    }
}

TEST_CASE("dock None: an unaligned child keeps its requested position")
{
    Scene scene;
    controls::Base *control=scene.child(Alignment::Absolute);
    control->setPos(23, 17);
    scene.settle();
    CHECK(text(control->getBounds()) == text(Rect(23, 17, 40, 20)));

    // a second pass
    control->invalidate();
    scene.parent->invalidate();
    scene.settle();
    CHECK(text(control->getBounds()) == text(Rect(23, 17, 40, 20)));

    // the parent resized, and back
    scene.parent->setSize(300, 150);
    scene.settle();
    CHECK(text(control->getBounds()) == text(Rect(23, 17, 40, 20)));
    scene.parent->setSize(200, 100);
    scene.settle();
    CHECK(text(control->getBounds()) == text(Rect(23, 17, 40, 20)));

    // moveTo is a request too
    control->moveTo(31, 9);
    scene.settle();
    CHECK(text(control->getBounds()) == text(Rect(31, 9, 40, 20)));

    // aligned, then not: back where it asked, not where the alignment put it
    control->setAlignment(Alignment::Right|Alignment::Bottom);
    scene.settle();
    CHECK(text(control->getBounds()) == text(Rect(150, 68, 40, 20)));
    control->setAlignment(Alignment::Absolute);
    scene.settle();
    CHECK(text(control->getBounds()) == text(Rect(31, 9, 40, 20)));
}

TEST_CASE("dock None: a Menu opens where it was put, not at the canvas origin")
{
    Scene scene;
    controls::Menu *menu=scene.canvas->newChild<controls::Menu>("menu");
    menu->setBounds(3, 19, 100, 50);
    scene.settle();
    CHECK(menu->getX() == 3);
    CHECK(menu->getY() == 19);
}

TEST_CASE("dock Bottom: the child is inset by its left margin, as for Top")
{
    for(SizeFlag horizontal:{SizeFlag::Expand, SizeFlag::Fixed})
    {
        CAPTURE(horizontal == SizeFlag::Fixed);
        Scene scene;
        controls::Base *top=scene.parent->newChild<controls::Base>("top");
        controls::Base *bottom=scene.parent->newChild<controls::Base>("bottom");

        for(controls::Base *control:{top, bottom})
        {
            control->setSizeFlags({horizontal, SizeFlag::Fixed});
            control->setBounds(0, 0, 50, 10);
            control->setMargin(Margin(9, 0, 3, 0));
        }
        top->setDock(DockPosition::Top);
        bottom->setDock(DockPosition::Bottom);
        scene.settle();

        const int width=horizontal == SizeFlag::Fixed ? 50 : 188-9-3;
        CHECK(top->getX() == 5+9);
        CHECK(bottom->getX() == 5+9);
        CHECK(top->getWidth() == width);
        CHECK(bottom->getWidth() == width);
    }
}
