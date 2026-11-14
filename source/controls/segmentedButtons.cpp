/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/segmentedButtons.h>
#include <gweni/controls/imagePanel.h>
#include <gweni/skin.h>

#include <algorithm>

namespace gweni
{
namespace controls
{

///////////////////////////////////////////////////////////////////////////
//SegmentedButton

// Owner-placed by its row, as wide and high as its content: its Text has no
// padding of its own, the segment keeps the room around it.
GWENI_CONTROL_CONSTRUCTOR(SegmentedButton)
{
    setDock(DockPosition::None);
    setSizeFlags({SizeFlag::Elastic, SizeFlag::Elastic});
    setTextPadding(Padding(0, 0, 0, 0));
    setDefaultTextAlignment(Alignment::Left|Alignment::CenterV);
    m_text->setPlacement(Placement::Owner);
}

void SegmentedButton::setImage(const String &name, bool center)
{
    const bool had=(m_image != nullptr);

    ParentClass::setImage(name, center);
    // Button's padding for its image is the segment's to keep
    setTextPadding(Padding(0, 0, 0, 0));
    if(m_image)
    {
        m_image->setPlacement(Placement::Owner);
        m_image->setMargin(Margin(0, 0, 0, 0));
        m_image->setMouseInputEnabled(false);
    }
    if(had != (m_image != nullptr))
        invalidate();
}

void SegmentedButton::setImageOnly(bool imageOnly)
{
    if(imageOnly == m_imageOnly)
        return;

    m_imageOnly=imageOnly;
    m_text->setHidden(imageOnly);
    setToolTipText(imageOnly?getText():String());
    invalidate();
}

void SegmentedButton::setCheckRoom(bool room)
{
    if(room == m_checkRoom)
        return;

    m_checkRoom=room;
    invalidate();
}

bool SegmentedButton::isDisabled()
{
    return ParentClass::isDisabled() || (m_row && m_row->isDisabled());
}

bool SegmentedButton::textShows() const
{
    return !m_imageOnly && !m_text->getText().empty();
}

int SegmentedButton::imageWidth() const
{
    return m_image?m_image->textureWidth():0;
}

int SegmentedButton::imageHeight() const
{
    return m_image?m_image->textureHeight():0;
}

int SegmentedButton::contentWidth() const
{
    int width=m_checkRoom?CheckSize+CheckGap:0;

    if(m_image)
        width+=imageWidth()+(textShows()?ImageGap:0);
    if(textShows())
        width+=m_text->getPreferredSize().width;
    return width;
}

void SegmentedButton::calculateSize(skin::Base *skin, Dim dim)
{
    // the Text and the image are measured for themselves; neither counts
    ParentClass::calculateSize(skin, dim);

    if(dim == Dim::X)
    {
        m_preferredSize.width=preferredFromContent(SidePadding*2+contentWidth(), Dim::X);
        return;
    }

    int height=std::max(m_checkRoom?CheckSize:0, imageHeight());

    if(textShows())
        height=std::max(height, m_text->getPreferredSize().height);
    m_preferredSize.height=preferredFromContent(PaddingY*2+height, Dim::Y);
}

// The check's room, the image and the text, as one group centred in the
// segment (it is wider than its content in a row of equal widths).
void SegmentedButton::arrangeHorizontal(skin::Base *skin)
{
    ParentClass::arrangeHorizontal(skin);

    int x=std::max(SidePadding, (getWidth()-contentWidth())/2);

    m_checkRect=m_checkRoom?Rect(x, m_checkRect.y, CheckSize, CheckSize):Rect();
    if(m_checkRoom)
        x+=CheckSize+CheckGap;
    if(m_image)
    {
        placeChild(m_image, x, m_image->getY(), imageWidth(), imageHeight());
        x+=imageWidth()+(textShows()?ImageGap:0);
    }
    placeChild(m_text, x, m_text->getY(), textShows()?m_text->getPreferredSize().width:0, m_text->getHeight());
}

// Each part centred on the segment's height.
void SegmentedButton::arrangeVertical(skin::Base *skin)
{
    ParentClass::arrangeVertical(skin);

    const int height=getHeight();

    if(m_checkRoom)
        m_checkRect.y=(height-CheckSize)/2;
    if(m_image)
        placeChild(m_image, m_image->getX(), (height-imageHeight())/2, m_image->getWidth(), imageHeight());

    const int textHeight=m_text->getPreferredSize().height;

    placeChild(m_text, m_text->getX(), (height-textHeight)/2, m_text->getWidth(), textHeight);
}

///////////////////////////////////////////////////////////////////////////
//SegmentedButtons

// As large as its segments, wherever it is put.
GWENI_CONTROL_CONSTRUCTOR(SegmentedButtons)
{
    setSizeFlags({SizeFlag::Elastic, SizeFlag::Elastic});
}

// The segments are deleted by Base's destructor, after the row's own part is
// gone: none may ask it anything by then.
SegmentedButtons::~SegmentedButtons()
{
    for(SegmentedButton *segment : m_segments)
        segment->m_row=nullptr;
}

SegmentedButton *SegmentedButtons::addSegment(const String &text, const String &name)
{
    SegmentedButton *segment=newChild<SegmentedButton>();

    segment->setPlacement(Placement::Owner);
    segment->setName(name);
    segment->setText(text);
    segment->m_row=this;
    segment->setCheckRoom(m_showCheck);
    segment->onSelect.add(this, &SegmentedButtons::onSegmentSelect);
    segment->onDeselect.add(this, &SegmentedButtons::onSegmentDeselect);
    m_segments.push_back(segment);

    if(m_mode == Mode::Exclusive)
    {
        m_group.add(segment);
        segment->setToggleable(m_allowNone);
        ensureSelection(segmentCount()-1);
    }
    invalidate();
    return segment;
}

SegmentedButton *SegmentedButtons::addSegment(const String &text, const String &image, bool imageOnly,
    const String &name)
{
    SegmentedButton *segment=addSegment(text, name);

    segment->setImage(image);
    segment->setImageOnly(imageOnly);
    return segment;
}

void SegmentedButtons::removeSegment(int index)
{
    SegmentedButton *segment=getSegment(index);

    if(!segment)
        return;

    const bool wasSelected=segment->isSelected();

    // out of the row first, so onChildRemoved finds nothing to do
    m_segments.erase(m_segments.begin()+index);
    segment->m_row=nullptr;
    delete segment;

    if(wasSelected && (m_mode == Mode::Exclusive))
        ensureSelection(index);
}

void SegmentedButtons::removeSegment(SegmentedButton *segment)
{
    removeSegment(indexOf(segment));
}

SegmentedButton *SegmentedButtons::getSegment(int index) const
{
    if(index < 0 || index >= segmentCount())
        return nullptr;
    return m_segments[static_cast<size_t>(index)];
}

int SegmentedButtons::indexOf(const Base *segment) const
{
    for(size_t i=0; i < m_segments.size(); ++i)
    {
        if(m_segments[i] == segment)
            return static_cast<int>(i);
    }
    return -1;
}

void SegmentedButtons::setSegmentImage(int index, const String &image, bool imageOnly)
{
    if(SegmentedButton *segment=getSegment(index))
    {
        segment->setImage(image);
        segment->setImageOnly(imageOnly);
    }
}

void SegmentedButtons::setMode(Mode mode)
{
    if(mode == m_mode)
        return;

    m_mode=mode;
    if(mode == Mode::Multi)
    {
        for(SegmentedButton *segment : m_segments)
            m_group.remove(segment);
        return;
    }

    // the first selected one stays, before the group sees any of them
    bool kept=false;

    for(SegmentedButton *segment : m_segments)
    {
        if(!segment->isSelected())
            continue;
        if(kept)
            segment->setSelected(false);
        kept=true;
    }
    for(SegmentedButton *segment : m_segments)
    {
        m_group.add(segment);
        segment->setToggleable(m_allowNone);
    }
    ensureSelection();
}

void SegmentedButtons::setAllowNone(bool allow)
{
    if(allow == m_allowNone)
        return;

    m_allowNone=allow;
    if(m_mode != Mode::Exclusive)
        return;

    for(SegmentedButton *segment : m_segments)
        segment->setToggleable(allow);
    ensureSelection();
}

void SegmentedButtons::setSelected(int index, bool selected)
{
    SegmentedButton *segment=getSegment(index);

    if(!segment)
        return;
    if(!selected && (m_mode == Mode::Exclusive) && !m_allowNone)
        return;

    segment->setSelected(selected);
}

bool SegmentedButtons::isSelected(int index) const
{
    const SegmentedButton *segment=getSegment(index);

    return segment && segment->isSelected();
}

int SegmentedButtons::getSelectedIndex() const
{
    for(size_t i=0; i < m_segments.size(); ++i)
    {
        if(m_segments[i]->isSelected())
            return static_cast<int>(i);
    }
    return -1;
}

std::vector<int> SegmentedButtons::getSelectedIndices() const
{
    std::vector<int> indices;

    for(size_t i=0; i < m_segments.size(); ++i)
    {
        if(m_segments[i]->isSelected())
            indices.push_back(static_cast<int>(i));
    }
    return indices;
}

void SegmentedButtons::setEqualWidths(bool equal)
{
    if(equal == m_equalWidths)
        return;

    m_equalWidths=equal;
    invalidate();
}

void SegmentedButtons::setShowCheck(bool show)
{
    if(show == m_showCheck)
        return;

    m_showCheck=show;
    for(SegmentedButton *segment : m_segments)
        segment->setCheckRoom(show);
}

void SegmentedButtons::setDisabled(bool active)
{
    if(active == isDisabled())
        return;

    ParentClass::setDisabled(active);
    // each segment's colours follow the row's state
    for(SegmentedButton *segment : m_segments)
        segment->redraw();
}

// Exclusive: the segment the selection moved to fires, the one it left does
// not (the group's selection is already the new one when it is deselected).
void SegmentedButtons::onSegmentSelect(event::Info info)
{
    onSelectionChange.call(this, event::Info(info.controlCaller));
}

void SegmentedButtons::onSegmentDeselect(event::Info info)
{
    if((m_mode == Mode::Exclusive) && m_group.getSelected())
        return;

    onSelectionChange.call(this, event::Info(info.controlCaller));
}

void SegmentedButtons::ensureSelection(int from)
{
    if((m_mode != Mode::Exclusive) || m_allowNone || m_segments.empty() || m_group.getSelected())
        return;

    const int count=segmentCount();

    from=std::min(std::max(from, 0), count-1);
    for(int i=from; i < count; ++i)
    {
        if(!m_segments[static_cast<size_t>(i)]->isDisabled())
        {
            m_segments[static_cast<size_t>(i)]->setSelected(true);
            return;
        }
    }
    for(int i=from-1; i >= 0; --i)
    {
        if(!m_segments[static_cast<size_t>(i)]->isDisabled())
        {
            m_segments[static_cast<size_t>(i)]->setSelected(true);
            return;
        }
    }
    // every segment disabled: one is still selected
    m_segments[static_cast<size_t>(from)]->setSelected(true);
}

// A segment deleted by the app (not through removeSegment) leaves the row.
void SegmentedButtons::onChildRemoved(Base *child)
{
    ParentClass::onChildRemoved(child);

    const int index=indexOf(child);

    if(index < 0)
        return;

    SegmentedButton *segment=m_segments[static_cast<size_t>(index)];
    const bool wasSelected=segment->isSelected();

    m_segments.erase(m_segments.begin()+index);
    segment->m_row=nullptr;
    // a deleted segment has left the group already; one moved to another
    // parent leaves it here
    m_group.remove(segment);
    if(wasSelected && (m_mode == Mode::Exclusive))
        ensureSelection(index);
}

int SegmentedButtons::segmentPositions(const std::vector<int> &widths, std::vector<int> &x)
{
    int right=0;

    x.resize(widths.size());
    for(size_t i=0; i < widths.size(); ++i)
    {
        x[i]=(i == 0)?0:right-Joint;
        right=x[i]+widths[i];
    }
    return right;
}

void SegmentedButtons::segmentWidths(std::vector<int> &widths) const
{
    int widest=0;

    widths.resize(m_segments.size());
    for(size_t i=0; i < m_segments.size(); ++i)
    {
        widths[i]=m_segments[i]->getPreferredSize().width;
        widest=std::max(widest, widths[i]);
    }
    if(m_equalWidths)
        std::fill(widths.begin(), widths.end(), widest);
}

void SegmentedButtons::calculateSize(skin::Base *skin, Dim dim)
{
    // the segments are measured for their own subtrees; none counts
    ParentClass::calculateSize(skin, dim);

    const Padding &padding=getPadding();

    if(dim == Dim::X)
    {
        std::vector<int> widths, x;

        segmentWidths(widths);
        m_preferredSize.width=preferredFromContent(segmentPositions(widths, x)+padding.left+padding.right,
            Dim::X);
        return;
    }

    int height=0;

    for(const SegmentedButton *segment : m_segments)
        height=std::max(height, segment->getPreferredSize().height);
    m_preferredSize.height=preferredFromContent(height+padding.top+padding.bottom, Dim::Y);
}

void SegmentedButtons::arrangeHorizontal(skin::Base *skin)
{
    ParentClass::arrangeHorizontal(skin);

    std::vector<int> widths, x;
    const int left=getPadding().left;

    segmentWidths(widths);
    segmentPositions(widths, x);
    for(size_t i=0; i < m_segments.size(); ++i)
    {
        SegmentedButton *segment=m_segments[i];

        placeChild(segment, left+x[i], segment->getY(), widths[i], segment->getHeight());
    }
}

// Every segment the tallest one's height, the row's content height.
void SegmentedButtons::arrangeVertical(skin::Base *skin)
{
    ParentClass::arrangeVertical(skin);

    const Padding &padding=getPadding();
    const int height=std::max(0, getHeight()-padding.top-padding.bottom);

    for(SegmentedButton *segment : m_segments)
        placeChild(segment, segment->getX(), padding.top, segment->getWidth(), height);
}

}//namespace controls
}//namespace gweni
