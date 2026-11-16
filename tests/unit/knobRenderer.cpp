/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The textured Knob renderer on a real Canvas with the real (un-init()'d)
 *  textured skin on a recording StubRenderer, its writes read per frame from
 *  writtenPrimitiveIds() and its lines from the stub's `lines`.
 *
 *  Its slots are the body, Knob::ArcSegments track segments, as many value
 *  segments, the indicator and one per tick, in that order; the body is a
 *  disc (radius half its side) at the knob's z, the track and the ticks at
 *  z+1, the value arc over the track at z+2 and the indicator on the body at
 *  z+1. The track is one polyline, each segment drawn with the grid points
 *  next to it as its neighbours. A value change writes the indicator and the
 *  value segments whose covered part changed alone: the one the value ends
 *  in, and those it crossed (shown or hidden); a whole segment keeps its slot
 *  and its neighbours. A bipolar knob's arc runs from the range's 0, nothing
 *  at 0. Hover, press and focus rewrite the body's slot alone, a settled
 *  frame writes nothing, the value text's glyphs are written only for a new
 *  string, a disabled knob draws its body and value in the disabled
 *  surfaces, a hidden knob hides every slot and is drawn again once shown,
 *  and a new tick count resizes the list's tail.
 */
#include "headlessFixture.h"

#include <gweni/controls/knob.h>
#include <gweni/controls/text.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <map>
#include <set>

using namespace gweni;
using controls::Knob;
namespace tokyonight=skin::textured::tokyonight;

namespace
{

constexpr size_t Segments=Knob::ArcSegments;

//! The stub, also keeping each body's fill and border as last drawn.
class BodyRecorder: public test::StubRenderer
{
public:
    struct Body
    {
        Rect rect;
        float radius;
        Color fill;
        Color border;
        int z;
    };

    void drawRoundedRect(size_t primitiveId, Rect rect, float radius, float border, Color top,
        Color bottom, Color borderColor, int zIndex) override
    {
        bodies[primitiveId]={rect, radius, top, borderColor, zIndex};
        test::StubRenderer::drawRoundedRect(primitiveId, rect, radius, border, top, bottom,
            borderColor, zIndex);
    }

    std::map<size_t, Body> bodies;
};

struct Scene
{
    BodyRecorder renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    float clock=nextClock();

    static float nextClock()
    {
        static float start=700.0f;

        start+=1000.0f;
        return start;
    }

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);
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
        renderer.lines.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    Knob *knob()
    {
        Knob *knob=canvas->newChild<Knob>();

        knob->setPos(100, 100);
        frame();
        return knob;
    }

    //! The ids the last frame wrote.
    std::set<size_t> written() const
    {
        const std::vector<size_t> &ids=renderer.writtenPrimitiveIds();

        return std::set<size_t>(ids.begin(), ids.end());
    }
};

//! The knob's slots, in the renderer's order.
const std::vector<size_t> &slotsOf(Knob *knob)
{
    return knob->getSkinControl()->getPrimitives();
}

size_t bodySlot(Knob *knob) { return slotsOf(knob)[0]; }
size_t trackSlot(Knob *knob, size_t i) { return slotsOf(knob)[1+i]; }
size_t valueSlot(Knob *knob, size_t i) { return slotsOf(knob)[1+Segments+i]; }
size_t indicatorSlot(Knob *knob) { return slotsOf(knob)[1+2*Segments]; }

//! The value segments [first, last] and the indicator.
std::set<size_t> valueWrites(Knob *knob, size_t first, size_t last)
{
    std::set<size_t> ids{indicatorSlot(knob)};

    for(size_t i=first; i <= last; ++i)
        ids.insert(valueSlot(knob, i));
    return ids;
}

//! The last line drawn in \p id this frame, or null.
const test::StubRenderer::LineDraw *lineOf(const Scene &scene, size_t id)
{
    for(auto it=scene.renderer.lines.rbegin(); it != scene.renderer.lines.rend(); ++it)
    {
        if(it->id == id)
            return &*it;
    }
    return nullptr;
}

}//namespace

