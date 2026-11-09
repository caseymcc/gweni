/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The profiling clock and the frame times the library records on it.
 *  platform::profileTimeMicros() is steady_clock by default (it never goes
 *  back), reads an installed platform::setProfileClock() instead, and an empty
 *  one gives steady_clock back. Under a clock that advances a fixed step per
 *  read, as a capture installs, the times are fixed: updateCanvas() and
 *  renderCanvas() each read it twice, so each takes one step, the renderer's
 *  end() inside renderCanvas() is what the renderer reports as flushMicros,
 *  previousFrameStats().times is the frame before (zeros before the second
 *  frame) and the interval between two frames' startMicros is every read
 *  between them. SlotRenderer::flush() reads it twice and reports one step in
 *  getFrameStats() and frameStats(), which is what the canvas takes.
 *  Timing a frame moves none of its counters.
 */
#include "headlessFixture.h"
#include "mirrorOnlyRenderer.h"

#include <gweni/gweni.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <cstdint>

using namespace gweni;

namespace
{

//! A profile clock that advances `step` per read, from 0, while it lives.
struct StepClock
{
    explicit StepClock(uint64_t stepMicros): step(stepMicros)
    {
        platform::setProfileClock([this]() { return (++reads)*step; });
    }

    ~StepClock() { platform::setProfileClock(nullptr); }

    uint64_t step;
    uint64_t reads=0;
};

//! A stub renderer whose end() is a flush timed as SlotRenderer's is: two
//! reads of the profile clock, the difference reported as flushMicros.
class TimedStub: public test::StubRenderer
{
public:
    void end() override
    {
        const uint64_t start=platform::profileTimeMicros();

        StubRenderer::end();
        m_flushMicros=platform::profileTimeMicros()-start;
    }

    renderer::FrameCounts frameStats() const override
    {
        renderer::FrameCounts counts=StubRenderer::frameStats();

        counts.flushMicros=m_flushMicros;
        return counts;
    }

private:
    uint64_t m_flushMicros=0;
};

//! A canvas holding a few controls on a skin over `renderer`.
template<typename Renderer>
struct Scene
{
    Renderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
        for(int i=0; i<3; ++i)
            canvas->newChild<controls::Base>()->setBounds(i*10, i*10, 50, 20);
    }

    ~Scene() { delete canvas; }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }
};

bool sameCounts(const controls::CanvasFrameStats &a, const controls::CanvasFrameStats &b)
{
    return a.layout.passes == b.layout.passes && a.layout.visited == b.layout.visited
        && a.update.zVisited == b.update.zVisited && a.update.zChanged == b.update.zChanged
        && a.update.animsRun == b.update.animsRun && a.render.visited == b.render.visited
        && a.render.controls == b.render.controls && a.render.geometry == b.render.geometry;
}

}//namespace

TEST_CASE("platform::profileTimeMicros: steady_clock by default, an installed clock instead")
{
    const uint64_t first=platform::profileTimeMicros();
    const uint64_t second=platform::profileTimeMicros();

    CHECK(second >= first);

    {
        StepClock clock(25);

        CHECK(platform::profileTimeMicros() == 25);
        CHECK(platform::profileTimeMicros() == 50);
        CHECK(clock.reads == 2);
    }

    // the empty clock it was reset to is steady_clock again
    CHECK(platform::profileTimeMicros() >= second);
}

TEST_CASE("CanvasFrameStats::times: each phase one step under a fixed clock, the flush the renderer's")
{
    SUBCASE("a renderer that does not time its end()")
    {
        Scene<test::StubRenderer> scene;
        StepClock clock(100);

        scene.frame();
        // before the second frame: zeros
        CHECK(scene.canvas->previousFrameStats().times.startMicros == 0);
        CHECK(scene.canvas->previousFrameStats().times.renderMicros == 0);

        scene.frame();

        const controls::FrameTimes times=scene.canvas->previousFrameStats().times;

        // the first frame's reads: 1 (start) and 2 (update's end), 3 and 4 (render)
        CHECK(times.startMicros == 100);
        CHECK(times.updateMicros == 100);
        CHECK(times.renderMicros == 100);
        CHECK(times.flushMicros == 0);
        CHECK(clock.reads == 8);

        // the next frame started at the fifth read: every read between two starts
        scene.frame();
        CHECK(scene.canvas->previousFrameStats().times.startMicros-times.startMicros == 400);
    }

    SUBCASE("a renderer that times its end(): the flush is inside the render phase")
    {
        Scene<TimedStub> scene;
        StepClock clock(100);

        scene.frame();
        scene.frame();

        const controls::FrameTimes times=scene.canvas->previousFrameStats().times;

        CHECK(times.startMicros == 100);
        CHECK(times.updateMicros == 100);
        CHECK(times.renderMicros == 300);
        CHECK(times.flushMicros == 100);
        // the second frame started after the first's six reads
        scene.frame();
        CHECK(scene.canvas->previousFrameStats().times.startMicros == 700);
    }
}

TEST_CASE("CanvasFrameStats::times: timing a frame moves none of its counters")
{
    Scene<test::StubRenderer> timed;
    Scene<test::StubRenderer> untimed;

    for(int frame=0; frame<4; ++frame)
    {
        {
            StepClock clock(100);
            timed.frame();
        }
        untimed.frame();
        CAPTURE(frame);
        CHECK(sameCounts(timed.canvas->previousFrameStats(), untimed.canvas->previousFrameStats()));
    }
}

#ifdef GWENI_TEST_SLOT_RENDERER
TEST_CASE("SlotRenderer::flush() times itself on the profile clock")
{
    test::MirrorOnlyRenderer renderer;
    StepClock clock(7);

    renderer.begin();
    renderer.end();
    CHECK(renderer.getFrameStats().flushMicros == 7);
    CHECK(renderer.frameStats().flushMicros == 7);
    CHECK(clock.reads == 2);
}
#endif//GWENI_TEST_SLOT_RENDERER
