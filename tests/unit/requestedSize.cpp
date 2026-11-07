/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A dock-None control is sized from the size it was asked for (setBounds /
 *  setSize outside the arrange code), its preferred size and its size flags,
 *  never from the bounds an earlier pass gave it: an Expand control that grew
 *  shrinks back once what made it grow is gone, inside a Base parent and inside
 *  a layout, and a canvas resize and back gives the first bounds again. A
 *  layout child is sized from its cell minus its margin, clamped at 0.
 */
#include "headlessFixture.h"

#include <gweni/controls/layout/layout.h>

#include <doctest/doctest.h>

#include <limits>
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

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);
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

    //! A panel docked Center in the canvas, so it takes the canvas size.
    controls::Base *centerPanel()
    {
        controls::Base *panel=canvas->newChild<controls::Base>("panel");
        panel->setDock(DockPosition::Center);
        return panel;
    }
};

}//namespace

TEST_CASE("dock None: Expand is at least its request and its preferred size, and shrinks back")
{
    Scene scene;
    Block *block=scene.centerPanel()->newChild<Block>("block");
    // placed where it asked, not aligned to the parent's top-left (the default)
    block->setAlignment(Alignment::Absolute);
    block->setBounds(30, 30, 200, 100);
    block->want=Size(50, 20);
    scene.settle();
    CHECK(text(block->getBounds()) == text(Rect(30, 30, 200, 100)));

    block->want=Size(300, 150);
    block->invalidate();
    scene.settle();
    CHECK(text(block->getBounds()) == text(Rect(30, 30, 300, 150)));

    // what made it grow is gone: back to the request, not stuck at 300x150
    block->want=Size(50, 20);
    block->invalidate();
    scene.settle();
    CHECK(text(block->getBounds()) == text(Rect(30, 30, 200, 100)));
    CHECK(text(block->getRequestedBounds()) == text(Rect(30, 30, 200, 100)));
}

TEST_CASE("dock None: Shrink, Elastic, Fixed and no request")
{
    Scene scene;
    controls::Base *panel=scene.centerPanel();

    Block *shrink=panel->newChild<Block>("shrink");
    shrink->setSizeFlags({SizeFlag::Shrink, SizeFlag::Shrink});
    shrink->setBounds(0, 0, 200, 100);
    shrink->want=Size(80, 120);

    Block *elastic=panel->newChild<Block>("elastic");
    elastic->setSizeFlags({SizeFlag::Elastic, SizeFlag::Elastic});
    elastic->setBounds(0, 0, 200, 100);
    elastic->want=Size(80, 120);

    controls::Base *fixed=panel->newChild<controls::Base>("fixed");
    fixed->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    fixed->setBounds(0, 0, 70, 30);

    Block *unasked=panel->newChild<Block>("unasked");
    unasked->want=Size(40, 25);

    scene.settle();
    CHECK(shrink->getWidth() == 80);
    CHECK(shrink->getHeight() == 100);
    CHECK(elastic->getWidth() == 80);
    CHECK(elastic->getHeight() == 120);
    CHECK(fixed->getWidth() == 70);
    CHECK(fixed->getHeight() == 30);
    CHECK(unasked->getWidth() == 40);
    CHECK(unasked->getHeight() == 25);
    CHECK_FALSE(unasked->hasRequestedSize(Dim::X));

    // the arrange code's writes are not requests
    CHECK(text(shrink->getRequestedBounds()) == text(Rect(0, 0, 200, 100)));
    CHECK(text(elastic->getRequestedBounds()) == text(Rect(0, 0, 200, 100)));

    // Shrink grows back when its content does
    shrink->want=Size(300, 300);
    shrink->invalidate();
    scene.settle();
    CHECK(shrink->getWidth() == 200);
    CHECK(shrink->getHeight() == 100);
}

TEST_CASE("Fixed takes its minimum and maximum size from the request, per axis")
{
    Scene scene;
    controls::Base *control=scene.centerPanel()->newChild<controls::Base>("control");
    control->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    control->setBounds(0, 0, 70, 30);
    scene.settle();

    CHECK(control->getMinimumSize().width == 70);
    CHECK(control->getMinimumSize().height == 30);
    CHECK(control->getMaximumSize().width == 70);
    CHECK(control->getMaximumSize().height == 30);

    // a horizontal Shrink no longer decides the vertical maximum
    control->setSizeFlags({SizeFlag::Shrink, SizeFlag::Expand});
    control->setPreferredSize(Size(10, 10));
    CHECK(control->getMaximumSize().height == std::numeric_limits<int>::max());
}