TEST_CASE("Knob renderer: the body, the track, the value arc, the indicator and the ticks, in that order")
{
    Scene scene;
    Knob *knob=scene.knob();

    REQUIRE(slotsOf(knob).size() == 2+2*Segments);
    const int z=knob->getZIndex();
    const Rect bounds=knob->getRenderBounds();
    const Knob::Geometry geometry=knob->getGeometry();
    const PointF centre(bounds.x+geometry.centre.x, bounds.y+geometry.centre.y);

    // the body: a disc at the knob's z
    REQUIRE(scene.renderer.bodies.count(bodySlot(knob)));
    const BodyRecorder::Body &body=scene.renderer.bodies[bodySlot(knob)];
    CHECK(body.rect.w == body.rect.h);
    CHECK(body.radius == doctest::Approx(body.rect.w*0.5f));
    CHECK(body.z == z);
    CHECK((body.fill == tokyonight::KnobBody.top));

    // the track: one polyline round the sweep, its joints given
    for(size_t i=0; i < Segments; ++i)
    {
        CAPTURE(i);
        const auto *line=lineOf(scene, trackSlot(knob, i));
        REQUIRE(line);
        CHECK(line->zIndex == z+1);
        CHECK((line->color == tokyonight::KnobTrack.top));
        CHECK(line->thickness == geometry.arcThickness);
        CHECK(line->prev.has_value() == (i > 0));
        CHECK(line->next.has_value() == (i+1 < Segments));
        if(i > 0)
        {
            const auto *before=lineOf(scene, trackSlot(knob, i-1));
            CHECK((line->p0 == before->p1));
            CHECK((*line->prev == before->p0));
        }
    }
    const PointF first=Knob::pointAt(centre, knob->getStartAngle(), geometry.arcRadius);
    const PointF last=Knob::pointAt(centre, knob->getEndAngle(), geometry.arcRadius);
    CHECK(lineOf(scene, trackSlot(knob, 0))->p0.x == doctest::Approx(first.x));
    CHECK(lineOf(scene, trackSlot(knob, 0))->p0.y == doctest::Approx(first.y));
    CHECK(lineOf(scene, trackSlot(knob, Segments-1))->p1.x == doctest::Approx(last.x));
    CHECK(lineOf(scene, trackSlot(knob, Segments-1))->p1.y == doctest::Approx(last.y));

    // at the minimum the value arc draws nothing, the indicator points at the start
    for(size_t i=0; i < Segments; ++i)
        CHECK_FALSE(lineOf(scene, valueSlot(knob, i)));
    const auto *indicator=lineOf(scene, indicatorSlot(knob));
    REQUIRE(indicator);
    CHECK(indicator->zIndex == z+1);
    CHECK((indicator->color == tokyonight::KnobIndicator.top));
    const PointF tip=Knob::pointAt(centre, knob->getStartAngle(), geometry.indicatorOuter);
    CHECK(indicator->p1.x == doctest::Approx(tip.x));
    CHECK(indicator->p1.y == doctest::Approx(tip.y));
    CHECK(const_cast<skin::Control *>(knob->getSkinControl())->depthLayers() == 3);

    // ticks: one slot each at the list's tail, outside the arc
    knob->setTickCount(5);
    scene.frame();
    REQUIRE(slotsOf(knob).size() == 2+2*Segments+5);
    const Knob::Geometry ticked=knob->getGeometry();
    CHECK(ticked.tickInner > ticked.arcRadius+ticked.arcThickness*0.5f);
    for(size_t i=0; i < 5; ++i)
    {
        CAPTURE(i);
        const auto *tick=lineOf(scene, slotsOf(knob)[2+2*Segments+i]);
        REQUIRE(tick);
        CHECK((tick->color == tokyonight::KnobTick.top));
        CHECK(tick->zIndex == z+1);
    }
    knob->setTickCount(0);
    scene.frame();
    CHECK(slotsOf(knob).size() == 2+2*Segments);
}

TEST_CASE("Knob renderer: a value change writes the indicator and the value segments whose covered part changed")
{
    Scene scene;
    Knob *knob=scene.knob();
    const int z=knob->getZIndex();

    // into segment 16 (of 32), every one before it whole
    knob->setNumber(0.51);
    scene.frame();
    for(size_t i=0; i < 16; ++i)
    {
        CAPTURE(i);
        const auto *line=lineOf(scene, valueSlot(knob, i));
        REQUIRE(line);
        CHECK(line->zIndex == z+2);
        CHECK((line->color == tokyonight::KnobValue.top));
        CHECK(line->prev.has_value() == (i > 0));
        CHECK(line->next.has_value());
    }
    const auto *partial=lineOf(scene, valueSlot(knob, 16));
    REQUIRE(partial);
    CHECK_FALSE(partial->next.has_value());
    const PointF partialEnd=partial->p1;

    // a settled frame writes nothing
    scene.frame();
    CHECK(scene.written().empty());

    // inside segment 16: it and the indicator alone
    knob->setNumber(0.52);
    scene.frame();
    CHECK(scene.written() == valueWrites(knob, 16, 16));
    CHECK_FALSE((lineOf(scene, valueSlot(knob, 16))->p1 == partialEnd));

    // on to segment 19: 16 now whole (with its next), 17 and 18 shown, 19 partial
    knob->setNumber(0.6);
    scene.frame();
    CHECK(scene.written() == valueWrites(knob, 16, 19));
    CHECK(lineOf(scene, valueSlot(knob, 16))->next.has_value());

    // back to segment 16: 17 to 19 hidden, 16 partial again
    knob->setNumber(0.52);
    scene.frame();
    CHECK(scene.written() == valueWrites(knob, 16, 19));
    for(size_t i=17; i <= 19; ++i)
        CHECK_FALSE(lineOf(scene, valueSlot(knob, i)));

    // the same value again, or a format that keeps the string: nothing
    knob->setNumber(0.52);
    scene.frame();
    CHECK(scene.written().empty());

    // the value text's glyphs only for a new string
    controls::internal::Text *text=knob->getValueTextControl();
    REQUIRE(text);
    knob->setNumber(0.521);
    scene.frame();
    CHECK(scene.renderer.writtenTextIds().empty());
    knob->setNumber(0.53);
    scene.frame();
    CHECK(knob->getValueText() == "0.53");
    CHECK_FALSE(scene.renderer.writtenTextIds().empty());
}

