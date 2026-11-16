/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  GradientEditor on a real Canvas with the real (un-init()'d) textured skin
 *  on a StubRenderer, pressed and dragged through input::onMouseClicked and
 *  Canvas::inputMouseMoved and keyed through input::onKeyEvent.
 *
 *  The bar spans the width less a handle, the strip right under it the whole
 *  width, each stop's handle centred on its position; the handles are the
 *  strip's children in stop order. A press on the strip away from a handle
 *  adds a stop of evaluate(t)'s colour there, selects it, focuses its handle
 *  and drags it; a press on a handle selects and drags it, clamped to 0..1,
 *  past other stops too (the gradient, the handles and the strip's children
 *  reordered, the selection following it); a press and release with no motion
 *  changes nothing; Escape mid-drag puts it back and the mouse moves nothing
 *  more until the release. Left/Right nudge by 0.01 (Shift 0.1), clamped;
 *  Delete removes the selected stop, focusing the next one, and is refused
 *  for the last; Tab and Shift+Tab go through the stops in order, a reordered
 *  one included. The row's ColorEdit and DragNumber show the selected stop
 *  and edit it; a double-click on a handle opens the picker. onChanged fires
 *  once per real change by the user (a drag frame that moves the stop, an
 *  add, a delete, a nudge, a colour, a position) and never for the app's own
 *  edits (gradientChanged(), setSelectedStop()). Review 1's fixes: a second
 *  press where a strip press just added a stop is the new handle's, and a
 *  nudge that moves a handle from under the mouse takes its hover (the hover
 *  is hit-tested again); Escape gives back every stop as the press found it,
 *  two at one position in their order; a drag moves its own handle's stop
 *  after a Tab moved the selection, and Left/Right/Delete the focused handle's
 *  own stop after the app moved it; a disabled editor's handles take no focus
 *  or Tab, lose one they had, and a press changes no selection.
 */
#include "headlessFixture.h"

#include <gweni/controls/colorEdit.h>
#include <gweni/controls/dragNumber.h>
#include <gweni/controls/gradientEditor.h>
#include <gweni/controls/textBox.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <doctest/doctest.h>

#include <string>

using namespace gweni;
using controls::GradientEditor;
using controls::internal::GradientStopHandle;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    float clock=nextClock();

    //! The input handler keeps the last click's time and place across cases.
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
        for(int key : {keys::Shift, keys::Alt, keys::Control})
            input::onKeyEvent(canvas, key, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    //! An editor at 20,20 on \p gradient, laid out.
    GradientEditor *editor(Gradient *gradient)
    {
        GradientEditor *editor=canvas->newChild<GradientEditor>();

        editor->setPos(20, 20);
        editor->setGradient(gradient);
        frame();
        return editor;
    }

    void move(int x, int y)
    {
        canvas->inputMouseMoved(x, y, 0, 0);
    }

    void press(Point at)
    {
        clock+=1.0f;
        move(at.x, at.y);
        input::onMouseClicked(canvas, 0, true);
    }

    void release()
    {
        input::onMouseClicked(canvas, 0, false);
        frame();
    }

    void key(int key)
    {
        input::onKeyEvent(canvas, key, true);
        input::onKeyEvent(canvas, key, false);
        frame();
    }

    void hold(int key, bool down)
    {
        input::onKeyEvent(canvas, key, down);
    }
};

Point centreOf(controls::Base *control)
{
    const Point at=control->localPosToCanvas();

    return Point(at.x+control->getWidth()/2, at.y+control->getHeight()/2);
}

//! The canvas point on the strip a stop at \p t is centred on.
Point stripAt(GradientEditor *editor, float t)
{
    controls::Base *strip=editor->getStrip();
    const Point at=strip->localPosToCanvas();

    return Point(at.x+editor->handleLeft(t)+GradientEditor::HandleWidth/2, at.y+strip->getHeight()/2);
}

struct Counter: public event::Handler
{
    int count=0;
    void changed(event::Info) { ++count; }
};

//! The red channel of each stop, in order: the stops' identities here.
std::string reds(const Gradient &gradient)
{
    std::string text;

    for(const GradientStop &stop : gradient.stops())
        text+=std::to_string(stop.color.r)+" ";
    return text;
}

Color red(unsigned char r) { return Color(r, 0, 0, 255); }

Gradient threeStops()
{
    return Gradient({{0.0f, red(10)}, {0.5f, red(20)}, {1.0f, red(30)}});
}

//! Three stops, the last short of 1: a stop dragged to 1 passes it.
Gradient spreadStops()
{
    return Gradient({{0.0f, red(10)}, {0.5f, red(20)}, {0.8f, red(30)}});
}

//! The handles' order as the strip's children: the stop order they stand for.
bool childrenInStopOrder(GradientEditor *editor)
{
    size_t i=0;

    for(controls::Base *child : editor->getStrip()->getChildren())
    {
        if(child->hidden())
            continue;
        if(child != editor->getHandle(i))
            return false;
        ++i;
    }
    return i == editor->getHandleCount();
}

}//namespace

