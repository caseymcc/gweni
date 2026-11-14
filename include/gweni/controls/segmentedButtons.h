/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_segmentedbuttons_h_
#define _gweni_controls_segmentedbuttons_h_

#include <gweni/controls/base.h>
#include <gweni/controls/selectable.h>

#include <vector>

namespace gweni
{
namespace controls
{

class SegmentedButtons;

//! One segment of a SegmentedButtons row: a Selectable (so a Button) holding
//! its text, an image (Button::setImage) or both. The segment places its own
//! parts (its Text and its image are Owner) in one centred group: the check's
//! room while its row shows checks (CheckSize and CheckGap, kept whether it is
//! selected or not, so a selection never changes a width), the image, ImageGap
//! when both show, and the text, SidePadding in from either side and PaddingY
//! from the top and bottom. setImageOnly(true) leaves the text out of the
//! segment and makes it the tooltip.
//!
//! It is disabled when it or its row is (isDisabled()), so a row disabled and
//! enabled again gives each segment its own state back.
class GWENI_EXPORT SegmentedButton: public Selectable
{
public:

    GWENI_CONTROL(SegmentedButton, Selectable);

public:

    //! Units between the segment's edges and its content.
    static constexpr int SidePadding=12;
    static constexpr int PaddingY=4;
    //! The check icon's square, and the units between it and what follows.
    static constexpr int CheckSize=12;
    static constexpr int CheckGap=4;
    //! Units between the image and the text when both show.
    static constexpr int ImageGap=4;

    //! The row it is a segment of, or null.
    SegmentedButtons *getSegmentedButtons() const { return m_row; }

    //! Button's image, placed by the segment before its text.
    void setImage(const String &name, bool center=false) override;
    bool hasImage() const { return m_image != nullptr; }

    //! The text left out of the segment (only the image and the check show)
    //! and set as its tooltip; false puts it back and clears the tooltip.
    void setImageOnly(bool imageOnly);
    bool isImageOnly() const { return m_imageOnly; }

    //! Whether the check's room is kept before the content (the row's
    //! setShowCheck).
    void setCheckRoom(bool room);
    bool hasCheckRoom() const { return m_checkRoom; }

    //! Where the check icon goes, in the segment's own coordinates (empty
    //! without the check's room). The skin draws it there while selected.
    const Rect &getCheckRect() const { return m_checkRect; }

    //! Disabled by its own setDisabled() or by its row's.
    bool isDisabled() override;

    //! The skin's SegmentedButton text colour for its state.
    void updateColors() override;

    //! The row's one Tab stop (SegmentedButtons::getTabStop()) alone is
    //! tabable; a segment outside a row as a Selectable.
    bool isTabable() override;

    void calculateSize(skin::Base *skin, Dim dim) override;

protected:

    friend class SegmentedButtons;

    void arrangeHorizontal(skin::Base *skin) override;
    void arrangeVertical(skin::Base *skin) override;

    //! The parts that show, in order: their widths and the group's.
    int contentWidth() const;
    bool textShows() const;
    int imageWidth() const;
    int imageHeight() const;

    SegmentedButtons *m_row=nullptr;
    bool m_imageOnly=false;
    bool m_checkRoom=false;
    Rect m_checkRect;
};

//! A row of joined toggle segments (Material's segmented button, WinUI's
//! RadioButtons, a QButtonGroup of tool buttons). Exclusive (the default): one
//! segment selected at a time, and one always is once a segment exists, unless
//! setAllowNone(true) lets a click on the selected one clear it. Multi: each
//! segment toggles on its own.
//!
//! The exclusive selection is a SelectableGroup, the one "at most one
//! selected" holder gweni has (RadioButtonController's radio buttons keep their
//! own checked state, so it is not one to reuse): every segment is a member
//! while the row is Exclusive and none is while it is Multi. setAllowNone gives
//! the members their toggling back.
//!
//! onSelectionChange fires once per real change of a segment's selection,
//! from a click, a key or the app, with the changed segment in info.control:
//! the newly selected one when the exclusive selection moves (the one it left
//! fires nothing), the cleared one when it is cleared, the toggled one in
//! Multi. A call that changes nothing fires nothing.
//!
//! The segments are owner-placed in one row in the order they were added, each
//! its content width or, with setEqualWidths(true), the widest one's, all of
//! them the tallest one's height; each one after the first starts Joint units
//! left of the previous one's right edge, so neighbours share their border
//! column. The row's preferred size is theirs and its padding.
//!
//! The keyboard: the row is one Tab stop, getTabStop(), the segment holding
//! the focus while one does, else the exclusive selection, else the first
//! enabled segment. Left and Up move to the previous enabled segment, Right
//! and Down to the next, Home and End to the first and last, without
//! wrapping; in an Exclusive row the selection moves with the focus (radio
//! groups', WinUI RadioButtons' keys), in a Multi row only the focus does, and
//! Space or Return toggles the focused segment (Selectable's keys). Disabled
//! segments are passed over, and a disabled row takes no key.
class GWENI_EXPORT SegmentedButtons: public Base
{
public:

