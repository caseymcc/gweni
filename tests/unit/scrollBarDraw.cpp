/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The textured skin draws a scroll bar as a ScrollTrack track the whole
 *  length of the bar (the BaseScrollBar renderer, which VerticalScrollBar and
 *  HorizontalScrollBar find through their parent type), a ScrollThumb thumb
 *  (ScrollThumbHover while hovered, ScrollThumbDown while dragged) in one slot,
 *  and arrows that are a kind-1 chevron in FgDark at the button's z+1 with no
 *  surface at rest (ScrollThumbHover / ScrollThumbDown on hover / press), so
 *  the thumb is the one filled shape on the bar.
 *
 *  With the real, un-init()'d TexturedSkin on a recording StubRenderer and a
 *  ScrollControl on a real Canvas that overflows both ways: the first frame
 *  draws each track over its bar's bounds, the thumb over its bounds above the
 *  track, and each arrow's chevron centred, pointing its way (0 down, 1 up,
 *  2 left, 3 right), with its surface hidden; a settled frame draws nothing. A
 *  nudge (a click on the down arrow through the input handler) draws, of the
 *  bars' slots, only the thumb's and that arrow's, and moves the thumb; a thumb
 *  drag only the thumb's (ScrollThumbDown while held); a hover on an arrow
 *  only that arrow's surface, hidden again once the mouse moves off, and on
 *  the thumb only the thumb. Content that stops overflowing hides the thumb's
 *  slot (bars kept shown), or the whole bar's (auto-hidden bars).
 */
#include "headlessFixture.h"

#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/horizontalScrollBar.h>
#include <gweni/controls/scrollBarBar.h>
#include <gweni/controls/scrollBarButton.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/verticalScrollBar.h>
#include <gweni/inputHandler.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdlib>
#include <vector>

using namespace gweni;
namespace tokyonight=gweni::skin::textured::tokyonight;

namespace
{

//! Records every rounded rect and icon drawn, and every slot shown or hidden.
class BarRecorder: public test::StubRenderer
{
public:
    struct Rounded
    {
        size_t id;
        Rect rect;
        Color top;
        int zIndex;
    };

    struct Icon
    {
        size_t id;
        Rect rect;
        int kind;
        int direction;
        Color color;
        int zIndex;
    };

    void drawRoundedRect(size_t primitiveId, Rect rect, float, float, Color top, Color, Color,
        int zIndex) override
    {
        rounded.push_back({primitiveId, rect, top, zIndex});
    }

    void drawIcon(size_t primitiveId, Rect rect, int kind, int direction, Color color,
        int zIndex) override
    {
        icons.push_back({primitiveId, rect, kind, direction, color, zIndex});
    }

    void showPrimitive(int style, size_t primitiveId) override
    {
        shown.push_back({style, primitiveId});
    }

    void hidePrimitive(size_t primitiveId) override
    {
        hidden.push_back(primitiveId);
    }

    //! The last rounded rect drawn into the slot, or null.
    const Rounded *lastRounded(size_t id) const
    {
        for(auto it=rounded.rbegin(); it != rounded.rend(); ++it)
        {
            if(it->id == id)
                return &*it;
        }
        return nullptr;
    }

    const Icon *lastIcon(size_t id) const
    {
        for(auto it=icons.rbegin(); it != icons.rend(); ++it)
        {
            if(it->id == id)
                return &*it;
        }
        return nullptr;
    }

    bool hid(size_t id) const
    {
        return std::find(hidden.begin(), hidden.end(), id) != hidden.end();
    }

    //! The style the slot was last shown with, -1 if it was not.
    int lastShownStyle(size_t id) const
    {
        int style=-1;

        for(const auto &entry : shown)
        {
            if(entry.second == id)
                style=entry.first;
        }
        return style;
    }

