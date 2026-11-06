/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A dock-None control is sized from the size it was asked for (setBounds /
 *  setSize outside the arrange code), its preferred size and its size flags,
 *  never from the bounds an earlier pass gave it: an Expand control that grew
 *  shrinks back once what made it grow is gone, inside a Base parent and inside
 *  a layout, and a canvas resize and back gives the first bounds again.
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
