/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  controls::Spinner on a real Canvas with a driven clock, drawn by the
 *  textured skin's Spinner renderer on the StubRenderer, which records the
 *  arcs it draws:
 *
 *    animation   registered only while it spins, its speed is not 0 and it is
 *                visible(): stop(), setSpeed(0), its own hide, an ancestor's
 *                and being built into a page hidden already leave it none
 *                (animsRun 0), and start(), a speed and the show give it back.
 *    stop/start  stop() hides it (setHideWhenStopped, the default) and
 *                start() shows it again, but not one the app hid itself; with
 *                setHideWhenStopped(false) it stays and draws a whole ring in
 *                SpinnerStopped's colour, standing still.
 *    angles      Spinner::arcAt at known times (the cycle's start, its
 *                widest, halfway back, the next cycle's start, a doubled
 *                speed), and the arc it marks and draws at stepped-clock
 *                times is arcAt's, as the slot keeps it (arcStartBits,
 *                arcSweepBits), in SpinnerArc's colour at z+1.
 *    quantised   a step whose angles keep their bits (a frozen clock, a speed
 *                too slow to move them) marks and draws nothing; one that
 *                changes them writes the arc's slot alone.
 *    sizes       the preferred size of Small is the skin font's line height,
 *                Medium and Large their diameters, setDiameter() its own; the
 *                stroke is a tenth of the shorter side, at least MinThickness.
 *    input       no keyboard input, no tab stop, no mouse (a hit test finds
 *                its parent) until a tooltip is set, which makes it hovered.
 *    track       setShowTrack draws a whole ring in SpinnerTrack's colour at
 *                its z, behind the arc, while it spins; stopped and shown, the
 *                still ring is drawn alone.
 */
#include "headlessFixture.h"

#include <gweni/anim.h>
#include <gweni/controls/spinner.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <doctest/doctest.h>

#include <cmath>

using namespace gweni;

namespace
{

//! The textured skin's Spinner renderer, a StubControl for the rest.
class SpinnerSkin: public test::StubSkin
{
public:
    explicit SpinnerSkin(test::StubRenderer *renderer): StubSkin(renderer) {}

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == "Spinner")
            return std::make_unique<skin::textured::Spinner>();
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

    controls::Spinner *spinner(controls::Base *parent=nullptr)
    {
        controls::Spinner *s=(parent?parent:canvas)->newChild<controls::Spinner>();

        s->setPos(10, 10);
        return s;
    }

