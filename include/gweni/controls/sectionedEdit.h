/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_sectionededit_h_
#define _gweni_controls_sectionededit_h_

#include <gweni/controls/textBox.h>
#include <gweni/date.h>

#include <optional>
#include <vector>

namespace gweni
{
namespace controls
{
namespace internal
{

//! A TextBox showing a date or a time formatted from a pattern of sections,
//! the field DateEdit and TimeEdit are (QDateTimeEdit's sections). It is not
//! an editor of its own: the text is a TextBox's, typed into freely, and is
//! read as a whole (parse()) on Return and when the box loses the focus.
//!
//! **The format** (setFormat) is a pattern of sections from a fixed
//! vocabulary, every other character a separator shown as it is:
//! YYYY (the year, 4 digits), MM / M (the month, 2 digits or as few as it
//! takes), DD / D (the day), HH / H (the hour 0-23), hh / h (the hour 1-12,
//! which needs AP), mm (the minute, 2 digits) and AP (AM or PM). A pattern
//! is a date (a year, a month and a day, nothing of a time) or a time (an
//! hour and the minute, AP exactly with a 12-hour hour, nothing of a date),
//! each section once: "YYYY-MM-DD", "DD/MM/YYYY", "M/D/YYYY", "HH:mm",
//! "h:mm AP". Anything else is refused, keeping the format it had.
//!
//! **Stepping.** Up and Down step the section the caret is in (sectionAt())
//! by one, wrapping inside the section with no carry, as QDateTimeEdit does:
//! the day 31 goes to 1 in the same month, the minute 59 to 0 in the same
//! hour, December to January in the same year; a month or year step clamps
//! the day to the new month's last (January 31 a month on is February 28 or
//! 29). An hour step wraps at 24 (so on a 12-hour clock 11 AM goes to 12 PM),
//! and the AP section's step switches AM and PM. The minute steps by
//! setMinuteStep() to the next multiple of it, up or down, so a typed 07 goes
//! to 15 or 0 with a step of 15. PageUp and PageDown take larger steps: a
//! year in a date's day and month sections, ten years in its year section,
//! ten minutes (rounded up to a multiple of the minute step) in the minute
//! section, twelve hours in the hour and AP sections. A step from an empty
//! field shows setEmptyStepValue() (clamped) instead. A step whose value the
//! range clamps or acceptsValue() refuses is taken further in its direction
//! until one is accepted, or not at all if none is.
//!
//! **The parse** is lenient. A date's digits are three groups split by
//! anything that is no digit, read in the format's order of year, month and
//! day ("2026-11-1" for YYYY-MM-DD, "11/1/2026" for M/D/YYYY), or one group
//! of 8 (or 6) digits split at the format's widths ("20261101"); a year of
//! one or two digits is in 2000-2099. A time is TimeOfDay::fromText's
//! ("1430", "2:30 pm"), in either clock. A letter in a date is no date.
//! A parsed value is clamped to setFieldRange(); one acceptsValue() refuses,
//! and text that is no value, revert to the last value, as Escape does when
//! the text is not the value's. An empty text is no value when
//! setAllowEmpty(true) says it may be, and reverts otherwise.
//!
//! **Events.** onValueChanged fires once per real change of the value from
//! the user (a step, a commit, an AP key); setFieldValue and the setters fire
//! nothing.
//!
//! **The width** is the format's at the box's font, not the text's: each
//! section as wide as its widest value (the widest digit in every digit
//! place, the wider of AM and PM), the separators, the padding, one unit for
//! the caret and any child docked Left or Right (DateEdit's button). So a
//! new value never changes the box's size, a size the app sets is not used,
//! and with a Text that keeps its layout for a string of the same size
//! (setSameSizeKeepsLayout), a step that keeps the string's width (a digit
//! for a digit, in a face whose digits are one width) lays nothing out; one
//! that changes it (9 AM to 10 AM) lays out the box and its Text alone.
class GWENI_EXPORT SectionedEdit: public TextBox
{
public:

    GWENI_CONTROL(SectionedEdit, TextBox);

public:

    enum class SectionKind
    {
        Year,
        Month,
        Day,
        Hour24,
        Hour12,
        Minute,
        AmPm
    };

    struct Section
    {
        SectionKind kind=SectionKind::Year;
        //! The digits it is shown with: 4 (YYYY), 2 (MM, DD, HH, hh, mm), 1
        //! (as few as the value takes: M, D, H, h), 0 for AP.
        int digits=0;
    };

    //! A date and a time: a date format leaves the time at 00:00, a time
    //! format the date at 1970-01-01. Ordered by the date, then the time.
    struct Value
    {
        Date date;
        TimeOfDay time;

        bool operator==(const Value &other) const { return (date == other.date) && (time == other.time); }
        bool operator!=(const Value &other) const { return !(*this == other); }
        bool operator<(const Value &other) const
        {
            return (date != other.date)?(date < other.date):(time < other.time);
        }
    };

    //! Takes \p pattern, refusing one that is no date or time format (see the
    //! class comment) and returning whether it took it. The text is the value
    //! formatted anew, and the width measured again.
    bool setFormat(const String &pattern);
    const String &getFormat() const { return m_format; }
    //! Whether the format is a date's (else a time's).
    bool isDateFormat() const { return m_dateFormat; }
    const std::vector<Section> &getSections() const { return m_sections; }