TEST_CASE("GradientEditor: the bar, the strip and a handle centred on each stop")
{
    Scene scene;
    Gradient gradient=threeStops();
    GradientEditor *editor=scene.editor(&gradient);

    CHECK(editor->getWidth() == 320);
    CHECK(editor->getBar()->getX() == GradientEditor::HandleWidth/2);
    CHECK(editor->getBar()->getWidth() == 320-GradientEditor::HandleWidth);
    CHECK(editor->getBar()->getHeight() == GradientEditor::BarHeight);
    CHECK(editor->getStrip()->getY() == GradientEditor::BarHeight);
    CHECK(editor->getStrip()->getWidth() == 320);
    CHECK(editor->getColorEdit()->getY() >= GradientEditor::BarHeight+GradientEditor::StripHeight+GradientEditor::Gap);
    CHECK(editor->getHeight() == editor->getColorEdit()->bottom());

    REQUIRE(editor->getHandleCount() == 3);
    const int span=320-GradientEditor::HandleWidth;

    CHECK(editor->getHandle(0)->getX() == 0);
    CHECK(editor->getHandle(1)->getX() == (span+1)/2);
    CHECK(editor->getHandle(2)->getX() == span);
    CHECK((editor->getHandle(1)->getStopColor() == red(20)));
    CHECK(childrenInStopOrder(editor));
    CHECK(editor->getSelectedStop() == 0);
    CHECK((editor->getColorEdit()->getColor() == red(10)));
}

TEST_CASE("GradientEditor: a press on the strip adds a stop of evaluate(t)'s colour, selects it and drags it")
{
    Scene scene;
    Gradient gradient=threeStops();
    GradientEditor *editor=scene.editor(&gradient);
    Counter changed, selection;

    editor->onChanged.add(&changed, &Counter::changed);
    editor->onSelectionChange.add(&selection, &Counter::changed);

    const Point at=stripAt(editor, 0.25f);
    const float t=editor->stripPosition(at.x-editor->getStrip()->localPosToCanvas().x);
    const Color expected=gradient.evaluate(t);

    scene.press(at);
    REQUIRE(gradient.stopCount() == 4);
    CHECK(gradient.stop(1).position == t);
    CHECK((gradient.stop(1).color == expected));
    CHECK(changed.count == 1);
    CHECK(selection.count == 1);
    CHECK(editor->getSelectedStop() == 1);
    CHECK(editor->getHandle(1)->isFocussed());
    CHECK(editor->isDragging());
    CHECK(childrenInStopOrder(editor));
    CHECK((editor->getColorEdit()->getColor() == expected));
    CHECK(editor->getPositionField()->getNumber() == doctest::Approx(t));

    // the press drags it
    scene.move(at.x+30, at.y);
    CHECK(gradient.stop(1).position > t);
    CHECK(changed.count == 2);
    scene.release();
    CHECK_FALSE(editor->isDragging());
    scene.frame();
    CHECK(editor->getHandle(1)->getX() == editor->handleLeft(gradient.stop(1).position));
}

TEST_CASE("GradientEditor: a handle dragged past another reorders the stops and the handles")
{
    Scene scene;
    Gradient gradient=threeStops();
    GradientEditor *editor=scene.editor(&gradient);
    Counter changed;

    editor->onChanged.add(&changed, &Counter::changed);

    GradientStopHandle *first=editor->getHandle(0);
    const Point at=centreOf(first);

    // a press and release without motion changes nothing
    scene.press(at);
    scene.release();
    CHECK(changed.count == 0);
    CHECK(reds(gradient) == "10 20 30 ");

    scene.press(at);
    CHECK(editor->getDraggedHandle() == first);
    scene.move(stripAt(editor, 0.75f).x, at.y);
    CHECK(changed.count == 1);
    CHECK(reds(gradient) == "20 10 30 ");
    CHECK(gradient.stop(1).position == doctest::Approx(0.75f).epsilon(0.01));
    CHECK(editor->getHandle(1) == first);
    CHECK(editor->getSelectedStop() == 1);
    CHECK(childrenInStopOrder(editor));

    // the same place again moves nothing
    scene.move(stripAt(editor, 0.75f).x, at.y);
    CHECK(changed.count == 1);

    // clamped at the end, past the last stop (before it, at 1)
    scene.move(at.x+1000, at.y);
    CHECK(changed.count == 2);
    CHECK(reds(gradient) == "20 10 30 ");
    CHECK(gradient.stop(1).position == 1.0f);
    scene.move(at.x+1100, at.y);
    CHECK(changed.count == 2);

    scene.release();
    CHECK(editor->getPositionField()->getNumber() == doctest::Approx(1.0));
    CHECK(editor->getHandle(1)->getX() == editor->handleLeft(1.0f));
}

