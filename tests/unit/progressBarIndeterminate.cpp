/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  ProgressBar::setIndeterminate on a real Canvas with a driven clock, drawn by
 *  the textured skin's ProgressBar renderer on the StubRenderer, which records
 *  the rects it draws: the segment's phase advances with time and wraps, the
 *  segment slides in at the track's start and out at its end (bottom to top
 *  when vertical) and is always inside the track, a frame where it moves less
 *  than a unit marks and draws nothing, no text changes while it runs, a hidden
 *  bar stands still, and setIndeterminate(false) gives back the fill and label
 *  of the stored progress. A determinate cycling bar sets its auto label only
 *  when the whole percentage changes.
 *
 *  The segment is drawn only on a frame its rect changes, and
 *  setIndeterminate(false) draws the stored progress's fill and label once and
 *  then nothing.
 */
#include "headlessFixture.h"

#include <gweni/controls/progressBar.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedControls.h>

#include <doctest/doctest.h>

#include <vector>

using namespace gweni;

namespace
{

bool sameRect(const Rect &a, const Rect &b)
{
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

bool inside(const Rect &r, const Rect &track)
{
    return r.w >= 0 && r.h >= 0 && r.x >= track.x && r.y >= track.y
        && r.x+r.w <= track.x+track.w && r.y+r.h <= track.y+track.h;
}

//! Records the rects the ProgressBar renderer draws.
class DrawRecorder: public test::StubRenderer
{
public:
    struct Draw
    {
        size_t id;
        Rect rect;
    };

    void drawRoundedRect(size_t primitiveId, Rect rect, float, float, Color, Color, Color, int) override
    {
        draws.push_back({primitiveId, rect});
    }

    //! The draws into one slot this frame.
    std::vector<Draw> drawsInto(size_t id) const
    {
        std::vector<Draw> found;

        for(const Draw &draw : draws)
        {
            if(draw.id == id)
                found.push_back(draw);
        }
        return found;
    }

    std::vector<Draw> draws;
};

//! The textured skin's ProgressBar renderer, a StubControl for the rest.
class ProgressBarSkin: public test::StubSkin
{
public:
    explicit ProgressBarSkin(test::StubRenderer *renderer): StubSkin(renderer) {}

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == "ProgressBar")
            return std::make_unique<skin::textured::ProgressBar>();
        return StubSkin::getControlRenderer(typeName);
    }
};

struct Scene
{
    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(300, 300);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    controls::ProgressBar *bar(bool vertical)
    {
        controls::ProgressBar *pb=canvas->newChild<controls::ProgressBar>();

        pb->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        if(vertical)
        {
            pb->setVertical();
            pb->setBounds(10, 10, 20, 200);
        }
        else
            pb->setBounds(10, 10, 200, 20);
        return pb;
    }

    //! One frame, \p seconds after the last; the counts are this frame's.
    void frame(float seconds=0.0f)
    {
        clock+=seconds;
        renderer.draws.clear();
        controls::resetChangeStats();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    static size_t fillSlot(controls::Base *control)
    {
        return control->getSkinControl()->getPrimitives()[1];
    }

    DrawRecorder renderer;
    ProgressBarSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    float clock=10.0f;
};

}//namespace

TEST_CASE("ProgressBar: an indeterminate bar's phase advances with the clock and wraps")
{
    Scene scene;
    controls::ProgressBar *pb=scene.bar(false);

    // one cycle is the track and the segment: 1.3 track lengths at 0.65 a
    // second is a phase of 0.5 a second
    pb->setSegmentLength(0.3f);
    pb->setIndeterminateSpeed(0.65f);
    pb->setIndeterminate(true);
    // the animation's first step measures from 0, clamped to 0.3 s
    scene.frame();
    scene.frame();

    const float start=pb->getIndeterminatePhase();

    for(int i=1; i<=4; ++i)
    {
        scene.frame(0.25f);
        CHECK(pb->getIndeterminatePhase() == doctest::Approx(start+0.125f*i).epsilon(1e-4));
    }

    // another second and a half: past 1, it comes round
    for(int i=0; i<6; ++i)
    {
        scene.frame(0.25f);
        CHECK(pb->getIndeterminatePhase() >= 0.0f);
        CHECK(pb->getIndeterminatePhase() < 1.0f);
    }
    float expected=start+1.25f;
    expected-=(int)expected;
    CHECK(pb->getIndeterminatePhase() == doctest::Approx(expected).epsilon(1e-4));
}

