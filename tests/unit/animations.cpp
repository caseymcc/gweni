/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The animation registry (anim::add/cancel/think, source/anim.cpp) holds only
 *  active animations, and each canvas's update phase runs only its own
 *  controls' ones, in the order they were added, counted by
 *  UpdateStats::animsRun.
 *
 *  With two canvases each animation runs once a frame, during the
 *  updateCanvas() of its control's canvas, so a size animation on the canvas
 *  updated first is laid out in that canvas's own update phase. A control
 *  moved to the other canvas animates there; one on no canvas, or on a hidden
 *  canvas, does not run. anim::cancel(control, animation) removes that one
 *  alone. A finished animation leaves the registry.
 *
 *  Timed animations under a stepped platform::setTimeSource clock: onStart()
 *  when one starts (after its delay) and onFinish() once at the end;
 *  anim_widthIn/heightIn grow from 0, anim_widthOut/heightOut hide only on
 *  their last frame, pos::X/Y move, and tools::Remove deletes its control,
 *  also on a frozen clock.
 *
 *  An animation may, from its think(), cancel itself, all of its control's,
 *  or another control's animation, add one (which first runs next frame),
 *  delayedDelete its control or delete another control outright; none of it
 *  touches freed memory (the ASAN tree checks).
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/anim.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <functional>
#include <string>
#include <vector>

using namespace gweni;

namespace
{

//! The canvas whose updateCanvas() is running, set by Scene::update().
controls::Canvas *g_updating=nullptr;

//! What the probes did this case, in order: "name@canvas" per run.
std::vector<std::string> g_log;

//! An animation that logs each run and the canvas it ran in, then runs body.
class Probe: public anim::Animation
{
public:
    Probe(std::string name, std::function<void(Probe &)> body={}):
        m_name(std::move(name)), m_body(std::move(body))
    {}

    void think() override
    {
        // the canvas whose update phase runs it is its own control's
        CHECK((m_control->getCanvas() == g_updating));
        g_log.push_back(m_name+"@"+canvasName(g_updating));

        if(m_body)
            m_body(*this);
    }

    static std::string canvasName(controls::Canvas *canvas)
    {
        return canvas ? canvas->getName() : "none";
    }

private:
    std::string m_name;
    std::function<void(Probe &)> m_body;
};

//! A TimedAnimation that logs its hooks.
class Recorder: public anim::TimedAnimation
{
public:
    Recorder(std::vector<std::string> &log, float length, float delay):
        TimedAnimation(length, delay, 1.0f), m_log(log)
    {}

    void onStart() override { m_log.push_back("start"); }
    void run(float delta) override { m_log.push_back("run "+std::to_string(delta)); }
    void onFinish() override { m_log.push_back("finish"); }

private:
    std::vector<std::string> &m_log;
};

//! A control that says when it is deleted.
class Tracked: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Tracked, controls::Base)
    {}

    ~Tracked() override
    {
        if(m_gone)
            *m_gone=true;
    }

    bool *m_gone=nullptr;
};

//! Two canvases, "a" and "b", on one stub skin; a frame updates b first.
struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *a;
    controls::Canvas *b;
    float clock=10.0f;
    size_t baseline;

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        g_log.clear();
        baseline=anim::activeCount();

        a=new controls::Canvas(&skin);
        a->setName("a");
        a->setSize(400, 300);
        b=new controls::Canvas(&skin);
        b->setName("b");
        b->setSize(400, 300);
        frame();
    }

    ~Scene()
    {
        delete a;
        delete b;
        // every animation of a deleted control went with it
        CHECK(anim::activeCount() == baseline);
        platform::setTimeSource(nullptr);
    }

    static void update(controls::Canvas *canvas)
    {
        g_updating=canvas;
        canvas->updateCanvas();
        g_updating=nullptr;
    }

    void frame()
    {
        update(b);
        update(a);
        b->renderCanvas();
        a->renderCanvas();
    }

    //! one frame's log, the log cleared first
    std::vector<std::string> logged()
    {
        g_log.clear();
        frame();
        return g_log;
    }

    controls::Base *child(controls::Base *parent, const char *name)
    {
        controls::Base *control=parent->newChild<controls::Base>(name);
        control->setBounds(10, 10, 100, 40);
        return control;
    }
};

using Log=std::vector<std::string>;

}//namespace