TEST_CASE("GradientEditor: Escape during a drag puts the stop back")
{
    Scene scene;
    Gradient gradient=spreadStops();
    GradientEditor *editor=scene.editor(&gradient);
    const Gradient before=gradient;
    Counter changed;

    editor->onChanged.add(&changed, &Counter::changed);

    GradientStopHandle *middle=editor->getHandle(1);
    const Point at=centreOf(middle);

    scene.press(at);
    scene.move(stripAt(editor, 0.9f).x, at.y);
    scene.move(stripAt(editor, 0.1f).x, at.y);
    CHECK(reds(gradient) == "10 20 30 ");
    scene.move(at.x+1000, at.y);
    CHECK(reds(gradient) == "10 30 20 ");
    CHECK(changed.count == 3);

    scene.key(keys::Escape);
    CHECK((gradient == before));
    CHECK(changed.count == 4);
    CHECK(editor->getHandle(1) == middle);
    CHECK(editor->getSelectedStop() == 1);
    CHECK(childrenInStopOrder(editor));

    // the mouse moves nothing more until the release
    scene.move(stripAt(editor, 0.8f).x, at.y);
    CHECK((gradient == before));
    CHECK(changed.count == 4);
    scene.release();
    CHECK_FALSE(editor->isDragging());

    // Escape with no drag is not the editor's
    Counter more;

    editor->onChanged.add(&more, &Counter::changed);
    scene.key(keys::Escape);
    CHECK(more.count == 0);
}

TEST_CASE("GradientEditor: Left and Right nudge the selected stop, Shift ten times as far, clamped")
{
    Scene scene;
    Gradient gradient=threeStops();
    GradientEditor *editor=scene.editor(&gradient);
    Counter changed;

    editor->onChanged.add(&changed, &Counter::changed);
    editor->getHandle(1)->focus();
    scene.frame();
    REQUIRE(editor->getSelectedStop() == 1);

    scene.key(keys::Right);
    CHECK(gradient.stop(1).position == doctest::Approx(0.51f));
    scene.key(keys::Left);
    scene.key(keys::Left);
    CHECK(gradient.stop(1).position == doctest::Approx(0.49f));
    CHECK(changed.count == 3);

    scene.hold(keys::Shift, true);
    scene.key(keys::Right);
    scene.hold(keys::Shift, false);
    CHECK(gradient.stop(1).position == doctest::Approx(0.59f));
    CHECK(editor->getPositionField()->getText() == "0.590");
    CHECK(changed.count == 4);

    // the first stop at 0 goes no lower, and says nothing
    editor->getHandle(0)->focus();
    scene.frame();
    scene.key(keys::Left);
    CHECK(gradient.stop(0).position == 0.0f);
    CHECK(changed.count == 4);

    // a nudge past a neighbour reorders it, the focus staying on its handle
    GradientStopHandle *last=editor->getHandle(2);

    last->focus();
    scene.frame();
    scene.hold(keys::Shift, true);
    for(int i=0; i < 5; ++i)
        scene.key(keys::Left);
    scene.hold(keys::Shift, false);
    CHECK(reds(gradient) == "10 30 20 ");
    CHECK(editor->getHandle(1) == last);
    CHECK(last->isFocussed());
    CHECK(editor->getSelectedStop() == 1);
}

