/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The textured skin's GradientBar and GradientStopHandle renderers on a
 *  recording StubRenderer, which keeps what each slot was last drawn with; a
 *  slot counts as written in a frame only when that changed, as SlotRenderer
 *  writes only the slots whose bytes differ.
 *
 *  The bar is a ColorChecker checkerboard under the whole bar in
 *  GradientBarFrame's border, then a border width in, at z+1, a flat span of
 *  the first stop's colour up to its column, one of the last stop's from its
 *  column (each hidden at no width) and a bilinear span per pair of stops, its
 *  left edge the first's colour and its right the second's (flat in Constant
 *  mode, hidden for two stops in one column); a stop at t is in the column its
 *  handle is centred on. The slot list follows the stop count: an added stop
 *  takes one slot and a deleted one gives it back. A handle is a triangle
 *  pointing up over a swatch of its stop's colour, in GradientStop,
 *  GradientStopHover, GradientStopSelected or GradientStopFocus. A stop moved
 *  writes its handle's slots and the spans on its sides (the flat end too for
 *  the first or last stop), a recoloured one its swatch and those spans, a
 *  reorder no span whose ends stayed, a hover the handle alone, a settled
 *  frame nothing.
 */
#include "headlessFixture.h"

#include <gweni/controls/colorEdit.h>
#include <gweni/controls/gradientEditor.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <doctest/doctest.h>

#include <map>
#include <set>
#include <sstream>
#include <string>

using namespace gweni;
using controls::GradientEditor;
using controls::internal::GradientStopHandle;
namespace tokyonight=skin::textured::tokyonight;

namespace
{

std::string text(const Color &c)
{
    std::ostringstream s;

    s<<int(c.r)<<","<<int(c.g)<<","<<int(c.b)<<","<<int(c.a);
    return s.str();
}

std::string text(const Rect &r)
{
    std::ostringstream s;

    s<<r.x<<","<<r.y<<" "<<r.w<<"x"<<r.h;
    return s.str();
}

//! What each slot was last drawn with, and the slots a frame changed.
class SlotRecorder: public test::StubRenderer
{
public:
    struct Slot
    {
        std::string kind;
        Rect rect;
        Color a, b, border;
        int z=0;
        std::string key;
    };

    void drawBilinearRect(size_t id, Rect rect, int z, Color c00, Color c10, Color c01, Color c11,
        float radius, float border, Color borderColor) override
    {
        record(id, {"bilinear", rect, c00, c10, borderColor, z,
            text(c01)+text(c11)+std::to_string(radius)+std::to_string(border)});
    }

    void drawCheckerRect(size_t id, Rect rect, int z, Color fill, Color light, Color dark,
        float radius, float border, Color borderColor) override
    {
        record(id, {"checker", rect, fill, light, borderColor, z,
            text(dark)+std::to_string(radius)+std::to_string(border)});
    }

    void drawIcon(size_t id, Rect rect, int kind, int direction, Color color, int z) override
    {
        record(id, {"icon", rect, color, color, color, z, std::to_string(kind)+"/"+std::to_string(direction)});
    }

    void hidePrimitive(size_t id) override
    {
        record(id, {"hidden", Rect(), Color(), Color(), Color(), 0, ""});
    }

    void releasePrimitive(size_t id) override
    {
        slots.erase(id);
        test::StubRenderer::releasePrimitive(id);
    }

    void record(size_t id, const Slot &slot)
    {
        auto it=slots.find(id);

        if(it == slots.end() || describe(it->second) != describe(slot))
            changed.insert(id);
        slots[id]=slot;
    }

    static std::string describe(const Slot &slot)
    {
        return slot.kind+" "+text(slot.rect)+" "+text(slot.a)+" "+text(slot.b)+" "+text(slot.border)+" "
            +std::to_string(slot.z)+" "+slot.key;
    }

    const Slot &at(size_t id) const { return slots.at(id); }

    std::map<size_t, Slot> slots;
    std::set<size_t> changed;
};

struct Scene
{
    SlotRecorder renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    float clock=nextClock();

