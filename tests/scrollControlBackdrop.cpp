/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The ScrollControl subclasses with a renderer of their own (ListBox,
 *  CollapsibleList, Menu) draw it: with their surfaces set to solid test
 *  colours, every pixel of the control outside its children and its rounded
 *  corners is the fill, and its edge outside its children the border. Each
 *  takes one primitive slot, a settled frame writes none, a Menu hidden and
 *  shown again keeps its slot and draws the same, and a deleted Menu gives
 *  every id back. ScrollControl::render used to be an empty override that
 *  skipped Base::render, so none of these backdrops was ever drawn.
 *
 *  A ListBox scrolled by half a row clips its rows at its viewport: every
 *  pixel of the padding and margin band around the viewport, outside the
 *  bar, is the backdrop (the border on the edge, the fill inside it), with a
 *  selected row half above the viewport and rows below it.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/collapsibleList.h>
#include <gweni/controls/listBox.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/scrollBar.h>
#include <gweni/primitiveIds.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <cstdlib>
#include <string>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Rect;
namespace tokyonight=gweni::skin::textured::tokyonight;

const Color TestFill(200, 30, 40, 255);
const Color TestBorder(30, 200, 60, 255);

//! One frame read back from the offscreen framebuffer, top-down RGBA8.
struct Frame
{
    std::vector<uint8_t> pixels;

    Color at(int x, int y) const
    {
        const uint8_t *p=&pixels[(static_cast<size_t>(y)*gweni::test::viewSize().x+x)*4];

        return Color(p[0], p[1], p[2], p[3]);
    }
};

bool near(const Color &a, const Color &b, int tolerance=2)
{
    return std::abs(a.r-b.r) <= tolerance && std::abs(a.g-b.g) <= tolerance &&
        std::abs(a.b-b.b) <= tolerance;
}

//! Sets a surface to the test colours, and puts it back.
struct SurfaceOverride
{
    tokyonight::Surface &surface;
    tokyonight::Surface saved;

    explicit SurfaceOverride(tokyonight::Surface &target):
        surface(target), saved(target)
    {
        surface.top=TestFill;
        surface.bottom=TestFill;
        surface.border=TestBorder;
    }

    ~SurfaceOverride() { surface=saved; }
};

size_t liveIds()
{
    return gweni::primitiveIdCount()-gweni::releasedPrimitiveIdCount();
}

Rect canvasRect(gweni::controls::Base *control)
{
    const gweni::Point at=control->localPosToCanvas(gweni::Point(0, 0));

    return Rect(at.x, at.y, control->getWidth(), control->getHeight());
}

//! The canvas rects of the visible descendants that draw, each grown by a
//! pixel for its anti-aliased edge.
void drawnChildren(gweni::controls::Base *control, std::vector<Rect> &rects)
{
    for(gweni::controls::Base *child : control->getChildren())
    {
        if(child->hidden())
            continue;

        if(child->getSkinControl())
        {
            const Rect r=canvasRect(child);

            rects.emplace_back(r.x-1, r.y-1, r.w+2, r.h+2);
        }
        drawnChildren(child, rects);
    }
}

bool inside(const std::vector<Rect> &rects, int x, int y)
{
    for(const Rect &r : rects)
    {
        if(x >= r.x && x < r.x+r.w && y >= r.y && y < r.y+r.h)
            return true;
    }
    return false;
}

//! Every pixel of \p control's rect outside its drawn children and away from
//! its rounded corners: the border colour on its outermost ring, the fill two
//! pixels in and further. Counts what it checked, so a control covered by its
//! children cannot pass by checking nothing.
void checkBackdrop(const Frame &shot, gweni::controls::Base *control, float radius)
{
    const std::string type=control->getTypeName();

    CAPTURE(type);
    const Rect r=canvasRect(control);
    std::vector<Rect> children;

    drawnChildren(control, children);

    const gweni::Point view=gweni::test::viewSize();

    REQUIRE(r.x >= 0);
    REQUIRE(r.y >= 0);
    REQUIRE(r.x+r.w <= view.x);
    REQUIRE(r.y+r.h <= view.y);

    const int corner=static_cast<int>(radius)+2;
    size_t fillChecked=0;
    size_t fillWrong=0;
    size_t borderChecked=0;
    size_t borderWrong=0;

    for(int y=r.y; y < r.y+r.h; ++y)
    {
        for(int x=r.x; x < r.x+r.w; ++x)
        {
            const int dx=std::min(x-r.x, r.x+r.w-1-x);
            const int dy=std::min(y-r.y, r.y+r.h-1-y);

            if((dx < corner && dy < corner) || inside(children, x, y))
                continue;

            const Color c=shot.at(x, y);

            if(dx == 0 || dy == 0)
            {
                ++borderChecked;
                borderWrong+=near(c, TestBorder) ? 0 : 1;
            }
            else if(dx >= 2 && dy >= 2)
            {
                ++fillChecked;
                fillWrong+=near(c, TestFill) ? 0 : 1;
            }
        }
    }

    CHECK(fillChecked >= 50);
    CHECK(fillWrong == 0);
    CHECK(borderChecked >= 10);
    CHECK(borderWrong == 0);
}