TEST_CASE("an animation runs once a frame, in its own canvas's update phase")
{
    Scene scene;

    anim::add(scene.child(scene.a, "x"), new Probe("x"));
    anim::add(scene.child(scene.b, "y"), new Probe("y"));

    for(int i=0; i<3; ++i)
    {
        CAPTURE(i);
        // b updates first
        CHECK(scene.logged() == Log{"y@b", "x@a"});
        CHECK(scene.a->getUpdateStats().animsRun == 1);
        CHECK(scene.b->getUpdateStats().animsRun == 1);
    }
}

TEST_CASE("nothing registered runs nothing")
{
    Scene scene;

    scene.child(scene.a, "x");
    CHECK(scene.logged().empty());
    CHECK(scene.a->getUpdateStats().animsRun == 0);
    CHECK(scene.b->getUpdateStats().animsRun == 0);
}

TEST_CASE("a size animation on the canvas updated first is laid out in its own update phase")
{
    Scene scene;

    controls::Base *panel=scene.child(scene.b, "panel");
    controls::Base *fill=panel->newChild<controls::Base>("fill");
    fill->setDock(DockPosition::Center);
    scene.frame();
    panel->anim_widthIn(1.0f);

    for(int i=0; i<=4; ++i)
    {
        CAPTURE(i);
        Scene::update(scene.b);
        // set and laid out before b's update phase ended
        CHECK(panel->getWidth() == 25*i);
        CHECK(fill->getWidth() == panel->getWidth());
        Scene::update(scene.a);
        scene.b->renderCanvas();
        CHECK_FALSE(scene.b->getLayoutStats().renderLayout);
        scene.a->renderCanvas();
        scene.clock+=0.25f;
    }

    CHECK(anim::activeCount(panel) == 0);
}

TEST_CASE("a control moved to another canvas animates in that one only")
{
    Scene scene;

    controls::Base *control=scene.child(scene.a, "x");
    anim::add(control, new Probe("x"));
    CHECK(scene.logged() == Log{"x@a"});

    control->setParent(scene.child(scene.b, "holder"));
    CHECK(scene.logged() == Log{"x@b"});
    CHECK(scene.a->getUpdateStats().animsRun == 0);
    CHECK(scene.b->getUpdateStats().animsRun == 1);
}

TEST_CASE("a control on no canvas does not run, and takes its animations when deleted")
{
    Scene scene;

    controls::Base *loose=scene.child(scene.a, "loose");
    loose->setParent(nullptr);
    anim::add(loose, new Probe("loose"));
    CHECK(anim::activeCount(loose) == 1);
    CHECK(scene.logged().empty());

    loose->setParent(scene.a);
    CHECK(scene.logged() == Log{"loose@a"});

    loose->setParent(nullptr);
    CHECK(scene.logged().empty());

    delete loose;
    CHECK(anim::activeCount() == scene.baseline);
}

TEST_CASE("a hidden canvas's animations do not run, in its own update or another's")
{
    Scene scene;

    anim::add(scene.child(scene.a, "x"), new Probe("x"));
    anim::add(scene.child(scene.b, "y"), new Probe("y"));

    scene.a->setHidden(true);
    CHECK(scene.logged() == Log{"y@b"});
    CHECK(scene.a->getUpdateStats().animsRun == 0);

    scene.a->setHidden(false);
    CHECK(scene.logged() == Log{"y@b", "x@a"});
}

TEST_CASE("animations run in the order they were added")
{
    Scene scene;

    controls::Base *c0=scene.child(scene.a, "c0");
    controls::Base *c1=scene.child(scene.a, "c1");
    controls::Base *c2=scene.child(scene.a, "c2");
    anim::add(c2, new Probe("c2"));
    anim::add(c0, new Probe("c0"));
    anim::add(c1, new Probe("c1"));
    anim::add(c2, new Probe("c2'"));

    CHECK(scene.logged() == Log{"c2@a", "c0@a", "c1@a", "c2'@a"});
    CHECK(scene.a->getUpdateStats().animsRun == 4);
    CHECK(anim::activeCount(c2) == 2);
}

TEST_CASE("cancelling one animation of a control leaves its others running")
{
    Scene scene;

    controls::Base *control=scene.child(scene.a, "x");
    Probe *first=new Probe("first");
    anim::add(control, first);
    anim::add(control, new Probe("second"));
    CHECK(scene.logged() == Log{"first@a", "second@a"});

    anim::cancel(control, first);
    CHECK(anim::activeCount(control) == 1);
    CHECK(scene.logged() == Log{"second@a"});
    // already gone: compared, not touched
    anim::cancel(control, first);
    CHECK(anim::activeCount(control) == 1);

    anim::cancel(control);
    CHECK(anim::activeCount(control) == 0);
    CHECK(scene.logged().empty());
}