    static float nextClock()
    {
        static float start=0.0f;

        start+=1000.0f;
        return start;
    }

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);
        // the mouse's place is the input handler's, kept across cases: off
        // the editor, so a re-hit-tested hover finds nothing there
        canvas->inputMouseMoved(630, 470, 0, 0);
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

    //! A frame; the slots it changed.
    std::set<size_t> frame()
    {
        renderer.changed.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
        return renderer.changed;
    }

    GradientEditor *editor(Gradient *gradient)
    {
        GradientEditor *editor=canvas->newChild<GradientEditor>();

        editor->setPos(20, 20);
        editor->setGradient(gradient);
        frame();
        frame();
        return editor;
    }

    void move(Point at) { canvas->inputMouseMoved(at.x, at.y, 0, 0); }
};

const std::vector<size_t> &barSlots(GradientEditor *editor)
{
    return editor->getBar()->getSkinControl()->getPrimitives();
}

const std::vector<size_t> &handleSlots(GradientEditor *editor, size_t stop)
{
    return editor->getHandle(stop)->getSkinControl()->getPrimitives();
}

std::set<size_t> handleSet(GradientEditor *editor, size_t stop)
{
    const std::vector<size_t> &slots=handleSlots(editor, stop);

    return std::set<size_t>(slots.begin(), slots.end());
}

//! The bar's slots \p indexes and the handle slots of stop \p handle.
std::set<size_t> expect(GradientEditor *editor, std::initializer_list<size_t> indexes, size_t handle)
{
    std::set<size_t> slots=handleSet(editor, handle);

    for(size_t i : indexes)
        slots.insert(barSlots(editor)[i]);
    return slots;
}

//! The bar's slots \p indexes and the swatch slot of stop \p handle.
std::set<size_t> swatchAnd(GradientEditor *editor, std::initializer_list<size_t> indexes, size_t handle)
{
    std::set<size_t> slots={handleSlots(editor, handle)[1]};

    for(size_t i : indexes)
        slots.insert(barSlots(editor)[i]);
    return slots;
}

size_t liveIds() { return primitiveIdCount()-releasedPrimitiveIdCount(); }

Color grey(unsigned char v, unsigned char a=255) { return Color(v, v, v, a); }

//! The bar's left plus round(t*width), as the renderer places a stop.
int column(GradientEditor *editor, float t)
{
    const Rect bounds=editor->getBar()->getRenderBounds();

    return bounds.x+(int)std::lround(t*(float)bounds.w);
}

}//namespace

TEST_CASE("GradientEditor renderer: a checkerboard under the bar and a span per pair of stops")
{
    Scene scene;
    Gradient gradient({{0.0f, grey(10)}, {0.4f, grey(100, 128)}, {1.0f, grey(200)}});
    GradientEditor *editor=scene.editor(&gradient);
    const std::vector<size_t> &bar=barSlots(editor);

    REQUIRE(bar.size() == 5);

    const Rect bounds=editor->getBar()->getRenderBounds();
    const int z=editor->getBar()->getZIndex();
    const SlotRecorder::Slot &checker=scene.renderer.at(bar[0]);

    CHECK(checker.kind == "checker");
    CHECK(text(checker.rect) == text(bounds));
    CHECK(checker.a.a == 0);
    CHECK(text(checker.b) == text(tokyonight::ColorChecker.top));
    CHECK(text(checker.border) == text(tokyonight::GradientBarFrame.border));
    CHECK(checker.z == z);

    // the stops at 0 and 1: no flat ends
    CHECK(scene.renderer.at(bar[1]).kind == "hidden");
    CHECK(scene.renderer.at(bar[2]).kind == "hidden");

    const SlotRecorder::Slot &left=scene.renderer.at(bar[3]);
    const SlotRecorder::Slot &right=scene.renderer.at(bar[4]);

    CHECK(left.kind == "bilinear");
    CHECK(left.z == z+1);
    CHECK(left.rect.x == bounds.x+1);
    CHECK(left.rect.y == bounds.y+1);
    CHECK(left.rect.h == bounds.h-2);
    CHECK(left.rect.x+left.rect.w == column(editor, 0.4f));
    CHECK(text(left.a) == text(grey(10)));
    CHECK(text(left.b) == text(grey(100, 128)));
    CHECK(right.rect.x == column(editor, 0.4f));
    CHECK(right.rect.x+right.rect.w == bounds.x+bounds.w-1);
    CHECK(text(right.a) == text(grey(100, 128)));
    CHECK(text(right.b) == text(grey(200)));

    // each stop's column is its handle's centre (a stop at 0 or 1 drawn up
    // to the frame's inside)
    for(size_t i=0; i < 3; ++i)
    {
        GradientStopHandle *handle=editor->getHandle(i);

        CHECK(handle->getRenderBounds().x+handle->getWidth()/2 == column(editor, gradient.stop(i).position));
    }

    // Constant: flat spans of each span's first stop
    gradient.setInterpolation(Gradient::Interpolation::Constant);
    editor->gradientChanged();
    scene.frame();
    CHECK(text(scene.renderer.at(bar[3]).b) == text(grey(10)));
    CHECK(text(scene.renderer.at(bar[4]).b) == text(grey(100, 128)));
}

