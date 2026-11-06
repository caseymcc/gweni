/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  ScrollControl decides its scrollbars in the layout pass that changes its
 *  size, from that pass's sizes, and sizes its inner panel for them: a resize
 *  that shows or hides a bar takes one pass, re-dirties nothing, and lays the
 *  content out at the width that leaves room for the bar.
 */
#include "headlessFixture.h"

#include <gweni/controls/scrollControl.h>

#include <doctest/doctest.h>

#include <string>

using namespace gweni;

namespace
{

//! A row that asks for a fixed size.
class Block: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Block, controls::Base)
    {}

    Size want;

    void calculateSize(skin::Base * /*skin*/, Dim dim) override
    {
        if(dim == Dim::X)
            m_preferredSize.width=want.width;
        else
            m_preferredSize.height=want.height;
    }
};

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    controls::ScrollControl *scroll;
    controls::Base *inner;
    controls::Base *verticalBar;
    controls::Base *horizontalBar;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
        scroll=canvas->newChild<controls::ScrollControl>("scroll");
        scroll->setDock(DockPosition::Center);

        // inner panel first (sent to back), then the two bars
        const controls::Base::List &parts=scroll->getActualChildren();
        REQUIRE(parts.size() == 3);
        auto part=parts.begin();
        inner=*part++;
        verticalBar=*part++;
        horizontalBar=*part;
    }

    ~Scene()
    {
        delete canvas;
    }

    const controls::LayoutStats &frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
        return canvas->getLayoutStats();
    }

    void settle()
    {
        for(int i=0; i<10 && frame().passes != 0; ++i)
            ;
    }

    Block *addRow(int width, int height)
    {
        Block *row=scroll->newChild<Block>();
        row->want=Size(width, height);
        row->setDock(DockPosition::Top);
        return row;
    }
};

void checkOnePass(const controls::LayoutStats &stats)
{
    INFO("re-dirtied by " << std::string(stats.redirtiedType?stats.redirtiedType:"none") << "#" << stats.redirtiedId);
    CHECK(stats.passes == 1);
    CHECK(stats.redirtiedType == nullptr);
    CHECK_FALSE(stats.renderLayout);
}

}//namespace

// Each case starts with the bars it checks already shown once, in the frames
// the control is created in: the first layout of a bar's arrow Text resizes
// that Text, which re-dirties layout on its own (the Text, not the
// ScrollControl; feature #131 handles it separately).

TEST_CASE("a ScrollControl hides and shows its vertical bar in the pass that resizes it")
{
    Scene scene;
    scene.canvas->setSize(400, 150);
    scene.scroll->setScroll(false, true);
    Block *row=scene.addRow(50, 200);
    scene.settle();

    // too short: the bar shows, and the row is laid out beside it (the inner
    // panel runs one pixel under the bar)
    REQUIRE_FALSE(scene.verticalBar->hidden());
    CHECK(scene.horizontalBar->hidden());
    const int barWidth=scene.verticalBar->getWidth();
    CHECK(barWidth > 1);
    CHECK(scene.verticalBar->getX() == 400-barWidth);
    CHECK(scene.verticalBar->getHeight() == 150);
    CHECK(scene.inner->getWidth() == 400-barWidth+1);
    CHECK(row->getWidth() == 400-barWidth+1);
    CHECK(scene.inner->getHeight() == 200);

    // the content fits: no bar, and the same pass gives the row the full width
    scene.canvas->setSize(400, 300);
    checkOnePass(scene.frame());
    CHECK(scene.verticalBar->hidden());
    CHECK(scene.inner->getWidth() == 400);
    CHECK(row->getWidth() == 400);
    CHECK(scene.inner->getHeight() == 300);
    CHECK(scene.frame().passes == 0);

    // and back, in one pass
    scene.canvas->setSize(400, 150);
    checkOnePass(scene.frame());
    CHECK_FALSE(scene.verticalBar->hidden());
    CHECK(scene.verticalBar->getHeight() == 150);
    CHECK(scene.inner->getWidth() == 400-barWidth+1);
    CHECK(row->getWidth() == 400-barWidth+1);
    CHECK(scene.frame().passes == 0);
}

TEST_CASE("showing one scrollbar can make the other one necessary")
{
    Scene scene;
    scene.canvas->setSize(400, 100);
    // 395 wide fits in 400, but not in 400 less the vertical bar
    scene.addRow(395, 200);
    scene.settle();
    REQUIRE_FALSE(scene.verticalBar->hidden());
    REQUIRE(scene.verticalBar->getWidth() > 5);
    CHECK_FALSE(scene.horizontalBar->hidden());
    // the horizontal bar runs along the bottom beside the vertical one
    CHECK(scene.horizontalBar->getWidth() == 400-scene.verticalBar->getWidth());
    CHECK(scene.horizontalBar->getY() == 100-scene.horizontalBar->getHeight());

    // tall enough for the content: neither bar is needed any more
    scene.canvas->setSize(400, 300);
    checkOnePass(scene.frame());
    CHECK(scene.verticalBar->hidden());
    CHECK(scene.horizontalBar->hidden());

    scene.canvas->setSize(400, 100);
    checkOnePass(scene.frame());
    CHECK_FALSE(scene.verticalBar->hidden());
    CHECK_FALSE(scene.horizontalBar->hidden());
    CHECK(scene.frame().passes == 0);
}

TEST_CASE("the scroll offset follows the bar and is reset when the bar goes away")
{
    Scene scene;
    scene.scroll->setScroll(false, true);
    scene.addRow(50, 400);
    scene.settle();
    REQUIRE_FALSE(scene.verticalBar->hidden());

    auto *bar=static_cast<controls::BaseScrollBar *>(scene.verticalBar);
    bar->setScrolledAmount(1, true);
    checkOnePass(scene.frame());
    // scrolled to the bottom: the 400 tall panel ends where the 300 tall
    // viewport does
    CHECK(scene.inner->getY() == 300-400);

    scene.canvas->setSize(400, 500);
    checkOnePass(scene.frame());
    CHECK(scene.verticalBar->hidden());
    CHECK(bar->getScrolledAmount() == 0);
    CHECK(scene.inner->getY() == 0);
}

TEST_CASE("a row added by app code is picked up by the next frame")
{
    Scene scene;
    scene.canvas->setSize(400, 150);
    scene.scroll->setScroll(false, true);
    scene.addRow(50, 200);
    scene.settle();
    REQUIRE_FALSE(scene.verticalBar->hidden());

    scene.canvas->setSize(400, 300);
    scene.settle();
    REQUIRE(scene.verticalBar->hidden());

    // The new row's own first frame may take a second pass (it is given its
    // height in the Y pass, after its X pass), but the ScrollControl adds none.
    scene.addRow(50, 200);
    const controls::LayoutStats &stats=scene.frame();
    for(controls::Base *part : {static_cast<controls::Base *>(scene.scroll),
        scene.inner, scene.verticalBar, scene.horizontalBar})
        CHECK(stats.redirtiedId != part->getControlId());
    CHECK_FALSE(scene.verticalBar->hidden());
    CHECK(scene.inner->getHeight() == 400);
    CHECK(scene.frame().passes == 0);
}