TEST_CASE("setPos asks for a position only, setWidth for a width only")
{
    Scene scene;
    Block *block=scene.centerPanel()->newChild<Block>("block");
    block->setAlignment(Alignment::Absolute);
    block->setBounds(0, 0, 100, 50);
    block->want=Size(300, 20);
    scene.settle();
    REQUIRE(block->getWidth() == 300);

    // moving the grown control must not make its grown width the request
    block->setPos(40, 10);
    block->setHeight(60);
    CHECK(text(block->getRequestedBounds()) == text(Rect(40, 10, 100, 60)));

    block->want=Size(10, 10);
    block->invalidate();
    scene.settle();
    CHECK(text(block->getBounds()) == text(Rect(40, 10, 100, 60)));
}

TEST_CASE("a layout child gets its request up to the cell, and a canvas resize and back gives it again")
{
    Scene scene;
    controls::Base *panel=scene.centerPanel();
    panel->setLayout(new controls::VerticalLayout());

    Block *asked=panel->newChild<Block>("asked");
    asked->setBounds(30, 30, 200, 100);
    asked->want=Size(50, 20);

    Block *unasked=panel->newChild<Block>("unasked");
    unasked->want=Size(50, 20);

    scene.settle();
    // a layout places it; its size is its request
    CHECK(asked->getWidth() == 200);
    CHECK(asked->getHeight() == 100);
    // nobody asked: it fills the cell, as before
    CHECK(unasked->getWidth() == 640);

    const Rect askedBefore=asked->getBounds();
    const Rect unaskedBefore=unasked->getBounds();

    scene.canvas->setSize(1024, 768);
    scene.settle();
    CHECK(asked->getWidth() == 200);
    CHECK(asked->getHeight() == 100);
    CHECK(unasked->getWidth() == 1024);

    // narrower than the request: capped at the cell
    scene.canvas->setSize(150, 480);
    scene.settle();
    CHECK(asked->getWidth() == 150);

    scene.canvas->setSize(640, 480);
    scene.settle();
    CHECK(text(asked->getBounds()) == text(askedBefore));
    CHECK(text(unasked->getBounds()) == text(unaskedBefore));
}

TEST_CASE("a Fixed layout child advances a Vertical and a HorizontalLayout by its size and margins")
{
    for(const bool vertical:{true, false})
    {
        CAPTURE(vertical);
        Scene scene;
        controls::Base *panel=scene.centerPanel();
        if(vertical)
            panel->setLayout(new controls::VerticalLayout());
        else
            panel->setLayout(new controls::HorizontalLayout());

        Block *before=panel->newChild<Block>("before");
        before->setSizeFlags({SizeFlag::Shrink, SizeFlag::Shrink});
        before->want=Size(50, 20);

        controls::Base *fixed=panel->newChild<controls::Base>("fixed");
        fixed->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        fixed->setSize(70, 30);
        fixed->setMargin(Margin(3, 4, 5, 6));

        Block *after=panel->newChild<Block>("after");
        after->setSizeFlags({SizeFlag::Shrink, SizeFlag::Shrink});
        after->want=Size(50, 20);

        scene.settle();
        // the layout's spacing is 2
        if(vertical)
        {
            CHECK(text(before->getBounds()) == text(Rect(0, 0, 50, 20)));
            CHECK(text(fixed->getBounds()) == text(Rect(3, 20+2+4, 70, 30)));
            CHECK(text(after->getBounds()) == text(Rect(0, 20+2+4+30+6+2, 50, 20)));
        }
        else
        {
            CHECK(text(before->getBounds()) == text(Rect(0, 0, 50, 20)));
            CHECK(text(fixed->getBounds()) == text(Rect(50+2+3, 4, 70, 30)));
            CHECK(text(after->getBounds()) == text(Rect(50+2+3+70+5+2, 0, 50, 20)));
        }
    }
}

