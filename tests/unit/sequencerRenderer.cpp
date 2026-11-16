/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The textured skin's Sequencer renderer (skin::textured::Sequencer) on a
 *  recording StubRenderer, a StubControl for every other type, the mouse and
 *  keys through the canvas. It pins: the frame at the sequencer's z, a clip
 *  at its getClipRect() at z+ClipLayer plus its overlap level in
 *  SequencerClip (a selected one with SequencerClipSelected's border), a key
 *  a disc at its getKeyRect() at z+KeyLayer in SequencerKey, depthLayers()
 *  every layer up to the playhead's, a ruler label run per tick at its
 *  getTickLabelRects() rect; a settled frame writing nothing; a hover frame
 *  writing the item it reached and the one it left, a move within an item
 *  nothing; a drag frame writing no slot but the dragged clip's and the snap
 *  guide's (which shows once the clip's edge snaps to its neighbour's); a
 *  scrub frame no slot but the playhead's two and no glyph; a Ctrl+wheel zoom
 *  and a Shift+wheel pan laying nothing out, writing the clips and keys in
 *  view, the labels whose text or place changed and the playhead, and no
 *  stripe; a Delete giving the removed item's slot back to its list's spares
 *  (hidden) with the live ids unchanged, and deleting the sequencer giving
 *  every id back.
 */
#include "headlessFixture.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/sequencer.h>
#include <gweni/inputHandler.h>
#include <gweni/primitiveIds.h>
#include <gweni/skins/textured/texturedControls.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <vector>

using namespace gweni;
using controls::SequencerItem;
namespace tokyonight=skin::textured::tokyonight;
typedef skin::textured::Sequencer SkinSequencer;

namespace
{

size_t livePrimitives() { return primitiveIdCount()-releasedPrimitiveIdCount(); }
size_t liveTexts() { return textIdCount()-releasedTextIdCount(); }

class Recorder: public test::StubRenderer
{
public:
    void drawRoundedRect(size_t primitiveId, Rect rect, float radius, float border, Color top, Color bottom,
        Color borderColor, int zIndex) override
    {
        rects[primitiveId]=rect;
        zIndices[primitiveId]=zIndex;
        fills[primitiveId]=top;
        borders[primitiveId]=borderColor;
        radii[primitiveId]=radius;
        StubRenderer::drawRoundedRect(primitiveId, rect, radius, border, top, bottom, borderColor, zIndex);
    }

    void drawLine(size_t primitiveId, PointF p0, PointF p1, float thickness, int zIndex,
        const PointF *prev=nullptr, const PointF *next=nullptr) override
    {
        zIndices[primitiveId]=zIndex;
        StubRenderer::drawLine(primitiveId, p0, p1, thickness, zIndex, prev, next);
    }

    void renderText(size_t textId, const Font &font, Point pos, int zIndex, const String &text) override
    {
        origins[textId]=pos;
        StubRenderer::renderText(textId, font, pos, zIndex, text);
    }

    std::map<size_t, Point> origins;
    std::map<size_t, Rect> rects;
    std::map<size_t, int> zIndices;
    std::map<size_t, Color> fills;
    std::map<size_t, Color> borders;
    std::map<size_t, float> radii;
};

class SequencerSkin: public test::StubSkin
{
public:
    explicit SequencerSkin(test::StubRenderer *renderer): StubSkin(renderer) {}

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == "Sequencer")
            return std::make_unique<SkinSequencer>();
        return StubSkin::getControlRenderer(typeName);
    }
};

struct Scene
{
    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(1000, 600);
        seq=canvas->newChild<controls::Sequencer>();
        seq->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        seq->setBounds(10, 20, 800, 260);
        seq->setFrameRate(10);
        seq->setTimeRange(0, 10);
        for(int i=0; i < 4; ++i)
            seq->addTrack("Track "+std::to_string(i), i);
        seq->addClip(0, 1, 2, "", 1);
        seq->addClip(0, 3, 4, "", 2);
        seq->addClip(1, 2, 5, "", 3);
        seq->addKey(2, 6, 4);
        seq->addKey(3, 1.5, 5);
        frame();
        frame();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        for(int key : {keys::Shift, keys::Control, keys::Alt})
            canvas->inputModifierKey(key, false);
        input::onMouseClicked(canvas, 0, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    const SkinSequencer &drawn() const { return *static_cast<const SkinSequencer *>(seq->getSkinControl()); }

    std::set<size_t> written() const
    {
        const std::vector<size_t> &ids=renderer.writtenPrimitiveIds();

        return std::set<size_t>(ids.begin(), ids.end());
    }

    size_t glyphsWritten() const { return renderer.frameStats().textSlotsWritten; }

    std::set<size_t> slots(std::vector<size_t> a, const std::vector<size_t> &b={}) const
    {
        a.insert(a.end(), b.begin(), b.end());
        return std::set<size_t>(a.begin(), a.end());
    }

    Point at(size_t track, double t) const
    {
        Rect lane;

        REQUIRE(seq->getLaneRect(track, lane));
        return Point(int(std::lround(seq->xPosition(t))), lane.y+lane.h/2);
    }

    void move(const Point &local)
    {
        const Point p=seq->localPosToCanvas(local);

        canvas->inputMouseMoved(p.x, p.y, 0, 0);
        frame();
    }

    Rect drawnRect(Rect rect) const
    {
        rect.x+=seq->getRenderBounds().x;
        rect.y+=seq->getRenderBounds().y;
        return rect;
    }

    Recorder renderer;
    SequencerSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::Sequencer *seq=nullptr;
};

}//namespace

