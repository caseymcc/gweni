/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  controls::Switch on a real Canvas, driven through the input handler, with
 *  the textured Switch renderer on a recording StubRenderer and a stepped
 *  platform::setTimeSource clock:
 *  - a click, Space and Return (each down and up, toggling on the release
 *    alone) toggle it, each firing onToggled once and onToggledOn or
 *    onToggledOff once; a click on its label toggles it too;
 *  - setOn with the value it has fires nothing and marks nothing;
 *  - disabled, a click, Space and Return toggle nothing, fire nothing and give
 *    it no keyboard focus; its cursor is normal, the hand restored on enabling;
 *  - a handler turning the switch back from onToggled or Button's toggle
 *    callers stops the outer call's stale notifications;
 *  - the slide: after a toggle the thumb is at the half-cosine eased place at
 *    each clock time (drawn there by the renderer, in the thumb's slot at z+1),
 *    the animation is registered while it moves and gone once it arrives, a
 *    step of under a unit marks nothing, a toggle mid-slide turns it round from
 *    where it is, hiding its parent mid-slide puts it at its end and cancels
 *    the animation in the next update, and a toggle while hidden or before the
 *    switch's first frame (on the canvas, or in containers built before it)
 *    jumps with no animation;
 *  - a toggle and its slide call no Base::invalidate() (controls::invalidateCalls()).
 */
#include "headlessFixture.h"

#include <gweni/anim.h>
#include <gweni/controls/switch.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <doctest/doctest.h>

#include <cmath>
#include <memory>
#include <vector>

using namespace gweni;

namespace
{

//! Records the rect and z of each rounded rect the renderers emit.
class DrawRecorder: public test::StubRenderer
{
public:
    struct Draw
    {
        size_t id;
        Rect rect;
        int z;
    };

    void drawRoundedRect(size_t primitiveId, Rect rect, float radius, float borderWidth, Color top,
        Color bottom, Color border, int zIndex) override
    {
        draws.push_back({primitiveId, rect, zIndex});
        StubRenderer::drawRoundedRect(primitiveId, rect, radius, borderWidth, top, bottom, border, zIndex);
    }

    std::vector<Draw> draws;
};

//! The textured skin's Switch renderer, a StubControl for the rest.
class SwitchSkin: public test::StubSkin
{
public:
    explicit SwitchSkin(test::StubRenderer *renderer): StubSkin(renderer) {}

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == "Switch")
            return std::make_unique<skin::textured::Switch>();
        return StubSkin::getControlRenderer(typeName);
    }
};

//! Counts a Caller's calls.
class Counter: public event::Handler
{
public:
    void count(event::Info)
    {
        ++calls;
    }

    void listen(event::Caller &caller)
    {
        caller.add(this, &Counter::count);
    }

    int calls=0;
};

//! Inspect the cursor Base::updateCursor sends to the platform (a no-op on Linux).
class CursorSwitch: public controls::Switch
{
public:
    static CursorSwitch *newInstance(const String &name) { return new CursorSwitch(name); }
    unsigned char currentCursor() const { return m_cursor; }

private:
    explicit CursorSwitch(const String &name): Switch(name) {}
};

