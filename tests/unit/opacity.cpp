/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Base::setOpacity: a control's own levels (surface, text) apply to its
 *  subtree, and a descendant's own setting replaces them rather than
 *  multiplying them. Pins, on a StubRenderer that records the levels each
 *  draw was made at:
 *
 *    draws         under nested settings every control's slots and text are
 *                  written at its effective levels, through the drain's tree
 *                  walk (the first frame) and its sorted path (a later change
 *                  to a few controls of a larger tree); a child at 1 inside a
 *                  0.5 panel draws at 1, and an override stops the walk; the
 *                  drain leaves the renderer at 1 afterwards.
 *    clamp         levels are clamped to [0, 1], NaN reads as 1.
 *    clear         clearOpacity() re-inherits the parent's levels, and the
 *                  drawn levels follow.
 *    attach        a control added under a translucent parent inherits; moved
 *                  to a parent at 1 or detached it is at 1; a control with an
 *                  own setting keeps it wherever it goes.
 *    marking       a change marks StateChange_Background|StateChange_Text on
 *                  exactly the controls whose effective levels changed, calls
 *                  no invalidate(), runs no layout pass and renders those
 *                  controls alone; the same levels again mark nothing.
 */
#include "headlessFixture.h"

#include <doctest/doctest.h>

#include <cmath>
#include <limits>
#include <vector>

using namespace gweni;

namespace
{

//! A StubControl that draws each of its slots as a filled rect and its text
//! id as a two-glyph run in every update(), so the stub records the levels
//! of each.
class DrawControl: public test::StubControl
{
public:
    using test::StubControl::StubControl;

    void update(renderer::Base *renderer, controls::Base *control) override
    {
        for(size_t id : m_primitives)
            renderer->drawFilledRect(id, control->getRenderBounds(), control->getZIndex());

        std::vector<size_t> primitiveIds;
        std::vector<size_t> textIds;

        appendIds(primitiveIds, textIds);
        for(size_t id : textIds)
            renderer->renderText(id, Font(), Point(0, 0), control->getZIndex(), "ab");
    }
};

class DrawSkin: public test::StubSkin
{
public:
    using test::StubSkin::StubSkin;

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        const bool takesText=typeName == test::StubText::getTypeNameStatic();
        return std::make_unique<DrawControl>(this, takesText, std::move(typeName));
    }
};

//! canvas
//!   panel             (own 0.5, 0.25)
//!     child           StubText, inherits
//!       override      own 1
//!         inner       StubText, inherits 1
//!     sibling         inherits
//!   other             at 1
//!   filler x 40       at 1, so a change of a few is drained in z order
struct Scene
{
    test::StubRenderer renderer;
    DrawSkin skin{&renderer};
    controls::Canvas *canvas;
    controls::Base *panel;
    controls::Base *child;
    controls::Base *override_;
    controls::Base *inner;
    controls::Base *sibling;
    controls::Base *other;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
        panel=canvas->newChild<controls::Base>();
        panel->setBounds(0, 0, 200, 100);
        child=panel->newChild<test::StubText>();
        override_=child->newChild<controls::Base>();
        inner=override_->newChild<test::StubText>();
        sibling=panel->newChild<controls::Base>();
        other=canvas->newChild<controls::Base>();
        for(int i=0; i<40; ++i)
            canvas->newChild<controls::Base>()->setBounds(0, 120+i, 10, 1);

        panel->setOpacity(0.5f, 0.25f);
        override_->setOpacity(1.0f);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene() { delete canvas; }

    void frame()
    {
        controls::resetChangeStats();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    size_t primitiveOf(controls::Base *control) const
    {
        REQUIRE(control->getSkinControl());
        REQUIRE(!control->getSkinControl()->getPrimitives().empty());
        return control->getSkinControl()->getPrimitives().front();
    }

    size_t textOf(controls::Base *control) const
    {
        std::vector<size_t> primitiveIds;
        std::vector<size_t> textIds;

        control->getSkinControl()->appendIds(primitiveIds, textIds);
        REQUIRE(textIds.size() == 1);
        return textIds.front();
    }

    //! The levels \p control's first slot was last written at.
    test::StubRenderer::Levels drawn(controls::Base *control)
    {
        const size_t id=primitiveOf(control);

        REQUIRE(renderer.primitiveLevels.count(id) == 1);
        return renderer.primitiveLevels[id];
    }

    test::StubRenderer::Levels drawnText(controls::Base *control)
    {
        const size_t id=textOf(control);

        REQUIRE(renderer.textLevels.count(id) == 1);
        return renderer.textLevels[id];
    }

    //! Forgets every recorded draw, so the next frame's are told apart.
    void forget()
    {
        renderer.primitiveLevels.clear();
        renderer.textLevels.clear();
    }
};

void checkLevels(test::StubRenderer::Levels levels, float surface, float text)
{
    CHECK(levels.surface == surface);
    CHECK(levels.text == text);
}

void checkEffective(controls::Base *control, float surface, float text)
{
    CHECK(control->effectiveSurfaceOpacity() == surface);
    CHECK(control->effectiveTextOpacity() == text);
}

}//namespace

