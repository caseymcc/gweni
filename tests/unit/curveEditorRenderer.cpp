/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The textured skin's CurvePlot renderer (a CurveEditor's plot), over a
 *  recording StubRenderer (a StubControl for every other type), which counts
 *  every draw as a write. It checks:
 *  - the slot lists taken back to front (the background, the grid, the
 *    marquee's fill, each curve's segments, each curve's keys, the handles'
 *    stems, their discs, the marquee's outline), their ids rising, and
 *    taken again so when a key added past the spare slots needs more; each
 *    layer drawn at its z;
 *  - a Linear span one segment from key to key, a Constant one the step's two,
 *    a Hermite one segmentsFor() its chord on the curve's evaluate(), in the
 *    curve's surface, each segment joined to its neighbours across the spans
 *    and the extensions, which run flat from the plot's left edge to the first
 *    key and from the last key to its right edge;
 *  - a key a disc of KeySize centred on it, CurveKey, CurveKeyHover or
 *    CurveKeySelected; a selected Free key's handles, two stems and two discs,
 *    given back when it is deselected;
 *  - the grid's minor and major lines at axis::niceStep's ticks and a label
 *    per major tick in the skin's tick colour, none in the time labels' row;
 *  - a segment past the plot cut to it, in the plot's clip box;
 *  - a key drag frame writing no slot of the plot but those of the spans its
 *    edit changed (and the spans beside them) and its disc, as many at 10 keys
 *    as at 200; a hover writing the two discs; a click's selection the discs
 *    whose look changed; a settled frame nothing; a zoom no layout pass and no
 *    slot outside the plot;
 *  - an added and a removed key, and the editor deleted, giving every id back;
 *  - a key added and deleted before a frame never hiding or recycling its
 *    unallocated id-0 sentinel: later additions keep only the plot's owned ids,
 *    leave other controls' slots unchanged and keep the live id/take counts;
 *  - a live theme edit recolouring every visible plot layer without layout;
 *  - a reorder reassigning tangent handle sets when the pool is full.
 *  Every single press advances the process-wide curveEditorFixture.h clock
 *  shared with the editor tests, so separate scenes cannot repeat a click time.
 */
#include "headlessFixture.h"
#include "curveEditorFixture.h"

#include <gweni/gweni.h>
#include <gweni/controls/chartAxis.h>
#include <gweni/controls/curveEditor.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/tools/toolSupport.h>
#include <gweni/tools/themeEditor.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <vector>

using namespace gweni;
using Key=Curve::Key;
using Mode=Curve::TangentMode;
namespace tokyonight=skin::textured::tokyonight;

namespace
{

Key makeKey(double time, double value, Mode mode=Mode::Smooth)
{
    Key k;

    k.time=time;
    k.value=value;
    k.mode=mode;
    return k;
}

//! Records each rounded rect and line drawn, by slot, with its clip box.
class DrawRecorder: public test::StubRenderer
{
public:
    struct RoundedDraw
    {
        Rect rect;
        float radius=0.0f;
        Color top;
        int zIndex=0;
        Rect clip;
    };

    void drawRoundedRect(size_t primitiveId, Rect rect, float radius, float border, Color top, Color bottom,
        Color borderColor, int zIndex) override
    {
        rounded[primitiveId]=RoundedDraw{rect, radius, top, zIndex, clipRegion()};
        hiddenSlots.erase(primitiveId);
        StubRenderer::drawRoundedRect(primitiveId, rect, radius, border, top, bottom, borderColor, zIndex);
    }

    void drawLine(size_t primitiveId, PointF p0, PointF p1, float thickness, int zIndex, const PointF *prev,
        const PointF *next) override
    {
        StubRenderer::drawLine(primitiveId, p0, p1, thickness, zIndex, prev, next);
        lastLine[primitiveId]=lines.back();
        lineClip[primitiveId]=clipRegion();
        hiddenSlots.erase(primitiveId);
    }

    void hidePrimitive(size_t primitiveId) override
    {
        StubRenderer::hidePrimitive(primitiveId);
        hiddenSlots.insert(primitiveId);
    }

    void renderText(size_t textId, const Font &font, Point pos, int zIndex, const String &text) override
    {
        textColors[textId]=drawColor;
        StubRenderer::renderText(textId, font, pos, zIndex, text);
    }

    std::map<size_t, RoundedDraw> rounded;
    std::map<size_t, LineDraw> lastLine;
    std::map<size_t, Rect> lineClip;
    std::set<size_t> hiddenSlots;
    std::map<size_t, Color> textColors;
};

class CurveSkin: public test::StubSkin
{
public:
    explicit CurveSkin(test::StubRenderer *renderer): StubSkin(renderer) {}

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == "CurvePlot")
            return std::make_unique<skin::textured::CurvePlot>();
        return StubSkin::getControlRenderer(typeName);
    }
};

