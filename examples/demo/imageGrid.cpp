/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include "demo/imageGrid.h"
#include "gweni/controls/button.h"
#include "gweni/controls/groupBox.h"
#include "gweni/controls/imagePanel.h"
#include "gweni/controls/layout/tile.h"
#include "gweni/controls/scrollControl.h"

#include <string>

namespace gweni
{

namespace
{

#include "demo/imageGridImages.inc"

struct EmbeddedImage
{
    const char *caption;
    const unsigned char *bytes;
    size_t size;
};

const EmbeddedImage Images[]=
{
    { "Sunset PNG", g_imageSunset, sizeof(g_imageSunset) },
    { "Tower JPEG", g_imageTower, sizeof(g_imageTower) },
    { "Badge PNG (alpha)", g_imageBadge, sizeof(g_imageBadge) },
    { "Wide JPEG", g_imageWide, sizeof(g_imageWide) },
    { "Checker PNG 64x64", g_imageChecker, sizeof(g_imageChecker) },
    { "Rings JPEG", g_imageRings, sizeof(g_imageRings) },
};
constexpr size_t ImageCount=sizeof(Images)/sizeof(Images[0]);

const EmbeddedImage Replacement={ "Swap JPEG", g_imageSwap, sizeof(g_imageSwap) };

// What the bad-data cell is given: bytes that are no image
const unsigned char NotAnImage[]="<html>404 not found</html>";

}//namespace

GWENI_CONTROL_CONSTRUCTOR(ImageGridDemo)
{
    setDock(DockPosition::Center);

    controls::Base *bar=newChild<controls::Base>();

    bar->setDock(DockPosition::Top);
    bar->setHeight(28);
    bar->setPadding(Padding(4, 4, 4, 4));

    m_replace=bar->newChild<controls::Button>();
    m_replace->setText("Replace");
    m_replace->setDock(DockPosition::Left);
    m_replace->setWidth(90);
    m_replace->onPressCaller.add(this, &ImageGridDemo::onReplace);

    m_free=bar->newChild<controls::Button>();
    m_free->setText("Free");
    m_free->setDock(DockPosition::Left);
    m_free->setWidth(90);
    m_free->setMargin(Margin(4, 0, 0, 0));
    m_free->onPressCaller.add(this, &ImageGridDemo::onFree);

    m_scroll=newChild<controls::ScrollControl>();
    m_scroll->setDock(DockPosition::Center);
    m_scroll->setScroll(false, true);
    m_scroll->setAutoHideBars(false);
    // a requested height (the fill dock ignores it), so the ScrollControl's
    // preferred height, and its ancestors', is not the Tile's at the width
    // it had before a resize
    m_scroll->setHeight(200);

    m_tile=m_scroll->newChild<controls::layout::Tile>();
    m_tile->setDock(DockPosition::Top);
    m_tile->setTileSize(CellWidth+6, CellHeight+6);
    m_tile->setPadding(Padding(4, 4, 4, 4));

    for(size_t cell=0; cell <= BadDataCell; ++cell)
    {
        controls::GroupBox *box=m_tile->newChild<controls::GroupBox>();

        box->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        box->setSize(CellWidth, CellHeight);

        controls::ImagePanel *panel=box->newChild<controls::ImagePanel>();

        panel->setDock(DockPosition::Center);
        panel->setScaleMode(controls::ImageScaleMode::Fit);
        m_cells.push_back(box);
        m_panels.push_back(panel);

        if(cell == EmptyCell)
            box->setText("Empty");
        else if(cell == BadDataCell)
        {
            box->setText("Bad data");
            panel->setImageData(NotAnImage, sizeof(NotAnImage)-1, MaxImageSize);
        }
        else
            loadCell(cell, cell%ImageCount);
    }
}

void ImageGridDemo::loadCell(size_t cell, size_t image)
{
    const EmbeddedImage &embedded=(image < ImageCount)?Images[image]:Replacement;

    m_cells[cell]->setText(embedded.caption);
    m_panels[cell]->setImageData(embedded.bytes, embedded.size, MaxImageSize);
}

void ImageGridDemo::onReplace(event::Info)
{
    setReplaced(!m_replaced);
}

void ImageGridDemo::onFree(event::Info)
{
    setFreed(!m_freed);
}

void ImageGridDemo::setReplaced(bool replaced)
{
    if(m_replaced == replaced)
        return;
    m_replaced=replaced;
    loadCell(0, replaced?ImageCount:0);
}

void ImageGridDemo::setFreed(bool freed)
{
    if(m_freed == freed)
        return;
    m_freed=freed;
    if(freed)
    {
        m_panels[1]->clearImage();
        m_cells[1]->setText("Freed");
    }
    else
        loadCell(1, 1);
}

}//namespace gweni