    //! Every slot drawn into, shown or hidden since the last clear().
    std::vector<size_t> touched() const
    {
        std::vector<size_t> ids;

        for(const Rounded &rect : rounded)
            ids.push_back(rect.id);
        for(const Icon &icon : icons)
            ids.push_back(icon.id);
        for(size_t id : hidden)
            ids.push_back(id);
        for(const auto &entry : shown)
            ids.push_back(entry.second);
        return ids;
    }

    void clear()
    {
        rounded.clear();
        icons.clear();
        shown.clear();
        hidden.clear();
    }

    std::vector<Rounded> rounded;
    std::vector<Icon> icons;
    std::vector<std::pair<int, size_t>> shown;
    std::vector<size_t> hidden;
};

//! The parts of one scroll bar: [0] the up/left arrow, [1] the down/right one.
struct BarParts
{
    controls::BaseScrollBar *bar=nullptr;
    controls::internal::ScrollBarButton *buttons[2]={nullptr, nullptr};
    controls::internal::ScrollBarBar *thumb=nullptr;
};

template<typename BarType>
BarParts findBar(controls::Base *scrollControl)
{
    BarParts parts;

    for(controls::Base *child : scrollControl->getActualChildren())
    {
        if(BarType *bar=gweni_cast<BarType>(child))
            parts.bar=bar;
    }
    REQUIRE(parts.bar != nullptr);

    for(controls::Base *child : parts.bar->getChildren())
    {
        if(auto *button=gweni_cast<controls::internal::ScrollBarButton>(child))
        {
            const Alignment direction=button->getDirection();
            const bool first=direction == Alignment::Top || direction == Alignment::Left;

            parts.buttons[first?0:1]=button;
        }
        else if(auto *thumb=gweni_cast<controls::internal::ScrollBarBar>(child))
            parts.thumb=thumb;
    }
    REQUIRE(parts.buttons[0] != nullptr);
    REQUIRE(parts.buttons[1] != nullptr);
    REQUIRE(parts.thumb != nullptr);
    return parts;
}

const std::vector<size_t> &ids(controls::Base *control)
{
    return control->getSkinControl()->getPrimitives();
}

//! A 200x150 ScrollControl at (20, 30) holding content that overflows it both
//! ways, drawn by the (un-init()'d) textured skin on a BarRecorder.
struct BarScene
{
    BarRecorder renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::ScrollControl *scroll=nullptr;
    controls::Button *content=nullptr;
    BarParts vertical;
    BarParts horizontal;

    explicit BarScene(bool keepBars=false)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 400);

        scroll=canvas->newChild<controls::ScrollControl>();
        scroll->setPos(20, 30);
        scroll->setSize(200, 150);
        scroll->setAutoHideBars(!keepBars);

        content=scroll->newChild<controls::Button>();
        content->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        content->setSize(500, 600);

        frame();
        vertical=findBar<controls::VerticalScrollBar>(scroll);
        horizontal=findBar<controls::HorizontalScrollBar>(scroll);
    }

    BarScene(const BarScene &)=delete;
    BarScene &operator=(const BarScene &)=delete;

    ~BarScene()
    {
        // the hovered control is process-global
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
    }

    void frame()
    {
        renderer.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void moveTo(int x, int y)
    {
        canvas->inputMouseMoved(x, y, 0, 0);
        frame();
    }

    Point centreOf(controls::Base *control) const
    {
        const Point at=control->localPosToCanvas();

        return Point(at.x+control->getWidth()/2, at.y+control->getHeight()/2);
    }

    void moveOnto(controls::Base *control)
    {
        const Point at=centreOf(control);

        moveTo(at.x, at.y);
    }

    //! Every slot of both bars: tracks, thumbs and arrows.
    std::vector<size_t> barSlots() const
    {
        std::vector<size_t> slots;

        for(const BarParts *parts : {&vertical, &horizontal})
        {
            for(controls::Base *part : {(controls::Base *)parts->bar, (controls::Base *)parts->thumb,
                (controls::Base *)parts->buttons[0], (controls::Base *)parts->buttons[1]})
            {
                const std::vector<size_t> &own=ids(part);

                slots.insert(slots.end(), own.begin(), own.end());
            }
        }
        return slots;
    }

    //! The bars' slots touched since the last frame() that are not among \p allowed's.
    std::vector<size_t> barSlotsTouchedOutside(std::initializer_list<controls::Base *> allowed,
        const std::vector<size_t> &touched) const
    {
        const std::vector<size_t> slots=barSlots();
        std::vector<size_t> outside;

        for(size_t id : touched)
        {
            if(std::find(slots.begin(), slots.end(), id) == slots.end())
                continue;

            bool own=false;

            for(controls::Base *control : allowed)
            {
                const std::vector<size_t> &controlIds=ids(control);

                own=own || std::find(controlIds.begin(), controlIds.end(), id) != controlIds.end();
            }
            if(!own)
                outside.push_back(id);
        }
        return outside;
    }
};