TEST_CASE("a timed animation starts after its delay, finishes once and leaves the registry")
{
    Scene scene;

    controls::Base *control=scene.child(scene.a, "x");
    Log log;
    anim::add(control, new Recorder(log, 1.0f, 0.5f));

    // before its delay it runs nothing of its own
    scene.frame();
    CHECK(log.empty());
    CHECK(scene.a->getUpdateStats().animsRun == 1);

    scene.clock+=0.5f;
    scene.frame();
    CHECK(log == Log{"start", "run 0.000000"});
    scene.clock+=0.5f;
    scene.frame();
    CHECK(log == Log{"start", "run 0.000000", "run 0.500000"});
    scene.clock+=0.5f;
    scene.frame();
    CHECK(log == Log{"start", "run 0.000000", "run 0.500000", "run 1.000000", "finish"});
    CHECK(anim::activeCount(control) == 0);

    scene.clock+=0.5f;
    scene.frame();
    CHECK(log.size() == 5);
    CHECK(scene.a->getUpdateStats().animsRun == 0);
}

TEST_CASE("anim_widthIn and anim_heightIn grow from 0 to the size")
{
    Scene scene;

    controls::Base *wide=scene.child(scene.a, "wide");
    controls::Base *tall=scene.child(scene.a, "tall");
    wide->anim_widthIn(1.0f);
    tall->anim_heightIn(1.0f);

    for(int i=0; i<=2; ++i)
    {
        CAPTURE(i);
        scene.frame();
        CHECK(wide->getWidth() == 50*i);
        CHECK(tall->getHeight() == 20*i);
        CHECK_FALSE(wide->hidden());
        CHECK_FALSE(tall->hidden());
        scene.clock+=0.5f;
    }

    CHECK(anim::activeCount() == scene.baseline);
}

TEST_CASE("anim_widthOut and anim_heightOut hide the control only on their last frame")
{
    Scene scene;

    controls::Base *wide=scene.child(scene.a, "wide");
    controls::Base *tall=scene.child(scene.a, "tall");
    controls::Base *kept=scene.child(scene.a, "kept");
    wide->anim_widthOut(1.0f);
    tall->anim_heightOut(1.0f, true);
    kept->anim_heightOut(1.0f, false);

    for(int i=0; i<=2; ++i)
    {
        CAPTURE(i);
        scene.frame();
        CHECK(wide->getWidth() == 100-50*i);
        CHECK(tall->getHeight() == 40-20*i);
        CHECK(kept->getHeight() == 40-20*i);
        CHECK(wide->hidden() == (i == 2));
        CHECK(tall->hidden() == (i == 2));
        CHECK_FALSE(kept->hidden());
        scene.clock+=0.5f;
    }

    CHECK(anim::activeCount() == scene.baseline);
}

TEST_CASE("pos::X and pos::Y move the control from its start to its end")
{
    Scene scene;

    controls::Base *across=scene.child(scene.a, "across");
    controls::Base *down=scene.child(scene.a, "down");
    anim::add(across, new anim::pos::X(0, 100, 1.0f));
    anim::add(down, new anim::pos::Y(0, 200, 1.0f));

    for(int i=0; i<=2; ++i)
    {
        CAPTURE(i);
        scene.frame();
        CHECK(across->getPos().x == 50*i);
        CHECK(across->getPos().y == 10);
        CHECK(down->getPos().x == 10);
        CHECK(down->getPos().y == 100*i);
        scene.clock+=0.5f;
    }

    CHECK(anim::activeCount() == scene.baseline);
}

TEST_CASE("tools::Remove deletes its control after its delay, also on a frozen clock")
{
    Scene scene;

    bool lateGone=false, nowGone=false;
    Tracked *late=scene.a->newChild<Tracked>("late");
    late->m_gone=&lateGone;
    anim::add(late, new anim::tools::Remove(0.5f));
    Tracked *now=scene.a->newChild<Tracked>("now");
    now->m_gone=&nowGone;
    anim::add(now, new anim::tools::Remove());

    // the clock does not move: a zero length is over as soon as it starts
    scene.frame();
    CHECK(nowGone);
    CHECK_FALSE(lateGone);

    scene.clock+=0.5f;
    scene.frame();
    CHECK(lateGone);
    CHECK(anim::activeCount() == scene.baseline);
}