TEST_CASE("Opacity: each draw is made at its control's effective levels")
{
    Scene scene;

    CHECK(scene.panel->hasOwnOpacity());
    CHECK(scene.panel->ownSurfaceOpacity() == 0.5f);
    CHECK(scene.panel->ownTextOpacity() == 0.25f);
    CHECK(!scene.child->hasOwnOpacity());
    CHECK(scene.child->ownSurfaceOpacity() == 1.0f);
    checkEffective(scene.panel, 0.5f, 0.25f);
    checkEffective(scene.child, 0.5f, 0.25f);
    checkEffective(scene.sibling, 0.5f, 0.25f);
    // an override stops the walk: its subtree keeps its own 1
    checkEffective(scene.override_, 1.0f, 1.0f);
    checkEffective(scene.inner, 1.0f, 1.0f);
    checkEffective(scene.other, 1.0f, 1.0f);
    checkEffective(scene.canvas, 1.0f, 1.0f);

    // the first frame: the drain's walk of the tree
    scene.frame();
    checkLevels(scene.drawn(scene.panel), 0.5f, 0.25f);
    checkLevels(scene.drawn(scene.child), 0.5f, 0.25f);
    checkLevels(scene.drawnText(scene.child), 0.5f, 0.25f);
    checkLevels(scene.drawn(scene.sibling), 0.5f, 0.25f);
    checkLevels(scene.drawn(scene.override_), 1.0f, 1.0f);
    checkLevels(scene.drawn(scene.inner), 1.0f, 1.0f);
    checkLevels(scene.drawnText(scene.inner), 1.0f, 1.0f);
    checkLevels(scene.drawn(scene.other), 1.0f, 1.0f);
    // left at 1 for whatever is drawn after the walk
    CHECK(scene.renderer.surfaceOpacity() == 1.0f);
    CHECK(scene.renderer.textOpacity() == 1.0f);

    // a change of three controls of 46: drained in z order, each its chain
    // rebuilt (the panel, then its child, then the sibling after a level at 1)
    scene.frame();
    scene.forget();
    scene.panel->setOpacity(0.75f, 0.5f);
    scene.frame();
    CHECK(scene.renderer.primitiveLevels.size() == 3*test::StubControl::primitivesPerControl);
    checkLevels(scene.drawn(scene.panel), 0.75f, 0.5f);
    checkLevels(scene.drawn(scene.child), 0.75f, 0.5f);
    checkLevels(scene.drawnText(scene.child), 0.75f, 0.5f);
    checkLevels(scene.drawn(scene.sibling), 0.75f, 0.5f);
    CHECK(scene.renderer.surfaceOpacity() == 1.0f);
    CHECK(scene.renderer.textOpacity() == 1.0f);
}

TEST_CASE("Opacity: levels are clamped to [0, 1], NaN reads as 1")
{
    Scene scene;

    scene.other->setOpacity(2.0f, -1.0f);
    CHECK(scene.other->ownSurfaceOpacity() == 1.0f);
    CHECK(scene.other->ownTextOpacity() == 0.0f);
    scene.other->setOpacity(std::numeric_limits<float>::quiet_NaN());
    checkEffective(scene.other, 1.0f, 1.0f);
    scene.other->setOpacity(0.0f);
    checkEffective(scene.other, 0.0f, 0.0f);
}