bool centredIn(const Rect &inner, const Rect &outer)
{
    return std::abs((inner.x*2+inner.w)-(outer.x*2+outer.w)) <= 1 &&
        std::abs((inner.y*2+inner.h)-(outer.y*2+outer.h)) <= 1;
}

}//namespace

TEST_CASE("the textured skin draws a scroll bar's track, thumb and chevron arrows, and a settled frame nothing")
{
    const test::IdBankCounts baseline=test::IdBankCounts::now();
    {
        // the constructor's frame() cleared the recorder before it drew, so the
        // recorder holds the first frame
        BarScene scene;
        BarRecorder &renderer=scene.renderer;

        for(const BarParts *parts : {&scene.vertical, &scene.horizontal})
        {
            CAPTURE(parts->bar->getTypeName());
            REQUIRE(parts->bar->visible());
            REQUIRE(parts->thumb->visible());

            CHECK(dynamic_cast<const skin::textured::BaseScrollBar *>(parts->bar->getSkinControl()) != nullptr);
            CHECK(dynamic_cast<const skin::textured::ScrollBarBar *>(parts->thumb->getSkinControl()) != nullptr);

            // the track: one ScrollTrack rect over the whole bar
            REQUIRE(ids(parts->bar).size() == 1);
            const BarRecorder::Rounded *track=renderer.lastRounded(ids(parts->bar)[0]);
            REQUIRE(track != nullptr);
            CHECK((track->rect == parts->bar->getRenderBounds()));
            CHECK((track->top == tokyonight::ScrollTrack.top));
            CHECK(track->zIndex == parts->bar->getZIndex());
            CHECK(renderer.lastShownStyle(ids(parts->bar)[0]) == 4);

            // the thumb: one ScrollThumb rect over its bounds, above the track
            REQUIRE(ids(parts->thumb).size() == 1);
            const BarRecorder::Rounded *thumb=renderer.lastRounded(ids(parts->thumb)[0]);
            REQUIRE(thumb != nullptr);
            CHECK((thumb->rect == parts->thumb->getRenderBounds()));
            CHECK((thumb->top == tokyonight::ScrollThumb.top));
            CHECK(thumb->zIndex > track->zIndex);
            CHECK(renderer.lastShownStyle(ids(parts->thumb)[0]) == 4);

            // each arrow: no surface, a centred FgDark chevron at its z+1
            for(controls::internal::ScrollBarButton *button : parts->buttons)
            {
                const std::vector<size_t> &own=ids(button);

                REQUIRE(own.size() == 2);
                CHECK(renderer.hid(own[0]));
                CHECK(renderer.lastRounded(own[0]) == nullptr);

                const BarRecorder::Icon *chevron=renderer.lastIcon(own[1]);
                REQUIRE(chevron != nullptr);
                CHECK(chevron->kind == 1);
                CHECK((chevron->color == tokyonight::FgDark));
                CHECK(chevron->zIndex == button->getZIndex()+1);
                CHECK(chevron->rect.w == chevron->rect.h);
                CHECK(chevron->rect.w > 0);
                CHECK(centredIn(chevron->rect, button->getRenderBounds()));
            }
        }

        // kind 1's directions: 0 down, 1 up, 2 left, 3 right
        CHECK(renderer.lastIcon(ids(scene.vertical.buttons[0])[1])->direction == 1);
        CHECK(renderer.lastIcon(ids(scene.vertical.buttons[1])[1])->direction == 0);
        CHECK(renderer.lastIcon(ids(scene.horizontal.buttons[0])[1])->direction == 2);
        CHECK(renderer.lastIcon(ids(scene.horizontal.buttons[1])[1])->direction == 3);

        // the arrows are in front of the track, and the thumb lies between them
        CHECK(scene.vertical.buttons[0]->getZIndex() > scene.vertical.bar->getZIndex());

        scene.frame();
        CHECK(scene.renderer.touched().empty());
    }
    CHECK(test::IdBankCounts::now().livePrimitives() == baseline.livePrimitives());
}