    GWENI_CONTROL(SegmentedButtons, Base);

public:

    //! A container of its segments, which draw the row.
    GWENI_DRAWS_NOTHING

    ~SegmentedButtons() override;

    //! The units two neighbours overlap by: their shared border column.
    static constexpr int Joint=1;

    enum class Mode
    {
        Exclusive,
        Multi
    };

    //! A new segment at the end of the row, named \p name. In an exclusive
    //! row that has no selection and does not allow none, it is selected.
    SegmentedButton *addSegment(const String &text, const String &name="");
    //! A new segment showing \p image (Button::setImage), its text as its
    //! tooltip with \p imageOnly, else beside the image.
    SegmentedButton *addSegment(const String &text, const String &image, bool imageOnly,
        const String &name="");

    //! Deletes the segment (its ids go back). If it was the exclusive
    //! selection and the row does not allow none, the nearest enabled segment
    //! after it, else before it, is selected.
    void removeSegment(int index);
    void removeSegment(SegmentedButton *segment);

    int segmentCount() const { return static_cast<int>(m_segments.size()); }
    //! Null out of range.
    SegmentedButton *getSegment(int index) const;
    //! -1 if it is not one of the row's.
    int indexOf(const Base *segment) const;

    //! Segment \p index's image (SegmentedButton::setImage and setImageOnly).
    void setSegmentImage(int index, const String &image, bool imageOnly=false);

    //! Switching to Exclusive keeps the first selected segment (the others
    //! are deselected, each firing) or, with none allowed off, selects the
    //! first enabled one; switching to Multi keeps the selection.
    void setMode(Mode mode);
    Mode getMode() const { return m_mode; }

    //! Exclusive only: whether a click on the selected segment, or
    //! setSelected(i, false), clears the selection. Turning it off selects
    //! the first enabled segment when none is.
    void setAllowNone(bool allow);
    bool allowsNone() const { return m_allowNone; }

    //! Selects or deselects segment \p index. Exclusive: selecting moves the
    //! selection there; deselecting the selected one is refused unless none
    //! is allowed. Nothing for an index out of range.
    void setSelected(int index, bool selected);
    bool isSelected(int index) const;
    //! The exclusive selection, -1 for none (in Multi, the first selected).
    int getSelectedIndex() const;
    //! Every selected segment, in row order.
    std::vector<int> getSelectedIndices() const;

    //! Every segment the widest one's width.
    void setEqualWidths(bool equal);
    bool hasEqualWidths() const { return m_equalWidths; }

    //! A check icon before a selected segment's content, its room kept in
    //! every segment so a selection lays nothing out.
    void setShowCheck(bool show);
    bool showsCheck() const { return m_showCheck; }

    //! The segment Tab enters the row at (see the class comment), null when
    //! no segment is enabled.
    SegmentedButton *getTabStop() const;

    //! Focuses the enabled segment \p steps from the focused one (or from
    //! the Tab stop), stopping at the row's ends, and in an Exclusive row
    //! selects it; false if no segment is enabled or the row is disabled.
    bool moveFocus(int steps);
    //! The same to the first (\p last false) or last enabled segment.
    bool moveFocusToEnd(bool last);

    bool onKeyLeft(bool down) override;
    bool onKeyRight(bool down) override;
    bool onKeyUp(bool down) override;
    bool onKeyDown(bool down) override;
    bool onKeyHome(bool down) override;
    bool onKeyend(bool down) override;

    //! The row and, through SegmentedButton::isDisabled(), every segment.
    void setDisabled(bool active) override;

    //! Where each segment of the widths \p widths starts, x from the row's
    //! content edge (the arrange's rule: each one Joint units left of the
    //! previous one's right edge), into \p x; the row's content width is
    //! returned.
    static int segmentPositions(const std::vector<int> &widths, std::vector<int> &x);

    void calculateSize(skin::Base *skin, Dim dim) override;

    //! A segment changed: info.control the segment.
    event::Caller onSelectionChange;

protected:

    void arrangeHorizontal(skin::Base *skin) override;
    void arrangeVertical(skin::Base *skin) override;
    void onChildRemoved(Base *child) override;

    void onSegmentSelect(event::Info info);
    void onSegmentDeselect(event::Info info);
    //! In an exclusive row that does not allow none and has no selection,
    //! selects the enabled segment nearest \p from (after it first), else
    //! any.
    void ensureSelection(int from=0);
    //! Repaints every segment: which of them are the row's ends changed.
    void restyleSegments();
    //! Focuses \p segment and, in an Exclusive row, selects it.
    bool focusSegment(SegmentedButton *segment);
    //! Whether the segment takes focus: enabled and shown.
    static bool canFocus(SegmentedButton *segment);
    //! Each segment's width, as placed.
    void segmentWidths(std::vector<int> &widths) const;

    std::vector<SegmentedButton *> m_segments;
    SelectableGroup m_group;
    Mode m_mode=Mode::Exclusive;
    bool m_allowNone=false;
    bool m_equalWidths=false;
    bool m_showCheck=false;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_segmentedbuttons_h_
