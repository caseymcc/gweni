/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  How a VerticalLayout or HorizontalLayout places its children, the same
 *  rules on both axes. Along its stacking axis the children follow each other
 *  from the lead padding, the spacing apart; space the layout has over its
 *  preferred length goes to Bloat children first, then to Expand and Elastic,
 *  and space short comes out of Bloat first, then out of Shrink and Elastic.
 *  Across it a child gets its preferred length, or the whole length inside the
 *  padding when it can grow (or shrink) to it; the padding is never added to a
 *  child. A hidden child takes no space and no spacing.
 */
#include "headlessFixture.h"

#include <gweni/controls/layout/layout.h>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace gweni;

namespace
{

//! the panel's length on both axes, so a row and a column have the same room
constexpr int PanelLength=400;

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

//! A square panel docked Center in the canvas, laid out by the layout under
//! test: a HorizontalLayout (a row) or a VerticalLayout (a column). "along"
//! and "across" are that layout's axes.
struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    controls::Base *panel;
    controls::LayoutItem *layout;
    std::vector<Block *> blocks;
    bool vertical;

    explicit Scene(bool vertical):
        vertical(vertical)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(PanelLength, PanelLength);

        panel=canvas->newChild<controls::Base>("panel");
        panel->setDock(DockPosition::Center);
        if(vertical)
            layout=new controls::VerticalLayout();
        else
            layout=new controls::HorizontalLayout();
        panel->setLayout(layout);
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

    //! a block preferring and requesting along x across
    Block *add(SizeFlag alongFlag, int along, SizeFlag acrossFlag=SizeFlag::Fixed, int across=20)
    {
        Block *block=panel->newChild<Block>("block");
        layout->addControl(block);
        set(block, alongFlag, along, acrossFlag, across);
        blocks.push_back(block);
        return block;
    }

    //! A size is asked for on a Fixed axis only; on the others the block
    //! fills its cell (Base::layoutCellSize), so its bounds are the cell.
    void set(Block *block, SizeFlag alongFlag, int along, SizeFlag acrossFlag=SizeFlag::Fixed, int across=20)
    {
        const SizeFlag xFlag=vertical?acrossFlag:alongFlag;
        const SizeFlag yFlag=vertical?alongFlag:acrossFlag;
        const Size size=vertical?Size(across, along):Size(along, across);

        block->setSizeFlags({xFlag, yFlag});
        if(xFlag == SizeFlag::Fixed)
            block->setWidth(size.width);
        if(yFlag == SizeFlag::Fixed)
            block->setHeight(size.height);
        block->want=size;
        block->invalidate();
    }

    //! a margin or padding given along and across the layout
    Margin sides(int alongLead, int alongTrail, int acrossLead, int acrossTrail) const
    {
        if(vertical)
            return Margin(acrossLead, alongLead, acrossTrail, alongTrail);
        return Margin(alongLead, acrossLead, alongTrail, acrossTrail);
    }

    int along(const Size &size) const { return vertical?size.height:size.width; }
    int across(const Size &size) const { return vertical?size.width:size.height; }
    SizeFlag across(const SizeFlags &flags) const { return vertical?flags.horizontal:flags.vertical; }
    int alongPos(const Rect &rect) const { return vertical?rect.y:rect.x; }
    int alongLength(const Rect &rect) const { return vertical?rect.h:rect.w; }
    int acrossPos(const Rect &rect) const { return vertical?rect.x:rect.y; }
    int acrossLength(const Rect &rect) const { return vertical?rect.w:rect.h; }

    //! where each block starts and how long it is along the layout
    std::string alongPlaces() const
    {
        std::string places;

        for(const Block *block:blocks)
        {
            if(!places.empty())
                places+=" ";
            places+=std::to_string(alongPos(block->getBounds()))+"+"+std::to_string(alongLength(block->getBounds()));
        }
        return places;
    }
};

//! every block of the two scenes has the same bounds
void checkSameBounds(const Scene &scene, const Scene &fresh)
{
    REQUIRE(scene.blocks.size() == fresh.blocks.size());
    for(size_t i=0; i<scene.blocks.size(); ++i)
    {
        CAPTURE(i);
        CHECK(text(scene.blocks[i]->getBounds()) == text(fresh.blocks[i]->getBounds()));
    }
}

}//namespace