TEST_CASE("a nudge on a scroll bar's arrow draws only the thumb and that arrow of the bars")
{
    BarScene scene;
    controls::internal::ScrollBarButton *down=scene.vertical.buttons[1];
    controls::internal::ScrollBarBar *thumb=scene.vertical.thumb;
    const int thumbY=thumb->getY();

    scene.moveOnto(down);
    REQUIRE(down->isHovered());
    std::vector<size_t> touched=scene.renderer.touched();

    scene.canvas->inputMouseButton(0, true);
    scene.frame();
    CHECK(down->isDepressed());
    CHECK((scene.renderer.lastRounded(ids(down)[0]) != nullptr &&
        scene.renderer.lastRounded(ids(down)[0])->top == tokyonight::ScrollThumbDown.top));
    for(size_t id : scene.renderer.touched())
        touched.push_back(id);

    scene.canvas->inputMouseButton(0, false);
    scene.frame();
    for(size_t id : scene.renderer.touched())
        touched.push_back(id);

    // until it settles
    for(int i=0; i < 3; ++i)
    {
        scene.frame();
        for(size_t id : scene.renderer.touched())
            touched.push_back(id);
    }

    CHECK(thumb->getY() > thumbY);
    CHECK(scene.barSlotsTouchedOutside({down, thumb}, touched).empty());
    // the thumb was redrawn where it moved to
    CHECK(std::find(touched.begin(), touched.end(), ids(thumb)[0]) != touched.end());

    scene.frame();
    CHECK(scene.renderer.touched().empty());
}

TEST_CASE("a thumb drag draws only the thumb of the bars, ScrollThumbDown while it is held")
{
    BarScene scene;
    controls::internal::ScrollBarBar *thumb=scene.vertical.thumb;
    const size_t slot=ids(thumb)[0];

    // a hover on the thumb draws only the thumb
    const Point at=scene.centreOf(thumb);
    scene.moveTo(at.x, at.y);
    REQUIRE(thumb->isHovered());
    CHECK(scene.barSlotsTouchedOutside({thumb}, scene.renderer.touched()).empty());
    REQUIRE(scene.renderer.lastRounded(slot) != nullptr);
    CHECK((scene.renderer.lastRounded(slot)->top == tokyonight::ScrollThumbHover.top));

    scene.canvas->inputMouseButton(0, true);
    scene.frame();
    REQUIRE(thumb->isDepressed());
    CHECK(scene.barSlotsTouchedOutside({thumb}, scene.renderer.touched()).empty());
    REQUIRE(scene.renderer.lastRounded(slot) != nullptr);
    CHECK((scene.renderer.lastRounded(slot)->top == tokyonight::ScrollThumbDown.top));

    const int thumbY=thumb->getY();

    for(int step=1; step <= 3; ++step)
    {
        CAPTURE(step);
        scene.canvas->inputMouseMoved(at.x, at.y+step*10, 0, 10);
        scene.frame();
        CHECK(scene.barSlotsTouchedOutside({thumb}, scene.renderer.touched()).empty());
        const BarRecorder::Rounded *drawn=scene.renderer.lastRounded(slot);
        REQUIRE(drawn != nullptr);
        CHECK((drawn->top == tokyonight::ScrollThumbDown.top));
        CHECK((drawn->rect == thumb->getRenderBounds()));
    }
    CHECK(thumb->getY() == thumbY+30);
    CHECK(scene.vertical.bar->getScrolledAmount() > 0.f);

    scene.canvas->inputMouseButton(0, false);
    scene.frame();
    CHECK(!thumb->isDepressed());
    CHECK(scene.barSlotsTouchedOutside({thumb}, scene.renderer.touched()).empty());

    scene.frame();
    scene.frame();
    CHECK(scene.renderer.touched().empty());
}