TEST_CASE("Sequencer renderer: clips, keys, layers and the ruler's labels")
{
    Scene scene;
    controls::Sequencer *seq=scene.seq;
    const SkinSequencer &drawn=scene.drawn();
    const int z=seq->getZIndex();

    CHECK(scene.written().empty());
    CHECK(int(const_cast<SkinSequencer &>(drawn).depthLayers()) == SkinSequencer::PlayheadLayer+1);

    REQUIRE(drawn.clipPrimitives(0).size() == 2);
    REQUIRE(drawn.clipPrimitives(1).size() == 1);
    REQUIRE(drawn.keyPrimitives(2).size() == 1);

    Rect rect;
    const size_t clip=drawn.clipPrimitives(1)[0];

    REQUIRE(seq->getClipRect(1, 0, rect));
    CHECK((scene.renderer.rects[clip] == scene.drawnRect(rect)));
    CHECK(scene.renderer.zIndices[clip] == z+SkinSequencer::ClipLayer);
    CHECK((scene.renderer.fills[clip] == tokyonight::SequencerClip.top));

    const size_t key=drawn.keyPrimitives(2)[0];

    REQUIRE(seq->getKeyRect(2, 0, rect));
    CHECK((scene.renderer.rects[key] == scene.drawnRect(rect)));
    CHECK(scene.renderer.zIndices[key] == z+SkinSequencer::KeyLayer);
    CHECK((scene.renderer.fills[key] == tokyonight::SequencerKey.top));
    CHECK(scene.renderer.radii[key] == doctest::Approx(controls::Sequencer::KeySize/2.0));

    // an overlapping later clip a layer in front
    seq->addClip(0, 1.5, 2.5, "", 6);
    scene.frame();
    REQUIRE(seq->getOverlapLevel(0, 2) == 1);
    CHECK(scene.renderer.zIndices[drawn.clipPrimitives(0)[2]] == z+SkinSequencer::ClipLayer+1);

    // a selected clip in its selection's border
    seq->select(SequencerItem(SequencerItem::Kind::Clip, 1, 0));
    scene.frame();
    CHECK(scene.written() == std::set<size_t>{clip});
    CHECK((scene.renderer.borders[clip] == tokyonight::SequencerClipSelected.border));

    // a label run per tick, at its rect
    REQUIRE(!seq->getTicks().empty());
    CHECK(drawn.labelTexts().size() == seq->getTicks().size());
    const Point origin=scene.renderer.origins[drawn.labelTexts().front()];
    const Rect label=scene.drawnRect(seq->getTickLabelRects().front());

    CHECK(origin.x == label.x);
    CHECK(origin.y == label.y);
}

TEST_CASE("Sequencer renderer: a hover writes the item it reached and the one it left")
{
    Scene scene;
    const SkinSequencer &drawn=scene.drawn();
    const size_t first=drawn.clipPrimitives(0)[0];
    const size_t second=drawn.clipPrimitives(0)[1];

    scene.move(scene.at(0, 1.5));
    CHECK(scene.written() == std::set<size_t>{first});
    scene.move(scene.at(0, 1.6));
    CHECK(scene.written().empty());
    scene.move(scene.at(0, 3.5));
    CHECK(scene.written() == std::set<size_t>{first, second});
    scene.move(scene.at(0, 8));
    CHECK(scene.written() == std::set<size_t>{second});
    scene.frame();
    CHECK(scene.written().empty());
}

