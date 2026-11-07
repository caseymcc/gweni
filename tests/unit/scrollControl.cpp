/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  ScrollControl decides its scrollbars in the layout pass that changes its
 *  size, from that pass's sizes, and sizes its inner panel for them: a resize
 *  that shows or hides a bar takes one pass, re-dirties nothing, and lays the
 *  content out at the width that leaves room for the bar.
 *
 *  It places the content at its viewport: the control less its padding, the
 *  bars that show and the inner panel's (default 5 unit) margin. Unscrolled
 *  the inner panel's top-left is the viewport's; at its content size the
 *  control shows the content with that inset on all four sides; a bar shows
 *  exactly when the content does not fit the viewport; scrolled to the end the
 *  content's far edge is the viewport's. A Menu and a ListBox at their content
 *  size have the same gap on opposite sides, and a MenuStrip keeps its items on
 *  one row, its full height, with no bar. A control in nested inner panels (a
 *  TreeNode in its TreeControl's ScrollControl) maps to and from canvas
 *  coordinates through every panel's offset, as hit-testing finds it.
 */
#include "headlessFixture.h"

#include <gweni/controls/scrollControl.h>
#include <gweni/controls/listBox.h>
#include <gweni/controls/menuStrip.h>
#include <gweni/controls/treeControl.h>
#include <gweni/controls/layout/layout.h>

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

    // too short: the bar shows, and the row is laid out beside it, inside the
    // inner panel's 5 unit margin on both sides
    REQUIRE_FALSE(scene.verticalBar->hidden());
    CHECK(scene.horizontalBar->hidden());
    const int barWidth=scene.verticalBar->getWidth();
    CHECK(barWidth > 1);
    CHECK(scene.verticalBar->getX() == 400-barWidth);
    CHECK(scene.verticalBar->getHeight() == 150);
    CHECK(scene.inner->getX() == 5);
    CHECK(scene.inner->getY() == 5);
    CHECK(scene.inner->getWidth() == 400-barWidth-10);
    CHECK(row->getWidth() == 400-barWidth-10);
    CHECK(scene.inner->getHeight() == 200);

    // the content fits: no bar, and the same pass gives the row the full
    // viewport width
    scene.canvas->setSize(400, 300);
    checkOnePass(scene.frame());
    CHECK(scene.verticalBar->hidden());
    CHECK(scene.inner->getWidth() == 390);
    CHECK(row->getWidth() == 390);
    CHECK(scene.inner->getHeight() == 290);
    CHECK(scene.frame().passes == 0);

    // and back, in one pass
    scene.canvas->setSize(400, 150);
    checkOnePass(scene.frame());
    CHECK_FALSE(scene.verticalBar->hidden());
    CHECK(scene.verticalBar->getHeight() == 150);
    CHECK(scene.inner->getWidth() == 400-barWidth-10);
    CHECK(row->getWidth() == 400-barWidth-10);
    CHECK(scene.frame().passes == 0);
}