TEST_CASE("GradientEditor renderer: the flat ends show for stops inside the bar")
{
    Scene scene;
    Gradient gradient({{0.25f, grey(10)}, {0.75f, grey(200)}});
    GradientEditor *editor=scene.editor(&gradient);
    const std::vector<size_t> &bar=barSlots(editor);
    const Rect bounds=editor->getBar()->getRenderBounds();

    REQUIRE(bar.size() == 4);
    const SlotRecorder::Slot &before=scene.renderer.at(bar[1]);
    const SlotRecorder::Slot &after=scene.renderer.at(bar[2]);

    CHECK(before.kind == "bilinear");
    CHECK(before.rect.x == bounds.x+1);
    CHECK(before.rect.x+before.rect.w == column(editor, 0.25f));
    CHECK(text(before.a) == text(grey(10)));
    CHECK(text(before.b) == text(grey(10)));
    CHECK(after.rect.x == column(editor, 0.75f));
    CHECK(after.rect.x+after.rect.w == bounds.x+bounds.w-1);
    CHECK(text(after.b) == text(grey(200)));

    // two stops in one column: their span is hidden
    gradient.moveStop(1, 0.25f);
    editor->gradientChanged();
    scene.frame();
    CHECK(scene.renderer.at(bar[3]).kind == "hidden");
}

TEST_CASE("GradientEditor renderer: a moved or recoloured stop writes its handle and the spans beside it")
{
    Scene scene;
    Gradient gradient({{0.0f, grey(10)}, {0.3f, grey(60)}, {0.6f, grey(120)}, {1.0f, grey(200)}});
    GradientEditor *editor=scene.editor(&gradient);

    CHECK(scene.frame().empty());

    // the middle stop 1: spans 0 and 1 (slots 3 and 4)
    editor->setSelectedStop(1);
    scene.frame();
    editor->moveSelectedTo(0.4f);
    CHECK(scene.frame() == expect(editor, {3, 4}, 1));
    CHECK(scene.frame().empty());

    // a new colour: the swatch, not the marker
    editor->setSelectedColor(grey(70));
    CHECK(scene.frame() == swatchAnd(editor, {3, 4}, 1));

    // the first stop, moved off 0: its flat end and span 0
    editor->setSelectedStop(0);
    scene.frame();
    editor->moveSelectedTo(0.1f);
    CHECK(scene.frame() == expect(editor, {1, 3}, 0));

    // the last, recoloured: span 2 and its flat end, hidden at 1 (written
    // nothing: it stays hidden)
    editor->setSelectedStop(3);
    scene.frame();
    editor->setSelectedColor(grey(210));
    CHECK(scene.frame() == swatchAnd(editor, {5}, 3));
}