TEST_CASE("a hover on a scroll bar's arrow draws only its surface, hidden again once the mouse moves off")
{
    BarScene scene;
    controls::internal::ScrollBarButton *left=scene.horizontal.buttons[0];
    const std::vector<size_t> &own=ids(left);

    scene.moveOnto(left);
    REQUIRE(left->isHovered());
    CHECK(scene.barSlotsTouchedOutside({left}, scene.renderer.touched()).empty());
    const BarRecorder::Rounded *surface=scene.renderer.lastRounded(own[0]);
    REQUIRE(surface != nullptr);
    CHECK((surface->top == tokyonight::ScrollThumbHover.top));
    CHECK((surface->rect == left->getRenderBounds()));

    scene.frame();
    CHECK(scene.renderer.touched().empty());

    // off the bar, onto the canvas
    scene.moveTo(300, 300);
    CHECK(!left->isHovered());
    CHECK(scene.barSlotsTouchedOutside({left}, scene.renderer.touched()).empty());
    CHECK(scene.renderer.hid(own[0]));
    CHECK(scene.renderer.lastRounded(own[0]) == nullptr);
    REQUIRE(scene.renderer.lastIcon(own[1]) != nullptr);
    CHECK((scene.renderer.lastIcon(own[1])->color == tokyonight::FgDark));

    scene.frame();
    CHECK(scene.renderer.touched().empty());
}

TEST_CASE("content that stops overflowing hides the thumb's slot, or with auto-hidden bars the whole bar's")
{
    SUBCASE("bars kept shown")
    {
        BarScene scene(true);

        scene.content->setSize(100, 80);
        scene.frame();

        for(const BarParts *parts : {&scene.vertical, &scene.horizontal})
        {
            CAPTURE(parts->bar->getTypeName());
            REQUIRE(parts->bar->visible());
            CHECK(!parts->thumb->visible());
            CHECK(scene.renderer.lastShownStyle(ids(parts->thumb)[0]) == 0);
            // the track stays
            CHECK(scene.renderer.lastShownStyle(ids(parts->bar)[0]) != 0);
            CHECK(!scene.renderer.hid(ids(parts->bar)[0]));
        }

        scene.frame();
        CHECK(scene.renderer.touched().empty());
    }

    SUBCASE("auto-hidden bars")
    {
        BarScene scene;

        scene.content->setSize(100, 80);
        scene.frame();

        CHECK(!scene.vertical.bar->visible());
        CHECK(!scene.horizontal.bar->visible());
        CHECK(scene.renderer.lastShownStyle(ids(scene.vertical.bar)[0]) == 0);
        CHECK(scene.renderer.lastShownStyle(ids(scene.vertical.thumb)[0]) == 0);
        CHECK(scene.renderer.lastShownStyle(ids(scene.horizontal.bar)[0]) == 0);
        CHECK(scene.renderer.lastShownStyle(ids(scene.horizontal.thumb)[0]) == 0);
    }
}