struct Scene
{
    DrawRecorder renderer;
    SwitchSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    double clock=10.0;

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(300, 200);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        // the hovered control, the focus and the button state are process-global
        input::onMouseClicked(canvas, 0, false);
        input::onKeyEvent(canvas, keys::Space, false);
        input::onKeyEvent(canvas, keys::Return, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    controls::Switch *newSwitch(const char *text=nullptr, controls::Base *parent=nullptr)
    {
        controls::Switch *control=(parent?parent:canvas)->newChild<controls::Switch>();

        if(text)
            control->setText(text);
        control->setPos(10, 10+30*int(canvas->getChildren().size()));
        return control;
    }

    void frame()
    {
        renderer.draws.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    //! A whole left click at the control's local point, then a frame; a
    //! second on, so two clicks are never a double click.
    void click(controls::Base *control, Point local)
    {
        clock+=1.0;
        const Point at=control->localPosToCanvas(local);

        canvas->inputMouseMoved(at.x, at.y, 0, 0);
        canvas->inputMouseButton(0, true);
        canvas->inputMouseButton(0, false);
        frame();
    }

    //! A click on the track.
    void click(controls::Switch *control)
    {
        const Rect track=control->getTrackRect();

        click(control, Point(track.x+track.w/2, track.y+track.h/2));
    }

    //! The key down and up, then a frame.
    void key(int key)
    {
        input::onKeyEvent(canvas, key, true);
        input::onKeyEvent(canvas, key, false);
        frame();
    }

    //! Lets the slide end, a frame at a time.
    void settle()
    {
        for(int i=0; i < 30; ++i)
        {
            clock+=1.0/60.0;
            frame();
        }
    }

    static size_t id(controls::Base *control, size_t slot)
    {
        return control->getSkinControl()->getPrimitives().at(slot);
    }

    //! The last rect drawn into the control's thumb slot this frame, if any.
    const DrawRecorder::Draw *thumbDraw(controls::Base *control) const
    {
        const DrawRecorder::Draw *found=nullptr;

        for(const DrawRecorder::Draw &draw : renderer.draws)
        {
            if(draw.id == id(control, 1))
                found=&draw;
        }
        return found;
    }
};

//! The thumb's left edge in units, at \p position along its travel.
int thumbX(const controls::Switch *control, float position)
{
    const int diameter=controls::Switch::TrackHeight-2*controls::Switch::ThumbInset;
    const int travel=controls::Switch::TrackWidth-2*controls::Switch::ThumbInset-diameter;

    return control->getTrackRect().x+controls::Switch::ThumbInset
        +int(std::floor(position*float(travel)+0.5f));
}

//! The half-cosine ease.
float eased(double t)
{
    return float(0.5-0.5*std::cos(3.14159265358979323846*t));
}

} //namespace

TEST_CASE("Switch: a click, Space and Return toggle it, each firing onToggled once")
{
    Scene scene;
    controls::Switch *control=scene.newSwitch();
    Counter toggled, on, off, buttonToggle;

    toggled.listen(control->onToggled);
    on.listen(control->onToggledOn);
    off.listen(control->onToggledOff);
    buttonToggle.listen(control->onToggleCaller);
    scene.frame();
    CHECK_FALSE(control->isOn());

    scene.click(control);
    CHECK(control->isOn());
    CHECK(toggled.calls == 1);
    CHECK(on.calls == 1);
    CHECK(off.calls == 0);
    CHECK(buttonToggle.calls == 1);
    CHECK(KeyboardFocus == control);

    // the press alone does nothing: a key held repeats its press
    input::onKeyEvent(scene.canvas, keys::Space, true);
    CHECK(control->isOn());
    CHECK(toggled.calls == 1);
    input::onKeyEvent(scene.canvas, keys::Space, false);
    scene.frame();
    CHECK_FALSE(control->isOn());
    CHECK(toggled.calls == 2);
    CHECK(off.calls == 1);

    scene.key(keys::Return);
    CHECK(control->isOn());
    CHECK(toggled.calls == 3);
    CHECK(on.calls == 2);
    CHECK(buttonToggle.calls == 3);

    control->toggle();
    CHECK_FALSE(control->isOn());
    CHECK(toggled.calls == 4);
    CHECK(off.calls == 2);
}

TEST_CASE("Switch: a click on its label toggles it, on either side")
{
    Scene scene;
    controls::Switch *right=scene.newSwitch("Label on the right");
    controls::Switch *left=scene.newSwitch("Label on the left");

    left->setLabelOnLeft(true);
    scene.frame();

    // the track is at the label's other side, and the label beside it
    CHECK(right->getTrackRect().x == 0);
    CHECK(left->getTrackRect().x == left->getWidth()-controls::Switch::TrackWidth);
    CHECK(right->getWidth() > controls::Switch::TrackWidth+controls::Switch::Gap);

    scene.click(right, Point(right->getWidth()-3, right->getHeight()/2));
    CHECK(right->isOn());
    scene.click(left, Point(3, left->getHeight()/2));
    CHECK(left->isOn());
}

TEST_CASE("Switch: setOn with the value it has fires and marks nothing")
{
    Scene scene;
    controls::Switch *control=scene.newSwitch("Label");
    Counter toggled;

    toggled.listen(control->onToggled);
    scene.frame();

    control->setOn(false);
    CHECK(toggled.calls == 0);
    CHECK(control->getStateChange() == controls::StateChange_Nothing);

    control->setOn(true);
    CHECK(toggled.calls == 1);
    scene.settle();
    CHECK(control->getStateChange() == controls::StateChange_Nothing);

    control->setOn(true);
    CHECK(toggled.calls == 1);
    CHECK(control->getStateChange() == controls::StateChange_Nothing);
    CHECK_FALSE(control->isSliding());
}

TEST_CASE("Switch: onToggled reversing the state emits no stale On or Off")
{
    for(bool original : {false, true})
    {
        CAPTURE(original);
        Scene scene;
        controls::Switch *control=scene.newSwitch();
        Counter toggled, on, off, reverse;

        control->setOn(original);
        toggled.listen(control->onToggled);
        on.listen(control->onToggledOn);
        off.listen(control->onToggledOff);
        control->onToggled.addCallback(&reverse, [control, original](event::Handler &, event::Info) {
            if(control->isOn() != original)
                control->setOn(original);
        });
        scene.frame();

        scene.click(control);
        CHECK(control->isOn() == original);
        CHECK(toggled.calls == 2);
        CHECK(on.calls == (original?1:0));
        CHECK(off.calls == (original?0:1));
    }
}

TEST_CASE("Switch: Button toggle callers reversing the state stop stale Switch notifications")
{
    for(bool original : {false, true})
    {
        CAPTURE(original);
        Scene scene;
        controls::Switch *control=scene.newSwitch();
        Counter toggled, on, off, reverse;

        control->setOn(original);
        toggled.listen(control->onToggled);
        on.listen(control->onToggledOn);
        off.listen(control->onToggledOff);
        control->onToggleCaller.addCallback(&reverse, [control, original](event::Handler &, event::Info) {
            if(control->isOn() != original)
                control->setOn(original);
        });
        scene.frame();

        scene.click(control);
        CHECK(control->isOn() == original);
        CHECK(toggled.calls == 1);
        CHECK(on.calls == (original?1:0));
        CHECK(off.calls == (original?0:1));
    }
}

TEST_CASE("Switch: disabled, a click, Space and Return toggle nothing")
{
    Scene scene;
    controls::Switch *off=scene.newSwitch("Off");
    controls::Switch *on=scene.newSwitch("On");
    Counter toggled;

    on->setOn(true);
    off->setDisabled(true);
    on->setDisabled(true);
    toggled.listen(off->onToggled);
    toggled.listen(on->onToggled);
    scene.frame();

    scene.click(off);
    scene.click(on, Point(on->getWidth()-3, on->getHeight()/2));
    CHECK_FALSE(off->isOn());
    CHECK(on->isOn());
    CHECK(KeyboardFocus != off);
    CHECK(KeyboardFocus != on);

    // given the focus anyway, its keys do nothing either
    KeyboardFocus=off;
    scene.key(keys::Space);
    // The Space frame drops focus because keyboard input is disabled.
    KeyboardFocus=off;
    scene.key(keys::Return);
    CHECK_FALSE(off->isOn());
    CHECK(toggled.calls == 0);
    CHECK_FALSE(off->isSliding());
}

TEST_CASE("Switch: disabling restores the normal cursor and enabling restores the hand")
{
    Scene scene;
    CursorSwitch *control=scene.canvas->newChild<CursorSwitch>();

    CHECK(control->currentCursor() == cursor::Finger);
    control->setDisabled(true);
    CHECK(control->currentCursor() == cursor::Normal);
    control->setDisabled(false);
    CHECK(control->currentCursor() == cursor::Finger);
}

TEST_CASE("Switch: the thumb slides, eased, while its animation is registered, and stops at its end")
{
    Scene scene;
    controls::Switch *control=scene.newSwitch();

    scene.frame();
    CHECK(anim::activeCount(control) == 0);
    REQUIRE(scene.thumbDraw(control));
    CHECK(scene.thumbDraw(control)->rect.x-control->getRenderBounds().x == thumbX(control, 0.0f));
    CHECK(scene.thumbDraw(control)->z == control->getZIndex()+1);

    const double start=scene.clock;
    const double length=controls::Switch::SlideSeconds;

    control->setOn(true);
    CHECK(control->isSliding());
    CHECK(anim::activeCount(control) == 1);
    CHECK(control->getThumbPosition() == 0.0f);

    for(double t : {0.25, 0.5, 0.75})
    {
        scene.clock=start+t*length;
        scene.frame();
        CHECK(control->isSliding());
        CHECK(anim::activeCount(control) == 1);
        CHECK(control->getThumbPosition() == doctest::Approx(eased(t)).epsilon(1e-4));
        CHECK(control->getThumbRect().x == thumbX(control, eased(t)));
        REQUIRE(scene.thumbDraw(control));
        CHECK(scene.thumbDraw(control)->rect.x == control->getRenderBounds().x+thumbX(control, eased(t)));
    }

    // a step of less than a unit marks nothing
    scene.clock+=1e-5;
    scene.canvas->updateCanvas();
    CHECK(control->getStateChange() == controls::StateChange_Nothing);
    scene.canvas->renderCanvas();

    scene.clock=start+length;
    scene.frame();
    CHECK(control->getThumbPosition() == 1.0f);
    CHECK(control->getThumbRect().x == thumbX(control, 1.0f));
    CHECK_FALSE(control->isSliding());
    CHECK(anim::activeCount(control) == 0);

    // settled: nothing marked, nothing registered
    scene.clock+=1.0;
    scene.canvas->updateCanvas();
    CHECK(control->getStateChange() == controls::StateChange_Nothing);
    scene.canvas->renderCanvas();
    CHECK(anim::activeCount(control) == 0);
}

TEST_CASE("Switch: a toggle mid-slide turns the thumb round from where it is")
{
    Scene scene;
    controls::Switch *control=scene.newSwitch();
    const double length=controls::Switch::SlideSeconds;

    scene.frame();
    control->setOn(true);
    scene.clock+=length/2;
    scene.frame();
    CHECK(control->getThumbPosition() == doctest::Approx(0.5f));

    // back from 0.5, over the half of the travel left: half the time
    const double turn=scene.clock;

    control->setOn(false);
    CHECK(control->isSliding());
    CHECK(anim::activeCount(control) == 1);
    scene.frame();
    CHECK(control->getThumbPosition() == doctest::Approx(0.5f));

    scene.clock=turn+length/4;
    scene.frame();
    CHECK(control->getThumbPosition() == doctest::Approx(0.5f-0.5f*eased(0.5)).epsilon(1e-4));

    // (the float 0.5 it turned at, a hair either side of it)
    scene.clock=turn+length/2+1e-6;
    scene.frame();
    CHECK(control->getThumbPosition() == 0.0f);
    CHECK_FALSE(control->isSliding());
    CHECK(anim::activeCount(control) == 0);
}

TEST_CASE("Switch: hidden mid-slide, it jumps to its end and cancels its animation")
{
    Scene scene;
    controls::Base *page=scene.canvas->newChild<controls::Base>();

    page->setBounds(0, 0, 300, 200);

    controls::Switch *control=scene.newSwitch(nullptr, page);

    scene.frame();
    control->setOn(true);
    scene.clock+=controls::Switch::SlideSeconds/4;
    scene.frame();
    CHECK(control->isSliding());
    CHECK(control->getThumbPosition() > 0.0f);
    CHECK(control->getThumbPosition() < 1.0f);

    page->hide();
    scene.clock+=1.0/60.0;
    scene.frame();
    CHECK(control->getThumbPosition() == 1.0f);
    CHECK_FALSE(control->isSliding());
    CHECK(anim::activeCount(control) == 0);

    // toggled while hidden, it jumps
    control->setOn(false);
    CHECK(control->getThumbPosition() == 0.0f);
    CHECK_FALSE(control->isSliding());
    CHECK(anim::activeCount(control) == 0);

    page->show();
    scene.frame();
    CHECK(control->getThumbRect().x == thumbX(control, 0.0f));
    CHECK(anim::activeCount(control) == 0);
}

TEST_CASE("Switch: set before its first frame, the thumb is at its end with no animation")
{
    Scene scene;
    controls::Switch *control=scene.newSwitch("Label");

    control->setOn(true);
    CHECK(control->getThumbPosition() == 1.0f);
    CHECK_FALSE(control->isSliding());
    CHECK(anim::activeCount(control) == 0);

    scene.frame();
    REQUIRE(scene.thumbDraw(control));
    CHECK(scene.thumbDraw(control)->rect.x == control->getRenderBounds().x+thumbX(control, 1.0f));
}

TEST_CASE("Switch: set in a container built before the first frame, the thumb is at its end")
{
    Scene scene;
    controls::Base *column=scene.canvas->newChild<controls::Base>();
    controls::Base *row=column->newChild<controls::Base>();
    controls::Switch *control=row->newChild<controls::Switch>();

    column->setBounds(0, 0, 300, 200);
    row->setBounds(0, 0, 300, 40);
    control->setText("Label");
    control->setOn(true);
    CHECK(control->getThumbPosition() == 1.0f);
    CHECK_FALSE(control->isSliding());

    scene.frame();
    CHECK(anim::activeCount(control) == 0);
}

TEST_CASE("Switch: a toggle and its slide invalidate no layout")
{
    Scene scene;
    controls::Switch *control=scene.newSwitch("Label");

    scene.frame();
    scene.frame();

    const size_t before=controls::invalidateCalls();

    scene.click(control);
    CHECK(control->isSliding());
    scene.settle();
    CHECK(control->isOn());
    CHECK_FALSE(control->isSliding());
    scene.key(keys::Space);
    scene.settle();
    CHECK_FALSE(control->isOn());
    CHECK(controls::invalidateCalls() == before);
}