TEST_CASE("Elastic children of an overflowing layout shrink by the same amount in a row and in a column")
{
    for(const bool vertical:{false, true})
    {
        CAPTURE(vertical);
        // room to spare first: the Elastic blocks share the 400-254 over
        Scene scene(vertical);
        Block *first=scene.add(SizeFlag::Elastic, 100);
        Block *second=scene.add(SizeFlag::Elastic, 100);
        scene.add(SizeFlag::Fixed, 50);
        scene.settle();
        REQUIRE(scene.alongLength(scene.panel->getBounds()) == PanelLength);
        CHECK(scene.alongPlaces() == "0+173 175+173 350+50");

        // 300+200+50 and the spacing twice is 554, 154 over the panel: the
        // Elastic blocks give 77 each and the Fixed one keeps its 50
        scene.set(first, SizeFlag::Elastic, 300);
        scene.set(second, SizeFlag::Elastic, 200);
        scene.settle();
        CHECK(scene.along(scene.layout->getPreferredSize()) == 554);
        CHECK(scene.alongPlaces() == "0+223 225+123 350+50");

        Scene fresh(vertical);
        fresh.add(SizeFlag::Elastic, 300);
        fresh.add(SizeFlag::Elastic, 200);
        fresh.add(SizeFlag::Fixed, 50);
        fresh.settle();
        CHECK(fresh.alongPlaces() == "0+223 225+123 350+50");
        checkSameBounds(scene, fresh);
    }
}

TEST_CASE("Bloat children of an overflowing layout give the space short first, in a row and in a column")
{
    for(const bool vertical:{false, true})
    {
        CAPTURE(vertical);
        // Bloat alone: 300+200+50 and the spacing twice is 154 over; all of it
        // comes out of the Bloat block, the Shrink one keeps its 200
        Scene scene(vertical);
        Block *bloat=scene.add(SizeFlag::Bloat, 100);
        scene.add(SizeFlag::Shrink, 200);
        scene.add(SizeFlag::Fixed, 50);
        scene.settle();
        // room to spare goes to the Bloat block too
        CHECK(scene.alongPlaces() == "0+146 148+200 350+50");

        scene.set(bloat, SizeFlag::Bloat, 300);
        scene.settle();
        CHECK(scene.along(scene.layout->getPreferredSize()) == 554);
        CHECK(scene.alongPlaces() == "0+146 148+200 350+50");

        Scene fresh(vertical);
        fresh.add(SizeFlag::Bloat, 300);
        fresh.add(SizeFlag::Shrink, 200);
        fresh.add(SizeFlag::Fixed, 50);
        fresh.settle();
        checkSameBounds(scene, fresh);

        // two Bloat blocks and a Shrink one between them: 604, 204 over,
        // 102 out of each Bloat block and none out of the Shrink one
        Scene mixed(vertical);
        mixed.add(SizeFlag::Bloat, 300);
        mixed.add(SizeFlag::Shrink, 150);
        mixed.add(SizeFlag::Bloat, 150);
        mixed.settle();
        CHECK(mixed.alongPlaces() == "0+198 200+150 352+48");
    }
}

