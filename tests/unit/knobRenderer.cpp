/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The textured Knob renderer on a real Canvas with the real (un-init()'d)
 *  textured skin on a recording StubRenderer, its writes read per frame from
 *  writtenPrimitiveIds() and its lines from the stub's `lines`.
 *
 *  Its slots are the body, the track arc, the value arc, the indicator and
 *  one per tick, in that order; the body is a disc (radius half its side,
 *  its edges rounded from the centre each, so an odd side is not half a unit
 *  off) at the knob's z, the track and the ticks at z+1, the value arc over
 *  the track at z+2 and the indicator on the body at z+1. The arcs are one
 *  drawArc each in the geometry's arcRect: the track the whole sweep, the
 *  value arc from the arc's origin to the value, hidden at the origin. A
 *  value change writes the indicator and the value arc alone, whichever part
 *  of the sweep it moves through. A bipolar knob's arc runs from the range's
 *  0. Hover, press and focus rewrite the body's slot alone, a settled frame
 *  writes nothing, the value text's glyphs are written only for a new
 *  string, a disabled knob draws its body and value in the disabled
 *  surfaces, a hidden knob hides every slot and is drawn again once shown, a
 *  new tick count resizes the list's tail, and the ticks' round caps stay
 *  inside the dial.
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
        renderer.arcs.clear();
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
size_t trackSlot(Knob *knob) { return slotsOf(knob)[1]; }
size_t valueSlot(Knob *knob) { return slotsOf(knob)[2]; }
size_t indicatorSlot(Knob *knob) { return slotsOf(knob)[3]; }
size_t tickSlot(Knob *knob, size_t i) { return slotsOf(knob)[4+i]; }

//! The value arc and the indicator.
std::set<size_t> valueWrites(Knob *knob)
{
    return std::set<size_t>{valueSlot(knob), indicatorSlot(knob)};
}

//! The last arc drawn in \p id this frame, or null.
const test::StubRenderer::ArcDraw *arcOf(const Scene &scene, size_t id)
{
    for(auto it=scene.renderer.arcs.rbegin(); it != scene.renderer.arcs.rend(); ++it)
    {
        if(it->id == id)
            return &*it;
    }
    return nullptr;
}

//! The share of \p knob's sweep at \p turns (drawArc's, from the top clockwise).
double shareAt(Knob *knob, double turns)
{
    const double degrees=90.0-turns*360.0;

    return (degrees-knob->getStartAngle())/(knob->getEndAngle()-knob->getStartAngle());
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

    REQUIRE(slotsOf(knob).size() == 4);
    const int z=knob->getZIndex();
    const Rect bounds=knob->getRenderBounds();
    const Knob::Geometry geometry=knob->getGeometry();
    const PointF centre(bounds.x+geometry.centre.x, bounds.y+geometry.centre.y);

    // the body: a disc at the knob's z, centred on the dial
    REQUIRE(scene.renderer.bodies.count(bodySlot(knob)));
    const BodyRecorder::Body &body=scene.renderer.bodies[bodySlot(knob)];
    CHECK(body.rect.w == body.rect.h);
    CHECK(body.radius == doctest::Approx(body.rect.w*0.5f));
    CHECK(body.z == z);
    CHECK((body.fill == tokyonight::KnobBody.top));
    CHECK(body.rect.x+body.rect.w*0.5f == doctest::Approx(centre.x));
    CHECK(body.rect.y+body.rect.h*0.5f == doctest::Approx(centre.y));
    // an 80-unit dial's body is 56.67 units across, a 72-unit one's 51 with
    // its edges on .5 either side of the centre: each side's distance from
    // the centre the same, not an odd side placed from a rounded corner nor
    // both edges rounded up
    for(int diameter: {80, 72, 30, 120})
    {
        CAPTURE(diameter);
        knob->setDiameter(diameter);
        scene.frame();
        const Rect wide=knob->getRenderBounds();
        const Knob::Geometry big=knob->getGeometry();
        const PointF middle(wide.x+big.centre.x, wide.y+big.centre.y);
        const BodyRecorder::Body &disc=scene.renderer.bodies[bodySlot(knob)];
        CAPTURE(disc.rect.x);
        CAPTURE(disc.rect.w);
        CHECK(disc.rect.x+disc.rect.w*0.5f == doctest::Approx(middle.x));
        CHECK(disc.rect.y+disc.rect.h*0.5f == doctest::Approx(middle.y));
    }
    knob->setDiameter(Knob::DefaultDiameter);
    scene.frame();

    // the track: one arc over the whole sweep, its outer edge on the dial's
    const Rect arcRect(bounds.x+geometry.arcRect.x, bounds.y+geometry.arcRect.y, geometry.arcRect.w,
        geometry.arcRect.h);
    const auto *track=arcOf(scene, trackSlot(knob));
    REQUIRE(track);
    CHECK((track->rect == arcRect));
    CHECK((geometry.arcRect == knob->getDialRect()));
    CHECK(track->zIndex == z+1);
    CHECK((track->color == tokyonight::KnobTrack.top));
    CHECK(track->thickness == geometry.arcThickness);
    // 225 degrees is 0.625 of a turn from the top, the sweep three quarters
    CHECK(track->startTurns == doctest::Approx(-0.375));
    CHECK(track->sweepTurns == doctest::Approx(0.75));

    // at the minimum the value arc draws nothing, the indicator points at the start
    CHECK_FALSE(arcOf(scene, valueSlot(knob)));
    const auto *indicator=lineOf(scene, indicatorSlot(knob));
    REQUIRE(indicator);
    CHECK(indicator->zIndex == z+1);
    CHECK((indicator->color == tokyonight::KnobIndicator.top));
    const PointF tip=Knob::pointAt(centre, knob->getStartAngle(), geometry.indicatorOuter);
    CHECK(indicator->p1.x == doctest::Approx(tip.x));
    CHECK(indicator->p1.y == doctest::Approx(tip.y));
    CHECK(const_cast<skin::Control *>(knob->getSkinControl())->depthLayers() == 3);

    // ticks: one slot each at the list's tail, outside the arc, the arc's
    // rect whole units in, each tick's round cap inside the dial
    knob->setTickCount(5);
    scene.frame();
    REQUIRE(slotsOf(knob).size() == 4+5);
    const Knob::Geometry ticked=knob->getGeometry();
    const Rect dial=knob->getDialRect();
    CHECK(ticked.tickInner > ticked.arcRadius+ticked.arcThickness*0.5f);
    CHECK(ticked.arcRect.w < dial.w);
    CHECK(ticked.arcRect.w == ticked.arcRect.h);
    CHECK(ticked.arcRect.x-dial.x == dial.x+dial.w-(ticked.arcRect.x+ticked.arcRect.w));
    REQUIRE(arcOf(scene, trackSlot(knob)));
    CHECK(arcOf(scene, trackSlot(knob))->rect.w == ticked.arcRect.w);
    CHECK(ticked.tickOuter+ticked.tickThickness*0.5f <= dial.w*0.5f);
    for(size_t i=0; i < 5; ++i)
    {
        CAPTURE(i);
        const auto *tick=lineOf(scene, tickSlot(knob, i));
        REQUIRE(tick);
        CHECK((tick->color == tokyonight::KnobTick.top));
        CHECK(tick->zIndex == z+1);
    }
    knob->setTickCount(0);
    scene.frame();
    CHECK(slotsOf(knob).size() == 4);
}

TEST_CASE("Knob renderer: a caption or value text made or deleted on a fixed-size knob draws the dial again where it moved")
{
    Scene scene;
    Knob *knob=scene.knob();

    knob->setShowValue(false);
    knob->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    knob->setSize(48, 70);
    scene.frame();
    scene.frame();
    REQUIRE(scene.written().empty());

    // the bounds stay, the dial moves down under the caption
    const Rect before=knob->getDialRect();
    const auto check=[&](const char *what)
    {
        CAPTURE(what);
        const Rect dial=knob->getDialRect();
        const Rect bounds=knob->getRenderBounds();
        REQUIRE_FALSE((dial == before));
        CHECK(scene.written().count(bodySlot(knob)));
        CHECK(scene.written().count(trackSlot(knob)));
        CHECK(scene.written().count(indicatorSlot(knob)));
        const BodyRecorder::Body &body=scene.renderer.bodies[bodySlot(knob)];
        CHECK(body.rect.y+body.rect.h*0.5f == doctest::Approx(bounds.y+dial.y+dial.h*0.5f));
        const auto *track=arcOf(scene, trackSlot(knob));
        REQUIRE(track);
        CHECK(track->rect.y == bounds.y+knob->getGeometry().arcRect.y);
    };

    knob->setCaption("Gain");
    scene.frame();
    check("caption made");
    scene.frame();
    CHECK(scene.written().empty());

    const Rect withCaption=knob->getDialRect();
    knob->setCaption("");
    scene.frame();
    CHECK((knob->getDialRect() == before));
    CHECK(scene.written().count(bodySlot(knob)));
    CHECK(scene.renderer.bodies[bodySlot(knob)].rect.y < knob->getRenderBounds().y+withCaption.y);

    // the value text, its row under the dial, leaves the dial a smaller side
    knob->setCaption("Gain");
    scene.frame();
    const Rect captioned=knob->getDialRect();
    knob->setShowValue(true);
    scene.frame();
    REQUIRE_FALSE((knob->getDialRect() == captioned));
    CHECK(scene.written().count(bodySlot(knob)));
    CHECK(scene.written().count(trackSlot(knob)));
    knob->setShowValue(false);
    scene.frame();
    CHECK((knob->getDialRect() == captioned));
    CHECK(scene.written().count(bodySlot(knob)));
    scene.frame();
    CHECK(scene.written().empty());
}

TEST_CASE("Knob renderer: a value change writes the indicator and the value arc alone")
{
    Scene scene;
    Knob *knob=scene.knob();
    const int z=knob->getZIndex();

    knob->setNumber(0.51);
    scene.frame();
    const auto *arc=arcOf(scene, valueSlot(knob));
    REQUIRE(arc);
    CHECK(arc->zIndex == z+2);
    CHECK((arc->color == tokyonight::KnobValue.top));
    CHECK(arc->rect.w == knob->getGeometry().arcRect.w);
    CHECK(shareAt(knob, arc->startTurns) == doctest::Approx(0.0));
    CHECK(shareAt(knob, arc->startTurns+arc->sweepTurns) == doctest::Approx(0.51));
    CHECK(arc->thickness == knob->getGeometry().arcThickness);

    // a settled frame writes nothing
    scene.frame();
    CHECK(scene.written().empty());

    // a small step, a large one, back, and a value on a quarter of the sweep:
    // the arc and the indicator alone, the arc ending at the value
    for(double value : {0.52, 0.6, 0.52, 0.5, 0.75, 1.0, 0.25})
    {
        CAPTURE(value);
        knob->setNumber(value);
        scene.frame();
        CHECK(scene.written() == valueWrites(knob));
        REQUIRE(arcOf(scene, valueSlot(knob)));
        arc=arcOf(scene, valueSlot(knob));
        CHECK(shareAt(knob, arc->startTurns) == doctest::Approx(0.0));
        CHECK(shareAt(knob, arc->startTurns+arc->sweepTurns) == doctest::Approx(value));
    }

    // back at the origin the arc is hidden, not a dot
    knob->setNumber(0.0);
    scene.frame();
    CHECK(scene.written() == valueWrites(knob));
    CHECK_FALSE(arcOf(scene, valueSlot(knob)));

    // the same value again, or a format that keeps the string: nothing
    knob->setNumber(0.52);
    scene.frame();
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
    CHECK_FALSE(arcOf(scene, valueSlot(knob)));

    // 0.5 is three quarters of the sweep: the arc from the middle to there
    knob->setNumber(0.5);
    scene.frame();
    CHECK(scene.written() == valueWrites(knob));
    const auto *arc=arcOf(scene, valueSlot(knob));
    REQUIRE(arc);
    CHECK(arc->sweepTurns > 0.0f);
    CHECK(shareAt(knob, arc->startTurns) == doctest::Approx(0.5));
    CHECK(shareAt(knob, arc->startTurns+arc->sweepTurns) == doctest::Approx(0.75));

    // below 0: from the value up to the middle, still clockwise
    knob->setNumber(-0.5);
    scene.frame();
    CHECK(scene.written() == valueWrites(knob));
    arc=arcOf(scene, valueSlot(knob));
    REQUIRE(arc);
    CHECK(arc->sweepTurns > 0.0f);
    CHECK(shareAt(knob, arc->startTurns) == doctest::Approx(0.25));
    CHECK(shareAt(knob, arc->startTurns+arc->sweepTurns) == doctest::Approx(0.5));
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
    REQUIRE(arcOf(scene, valueSlot(knob)));
    CHECK((arcOf(scene, valueSlot(knob))->color == tokyonight::KnobValueDisabled.top));
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
    CHECK(scene.renderer.arcs.empty());

    // a value set while hidden draws once shown
    knob->setNumber(0.75);
    scene.frame();
    knob->show();
    scene.frame();
    REQUIRE(arcOf(scene, valueSlot(knob)));
    CHECK(shareAt(knob, arcOf(scene, valueSlot(knob))->startTurns+arcOf(scene, valueSlot(knob))->sweepTurns)
        == doctest::Approx(0.75));
    CHECK(arcOf(scene, trackSlot(knob)));
    scene.frame();
    CHECK(scene.written().empty());
}
