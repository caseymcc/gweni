/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A control created by the render phase's layout pass (Canvas::renderCanvas's
 *  renderPhaseLayout(), where a wrapped Text builds its lines) is numbered
 *  before the walk draws it: Canvas::renumberZOrder() runs again after that
 *  pass, so its first draw is at its z in the depth-first numbering, not at the
 *  constructor's 0 (the depth-clear value, which draws nothing). It takes a z
 *  in the gap before the next control, which keeps its own, and the
 *  render-phase renumber's counts go into that frame's UpdateStats.
 *  UpdateStats::zChanged counts the controls whose z the renumber changed: on
 *  an insert, the inserted control and the controls after it up to the gap
 *  that holds it.
 */
#include "headlessFixture.h"

#include <doctest/doctest.h>

#include <vector>

using namespace gweni;

namespace
{

//! Adds one child from its own layout() once armed, as a wrapped Text adds a
//! line.
class Grower: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Grower, controls::Base)
    {}

    bool armed=false;
    controls::Base *grown=nullptr;

protected:
    void layout(skin::Base *skin) override
    {
        if(armed && !grown)
        {
            grown=newChild<controls::Base>("grown");
        }
        ParentClass::layout(skin);
    }
};

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
    }

    ~Scene()
    {
        delete canvas;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void settle()
    {
        for(int i=0; i<5; ++i)
            frame();
        REQUIRE_FALSE(canvas->zOrderDirty());
    }
};

}//namespace

TEST_CASE("a control the render phase's layout creates is drawn at its z in that frame")
{
    Scene scene;
    controls::Base *before=scene.canvas->newChild<controls::Base>("before");
    Grower *grower=scene.canvas->newChild<Grower>("grower");
    controls::Base *after=scene.canvas->newChild<controls::Base>("after");
    scene.settle();

    const int afterZ=after->getZIndex();

    // the update phase is over: only the render phase's pass lays it out
    scene.canvas->updateCanvas();
    grower->armed=true;
    grower->invalidate();
    scene.canvas->renderCanvas();

    REQUIRE(grower->grown);
    CHECK(scene.canvas->getLayoutStats().renderLayout);
    CHECK_FALSE(scene.canvas->zOrderDirty());

    const auto first=scene.renderer.firstDrawZ.find(grower->grown);
    REQUIRE(first != scene.renderer.firstDrawZ.end());
    CHECK(first->second > 0);
    CHECK(first->second == grower->grown->getZIndex());
    CHECK(first->second > grower->getZIndex());
    CHECK(first->second > before->getZIndex());
    // it took a z in the gap before the next sibling, which keeps its own
    CHECK(after->getZIndex() > first->second);
    CHECK(after->getZIndex() == afterZ);
    CHECK(after->getStateChange() == controls::StateChange_Nothing);

    // the render-phase renumber counted into this frame's stats: the new
    // control alone changed, and the walk entered the canvas, the grower and
    // it, passing the clean siblings
    CHECK(scene.canvas->getUpdateStats().zVisited == 3);
    CHECK(scene.canvas->getUpdateStats().zChanged == 1);

    // and the next frame has nothing left to number
    scene.frame();
    CHECK(scene.canvas->getUpdateStats().zVisited == 0);
    CHECK(scene.canvas->getUpdateStats().zChanged == 0);
}

TEST_CASE("UpdateStats::zChanged counts the controls an insert renumbered")
{
    Scene scene;
    std::vector<controls::Base *> rows;
    for(int i=0; i<10; ++i)
        rows.push_back(scene.canvas->newChild<controls::Base>());
    scene.settle();
    CHECK(scene.canvas->getUpdateStats().zChanged == 0);

    // a child of the fourth row: it and the six rows after it move
    rows[3]->newChild<controls::Base>();
    scene.frame();

    const controls::UpdateStats &stats=scene.canvas->getUpdateStats();
    // the canvas, the row and the child: the rows keep theirs
    CHECK(stats.zVisited == 3);
    CHECK(stats.zChanged > 0);
    CHECK(stats.zChanged <= stats.zVisited);
    // the rows before it keep their numbers
    CHECK(stats.zChanged <= 1+6);

    scene.frame();
    CHECK(scene.canvas->getUpdateStats().zChanged == 0);
}