TEST_CASE("an animation may cancel itself from its think()")
{
    Scene scene;

    controls::Base *control=scene.child(scene.a, "x");
    anim::add(control, new Probe("self", [control](Probe &probe) {
        anim::cancel(control, &probe);
    }));
    anim::add(control, new Probe("other"));

    CHECK(scene.logged() == Log{"self@a", "other@a"});
    CHECK(anim::activeCount(control) == 1);
    CHECK(scene.logged() == Log{"other@a"});
}

TEST_CASE("an animation may cancel all of its control's from its think()")
{
    Scene scene;

    controls::Base *control=scene.child(scene.a, "x");
    anim::add(control, new Probe("all", [control](Probe &) { anim::cancel(control); }));
    anim::add(control, new Probe("next"));

    // the one after it is cancelled before its turn
    CHECK(scene.logged() == Log{"all@a"});
    CHECK(anim::activeCount(control) == 0);
    CHECK(scene.logged().empty());
}

TEST_CASE("an animation may cancel another control's from its think()")
{
    Scene scene;

    controls::Base *c0=scene.child(scene.a, "c0");
    controls::Base *c1=scene.child(scene.b, "c1");
    controls::Base *c2=scene.child(scene.b, "c2");
    Probe *victim=new Probe("victim");
    Probe *earlier=new Probe("earlier");
    anim::add(c1, earlier);
    anim::add(c0, new Probe("killer", [c1, c2, victim, earlier](Probe &) {
        anim::cancel(c2, victim);
        anim::cancel(c1, earlier);
    }));
    anim::add(c2, victim);

    // b updates first: both of b's run before a's killer does
    CHECK(scene.logged() == Log{"earlier@b", "victim@b", "killer@a"});
    CHECK(anim::activeCount(c1) == 0);
    CHECK(anim::activeCount(c2) == 0);
    CHECK(scene.logged() == Log{"killer@a"});
}

TEST_CASE("an animation added from inside a think() first runs on the next frame")
{
    Scene scene;

    controls::Base *c0=scene.child(scene.a, "c0");
    controls::Base *c1=scene.child(scene.a, "c1");
    bool added=false;
    anim::add(c0, new Probe("adder", [c0, c1, &added](Probe &) {
        if(added)
            return;
        added=true;
        // enough to reallocate the registry under the loop
        for(int i=0; i<64; ++i)
            anim::add(i == 0 ? c1 : c0, new Probe(i == 0 ? "new" : "filler"));
    }));

    CHECK(scene.logged() == Log{"adder@a"});
    CHECK(scene.a->getUpdateStats().animsRun == 1);

    const Log next=scene.logged();
    REQUIRE(next.size() == 65);
    CHECK(next[0] == "adder@a");
    CHECK(next[1] == "new@a");
    anim::cancel(c0);
    anim::cancel(c1);
}

TEST_CASE("an animation may delayedDelete its control from its think()")
{
    Scene scene;

    bool gone=false;
    Tracked *control=scene.a->newChild<Tracked>("x");
    control->m_gone=&gone;
    anim::add(control, new Probe("doomed", [control](Probe &) { control->delayedDelete(); }));
    anim::add(control, new Probe("sibling"));

    CHECK(scene.logged() == Log{"doomed@a", "sibling@a"});
    CHECK(gone);
    CHECK(anim::activeCount() == scene.baseline);
    CHECK(scene.logged().empty());
}

TEST_CASE("an animation may delete another control outright from its think()")
{
    Scene scene;

    controls::Base *c0=scene.child(scene.a, "c0");
    bool gone=false;
    Tracked *c1=scene.a->newChild<Tracked>("c1");
    c1->m_gone=&gone;
    anim::add(c0, new Probe("killer", [c1](Probe &) { delete c1; }));
    anim::add(c1, new Probe("victim"));
    c1->anim_widthIn(1.0f);

    // c1's animations went with it, before their turn
    CHECK(scene.logged() == Log{"killer@a"});
    CHECK(gone);
    CHECK(anim::activeCount() == scene.baseline+1);
    anim::cancel(c0);
}

TEST_CASE("deleting a control in the middle of a timed animation is safe")
{
    Scene scene;

    controls::Base *control=scene.child(scene.a, "x");
    control->anim_widthIn(1.0f);
    scene.clock+=0.5f;
    scene.frame();
    CHECK(control->getWidth() == 50);

    delete control;
    CHECK(anim::activeCount() == scene.baseline);
    scene.clock+=0.5f;
    CHECK(scene.logged().empty());
}
