/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#ifndef _gweni_segmentedbuttons_example_h_
#define _gweni_segmentedbuttons_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class Label;
class SegmentedButtons;
}

//! SegmentedButtons rows: an exclusive row of text segments, one of an image
//! and text each in equal widths, one of images alone (their text the
//! tooltip), a multi-select row with checks and a disabled row, and a label
//! naming what the view, layout, zoom and format rows have selected.
class SegmentedButtonsDemo: public controls::Base
{
public:
    GWENI_CONTROL(SegmentedButtonsDemo, controls::Base);

public:
    //! The rows a test drives.
    controls::SegmentedButtons *textRow() const { return m_text; }
    controls::SegmentedButtons *imageTextRow() const { return m_imageText; }
    controls::SegmentedButtons *imageRow() const { return m_image; }
    controls::SegmentedButtons *multiRow() const { return m_multi; }
    controls::SegmentedButtons *disabledRow() const { return m_disabled; }
    controls::Label *selectionLabel() const { return m_selection; }

    //! Whether a selection change sets the label's text (on by default): a
    //! test weighing a row's own cost turns it off, since the label's new
    //! text is laid out.
    void setLabelUpdates(bool updates);

private:
    void onSelectionChange(event::Info info);
    void updateLabel();

    controls::SegmentedButtons *m_text=nullptr;
    controls::SegmentedButtons *m_imageText=nullptr;
    controls::SegmentedButtons *m_image=nullptr;
    controls::SegmentedButtons *m_multi=nullptr;
    controls::SegmentedButtons *m_disabled=nullptr;
    controls::Label *m_selection=nullptr;
    bool m_labelUpdates=true;
};

}//namespace gweni

#endif//_gweni_segmentedbuttons_example_h_
