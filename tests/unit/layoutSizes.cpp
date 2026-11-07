/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A VerticalLayout's or HorizontalLayout's minimum, preferred and maximum
 *  size, the same rules on both axes. Along its stacking axis each is the sum
 *  over its children, the spacing between them and its padding, and the
 *  maximum is finite when every child's is. Across it the preferred and
 *  minimum are the largest child's and the maximum the smallest child's
 *  maximum, each with the padding. A child's cell holds its margins, in all
 *  three, so minimum <= preferred <= maximum holds from the leaves up.
 */
#include "headlessFixture.h"

#include <gweni/controls/layout/layout.h>

#include <doctest/doctest.h>

#include <limits>
#include <string>

using namespace gweni;

namespace
{

constexpr int Unbounded=std::numeric_limits<int>::max();

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

//! A panel docked Center in the canvas, with a layout holding the nested
//! layout under test, of the other direction; blocks are added to the nested
//! one. "along" and "across" are the nested layout's axes.
struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    controls::Base *panel;
    controls::LayoutItem *nested;
    bool vertical;

    explicit Scene(bool vertical):
        vertical(vertical)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);

        panel=canvas->newChild<controls::Base>("panel");
        panel->setDock(DockPosition::Center);
        if(vertical)
        {
            panel->setLayout(new controls::HorizontalLayout());
            nested=new controls::VerticalLayout();
        }
        else
        {
            panel->setLayout(new controls::VerticalLayout());
            nested=new controls::HorizontalLayout();
        }
        panel->getLayout()->addLayout(nested);
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

    //! a block preferring and requesting along x across, in the nested layout
    Block *add(SizeFlag alongFlag, int along, SizeFlag acrossFlag, int across, const Margin &margin=Margin())
    {
        Block *block=panel->newChild<Block>("block");
        nested->addControl(block);

        if(vertical)
        {
            block->setSizeFlags({acrossFlag, alongFlag});
            block->setSize(across, along);
            block->want=Size(across, along);
        }
        else
        {
            block->setSizeFlags({alongFlag, acrossFlag});
            block->setSize(along, across);
            block->want=Size(along, across);
        }
        block->setMargin(margin);
        return block;
    }

    //! a margin or padding given along and across the nested layout
    Margin sides(int alongLead, int alongTrail, int acrossLead, int acrossTrail) const
    {
        if(vertical)
            return Margin(acrossLead, alongLead, acrossTrail, alongTrail);
        return Margin(alongLead, acrossLead, alongTrail, acrossTrail);
    }

    int along(const Size &size) const { return vertical?size.height:size.width; }
    int across(const Size &size) const { return vertical?size.width:size.height; }
};

//! minimum <= preferred <= maximum, on one axis
void checkOrdered(const char *axis, int minimum, int preferred, int maximum)
{
    CAPTURE(axis);
    CHECK(minimum <= preferred);
    CHECK(preferred <= maximum);
}

}//namespace

TEST_CASE("along a layout, its minimum and maximum are the children's sums with the spacing and padding")
{
    for(const bool vertical:{false, true})
    {
        CAPTURE(vertical);
        Scene scene(vertical);
        scene.add(SizeFlag::Fixed, 30, SizeFlag::Fixed, 20, scene.sides(1, 2, 0, 0));
        scene.add(SizeFlag::Fixed, 40, SizeFlag::Fixed, 20);
        scene.add(SizeFlag::Fixed, 50, SizeFlag::Fixed, 20);
        scene.settle();

        // 33 (30 and its margins) + 40 + 50, and the default spacing of 2 twice
        const controls::LayoutItem *nested=scene.nested;
        CHECK(scene.along(nested->getPreferredSize()) == 127);
        CHECK(scene.along(nested->getMinimumSize()) == 127);
        // every child has a finite maximum, so the layout has one
        CHECK(scene.along(nested->getMaximumSize()) == 127);

        // a child that can grow without bound takes the layout's maximum with it
        scene.add(SizeFlag::Expand, 10, SizeFlag::Fixed, 20);
        scene.settle();
        CHECK(scene.along(nested->getPreferredSize()) == 139);
        CHECK(scene.along(nested->getMinimumSize()) == 139);
        CHECK(scene.along(nested->getMaximumSize()) == Unbounded);
    }
}

TEST_CASE("along a layout, its sizes follow setSpacing and setPadding")
{
    for(const bool vertical:{false, true})
    {
        CAPTURE(vertical);
        Scene scene(vertical);
        scene.add(SizeFlag::Fixed, 30, SizeFlag::Fixed, 20, scene.sides(1, 2, 0, 0));
        scene.add(SizeFlag::Fixed, 40, SizeFlag::Fixed, 20);
        scene.add(SizeFlag::Fixed, 50, SizeFlag::Fixed, 20);
        scene.settle();
        REQUIRE(scene.along(scene.nested->getMaximumSize()) == 127);

        scene.nested->setSpacing(5);
        scene.nested->setPadding(scene.sides(3, 4, 6, 7));
        scene.settle();

        // 123 of children, 5 twice between them and 3+4 of padding
        const controls::LayoutItem *nested=scene.nested;
        CHECK(scene.nested->getSpacing() == 5);
        CHECK(scene.along(nested->getPreferredSize()) == 140);
        CHECK(scene.along(nested->getMinimumSize()) == 140);
        CHECK(scene.along(nested->getMaximumSize()) == 140);

        scene.add(SizeFlag::Expand, 10, SizeFlag::Fixed, 20);
        scene.settle();
        CHECK(scene.along(nested->getPreferredSize()) == 155);
        CHECK(scene.along(nested->getMinimumSize()) == 155);
        // saturates, the padding is not added past INT_MAX
        CHECK(scene.along(nested->getMaximumSize()) == Unbounded);
    }
}