TEST_CASE("a layout child with a margin is sized from its cell minus the margin")
{
    Scene scene;
    controls::Base *panel=scene.centerPanel();
    panel->setLayout(new controls::VerticalLayout());

    controls::Base *child=panel->newChild<controls::Base>("child");
    child->setSizeFlags({SizeFlag::Expand, SizeFlag::Expand});
    child->setMargin(Margin(10, 10, 10, 10));

    scene.settle();
    CHECK(text(child->getBounds()) == text(Rect(10, 10, 620, 460)));
}

TEST_CASE("a layout child's request wider than its cell minus the margin is capped there")
{
    Scene scene;
    controls::Base *panel=scene.centerPanel();
    panel->setLayout(new controls::VerticalLayout());

    Block *asked=panel->newChild<Block>("asked");
    asked->setBounds(0, 0, 700, 20);
    asked->want=Size(700, 20);
    asked->setSizeFlags({SizeFlag::Shrink, SizeFlag::Shrink});
    asked->setMargin(Margin(7, 3, 11, 5));

    scene.settle();
    // the cell is the panel width (the preferred 700+7+11 shrunk to it) and
    // 20+3+5 high
    CHECK(text(asked->getBounds()) == text(Rect(7, 3, 640-7-11, 20)));
}

TEST_CASE("Expand children with margins in a HorizontalLayout do not overlap or pass the panel edge")
{
    Scene scene;
    controls::Base *panel=scene.centerPanel();
    panel->setLayout(new controls::HorizontalLayout());

    controls::Base *c0=panel->newChild<controls::Base>("c0");
    controls::Base *c1=panel->newChild<controls::Base>("c1");
    for(controls::Base *c:{c0, c1})
    {
        c->setSizeFlags({SizeFlag::Expand, SizeFlag::Expand});
        c->setMargin(Margin(5, 5, 5, 5));
    }

    scene.settle();
    CAPTURE(text(c0->getBounds()));
    CAPTURE(text(c1->getBounds()));
    CHECK(c0->getX() == 5);
    CHECK(c0->getWidth() > 0);
    CHECK(c1->getWidth() > 0);
    CHECK(c0->getX()+c0->getWidth()+5 <= c1->getX()-5);
    CHECK(c1->getX()+c1->getWidth() <= 640-5);
    CHECK(c0->getY() == 5);
    CHECK(c0->getHeight() == 480-5-5);
    CHECK(c1->getHeight() == 480-5-5);
}

TEST_CASE("a Fixed layout child with a margin keeps its requested size, also in a cell smaller than the margin")
{
    Scene scene;
    controls::Base *panel=scene.centerPanel();
    panel->setLayout(new controls::VerticalLayout());

    controls::Base *fixed=panel->newChild<controls::Base>("fixed");
    fixed->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    fixed->setSize(70, 30);
    fixed->setMargin(Margin(3, 4, 5, 6));

    scene.settle();
    CHECK(text(fixed->getBounds()) == text(Rect(3, 4, 70, 30)));

    scene.canvas->setSize(6, 8);
    scene.settle();
    CHECK(text(fixed->getBounds()) == text(Rect(3, 4, 70, 30)));
}

TEST_CASE("a layout cell smaller than the child's margins gives it size 0, never a negative size")
{
    Scene scene;
    controls::Base *panel=scene.centerPanel();
    panel->setLayout(new controls::VerticalLayout());

    controls::Base *expand=panel->newChild<controls::Base>("expand");
    expand->setSizeFlags({SizeFlag::Expand, SizeFlag::Expand});
    expand->setMargin(Margin(10, 10, 10, 10));

    Block *asked=panel->newChild<Block>("asked");
    asked->setBounds(0, 0, 50, 20);
    asked->setSizeFlags({SizeFlag::Shrink, SizeFlag::Shrink});
    asked->setMargin(Margin(10, 10, 10, 10));

    scene.canvas->setSize(15, 15);
    scene.settle();
    CAPTURE(text(expand->getBounds()));
    CAPTURE(text(asked->getBounds()));
    CHECK(expand->getX() == 10);
    CHECK(expand->getY() == 10);
    CHECK(expand->getWidth() == 0);
    CHECK(expand->getHeight() == 0);
    CHECK(asked->getWidth() == 0);
    CHECK(asked->getHeight() >= 0);
}

namespace
{

//! A panel with a layout holding a nested layout of the other direction, with
//! "inner" in it, and a Fixed 30x30 "after" past it along the row (vertical:
//! the column), whose position shows how long the outer layout made the
//! nested one.
struct NestedScene: Scene
{
    bool vertical;
    controls::LayoutItem *nested;
    Block *inner;
    controls::Base *after;