TEST_CASE("GradientEditor: Tab and Shift+Tab go through the stops in order")
{
    Scene scene;
    Gradient gradient=threeStops();
    GradientEditor *editor=scene.editor(&gradient);
    Counter selection;

    editor->onSelectionChange.add(&selection, &Counter::changed);
    editor->getHandle(0)->focus();
    scene.frame();

    scene.key(keys::Tab);
    CHECK(editor->getHandle(1)->isFocussed());
    CHECK(editor->getSelectedStop() == 1);
    CHECK((editor->getColorEdit()->getColor() == red(20)));
    scene.key(keys::Tab);
    CHECK(editor->getHandle(2)->isFocussed());
    CHECK(selection.count == 2);

    scene.hold(keys::Shift, true);
    scene.key(keys::Tab);
    scene.hold(keys::Shift, false);
    CHECK(editor->getHandle(1)->isFocussed());
    CHECK(editor->getSelectedStop() == 1);

    // a stop moved to the front is the first stop Tab reaches
    editor->moveSelectedTo(0.0f);
    REQUIRE(reds(gradient) == "10 20 30 ");
    editor->getHandle(0)->focus();
    scene.frame();
    editor->moveSelectedTo(0.7f);
    REQUIRE(reds(gradient) == "20 10 30 ");
    editor->getHandle(0)->focus();
    scene.frame();
    CHECK((editor->getHandle(0)->getStopColor() == red(20)));
    scene.key(keys::Tab);
    CHECK(editor->getHandle(1)->isFocussed());
    CHECK((editor->getHandle(1)->getStopColor() == red(10)));

    // past the last stop Tab leaves the strip for the row
    scene.key(keys::Tab);
    scene.key(keys::Tab);
    CHECK_FALSE(editor->getHandle(0)->isFocussed());
    CHECK_FALSE(editor->getHandle(1)->isFocussed());
    CHECK_FALSE(editor->getHandle(2)->isFocussed());
}

TEST_CASE("GradientEditor: Delete removes the selected stop, never the last one")
{
    Scene scene;
    Gradient gradient=threeStops();
    GradientEditor *editor=scene.editor(&gradient);
    Counter changed;

    editor->onChanged.add(&changed, &Counter::changed);
    editor->getHandle(1)->focus();
    scene.frame();

    scene.key(keys::Delete);
    CHECK(reds(gradient) == "10 30 ");
    CHECK(editor->getHandleCount() == 2);
    CHECK(editor->getSelectedStop() == 1);
    CHECK(editor->getHandle(1)->isFocussed());
    CHECK((editor->getColorEdit()->getColor() == red(30)));
    CHECK(changed.count == 1);
    CHECK(childrenInStopOrder(editor));

    // the last stop of the list: the one before it next
    scene.key(keys::Delete);
    CHECK(reds(gradient) == "10 ");
    CHECK(editor->getHandle(0)->isFocussed());
    CHECK(changed.count == 2);

    // the last stop stays
    scene.key(keys::Delete);
    CHECK(reds(gradient) == "10 ");
    CHECK(editor->getHandleCount() == 1);
    CHECK(changed.count == 2);
    CHECK_FALSE(editor->deleteSelected());

    // the deleted handles are gone once the delayed deletes ran
    scene.frame();
    CHECK(editor->getStrip()->getChildren().size() == 1);
}

TEST_CASE("GradientEditor: the row edits the selected stop's colour and position")
{
    Scene scene;
    Gradient gradient=spreadStops();
    GradientEditor *editor=scene.editor(&gradient);
    Counter changed;

    editor->onChanged.add(&changed, &Counter::changed);
    editor->setSelectedStop(1);
    scene.frame();
    CHECK((editor->getColorEdit()->getColor() == red(20)));
    CHECK(editor->getPositionField()->getText() == "0.500");
    CHECK(changed.count == 0);

    editor->getColorEdit()->setColor(Color(1, 2, 3, 4));
    CHECK((gradient.stop(1).color == Color(1, 2, 3, 4)));
    CHECK((editor->getHandle(1)->getStopColor() == Color(1, 2, 3, 4)));
    CHECK(changed.count == 1);
    editor->getColorEdit()->setColor(Color(1, 2, 3, 4));
    CHECK(changed.count == 1);

    // typed into the R field: applied as the number is whole
    controls::TextBox *r=editor->getColorEdit()->getField(0);

    r->focus();
    r->setText("", false);
    scene.canvas->inputCharacter('9');
    CHECK((gradient.stop(1).color == Color(9, 2, 3, 4)));
    CHECK(changed.count == 2);

    // the position field moves it, past a neighbour too
    editor->getPositionField()->setNumber(0.25);
    CHECK(gradient.stop(1).position == 0.25f);
    CHECK(changed.count == 3);
    editor->getPositionField()->setNumber(0.9);
    CHECK(reds(gradient) == "10 30 9 ");
    CHECK(editor->getSelectedStop() == 2);
    CHECK((editor->getHandle(2)->getStopColor() == Color(9, 2, 3, 4)));
    CHECK(changed.count == 4);
}

