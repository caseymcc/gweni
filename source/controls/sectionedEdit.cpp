/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/controls/sectionedEdit.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace gweni
{
namespace controls
{
namespace internal
{

namespace
{

//! The format's tokens, the longer of two sharing a letter first.
struct Token
{
    const char *text;
    SectionedEdit::SectionKind kind;
    int digits;
};

const Token Tokens[]=
{
    {"YYYY", SectionedEdit::SectionKind::Year, 4},
    {"MM", SectionedEdit::SectionKind::Month, 2},
    {"DD", SectionedEdit::SectionKind::Day, 2},
    {"HH", SectionedEdit::SectionKind::Hour24, 2},
    {"hh", SectionedEdit::SectionKind::Hour12, 2},
    {"mm", SectionedEdit::SectionKind::Minute, 2},
    {"AP", SectionedEdit::SectionKind::AmPm, 0},
    {"M", SectionedEdit::SectionKind::Month, 1},
    {"D", SectionedEdit::SectionKind::Day, 1},
    {"H", SectionedEdit::SectionKind::Hour24, 1},
    {"h", SectionedEdit::SectionKind::Hour12, 1},
};

bool isAsciiLetter(char c)
{
    return ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z'));
}

bool isDigit(char c)
{
    return (c >= '0') && (c <= '9');
}

//! The runs of ASCII letters and digits in \p text, [first, second).
std::vector<std::pair<int, int>> alnumRuns(const String &text)
{
    std::vector<std::pair<int, int>> runs;
    int start=-1;

    for(int i=0; i <= static_cast<int>(text.size()); ++i)
    {
        const bool alnum=(i < static_cast<int>(text.size())) && (isDigit(text[i]) || isAsciiLetter(text[i]));

        if(alnum && start < 0)
            start=i;
        else if(!alnum && start >= 0)
        {
            runs.emplace_back(start, i);
            start=-1;
        }
    }
    return runs;
}

//! \p value modulo \p count, in [0, count).
int wrapInto(int value, int count)
{
    const int m=value%count;
    return m < 0?m+count:m;
}

bool isBlank(const String &text)
{
    return text.find_first_not_of(" \t") == String::npos;
}

// a date field's years: the format's four digits
constexpr Date MinDate(1, 1, 1);
constexpr Date MaxDate(9999, 12, 31);

//! Whether \p value is a real date and time: an end, a value or a start
//! that is none (a 13th month, 25:00) would be shown as it is and stepped
//! with no days to wrap in.
bool isValidValue(const SectionedEdit::Value &value)
{
    return value.date.isValid() && value.time.isValid();
}

void clampDay(Date &date)
{
    const int last=Date::daysInMonth(date.year, date.month);

    if(date.day > last)
        date.day=last;
}

}//namespace


GWENI_CONTROL_CONSTRUCTOR(SectionedEdit)
{
    // a string of the same width keeps the layout: a step that changes a
    // digit lays nothing out
    m_text->setSameSizeKeepsLayout(true);
    m_value=m_emptyStepValue;
    setFormat("YYYY-MM-DD");
}

bool SectionedEdit::setFormat(const String &pattern)
{
    std::vector<Section> sections;
    std::vector<Piece> pieces;
    bool hasKind[7]={};
    int hourAt=-1;
    int minuteAt=-1;
    size_t i=0;

    while(i < pattern.size())
    {
        const Token *found=nullptr;

        for(const Token &token : Tokens)
        {
            if(pattern.compare(i, std::char_traits<char>::length(token.text), token.text) == 0)
            {
                found=&token;
                break;
            }
        }

        if(found)
        {
            const int kind=static_cast<int>(found->kind);

            // each section once, and a separator between two
            if(hasKind[kind] || (!pieces.empty() && (pieces.back().section >= 0)))
                return false;
            hasKind[kind]=true;
            if(found->kind == SectionKind::Minute)
                minuteAt=static_cast<int>(sections.size());
            else if((found->kind == SectionKind::Hour24) || (found->kind == SectionKind::Hour12))
                hourAt=static_cast<int>(sections.size());
            pieces.push_back({static_cast<int>(sections.size()), String()});
            sections.push_back({found->kind, found->digits});
            i+=std::char_traits<char>::length(found->text);
            continue;
        }

        const char c=pattern[i];

        // a letter that is no section, or anything outside ASCII, is no
        // separator: separators are what the parse reads as none
        if(isAsciiLetter(c) || isDigit(c) || (static_cast<unsigned char>(c) >= 0x80))
            return false;
        if(pieces.empty() || (pieces.back().section >= 0))
            pieces.push_back({-1, String()});
        pieces.back().separator+=c;
        ++i;
    }

    auto has=[&](SectionKind kind) { return hasKind[static_cast<int>(kind)]; };
    const bool date=has(SectionKind::Year) || has(SectionKind::Month) || has(SectionKind::Day);
    const bool time=has(SectionKind::Hour24) || has(SectionKind::Hour12) || has(SectionKind::Minute)
        || has(SectionKind::AmPm);

    if((date == time) || !formatAllowed(date))
        return false;
    if(date && !(has(SectionKind::Year) && has(SectionKind::Month) && has(SectionKind::Day)))
        return false;
    if(time)
    {
        if(has(SectionKind::Hour24) == has(SectionKind::Hour12))
            return false;
        if(!has(SectionKind::Minute) || (has(SectionKind::AmPm) != has(SectionKind::Hour12)))
            return false;
        // the order TimeOfDay::fromText reads: the hour before the minute,
        // AP after both, so the field can read back what it shows
        if(hourAt > minuteAt)
            return false;
        if(has(SectionKind::AmPm) && (sections.back().kind != SectionKind::AmPm))
            return false;
    }

    m_format=pattern;
    m_dateFormat=date;
    m_sections=std::move(sections);
    m_pieces=std::move(pieces);
    m_width=-1;
    invalidate();
    showValue();
    return true;
}

void SectionedEdit::setFieldValue(const std::optional<Value> &value)
{
    if((!value && !m_allowEmpty) || (value && !isValidValue(*value)))
        return;

    m_value=value?std::optional<Value>(clampValue(*value)):std::nullopt;
    showValue();
}

void SectionedEdit::setFieldRange(const std::optional<Value> &min, const std::optional<Value> &max)
{
    if((min && !isValidValue(*min)) || (max && !isValidValue(*max)))
        return;

    m_min=min;
    m_max=max;
    if(m_min && m_max && (*m_max < *m_min))
        std::swap(m_min, m_max);

    if(m_value)
        m_value=clampValue(*m_value);
    showValue();
}

void SectionedEdit::setAllowEmpty(bool allow)
{
    m_allowEmpty=allow;

    if(!allow && !m_value)
    {
        m_value=clampValue(m_emptyStepValue);
        showValue();
    }
}

void SectionedEdit::setEmptyStepValue(const Value &value)
{
    if(!isValidValue(value))
        return;
    m_emptyStepValue=value;
}

void SectionedEdit::setMinuteStep(int step)
{
    if((step < 1) || (step > 60))
        return;
    m_minuteStep=step;
}

SectionedEdit::Value SectionedEdit::clampValue(const Value &value) const
{
    Value clamped=value;

    if(m_min && (clamped < *m_min))
        clamped=*m_min;
    if(m_max && (*m_max < clamped))
        clamped=*m_max;
    // a date's year is 1..9999 whatever the range: the format's 4 digits
    if(m_dateFormat)
    {
        if(clamped.date < MinDate)
            clamped.date=MinDate;
        else if(MaxDate < clamped.date)
            clamped.date=MaxDate;
    }
    return clamped;
}

String SectionedEdit::formatValue(const std::optional<Value> &value) const
{
    if(!value)
        return String();

    String text;
    char buffer[24];

    for(const Piece &piece : m_pieces)
    {
        if(piece.section < 0)
        {
            text+=piece.separator;
            continue;
        }

        const Section &section=m_sections[piece.section];
        int number=0;

        switch(section.kind)
        {
        case SectionKind::Year: number=value->date.year; break;
        case SectionKind::Month: number=value->date.month; break;
        case SectionKind::Day: number=value->date.day; break;
        case SectionKind::Hour24: number=value->time.hour; break;
        case SectionKind::Hour12: number=value->time.hour12(); break;
        case SectionKind::Minute: number=value->time.minute; break;
        case SectionKind::AmPm:
            text+=value->time.isPm()?"PM":"AM";
            continue;
        }

        std::snprintf(buffer, sizeof(buffer), "%0*d", section.digits, number);
        text+=buffer;
    }
    return text;
}

std::optional<SectionedEdit::Value> SectionedEdit::parse(const String &text) const
{
    if(!m_dateFormat)
    {
        const std::optional<TimeOfDay> time=TimeOfDay::fromText(text);

        if(!time)
            return std::nullopt;

        Value value;
        value.time=*time;
        return value;
    }

    std::vector<String> groups;
    bool inGroup=false;

    for(const char c : text)
    {
        if(isDigit(c))
        {
            if(!inGroup)
                groups.emplace_back();
            inGroup=true;
            groups.back()+=c;
            continue;
        }
        if(isAsciiLetter(c) || (static_cast<unsigned char>(c) >= 0x80))
            return std::nullopt;
        inGroup=false;
    }

    // one run of digits is split at the format's widths: 8 digits with a
    // 4-digit year, 6 with a 2-digit one
    if((groups.size() == 1) && ((groups[0].size() == 8) || (groups[0].size() == 6)))
    {
        const String digits=groups[0];
        const size_t yearDigits=(digits.size() == 8)?4:2;
        size_t at=0;

        groups.clear();
        for(const Section &section : m_sections)
        {
            const size_t length=(section.kind == SectionKind::Year)?yearDigits:2;

            groups.push_back(digits.substr(at, length));
            at+=length;
        }
    }

    if(groups.size() != m_sections.size())
        return std::nullopt;

    Value value;
    Date &date=value.date;

    for(size_t i=0; i < m_sections.size(); ++i)
    {
        const String &group=groups[i];
        const bool year=(m_sections[i].kind == SectionKind::Year);

        if(group.size() > (year?4u:2u))
            return std::nullopt;

        int number=std::atoi(group.c_str());

        switch(m_sections[i].kind)
        {
        case SectionKind::Year:
            date.year=(group.size() <= 2)?2000+number:number;
            break;
        case SectionKind::Month: date.month=number; break;
        case SectionKind::Day: date.day=number; break;
        default: break;
        }
    }

    if(!date.isValid())
        return std::nullopt;
    return value;
}

int SectionedEdit::sectionAt(int caret) const
{
    if(m_sections.empty())
        return -1;

    const std::vector<std::pair<int, int>> runs=alnumRuns(getText());
    int index=static_cast<int>(runs.size())-1;

    for(size_t i=0; i < runs.size(); ++i)
    {
        if(caret <= runs[i].second)
        {
            index=static_cast<int>(i);
            break;
        }
    }
    return std::clamp(index, 0, static_cast<int>(m_sections.size())-1);
}

SectionedEdit::Value SectionedEdit::stepValue(const Value &value, SectionKind section, int direction,
    bool large) const
{
    Value next=value;
    Date &date=next.date;
    TimeOfDay &time=next.time;

    switch(section)
    {
    case SectionKind::Year:
        date.year+=direction*(large?10:1);
        clampDay(date);
        break;
    case SectionKind::Month:
        if(large)
            date=date.addYears(direction);
        else
        {
            date.month=wrapInto(date.month-1+direction, 12)+1;
            clampDay(date);
        }
        break;
    case SectionKind::Day:
        if(large)
            date=date.addYears(direction);
        else
            date.day=wrapInto(date.day-1+direction, Date::daysInMonth(date.year, date.month))+1;
        break;
    case SectionKind::Hour24:
    case SectionKind::Hour12:
        time.hour=wrapInto(time.hour+direction*(large?12:1), 24);
        break;
    case SectionKind::AmPm:
        time.hour=wrapInto(time.hour+12, 24);
        break;
    case SectionKind::Minute:
    {
        const int step=m_minuteStep;
        // a large step is ten minutes, rounded up to a multiple of the step
        const int amount=large?step*((10+step-1)/step):step;
        int minute=time.minute;

        // from the multiple of the step at or beyond the minute, in the
        // step's direction: a typed 07 steps to 15 or 0 by 15
        if(direction > 0)
            minute=(minute/step)*step+amount;
        else
            minute=((minute+step-1)/step)*step-amount;
        time.minute=wrapInto(minute, 60);
        break;
    }
    }
    return next;
}

bool SectionedEdit::stepSection(int direction, bool large)
{
    if(m_sections.empty())
        return false;

    // the section is the caret's in the text as typed
    const int section=sectionAt(m_cursorPos);

    if(getText() != formatValue(m_value))
        commit();

    if(!m_value)
    {
        Value start=clampValue(m_emptyStepValue);

        // a date searched a day at a time (the filter refuses days), a time
        // a minute step at a time
        for(int i=0; (i < 1000) && !acceptsValue(start); ++i)
        {
            if(m_dateFormat)
                start.date=start.date.addDays(1);
            else
                start=stepValue(start, SectionKind::Minute, 1, false);
            start=clampValue(start);
        }
        return acceptsValue(start) && userSetValue(start);
    }

    const SectionKind kind=m_sections[section].kind;
    const Value current=*m_value;
    Value stepped=current;

    // on in the step's direction until a value the range and the filter take,
    // or back where it began
    // a year or month step (the day section's PageUp and PageDown are a
    // year's) goes on from the day it began with, clamped to each month's
    // last only for the value tried: Jan 30 refused on Feb 28 tries Mar 30,
    // not Mar 28
    const bool keepDay=(kind == SectionKind::Year) || (kind == SectionKind::Month)
        || ((kind == SectionKind::Day) && large);

    for(int i=0; i < 1000; ++i)
    {
        stepped=stepValue(stepped, kind, direction, large);
        if(keepDay)
            stepped.date.day=current.date.day;

        Value tried=stepped;

        clampDay(tried.date);

        const Value clamped=clampValue(tried);

        if((clamped != current) && acceptsValue(clamped))
            return userSetValue(clamped);
        if(stepped == current)
            break;
    }
    return false;
}

bool SectionedEdit::userSetValue(const std::optional<Value> &value)
{
    std::optional<Value> next=value?std::optional<Value>(clampValue(*value)):std::nullopt;

    if((!next && !m_allowEmpty) || (next && !acceptsValue(*next)))
    {
        showValue();
        return false;
    }

    const bool changed=(next != m_value);

    m_value=next;
    showValue();
    if(changed)
        onValueChanged.call(this);
    return true;
}

bool SectionedEdit::commit()
{
    const String text=getText();

    if(text == formatValue(m_value))
        return true;

    if(isBlank(text))
    {
        if(m_allowEmpty)
            return userSetValue(std::nullopt);
        revert();
        return false;
    }

    const std::optional<Value> value=parse(text);

    if(!value)
    {
        revert();
        return false;
    }
    return userSetValue(*value);
}

void SectionedEdit::revert()
{
    showValue();
}

void SectionedEdit::showValue()
{
    const String text=formatValue(m_value);
    const String &old=getText();

    if(text == old)
        return;

    // the caret stays in its section, as far into it as it was
    const int section=sectionAt(m_cursorPos);
    const std::vector<std::pair<int, int>> oldRuns=alnumRuns(old);
    int offset=0;

    if((section >= 0) && (section < static_cast<int>(oldRuns.size())))
        offset=std::clamp(m_cursorPos-oldRuns[section].first, 0, oldRuns[section].second-oldRuns[section].first);

    setText(text);

    const std::vector<std::pair<int, int>> runs=alnumRuns(text);
    int caret=static_cast<int>(text.size());

    if((section >= 0) && (section < static_cast<int>(runs.size())))
        caret=runs[section].first+std::min(offset, runs[section].second-runs[section].first);

    m_cursorPos=caret;
    m_cursorEnd=caret;
    if(isFocussed())
        refreshCursorBounds();
    else
        updateCursorRects();
}

bool SectionedEdit::onKeyUp(bool down)
{
    if(down && m_editable && !isDisabled())
        stepSection(1, false);
    return true;
}

bool SectionedEdit::onKeyDown(bool down)
{
    if(down && m_editable && !isDisabled())
        stepSection(-1, false);
    return true;
}

bool SectionedEdit::onKeyPageUp(bool down)
{
    if(down && m_editable && !isDisabled())
        stepSection(1, true);
    return true;
}

bool SectionedEdit::onKeyPageDown(bool down)
{
    if(down && m_editable && !isDisabled())
        stepSection(-1, true);
    return true;
}

bool SectionedEdit::onKeyReturn(bool down)
{
    if(down)
        commit();
    return ParentClass::onKeyReturn(down);
}

bool SectionedEdit::onKeyEscape(bool down)
{
    if(getText() == formatValue(m_value))
        return ParentClass::onKeyEscape(down);

    if(down)
        revert();
    return true;
}

bool SectionedEdit::onChar(gweni::UnicodeChar c)
{
    const bool am=(c == 'a') || (c == 'A');
    const bool pm=(c == 'p') || (c == 'P');

    if((am || pm) && m_editable && !isDisabled())
    {
        const int section=sectionAt(m_cursorPos);

        if((section >= 0) && (m_sections[section].kind == SectionKind::AmPm)
            && (getText() == formatValue(m_value)))
        {
            Value value=m_value?*m_value:clampValue(m_emptyStepValue);

            value.time.hour=TimeOfDay::hourFrom12(value.time.hour12(), pm);
            userSetValue(value);
            return true;
        }
    }
    return ParentClass::onChar(c);
}

void SectionedEdit::onLostKeyboardFocus()
{
    commit();
    ParentClass::onLostKeyboardFocus();
}

void SectionedEdit::setFont(const gweni::Font &font)
{
    ParentClass::setFont(font);
    m_width=-1;
    invalidate();
}

int SectionedEdit::formatWidth()
{
    const renderer::Base *renderer=getSkin()?getSkin()->getRenderer():nullptr;
    const unsigned serial=renderer?renderer->textLayoutSerial():0;

    if((m_width >= 0) && (m_widthFont == &getFont()) && (m_widthSerial == serial))
        return m_width;

    const gweni::Font &font=getFont();
    const String ampm=(measureText(font, "PM").x >= measureText(font, "AM").x)?"PM":"AM";
    int width=0;

    // the widest digit in every place: a face's digits may differ in width
    for(char digit='0'; digit <= '9'; ++digit)
    {
        String sample;

        for(const Piece &piece : m_pieces)
        {
            if(piece.section < 0)
                sample+=piece.separator;
            else if(m_sections[piece.section].kind == SectionKind::AmPm)
                sample+=ampm;
            else
                sample.append(std::max(2, m_sections[piece.section].digits), digit);
        }
        width=std::max(width, measureText(font, sample).x);
    }

    m_widthFont=&font;
    m_widthSerial=serial;
    m_width=width;
    return width;
}

void SectionedEdit::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    if(dim != Dim::X)
        return;

    int docked=0;

    for(auto &&child : getChildren())
    {
        if(child == m_text || child->hidden() || !child->shouldIncludeInSize())
            continue;

        if(child->getDock() & (DockPosition::Left|DockPosition::Right))
            docked+=child->getPreferredSize().width+child->getMargin().left+child->getMargin().right;
    }

    const Margin &margin=m_text->getMargin();

    // the format's widest text, the caret's unit after it and the padding:
    // the request a parent reads too (requestedSizeExtra)
    m_fieldWidth=formatWidth()+1+margin.left+margin.right+m_padding.left+m_padding.right+docked;
    m_preferredSize.width=m_fieldWidth;
}

int SectionedEdit::requestedSizeExtra(Dim dim) const
{
    if((dim == Dim::X) && (m_fieldWidth >= 0))
        return m_fieldWidth-getRequestedBounds().w;
    return ParentClass::requestedSizeExtra(dim);
}

}//namespace internal
}//namespace controls
}//namespace gweni