    //! One frame, \p seconds after the last; the counts are this frame's.
    void frame(double seconds=0.0)
    {
        clock+=seconds;
        renderer.arcs.clear();
        controls::resetChangeStats();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    size_t animsRun() const { return canvas->getUpdateStats().animsRun; }

    static size_t slot(controls::Base *control, size_t i)
    {
        return control->getSkinControl()->getPrimitives()[i];
    }

    //! The arcs drawn into one slot this frame.
    std::vector<test::StubRenderer::ArcDraw> arcsInto(size_t id) const
    {
        std::vector<test::StubRenderer::ArcDraw> found;

        for(const auto &arc : renderer.arcs)
        {
            if(arc.id == id)
                found.push_back(arc);
        }
        return found;
    }

    test::StubRenderer renderer;
    SpinnerSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    double clock=10.0;
};

bool sameColor(const Color &a, const Color &b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

}//namespace

TEST_CASE("Spinner: its animation is registered only while it spins and is visible")
{
    Scene scene;
    controls::Spinner *s=scene.spinner();

    CHECK(s->isSpinning());
    CHECK(anim::activeCount(s) == 1);
    scene.frame(1.0/60.0);
    CHECK(scene.animsRun() == 1);

    s->stop();
    CHECK_FALSE(s->isSpinning());
    CHECK(anim::activeCount(s) == 0);
    scene.frame(1.0/60.0);
    CHECK(scene.animsRun() == 0);

    s->start();
    CHECK(anim::activeCount(s) == 1);
    scene.frame(1.0/60.0);
    CHECK(scene.animsRun() == 1);

    s->setSpeed(0.0f);
    CHECK(anim::activeCount(s) == 0);
    s->setSpeed(2.0f);
    CHECK(anim::activeCount(s) == 1);

    // stopped where it stays shown: nothing either
    s->setHideWhenStopped(false);
    s->stop();
    CHECK_FALSE(s->hidden());
    CHECK(anim::activeCount(s) == 0);
    scene.frame(1.0/60.0);
    CHECK(scene.animsRun() == 0);
    s->start();
    CHECK(anim::activeCount(s) == 1);
    s->setHideWhenStopped(true);

    // stopped and hidden, then shown by the app: still stopped, nothing
    s->stop();
    s->show();
    CHECK(anim::activeCount(s) == 0);
}

TEST_CASE("Spinner: hiding it, an ancestor or a hidden page cancels its animation, showing registers it again")
{
    Scene scene;
    controls::Base *page=scene.canvas->newChild<controls::Base>();
    controls::Spinner *s=scene.spinner(page);

    scene.frame(1.0/60.0);
    REQUIRE(scene.animsRun() == 1);

    s->hide();
    CHECK(anim::activeCount(s) == 0);
    scene.frame(1.0/60.0);
    CHECK(scene.animsRun() == 0);
    s->show();
    CHECK(anim::activeCount(s) == 1);
    scene.frame(1.0/60.0);
    CHECK(scene.animsRun() == 1);

    page->hide();
    CHECK(anim::activeCount(s) == 0);
    scene.frame(1.0/60.0);
    CHECK(scene.animsRun() == 0);
    page->show();
    CHECK(anim::activeCount(s) == 1);
    scene.frame(1.0/60.0);
    CHECK(scene.animsRun() == 1);

    // built into a page hidden already, as the shell's pages from index 1 on
    page->hide();
    controls::Spinner *built=scene.spinner(page);

    CHECK(anim::activeCount(built) == 0);
    scene.frame(1.0/60.0);
    CHECK(scene.animsRun() == 0);
    page->show();
    CHECK(anim::activeCount(built) == 1);
    scene.frame(1.0/60.0);
    CHECK(scene.animsRun() == 2);
    // shown, it is where the clock says, not where it was hidden
    const controls::Spinner::Arc arc=controls::Spinner::arcAt(scene.clock, built->getSpeed());

    CHECK(built->getStartBits() == renderer::arcStartBits(float(arc.start)));
    CHECK(built->getSweepBits() == renderer::arcSweepBits(float(arc.sweep)));
}

TEST_CASE("Spinner: stop() hides it and start() shows it, or with setHideWhenStopped(false) a still ring stays")
{
    Scene scene;
    controls::Spinner *s=scene.spinner();

    scene.frame(1.0/60.0);
    CHECK(s->hidesWhenStopped());
    s->stop();
    CHECK(s->hidden());
    scene.frame(1.0/60.0);
    s->start();
    CHECK_FALSE(s->hidden());

    // one the app hid stays hidden through a stop and a start
    s->hide();
    s->stop();
    s->start();
    CHECK(s->hidden());
    s->show();
    scene.frame(1.0/60.0);

    // a stop that hid it, then the hiding turned off: the stopped ring shows
    s->stop();
    CHECK(s->hidden());
    s->setHideWhenStopped(false);
    CHECK_FALSE(s->hidden());
    CHECK(anim::activeCount(s) == 0);
    scene.frame(1.0/60.0);

    const auto ring=scene.arcsInto(Scene::slot(s, 1));

    REQUIRE(ring.size() == 1);
    CHECK(ring[0].sweepTurns == 1.0f);
    CHECK(sameColor(ring[0].color, skin::textured::tokyonight::SpinnerStopped.top));

    // standing still: frames later nothing is marked or drawn
    for(int i=0; i<5; ++i)
    {
        scene.frame(1.0/60.0);
        CHECK(controls::getChangeStats().controls == 0);
        CHECK(scene.renderer.arcs.empty());
        CHECK(scene.animsRun() == 0);
    }

    // turned on again while stopped, it hides
    s->setHideWhenStopped(true);
    CHECK(s->hidden());
    s->start();
    CHECK_FALSE(s->hidden());
}

TEST_CASE("Spinner: arcAt grows the sweep with its head and shrinks it with its tail, each cycle on from the last")
{
    using controls::Spinner;
    const double d=Spinner::MaxSweep-Spinner::MinSweep;

    struct Case
    {
        double seconds;
        float speed;
        double start;
        double sweep;
    };

    const Case cases[]={
        // a cycle's start: the shortest sweep
        {0.0, 1.0f, 0.0, Spinner::MinSweep},
        // its widest: the head ran d ahead while the base turned half a cycle
        {0.75, 1.0f, 0.75, Spinner::MaxSweep},
        // halfway back: the tail has caught up half of it
        {1.125, 1.0f, 1.125+0.5*d-1.0, Spinner::MaxSweep-0.5*d},
        // the next cycle starts where the tail ended
        {1.5, 1.0f, 1.5+d-1.0-1.0, Spinner::MinSweep},
        // twice the speed is the same arc at half the time
        {0.375, 2.0f, 0.75, Spinner::MaxSweep},
    };

    for(const Case &c : cases)
    {
        CAPTURE(c.seconds);
        CAPTURE(c.speed);

        const Spinner::Arc arc=Spinner::arcAt(c.seconds, c.speed);
        double start=c.start-std::floor(c.start);

        CHECK(arc.start == doctest::Approx(start).epsilon(1e-9));
        CHECK(arc.sweep == doctest::Approx(c.sweep).epsilon(1e-9));
        CHECK(arc.start >= 0.0);
        CHECK(arc.start < 1.0);
    }
}

TEST_CASE("Spinner: the arc it marks and draws at each stepped-clock time is arcAt's")
{
    Scene scene;
    controls::Spinner *s=scene.spinner();

    scene.frame();
    for(int i=0; i<40; ++i)
    {
        scene.frame(1.0/60.0);
        CAPTURE(i);

        const controls::Spinner::Arc arc=controls::Spinner::arcAt(scene.clock, 1.0f);
        const uint16_t start=renderer::arcStartBits(float(arc.start));
        const uint16_t sweep=renderer::arcSweepBits(float(arc.sweep));

        CHECK(s->getStartBits() == start);
        CHECK(s->getSweepBits() == sweep);

        const auto drawn=scene.arcsInto(Scene::slot(s, 1));

        REQUIRE(drawn.size() == 1);
        CHECK(renderer::arcStartBits(drawn[0].startTurns) == start);
        CHECK(renderer::arcSweepBits(drawn[0].sweepTurns) == sweep);
        CHECK(drawn[0].zIndex == s->getZIndex()+1);
        CHECK(sameColor(drawn[0].color, skin::textured::tokyonight::SpinnerArc.top));
        // no track: its slot draws no arc
        CHECK(scene.arcsInto(Scene::slot(s, 0)).empty());
    }
}

TEST_CASE("Spinner: a step that keeps the arc's bits marks and draws nothing")
{
    Scene scene;
    controls::Spinner *s=scene.spinner();

    scene.frame(1.0/60.0);
    scene.frame(1.0/60.0);

    // the clock standing still
    for(int i=0; i<3; ++i)
    {
        scene.frame(0.0);
        CHECK(scene.animsRun() == 1);
        CHECK(controls::getChangeStats().controls == 0);
        CHECK(scene.renderer.arcs.empty());
    }

    // so slow its angles move less than a bit a frame
    s->setSpeed(1e-7f);
    scene.frame(1.0/60.0);
    for(int i=0; i<3; ++i)
    {
        scene.frame(1.0/60.0);
        CHECK(scene.animsRun() == 1);
        CHECK(controls::getChangeStats().controls == 0);
        CHECK(scene.renderer.arcs.empty());
    }

    // at its speed a frame moves them: the arc's slot alone
    s->setSpeed(1.0f);
    scene.frame(1.0/60.0);
    scene.frame(1.0/60.0);
    CHECK(controls::getChangeStats().controls == 1);
    CHECK(scene.renderer.arcs.size() == 1);
    CHECK(scene.arcsInto(Scene::slot(s, 1)).size() == 1);
}

TEST_CASE("Spinner: the preferred sizes, Small the skin font's line height, and the stroke a tenth of it")
{
    Scene scene;
    controls::Spinner *s=scene.spinner();
    const int lineHeight=scene.renderer.lineHeight(scene.skin.getDefaultFont());

    scene.frame();
    CHECK(s->getSpinnerSize() == controls::Spinner::Size::Small);
    CHECK(s->getDiameter() == lineHeight);
    CHECK(s->getPreferredSize().width == lineHeight);
    CHECK(s->getPreferredSize().height == lineHeight);
    CHECK(s->getWidth() == lineHeight);
    CHECK(s->getHeight() == lineHeight);
    CHECK(s->getThickness() == doctest::Approx(std::max(controls::Spinner::MinThickness, 0.1f*lineHeight)));

    s->setSpinnerSize(controls::Spinner::Size::Medium);
    scene.frame();
    CHECK(s->getWidth() == controls::Spinner::MediumDiameter);
    CHECK(s->getHeight() == controls::Spinner::MediumDiameter);
    CHECK(s->getThickness() == doctest::Approx(3.2f));

    s->setSpinnerSize(controls::Spinner::Size::Large);
    scene.frame();
    CHECK(s->getWidth() == controls::Spinner::LargeDiameter);
    CHECK(s->getThickness() == doctest::Approx(4.8f));

    s->setDiameter(10);
    scene.frame();
    CHECK(s->getWidth() == 10);
    CHECK(s->getHeight() == 10);
    CHECK(s->getThickness() == doctest::Approx(controls::Spinner::MinThickness));

    // a size clears the diameter
    s->setSpinnerSize(controls::Spinner::Size::Medium);
    scene.frame();
    CHECK(s->getWidth() == controls::Spinner::MediumDiameter);
}

TEST_CASE("Spinner: it takes no keys, no tab and no mouse until a tooltip is set")
{
    Scene scene;
    controls::Base *parent=scene.canvas->newChild<controls::Base>();

    parent->setBounds(0, 0, 100, 100);

    controls::Spinner *s=scene.spinner(parent);

    scene.frame();
    CHECK_FALSE(s->getKeyboardInputEnabled());
    CHECK_FALSE(s->isTabable());
    CHECK_FALSE(s->getMouseInputEnabled());

    const int x=s->getX()+s->getWidth()/2;
    const int y=s->getY()+s->getHeight()/2;

    CHECK(parent->getControlAt(x, y) == parent);

    s->setToolTipText("Saving");
    CHECK(s->getMouseInputEnabled());
    CHECK(parent->getControlAt(x, y) == s);
    s->setToolTipText("");
    CHECK_FALSE(s->getMouseInputEnabled());
}

TEST_CASE("Spinner: setShowTrack draws a whole ring in SpinnerTrack's colour behind the arc while it spins")
{
    Scene scene;
    controls::Spinner *s=scene.spinner();

    scene.frame(1.0/60.0);
    CHECK(scene.arcsInto(Scene::slot(s, 0)).empty());

    s->setShowTrack(true);
    scene.frame(0.0);

    const auto track=scene.arcsInto(Scene::slot(s, 0));

    REQUIRE(track.size() == 1);
    CHECK(track[0].sweepTurns == 1.0f);
    CHECK(track[0].zIndex == s->getZIndex());
    CHECK(sameColor(track[0].color, skin::textured::tokyonight::SpinnerTrack.top));

    // stopped where it stays shown, the still ring alone: no track under it
    s->setHideWhenStopped(false);
    s->stop();
    scene.frame(1.0/60.0);
    CHECK(scene.arcsInto(Scene::slot(s, 0)).empty());
    CHECK(scene.arcsInto(Scene::slot(s, 1)).size() == 1);
}
