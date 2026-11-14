/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#ifndef _gweni_imageGrid_example_h_
#define _gweni_imageGrid_example_h_

#include <gweni/controls/base.h>

#include <vector>

namespace gweni
{
namespace controls
{
class Button;
class GroupBox;
class ImagePanel;
class ScrollControl;
namespace layout
{
class Tile;
}
}

//! A grid of images from bytes embedded in the binary (imageGridImages.inc,
//! PNGs and JPEGs), each fitted (ImageScaleMode::Fit) in a captioned cell of
//! a layout::Tile docked Top in a vertical ScrollControl with a stable gutter,
//! kept at twice the cell's size. One cell is left empty and one is given
//! bytes that are no image (it draws the missing image); "Replace" swaps the
//! first cell's image with another and back, "Free" clears the second cell's
//! and loads it again. Not in the demo shell: the bad-data cell logs a line.
class ImageGridDemo: public controls::Base
{
public:
    GWENI_CONTROL(ImageGridDemo, controls::Base);

public:
    static constexpr int CellWidth=144;
    static constexpr int CellHeight=124;
    static constexpr int MaxImageSize=2*CellWidth;
    //! the cells that are not loaded from an image
    static constexpr size_t EmptyCell=12;
    static constexpr size_t BadDataCell=13;

    const std::vector<controls::ImagePanel *> &panels() const { return m_panels; }
    const std::vector<controls::GroupBox *> &cells() const { return m_cells; }
    controls::Button *replaceButton() const { return m_replace; }
    controls::Button *freeButton() const { return m_free; }
    controls::ScrollControl *scroll() const { return m_scroll; }
    controls::layout::Tile *tile() const { return m_tile; }

    //! The first cell's image: its own (false) or the replacement (true)
    void setReplaced(bool replaced);
    bool replaced() const { return m_replaced; }
    //! The second cell's image freed (true) or loaded again (false)
    void setFreed(bool freed);
    bool freed() const { return m_freed; }

private:
    void onReplace(event::Info);
    void onFree(event::Info);
    void loadCell(size_t cell, size_t image);

    controls::Button *m_replace=nullptr;
    controls::Button *m_free=nullptr;
    controls::ScrollControl *m_scroll=nullptr;
    controls::layout::Tile *m_tile=nullptr;
    std::vector<controls::GroupBox *> m_cells;
    std::vector<controls::ImagePanel *> m_panels;
    bool m_replaced=false;
    bool m_freed=false;
};

}//namespace gweni

#endif//_gweni_imageGrid_example_h_