TEST_CASE("across a layout, its minimum is the largest child minimum and its maximum the smallest child maximum, with the padding")
{
    for(const bool vertical:{false, true})
    {
        CAPTURE(vertical);
        Scene scene(vertical);

        // with no children nothing caps it
        scene.settle();
        CHECK(scene.across(scene.nested->getMaximumSize()) == Unbounded);

        // cells of 24..24 (20 and its margins), 30..30 and 26..unbounded
        scene.add(SizeFlag::Fixed, 10, SizeFlag::Fixed, 20, scene.sides(0, 0, 2, 2));
        scene.add(SizeFlag::Fixed, 10, SizeFlag::Fixed, 30);
        scene.add(SizeFlag::Fixed, 10, SizeFlag::Expand, 26);
        scene.settle();

        // No length fits both Fixed children, so the minimum is above the
        // maximum here; each still follows its rule.
        const controls::LayoutItem *nested=scene.nested;
        CHECK(scene.across(nested->getPreferredSize()) == 30);
        CHECK(scene.across(nested->getMinimumSize()) == 30);
        CHECK(scene.across(nested->getMaximumSize()) == 24);

        scene.nested->setPadding(scene.sides(0, 0, 3, 4));
        scene.settle();
        CHECK(scene.across(nested->getPreferredSize()) == 37);
        CHECK(scene.across(nested->getMinimumSize()) == 37);
        CHECK(scene.across(nested->getMaximumSize()) == 31);
    }
}

TEST_CASE("a layout's minimum <= preferred <= maximum on both axes, with margins on its children")
{
    for(const bool vertical:{false, true})
    {
        CAPTURE(vertical);
        for(const bool padded:{false, true})
        {
            CAPTURE(padded);
            Scene scene(vertical);

            // across: a Fixed 20 with 2+2 of margins caps the cross length at
            // 24, and every other child fits in it
            scene.add(SizeFlag::Fixed, 30, SizeFlag::Fixed, 20, scene.sides(1, 2, 2, 2));
            scene.add(SizeFlag::Expand, 40, SizeFlag::Expand, 10, scene.sides(3, 3, 1, 1));
            scene.add(SizeFlag::Shrink, 20, SizeFlag::Shrink, 18, scene.sides(2, 0, 0, 2));
            scene.add(SizeFlag::Elastic, 25, SizeFlag::Elastic, 22, scene.sides(0, 4, 1, 0));
            scene.add(SizeFlag::Bloat, 15, SizeFlag::Bloat, 20, scene.sides(1, 1, 2, 2));
            if(padded)
            {
                scene.nested->setSpacing(6);
                scene.nested->setPadding(scene.sides(5, 1, 2, 9));
            }
            scene.settle();

            const controls::LayoutItem *nested=scene.nested;
            const Size minimum=nested->getMinimumSize();
            const Size preferred=nested->getPreferredSize();
            const Size maximum=nested->getMaximumSize();

            checkOrdered("along", scene.along(minimum), scene.along(preferred), scene.along(maximum));
            checkOrdered("across", scene.across(minimum), scene.across(preferred), scene.across(maximum));

            // and the same for a layout of Fixed children only, whose sizes
            // are all finite
            Scene fixed(vertical);
            fixed.add(SizeFlag::Fixed, 30, SizeFlag::Fixed, 20, fixed.sides(1, 2, 2, 2));
            fixed.add(SizeFlag::Fixed, 40, SizeFlag::Fixed, 24, fixed.sides(3, 3, 0, 0));
            if(padded)
            {
                fixed.nested->setSpacing(6);
                fixed.nested->setPadding(fixed.sides(5, 1, 2, 9));
            }
            fixed.settle();

            const Size fixedMinimum=fixed.nested->getMinimumSize();
            const Size fixedPreferred=fixed.nested->getPreferredSize();
            const Size fixedMaximum=fixed.nested->getMaximumSize();

            CHECK(fixed.along(fixedMaximum) != Unbounded);
            CHECK(fixed.across(fixedMaximum) != Unbounded);
            checkOrdered("fixed along", fixed.along(fixedMinimum), fixed.along(fixedPreferred), fixed.along(fixedMaximum));
            checkOrdered("fixed across", fixed.across(fixedMinimum), fixed.across(fixedPreferred), fixed.across(fixedMaximum));
        }
    }
}