TEST_CASE("ProgressBar: the segment slides in at the start, out at the end, and stays in the track")
{
    for(const bool vertical : {false, true})
    {
        CAPTURE(vertical);
        Scene scene;
        controls::ProgressBar *pb=scene.bar(vertical);

        pb->setIndeterminate(true);
        scene.frame();
        scene.frame();

        const Rect track=pb->getRenderBounds();
        const int extent=vertical ? track.h : track.w;
        bool sawEntering=false, sawWhole=false, sawLeaving=false;
        Rect last=pb->indeterminateSegment(track);

        // two and a half cycles in 1/60 s steps
        for(int i=0; i<360; ++i)
        {
            scene.frame(1.0f/60.0f);

            const Rect segment=pb->indeterminateSegment(track);
            const int length=vertical ? segment.h : segment.w;
            // the start edge is the left, or the bottom of a vertical bar
            const int from=vertical ? track.y+track.h-(segment.y+segment.h) : segment.x-track.x;

            REQUIRE(inside(segment, track));
            // vertical: always the track's width; horizontal: its height
            if(vertical)
                CHECK(segment.w == track.w);
            else
                CHECK(segment.h == track.h);

            if(length > 0 && from == 0 && length < (int)(0.3f*extent)-1)
                sawEntering=true;
            if(length >= (int)(0.3f*extent)-1 && from > 0 && from+length < extent)
                sawWhole=true;
            if(length > 0 && from+length == extent && from > (int)(0.7f*extent))
                sawLeaving=true;

            // what is drawn is that rect, and only on a frame it changes
            const std::vector<DrawRecorder::Draw> fill=scene.renderer.drawsInto(Scene::fillSlot(pb));

            if(sameRect(segment, last))
            {
                CHECK(fill.empty());
                CHECK(scene.renderer.draws.empty());
            }
            else
            {
                REQUIRE(fill.size() == 1);
                CHECK(sameRect(fill[0].rect, segment));
            }
            CHECK(controls::getChangeStats().text == 0);
            last=segment;
        }

        CHECK(sawEntering);
        CHECK(sawWhole);
        CHECK(sawLeaving);
    }
}

TEST_CASE("ProgressBar: a step of less than a unit marks and draws nothing")
{
    Scene scene;
    controls::ProgressBar *pb=scene.bar(false);

    pb->setIndeterminate(true);
    scene.frame();
    scene.frame();
    // bring the segment onto the track
    for(int i=0; i<30; ++i)
        scene.frame(1.0f/60.0f);

    // 0.01 track lengths a second is 2 units a second on a 200-unit track
    pb->setIndeterminateSpeed(0.01f);

    const Rect before=pb->indeterminateSegment(pb->getRenderBounds());
    size_t marked=0;

    for(int i=0; i<60; ++i)
    {
        scene.clock+=1.0f/60.0f;
        scene.renderer.draws.clear();
        controls::resetChangeStats();
        scene.canvas->updateCanvas();

        const bool changed=pb->getStateChange() != controls::StateChange_Nothing;

        if(changed)
        {
            CHECK(pb->getStateChange() == controls::StateChange_Background);
            ++marked;
        }
        scene.canvas->renderCanvas();
        // an unmoved segment: nothing entered, nothing drawn
        if(!changed)
        {
            CHECK(controls::getChangeStats().controls == 0);
            CHECK(scene.renderer.draws.empty());
        }
    }

    const Rect after=pb->indeterminateSegment(pb->getRenderBounds());
    // a second at 2 units a second moves each edge by 2 units (or 1 or 3, as
    // they round): a few marks, not 60
    CHECK(after.x > before.x);
    CHECK(marked >= 1);
    CHECK(marked <= 3);
}