TEST_CASE("GradientEditor renderer: a reorder writes the spans whose ends changed")
{
    Scene scene;
    Gradient gradient({{0.0f, grey(10)}, {0.3f, grey(60)}, {0.6f, grey(120)}, {1.0f, grey(200)}});
    GradientEditor *editor=scene.editor(&gradient);

    editor->setSelectedStop(1);
    scene.frame();
    // stop 1 passes stop 2: spans 0..2 change ends, stop 1's and 2's handles
    // swap places in the strip (each moves, so each redraws)
    editor->moveSelectedTo(0.7f);

    const std::set<size_t> written=scene.frame();
    std::set<size_t> allowed=expect(editor, {3, 4, 5}, 1);

    for(size_t id : handleSet(editor, 2))
        allowed.insert(id);
    for(size_t id : written)
        CHECK(allowed.count(id) == 1);
    CHECK(written.count(barSlots(editor)[0]) == 0);
    CHECK(written.count(barSlots(editor)[1]) == 0);
    CHECK(written.count(barSlots(editor)[2]) == 0);
}

TEST_CASE("GradientEditor renderer: the slot list follows the stop count")
{
    Scene scene;
    Gradient gradient({{0.0f, grey(10)}, {1.0f, grey(200)}});
    GradientEditor *editor=scene.editor(&gradient);
    const size_t ids=liveIds();

    REQUIRE(barSlots(editor).size() == 4);
    editor->addStopAt(0.5f);
    scene.frame();
    CHECK(barSlots(editor).size() == 5);
    // a span slot and the handle's two
    CHECK(liveIds() == ids+3);

    editor->setSelectedStop(1);
    CHECK(editor->deleteSelected());
    scene.frame();
    scene.frame();
    CHECK(barSlots(editor).size() == 4);
    CHECK(liveIds() == ids);
}

TEST_CASE("GradientEditor renderer: a handle's looks, and a hover writes the handle alone")
{
    Scene scene;
    Gradient gradient({{0.0f, grey(10)}, {0.5f, grey(60, 100)}, {1.0f, grey(200)}});
    GradientEditor *editor=scene.editor(&gradient);
    GradientStopHandle *handle=editor->getHandle(1);
    const std::vector<size_t> &slots=handleSlots(editor, 1);

    REQUIRE(slots.size() == 2);
    const SlotRecorder::Slot &marker=scene.renderer.at(slots[0]);
    const SlotRecorder::Slot &swatch=scene.renderer.at(slots[1]);
    const Rect bounds=handle->getRenderBounds();

    CHECK(marker.kind == "icon");
    CHECK(marker.key == "7/1");
    CHECK(text(marker.rect) == text(Rect(bounds.x, bounds.y, bounds.w, GradientEditor::MarkerHeight)));
    CHECK(text(marker.a) == text(tokyonight::GradientStop.top));
    CHECK(swatch.kind == "checker");
    CHECK(text(swatch.rect) == text(Rect(bounds.x, bounds.y+GradientEditor::MarkerHeight, bounds.w,
        bounds.h-GradientEditor::MarkerHeight)));
    CHECK(text(swatch.a) == text(grey(60, 100)));
    CHECK(text(swatch.border) == text(tokyonight::GradientStop.border));

    const Point centre(handle->localPosToCanvas().x+handle->getWidth()/2,
        handle->localPosToCanvas().y+handle->getHeight()/2);

    scene.move(centre);
    CHECK(scene.frame() == handleSet(editor, 1));
    CHECK(text(scene.renderer.at(slots[0]).a) == text(tokyonight::GradientStopHover.top));
    scene.move(Point(centre.x+1, centre.y));
    CHECK(scene.frame().empty());
    scene.move(Point(600, 400));
    CHECK(scene.frame() == handleSet(editor, 1));

    // selected: its look, and the old selection's back at rest (the row's
    // swatch shows the new stop's colour too), no bar slot
    editor->setSelectedStop(1);
    const std::set<size_t> selected=scene.frame();

    for(size_t stop : {0, 1})
    {
        for(size_t id : handleSet(editor, stop))
            CHECK(selected.count(id) == 1);
    }
    for(size_t id : barSlots(editor))
        CHECK(selected.count(id) == 0);
    CHECK(selected.count(handleSlots(editor, 2)[0]) == 0);
    CHECK(text(scene.renderer.at(slots[1]).border) == text(tokyonight::GradientStopSelected.border));

    handle->focus();
    scene.frame();
    CHECK(text(scene.renderer.at(slots[1]).border) == text(tokyonight::GradientStopFocus.border));
}
