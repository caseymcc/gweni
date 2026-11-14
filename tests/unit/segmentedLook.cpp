/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The textured SegmentedButton renderer on a recording StubRenderer, with the
 *  real (un-init()'d) textured skin and Tokyo Night's compiled surfaces.
 *
 *  Only the row's ends are rounded: the first segment is square on its right,
 *  the last on its left, the inner ones on both sides, a lone segment nowhere.
 *  A segment after the first is drawn from Joint units into its bounds with no
 *  border on its left, so its fill starts where its left neighbour's bounds
 *  end and the shared column is its neighbour's border alone; adding or
 *  removing a segment restyles the new ends. Each state draws its surface
 *  (Segment, SegmentHover, SegmentDown, SegmentOn, SegmentOnHover,
 *  SegmentDisabled, SegmentOnDisabled) and its text in the skin's
 *  SegmentedButton colours. The check icon is drawn at the segment's z+1 in
 *  getCheckRect() while it is selected in a row that shows checks, and hidden
 *  otherwise. The segment holding the keyboard focus is drawn over its whole
 *  bounds, covering its left joint, its border SegmentFocus's on every side,
 *  its fill its state's. A hover writes the hovered segment's fill slot alone, an
 *  exclusive click only the two segments' slots, a multi toggle one
 *  segment's, and a settled frame nothing.
 */
#include "headlessFixture.h"

#include <gweni/controls/segmentedButtons.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <map>
#include <set>
#include <vector>

using namespace gweni;
using controls::SegmentedButton;
using controls::SegmentedButtons;
namespace tn=skin::textured::tokyonight;

namespace
{

//! Each slot's last rounded rect and icon, which slots are hidden, and every
//! slot a frame wrote.
class Recorder: public test::StubRenderer
{
public:
    using test::StubRenderer::drawRoundedRect;

    struct Draw
    {
        Rect rect;
        Color top, bottom, border;
        uint8_t edges;
    };

    struct Icon
    {
        Rect rect;
        int kind;
        Color color;
        int z;
    };

    void drawRoundedRect(size_t primitiveId, Rect rect, float radius, float border, Color top,
        Color bottom, Color borderColor, int zIndex) override
    {
        draws[primitiveId]={rect, top, bottom, borderColor, 0};
        hidden.erase(primitiveId);
        touched.push_back(primitiveId);
        test::StubRenderer::drawRoundedRect(primitiveId, rect, radius, border, top, bottom,
            borderColor, zIndex);
    }

    void drawRoundedRect(size_t primitiveId, Rect rect, float radius, float border, Color top,
        Color bottom, Color borderColor, int zIndex, uint8_t edges, float reach) override
    {
        drawRoundedRect(primitiveId, rect, radius, border, top, bottom, borderColor, zIndex);
        draws[primitiveId].edges=edges;
    }

    void drawIcon(size_t primitiveId, Rect rect, int kind, int direction, Color color, int zIndex) override
    {
        icons[primitiveId]={rect, kind, color, zIndex};
        hidden.erase(primitiveId);
        touched.push_back(primitiveId);
        test::StubRenderer::drawIcon(primitiveId, rect, kind, direction, color, zIndex);
    }

    void hidePrimitive(size_t primitiveId) override
    {
        hidden.insert(primitiveId);
        touched.push_back(primitiveId);
        test::StubRenderer::hidePrimitive(primitiveId);
    }

    std::map<size_t, Draw> draws;
    std::map<size_t, Icon> icons;
    std::set<size_t> hidden;
    std::vector<size_t> touched;
};

struct Scene
{
    Recorder renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    SegmentedButtons *row=nullptr;
    float clock=nextClock();

    static float nextClock()
    {
        static float start=0.0f;

        start+=1000.0f;
        return start;
    }

    explicit Scene(std::initializer_list<const char *> texts={"Day", "Week", "Month"})
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);
        row=canvas->newChild<SegmentedButtons>();
        row->setPos(20, 20);
        for(const char *text : texts)
            row->addSegment(text);
        settle();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        input::onMouseClicked(canvas, 0, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    void frame()
    {
        renderer.touched.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void settle()
    {
        for(int i=0; i < 3; ++i)
            frame();
    }

    void moveTo(controls::Base *over)
    {
        const Point at=over->localPosToCanvas();

        canvas->inputMouseMoved(at.x+over->getWidth()/2, at.y+over->getHeight()/2, 0, 0);
        frame();
    }

    void moveOff()
    {
        canvas->inputMouseMoved(639, 479, 0, 0);
        frame();
    }

    void click(controls::Base *over)
    {
        clock+=1.0f;
        moveTo(over);
        canvas->inputMouseButton(0, true);
        canvas->inputMouseButton(0, false);
        frame();
    }

    //! The segment's fill slot [0] and check slot [1].
    static std::vector<size_t> slotsOf(const controls::Base *control)
    {
        std::vector<size_t> primitives, texts;

        control->getSkinControl()->appendIds(primitives, texts);
        REQUIRE(primitives.size() == 2);
        return primitives;
    }

    const Recorder::Draw &fillOf(int index)
    {
        const size_t id=slotsOf(row->getSegment(index))[0];

        REQUIRE(renderer.draws.count(id) == 1);
        return renderer.draws.at(id);
    }

    bool checkShown(int index)
    {
        const size_t id=slotsOf(row->getSegment(index))[1];

        return renderer.icons.count(id) == 1 && renderer.hidden.count(id) == 0;
    }

    //! The slots the last frame wrote, as the segments that own them.
    std::set<int> segmentsTouched()
    {
        std::set<int> segments;

        for(size_t id : renderer.touched)
        {
            bool found=false;

            for(int i=0; i < row->segmentCount(); ++i)
            {
                const std::vector<size_t> slots=slotsOf(row->getSegment(i));

                if(std::find(slots.begin(), slots.end(), id) != slots.end())
                {
                    segments.insert(i);
                    found=true;
                }
            }
            if(!found)
                segments.insert(-1);
        }
        return segments;
    }
};

bool sameColor(const Color &a, const Color &b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

void checkSurface(const Recorder::Draw &draw, const tn::Surface &surface)
{
    CHECK(sameColor(draw.top, surface.top));
    CHECK(sameColor(draw.bottom, surface.bottom));
    CHECK(sameColor(draw.border, surface.border));
}

const uint8_t kLeftJoined=renderer::SquareTopLeft|renderer::SquareBottomLeft|renderer::NoBorderLeft;
const uint8_t kRightJoined=renderer::SquareTopRight|renderer::SquareBottomRight;

}//namespace

TEST_CASE("Segmented look: only the row's ends are rounded, each joint drawn by the left segment")
{
    Scene scene({"A", "Bb", "Ccc", "Dddd"});
    SegmentedButtons *row=scene.row;

    CHECK(scene.fillOf(0).edges == kRightJoined);
    CHECK(scene.fillOf(1).edges == (kLeftJoined|kRightJoined));
    CHECK(scene.fillOf(2).edges == (kLeftJoined|kRightJoined));
    CHECK(scene.fillOf(3).edges == kLeftJoined);

    for(int i=0; i < row->segmentCount(); ++i)
    {
        const SegmentedButton *segment=row->getSegment(i);
        const Rect bounds=segment->getRenderBounds();
        const Rect drawn=scene.fillOf(i).rect;
        const int joint=(i == 0)?0:SegmentedButtons::Joint;

        CHECK(drawn.x == bounds.x+joint);
        CHECK(drawn.w == bounds.w-joint);
        CHECK(drawn.y == bounds.y);
        CHECK(drawn.h == bounds.h);
        // the fill starts where the left neighbour's bounds end, on the canvas
        if(i > 0)
            CHECK(segment->getX()+drawn.x-bounds.x == row->getSegment(i-1)->right());
    }

    // the ends follow the segments added and removed
    row->removeSegment(0);
    scene.settle();
    CHECK(scene.fillOf(0).edges == kRightJoined);
    CHECK(scene.fillOf(0).rect.x == row->getSegment(0)->getRenderBounds().x);
    row->addSegment("E");
    scene.settle();
    CHECK(scene.fillOf(2).edges == (kLeftJoined|kRightJoined));
    CHECK(scene.fillOf(3).edges == kLeftJoined);
    // the last one gone, the one before it is the end, though nothing else
    // about it changed (a row of a fixed size, which a removal does not
    // resize, so its segments' clip region stays)
    row->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    row->setSize(400, row->getHeight());
    scene.settle();
    row->removeSegment(3);
    scene.settle();
    CHECK(scene.fillOf(2).edges == kLeftJoined);
    while(row->segmentCount() > 1)
        row->removeSegment(1);
    scene.settle();
    CHECK(scene.fillOf(0).edges == 0);
}

TEST_CASE("Segmented look: each state draws its surface and its text colour")
{
    Scene scene;
    SegmentedButtons *row=scene.row;
    const auto &colors=scene.skin.Colors.SegmentedButton;

    checkSurface(scene.fillOf(0), tn::SegmentOn);
    checkSurface(scene.fillOf(1), tn::Segment);
    CHECK(sameColor(row->getSegment(0)->getTextColor(), colors.Selected));
    CHECK(sameColor(row->getSegment(1)->getTextColor(), colors.Normal));

    scene.moveTo(row->getSegment(1));
    checkSurface(scene.fillOf(1), tn::SegmentHover);
    scene.moveTo(row->getSegment(0));
    checkSurface(scene.fillOf(0), tn::SegmentOnHover);
    checkSurface(scene.fillOf(1), tn::Segment);

    // the press focuses it: its state's fill, the focus border
    scene.canvas->inputMouseButton(0, true);
    scene.frame();
    CHECK(sameColor(scene.fillOf(0).top, tn::SegmentDown.top));
    CHECK(sameColor(scene.fillOf(0).bottom, tn::SegmentDown.bottom));
    CHECK(sameColor(scene.fillOf(0).border, tn::SegmentFocus.border));
    scene.canvas->inputMouseButton(0, false);
    scene.moveOff();
    CHECK(sameColor(scene.fillOf(0).top, tn::SegmentOn.top));
    CHECK(sameColor(scene.fillOf(0).border, tn::SegmentFocus.border));

    // disabled, it keeps the focus but not the focused look

    row->setDisabled(true);
    scene.frame();
    checkSurface(scene.fillOf(0), tn::SegmentOnDisabled);
    checkSurface(scene.fillOf(1), tn::SegmentDisabled);
    CHECK(sameColor(row->getSegment(0)->getTextColor(), colors.Disabled));
    CHECK(sameColor(row->getSegment(1)->getTextColor(), colors.Disabled));
    row->setDisabled(false);
    scene.frame();
    checkSurface(scene.fillOf(1), tn::Segment);
    CHECK(sameColor(row->getSegment(1)->getTextColor(), colors.Normal));
}

TEST_CASE("Segmented look: the check icon shows on a selected segment of a row that shows checks")
{
    Scene scene;
    SegmentedButtons *row=scene.row;

    CHECK_FALSE(scene.checkShown(0));
    row->setShowCheck(true);
    scene.settle();
    CHECK(scene.checkShown(0));
    CHECK_FALSE(scene.checkShown(1));

    SegmentedButton *segment=row->getSegment(0);
    const Recorder::Icon &icon=scene.renderer.icons.at(Scene::slotsOf(segment)[1]);
    const Rect bounds=segment->getRenderBounds();
    const Rect &check=segment->getCheckRect();

    CHECK(icon.kind == 0);
    CHECK(icon.rect.x == bounds.x+check.x);
    CHECK(icon.rect.y == bounds.y+check.y);
    CHECK(icon.rect.w == SegmentedButton::CheckSize);
    CHECK(icon.rect.h == SegmentedButton::CheckSize);
    CHECK(icon.z == segment->getZIndex()+1);
    CHECK(sameColor(icon.color, scene.skin.Colors.SegmentedButton.Check));
    // before the text
    CHECK(check.x+check.w <= segment->textX());

    scene.click(row->getSegment(2));
    CHECK_FALSE(scene.checkShown(0));
    CHECK(scene.checkShown(2));

    row->setShowCheck(false);
    scene.settle();
    CHECK_FALSE(scene.checkShown(2));
}

TEST_CASE("Segmented look: a hover, a click and a toggle write only the segments they change")
{
    Scene scene({"A", "B", "C", "D"});
    SegmentedButtons *row=scene.row;

    row->setShowCheck(true);
    scene.settle();
    scene.frame();
    CHECK(scene.renderer.touched.empty());

    scene.moveTo(row->getSegment(2));
    CHECK(scene.segmentsTouched() == std::set<int>{2});
    scene.frame();
    CHECK(scene.renderer.touched.empty());
    scene.moveOff();
    CHECK(scene.segmentsTouched() == std::set<int>{2});

    // the hover and the press come and go on 2, the selection leaves 0
    scene.click(row->getSegment(2));
    scene.moveOff();
    CHECK(row->getSelectedIndex() == 2);
    scene.frame();
    CHECK(scene.renderer.touched.empty());

    // the selection alone, from the app: the two segments
    row->setSelected(1, true);
    scene.frame();
    CHECK(scene.segmentsTouched() == std::set<int>({1, 2}));

    row->setMode(SegmentedButtons::Mode::Multi);
    scene.frame();
    row->setSelected(3, true);
    scene.frame();
    CHECK(scene.segmentsTouched() == std::set<int>{3});
}

TEST_CASE("Segmented look: the focused segment draws its whole outline in the focus colour")
{
    Scene scene({"A", "B", "C"});
    SegmentedButtons *row=scene.row;

    row->setSelected(1, true);
    row->getSegment(1)->focus();
    scene.frame();

    const SegmentedButton *segment=row->getSegment(1);
    const Recorder::Draw &fill=scene.fillOf(1);
    const Rect bounds=segment->getRenderBounds();

    CHECK(fill.rect.x == bounds.x);
    CHECK(fill.rect.w == bounds.w);
    CHECK(fill.edges == (renderer::SquareTopLeft|renderer::SquareBottomLeft|kRightJoined));
    CHECK(sameColor(fill.border, tn::SegmentFocus.border));
    CHECK(sameColor(fill.top, tn::SegmentOn.top));
    // the others as they were
    CHECK(sameColor(scene.fillOf(2).border, tn::Segment.border));
    CHECK(scene.fillOf(2).edges == kLeftJoined);

    // moved by a key with the selection: only the two segments are written,
    // and the one it left is joined again
    scene.renderer.touched.clear();
    input::onKeyEvent(scene.canvas, keys::Right, true);
    input::onKeyEvent(scene.canvas, keys::Right, false);
    scene.frame();
    CHECK(scene.segmentsTouched() == std::set<int>({1, 2}));
    CHECK(sameColor(scene.fillOf(2).border, tn::SegmentFocus.border));
    CHECK(scene.fillOf(2).rect.x == row->getSegment(2)->getRenderBounds().x);
    CHECK(scene.fillOf(1).edges == (kLeftJoined|kRightJoined));
    CHECK(scene.fillOf(1).rect.x == row->getSegment(1)->getRenderBounds().x+SegmentedButtons::Joint);
    CHECK(sameColor(scene.fillOf(1).border, tn::Segment.border));

    KeyboardFocus->unfocus();
    scene.frame();
    CHECK(sameColor(scene.fillOf(2).border, tn::SegmentOn.border));
    CHECK(scene.fillOf(2).edges == kLeftJoined);
}