TEST_CASE("ProgressBar: no text changes while indeterminate, a hidden bar stands still")
{
    Scene scene;
    controls::ProgressBar *pb=scene.bar(false);

    pb->setProgress(0.4f);
    scene.frame();
    REQUIRE(pb->getText() == "40%");

    pb->setIndeterminate(true);
    // the auto label is empty while it runs
    CHECK(pb->getText() == "");
    scene.frame();

    for(int i=0; i<30; ++i)
    {
        scene.frame(1.0f/60.0f);
        CHECK(controls::getChangeStats().text == 0);
        CHECK(pb->getText() == "");
    }

    // setProgress still stores its value, and draws nothing meanwhile
    pb->setProgress(0.6f);
    CHECK(pb->getStateChange() == controls::StateChange_Nothing);
    CHECK(pb->getText() == "");

    pb->hide();
    scene.frame();

    const float phase=pb->getIndeterminatePhase();

    for(int i=0; i<10; ++i)
    {
        scene.frame(1.0f/60.0f);
        CHECK(pb->getIndeterminatePhase() == phase);
        CHECK(controls::getChangeStats().controls == 0);
    }

    pb->show();
    scene.frame(1.0f/60.0f);
    scene.frame(1.0f/60.0f);
    CHECK(pb->getIndeterminatePhase() != phase);
}

TEST_CASE("ProgressBar: setIndeterminate(false) gives back the fill and label of the progress")
{
    for(const bool vertical : {false, true})
    {
        CAPTURE(vertical);
        Scene scene;
        controls::ProgressBar *pb=scene.bar(vertical);

        pb->setProgress(0.4f);
        pb->setIndeterminate(true);
        scene.frame();
        for(int i=0; i<20; ++i)
            scene.frame(1.0f/60.0f);
        pb->setProgress(0.25f);

        pb->setIndeterminate(false);
        CHECK(pb->getText() == "25%");
        CHECK((pb->getStateChange()&controls::StateChange_Background) != 0);
        scene.frame(1.0f/60.0f);

        const std::vector<DrawRecorder::Draw> fill=scene.renderer.drawsInto(Scene::fillSlot(pb));

        REQUIRE(fill.size() == 1);
        // the determinate fill: a quarter, from the left or from the bottom
        const Rect track=pb->getRenderBounds();

        if(vertical)
            CHECK(sameRect(fill[0].rect, Rect(track.x, track.y+150, 20, 50)));
        else
            CHECK(sameRect(fill[0].rect, Rect(track.x, track.y, 50, 20)));

        // and it is still: the next frames draw nothing
        for(int i=0; i<5; ++i)
        {
            scene.frame(1.0f/60.0f);
            CHECK(scene.renderer.draws.empty());
            CHECK(controls::getChangeStats().controls == 0);
        }
    }
}

TEST_CASE("ProgressBar: a cycling bar's auto label is set only on a new whole percentage")
{
    Scene scene;
    controls::ProgressBar *pb=scene.bar(false);

    pb->setProgress(0.5f);
    // 0.06 a second is a percent every 1/6 s: ten 1/60 s steps
    pb->setCycleSpeed(0.06f);
    scene.frame();
    scene.frame();

    size_t textFrames=0;

    for(int i=0; i<60; ++i)
    {
        scene.frame(1.0f/60.0f);
        // the value moves every frame
        CHECK(controls::getChangeStats().background == 1);
        textFrames+=controls::getChangeStats().text;
    }

    // six new percentages in a second (give or take one at the ends)
    CHECK(textFrames >= 5);
    CHECK(textFrames <= 7);

    // with the label off and back on, it shows the progress again
    pb->setAutoLabel(false);
    pb->setText("busy");
    scene.frame(1.0f/60.0f);
    CHECK(pb->getText() == "busy");
    pb->setAutoLabel(true);
    scene.frame(1.0f/60.0f);
    CHECK(pb->getText() != "busy");
}
