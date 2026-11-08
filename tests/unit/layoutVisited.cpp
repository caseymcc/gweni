/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  LayoutStats::visited and LayoutStats::measureCalls, on a real Canvas with
 *  the StubRenderer: what a layout pass enters and measures.
 *
 *  The first pass enters every shown control, once per calculateSize() and
 *  once per arrange() in each axis, so 4 per control; a wrapped Text's lines
 *  are built in its X arrange, so on the frame they appear they miss the X
 *  calculateSize and are entered 3 times. A settled frame enters and measures
 *  nothing, and one Label's setText enters only the way down to it (the
 *  canvas, its panel, the Label and its Text) and measures only that Text.
 *  measureCalls is every renderer measureText() the controls made in the
 *  frame, the same number the renderer saw.
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/controls/label.h>

#include <doctest/doctest.h>

#include <vector>

using namespace gweni;

namespace
{

//! A canvas with three panels of four Labels each, and a fourth panel holding
//! one wrapped Label of two lines. A Label is the Label and its Text.
struct Scene
{
    static constexpr int Panels=3;
    static constexpr int Labels=4;
    //! the canvas, the panels, the Labels and their Texts, the wrapped
    //! Label's panel, Label and Text; not the wrapped Text's lines
    static constexpr size_t Controls=1+Panels+Panels*Labels*2+3;
    static constexpr size_t Lines=2;

    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    std::vector<controls::Label *> labels;
    controls::Label *wrapped;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);

        for(int p=0; p<Panels; ++p)
        {
            controls::Base *panel=canvas->newChild<controls::Base>();
            panel->setBounds(0, p*60, 300, 60);

            for(int l=0; l<Labels; ++l)
            {
                controls::Label *label=panel->newChild<controls::Label>();
                label->setText("label");
                label->setPos(l*70, 0);
                labels.push_back(label);
            }
        }

        controls::Base *panel=canvas->newChild<controls::Base>();
        panel->setBounds(0, 200, 300, 60);
        wrapped=panel->newChild<controls::Label>();
        wrapped->setBounds(0, 0, 200, 40);
        wrapped->setWrap(true);
        wrapped->setText("one two\nthree");
    }

    ~Scene()
    {
        delete canvas;
    }

    //! One frame; how many measureText() calls the renderer saw in it.
    size_t frame()
    {
        const size_t before=renderer.measures;

        canvas->updateCanvas();
        canvas->renderCanvas();
        return renderer.measures-before;
    }

    const controls::LayoutStats &stats() const { return canvas->getLayoutStats(); }
};

}//namespace

TEST_CASE("layout_visited: a pass enters every shown control, 4 times each")
{
    Scene scene;

    const size_t measured=scene.frame();

    REQUIRE(scene.stats().passes == 1);
    CHECK_FALSE(scene.stats().renderLayout);
    // the lines appear in the X arrange, after their X calculateSize
    CHECK(scene.stats().visited == 4*Scene::Controls+3*Scene::Lines);
    CHECK(scene.stats().measureCalls == measured);
    CHECK(scene.stats().measureCalls > 0);
}

TEST_CASE("layout_visited: a settled frame enters and measures nothing")
{
    Scene scene;

    scene.frame();
    scene.frame();

    const size_t measured=scene.frame();

    CHECK(scene.stats().passes == 0);
    CHECK(scene.stats().visited == 0);
    CHECK(scene.stats().measureCalls == 0);
    CHECK(measured == 0);
}

TEST_CASE("layout_visited: one Label's setText enters only the way down to it")
{
    Scene scene;

    scene.frame();
    scene.frame();

    scene.labels[5]->setText("other");

    const size_t measured=scene.frame();

    REQUIRE(scene.stats().passes == 1);
    // the canvas, the Label's panel, the Label and its Text, in all four
    // steps; the other panels, Labels and the wrapped Text's lines are clean
    CHECK(scene.stats().visited == 4*4);
    CHECK(scene.stats().measureCalls == measured);
    // Only the changed Text is measured: twice in each axis
    // (Text::lineHeight()'s " " and its string).
    CHECK(scene.stats().measureCalls == 4);
}
