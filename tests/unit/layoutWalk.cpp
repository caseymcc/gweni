/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A layout pass enters only the dirty paths (#321), on a real Canvas with the
 *  StubRenderer. Every control of the scene counts the calculateSize() and
 *  arrange() calls the pass makes on it, so a case checks which controls were
 *  entered, not only how many (LayoutStats::visited).
 *
 *  A settled frame enters none. A changed leaf enters the canvas, its panel,
 *  its branch and itself, each in all four steps, and no other control: the
 *  dirty parents loop over their children without entering the clean ones. A
 *  moved branch enters the way down to it and none of its own subtree; a
 *  resized one enters its subtree too. A wrapped Text, which reads its
 *  parent's requested width and padding (readsParentLayout()), is laid out
 *  again when either changes, even when the parent's bounds stay where they
 *  are, and ends with the preferred size, bounds and lines of a fresh layout
 *  of the same tree.
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/controls/text.h>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace gweni;

namespace
{

//! A control that counts the steps a pass entered it in, and measures as
//! `want` when it has no children.
class Probe: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Probe, controls::Base)
    {}

    Size want{10, 10};
    int calculated=0;
    int arranged=0;

    int entered() const { return calculated+arranged; }
    void reset() { calculated=arranged=0; }

    void calculateSize(skin::Base *skin, Dim dim) override
    {
        ++calculated;

        if(!getActualChildren().empty())
        {
            ParentClass::calculateSize(skin, dim);
            return;
        }

        if(dim == Dim::X)
            m_preferredSize.width=want.width;
        else
            m_preferredSize.height=want.height;
    }

    void arrange(skin::Base *skin, Dim dim) override
    {
        ++arranged;
        ParentClass::arrange(skin, dim);
    }
};

//! A canvas holding three panels of three branches of two leaves each.
struct Scene
{
    static constexpr int Panels=3;
    static constexpr int Branches=3;
    static constexpr int Leaves=2;

    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    std::vector<Probe *> probes;
    std::vector<Probe *> panels;
    std::vector<Probe *> branches;
    std::vector<Probe *> leaves;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);

        for(int p=0; p<Panels; ++p)
        {
            Probe *panel=add(canvas, panels);
            panel->setBounds(0, p*100, 400, 100);

            for(int b=0; b<Branches; ++b)
            {
                Probe *branch=add(panel, branches);
                branch->setBounds(b*120, 0, 100, 80);

                for(int l=0; l<Leaves; ++l)
                {
                    Probe *leaf=add(branch, leaves);
                    leaf->setPos(l*40, 0);
                }
            }
        }

        settle();
    }

    ~Scene()
    {
        delete canvas;
    }

    Probe *add(controls::Base *parent, std::vector<Probe *> &list)
    {
        Probe *probe=parent->newChild<Probe>();
        probes.push_back(probe);
        list.push_back(probe);
        return probe;
    }

    void frame()
    {
        for(Probe *probe : probes)
            probe->reset();

        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void settle()
    {
        frame();
        frame();
    }

    const controls::LayoutStats &stats() const { return canvas->getLayoutStats(); }

    //! The probes the last frame entered, in creation order.
    std::vector<Probe *> entered() const
    {
        std::vector<Probe *> result;

        for(Probe *probe : probes)
        {
            if(probe->entered())
                result.push_back(probe);
        }
        return result;
    }
};

}//namespace

TEST_CASE("layout walk: a settled frame enters no control")
{
    Scene scene;

    scene.frame();

    CHECK(scene.stats().passes == 0);
    CHECK(scene.stats().visited == 0);
    CHECK(scene.entered().empty());
}

TEST_CASE("layout walk: a changed leaf enters only the way down to it")
{
    Scene scene;
    Probe *panel=scene.panels[1];
    Probe *branch=scene.branches[4];
    Probe *leaf=scene.leaves[9];
    REQUIRE(leaf->getParent() == branch);
    REQUIRE(branch->getParent() == panel);

    leaf->want=Size(30, 20);
    leaf->invalidate();
    scene.frame();

    REQUIRE(scene.stats().passes == 1);
    // the canvas and the three probes, in all four steps
    CHECK(scene.stats().visited == 4*4);
    CHECK(scene.entered() == std::vector<Probe *>{panel, branch, leaf});
    CHECK(leaf->calculated == 2);
    CHECK(leaf->arranged == 2);
    CHECK(leaf->getWidth() == 30);
    CHECK(leaf->getHeight() == 20);

    scene.frame();
    CHECK(scene.stats().visited == 0);
}