TEST_CASE("Sequencer renderer: a drag frame writes the dragged clip and the snap guide alone")
{
    Scene scene;
    controls::Sequencer *seq=scene.seq;
    const SkinSequencer &drawn=scene.drawn();
    const size_t dragged=drawn.clipPrimitives(0)[0];
    const std::set<size_t> allowed=scene.slots({dragged}, drawn.guidePrimitives());
    const Point from=scene.at(0, 1.5);
    const double pps=seq->getBarArea().w/10.0;

    // nothing on the other tracks near the dragged clip's edges
    seq->setClip(1, 0, 6, 7);
    scene.frame();
    scene.move(from);
    scene.canvas->inputMouseButton(0, true);
    scene.frame();

    // towards the neighbour [3, 4]: the end snaps to its start once in reach
    bool snapped=false;

    for(int step=1; step <= 8; ++step)
    {
        const int dx=int(std::lround(step*(pps-3)/8.0));

        scene.move(Point(from.x+dx, from.y));

        const std::set<size_t> written=scene.written();

        CHECK(!written.empty());
        for(size_t id : written)
            CHECK(allowed.count(id) == 1);
        CHECK(scene.glyphsWritten() == 0);
        CHECK(scene.canvas->getLayoutStats().passes == 0);
    }

    double guide=0.0;

    snapped=seq->getSnapGuide(guide);
    CHECK(snapped);
    CHECK(guide == 3.0);
    CHECK(seq->getClip(0, 0).end == 3.0);
    CHECK(scene.renderer.zIndices[drawn.guidePrimitives().front()] == seq->getZIndex()+SkinSequencer::GuideLayer);
    scene.canvas->inputMouseButton(0, false);
    scene.frame();
    CHECK(scene.written() == std::set<size_t>{drawn.guidePrimitives().front()});
    scene.frame();
    CHECK(scene.written().empty());
}

TEST_CASE("Sequencer renderer: a scrub writes the playhead, a zoom and a pan lay nothing out")
{
    Scene scene;
    controls::Sequencer *seq=scene.seq;
    const SkinSequencer &drawn=scene.drawn();
    const std::set<size_t> playhead=scene.slots(drawn.playheadPrimitives());
    const Rect row=seq->getAxisRow();

    REQUIRE(playhead.size() == 2);

    // a scrub in the ruler: the playhead's two slots, no glyph
    scene.move(Point(int(std::lround(seq->xPosition(3.3))), row.y+2));
    scene.canvas->inputMouseButton(0, true);
    scene.frame();
    CHECK(scene.written() == playhead);
    CHECK(scene.glyphsWritten() == 0);
    scene.move(Point(int(std::lround(seq->xPosition(5.1))), row.y+2));
    CHECK(scene.written() == playhead);
    scene.canvas->inputMouseButton(0, false);
    scene.frame();
    CHECK(scene.written().empty());

    // Ctrl+wheel: no layout, the items in view and the playhead, no stripe
    const std::set<size_t> lanes=scene.slots(drawn.lanePrimitives());

    scene.move(scene.at(3, 4));
    scene.canvas->inputModifierKey(keys::Control, true);
    scene.canvas->inputMouseWheel(60);
    scene.canvas->inputModifierKey(keys::Control, false);
    scene.frame();
    CHECK(scene.canvas->getLayoutStats().passes == 0);

    std::set<size_t> written=scene.written();

    CHECK(written.count(drawn.clipPrimitives(1)[0]) == 1);
    CHECK(written.count(*playhead.begin()) == 1);
    for(size_t id : lanes)
        CHECK(written.count(id) == 0);
    CHECK(scene.glyphsWritten() > 0);
    scene.frame();
    CHECK(scene.written().empty());

    // Shift+wheel pans: the same
    scene.canvas->inputModifierKey(keys::Shift, true);
    scene.canvas->inputMouseWheel(-60);
    scene.canvas->inputModifierKey(keys::Shift, false);
    scene.frame();
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    written=scene.written();
    CHECK(written.count(drawn.clipPrimitives(1)[0]) == 1);
    for(size_t id : lanes)
        CHECK(written.count(id) == 0);
    scene.frame();
    CHECK(scene.written().empty());
}

TEST_CASE("Sequencer renderer: a Delete hides the item's slot, and deleting the sequencer gives every id back")
{
    const size_t primitives=livePrimitives();
    const size_t texts=liveTexts();

    {
        Scene scene;
        controls::Sequencer *seq=scene.seq;
        const SkinSequencer &drawn=scene.drawn();
        const size_t live=livePrimitives();
        const size_t last=drawn.clipPrimitives(0)[1];

        seq->select(SequencerItem(SequencerItem::Kind::Clip, 0, 1));
        scene.frame();
        seq->focus();
        scene.canvas->inputModifierKey(keys::Delete, true);
        scene.canvas->inputModifierKey(keys::Delete, false);
        scene.frame();
        CHECK(seq->getClipCount(0) == 1);
        CHECK(drawn.clipPrimitives(0).size() == 1);
        CHECK(scene.written() == std::set<size_t>{last});
        CHECK(livePrimitives() == live);
    }
    CHECK(livePrimitives() == primitives);
    CHECK(liveTexts() == texts);
}