//! An editor whose plot is 400x200 over the view 0..4 x 0..2 (100 plot units a
//! curve unit), at (20, 30) on the canvas.
struct Scene
{
    explicit Scene(std::vector<Key> keys={makeKey(0, 0.5, Mode::Linear), makeKey(1, 1.5, Mode::Constant),
        makeKey(2, 0.5), makeKey(3, 1.5)})
    {
        platform::setTimeSource([]() { return test::curveEditorClock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(800, 600);
        editor=canvas->newChild<controls::CurveEditor>();
        editor->setBounds(20, 30, 400, 224);
        curve=Curve(std::move(keys));
        editor->addCurve(&curve);
        editor->setView(0, 4, 0, 2);
        frame();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        HoveredControl=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    void frame()
    {
        renderer.lines.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    skin::textured::CurvePlot *plotRenderer() const
    {
        return const_cast<skin::textured::CurvePlot *>(
            static_cast<const skin::textured::CurvePlot *>(editor->getPlot()->getSkinControl()));
    }

    //! Where the plot draws its own origin: its render bounds' top-left, in
    //! the units the renderer is given (the render offset is added after).
    PointF origin() const
    {
        const Rect &bounds=editor->getPlot()->getRenderBounds();

        return PointF(float(bounds.x), float(bounds.y));
    }

    //! The plot's top-left on the canvas.
    PointF canvasOrigin() const
    {
        const Point at=editor->getPlot()->localPosToCanvas(Point(0, 0));

        return PointF(float(at.x), float(at.y));
    }

    //! A curve point where the plot draws it.
    PointF drawn(double x, double y) const
    {
        const PointF local=editor->curveToLocal(CurvePoint{x, y});

        return PointF(origin().x+local.x, origin().y+local.y);
    }

    //! The canvas point of a point drawn at \p point.
    Point at(const PointF &point) const
    {
        return Point(int(std::lround(canvasOrigin().x+point.x-origin().x)),
            int(std::lround(canvasOrigin().y+point.y-origin().y)));
    }

    void move(const Point &to)
    {
        canvas->inputMouseMoved(to.x, to.y, 0, 0);
        frame();
    }

    //! The clock moves on first: the last click is process-wide, and a press
    //! where a test before pressed would otherwise be a double-click.
    void press(const Point &to)
    {
        test::curveEditorClock+=1.0;
        move(to);
        canvas->inputMouseButton(0, true);
        frame();
    }

    void release()
    {
        canvas->inputMouseButton(0, false);
        frame();
    }

    void click(const Point &to)
    {
        press(to);
        release();
    }

    std::set<size_t> written() const
    {
        const std::vector<size_t> &ids=renderer.writtenPrimitiveIds();

        return std::set<size_t>(ids.begin(), ids.end());
    }

    std::set<size_t> plotIds() const
    {
        std::vector<size_t> primitives, texts;

        tools::appendSubtreeIds(editor->getPlot(), primitives, texts);
        return std::set<size_t>(primitives.begin(), primitives.end());
    }

    //! The ids written this frame that are the plot's.
    std::set<size_t> plotWritten() const
    {
        const std::set<size_t> ids=plotIds();
        std::set<size_t> out;

        for(size_t id : written())
        {
            if(ids.count(id))
                out.insert(id);
        }
        return out;
    }

    std::set<size_t> spanIds(size_t from, size_t to) const
    {
        std::set<size_t> ids;

        for(size_t k=from; k<to; ++k)
        {
            for(size_t id : plotRenderer()->spanPrimitives(0, k))
                ids.insert(id);
        }
        return ids;
    }

    DrawRecorder renderer;
    CurveSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::CurveEditor *editor=nullptr;
    Curve curve;
};

bool near(const PointF &a, const PointF &b, float tolerance=0.01f)
{
    return (std::fabs(a.x-b.x) <= tolerance) && (std::fabs(a.y-b.y) <= tolerance);
}

}//namespace

TEST_CASE("CurvePlot: live theme edits recolour every plot layer without layout")
{
    Scene scene;
    scene.editor->setSelection({{0, 2}});
    scene.editor->setSelectionMode(Mode::Free);
    scene.frame();
    auto *plot=scene.plotRenderer();
    struct Restore
    {
        Restore()
        {
            for(auto entry : tokyonight::surfaceRegistry())
                surfaces.push_back({entry.second, *entry.second});
        }
        ~Restore() { for(auto &entry : surfaces) *entry.first=entry.second; }
        std::vector<std::pair<tokyonight::Surface *, tokyonight::Surface>> surfaces;
    } restore;
    const Color changed(230, 60, 100);
    for(auto &entry : restore.surfaces)
        entry.first->top=changed;
    scene.skin.Colors.CurveEditor.TickText=changed;
    tools::ThemeEditor::applyToCanvas(scene.canvas, false);
    scene.frame();

    CHECK((scene.renderer.rounded[plot->takenOrder().front()].top == changed));
    for(size_t k=0; k<scene.curve.spanCount(); ++k)
        for(size_t id : plot->spanPrimitives(0, k))
            if(!scene.renderer.hiddenSlots.count(id))
                CHECK((scene.renderer.lastLine.at(id).color == changed));
    for(size_t id : plot->gridPrimitives())
        if(!scene.renderer.hiddenSlots.count(id))
            CHECK((scene.renderer.lastLine.at(id).color == changed));
    for(size_t k=0; k<scene.curve.keyCount(); ++k)
        CHECK((scene.renderer.rounded[plot->keyPrimitive(0, k)].top == changed));
    const auto handles=plot->handlePrimitives(0, 2);
    REQUIRE(handles.size() == 4);
    CHECK((scene.renderer.lastLine.at(handles[0]).color == changed));
    CHECK((scene.renderer.rounded.at(handles[2]).top == changed));
    for(size_t id : plot->labelTexts())
        CHECK((scene.renderer.textColors.at(id) == changed));
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    scene.frame();
    CHECK(scene.plotWritten().empty());
}

TEST_CASE("CurvePlot: a reorder reassigns a full pool of tangent handles")
{
    Scene scene({makeKey(1.0, 1, Mode::Free), makeKey(1.5, 1, Mode::Free),
        makeKey(2.0, 1, Mode::Free), makeKey(2.5, 1, Mode::Free),
        makeKey(3.0, 1, Mode::Free), makeKey(3.5, 1, Mode::Free)});
    // Four sets, initially none used. Select four without retaking any slots.
    scene.editor->setSelection({{0, 1}, {0, 2}, {0, 3}, {0, 4}});
    scene.frame();
    REQUIRE(scene.editor->shownHandleCount() == 4);
    scene.editor->nudgeSelection(-0.6, 0.0);
    scene.frame();
    const auto selected=scene.editor->getSelection();
    CHECK(selected.front().key == 0);
    std::set<size_t> ids;
    for(const auto &ref : selected)
    {
        const auto handles=scene.plotRenderer()->handlePrimitives(ref.curve, ref.key);
        CHECK(handles.size() == 4);
        for(size_t id : handles)
        {
            CHECK(!scene.renderer.hiddenSlots.count(id));
            CHECK(ids.insert(id).second);
        }
    }
}

TEST_CASE("CurvePlot: the lists are taken back to front, each layer at its z")
{
    Scene scene;
    skin::textured::CurvePlot *plot=scene.plotRenderer();
    const std::vector<size_t> order=plot->takenOrder();
    const int z=scene.editor->getPlot()->getZIndex();

    REQUIRE(order.size() > 1);
    CHECK(order.front() == scene.editor->getPlot()->getSkinControl()->getPrimitives()[0]);
    CHECK(std::is_sorted(order.begin(), order.end()));
    CHECK(std::adjacent_find(order.begin(), order.end()) == order.end());

    // back to front: the grid before the fill, the segments, the keys, the outline
    const std::vector<size_t> marquee=plot->marqueePrimitives();
    const size_t grid=plot->gridPrimitives().back();
    const size_t segment=plot->spanPrimitives(0, 2).back();
    const size_t key=plot->keyPrimitive(0, 3);

    CHECK(grid < marquee[0]);
    CHECK(marquee[0] < plot->extensionPrimitives(0)[0]);
    CHECK(segment < key);
    CHECK(key < marquee[1]);

    CHECK(scene.renderer.rounded[order.front()].zIndex == z);
    CHECK(scene.renderer.lastLine[plot->gridPrimitives().front()].zIndex == z+1);
    CHECK(scene.renderer.lastLine[plot->spanPrimitives(0, 0).front()].zIndex == z+3);
    CHECK(scene.renderer.rounded[key].zIndex == z+4);
    CHECK(plot->depthLayersOf(scene.editor->getPlot()) == skin::textured::CurvePlot::FixedLayers+1);

    // a second curve adds a layer
    Curve other({makeKey(0, 1), makeKey(4, 1)});

    scene.editor->addCurve(&other);
    scene.frame();
    CHECK(plot->depthLayersOf(scene.editor->getPlot()) == skin::textured::CurvePlot::FixedLayers+2);
    CHECK(scene.renderer.lastLine[plot->spanPrimitives(1, 0).front()].zIndex == z+4);
    CHECK(scene.renderer.rounded[plot->keyPrimitive(0, 0)].zIndex == z+5);
    const std::vector<size_t> again=plot->takenOrder();
    CHECK(std::is_sorted(again.begin(), again.end()));
}

TEST_CASE("CurvePlot: each span is drawn in its shape's segments, joined to its neighbours")
{
    Scene scene;
    skin::textured::CurvePlot *plot=scene.plotRenderer();
    DrawRecorder &r=scene.renderer;
    const Color color=tokyonight::CurveCurve0.top;

    // Linear: one segment from key to key, the rest of its slots none
    const std::vector<size_t> linear=plot->spanPrimitives(0, 0);

    REQUIRE(linear.size() == 1);
    CHECK(near(r.lastLine[linear[0]].p0, scene.drawn(0, 0.5)));
    CHECK(near(r.lastLine[linear[0]].p1, scene.drawn(1, 1.5)));
    CHECK((r.lastLine[linear[0]].color == color));
    CHECK(r.lastLine[linear[0]].thickness == skin::textured::CurvePlot::LineThickness);

    // Constant: across at the key's value, then down to the next
    const std::vector<size_t> constant=plot->spanPrimitives(0, 1);

    REQUIRE(constant.size() == 2);
    CHECK(near(r.lastLine[constant[0]].p0, scene.drawn(1, 1.5)));
    CHECK(near(r.lastLine[constant[0]].p1, scene.drawn(2, 1.5)));
    CHECK(near(r.lastLine[constant[1]].p1, scene.drawn(2, 0.5)));

    // Hermite: segmentsFor() its chord, on the curve, the slots past them hidden
    const std::vector<size_t> hermite=plot->spanPrimitives(0, 2);
    const int segments=skin::textured::CurvePlot::segmentsFor(Curve::SpanShape::Hermite, std::hypot(100.0f, 100.0f));

    REQUIRE(hermite.size() == size_t(skin::textured::CurvePlot::MaxHermiteSegments));
    CHECK(segments == 15);
    for(int i=0; i<segments; ++i)
    {
        const double t=2.0+double(i+1)/segments;

        CHECK(near(r.lastLine[hermite[i]].p1, scene.drawn(t, scene.curve.evaluate(t)), 0.02f));
    }
    for(size_t i=size_t(segments); i<hermite.size(); ++i)
        CHECK(r.hiddenSlots.count(hermite[i]));

    // joined: each segment's neighbours are the segments before and after it,
    // across the spans
    CHECK(r.lastLine[linear[0]].next.has_value());
    CHECK(near(*r.lastLine[linear[0]].next, r.lastLine[constant[0]].p1));
    CHECK(near(*r.lastLine[constant[0]].prev, r.lastLine[linear[0]].p0));
    CHECK(near(*r.lastLine[hermite[0]].prev, r.lastLine[constant[1]].p0));
    CHECK(near(*r.lastLine[hermite[3]].next, r.lastLine[hermite[4]].p1));

    // the extensions: flat to the plot's edges (and a margin past them)
    const std::vector<size_t> ext=plot->extensionPrimitives(0);
    const float margin=skin::textured::CurvePlot::LineThickness+4.0f;

    CHECK(near(r.lastLine[ext[0]].p0, PointF(scene.origin().x-margin, scene.drawn(0, 0.5).y)));
    CHECK(near(r.lastLine[ext[0]].p1, scene.drawn(0, 0.5)));
    CHECK(near(r.lastLine[ext[1]].p0, scene.drawn(3, 1.5)));
    CHECK(near(r.lastLine[ext[1]].p1, PointF(scene.origin().x+400.0f+margin, scene.drawn(3, 1.5).y)));
    CHECK(near(*r.lastLine[ext[0]].next, r.lastLine[linear[0]].p1));
    CHECK(near(*r.lastLine[linear[0]].prev, r.lastLine[ext[0]].p0));
}

TEST_CASE("CurvePlot: a key is a disc in its look, a selected Free key's handles come and go")
{
    Scene scene({makeKey(0, 0.5), makeKey(2, 1.0, Mode::Free), makeKey(4, 0.5)});
    skin::textured::CurvePlot *plot=scene.plotRenderer();
    DrawRecorder &r=scene.renderer;
    const int size=controls::CurveEditor::KeySize;
    const size_t disc=plot->keyPrimitive(0, 1);
    const PointF at=scene.drawn(2, 1.0);

    CHECK((r.rounded[disc].rect == Rect(int(at.x)-size/2, int(at.y)-size/2, size, size)));
    CHECK(r.rounded[disc].radius == 0.5f*size);
    CHECK((r.rounded[disc].top == tokyonight::CurveKey.top));
    CHECK(plot->handlePrimitives(0, 1).empty());

    scene.move(scene.at(at));
    CHECK((r.rounded[disc].top == tokyonight::CurveKeyHover.top));

    scene.click(scene.at(at));
    CHECK((r.rounded[disc].top == tokyonight::CurveKeySelected.top));

    const std::vector<size_t> handles=plot->handlePrimitives(0, 1);

    REQUIRE(handles.size() == 4);
    PointF in, out;
    scene.editor->handlePositions(0, 1, in, out);
    const PointF origin=scene.origin();
    CHECK(near(r.lastLine[handles[0]].p1, PointF(origin.x+in.x, origin.y+in.y)));
    CHECK(near(r.lastLine[handles[1]].p1, PointF(origin.x+out.x, origin.y+out.y)));
    CHECK((r.lastLine[handles[1]].color == tokyonight::CurveTangent.top));
    CHECK(r.rounded[handles[3]].rect.w == controls::CurveEditor::HandleSize);
    CHECK(handles[1] < handles[2]);

    // deselected: the handles hidden and given back to the free sets
    scene.click(scene.at(PointF(origin.x+300, origin.y+180)));
    CHECK(plot->handlePrimitives(0, 1).empty());
    for(size_t id : handles)
        CHECK(r.hiddenSlots.count(id));
}

TEST_CASE("CurvePlot: the grid at the 1/2/5 ticks, a label per major tick")
{
    Scene scene;
    DrawRecorder &r=scene.renderer;
    skin::textured::CurvePlot *plot=scene.plotRenderer();
    std::vector<test::StubRenderer::LineDraw> minor, major;

    for(size_t id : plot->gridPrimitives())
    {
        if(r.hiddenSlots.count(id) || !r.lastLine.count(id))
            continue;
        if(r.lastLine[id].color == tokyonight::CurveEditorGridMajor.top)
            major.push_back(r.lastLine[id]);
        else
        {
            CHECK((r.lastLine[id].color == tokyonight::CurveEditorGrid.top));
            minor.push_back(r.lastLine[id]);
        }
    }
    // time every 0.2 minor (20 intervals), 1 major; value every 0.2, 0.5 major
    CHECK(controls::axis::niceStep(4.0, 400/12) == doctest::Approx(0.2));
    CHECK(controls::axis::niceStep(4.0, 400/64) == doctest::Approx(1.0));
    CHECK(controls::axis::niceStep(2.0, 200/32) == doctest::Approx(0.5));
    CHECK(major.size() == 5+5);
    CHECK(minor.size() == 16+8);
    for(const auto &line : major)
        CHECK(((line.p0.x == line.p1.x) || (line.p0.y == line.p1.y)));

    // the labels: every time label, the value labels above the time row
    const Color tick=scene.skin.Colors.CurveEditor.TickText;
    (void)tick;
    for(const char *text : {"0", "1", "2", "3", "4", "1.5"})
        CHECK(r.textClips.count(text));
    CHECK(plot->labelTexts().size() >= 9);
}

TEST_CASE("CurvePlot: a segment past the plot is cut to it, in the plot's clip box")
{
    Scene scene;
    DrawRecorder &r=scene.renderer;
    skin::textured::CurvePlot *plot=scene.plotRenderer();

    // the view's top at 1: the Constant span's value 1.5 is above it
    scene.editor->setView(0, 4, 0, 1);
    scene.frame();

    const PointF origin=scene.origin();
    const PointF canvas=scene.canvasOrigin();
    const float margin=skin::textured::CurvePlot::LineThickness+4.0f;
    const Rect clip=r.lineClip[plot->spanPrimitives(0, 0)[0]];

    CHECK((clip == Rect(int(canvas.x), int(canvas.y), 400, 200)));
    for(size_t k=0; k<3; ++k)
    {
        for(size_t id : plot->spanPrimitives(0, k))
        {
            if(r.hiddenSlots.count(id))
                continue;
            for(const PointF &p : {r.lastLine[id].p0, r.lastLine[id].p1})
            {
                CHECK(p.y >= origin.y-margin-0.01f);
                CHECK(p.y <= origin.y+200.0f+margin+0.01f);
            }
        }
    }
    // the Constant span's flat part is wholly above the plot: hidden
    CHECK(r.hiddenSlots.count(plot->spanPrimitives(0, 1)[0]));
}

TEST_CASE("CurvePlot: a key drag writes its spans' and its disc's slots alone, at 10 keys and at 200")
{
    auto dragFrame=[](size_t count)
    {
        std::vector<Key> keys;

        for(size_t i=0; i<count; ++i)
            keys.push_back(makeKey(0.2*double(i), 0.5+0.4*double(i%2)));

        Scene scene(keys);

        scene.editor->setView(0, 2, 0, 2);
        scene.frame();

        const PointF key=scene.drawn(0.2*5, 0.9);

        scene.press(scene.at(key));
        scene.move(scene.at(PointF(key.x+3, key.y-4)));

        const CurveSpanChange change=scene.curve.lastChange();
        std::set<size_t> allowed=scene.spanIds(change.first > 0?change.first-1:0,
            std::min(scene.curve.spanCount(), change.first+change.inserted+1));

        allowed.insert(scene.plotRenderer()->keyPrimitive(0, 5));

        const std::set<size_t> written=scene.plotWritten();

        CHECK(!written.empty());
        for(size_t id : written)
            CHECK(allowed.count(id));
        scene.release();
        return written.size();
    };

    const size_t small=dragFrame(10);
    const size_t large=dragFrame(200);

    CHECK(small == large);
}

TEST_CASE("CurvePlot: a hover writes two discs, a click the discs whose look changed, a settled frame nothing")
{
    Scene scene;
    skin::textured::CurvePlot *plot=scene.plotRenderer();

    scene.frame();
    CHECK(scene.written().empty());

    scene.move(scene.at(scene.drawn(1, 1.5)));
    CHECK((scene.plotWritten() == std::set<size_t>{plot->keyPrimitive(0, 1)}));
    scene.move(scene.at(scene.drawn(2, 0.5)));
    CHECK((scene.plotWritten() == std::set<size_t>{plot->keyPrimitive(0, 1), plot->keyPrimitive(0, 2)}));
    scene.frame();
    CHECK(scene.written().empty());

    // a press selects the hovered key: its disc alone
    scene.press(scene.at(scene.drawn(2, 0.5)));
    CHECK((scene.plotWritten() == std::set<size_t>{plot->keyPrimitive(0, 2)}));
    scene.release();
    scene.frame();
    CHECK(scene.plotWritten().empty());
}

TEST_CASE("CurvePlot: a zoom lays nothing out and writes the plot's slots alone")
{
    Scene scene;

    scene.move(scene.at(scene.drawn(2, 1.0)));
    scene.canvas->inputMouseWheel(60);
    scene.frame();
    CHECK(scene.canvas->getLayoutStats().passes == 0);

    const std::set<size_t> plot=scene.plotIds();

    CHECK(!scene.written().empty());
    for(size_t id : scene.written())
        CHECK(plot.count(id));
    scene.frame();
    CHECK(scene.written().empty());
}

TEST_CASE("CurvePlot: keys added and removed, and the editor deleted, give every id back")
{
    const test::IdBankCounts before=test::IdBankCounts::now();

    {
        Scene scene;
        skin::textured::CurvePlot *plot=scene.plotRenderer();
        const size_t live=scene.renderer.livePrimitives.size();
        const size_t takes=plot->takeCount();

        // a key added and removed by the user: taken from the spare slots and
        // given back to them
        scene.editor->addKeyAt(0, 2.5);
        scene.frame();
        CHECK(scene.renderer.livePrimitives.size() == live);
        scene.editor->deleteSelection();
        scene.frame();
        CHECK(scene.renderer.livePrimitives.size() == live);
        CHECK(plot->takeCount() == takes);

        // past the spare slots: every list taken again, back to front
        for(int i=0; i<40; ++i)
        {
            scene.editor->addKeyAt(0, 3.0+0.02*i);
            scene.frame();
        }
        CHECK(plot->takeCount() > takes);
        const std::vector<size_t> order=plot->takenOrder();
        CHECK(std::is_sorted(order.begin(), order.end()));
        CHECK(plot->keyPrimitive(0, 43) > plot->spanPrimitives(0, 42).back());
    }

    const test::IdBankCounts after=test::IdBankCounts::now();

    CHECK(after.livePrimitives() == before.livePrimitives());
    CHECK(after.liveTexts() == before.liveTexts());
}

TEST_CASE("CurvePlot: a key added and deleted before a frame never recycles an unallocated slot")
{
    Scene scene;
    const auto owned=scene.plotIds();
    const size_t live=scene.renderer.livePrimitives.size();
    const size_t takes=scene.plotRenderer()->takeCount();
    scene.renderer.hiddenSlots.clear();
    scene.editor->addKeyAt(0, 2.5);
    REQUIRE(scene.editor->deleteSelection());
    scene.frame();
    for(size_t id : scene.renderer.hiddenSlots)
        CHECK(owned.count(id) == 1);
    const size_t key=scene.editor->addKeyAt(0, 2.5);
    scene.frame();
    CHECK(owned.count(scene.plotRenderer()->keyPrimitive(0, key)) == 1);
    CHECK(scene.renderer.livePrimitives.size() == live);
    CHECK(scene.plotRenderer()->takeCount() == takes);
    // A later look change keeps this key's slot instead of draining spares.
    const size_t id=scene.plotRenderer()->keyPrimitive(0, key);
    scene.editor->clearSelection();
    scene.frame();
    CHECK(scene.plotRenderer()->keyPrimitive(0, key) == id);
}