size_t slots(gweni::controls::Base *control)
{
    const gweni::skin::Control *skinControl=control->getSkinControl();

    return skinControl ? skinControl->getPrimitives().size() : 0;
}

//! A textured-skin canvas over the whole view: a ListBox, a CollapsibleList
//! and a Menu side by side, each shorter content than the control.
struct Scene
{
    gweni::skin::textured::TexturedSkin *skin;
    gweni::controls::Canvas *canvas;
    gweni::controls::ListBox *listBox;
    gweni::controls::CollapsibleList *collapsible;
    gweni::controls::Menu *menu;

    Scene()
    {
        const gweni::Point viewSize=gweni::test::viewSize();

        skin=new gweni::skin::textured::TexturedSkin(&gweni::test::gpuRenderer());
        REQUIRE(skin->init("DefaultSkin.json"));
        skin->setDefaultFont("OpenSans.ttf", 11);

        canvas=new gweni::controls::Canvas(skin);
        canvas->setSize(viewSize.x, viewSize.y);

        listBox=canvas->newChild<gweni::controls::ListBox>();
        listBox->setPos(5, 5);
        listBox->setSize(130, 140);
        listBox->addItem("First");
        listBox->addItem("Second");
        listBox->addItem("Third");

        collapsible=canvas->newChild<gweni::controls::CollapsibleList>();
        collapsible->setPos(145, 5);
        collapsible->setSize(130, 140);
        gweni::controls::CollapsibleCategory *category=collapsible->add("Category");
        category->add("One");
        category->add("Two");

        menu=canvas->newChild<gweni::controls::Menu>();
        menu->setPos(285, 5);
        menu->addItem("New");
        menu->addItem("Open");
        menu->addDivider();
        menu->addItem("Quit");
    }

    ~Scene()
    {
        delete canvas;
        delete skin;

        gweni::renderer::SlotRenderer &renderer=gweni::test::gpuRenderer();

        renderer.begin();
        renderer.end();
    }

    Frame frame()
    {
        gweni::test::clearView(gweni::Color(0, 0, 0, 255));
        canvas->updateCanvas();
        canvas->renderCanvas();

        Frame shot;

        gweni::test::readView(shot.pixels);
        return shot;
    }

    size_t slotsWritten() const
    {
        return gweni::test::gpuRenderer().getFrameStats().primitiveSlotsWritten;
    }
};

}//namespace

TEST_CASE("ScrollControl subclasses: ListBox, CollapsibleList and Menu draw their backdrops")
{
    SurfaceOverride panelDark(tokyonight::PanelDark);
    SurfaceOverride categoryList(tokyonight::CategoryList);
    SurfaceOverride menuSurface(tokyonight::Menu);
    Scene scene;

    scene.frame();
    Frame shot=scene.frame();

    for(gweni::controls::Base *control :
        {static_cast<gweni::controls::Base *>(scene.listBox),
         static_cast<gweni::controls::Base *>(scene.collapsible),
         static_cast<gweni::controls::Base *>(scene.menu)})
    {
        const std::string type=control->getTypeName();

        CAPTURE(type);
        CHECK(slots(control) == 1);
    }

    checkBackdrop(shot, scene.listBox, tokyonight::PanelDark.radius);
    checkBackdrop(shot, scene.collapsible, tokyonight::CategoryList.radius);
    checkBackdrop(shot, scene.menu, tokyonight::Menu.radius);

    // a settled frame writes nothing
    scene.frame();
    CHECK(scene.slotsWritten() == 0);
}