TEST_CASE("showing one scrollbar can make the other one necessary")
{
    Scene scene;
    scene.canvas->setSize(400, 100);
    // 385 wide fits the 390 wide viewport, but not 390 less the vertical bar
    scene.addRow(385, 200);
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
    // scrolled to the bottom: the 400 tall panel ends where the 290 tall
    // viewport does, 5 above the control's bottom
    CHECK(scene.inner->getY() == 5+290-400);
    CHECK(scene.inner->getY()+scene.inner->getHeight() == 300-5);

    scene.canvas->setSize(400, 500);
    checkOnePass(scene.frame());
    CHECK(scene.verticalBar->hidden());
    CHECK(bar->getScrolledAmount() == 0);
    CHECK(scene.inner->getY() == 5);
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

// A size the app asked for is the ScrollControl's outer size: the bars go
// inside it and the content scrolls. With no request it is its content.

TEST_CASE("a requested size smaller than the content caps a ScrollControl in a Base parent")
{
    Scene scene;
    scene.scroll->setDock(DockPosition::None);
    scene.scroll->setBounds(10, 20, 100, 80);
    Block *row=scene.addRow(50, 200);
    scene.settle();

    CHECK(scene.scroll->getX() == 10);
    CHECK(scene.scroll->getY() == 20);
    CHECK(scene.scroll->getWidth() == 100);
    CHECK(scene.scroll->getHeight() == 80);

    // the vertical bar shows, enabled, inside the requested size
    REQUIRE_FALSE(scene.verticalBar->hidden());
    CHECK(scene.verticalBar->visible());
    CHECK_FALSE(scene.verticalBar->isDisabled());
    const int barWidth=scene.verticalBar->getWidth();
    CHECK(scene.verticalBar->getX() == 100-barWidth);
    CHECK(scene.verticalBar->getHeight() == 80);
    // 50 wide fits beside it: no horizontal bar
    CHECK(scene.horizontalBar->hidden());
    // the inner panel holds all of the content, to scroll it
    CHECK(scene.inner->getHeight() == 200);
    CHECK(row->getHeight() == 200);
    CHECK(scene.frame().passes == 0);

    // content that fits: still the requested size, and no bar
    row->want=Size(50, 40);
    row->invalidate();
    scene.settle();
    CHECK(scene.scroll->getWidth() == 100);
    CHECK(scene.scroll->getHeight() == 80);
    CHECK(scene.verticalBar->hidden());
}

TEST_CASE("a requested size smaller than the content caps a ScrollControl in a layout cell")
{
    Scene scene;
    controls::Base *panel=scene.canvas->newChild<controls::Base>("panel");
    panel->setDock(DockPosition::Center);
    panel->setLayout(new controls::VerticalLayout());
    scene.scroll->setDock(DockPosition::None);
    scene.scroll->setParent(panel);
    scene.scroll->setBounds(0, 0, 100, 200);
    scene.addRow(40, 500);
    scene.settle();

    // the request, not the 500 tall content
    CHECK(scene.scroll->getWidth() == 100);
    CHECK(scene.scroll->getHeight() == 200);
    REQUIRE_FALSE(scene.verticalBar->hidden());
    CHECK_FALSE(scene.verticalBar->isDisabled());
    CHECK(scene.verticalBar->getHeight() == 200);
    CHECK(scene.horizontalBar->hidden());

    // a larger cell does not stretch it either
    scene.canvas->setSize(640, 480);
    scene.settle();
    CHECK(scene.scroll->getWidth() == 100);
    CHECK(scene.scroll->getHeight() == 200);
    CHECK_FALSE(scene.verticalBar->hidden());
}

TEST_CASE("a ScrollControl with no requested size is its content's size")
{
    Scene scene;
    scene.scroll->setDock(DockPosition::None);
    scene.addRow(60, 70);
    scene.settle();

    CHECK_FALSE(scene.scroll->hasRequestedSize(Dim::X));
    CHECK_FALSE(scene.scroll->hasRequestedSize(Dim::Y));
    // the row and the inner panel's 5 unit margins
    CHECK(scene.scroll->getWidth() == 60+10);
    CHECK(scene.scroll->getHeight() == 70+10);
    CHECK(scene.verticalBar->hidden());
    CHECK(scene.horizontalBar->hidden());
}

TEST_CASE("a requested height caps only the height")
{
    Scene scene;
    scene.scroll->setDock(DockPosition::None);
    scene.scroll->setScroll(false, true);
    scene.scroll->setHeight(60);
    scene.addRow(80, 200);
    scene.settle();

    // as wide as its content, as high as asked
    CHECK(scene.scroll->getWidth() == 80+10);
    CHECK(scene.scroll->getHeight() == 60);
    CHECK_FALSE(scene.verticalBar->hidden());
}

TEST_CASE("setScroll turns an axis off even when its content overflows the requested size")
{
    SUBCASE("vertical only")
    {
        Scene scene;
        scene.scroll->setDock(DockPosition::None);
        scene.scroll->setScroll(false, true);
        scene.scroll->setBounds(0, 0, 100, 80);
        scene.addRow(300, 200);
        scene.settle();

        CHECK(scene.scroll->getWidth() == 100);
        CHECK(scene.scroll->getHeight() == 80);
        CHECK_FALSE(scene.verticalBar->hidden());
        CHECK(scene.horizontalBar->hidden());
    }

    SUBCASE("horizontal only")
    {
        Scene scene;
        scene.scroll->setDock(DockPosition::None);
        scene.scroll->setScroll(true, false);
        scene.scroll->setBounds(0, 0, 100, 80);
        scene.addRow(300, 200);
        scene.settle();

        CHECK(scene.scroll->getWidth() == 100);
        CHECK(scene.scroll->getHeight() == 80);
        CHECK(scene.verticalBar->hidden());
        CHECK_FALSE(scene.horizontalBar->hidden());
        CHECK_FALSE(scene.horizontalBar->isDisabled());
    }
}

// The wheel scrolls a bar that is shown and enabled; one the ScrollControl
// cannot use goes on to its parent, as Canvas::inputMouseWheel's dispatch to
// the hovered control does through Base::onMouseWheeled.

namespace
{

float scrolled(controls::Base *bar)
{
    return static_cast<controls::BaseScrollBar *>(bar)->getScrolledAmount();
}

//! The bars of a ScrollControl, inner panel first as in Scene.
struct Parts
{
    controls::Base *inner;
    controls::Base *verticalBar;

    explicit Parts(controls::ScrollControl *scroll)
    {
        const controls::Base::List &parts=scroll->getActualChildren();
        REQUIRE(parts.size() == 3);
        auto part=parts.begin();
        inner=*part++;
        verticalBar=*part;
    }
};

}//namespace

TEST_CASE("the wheel does not move a bar that is shown but disabled")
{
    Scene scene;
    scene.scroll->setAutoHideBars(false);
    Block *row=scene.addRow(50, 50);
    scene.settle();

    // the content fits: both bars are shown, and disabled
    REQUIRE(scene.verticalBar->visible());
    REQUIRE(scene.verticalBar->isDisabled());
    REQUIRE(scene.horizontalBar->visible());
    REQUIRE(scene.horizontalBar->isDisabled());

    CHECK_FALSE(row->onMouseWheeled(-60));
    CHECK_FALSE(row->onMouseWheeled(60));
    CHECK(scrolled(scene.verticalBar) == 0);
    CHECK(scrolled(scene.horizontalBar) == 0);
    CHECK(scene.frame().passes == 0);
    CHECK(scene.inner->getY() == 5);
    CHECK(scene.inner->getX() == 5);
}

TEST_CASE("a wheel over a ScrollControl that cannot scroll scrolls the one around it")
{
    Scene scene;
    scene.canvas->setSize(400, 150);
    scene.scroll->setScroll(false, true);

    // a short ScrollControl whose content fits, above a tall row
    controls::ScrollControl *nested=scene.scroll->newChild<controls::ScrollControl>("nested");
    nested->setDock(DockPosition::Top);
    nested->setHeight(60);
    Block *small=nested->newChild<Block>();
    small->want=Size(20, 20);
    small->setDock(DockPosition::Top);
    scene.addRow(50, 400);
    scene.settle();

    Parts nestedParts(nested);
    REQUIRE(nested->getHeight() == 60);
    REQUIRE(nestedParts.verticalBar->hidden());
    REQUIRE_FALSE(scene.verticalBar->hidden());
    REQUIRE_FALSE(scene.verticalBar->isDisabled());

    // down a notch: the nested one cannot, so the outer one scrolls
    CHECK(small->onMouseWheeled(-60));
    CHECK(scrolled(scene.verticalBar) > 0);
    scene.frame();
    CHECK(scene.inner->getY() < 5);
    CHECK(nestedParts.inner->getY() == 5);
}

TEST_CASE("a wheel past the end of a ScrollControl goes on to the one around it")
{
    Scene scene;
    scene.canvas->setSize(400, 150);
    scene.scroll->setScroll(false, true);

    controls::ScrollControl *nested=scene.scroll->newChild<controls::ScrollControl>("nested");
    nested->setDock(DockPosition::Top);
    nested->setScroll(false, true);
    nested->setHeight(60);
    Block *tall=nested->newChild<Block>();
    tall->want=Size(20, 200);
    tall->setDock(DockPosition::Top);
    scene.addRow(50, 400);
    scene.settle();

    Parts nestedParts(nested);
    REQUIRE_FALSE(nestedParts.verticalBar->hidden());
    REQUIRE_FALSE(nestedParts.verticalBar->isDisabled());
    REQUIRE_FALSE(scene.verticalBar->hidden());

    // the nested one scrolls first, while it can
    static_cast<controls::BaseScrollBar *>(nestedParts.verticalBar)->setScrolledAmount(1, true);
    scene.settle();
    REQUIRE(scrolled(nestedParts.verticalBar) == 1);
    REQUIRE(scrolled(scene.verticalBar) == 0);

    // at its bottom, a notch down goes to the outer one
    CHECK(tall->onMouseWheeled(-60));
    CHECK(scrolled(nestedParts.verticalBar) == 1);
    CHECK(scrolled(scene.verticalBar) > 0);

    // a notch up still scrolls the nested one, and leaves the outer one
    const float outer=scrolled(scene.verticalBar);
    CHECK(tall->onMouseWheeled(60));
    CHECK(scrolled(nestedParts.verticalBar) < 1);
    CHECK(scrolled(scene.verticalBar) == outer);

    // the outer one at its top, a notch up over it goes nowhere
    static_cast<controls::BaseScrollBar *>(scene.verticalBar)->setScrolledAmount(0, true);
    static_cast<controls::BaseScrollBar *>(nestedParts.verticalBar)->setScrolledAmount(0, true);
    scene.settle();
    CHECK_FALSE(tall->onMouseWheeled(60));
}

// The viewport: the control less its padding, the bars that show and the inner
// panel's margin. The bars are decided against it, the inner panel sits at its
// origin unscrolled and at least fills it, and scrolled to the end the
// content's far edge is the viewport's.

namespace
{

//! The scroll bar of a ScrollControl, cast for its scrolled amount.
controls::BaseScrollBar *asBar(controls::Base *bar)
{
    return static_cast<controls::BaseScrollBar *>(bar);
}

//! A control's bounds in canvas coordinates.
Rect onCanvas(controls::Base *control)
{
    const Point origin=control->localPosToCanvas();
    return Rect(origin.x, origin.y, control->getWidth(), control->getHeight());
}

}//namespace

TEST_CASE("a ScrollControl at its content size shows the content inside its padding and margin, the same inset on every side")
{
    Scene scene;
    scene.scroll->setDock(DockPosition::None);
    scene.scroll->setPadding(Padding(3, 4, 6, 8));
    Block *row=scene.addRow(60, 70);
    scene.settle();

    // the content, the inner panel's 5 unit margins and the padding
    CHECK(scene.scroll->getWidth() == 60+10+3+6);
    CHECK(scene.scroll->getHeight() == 70+10+4+8);
    CHECK(scene.verticalBar->hidden());
    CHECK(scene.horizontalBar->hidden());

    // the panel at the viewport's origin, the content filling it exactly
    const Rect &viewport=scene.scroll->getViewport();
    CHECK(viewport.x == 3+5);
    CHECK(viewport.y == 4+5);
    CHECK(viewport.w == 60);
    CHECK(viewport.h == 70);
    CHECK(scene.inner->getX() == viewport.x);
    CHECK(scene.inner->getY() == viewport.y);
    CHECK(scene.inner->getWidth() == 60);
    CHECK(scene.inner->getHeight() == 70);
    CHECK(row->getWidth() == 60);
    CHECK(row->getHeight() == 70);
    // nothing left over below or right of the content
    CHECK(scene.scroll->getWidth()-(scene.inner->getX()+scene.inner->getWidth()) == 6+5);
    CHECK(scene.scroll->getHeight()-(scene.inner->getY()+scene.inner->getHeight()) == 8+5);
}

TEST_CASE("a ScrollControl with padding keeps its bars inside it and scrolls its content inside the viewport")
{
    Scene scene;
    scene.scroll->setDock(DockPosition::None);
    scene.scroll->setScroll(false, true);
    scene.scroll->setPadding(Padding(3, 4, 6, 8));
    scene.scroll->setBounds(0, 0, 100, 80);
    scene.addRow(40, 200);
    scene.settle();

    REQUIRE_FALSE(scene.verticalBar->hidden());
    const int barWidth=scene.verticalBar->getWidth();
    CHECK(scene.verticalBar->getX() == 100-6-barWidth);
    CHECK(scene.verticalBar->getY() == 4);
    CHECK(scene.verticalBar->getHeight() == 80-4-8);

    const Rect &viewport=scene.scroll->getViewport();
    CHECK(viewport.x == 8);
    CHECK(viewport.y == 9);
    CHECK(viewport.w == 100-3-6-barWidth-10);
    CHECK(viewport.h == 80-4-8-10);
    CHECK(scene.inner->getX() == 8);
    CHECK(scene.inner->getY() == 9);
    CHECK(scene.inner->getWidth() == viewport.w);
    CHECK(scene.inner->getHeight() == 200);

    asBar(scene.verticalBar)->setScrolledAmount(1, true);
    checkOnePass(scene.frame());
    CHECK(scene.inner->getY()+scene.inner->getHeight() == viewport.y+viewport.h);

    asBar(scene.verticalBar)->setScrolledAmount(0, true);
    checkOnePass(scene.frame());
    CHECK(scene.inner->getY() == viewport.y);
}

TEST_CASE("a bar shows exactly when the content does not fit the viewport")
{
    // 100x80 asked for, so a 90x70 viewport inside the 5 unit margins
    SUBCASE("vertical")
    {
        Scene scene;
        scene.scroll->setDock(DockPosition::None);
        scene.scroll->setScroll(false, true);
        scene.scroll->setBounds(0, 0, 100, 80);
        Block *row=scene.addRow(40, 70);
        scene.settle();
        CHECK(scene.verticalBar->hidden());
        CHECK(scene.scroll->getViewport().h == 70);
        CHECK(scene.inner->getHeight() == 70);

        row->want=Size(40, 71);
        row->invalidate();
        checkOnePass(scene.frame());
        CHECK_FALSE(scene.verticalBar->hidden());
        CHECK_FALSE(scene.verticalBar->isDisabled());
        CHECK(scene.inner->getHeight() == 71);

        row->want=Size(40, 70);
        row->invalidate();
        checkOnePass(scene.frame());
        CHECK(scene.verticalBar->hidden());
    }

    SUBCASE("horizontal")
    {
        Scene scene;
        scene.scroll->setDock(DockPosition::None);
        scene.scroll->setScroll(true, false);
        scene.scroll->setBounds(0, 0, 100, 80);
        Block *row=scene.addRow(90, 20);
        scene.settle();
        CHECK(scene.horizontalBar->hidden());
        CHECK(scene.scroll->getViewport().w == 90);
        CHECK(scene.inner->getWidth() == 90);

        row->want=Size(91, 20);
        row->invalidate();
        checkOnePass(scene.frame());
        CHECK_FALSE(scene.horizontalBar->hidden());
        CHECK_FALSE(scene.horizontalBar->isDisabled());
        CHECK(scene.inner->getWidth() == 91);

        row->want=Size(90, 20);
        row->invalidate();
        checkOnePass(scene.frame());
        CHECK(scene.horizontalBar->hidden());
    }
}

TEST_CASE("scrolled to either end, the content's edge is the viewport's")
{
    SUBCASE("vertical")
    {
        Scene scene;
        scene.scroll->setDock(DockPosition::None);
        scene.scroll->setScroll(false, true);
        scene.scroll->setBounds(0, 0, 100, 80);
        scene.addRow(40, 300);
        scene.settle();
        REQUIRE_FALSE(scene.verticalBar->hidden());
        const Rect viewport=scene.scroll->getViewport();
        CHECK(viewport.y == 5);
        CHECK(viewport.h == 70);
        CHECK(scene.inner->getY() == 5);

        asBar(scene.verticalBar)->setScrolledAmount(1, true);
        checkOnePass(scene.frame());
        CHECK(scene.inner->getY() == 5+70-300);
        CHECK(scene.inner->getY()+scene.inner->getHeight() == viewport.y+viewport.h);

        asBar(scene.verticalBar)->setScrolledAmount(0, true);
        checkOnePass(scene.frame());
        CHECK(scene.inner->getY() == 5);
    }

    SUBCASE("horizontal")
    {
        Scene scene;
        scene.scroll->setDock(DockPosition::None);
        scene.scroll->setScroll(true, false);
        scene.scroll->setBounds(0, 0, 100, 80);
        scene.addRow(300, 20);
        scene.settle();
        REQUIRE_FALSE(scene.horizontalBar->hidden());
        const Rect viewport=scene.scroll->getViewport();
        CHECK(viewport.x == 5);
        CHECK(viewport.w == 90);
        CHECK(viewport.h == 70-scene.horizontalBar->getHeight());
        CHECK(scene.inner->getX() == 5);
        CHECK(scene.inner->getWidth() == 300);

        asBar(scene.horizontalBar)->setScrolledAmount(1, true);
        checkOnePass(scene.frame());
        CHECK(scene.inner->getX() == 5+90-300);
        CHECK(scene.inner->getX()+scene.inner->getWidth() == viewport.x+viewport.w);

        asBar(scene.horizontalBar)->setScrolledAmount(0, true);
        checkOnePass(scene.frame());
        CHECK(scene.inner->getX() == 5);
    }
}

TEST_CASE("a MenuStrip keeps its items on one row, the strip's full height, with no bar")
{
    Scene scene;
    controls::MenuStrip *strip=scene.canvas->newChild<controls::MenuStrip>();
    strip->addItem("File");
    strip->addItem("Edit");
    strip->addItem("A rather long caption");
    scene.settle();

    Parts parts(strip);
    const controls::Base::List &items=parts.inner->getChildren();
    REQUIRE(items.size() == 3);

    // at least the 22 it asks for, and as high as an item
    CHECK(strip->getHeight() >= 22);
    CHECK(parts.verticalBar->hidden());
    CHECK(strip->getActualChildren().back()->hidden());
    CHECK(parts.inner->getX() == 0);
    CHECK(parts.inner->getY() == 0);
    CHECK(parts.inner->getHeight() == strip->getHeight());

    // one row, 5 in from the left, each item the strip's height with its
    // caption StripItemInset from the top
    int x=5;
    for(controls::Base *item : items)
    {
        auto *menuItem=static_cast<controls::MenuItem *>(item);
        CHECK(item->getX() == x);
        CHECK(item->getY() == 0);
        CHECK(item->getHeight() == strip->getHeight());
        CHECK(menuItem->textHeight()+2*controls::MenuStrip::StripItemInset == strip->getHeight());
        CHECK(menuItem->textY() == controls::MenuStrip::StripItemInset);
        x+=item->getWidth();
    }
    CHECK(scene.frame().passes == 0);
}

TEST_CASE("a Menu at its content size has the same gap above its first item as below its last")
{
    Scene scene;
    controls::Menu *menu=scene.canvas->newChild<controls::Menu>();
    menu->addItem("One");
    menu->addItem("A longer item");
    menu->addItem("Three");
    scene.settle();

    Parts parts(menu);
    REQUIRE(parts.verticalBar->hidden());
    const controls::Base::List &items=parts.inner->getChildren();
    REQUIRE(items.size() == 3);
    const Rect box=onCanvas(menu);
    const Rect first=onCanvas(items.front());
    const Rect last=onCanvas(items.back());

    CHECK(first.y-box.y > 0);
    CHECK(first.y-box.y == (box.y+box.h)-(last.y+last.h));
    CHECK(first.x-box.x > 0);
    CHECK(first.x-box.x == (box.x+box.w)-(first.x+first.w));
}

TEST_CASE("a ListBox at its content size has the same gap left of its rows as right of them")
{
    Scene scene;
    controls::ListBox *list=scene.canvas->newChild<controls::ListBox>();
    list->addItem("One");
    list->addItem("A much wider row than the others");
    list->addItem("Three");
    scene.settle();

    Parts parts(list);
    REQUIRE(parts.verticalBar->hidden());
    const Rect box=onCanvas(list);
    const Rect widest=onCanvas(list->getTable()->getChildren().front());
    const Rect last=onCanvas(list->getTable()->getChildren().back());

    // the inner panel's 5 unit margin and its 2 unit padding
    CHECK(widest.x-box.x == 5+2);
    CHECK((box.x+box.w)-(widest.x+widest.w) == 5+2);
    CHECK(widest.y-box.y == 5+2);
    CHECK((box.y+box.h)-(last.y+last.h) == 5+2);
}

TEST_CASE("a TreeNode maps to the canvas through its TreeControl's ScrollControl's inner panel")
{
    Scene scene;
    controls::TreeControl *tree=scene.canvas->newChild<controls::TreeControl>();
    tree->setBounds(20, 30, 200, 200);
    controls::TreeNode *first=tree->addNode("First");
    controls::TreeNode *second=tree->addNode("Second");
    second->addNode("Child");
    scene.settle();

    // the TreeControl's ScrollControl, 1 in, and its inner panel 5 in that
    const Point origin=second->localPosToCanvas();
    CHECK(origin.x == 20+1+5+second->getX());
    CHECK(origin.y == 30+1+5+second->getY());
    CHECK(second->getY() == first->getHeight());
    CHECK(second->canvasPosToLocal(origin).x == 0);
    CHECK(second->canvasPosToLocal(origin).y == 0);

    // the point it maps to is the control hit-testing finds there
    const Point inside=second->localPosToCanvas(Point(second->getWidth()-2, 2));
    controls::Base *hit=scene.canvas->getControlAt(inside.x, inside.y, false);
    controls::Base *up=hit;

    while(up && up != second)
        up=up->getParent();

    CHECK(up == second);
}
