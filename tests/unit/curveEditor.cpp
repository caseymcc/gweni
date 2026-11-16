/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  CurveEditor (gweni/controls/curveEditor.h), the function-curve editor, on a
 *  StubSkin, driven through the canvas's mouse and keys. It checks:
 *  - the view: a curve point at (p-x0)*scale across the plot and (y1-p)*scale
 *    down it, and back; the plot over the footer's fields;
 *  - the key hit test within KeyHitRadius on screen, at zoom 1 and 2, the
 *    nearest key winning, a key of the first curve on a tie (its disc is on top);
 *  - the wheel zooming about the cursor (the key under it stays), Ctrl the
 *    time axis alone, Shift (the canvas's horizontal wheel) the value axis
 *    alone; a right drag past PanThreshold panning by the mouse's motion, with
 *    no menu; A and F framing every key and the selection;
 *  - selection by click (alone, Shift adding, Ctrl toggling, a plain click on
 *    a key of a selection making it the selection, a click on empty space
 *    clearing it), onSelectionChanged once per click that changed it; the
 *    app's own selection firing nothing;
 *  - the marquee: nothing until PanThreshold, then the keys it touches, each
 *    frame logging a look change for the keys whose selection changed alone,
 *    the release firing onSelectionChanged once, Escape giving the selection
 *    back with no event;
 *  - a key drag: each frame moving the selection by the mouse's motion in
 *    curve units and firing onChanged once per key it moved (none for a frame
 *    that keeps it), logging the curve's span and key splices; a key dragged
 *    past a neighbour reordered with the selection following it; Shift
 *    keeping to the axis moved most; Escape putting the keys back;
 *  - a tangent handle drag on a selected Free key: the slope from the key to
 *    the mouse, both tangents while joined, the one side once broken, Escape
 *    putting it back; a key not Free showing no handles;
 *  - Delete removing the selection, onChanged per key and onSelectionChanged
 *    once; Escape with no gesture not taken;
 *  - a double-click near a curve adding a key on it at the mouse's time, made
 *    the selection; far from every curve adding nothing;
 *  - a right-click on a key selecting it and opening the key menu, its mode
 *    items setting the selection's mode, Broken its broken flag, Delete
 *    removing it; a right-click on empty space opening none;
 *  - the arrows nudging the selection a plot unit's worth (Shift ten);
 *  - the footer: blank and disabled unless one key is selected, showing its
 *    time and value, a field's new number moving it;
 *  - a hover logging the look of the key it left and the one it took;
 *  - an editor made under a hidden parent (told it is not visible before its
 *    parts exist) showing once the parent is shown;
 *  - a pan's hover and wheel following its captured mouse, axis modifiers
 *    pressed during the capture, and motion continuing after a zoom;
 *  - a reorder drag firing no selection event, a right press during a key
 *    drag leaving it alone, and disabling clearing the hover;
 *  - onChanged reporting an app edit via curveChanged(), safely stopping a
 *    multi-key drag, nudge, deletion or tangent-mode/broken change, and one
 *    setting or clearing the selection stopping a nudge;
 *  - a wheel zoom during a key drag keeping the key under the cursor, and
 *    during a marquee keeping its press on the same curve point;
 *  - disabling closing the key menu, whose items then change nothing.
 *  Every press moves the scene's clock on first: the last click's point and
 *  time are process-wide, so a press where the case before pressed would
 *  otherwise be a double-click.
 */
#include "headlessFixture.h"
#include "curveEditorFixture.h"

#include <gweni/gweni.h>
#include <gweni/controls/curveEditor.h>
#include <gweni/controls/dragNumber.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/menuItem.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <cmath>
#include <vector>

using namespace gweni;
using Key=Curve::Key;
using Mode=Curve::TangentMode;
using Kind=controls::CurveEditorEdit::Kind;

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

//! An editor whose plot is 400x200 over the view 0..4 x 0..2: 100 plot units
//! a curve unit on both axes. Curve 0's keys are at (0,150), (100,50),
//! (200,150) and (300,50) in the plot; curve 1's at (50,180) and (350,20).
struct Scene
{
    Scene()
    {
        platform::setTimeSource([]() { return test::curveEditorClock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(800, 600);
        editor=canvas->newChild<controls::CurveEditor>();
        editor->setBounds(20, 30, 400, 224);
        a=Curve({makeKey(0, 0.5, Mode::Linear), makeKey(1, 1.5), makeKey(2, 0.5, Mode::Free),
            makeKey(3, 1.5, Mode::Constant)});
        b=Curve({makeKey(0.5, 0.2), makeKey(3.5, 1.8)});
        editor->addCurve(&a);
        editor->addCurve(&b);
        editor->setView(0, 4, 0, 2);
        editor->onChanged.add(&events, &Events::changed);
        editor->onSelectionChanged.add(&events, &Events::selection);
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
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    //! The canvas point a plot point is at.
    Point at(float x, float y) const
    {
        const Point origin=editor->getPlot()->localPosToCanvas(Point(0, 0));

        return Point(origin.x+int(std::lround(x)), origin.y+int(std::lround(y)));
    }

    void move(const Point &to)
    {
        canvas->inputMouseMoved(to.x, to.y, 0, 0);
        frame();
    }

    //! A press of its own: the clock moves on first, since the last click's
    //! point and time are process-wide and a press at the place a test before
    //! pressed would be a double-click.
    void press(const Point &to, int button=0)
    {
        test::curveEditorClock+=1.0;
        pressNow(to, button);
    }

    //! A press with the clock where it is (a double-click's second press).
    void pressNow(const Point &to, int button=0)
    {
        move(to);
        canvas->inputMouseButton(button, true);
        frame();
    }

    void release(int button=0)
    {
        canvas->inputMouseButton(button, false);
        frame();
    }

    //! A single click, never a double-click.
    void click(const Point &to, int button=0)
    {
        press(to, button);
        release(button);
    }

    void doubleClick(const Point &to)
    {
        press(to);
        release();
        pressNow(to);
        release();
    }

    void key(int k, bool down=true)
    {
        canvas->inputModifierKey(k, down);
        frame();
    }

    void tap(int k)
    {
        key(k, true);
        key(k, false);
    }

    struct Events: public event::Handler
    {
        void changed(event::Info info)
        {
            ++changes;
            last=controls::CurveKeyRef{size_t(info.point.x), size_t(info.point.y)};
        }
        void selection(event::Info) { ++selections; }

        int changes=0;
        int selections=0;
        controls::CurveKeyRef last;
    };

    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::CurveEditor *editor=nullptr;
    Curve a;
    Curve b;
    Events events;
};

std::vector<controls::CurveKeyRef> refs(std::initializer_list<std::pair<size_t, size_t>> list)
{
    std::vector<controls::CurveKeyRef> out;

    for(const auto &pair : list)
        out.push_back(controls::CurveKeyRef{pair.first, pair.second});
    return out;
}

}//namespace

namespace
{
void nestedEditStopsOuter(const std::function<void(controls::CurveEditor *)> &nested)
{
    Scene scene;
    auto *editor=scene.editor;
    editor->setSelection(refs({{0, 1}, {0, 2}, {0, 3}}));
    struct App: event::Handler
    {
        void changed(event::Info)
        {
            if(called)
                return;
            called=true;
            const int before=scene->events.changes;
            nested(editor);
            CHECK(scene->events.changes > before);
            after=*editor->getCurve(0);
            selection=editor->getSelection();
            changes=scene->events.changes;
        }
        Scene *scene;
        controls::CurveEditor *editor;
        std::function<void(controls::CurveEditor *)> nested;
        bool called=false;
        Curve after;
        std::vector<controls::CurveKeyRef> selection;
        int changes=0;
    } app;
    app.scene=&scene;
    app.editor=editor;
    app.nested=nested;
    editor->onChanged.add(&app, &App::changed);

    SUBCASE("nudge") { editor->nudgeSelection(0.1, 0.1); }
    SUBCASE("delete") { CHECK(editor->deleteSelection()); }
    SUBCASE("mode") { editor->setSelectionMode(Mode::Constant); }
    SUBCASE("broken") { editor->setSelectionBroken(true); }
    REQUIRE(app.called);
    CHECK(bool(scene.a.keys() == app.after.keys()));
    CHECK(editor->getSelection() == app.selection);
    CHECK(scene.events.changes == app.changes);
}
}

TEST_CASE("CurveEditor: a nested delete stops the outer edit")
{
    nestedEditStopsOuter([](auto *editor) { CHECK(editor->deleteSelection()); });
}

TEST_CASE("CurveEditor: a nested add stops the outer edit")
{
    nestedEditStopsOuter([](auto *editor) { editor->addKeyAt(0, 1.25); });
}

TEST_CASE("CurveEditor: a nested nudge stops the outer edit")
{
    nestedEditStopsOuter([](auto *editor) { editor->nudgeSelection(0.2, 0.2); });
}

TEST_CASE("CurveEditor: a nested mode change stops the outer edit")
{
    nestedEditStopsOuter([](auto *editor) { editor->setSelectionMode(Mode::Free); });
}

TEST_CASE("CurveEditor: a nested broken change stops the outer edit")
{
    nestedEditStopsOuter([](auto *editor)
    {
        const auto ref=editor->getSelection().front();
        editor->setSelectionBroken(!editor->getCurve(ref.curve)->key(ref.key).broken);
    });
}

TEST_CASE("CurveEditor: a nested time field edit stops the outer edit")
{
    nestedEditStopsOuter([](auto *editor)
    {
        editor->setSelection({{0, 0}});
        editor->setSelectedTime(0.25);
    });
}

TEST_CASE("CurveEditor: a nested value field edit stops the outer edit")
{
    nestedEditStopsOuter([](auto *editor)
    {
        editor->setSelection({{0, 0}});
        editor->setSelectedValue(0.25);
    });
}

TEST_CASE("CurveEditor: a selection handler editing keys stops a press")
{
    Scene scene;
    auto *editor=scene.editor;
    struct App: event::Handler
    {
        void selection(event::Info)
        {
            if(called)
                return;
            called=true;
            if(add)
                editor->addKeyAt(0, 0.25);
            else
                editor->deleteSelection();
        }
        controls::CurveEditor *editor;
        bool called=false;
        bool add=false;
    } app;
    app.editor=editor;
    editor->onSelectionChanged.add(&app, &App::selection);
    SUBCASE("delete on left press") { editor->pressAt(scene.at(300, 50)); }
    SUBCASE("delete on right press") { editor->rightPressAt(scene.at(300, 50)); }
    SUBCASE("add on left press") { app.add=true; editor->pressAt(scene.at(300, 50)); }
    SUBCASE("add on right press") { app.add=true; editor->rightPressAt(scene.at(300, 50)); }
    REQUIRE(app.called);
    CHECK(!editor->draggingKeys());
    CHECK(editor->getKeyMenu() == nullptr);
    CHECK(scene.a.keyCount() == (app.add?5:3));
    CHECK(scene.events.changes == 1);
}

TEST_CASE("CurveEditor: a nested field edit ends the outer key drag")
{
    Scene scene;
    struct App: event::Handler
    {
        void changed(event::Info)
        {
            if(called)
                return;
            called=true;
            if(time)
                editor->setSelectedTime(2.5);
            else
                editor->setSelectedValue(0.9);
            after=*editor->getCurve(0);
        }
        controls::CurveEditor *editor;
        bool called=false;
        bool time=false;
        Curve after;
    } app;
    app.editor=scene.editor;
    scene.editor->onChanged.add(&app, &App::changed);
    SUBCASE("time") { app.time=true; }
    SUBCASE("value") { app.time=false; }
    scene.press(scene.at(100, 50));
    scene.move(scene.at(110, 40));
    REQUIRE(app.called);
    CHECK(!scene.editor->draggingKeys());
    CHECK(scene.events.changes == 2);
    scene.move(scene.at(120, 30));
    CHECK(bool(scene.a.keys() == app.after.keys()));
    CHECK(scene.events.changes == 2);
    scene.release();
}

TEST_CASE("CurveEditor: the view maps curve points onto the plot, the plot over the fields")
{
    Scene scene;
    controls::CurveEditor *editor=scene.editor;

    CHECK((editor->getPlot()->getBounds() == Rect(0, 0, 400, 200)));
    CHECK(editor->getTimeField()->getY() == 204);
    CHECK(editor->getValueField()->getY() == 204);
    CHECK(editor->getValueField()->getX() == controls::CurveEditor::FieldWidth+controls::CurveEditor::Gap);

    const PointF p=editor->curveToLocal(CurvePoint{1.0, 1.5});

    CHECK(p.x == doctest::Approx(100.0f));
    CHECK(p.y == doctest::Approx(50.0f));

    const CurvePoint back=editor->localToCurve(PointF(250.0f, 125.0f));

    CHECK(back.x == doctest::Approx(2.5));
    CHECK(back.y == doctest::Approx(0.75));

    // backwards ends are swapped, a span of nothing kept at the least
    editor->setView(4, 0, 2, 2);
    CHECK(editor->viewX0() == 0.0);
    CHECK(editor->viewX1() == 4.0);
    CHECK(editor->viewY1()-editor->viewY0() == doctest::Approx(controls::CurveEditor::MinViewSpan));
}

TEST_CASE("CurveEditor: a key is hit within KeyHitRadius on screen, at zoom 1 and 2")
{
    Scene scene;
    controls::CurveEditor *editor=scene.editor;
    controls::CurveKeyRef ref;

    REQUIRE(editor->keyAt(PointF(103.0f, 52.0f), ref));
    CHECK(ref == controls::CurveKeyRef{0, 1});
    CHECK(editor->keyAt(PointF(106.0f, 50.0f), ref));
    CHECK(!editor->keyAt(PointF(107.0f, 50.0f), ref));
    // straight below it: inside the time window, outside the radius
    CHECK(editor->keyAt(PointF(100.0f, 56.0f), ref));
    CHECK(!editor->keyAt(PointF(100.0f, 57.0f), ref));
    CHECK(!editor->keyAt(PointF(150.0f, 100.0f), ref));
    REQUIRE(editor->keyAt(PointF(52.0f, 179.0f), ref));
    CHECK(ref == controls::CurveKeyRef{1, 0});

    // zoomed in 2x about the key: it stays, the radius stays on screen
    editor->zoomAt(2.0, 2.0, PointF(100.0f, 50.0f));
    CHECK(editor->scaleX() == doctest::Approx(200.0));
    CHECK(editor->curveToLocal(CurvePoint{1.0, 1.5}).x == doctest::Approx(100.0f));
    CHECK(editor->curveToLocal(CurvePoint{2.0, 0.5}).x == doctest::Approx(300.0f));
    CHECK(editor->curveToLocal(CurvePoint{2.0, 0.5}).y == doctest::Approx(250.0f));
    CHECK(editor->keyAt(PointF(106.0f, 50.0f), ref));
    CHECK(!editor->keyAt(PointF(107.0f, 50.0f), ref));
    CHECK(!editor->keyAt(PointF(100.0f, 57.0f), ref));
    REQUIRE(editor->keyAt(PointF(299.0f, 249.0f), ref));
    CHECK(ref == controls::CurveKeyRef{0, 2});

    // Two keys close together: the nearer; coincident curves: the first disc is on top.
    scene.a.addKey(1.04, 1.5);
    editor->curveChanged(0);
    editor->setView(0, 4, 0, 2);
    REQUIRE(editor->keyAt(PointF(103.0f, 50.0f), ref));
    CHECK(ref == controls::CurveKeyRef{0, 2});
    scene.b.addKey(1.0, 1.5);
    editor->curveChanged(1);
    REQUIRE(editor->keyAt(PointF(100.0f, 50.0f), ref));
    CHECK(ref == controls::CurveKeyRef{0, 1});
}

TEST_CASE("CurveEditor: the wheel zooms about the cursor, Ctrl the time alone, Shift the value alone")
{
    Scene scene;
    controls::CurveEditor *editor=scene.editor;

    scene.move(scene.at(100, 50));
    scene.canvas->inputMouseWheel(60);
    scene.frame();
    CHECK(editor->viewX1()-editor->viewX0() == doctest::Approx(4.0/1.1));
    CHECK(editor->viewY1()-editor->viewY0() == doctest::Approx(2.0/1.1));
    CHECK(editor->curveToLocal(CurvePoint{1.0, 1.5}).x == doctest::Approx(100.0f));
    CHECK(editor->curveToLocal(CurvePoint{1.0, 1.5}).y == doctest::Approx(50.0f));

    const double spanY=editor->viewY1()-editor->viewY0();

    scene.key(keys::Control, true);
    scene.canvas->inputMouseWheel(-60);
    scene.frame();
    scene.key(keys::Control, false);
    CHECK(editor->viewX1()-editor->viewX0() == doctest::Approx(4.0));
    CHECK(editor->viewY1()-editor->viewY0() == doctest::Approx(spanY));

    scene.key(keys::Shift, true);
    scene.canvas->inputMouseWheel(-60);
    scene.frame();
    scene.key(keys::Shift, false);
    CHECK(editor->viewX1()-editor->viewX0() == doctest::Approx(4.0));
    CHECK(editor->viewY1()-editor->viewY0() == doctest::Approx(2.0));
    CHECK(editor->curveToLocal(CurvePoint{1.0, 1.5}).y == doctest::Approx(50.0f));
}

TEST_CASE("CurveEditor: a right drag pans with no menu, A and F frame the keys")
{
    Scene scene;
    controls::CurveEditor *editor=scene.editor;

    scene.press(scene.at(300, 50), 1);
    scene.move(scene.at(302, 51));
    CHECK(!editor->panning());
    CHECK(editor->viewX0() == 0.0);
    scene.move(scene.at(340, 70));
    CHECK(editor->panning());
    scene.release(1);
    CHECK(editor->viewX0() == doctest::Approx(-0.4));
    CHECK(editor->viewY0() == doctest::Approx(0.2));
    CHECK(scene.canvas->shownContextMenu() == nullptr);
    CHECK(editor->selectionSize() == 0);

    // A: every key, FramePadding of the extent around them
    scene.click(scene.at(150, 100));
    scene.canvas->inputCharacter('a');
    scene.frame();
    CHECK(editor->viewX0() == doctest::Approx(0.0-0.35));
    CHECK(editor->viewX1() == doctest::Approx(3.5+0.35));
    CHECK(editor->viewY0() == doctest::Approx(0.2-0.16));
    CHECK(editor->viewY1() == doctest::Approx(1.8+0.16));

    // F: the selection, one key a span of 1 around it
    editor->setView(0, 4, 0, 2);
    scene.click(scene.at(100, 50));
    scene.canvas->inputCharacter('f');
    scene.frame();
    CHECK(editor->viewX0() == doctest::Approx(0.5));
    CHECK(editor->viewX1() == doctest::Approx(1.5));
    CHECK(editor->viewY0() == doctest::Approx(1.0));
    CHECK(editor->viewY1() == doctest::Approx(2.0));
}

TEST_CASE("CurveEditor: a pan hovers and zooms at the captured mouse, including axis modifiers")
{
    Scene scene;
    auto *editor=scene.editor;

    scene.press(scene.at(100, 50), 2);
    scene.move(scene.at(200, 100));
    REQUIRE(editor->panning());
    controls::CurveKeyRef hovered;
    CHECK(editor->getHoveredKey(hovered));
    CHECK(hovered == controls::CurveKeyRef{0, 1});

    const CurvePoint fixed=editor->localToCurve(PointF(200, 100));
    const double spanX=editor->viewX1()-editor->viewX0();
    const double spanY=editor->viewY1()-editor->viewY0();
    SUBCASE("both axes")
    {
        scene.canvas->inputMouseWheel(60);
        CHECK(editor->viewX1()-editor->viewX0() == doctest::Approx(spanX/1.1));
        CHECK(editor->viewY1()-editor->viewY0() == doctest::Approx(spanY/1.1));
    }
    SUBCASE("Shift pressed during the capture")
    {
        scene.key(keys::Shift);
        scene.canvas->inputMouseWheel(60);
        scene.key(keys::Shift, false);
        CHECK(editor->viewX1()-editor->viewX0() == doctest::Approx(spanX));
        CHECK(editor->viewY1()-editor->viewY0() == doctest::Approx(spanY/1.1));
    }
    SUBCASE("Ctrl pressed during the capture")
    {
        scene.key(keys::Control);
        scene.canvas->inputMouseWheel(60);
        scene.key(keys::Control, false);
        CHECK(editor->viewX1()-editor->viewX0() == doctest::Approx(spanX/1.1));
        CHECK(editor->viewY1()-editor->viewY0() == doctest::Approx(spanY));
    }
    const CurvePoint after=editor->localToCurve(PointF(200, 100));
    CHECK(after.x == doctest::Approx(fixed.x));
    CHECK(after.y == doctest::Approx(fixed.y));
    // After the wheel, subsequent motion continues from the new view.
    const double x0=editor->viewX0();
    const double y0=editor->viewY0();
    const double scaleX=editor->scaleX(), scaleY=editor->scaleY();
    scene.move(scene.at(210, 110));
    CHECK(editor->viewX0() == doctest::Approx(x0-10/scaleX));
    CHECK(editor->viewY0() == doctest::Approx(y0+10/scaleY));
    scene.release(2);
}

TEST_CASE("CurveEditor: a reorder drag keeps the selected keys without a selection event")
{
    Scene scene;
    scene.editor->setSelection(refs({{0, 1}, {1, 1}}));
    scene.press(scene.at(100, 50));
    scene.move(scene.at(250, 40));
    scene.release();
    CHECK(scene.editor->getSelection() == refs({{0, 2}, {1, 1}}));
    CHECK(scene.events.selections == 0);
    CHECK(scene.events.changes == 2);
}

TEST_CASE("CurveEditor: a right press during a key drag keeps the gesture and selection")
{
    Scene scene;
    scene.press(scene.at(100, 50));
    scene.move(scene.at(120, 40));
    scene.editor->rightPressAt(scene.at(300, 50));
    CHECK(scene.editor->getSelection() == refs({{0, 1}}));
    CHECK(scene.canvas->shownContextMenu() == nullptr);
    CHECK(scene.editor->getKeyMenu() == nullptr);
    CHECK(scene.editor->draggingKeys());
    CHECK(scene.events.selections == 1);
    scene.release();
}

TEST_CASE("CurveEditor: disabling drops the hovered key")
{
    Scene scene;
    controls::CurveKeyRef hovered;
    scene.move(scene.at(100, 50));
    REQUIRE(scene.editor->getHoveredKey(hovered));
    scene.editor->setDisabled(true);
    scene.frame();
    CHECK(!scene.editor->getHoveredKey(hovered));
}

TEST_CASE("CurveEditor: onChanged can report an app edit and stop the remaining user edits")
{
    Scene scene;
    auto *editor=scene.editor;
    struct App: event::Handler
    {
        void changed(event::Info)
        {
            ++calls;
            editor->curveChanged(0);
        }
        controls::CurveEditor *editor;
        int calls=0;
    } app;
    app.editor=editor;
    editor->onChanged.add(&app, &App::changed);
    editor->setSelection(refs({{0, 1}, {0, 2}}));

    SUBCASE("drag")
    {
        scene.press(scene.at(100, 50));
        scene.move(scene.at(110, 40));
        CHECK(!editor->draggingKeys());
        scene.release();
    }
    SUBCASE("nudge") { editor->nudgeSelection(0.1, 0.1); }
    SUBCASE("delete") { CHECK(editor->deleteSelection()); CHECK(scene.a.keyCount() == 3); }
    SUBCASE("mode") { editor->setSelectionMode(Mode::Constant); }
    SUBCASE("broken") { editor->setSelectionBroken(true); }
    CHECK(app.calls == 1);
    CHECK(scene.events.changes == 1);
}

TEST_CASE("CurveEditor: an onChanged handler setting the selection stops a nudge")
{
    Scene scene;
    auto *editor=scene.editor;
    struct App: event::Handler
    {
        void changed(event::Info)
        {
            ++calls;
            if(clear)
                editor->clearSelection();
            else
                editor->setSelection({controls::CurveKeyRef{0, 0}});
        }
        controls::CurveEditor *editor;
        bool clear=false;
        int calls=0;
    } app;
    app.editor=editor;
    editor->onChanged.add(&app, &App::changed);
    editor->setSelection(refs({{0, 1}, {0, 2}}));

    SUBCASE("set") { app.clear=false; }
    SUBCASE("clear") { app.clear=true; }
    // forward: key 2 moves first, its handler sets the selection, key 1 stays
    editor->nudgeSelection(0.1, 0.0);
    CHECK(app.calls == 1);
    CHECK(scene.events.changes == 1);
    CHECK(scene.a.key(2).time == doctest::Approx(2.1));
    CHECK(scene.a.key(1).time == doctest::Approx(1.0));
    CHECK(scene.a.key(0).time == doctest::Approx(0.0));
}

TEST_CASE("CurveEditor: a wheel zoom during a key drag keeps the key under the cursor")
{
    Scene scene;
    controls::CurveEditor *editor=scene.editor;

    scene.press(scene.at(100, 50));
    scene.move(scene.at(150, 50));
    REQUIRE(editor->draggingKeys());
    CHECK(scene.a.key(1).time == doctest::Approx(1.5));
    scene.canvas->inputMouseWheel(60);
    scene.frame();
    REQUIRE(editor->scaleX() > 100.0);
    // the zoom is about the cursor: the key is still under it
    CHECK(scene.a.key(1).time == doctest::Approx(1.5));
    scene.move(scene.at(170, 40));
    const CurvePoint mouse=editor->localToCurve(PointF(170, 40));
    CHECK(scene.a.key(1).time == doctest::Approx(mouse.x));
    CHECK(scene.a.key(1).value == doctest::Approx(mouse.y));
    scene.release();
}

TEST_CASE("CurveEditor: a wheel zoom during a marquee keeps its press on the curve point")
{
    Scene scene;
    controls::CurveEditor *editor=scene.editor;

    scene.press(scene.at(250, 120));
    const CurvePoint press=editor->localToCurve(PointF(250, 120));
    scene.move(scene.at(280, 100));
    REQUIRE(editor->marqueeActive());
    scene.canvas->inputMouseWheel(60);
    scene.frame();
    REQUIRE(editor->scaleX() > 100.0);
    const PointF corner=editor->curveToLocal(press);
    const Rect rect=editor->getMarqueeRect();

    CHECK(rect.x == int(std::floor(corner.x)));
    CHECK(rect.y+rect.h == int(std::ceil(corner.y)));
    CHECK(rect.x+rect.w == 280);
    scene.release();
}

TEST_CASE("CurveEditor: disabling closes the key menu, whose items then edit nothing")
{
    Scene scene;
    controls::CurveEditor *editor=scene.editor;

    scene.click(scene.at(300, 50), 1);
    controls::Menu *menu=editor->getKeyMenu();
    REQUIRE(menu != nullptr);
    REQUIRE(scene.canvas->shownContextMenu() == menu);

    editor->setDisabled(true);
    scene.frame();
    CHECK(scene.canvas->shownContextMenu() == nullptr);

    for(controls::Base *child : menu->getChildren())
    {
        if(auto *item=gweni_cast<controls::MenuItem>(child))
        {
            if(item->getText() == "Linear")
                item->onMenuItemSelected.call(item);
        }
    }
    CHECK((scene.a.key(3).mode == Mode::Constant));
    CHECK(scene.events.changes == 0);
}

TEST_CASE("CurveEditor: a click selects, Shift adds, Ctrl toggles, empty space clears")
{
    Scene scene;
    controls::CurveEditor *editor=scene.editor;

    scene.click(scene.at(100, 50));
    CHECK(editor->getSelection() == refs({{0, 1}}));
    CHECK(scene.events.selections == 1);
    scene.click(scene.at(100, 50));
    CHECK(scene.events.selections == 1);

    scene.key(keys::Shift, true);
    scene.click(scene.at(350, 20));
    scene.key(keys::Shift, false);
    CHECK(editor->getSelection() == refs({{0, 1}, {1, 1}}));
    CHECK(scene.events.selections == 2);

    // a plain click on a key of the selection makes it the selection
    scene.click(scene.at(350, 20));
    CHECK(editor->getSelection() == refs({{1, 1}}));
    CHECK(scene.events.selections == 3);

    scene.key(keys::Control, true);
    scene.click(scene.at(200, 150));
    CHECK(editor->getSelection() == refs({{0, 2}, {1, 1}}));
    scene.click(scene.at(350, 20));
    CHECK(editor->getSelection() == refs({{0, 2}}));
    // a click on empty space under Ctrl keeps it
    scene.click(scene.at(150, 100));
    scene.key(keys::Control, false);
    CHECK(editor->getSelection() == refs({{0, 2}}));
    CHECK(scene.events.selections == 5);

    scene.click(scene.at(150, 100));
    CHECK(editor->selectionSize() == 0);
    CHECK(scene.events.selections == 6);

    // the app's selection fires nothing
    editor->setSelection(refs({{0, 0}}));
    editor->clearSelection();
    CHECK(scene.events.selections == 6);
    CHECK(scene.events.changes == 0);
}

TEST_CASE("CurveEditor: a marquee selects what it touches, logging only the keys that changed")
{
    Scene scene;
    controls::CurveEditor *editor=scene.editor;

    scene.press(scene.at(150, 10));
    scene.move(scene.at(152, 11));
    CHECK(editor->getMarqueeRect().w == 0);
    editor->clearPendingEdits();
    scene.move(scene.at(320, 100));
    CHECK((editor->getMarqueeRect() == Rect(150, 10, 170, 90)));
    CHECK(editor->getSelection() == refs({{0, 3}}));
    REQUIRE(editor->pendingEdits(0).size() == 1);
    CHECK((editor->pendingEdits(0)[0].kind == Kind::KeyLook));
    CHECK(editor->pendingEdits(0)[0].first == 3);
    CHECK(editor->pendingEdits(1).empty());

    editor->clearPendingEdits();
    scene.move(scene.at(390, 100));
    CHECK(editor->getSelection() == refs({{0, 3}, {1, 1}}));
    CHECK(editor->pendingEdits(0).empty());
    REQUIRE(editor->pendingEdits(1).size() == 1);
    CHECK(editor->pendingEdits(1)[0].first == 1);
    CHECK(scene.events.selections == 0);

    // Escape: the selection back, no event, the release ending nothing
    scene.key(keys::Escape, true);
    scene.key(keys::Escape, false);
    CHECK(editor->selectionSize() == 0);
    CHECK(!editor->marqueeActive());
    scene.release();
    CHECK(scene.events.selections == 0);

    // a marquee let go fires once
    scene.press(scene.at(150, 10));
    scene.move(scene.at(320, 100));
    scene.release();
    CHECK(editor->getSelection() == refs({{0, 3}}));
    CHECK(scene.events.selections == 1);

    // Shift adds to the selection
    scene.key(keys::Shift, true);
    scene.press(scene.at(30, 170));
    scene.move(scene.at(70, 190));
    scene.release();
    scene.key(keys::Shift, false);
    CHECK(editor->getSelection() == refs({{0, 3}, {1, 0}}));
    CHECK(scene.events.selections == 2);
}

TEST_CASE("CurveEditor: a key drag moves the selection, past a neighbour too, Escape puts it back")
{
    Scene scene;
    controls::CurveEditor *editor=scene.editor;

    scene.press(scene.at(100, 50));
    CHECK(editor->draggingKeys());
    CHECK(scene.events.changes == 0);
    editor->clearPendingEdits();
    scene.move(scene.at(125, 40));
    CHECK(scene.a.key(1).time == doctest::Approx(1.25));
    CHECK(scene.a.key(1).value == doctest::Approx(1.6));
    CHECK(scene.events.changes == 1);
    CHECK(scene.events.last == controls::CurveKeyRef{0, 1});
    // the move's splices: the spans it changed, and the key
    REQUIRE(editor->pendingEdits(0).size() == 2);
    CHECK((editor->pendingEdits(0)[0].kind == Kind::Spans));
    CHECK(editor->pendingEdits(0)[0].first == scene.a.lastChange().first);
    CHECK((editor->pendingEdits(0)[1].kind == Kind::Keys));
    CHECK(editor->pendingEdits(0)[1].first == 1);
    CHECK(editor->pendingEdits(0)[1].removed == 1);
    CHECK(editor->pendingEdits(1).empty());

    // the same point again: nothing changes, nothing fires
    scene.move(scene.at(125, 40));
    CHECK(scene.events.changes == 1);

    // past key 2 (time 2): reordered, the selection following it
    scene.move(scene.at(250, 40));
    CHECK(scene.a.key(2).time == doctest::Approx(2.5));
    CHECK(scene.a.key(1).time == doctest::Approx(2.0));
    CHECK(editor->getSelection() == refs({{0, 2}}));
    CHECK(scene.events.last == controls::CurveKeyRef{0, 2});
    CHECK(editor->getTimeField()->getNumber() == doctest::Approx(2.5));

    // Escape: back where it was, at its index
    scene.key(keys::Escape, true);
    scene.key(keys::Escape, false);
    CHECK(scene.a.key(1).time == doctest::Approx(1.0));
    CHECK(scene.a.key(1).value == doctest::Approx(1.5));
    CHECK(editor->getSelection() == refs({{0, 1}}));
    CHECK(!editor->draggingKeys());
    const int changes=scene.events.changes;
    scene.move(scene.at(300, 120));
    scene.release();
    CHECK(scene.a.key(1).time == doctest::Approx(1.0));
    CHECK(scene.events.changes == changes);

    // Shift keeps to the axis moved most; two keys move together
    scene.key(keys::Shift, true);
    scene.press(scene.at(200, 150));
    CHECK(editor->getSelection() == refs({{0, 1}, {0, 2}}));
    scene.move(scene.at(230, 140));
    scene.release();
    scene.key(keys::Shift, false);
    CHECK(scene.a.key(1).time == doctest::Approx(1.3));
    CHECK(scene.a.key(1).value == doctest::Approx(1.5));
    CHECK(scene.a.key(2).time == doctest::Approx(2.3));
    CHECK(scene.a.key(2).value == doctest::Approx(0.5));
}

TEST_CASE("CurveEditor: a selected Free key's handle drags its tangent, joined unless broken")
{
    Scene scene;
    controls::CurveEditor *editor=scene.editor;

    // a key not Free shows none
    scene.click(scene.at(100, 50));
    CHECK(!editor->showsHandles(0, 1));

    scene.click(scene.at(200, 150));
    REQUIRE(editor->showsHandles(0, 2));

    PointF in, out;

    editor->handlePositions(0, 2, in, out);
    CHECK(out.x == doctest::Approx(200.0f+controls::CurveEditor::HandleLength));
    CHECK(out.y == doctest::Approx(150.0f));
    CHECK(in.x == doctest::Approx(200.0f-controls::CurveEditor::HandleLength));

    scene.press(scene.at(240, 150));
    CHECK(editor->draggingTangent());
    scene.move(scene.at(240, 110));
    CHECK(scene.a.key(2).outTangent == doctest::Approx(1.0));
    CHECK(scene.a.key(2).inTangent == doctest::Approx(1.0));
    CHECK(scene.events.changes == 1);
    scene.release();
    CHECK(editor->getSelection() == refs({{0, 2}}));

    // broken: the in handle sets the in tangent alone
    editor->setSelectionBroken(true);
    CHECK(scene.a.key(2).broken);
    editor->handlePositions(0, 2, in, out);
    scene.press(scene.at(in.x, in.y));
    REQUIRE(editor->draggingTangent());
    scene.move(scene.at(160, 150));
    CHECK(scene.a.key(2).inTangent == doctest::Approx(0.0));
    CHECK(scene.a.key(2).outTangent == doctest::Approx(1.0));

    // Escape puts it back
    scene.key(keys::Escape, true);
    scene.key(keys::Escape, false);
    CHECK(scene.a.key(2).inTangent == doctest::Approx(1.0));
    scene.release();
}

TEST_CASE("CurveEditor: Delete removes the selection, onChanged per key")
{
    Scene scene;
    controls::CurveEditor *editor=scene.editor;

    // two keys of one curve and one of the other
    scene.click(scene.at(100, 50));
    scene.key(keys::Shift, true);
    scene.click(scene.at(300, 50));
    scene.click(scene.at(350, 20));
    scene.key(keys::Shift, false);
    const int selections=scene.events.selections;

    REQUIRE(editor->getPlot()->isFocussed());
    scene.tap(keys::Delete);
    REQUIRE(scene.a.keyCount() == 2);
    CHECK(scene.a.key(0).time == 0.0);
    CHECK(scene.a.key(1).time == 2.0);
    CHECK(scene.b.keyCount() == 1);
    CHECK(editor->selectionSize() == 0);
    CHECK(scene.events.changes == 3);
    CHECK(scene.events.selections == selections+1);

    // nothing selected: nothing more
    scene.tap(keys::Delete);
    CHECK(scene.events.changes == 3);
    // Escape with no gesture goes on up
    CHECK(!editor->cancelGesture());
}

TEST_CASE("CurveEditor: a double-click near a curve adds a key on it there")
{
    Scene scene;
    controls::CurveEditor *editor=scene.editor;
    const float y=editor->curveToLocal(CurvePoint{1.5, scene.a.evaluate(1.5)}).y;

    scene.doubleClick(scene.at(150, y+3));
    REQUIRE(scene.a.keyCount() == 5);
    CHECK(scene.a.key(2).time == doctest::Approx(1.5));
    CHECK(scene.a.key(2).value == doctest::Approx(scene.a.evaluate(1.5)));
    CHECK(editor->getSelection() == refs({{0, 2}}));
    CHECK(scene.events.changes == 1);
    CHECK(scene.events.last == controls::CurveKeyRef{0, 2});

    // far from every curve: nothing
    scene.doubleClick(scene.at(150, 5));
    CHECK(scene.a.keyCount() == 5);
    CHECK(scene.b.keyCount() == 2);
    CHECK(scene.events.changes == 1);
}

TEST_CASE("CurveEditor: a right-click on a key opens the key menu, which sets its mode")
{
    Scene scene;
    controls::CurveEditor *editor=scene.editor;

    // on empty space: no menu
    scene.click(scene.at(150, 100), 1);
    CHECK(scene.canvas->shownContextMenu() == nullptr);

    scene.click(scene.at(300, 50), 1);
    controls::Menu *menu=editor->getKeyMenu();
    REQUIRE(menu != nullptr);
    CHECK(scene.canvas->shownContextMenu() == menu);
    CHECK(editor->getSelection() == refs({{0, 3}}));
    CHECK(scene.events.selections == 1);

    auto item=[&](const String &name)
    {
        for(controls::Base *child : menu->getChildren())
        {
            if(auto *menuItem=gweni_cast<controls::MenuItem>(child))
            {
                if(menuItem->getText() == name)
                    return menuItem;
            }
        }
        return static_cast<controls::MenuItem *>(nullptr);
    };

    REQUIRE(item("Linear"));
    item("Linear")->onMenuItemSelected.call(item("Linear"));
    CHECK((scene.a.key(3).mode == Mode::Linear));
    CHECK(scene.events.changes == 1);
    // the same mode again changes nothing
    item("Linear")->onMenuItemSelected.call(item("Linear"));
    CHECK(scene.events.changes == 1);

    REQUIRE(item("Broken"));
    item("Broken")->setChecked(true);
    item("Broken")->onMenuItemSelected.call(item("Broken"));
    CHECK((scene.a.key(3).mode == Mode::Free));
    CHECK(scene.a.key(3).broken);

    item("Delete")->onMenuItemSelected.call(item("Delete"));
    CHECK(scene.a.keyCount() == 3);
    CHECK(editor->selectionSize() == 0);
}

TEST_CASE("CurveEditor: the arrows nudge the selection a plot unit, Shift ten")
{
    Scene scene;

    scene.click(scene.at(100, 50));
    scene.tap(keys::Right);
    CHECK(scene.a.key(1).time == doctest::Approx(1.01));
    scene.key(keys::Shift, true);
    scene.tap(keys::Up);
    scene.key(keys::Shift, false);
    CHECK(scene.a.key(1).value == doctest::Approx(1.6));
    scene.tap(keys::Left);
    scene.tap(keys::Down);
    CHECK(scene.a.key(1).time == doctest::Approx(1.0));
    CHECK(scene.a.key(1).value == doctest::Approx(1.59));
    CHECK(scene.events.changes == 4);
}

TEST_CASE("CurveEditor: the fields show the one selected key, blank and disabled otherwise")
{
    Scene scene;
    controls::CurveEditor *editor=scene.editor;
    controls::DragNumber *time=editor->getTimeField();
    controls::DragNumber *value=editor->getValueField();

    CHECK(time->isBlank());
    CHECK(time->isDisabled());
    CHECK(value->isBlank());
    CHECK(time->getText().empty());

    scene.click(scene.at(100, 50));
    CHECK(!time->isBlank());
    CHECK(!time->isDisabled());
    CHECK(time->getNumber() == doctest::Approx(1.0));
    CHECK(value->getNumber() == doctest::Approx(1.5));
    CHECK(time->getText() == "t 1.000");

    value->setNumber(0.25);
    CHECK(scene.a.key(1).value == doctest::Approx(0.25));
    CHECK(scene.events.changes == 1);
    time->setNumber(2.5);
    CHECK(scene.a.key(2).time == doctest::Approx(2.5));
    CHECK(editor->getSelection() == refs({{0, 2}}));
    CHECK(scene.events.changes == 2);

    scene.key(keys::Shift, true);
    scene.click(scene.at(350, 20));
    scene.key(keys::Shift, false);
    CHECK(time->isBlank());
    CHECK(time->isDisabled());
}

TEST_CASE("CurveEditor: a hover logs the look of the key it left and the one it took")
{
    Scene scene;
    controls::CurveEditor *editor=scene.editor;
    controls::CurveKeyRef ref;

    editor->clearPendingEdits();
    scene.move(scene.at(101, 51));
    REQUIRE(editor->getHoveredKey(ref));
    CHECK(ref == controls::CurveKeyRef{0, 1});
    REQUIRE(editor->pendingEdits(0).size() == 1);
    CHECK((editor->pendingEdits(0)[0].kind == Kind::KeyLook));
    CHECK(editor->pendingEdits(0)[0].first == 1);

    // a move on the same key logs nothing
    editor->clearPendingEdits();
    scene.move(scene.at(99, 50));
    CHECK(editor->pendingEdits(0).empty());

    scene.move(scene.at(351, 21));
    CHECK(editor->getHoveredKey(ref));
    CHECK(ref == controls::CurveKeyRef{1, 1});
    REQUIRE(editor->pendingEdits(0).size() == 1);
    REQUIRE(editor->pendingEdits(1).size() == 1);

    editor->clearPendingEdits();
    scene.move(scene.at(150, 100));
    CHECK(!editor->getHoveredKey(ref));
    CHECK(editor->pendingEdits(1).size() == 1);
}

TEST_CASE("CurveEditor: made under a hidden parent, it shows and works once the parent is shown")
{
    test::StubRenderer renderer;
    test::StubSkin skin(&renderer);
    controls::Canvas *canvas=new controls::Canvas(&skin);
    controls::Base *page=canvas->newChild<controls::Base>();

    canvas->setSize(800, 600);
    page->setHidden(true);

    // the hidden parent tells the editor it is not visible as it is attached,
    // before its own construction has made its parts
    controls::CurveEditor *editor=page->newChild<controls::CurveEditor>();
    Curve curve({makeKey(0, 0.5), makeKey(1, 1.5)});

    editor->addCurve(&curve);
    CHECK(!editor->visible());
    page->setHidden(false);
    canvas->updateCanvas();
    canvas->renderCanvas();
    CHECK(editor->visible());
    CHECK(editor->getPlot()->visible());
    delete canvas;
}
