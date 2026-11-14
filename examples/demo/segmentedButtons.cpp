#include "demo/segmentedButtons.h"
#include "gweni/controls/label.h"
#include "gweni/controls/segmentedButtons.h"

namespace gweni
{

namespace
{

// A section's caption, docked Top in the column.
void caption(controls::Base *column, const char *text)
{
    controls::Label *label=column->newChild<controls::Label>();

    label->setText(text);
    label->setDock(DockPosition::Top);
    label->setMargin(Margin(0, 8, 0, 3));
}

// A row docked Top in the column, its segments at its left.
controls::SegmentedButtons *row(controls::Base *column)
{
    controls::SegmentedButtons *row=column->newChild<controls::SegmentedButtons>();

    row->setDock(DockPosition::Top);
    return row;
}

}//namespace

// A column of captioned rows, then the label: the view (Week selected), the
// layout in equal widths with an image and text each, the zoom as images alone
// with their text as tooltips, the text format as a multi-select row with
// checks (Bold on) and a disabled row (Auto on).
GWENI_CONTROL_CONSTRUCTOR(SegmentedButtonsDemo)
{
    setDock(DockPosition::Center);

    controls::Base *column=newChild<controls::Base>();

    column->setSizeFlags({SizeFlag::Fixed, SizeFlag::Shrink});
    column->setPos(10, 4);
    column->setWidth(520);

    caption(column, "View: one at a time");
    m_text=row(column);
    for(const char *text : {"Day", "Week", "Month", "Year"})
        m_text->addSegment(text);
    m_text->setSelected(1, true);

    caption(column, "Layout: an image and text each, equal widths");
    m_imageText=row(column);
    m_imageText->setEqualWidths(true);
    for(const char *text : {"List", "Grid", "Gallery"})
        m_imageText->addSegment(text, "test16.png", false);

    caption(column, "Zoom: images alone, the text as a tooltip");
    m_image=row(column);
    for(const char *text : {"Zoom out", "Actual size", "Zoom in", "Fit"})
        m_image->addSegment(text, "test16.png", true);
    m_image->setSelected(1, true);

    caption(column, "Format: any number, with checks");
    m_multi=row(column);
    m_multi->setMode(controls::SegmentedButtons::Mode::Multi);
    m_multi->setShowCheck(true);
    for(const char *text : {"Bold", "Italic", "Underline", "Strike"})
        m_multi->addSegment(text);
    m_multi->setSelected(0, true);

    caption(column, "Disabled");
    m_disabled=row(column);
    for(const char *text : {"Off", "Auto", "On"})
        m_disabled->addSegment(text);
    m_disabled->setSelected(1, true);
    m_disabled->setDisabled(true);

    m_selection=column->newChild<controls::Label>();
    m_selection->setDock(DockPosition::Top);
    m_selection->setMargin(Margin(0, 12, 0, 0));
    updateLabel();

    for(controls::SegmentedButtons *buttons : {m_text, m_imageText, m_image, m_multi})
        buttons->onSelectionChange.add(this, &SegmentedButtonsDemo::onSelectionChange);
}

void SegmentedButtonsDemo::setLabelUpdates(bool updates)
{
    m_labelUpdates=updates;
    if(updates)
        updateLabel();
}

void SegmentedButtonsDemo::onSelectionChange(event::Info)
{
    if(m_labelUpdates)
        updateLabel();
}

// "View: Week   Layout: List   Zoom: Actual size   Format: Bold, Italic"
void SegmentedButtonsDemo::updateLabel()
{
    auto exclusive=[](controls::SegmentedButtons *buttons) -> String {
        const int index=buttons->getSelectedIndex();

        return (index < 0)?String("-"):buttons->getSegment(index)->getText();
    };
    String format;

    for(int index : m_multi->getSelectedIndices())
        format+=(format.empty()?"":", ")+m_multi->getSegment(index)->getText();

    m_selection->setText("View: "+exclusive(m_text)+"   Layout: "+exclusive(m_imageText)+"   Zoom: "
        +exclusive(m_image)+"   Format: "+(format.empty()?String("-"):format));
}

}//namespace gweni