TEST_CASE("a hidden layout child takes no space and no spacing, and shown again is laid out as in a fresh layout")
{
    for(const bool vertical:{false, true})
    {
        CAPTURE(vertical);
        Scene scene(vertical);
        Block *a=scene.add(SizeFlag::Fixed, 50);
        Block *b=scene.add(SizeFlag::Fixed, 60, SizeFlag::Fixed, 90);
        Block *c=scene.add(SizeFlag::Fixed, 70);
        scene.settle();
        CHECK(scene.alongPlaces() == "0+50 52+60 114+70");
        CHECK(scene.along(scene.layout->getPreferredSize()) == 184);
        CHECK(scene.across(scene.layout->getPreferredSize()) == 90);

        b->setHidden(true);
        scene.settle();

        // as if b were not there: c one spacing after a, and b's 90 across gone
        Scene without(vertical);
        without.add(SizeFlag::Fixed, 50);
        without.add(SizeFlag::Fixed, 70);
        without.settle();
        CHECK(text(a->getBounds()) == text(without.blocks[0]->getBounds()));
        CHECK(text(c->getBounds()) == text(without.blocks[1]->getBounds()));
        CHECK(scene.alongPos(c->getBounds()) == 52);
        CHECK(scene.along(scene.layout->getPreferredSize()) == 122);
        CHECK(scene.across(scene.layout->getPreferredSize()) == 20);
        CHECK(scene.along(scene.panel->getPreferredSize()) == 122);
        CHECK(scene.across(scene.panel->getPreferredSize()) == 20);

        b->setHidden(false);
        scene.settle();

        Scene fresh(vertical);
        fresh.add(SizeFlag::Fixed, 50);
        fresh.add(SizeFlag::Fixed, 60, SizeFlag::Fixed, 90);
        fresh.add(SizeFlag::Fixed, 70);
        fresh.settle();
        CHECK(scene.alongPlaces() == "0+50 52+60 114+70");
        checkSameBounds(scene, fresh);
        CHECK(scene.along(scene.layout->getPreferredSize()) == 184);
        CHECK(scene.across(scene.layout->getPreferredSize()) == 90);
    }
}

TEST_CASE("a layout's padding insets its children without growing them, and its spacing is the gap between them")
{
    for(const bool vertical:{false, true})
    {
        CAPTURE(vertical);
        Scene scene(vertical);
        Block *fixed=scene.add(SizeFlag::Fixed, 50, SizeFlag::Fixed, 20);
        Block *filling=scene.add(SizeFlag::Fixed, 50, SizeFlag::Expand, 20);
        Block *expand=scene.add(SizeFlag::Expand, 50, SizeFlag::Fixed, 20);
        scene.layout->setPadding(scene.sides(5, 7, 11, 13));
        scene.layout->setSpacing(6);
        scene.settle();

        // along: 5 in, 6 apart, and the Expand block takes the 400-174 over,
        // ending at the trailing padding
        CHECK(scene.along(scene.layout->getPreferredSize()) == 174);
        CHECK(scene.alongPlaces() == "5+50 61+50 117+276");
        CHECK(scene.alongPos(expand->getBounds())+scene.alongLength(expand->getBounds()) == PanelLength-7);

        // across: 11 in; a Fixed block keeps its 20, an Expand one fills the
        // 400 less the padding exactly
        for(const Block *block:scene.blocks)
            CHECK(scene.acrossPos(block->getBounds()) == 11);
        CHECK(scene.acrossLength(fixed->getBounds()) == 20);
        CHECK(scene.acrossLength(expand->getBounds()) == 20);
        CHECK(scene.acrossLength(filling->getBounds()) == PanelLength-11-13);
    }
}

TEST_CASE("a nested layout across a padded layout is its preferred length, not grown by the padding")
{
    for(const bool vertical:{false, true})
    {
        CAPTURE(vertical);
        // A Fixed control takes its request whatever its cell, so the cell
        // shows only in a child that is not a control: a nested layout of the
        // other direction, holding one Fixed 30x20 block
        Scene scene(vertical);
        controls::LayoutItem *nested;
        if(vertical)
            nested=new controls::HorizontalLayout();
        else
            nested=new controls::VerticalLayout();
        scene.layout->addLayout(nested);

        Block *inner=scene.panel->newChild<Block>("inner");
        nested->addControl(inner);
        scene.set(inner, SizeFlag::Fixed, 30, SizeFlag::Fixed, 20);

        scene.layout->setPadding(scene.sides(5, 7, 11, 13));
        scene.settle();

        REQUIRE(sizeFlagName(scene.across(nested->getSizeFlags())) == "Fixed");
        CHECK(scene.across(nested->getPreferredSize()) == 20);
        CHECK(scene.acrossPos(nested->getBounds()) == 11);
        CHECK(scene.acrossLength(nested->getBounds()) == 20);
        CHECK(scene.alongPos(nested->getBounds()) == 5);
        CHECK(scene.alongLength(nested->getBounds()) == 30);
        CHECK(scene.acrossPos(inner->getBounds()) == 11);
        CHECK(scene.alongPos(inner->getBounds()) == 5);
    }
}