TEST_CASE("Opacity: clearOpacity() inherits the parent's levels again")
{
    Scene scene;

    scene.frame();
    scene.frame();
    scene.forget();
    scene.override_->clearOpacity();
    CHECK(!scene.override_->hasOwnOpacity());
    checkEffective(scene.override_, 0.5f, 0.25f);
    checkEffective(scene.inner, 0.5f, 0.25f);
    scene.frame();
    checkLevels(scene.drawn(scene.override_), 0.5f, 0.25f);
    checkLevels(scene.drawn(scene.inner), 0.5f, 0.25f);
    checkLevels(scene.drawnText(scene.inner), 0.5f, 0.25f);

    // the panel's own cleared: everything under it at 1
    scene.panel->clearOpacity();
    for(controls::Base *control : {scene.panel, scene.child, scene.override_, scene.inner,
        scene.sibling})
        checkEffective(control, 1.0f, 1.0f);
    scene.frame();
    checkLevels(scene.drawn(scene.inner), 1.0f, 1.0f);
}

TEST_CASE("Opacity: a control attached under a translucent parent inherits")
{
    Scene scene;

    scene.frame();

    // added under the panel
    controls::Base *added=scene.sibling->newChild<test::StubText>();
    checkEffective(added, 0.5f, 0.25f);
    scene.frame();
    checkLevels(scene.drawn(added), 0.5f, 0.25f);
    checkLevels(scene.drawnText(added), 0.5f, 0.25f);

    // moved to a parent at 1
    added->setParent(scene.other);
    checkEffective(added, 1.0f, 1.0f);
    scene.frame();
    checkLevels(scene.drawn(added), 1.0f, 1.0f);

    // moved back, with a child of its own
    controls::Base *below=added->newChild<controls::Base>();
    added->setParent(scene.child);
    checkEffective(added, 0.5f, 0.25f);
    checkEffective(below, 0.5f, 0.25f);

    // detached: on its own it is at 1
    added->setParent(nullptr);
    checkEffective(added, 1.0f, 1.0f);
    checkEffective(below, 1.0f, 1.0f);

    // an own setting goes with it
    added->setOpacity(0.125f);
    added->setParent(scene.sibling);
    checkEffective(added, 0.125f, 0.125f);
    checkEffective(below, 0.125f, 0.125f);
    added->setParent(scene.other);
    checkEffective(below, 0.125f, 0.125f);
}

TEST_CASE("Opacity: a change marks the controls whose levels changed, lays nothing out")
{
    Scene scene;

    scene.frame();
    scene.frame();
    for(controls::Base *control : {scene.panel, scene.child, scene.override_, scene.inner,
        scene.sibling, scene.other})
        REQUIRE(control->getStateChange() == controls::StateChange_Nothing);

    const size_t invalidations=controls::invalidateCalls();
    const int both=controls::StateChange_Background|controls::StateChange_Text;

    scene.panel->setOpacity(0.25f);
    CHECK(controls::invalidateCalls() == invalidations);
    CHECK(scene.panel->getStateChange() == both);
    CHECK(scene.child->getStateChange() == both);
    CHECK(scene.sibling->getStateChange() == both);
    // under its own setting: not changed
    CHECK(scene.override_->getStateChange() == controls::StateChange_Nothing);
    CHECK(scene.inner->getStateChange() == controls::StateChange_Nothing);
    CHECK(scene.other->getStateChange() == controls::StateChange_Nothing);
    CHECK(scene.canvas->getStateChange() == controls::StateChange_Nothing);

    scene.frame();
    CHECK(controls::invalidateCalls() == invalidations);
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    CHECK(scene.canvas->getLayoutStats().renderLayout == false);
    CHECK(controls::getChangeStats().controls == 3);
    CHECK(controls::getChangeStats().visited == 3);

    // the same levels again: nothing
    scene.panel->setOpacity(0.25f, 0.25f);
    CHECK(scene.panel->getStateChange() == controls::StateChange_Nothing);
    // a child's own setting equal to what it inherits: nothing changes on it
    scene.child->setOpacity(0.25f);
    CHECK(scene.child->getStateChange() == controls::StateChange_Nothing);
    // and the panel's change then stops at that child
    scene.panel->setOpacity(0.5f);
    CHECK(scene.panel->getStateChange() == both);
    CHECK(scene.sibling->getStateChange() == both);
    CHECK(scene.child->getStateChange() == controls::StateChange_Nothing);
    checkEffective(scene.child, 0.25f, 0.25f);
    scene.frame();
    CHECK(controls::invalidateCalls() == invalidations);
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    CHECK(controls::getChangeStats().controls == 2);
}
