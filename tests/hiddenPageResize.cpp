/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A hidden page resized with its TabControl, drawn with the textured skin on
 *  the backend under test (#578). A new size invalidates only the children a
 *  control holds, so a page hidden as a TabControl leaves the pages switched
 *  away from is marked by its TabControl's resize, and nothing under it is
 *  laid out until it is shown. The page holds nested containers (plain panels
 *  and ScrollControls docked Center, a Label docked Top in each), Labels and a
 *  TreeControl of open TreeNodes. Drawn shown at the view's size, hidden, its
 *  canvas resized to 3/4 of the view and then the page shown again, every
 *  control's bounds and every pixel of the view are those of a canvas built
 *  at 3/4 of the view with the page shown, at canvas scales 1 and 1.5. The
 *  layout alone is checked headless in tests/unit/sizeInvalidation.cpp.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/label.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/tabButton.h>
#include <gweni/controls/tabControl.h>
#include <gweni/controls/treeControl.h>
#include <gweni/controls/treeNode.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <cmath>
#include <string>
#include <vector>

namespace
{

using gweni::Color;
using gweni::DockPosition;
using gweni::Point;
namespace controls=gweni::controls;

//! The page's content: \a depth containers docked Center, odd levels
//! ScrollControls scrolling vertically only, a Label docked Top in each; the
//! innermost holds Labels and a TreeControl docked Bottom.
void buildNest(controls::Base *parent, int depth)
{
    for(int level=0; level<depth; ++level)
    {
        controls::Base *container;

        if(level%2 == 1)
        {
            controls::ScrollControl *scroll=parent->newChild<controls::ScrollControl>();
            scroll->setScroll(false, true);
            container=scroll;
        }
        else
            container=parent->newChild<controls::Base>();

        container->setDock(DockPosition::Center);
        container->setPadding(gweni::Padding(4, 4, 4, 4));
        controls::Label *label=container->newChild<controls::Label>();
        label->setDock(DockPosition::Top);
        label->setText("level "+std::to_string(level));
        parent=container;
    }

    for(int i=0; i<6; ++i)
    {
        controls::Label *label=parent->newChild<controls::Label>();
        label->setDock(DockPosition::Top);
        label->setText("leaf "+std::to_string(i));
    }

    controls::TreeControl *tree=parent->newChild<controls::TreeControl>();
    tree->setDock(DockPosition::Bottom);

    controls::TreeNode *node=tree->addNode("root");
    for(int level=0; level<3; ++level)
    {
        node->addNode("leaf");
        controls::TreeNode *next=node->addNode("level");
        node->open();
        node=next;
    }
    node->open();
}

//! A textured-skin canvas at \p scale, \p size logical units, holding a
//! TabControl filling it: a first page with a Label, a second with the nest.
struct Scene
{
    gweni::skin::textured::TexturedSkin *skin;
    controls::Canvas *canvas;
    controls::TabControl *tabs;

    Scene(float scale, const Point &size)
    {
        skin=new gweni::skin::textured::TexturedSkin(&gweni::test::gpuRenderer());
        REQUIRE(skin->init("DefaultSkin.json"));
        skin->setDefaultFont("OpenSans.ttf", 11);

        canvas=new controls::Canvas(skin);
        canvas->setScale(scale);
        canvas->setSize(size.x, size.y);

        tabs=canvas->newChild<controls::TabControl>();
        tabs->setDock(DockPosition::Center);
        tabs->addPage("First")->getPage()->newChild<controls::Label>()->setText("first page");
        buildNest(tabs->addPage("Second")->getPage(), 6);
    }

    ~Scene()
    {
        delete canvas;
        delete skin;

        // scale 1 for the next Scene, as in tests/tabJoin.cpp: its skin loads
        // its font at the renderer's scale
        gweni::renderer::SlotRenderer &renderer=gweni::test::gpuRenderer();

        renderer.setScale(1.0f);
        renderer.begin();
        renderer.end();
    }

    std::vector<uint8_t> settle()
    {
        std::vector<uint8_t> pixels;

        for(int i=0; i < 3; ++i)
        {
            gweni::test::clearView(Color(0, 0, 0, 255));
            canvas->updateCanvas();
            canvas->renderCanvas();
        }
        gweni::test::readView(pixels);
        return pixels;
    }
};

//! The shown controls under \p control, pre-order: type and bounds.
std::string shownListing(controls::Base *control)
{
    if(control->hidden())
        return std::string();

    const gweni::Rect &b=control->getBounds();
    std::string out=std::string(control->getTypeName())+" "+std::to_string(b.x)+","+std::to_string(b.y)+" "+
        std::to_string(b.w)+"x"+std::to_string(b.h)+"\n";

    for(controls::Base *child : control->getActualChildren())
        out+=shownListing(child);
    return out;
}

size_t differingPixels(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b)
{
    size_t count=0;

    for(size_t i=0; i+3 < a.size() && i+3 < b.size(); i+=4)
    {
        if(a[i] != b[i] || a[i+1] != b[i+1] || a[i+2] != b[i+2])
            ++count;
    }
    return count;
}

}//namespace

TEST_CASE("Hidden page resize: a page resized while hidden draws when shown as one built shown at that size")
{
    const Point view=gweni::test::viewSize();

    for(const float scale : {1.0f, 1.5f})
    {
        CAPTURE(scale);

        const Point full(int(std::lround(view.x/scale)), int(std::lround(view.y/scale)));
        const Point small(full.x*3/4, full.y*3/4);

        std::vector<uint8_t> resized, built;
        std::string resizedListing, builtListing;

        {
            Scene scene(scale, full);

            // drawn shown, then hidden as a TabControl leaves a page switched away from
            scene.tabs->getTab(1)->doAction();
            scene.settle();
            scene.tabs->getTab(0)->doAction();
            scene.settle();
            REQUIRE(scene.tabs->getTab(1)->getPage()->hidden());

            // resized while hidden, then shown
            scene.canvas->setSize(small.x, small.y);
            scene.settle();
            scene.tabs->getTab(1)->doAction();
            resized=scene.settle();
            resizedListing=shownListing(scene.canvas);
        }
        {
            Scene scene(scale, small);

            scene.tabs->getTab(1)->doAction();
            built=scene.settle();
            builtListing=shownListing(scene.canvas);
        }

        CHECK(resizedListing == builtListing);
        REQUIRE(resized.size() == built.size());
        CHECK(differingPixels(resized, built) == 0);
    }
}