    NestedScene(bool vertical, SizeFlag innerFlag, int innerLength):
        vertical(vertical)
    {
        controls::Base *panel=centerPanel();
        if(vertical)
        {
            panel->setLayout(new controls::VerticalLayout());
            nested=new controls::HorizontalLayout();
        }
        else
        {
            panel->setLayout(new controls::HorizontalLayout());
            nested=new controls::VerticalLayout();
        }
        panel->getLayout()->addLayout(nested);

        inner=panel->newChild<Block>("inner");
        nested->addControl(inner);
        setInner(innerFlag, innerLength);

        after=panel->newChild<controls::Base>("after");
        after->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        after->setSize(30, 30);
    }

    //! inner's flag and length along the outer layout's direction; 20 and
    //! Fixed across it
    void setInner(SizeFlag flag, int length)
    {
        if(vertical)
        {
            inner->setSizeFlags({SizeFlag::Fixed, flag});
            inner->setSize(20, length);
            inner->want=Size(20, length);
        }
        else
        {
            inner->setSizeFlags({flag, SizeFlag::Fixed});
            inner->setSize(length, 20);
            inner->want=Size(length, 20);
        }
        inner->invalidate();
    }

    int nestedLength() const
    {
        return vertical?nested->getPreferredSize().height:nested->getPreferredSize().width;
    }

    SizeFlag nestedFlag() const
    {
        return vertical?nested->getSizeFlags().vertical:nested->getSizeFlags().horizontal;
    }

    int afterPos() const
    {
        return vertical?after->getY():after->getX();
    }
};

}//namespace

TEST_CASE("a nested layout's preferred length shrinks back with its child, as in a fresh layout")
{
    for(const bool vertical:{false, true})
    {
        CAPTURE(vertical);
        NestedScene scene(vertical, SizeFlag::Shrink, 300);
        scene.settle();
        REQUIRE(scene.nestedLength() == 300);

        scene.setInner(SizeFlag::Shrink, 50);
        scene.settle();

        NestedScene fresh(vertical, SizeFlag::Shrink, 50);
        fresh.settle();

        // not stuck at the 300 the child was before
        CHECK(fresh.nestedLength() == 50);
        CHECK(scene.nestedLength() == fresh.nestedLength());
        CHECK(text(scene.nested->getBounds()) == text(fresh.nested->getBounds()));
        CHECK(text(scene.inner->getBounds()) == text(fresh.inner->getBounds()));
        CHECK(text(scene.after->getBounds()) == text(fresh.after->getBounds()));

        // and laying it out again and again changes nothing
        const Rect nestedBounds=scene.nested->getBounds();
        const Rect afterBounds=scene.after->getBounds();
        for(int i=0; i<5; ++i)
        {
            scene.inner->invalidate();
            scene.settle();
        }
        CHECK(scene.nestedLength() == 50);
        CHECK(text(scene.nested->getBounds()) == text(nestedBounds));
        CHECK(text(scene.after->getBounds()) == text(afterBounds));
    }
}

TEST_CASE("a nested layout's size flag follows its child from Expand to Fixed, as in a fresh layout")
{
    for(const bool vertical:{false, true})
    {
        CAPTURE(vertical);
        NestedScene scene(vertical, SizeFlag::Expand, 50);
        scene.settle();
        // the Expand nested layout takes the extra space: "after" is at the far end
        REQUIRE(sizeFlagName(scene.nestedFlag()) == "Expand");
        REQUIRE(scene.afterPos() > 52);

        scene.setInner(SizeFlag::Fixed, 50);
        scene.settle();

        NestedScene fresh(vertical, SizeFlag::Fixed, 50);
        fresh.settle();

        CHECK(sizeFlagName(fresh.nestedFlag()) == "Fixed");
        CHECK(sizeFlagName(scene.nestedFlag()) == "Fixed");
        CHECK(text(scene.nested->getBounds()) == text(fresh.nested->getBounds()));
        CHECK(text(scene.inner->getBounds()) == text(fresh.inner->getBounds()));
        // the nested layout is its child's 50 long again, "after" 2 (the spacing) past it
        CHECK(fresh.afterPos() == 52);
        CHECK(text(scene.after->getBounds()) == text(fresh.after->getBounds()));
    }
}