TEST_CASE("ScrollControl subclasses: a Menu hidden and shown keeps its slot, and gives its ids back when deleted")
{
    SurfaceOverride menuSurface(tokyonight::Menu);
    Scene scene;

    scene.frame();
    const Frame open=scene.frame();
    const size_t live=liveIds();

    scene.menu->setHidden(true);
    scene.frame();
    CHECK(slots(scene.menu) == 1);
    CHECK(liveIds() == live);
    scene.frame();
    CHECK(scene.slotsWritten() == 0);

    scene.menu->setHidden(false);
    scene.frame();
    const Frame reopened=scene.frame();

    CHECK(scene.slotsWritten() == 0);
    CHECK(liveIds() == live);
    CHECK(reopened.pixels == open.pixels);
    checkBackdrop(reopened, scene.menu, tokyonight::Menu.radius);

    // the ids the Menu and its items hold go back to the bank with it, and
    // the same Menu built again takes as many
    scene.menu->delayedDelete();
    scene.menu=nullptr;
    scene.frame();
    CHECK(liveIds() < live);

    const size_t without=liveIds();
    gweni::controls::Menu *again=scene.canvas->newChild<gweni::controls::Menu>();

    again->setPos(285, 5);
    again->addItem("New");
    again->addItem("Open");
    again->addDivider();
    again->addItem("Quit");
    scene.frame();
    CHECK(liveIds() == live);

    again->delayedDelete();
    scene.frame();
    CHECK(liveIds() == without);
}

TEST_CASE("ScrollControl subclasses: a ListBox scrolled by half a row draws only its backdrop around its viewport")
{
    SurfaceOverride panelDark(tokyonight::PanelDark);
    Scene scene;
    gweni::controls::ListBox *list=scene.listBox;

    list->setSize(130, 100);
    for(int i=0; i<12; ++i)
        list->addItem("Row "+std::to_string(i));
    // a selected row paints its highlight right up to the viewport's edge
    list->setSelectedRow(list->getTable()->getRow(0));

    scene.frame();
    scene.frame();

    gweni::controls::BaseScrollBar *bar=nullptr;

    for(gweni::controls::Base *part : list->getActualChildren())
    {
        if(part != list->getInnerPanel() && part->getDock() == gweni::DockPosition::Right)
            bar=static_cast<gweni::controls::BaseScrollBar *>(part);
    }
    REQUIRE(bar);
    REQUIRE_FALSE(bar->hidden());

    // the first row starts 2 into the inner panel (the ListBox's padding
    // inside it): scrolled by that and half its height, its top half is
    // above the viewport
    const Rect &viewport=list->getViewport();
    gweni::controls::Base *first=list->getTable()->getRow(0);
    const int scrolled=first->getY()+first->getHeight()/2;
    const int range=list->getInnerPanel()->getHeight()-viewport.h;

    REQUIRE(range > scrolled);
    bar->setScrolledAmount(static_cast<float>(scrolled)/range, true);
    scene.frame();
    const Frame shot=scene.frame();

    REQUIRE(list->getInnerPanel()->getY() == viewport.y-scrolled);
    // and a row reaches past the viewport's bottom too
    REQUIRE(list->getInnerPanel()->getY()+list->getInnerPanel()->getHeight() > viewport.y+viewport.h);

    const Rect r=canvasRect(list);
    const Rect vp(r.x+viewport.x, r.y+viewport.y, viewport.w, viewport.h);
    const Rect b=canvasRect(bar);
    const std::vector<Rect> excluded={vp, Rect(b.x-1, b.y-1, b.w+2, b.h+2)};
    const int corner=static_cast<int>(tokyonight::PanelDark.radius)+2;
    size_t fillChecked=0;
    size_t fillWrong=0;
    size_t borderChecked=0;
    size_t borderWrong=0;
    size_t bandAboveChecked=0;

    for(int y=r.y; y < r.y+r.h; ++y)
    {
        for(int x=r.x; x < r.x+r.w; ++x)
        {
            const int dx=std::min(x-r.x, r.x+r.w-1-x);
            const int dy=std::min(y-r.y, r.y+r.h-1-y);

            if((dx < corner && dy < corner) || inside(excluded, x, y))
                continue;

            const Color c=shot.at(x, y);

            if(dx == 0 || dy == 0)
            {
                ++borderChecked;
                borderWrong+=near(c, TestBorder) ? 0 : 1;
            }
            else if(dx >= 2 && dy >= 2)
            {
                ++fillChecked;
                fillWrong+=near(c, TestFill) ? 0 : 1;
                // the band over the selected row's hidden half
                if(y < vp.y && x >= vp.x && x < vp.x+vp.w)
                    ++bandAboveChecked;
            }
        }
    }

    CHECK(fillChecked >= 400);
    CHECK(fillWrong == 0);
    CHECK(borderChecked >= 200);
    CHECK(borderWrong == 0);
    CHECK(bandAboveChecked >= static_cast<size_t>(2*vp.w));

    // the row's visible half is drawn, right down from the viewport's top
    CHECK_FALSE(near(shot.at(vp.x+vp.w/2, vp.y), TestFill));
}