TEST_CASE("layout walk: a moved control enters none of its own subtree")
{
    Scene scene;
    Probe *branch=scene.branches[4];

    branch->setPos(130, 10);
    scene.frame();

    REQUIRE(scene.stats().passes == 1);
    CHECK(scene.stats().visited == 4*3);
    CHECK(scene.entered() == std::vector<Probe *>{scene.panels[1], branch});
    CHECK(branch->getX() == 130);
    CHECK(branch->getY() == 10);

    scene.frame();
    CHECK(scene.stats().visited == 0);
}

TEST_CASE("layout walk: a resized control enters its subtree")
{
    Scene scene;
    Probe *branch=scene.branches[4];

    branch->setSize(110, 90);
    scene.frame();

    REQUIRE(scene.stats().passes == 1);
    // the canvas, the panel, the branch and its two leaves
    CHECK(scene.stats().visited == 4*5);
    CHECK(scene.entered() == std::vector<Probe *>{scene.panels[1], branch,
        scene.leaves[8], scene.leaves[9]});
    CHECK(branch->getWidth() == 110);
    CHECK(branch->getHeight() == 90);
}

namespace
{

//! A wrapped Text in a panel docked Center, filling the canvas, so a size the panel
//! asks for changes its request but not its bounds.
struct WrapScene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    controls::Base *panel;
    controls::internal::Text *text;

    WrapScene(int requestedWidth, const Padding &padding)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(240, 200);
        panel=canvas->newChild<controls::Base>();
        panel->setDock(DockPosition::Center);
        panel->setWidth(requestedWidth);
        panel->setPadding(padding);
        text=panel->newChild<controls::internal::Text>();
        text->setWrap(true);
        text->setText("one two three four five six seven eight nine ten"
            " eleven twelve thirteen fourteen fifteen sixteen");
        frame();
        frame();
    }

    ~WrapScene()
    {
        delete canvas;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    std::vector<std::string> lines()
    {
        std::vector<std::string> result;

        for(unsigned i=0; i<text->numLines(); ++i)
            result.push_back(text->getLine((int)i)->getText());
        return result;
    }
};

void checkSameLayout(WrapScene &scene, int requestedWidth, const Padding &padding)
{
    WrapScene fresh(requestedWidth, padding);

    CHECK((scene.panel->getBounds() == fresh.panel->getBounds()));
    CHECK(scene.text->getPreferredSize().width == fresh.text->getPreferredSize().width);
    CHECK(scene.text->getPreferredSize().height == fresh.text->getPreferredSize().height);
    CHECK((scene.text->getBounds() == fresh.text->getBounds()));
    CHECK(scene.lines() == fresh.lines());
}

}//namespace

TEST_CASE("layout walk: a wrapped Text re-lays when its parent's requested width changes")
{
    WrapScene scene(200, Padding(2, 2, 2, 2));
    const Rect bounds=scene.panel->getBounds();
    REQUIRE(scene.lines().size() > 1);

    REQUIRE(scene.text->getPreferredSize().width == 200-4);

    // The panel is as wide as the canvas already, docked, so asking for that
    // width changes its request alone: no bounds change reaches the Text,
    // whose preferred width is the request's inner width.
    REQUIRE(bounds.w == 240);
    scene.panel->setWidth(240);
    scene.frame();

    CHECK((scene.panel->getBounds() == bounds));
    CHECK(scene.text->getPreferredSize().width == 240-4);
    checkSameLayout(scene, 240, Padding(2, 2, 2, 2));
}

TEST_CASE("layout walk: a wrapped Text re-lays when its parent's padding changes")
{
    WrapScene scene(200, Padding(2, 2, 2, 2));
    const std::vector<std::string> before=scene.lines();

    scene.panel->setPadding(Padding(40, 2, 60, 2));
    scene.frame();

    CHECK(scene.text->getPreferredSize().width == 200-100);
    CHECK(scene.lines() != before);
    checkSameLayout(scene, 200, Padding(40, 2, 60, 2));
}