TEST_CASE("GradientEditor: the app's own edits fire nothing")
{
    Scene scene;
    Gradient gradient=threeStops();
    GradientEditor *editor=scene.editor(&gradient);
    Counter changed, selection;

    editor->onChanged.add(&changed, &Counter::changed);
    editor->onSelectionChange.add(&selection, &Counter::changed);

    gradient.addStop(0.25f, red(40));
    gradient.setStopColor(0, red(50));
    editor->gradientChanged();
    scene.frame();
    CHECK(editor->getHandleCount() == 4);
    CHECK((editor->getHandle(0)->getStopColor() == red(50)));
    CHECK((editor->getHandle(1)->getStopColor() == red(40)));
    CHECK((editor->getColorEdit()->getColor() == red(50)));

    editor->setSelectedStop(3);
    gradient.removeStop(3);
    gradient.removeStop(2);
    editor->gradientChanged();
    scene.frame();
    CHECK(editor->getHandleCount() == 2);
    CHECK(editor->getSelectedStop() == 1);

    Gradient other({{0.3f, red(60)}});

    editor->setGradient(&other);
    scene.frame();
    CHECK(editor->getHandleCount() == 1);
    CHECK((editor->getColorEdit()->getColor() == red(60)));
    CHECK(changed.count == 0);
    CHECK(selection.count == 0);
}

TEST_CASE("GradientEditor: a double-click on a handle opens the picker")
{
    Scene scene;
    Gradient gradient=threeStops();
    GradientEditor *editor=scene.editor(&gradient);
    const Point at=centreOf(editor->getHandle(2));

    scene.press(at);
    scene.release();
    CHECK_FALSE(editor->getColorEdit()->isPickerOpen());
    input::onMouseClicked(scene.canvas, 0, true);
    scene.release();
    CHECK(editor->getColorEdit()->isPickerOpen());
    CHECK(editor->getSelectedStop() == 2);
    CHECK((gradient == threeStops()));
}

TEST_CASE("GradientEditor: a second press where a strip press added a stop is the new handle's")
{
    Scene scene;
    Gradient gradient=threeStops();
    GradientEditor *editor=scene.editor(&gradient);
    const Point at=stripAt(editor, 0.25f);

    // the release reaches the strip (no hit test since the press, as in one
    // event batch) and ends the drag all the same
    scene.press(at);
    scene.release();
    REQUIRE(gradient.stopCount() == 4);
    GradientStopHandle *added=editor->getHandle(1);

    CHECK_FALSE(editor->isDragging());
    CHECK(MouseFocus == nullptr);

    // the second press with no move: the hover was taken again once the new
    // handle was laid out under the mouse
    CHECK(HoveredControl == added);
    scene.clock+=1.0f;
    input::onMouseClicked(scene.canvas, 0, true);
    CHECK(gradient.stopCount() == 4);
    CHECK(editor->getDraggedHandle() == added);
    scene.release();
    CHECK(gradient.stopCount() == 4);

    // a nudge that takes the handle from under the mouse takes its hover
    scene.hold(keys::Shift, true);
    scene.key(keys::Right);
    scene.hold(keys::Shift, false);
    CHECK(added->isFocussed());
    CHECK(HoveredControl != added);
    CHECK(HoveredControl == editor->getStrip());
}

TEST_CASE("GradientEditor: Escape gives back two stops at one position in their order")
{
    Scene scene;
    Gradient gradient({{0.0f, red(1)}, {0.5f, red(2)}, {0.5f, red(3)}, {1.0f, red(4)}});
    GradientEditor *editor=scene.editor(&gradient);
    const Gradient before=gradient;
    const Point at=stripAt(editor, 0.5f);

    // the later handle is on top: the press takes stop 2 (red 3)
    scene.press(at);
    REQUIRE(editor->getDraggedHandle() == editor->getHandle(2));
    scene.move(stripAt(editor, 0.2f).x, at.y);
    REQUIRE(reds(gradient) == "1 3 2 4 ");
    scene.key(keys::Escape);
    CHECK(reds(gradient) == "1 2 3 4 ");
    CHECK((gradient == before));
    CHECK(childrenInStopOrder(editor));
    CHECK(editor->getSelectedStop() == 2);
    CHECK((editor->getHandle(2)->getStopColor() == red(3)));
    scene.release();
}