    //! Sets the value, clamped to the range, with no event; empty only when
    //! setAllowEmpty() allows it (else it is ignored). The text shows it.
    void setFieldValue(const std::optional<Value> &value);
    const std::optional<Value> &getFieldValue() const { return m_value; }

    //! The values the field takes, both ends included; either may be empty
    //! for none. The value is clamped at once, with no event.
    void setFieldRange(const std::optional<Value> &min, const std::optional<Value> &max);
    const std::optional<Value> &getFieldMin() const { return m_min; }
    const std::optional<Value> &getFieldMax() const { return m_max; }

    //! Whether an empty text is a value (no value). Off by default: a field
    //! holds a value from the start (setEmptyStepValue's).
    void setAllowEmpty(bool allow);
    bool allowsEmpty() const { return m_allowEmpty; }

    //! What a step from an empty field shows, and the value a field that may
    //! not be empty starts with: 1970-01-01 00:00 unless set.
    void setEmptyStepValue(const Value &value);
    const Value &getEmptyStepValue() const { return m_emptyStepValue; }

    //! The minutes Up and Down step the minute section by, 1 to 60 (1, 5, 15
    //! are the usual); anything else is refused. A typed minute is kept as
    //! typed.
    void setMinuteStep(int step);
    int getMinuteStep() const { return m_minuteStep; }

    //! The text \p value is shown as in this format; empty for none.
    String formatValue(const std::optional<Value> &value) const;
    //! The value \p text reads as in this format, empty for none (see the
    //! class comment). Neither clamped nor checked by acceptsValue().
    std::optional<Value> parse(const String &text) const;

    //! The section the caret is in, by the alphanumeric runs of the text: the
    //! run the caret is in or at the end of, the next one when it is between
    //! two, clamped to the sections. -1 without sections.
    int sectionAt(int caret) const;

    //! Steps the caret's section (see the class comment): \p direction 1 or
    //! -1, \p large for PageUp and PageDown. Typed text is read first, as a
    //! commit is. Returns whether the value changed.
    bool stepSection(int direction, bool large);
    //! Reads the text as a value: a new one is taken (onValueChanged), the
    //! text is formatted from the value either way. Returns whether the text
    //! was a value the field takes (an unchanged one included).
    bool commit();
    //! Shows the value's text again, dropping what was typed.
    void revert();

    bool onKeyUp(bool down) override;
    bool onKeyDown(bool down) override;
    bool onKeyPageUp(bool down) override;
    bool onKeyPageDown(bool down) override;
    //! The press reads the text (commit()) and goes on up to the parents, as
    //! a TextBox's does; the release is TextBox's.
    bool onKeyReturn(bool down) override;
    //! Reverts typed text, taking the key; with nothing typed it goes on up
    //! (a Popup's or a Dialog's Escape).
    bool onKeyEscape(bool down) override;
    //! A, a, P or p in the AP section sets AM or PM; any other character is
    //! typed.
    bool onChar(gweni::UnicodeChar c) override;
    //! Reads the text, then fires onLostFocusCaller (TextBox's).
    void onLostKeyboardFocus() override;

    //! The format's width (see the class comment).
    void calculateSize(skin::Base *skin, Dim dim) override;

    void setFont(const gweni::Font &font) override;
    using TextBox::setFont;

    //! Fired once per real change of the value by the user, with this field.
    event::Caller onValueChanged;

protected:

    //! Whether the field takes \p value (in its range): a DateEdit's date
    //! filter. Everything by default.
    virtual bool acceptsValue(const Value &/*value*/) const { return true; }
    //! Whether the field takes a date format (\p date) or a time format: a
    //! DateEdit only a date's, a TimeEdit only a time's. Either by default.
    //! Not asked by the format SectionedEdit's own constructor sets.
    virtual bool formatAllowed(bool /*date*/) const { return true; }

    //! The value clamped to the range.
    Value clampValue(const Value &value) const;
    //! A value from the user: clamped, then taken if acceptsValue() takes it
    //! (firing onValueChanged when it changed); the text is the value's
    //! either way. Returns whether it was taken.
    bool userSetValue(const std::optional<Value> &value);
    //! \p value with section \p section stepped once (see the class comment).
    Value stepValue(const Value &value, SectionKind section, int direction, bool large) const;
    //! The text shown for the value, the caret kept in its section.
    void showValue();

    //! The width of the format's widest text at the box's font.
    int formatWidth();
    //! In X, what makes the request the format's width (m_fieldWidth),
    //! whatever width was asked for: a parent placing the box by its request
    //! (a child with no dock) gets that width too.
    int requestedSizeExtra(Dim dim) const override;

    String m_format;
    bool m_dateFormat=true;
    std::vector<Section> m_sections;
    //! The format in order: each a section (its index into m_sections) or a
    //! separator (its text, section -1).
    struct Piece
    {
        int section=-1;
        String separator;
    };
    std::vector<Piece> m_pieces;

    std::optional<Value> m_value;
    std::optional<Value> m_min;
    std::optional<Value> m_max;
    Value m_emptyStepValue;
    bool m_allowEmpty=false;
    int m_minuteStep=1;

    //! formatWidth()'s measure, for the font and format it was taken in.
    const gweni::Font *m_widthFont=nullptr;
    int m_width=-1;
    //! The box's width from its last X measure: the format's, the padding,
    //! the caret and the docked children; -1 before it.
    int m_fieldWidth=-1;
};

}//namespace internal
}//namespace controls
}//namespace gweni

#endif//_gweni_controls_sectionededit_h_