TEST_CASE("Knob renderer: a bipolar arc runs from the range's 0")
{
    Scene scene;
    Knob *knob=scene.knob();

    knob->setRange(-1.0, 1.0);
    knob->setBipolar(true);
    knob->setNumber(0.0);
    scene.frame();
    for(size_t i=0; i < Segments; ++i)
        CHECK_FALSE(lineOf(scene, valueSlot(knob, i)));

    // 0.5 is three quarters of the sweep: segments 16 to 23
    knob->setNumber(0.5);
    scene.frame();
    CHECK(scene.written() == valueWrites(knob, 16, 23));
    CHECK_FALSE(lineOf(scene, valueSlot(knob, 16))->prev.has_value());
    CHECK_FALSE(lineOf(scene, valueSlot(knob, 23))->next.has_value());

    // below 0: the segments under it, those above hidden
    knob->setNumber(-0.5);
    scene.frame();
    CHECK(scene.written() == valueWrites(knob, 8, 23));
    CHECK(lineOf(scene, valueSlot(knob, 8)));
    CHECK(lineOf(scene, valueSlot(knob, 15)));
    CHECK_FALSE(lineOf(scene, valueSlot(knob, 16)));
}

TEST_CASE("Knob renderer: hover, press and focus rewrite the body alone, disabled draws the disabled surfaces")
{
    Scene scene;
    Knob *knob=scene.knob();
    knob->setNumber(0.5);
    scene.frame();

    const Point at=knob->localPosToCanvas();
    const Rect dial=knob->getDialRect();
    const Point centre(at.x+dial.x+dial.w/2, at.y+dial.y+dial.h/2);

    // hover
    scene.canvas->inputMouseMoved(centre.x, centre.y, 0, 0);
    scene.frame();
    CHECK(scene.written() == std::set<size_t>{bodySlot(knob)});
    CHECK((scene.renderer.bodies[bodySlot(knob)].fill == tokyonight::KnobBodyHover.top));

    // press (a press without motion changes no value)
    input::onMouseClicked(scene.canvas, 0, true);
    scene.frame();
    CHECK(scene.written() == std::set<size_t>{bodySlot(knob)});
    CHECK((scene.renderer.bodies[bodySlot(knob)].border == tokyonight::KnobBodyPressed.border));
    input::onMouseClicked(scene.canvas, 0, false);
    scene.frame();
    CHECK(scene.written() == std::set<size_t>{bodySlot(knob)});
    // the focus the press gave it shows over the hover, so moving off keeps it
    CHECK((scene.renderer.bodies[bodySlot(knob)].border == tokyonight::KnobBodyFocus.border));
    scene.canvas->inputMouseMoved(10, 10, 0, 0);
    scene.frame();
    CHECK(scene.written().empty());
    knob->unfocus();
    scene.frame();
    CHECK(scene.written() == std::set<size_t>{bodySlot(knob)});
    CHECK((scene.renderer.bodies[bodySlot(knob)].fill == tokyonight::KnobBody.top));
    scene.frame();
    CHECK(scene.written().empty());

    // disabled: the body, the value arc and the indicator in the disabled surfaces
    knob->setDisabled(true);
    scene.frame();
    CHECK((scene.renderer.bodies[bodySlot(knob)].fill == tokyonight::KnobBodyDisabled.top));
    REQUIRE(lineOf(scene, valueSlot(knob, 0)));
    CHECK((lineOf(scene, valueSlot(knob, 0))->color == tokyonight::KnobValueDisabled.top));
    CHECK((lineOf(scene, indicatorSlot(knob))->color == tokyonight::KnobValueDisabled.top));
    CHECK((knob->getValueTextControl()->getTextColor() == scene.skin.Colors.Button.Disabled));
    knob->setDisabled(false);
    scene.frame();
    CHECK((knob->getValueTextControl()->getTextColor() == scene.skin.Colors.Knob.ValueText));
}

TEST_CASE("Knob renderer: a hidden knob hides every slot and is drawn again once shown")
{
    Scene scene;
    Knob *knob=scene.knob();
    knob->setNumber(0.25);
    scene.frame();

    knob->hide();
    scene.frame();
    const std::vector<size_t> &slots=slotsOf(knob);
    CHECK(scene.written() == std::set<size_t>(slots.begin(), slots.end()));
    CHECK(scene.renderer.lines.empty());

    // a value set while hidden draws once shown
    knob->setNumber(0.75);
    scene.frame();
    knob->show();
    scene.frame();
    CHECK(lineOf(scene, valueSlot(knob, 23)));
    CHECK_FALSE(lineOf(scene, valueSlot(knob, 24)));
    CHECK(lineOf(scene, trackSlot(knob, 0)));
    scene.frame();
    CHECK(scene.written().empty());
}