TEST_CASE("GradientEditor: a drag and the keys move their handle's own stop, not the selection")
{
    Scene scene;
    Gradient gradient({{0.0f, red(1)}, {0.3f, red(2)}, {0.6f, red(3)}, {1.0f, red(4)}});
    GradientEditor *editor=scene.editor(&gradient);
    const Gradient before=gradient;
    GradientStopHandle *dragged=editor->getHandle(1);
    const Point at=centreOf(dragged);

    scene.press(at);
    scene.move(stripAt(editor, 0.4f).x, at.y);
    // Tab with the button held: the next stop is selected, the drag is still
    // stop 1's
    scene.key(keys::Tab);
    CHECK(editor->getSelectedStop() == 2);
    scene.move(stripAt(editor, 0.45f).x, at.y);
    CHECK(reds(gradient) == "1 2 3 4 ");
    CHECK(gradient.stop(1).position == doctest::Approx(0.45f).epsilon(0.01));
    CHECK(gradient.stop(2).position == 0.6f);
    scene.key(keys::Escape);
    CHECK((gradient == before));
    scene.release();

    // the app selects another stop: Right moves the focused handle's own stop
    editor->getHandle(2)->focus();
    scene.frame();
    editor->setSelectedStop(0);
    scene.key(keys::Right);
    CHECK(gradient.stop(0).position == 0.0f);
    CHECK(gradient.stop(2).position == doctest::Approx(0.61f));
    CHECK(editor->getSelectedStop() == 2);

    // and Delete removes it
    editor->setSelectedStop(0);
    scene.key(keys::Delete);
    CHECK(reds(gradient) == "1 2 4 ");
}

TEST_CASE("GradientEditor: a disabled editor's handles take no focus and keep the selection")
{
    Scene scene;
    Gradient gradient=threeStops();
    GradientEditor *editor=scene.editor(&gradient);
    Counter selection;

    editor->onSelectionChange.add(&selection, &Counter::changed);
    editor->getHandle(2)->focus();
    scene.frame();
    REQUIRE(selection.count == 1);
    editor->setSelectedStop(0);

    // the focus a handle had is dropped
    editor->setDisabled(true);
    scene.frame();
    CHECK_FALSE(editor->getHandle(2)->isFocussed());

    scene.press(centreOf(editor->getHandle(1)));
    scene.release();
    CHECK_FALSE(editor->getHandle(1)->isFocussed());
    CHECK(editor->getSelectedStop() == 0);
    CHECK(selection.count == 1);
    CHECK_FALSE(editor->getHandle(1)->isTabable());

    editor->setDisabled(false);
    CHECK(editor->getHandle(1)->isTabable());
}

TEST_CASE("GradientEditor: an editor hidden during a drag ends it")
{
    Scene scene;
    Gradient gradient=threeStops();
    GradientEditor *editor=scene.editor(&gradient);
    GradientStopHandle *handle=editor->getHandle(1);
    const Point at=centreOf(handle);

    scene.press(at);
    scene.move(at.x+10, at.y);
    REQUIRE(editor->isDragging());
    const float moved=gradient.stop(1).position;

    CHECK(moved != doctest::Approx(0.5f));
    editor->hide();
    scene.release();
    editor->show();
    scene.frame();
    CHECK_FALSE(editor->isDragging());
    CHECK(editor->getDraggedHandle() == nullptr);

    // with the button up a move over the handle moves no stop
    const Point now=centreOf(handle);

    scene.move(now.x, now.y);
    scene.move(now.x+5, now.y);
    CHECK(gradient.stop(1).position == doctest::Approx(moved));

    // and Delete, refused during a drag, removes the focused handle's stop
    // (the hide took the focus)
    handle->focus();
    REQUIRE(handle->isFocussed());
    scene.key(keys::Delete);
    CHECK(gradient.stopCount() == 2);
}

TEST_CASE("GradientEditor: a drag whose handle another control took the mouse from has ended")
{
    Scene scene;
    Gradient gradient=threeStops();
    GradientEditor *editor=scene.editor(&gradient);
    GradientStopHandle *handle=editor->getHandle(1);
    const Point at=centreOf(handle);

    scene.press(at);
    REQUIRE(editor->isDragging());

    // another capture takes the mouse, and the release is its
    MouseFocus=editor;
    CHECK_FALSE(editor->isDragging());
    CHECK(editor->getDraggedHandle() == nullptr);
    scene.release();
    CHECK_FALSE(editor->isDragging());
}
