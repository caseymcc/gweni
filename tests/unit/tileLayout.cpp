/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  layout::Tile on the StubRenderer: its preferred height is its rows at its
 *  arranged width times the tile height, plus its padding (rows: the shown
 *  children over the cells a row holds, floor of the content width over the
 *  tile width, at least 1); hidden children take no cell. Docked Top in a vertical ScrollControl with a stable gutter,
 *  its whole height scrolls (the bar's range ends at its last row, which a
 *  scroll to the bottom brings into the viewport), a cell added after the
 *  canvas settled lays out in one pass and lengthens the range by a row when
 *  it starts one, and a canvas resized to fewer columns re-rows it in one
 *  pass, as a canvas built at that size lays it out.
 */
#include "headlessFixture.h"

#include <gweni/controls/layout/tile.h>
#include <gweni/controls/scrollControl.h>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace gweni;
using controls::layout::Tile;

namespace
{

constexpr int TileW=60;
constexpr int TileH=50;

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    controls::ScrollControl *scroll;
    Tile *tile;
    std::vector<controls::Base *> cells;

    Scene(int width, int count)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(width, 200);
        scroll=canvas->newChild<controls::ScrollControl>();
        scroll->setDock(DockPosition::Center);
        scroll->setScroll(false, true);
        scroll->setAutoHideBars(false);
        tile=scroll->newChild<Tile>();
        tile->setDock(DockPosition::Top);
        tile->setTileSize(TileW, TileH);
        tile->setPadding(Padding(4, 4, 4, 4));
        for(int i=0; i < count; ++i)
            addCell();
    }

    ~Scene()
    {
        delete canvas;
    }

    controls::Base *addCell()
    {
        controls::Base *cell=tile->newChild<controls::Base>();

        cell->setSize(TileW-4, TileH-4);
        cells.push_back(cell);
        return cell;
    }

    const controls::LayoutStats &frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
        return canvas->getLayoutStats();
    }

    void settle()
    {
        for(int i=0; i < 10 && frame().passes != 0; ++i)
            ;
    }

    //! The cells a row holds at the tile's width now
    int columns() const
    {
        return std::max(1, (tile->getWidth()-8)/TileW);
    }

    int expectedHeight() const
    {
        const int rows=((int)cells.size()+columns()-1)/columns();

        return rows*TileH+8;
    }

    std::vector<std::string> bounds() const
    {
        std::vector<std::string> out;

        auto rect=[](const controls::Base *control)
        {
            const Rect &r=control->getBounds();

            return std::to_string(r.x)+","+std::to_string(r.y)+" "+std::to_string(r.w)+"x"+std::to_string(r.h);
        };
        out.push_back(rect(tile));
        for(const controls::Base *cell:cells)
            out.push_back(rect(cell));
        out.push_back(std::to_string(scroll->getMaxVerticalScrollOffset()));
        return out;
    }
};

void checkOnePass(const controls::LayoutStats &stats)
{
    INFO("re-dirtied by " << std::string(stats.redirtiedType?stats.redirtiedType:"none")
        << "#" << stats.redirtiedId);
    CHECK(stats.passes == 1);
    CHECK(stats.redirtiedType == nullptr);
    CHECK_FALSE(stats.renderLayout);
}

}//namespace

TEST_CASE("Tile: its preferred height is its rows at its width, a hidden child taking no cell")
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas canvas(&skin);

    canvas.setSize(400, 600);

    Tile *tile=canvas.newChild<Tile>();

    tile->setDock(DockPosition::Top);
    tile->setTileSize(50, 40);
    tile->setPadding(Padding(5, 5, 5, 5));
    // 220 wide: a content width of 210 holds 4 cells
    tile->setSizeFlags({SizeFlag::Fixed, SizeFlag::Shrink});
    tile->setWidth(220);

    std::vector<controls::Base *> cells;

    for(int i=0; i < 10; ++i)
    {
        cells.push_back(tile->newChild<controls::Base>());
        cells.back()->setSize(30, 30);
    }
    canvas.updateCanvas();
    canvas.updateCanvas();

    // 10 cells in rows of 4: 3 rows
    CHECK(tile->getPreferredSize().height == 3*40+10);
    CHECK(tile->getHeight() == 3*40+10);
    // the last cell in the third row, centred in its cell
    CHECK(cells.back()->getY() == 5+2*40+5);

    // two hidden: 8 cells, 2 rows
    cells[0]->hide();
    cells[1]->hide();
    canvas.updateCanvas();
    canvas.updateCanvas();
    CHECK(tile->getHeight() == 2*40+10);
}

TEST_CASE("Tile: in a stable-gutter ScrollControl its last row scrolls into view, and a new cell lays out in one pass")
{
    Scene scene(300, 23);

    scene.settle();

    const int columns=scene.columns();

    REQUIRE(columns >= 2);
    CHECK(scene.tile->getHeight() == scene.expectedHeight());
    // the range reaches the tile's last row
    CHECK(scene.scroll->getMaxVerticalScrollOffset() > 0);
    CHECK(scene.scroll->getMaxVerticalScrollOffset() ==
        scene.tile->getHeight()-scene.scroll->getViewportHeight());

    scene.scroll->scrollToBottom();
    scene.settle();

    const controls::Base *last=scene.cells.back();
    const int lastBottom=last->getY()+last->getHeight()+scene.tile->getY()-scene.scroll->getVerticalScrollOffset();

    CHECK(lastBottom <= scene.scroll->getViewportHeight());
    CHECK(lastBottom > 0);

    // cells up to a full row, then one that starts a row: one pass each, the
    // range a row longer for the second
    while(scene.cells.size()%columns != 0)
    {
        scene.addCell();
        checkOnePass(scene.frame());
    }

    const int before=scene.scroll->getMaxVerticalScrollOffset();

    scene.addCell();
    checkOnePass(scene.frame());
    CHECK(scene.tile->getHeight() == scene.expectedHeight());
    CHECK(scene.scroll->getMaxVerticalScrollOffset() == before+TileH);
}

TEST_CASE("Tile: a resize to fewer columns re-rows it in one pass, as a canvas built at that size")
{
    Scene resized(420, 17);

    resized.settle();

    const int wide=resized.columns();

    resized.canvas->setSize(260, 200);
    checkOnePass(resized.frame());
    CHECK(resized.columns() < wide);
    CHECK(resized.tile->getHeight() == resized.expectedHeight());

    Scene built(260, 17);

    built.settle();
    CHECK(resized.bounds() == built.bounds());
}
