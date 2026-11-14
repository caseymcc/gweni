/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/rangeSlider.h>
#include <gweni/inputHandler.h>

#include <algorithm>
#include <cmath>
#include <iostream>

namespace gweni
{
namespace controls
{
namespace internal
{

GWENI_CONTROL_CONSTRUCTOR(RangeSliderThumb)
{
    setPlacement(Placement::Owner);
    setKeyboardInputEnabled(true);
    setTabable(true);
}

void RangeSliderThumb::setDepressed(bool depressed)
{
    if(m_depressed == depressed)
        return;

    m_depressed=depressed;
    redraw();
}

void RangeSliderThumb::onMouseClickLeft(int x, int y, bool down)
{
    if(m_slider)
        m_slider->thumbPressed(m_index, m_slider->canvasPosToLocal(Point(x, y)), down);
}

void RangeSliderThumb::onMouseMoved(int x, int y, int /*deltaX*/, int /*deltaY*/)
{
    if(m_slider)
        m_slider->thumbMoved(m_index, m_slider->canvasPosToLocal(Point(x, y)));
}

void RangeSliderThumb::onKeyboardFocus()
{
    if(m_slider)
        m_slider->thumbFocused(m_index);
}

bool RangeSliderThumb::onKeyLeft(bool down)
{
    if(down && m_slider)
        m_slider->thumbStepped(m_index, -1);
    return true;
}

bool RangeSliderThumb::onKeyRight(bool down)
{
    if(down && m_slider)
        m_slider->thumbStepped(m_index, 1);
    return true;
}

bool RangeSliderThumb::onKeyUp(bool down)
{
    if(down && m_slider)
        m_slider->thumbStepped(m_index, 1);
    return true;
}

bool RangeSliderThumb::onKeyDown(bool down)
{
    if(down && m_slider)
        m_slider->thumbStepped(m_index, -1);
    return true;
}

bool RangeSliderThumb::onKeyPageUp(bool down)
{
    if(down && m_slider)
        m_slider->thumbStepped(m_index, 10);
    return true;
}

bool RangeSliderThumb::onKeyPageDown(bool down)
{
    if(down && m_slider)
        m_slider->thumbStepped(m_index, -10);
    return true;
}

bool RangeSliderThumb::onKeyHome(bool down)
{
    if(down && m_slider)
        m_slider->thumbToLimit(m_index, false);
    return true;
}

bool RangeSliderThumb::onKeyend(bool down)
{
    if(down && m_slider)
        m_slider->thumbToLimit(m_index, true);
    return true;
}

}//namespace internal


const char *RangeSlider::JoinedSeparator=" \xE2\x80\x93 ";

GWENI_CONTROL_CONSTRUCTOR(RangeSlider)
{
    setBounds(Rect(0, 0, 128, 20));

    for(int i=0; i<2; ++i)
    {
        m_thumbs[i]=newChild<internal::RangeSliderThumb>();
        m_thumbs[i]->m_slider=this;
        m_thumbs[i]->m_index=i;
        m_thumbs[i]->setHorizontal(true);
    }
}

// Values a few ulps off the grid, from (v-min)/grid, still count as on it.
static constexpr double GridSlack=1e-9;

void RangeSlider::setHorizontal(bool horizontal)
{
    if(m_horizontal == horizontal)
        return;

    m_horizontal=horizontal;

    for(internal::RangeSliderThumb *thumb:m_thumbs)
        thumb->setHorizontal(horizontal);
    invalidate();
    redraw();
}

double RangeSlider::getGrid() const
{
    if(m_step > 0.0)
        return m_step;
    if(m_clampToNotches && m_numNotches > 0)
        return (m_max-m_min)/m_numNotches;
    return 0.0;
}

double RangeSlider::getKeyStep() const
{
    const double grid=getGrid();

    return grid > 0.0?grid:(m_max-m_min)/100.0;
}

double RangeSlider::snap(double value) const
{
    const double grid=getGrid();

    if(grid > 0.0)
        value=m_min+std::round((value-m_min)/grid)*grid;
    return std::clamp(value, m_min, m_max);
}

// A limit on the grid on its inner side, so a thumb stopped at it keeps the gap.
static double gridFloor(double value, double min, double grid)
{
    if(grid <= 0.0)
        return value;
    return min+std::floor((value-min)/grid+GridSlack)*grid;
}

static double gridCeil(double value, double min, double grid)
{
    if(grid <= 0.0)
        return value;
    return min+std::ceil((value-min)/grid-GridSlack)*grid;
}

double RangeSlider::getLowerLimit(int thumb) const
{
    if(!thumb)
        return m_min;
    return std::clamp(gridCeil(m_values[0]+m_gap, m_min, getGrid()), m_min, m_max);
}

double RangeSlider::getUpperLimit(int thumb) const
{
    if(thumb)
        return m_max;
    return std::clamp(gridFloor(m_values[1]-m_gap, m_min, getGrid()), m_min, m_max);
}

void RangeSlider::setRange(double min, double max)
{
    if(min > max)
        std::swap(min, max);
    if((min == m_min) && (max == m_max))
        return;

    m_min=min;
    m_max=max;
    m_gap=std::min(m_gap, m_max-m_min);
    applyConstraints();
    // the thumbs' places and the fill follow the range, the same values or not
    placeThumbs();
    redraw();
    // a vertical text column is measured from the range
    if(m_valueTexts[0])
        invalidate();
}

void RangeSlider::setValues(double low, double high)
{
    const double grid=getGrid();

    low=snap(low);
    high=snap(high);
    if(low > high)
        std::swap(low, high);
    if(high-low < m_gap)
    {
        high=std::min(m_max, gridCeil(low+m_gap, m_min, grid));
        if(high-low < m_gap)
            low=std::max(m_min, gridFloor(high-m_gap, m_min, grid));
    }

    if((low == m_values[0]) && (high == m_values[1]))
        return;

    m_values[0]=low;
    m_values[1]=high;
    placeThumbs();
    updateValueText();
    redraw();
}

void RangeSlider::applyConstraints()
{
    setValues(m_values[0], m_values[1]);
}

void RangeSlider::setMinimumGap(double gap)
{
    gap=std::clamp(gap, 0.0, m_max-m_min);
    if(gap == m_gap)
        return;

    m_gap=gap;
    applyConstraints();
}

void RangeSlider::setStep(double step)
{
    step=std::max(step, 0.0);
    if(step == m_step)
        return;

    m_step=step;
    applyConstraints();
}

void RangeSlider::setNotchCount(int count)
{
    if(count == m_numNotches)
        return;

    m_numNotches=count;
    applyConstraints();
    redraw();
}

void RangeSlider::setClampToNotches(bool clamp)
{
    if(clamp == m_clampToNotches)
        return;

    m_clampToNotches=clamp;
    applyConstraints();
    redraw();
}

bool RangeSlider::moveThumb(int thumb, double value)
{
    thumb=thumb?1:0;
    value=std::clamp(snap(value), getLowerLimit(thumb), getUpperLimit(thumb));

    if(value == m_values[thumb])
        return false;

    m_values[thumb]=value;
    placeThumb(thumb);
    updateValueText();
    // the fill
    redraw();

    event::Info info(this);

    info.integer=size_t(thumb);
    onValueChanged.call(this, info);
    if(thumb)
        onHighChanged.call(this, info);
    else
        onLowChanged.call(this, info);
    return true;
}

int RangeSlider::getTravel() const
{
    const Rect track=getTrackBounds();

    return std::max(0, (m_horizontal?track.w:track.h)-ThumbLength);
}

double RangeSlider::valueAt(int position) const
{
    const int travel=getTravel();

    if(travel <= 0)
        return m_min;

    double fraction=double(std::clamp(position, 0, travel))/travel;

    if(!m_horizontal)
        fraction=1.0-fraction;
    return m_min+fraction*(m_max-m_min);
}

int RangeSlider::positionOf(double value) const
{
    const double length=m_max-m_min;
    const double fraction=(length > 0.0)?(value-m_min)/length:0.0;

    return Slider::thumbOffset(getTravel(), m_horizontal?fraction:1.0-fraction);
}

Rect RangeSlider::getTrackBounds() const
{
    Rect r=getRenderBounds();

    if(m_valueTexts[0] && !m_horizontal)
        r.w=std::max(0, r.w-ValueTextGap-m_valueColumn);
    return r;
}

Rect RangeSlider::getFillBounds() const
{
    const Rect track=getTrackBounds();
    Rect low=getThumbRect(0);
    Rect high=getThumbRect(1);

    // the thumbs' rects are in this control's units, the track in its parent's
    low.x+=track.x;
    low.y+=track.y;
    high.x+=track.x;
    high.y+=track.y;

    if(m_horizontal)
    {
        const int from=low.x+low.w/2;

        return Rect(from, track.y, std::max(0, high.x+high.w/2-from), track.h);
    }

    const int from=high.y+high.h/2;

    return Rect(track.x, from, track.w, std::max(0, low.y+low.h/2-from));
}

Rect RangeSlider::getThumbRect(int thumb) const
{
    const int position=positionOf(m_values[thumb?1:0]);

    if(m_horizontal)
        return Rect(position, 0, ThumbLength, getHeight());
    return Rect(0, position, getTrackBounds().w, ThumbLength);
}

// A thumb's own bounds are the track's column, fixed: it draws, and is hit,
// at getThumbRect(), so a value change marks it and moves no control (a move
// would lay the way down to it out again).
void RangeSlider::placeThumb(int thumb)
{
    placeChild(m_thumbs[thumb], 0, 0, getTrackBounds().w, getHeight());
    m_thumbs[thumb]->redraw();
}

void RangeSlider::placeThumbs()
{
    placeThumb(0);
    placeThumb(1);
}

// The thumbs by their rects, the front one first; anywhere else this control.
Base *RangeSlider::getControlAt(int x, int y, bool onlyIfMouseEnabled)
{
    countHitTestVisit();

    if(hidden() || x < 0 || y < 0 || x >= getWidth() || y >= getHeight())
        return nullptr;

    for(int thumb:{m_front, 1-m_front})
    {
        const Rect r=getThumbRect(thumb);

        if(x >= r.x && y >= r.y && x < r.x+r.w && y < r.y+r.h && !m_thumbs[thumb]->hidden())
            return m_thumbs[thumb];
    }

    if(onlyIfMouseEnabled && !getMouseInputEnabled())
        return nullptr;
    return this;
}

void RangeSlider::layout(skin::Base * /*skin*/)
{
    // a resize moves the travel, so the thumbs go back to their values
    placeThumbs();
    placeValueText();
}

// As a Slider's: the text's height is final in the Y pass, after layout().
void RangeSlider::arrange(skin::Base *skin, Dim dim)
{
    if(dim == Dim::Y)
        placeValueText();
    ParentClass::arrange(skin, dim);
}

void RangeSlider::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    if(!m_valueTexts[0])
    {
        m_valueColumn=0;
        return;
    }

    if(m_horizontal)
    {
        // the text's line inside the track's 1-unit border, as a Slider's
        if(dim == Dim::Y)
            m_preferredSize.height=preferredFromContent(std::max(m_requestedBounds.h,
                m_valueTexts[0]->getPreferredSize().height+2), Dim::Y);
        return;
    }

    if(dim != Dim::X)
        return;

    // From the range, not the values shown, so a drag never changes it.
    const Font &font=m_valueTexts[0]->getFont();
    const String low=Slider::formatValue(m_valueFormat, m_min);
    const String high=Slider::formatValue(m_valueFormat, m_max);
    const int lowWidth=measureText(font, low).x;
    const int highWidth=measureText(font, high).x;

    if(m_textMode == ValueTextMode::Joined)
    {
        const String &widest=(lowWidth >= highWidth)?low:high;

        m_valueColumn=measureText(font, widest+JoinedSeparator+widest).x;
    }
    else
        m_valueColumn=std::max(lowWidth, highWidth);
    m_preferredSize.width=preferredFromContent(m_requestedBounds.w+ValueTextGap+m_valueColumn, Dim::X);
}

void RangeSlider::placeValueText()
{
    if(!m_valueTexts[0])
        return;

    const Rect track=getTrackBounds();

    if(m_textMode == ValueTextMode::Joined)
    {
        internal::Text *text=m_valueTexts[0];
        const Size &size=text->getPreferredSize();
        const int y=(getHeight()-size.height)/2;

        if(m_horizontal)
            placeChild(text, (getWidth()-size.width)/2, y, size.width, size.height);
        else
            placeChild(text, track.w+ValueTextGap, y, size.width, size.height);
        return;
    }

    const Size &low=m_valueTexts[0]->getPreferredSize();
    const Size &high=m_valueTexts[1]->getPreferredSize();

    if(m_horizontal)
    {
        // each centred in its half of the field
        const int half=getWidth()/2;

        placeChild(m_valueTexts[0], (half-low.width)/2, (getHeight()-low.height)/2, low.width, low.height);
        placeChild(m_valueTexts[1], half+(getWidth()-half-high.width)/2, (getHeight()-high.height)/2,
            high.width, high.height);
        return;
    }

    // the high value at the column's top, the low one at its bottom
    const int x=track.w+ValueTextGap;

    placeChild(m_valueTexts[1], x, 0, high.width, high.height);
    placeChild(m_valueTexts[0], x, std::max(0, getHeight()-low.height), low.width, low.height);
}

String RangeSlider::joinedText(double low, double high) const
{
    return Slider::formatValue(m_valueFormat, low)+JoinedSeparator+Slider::formatValue(m_valueFormat, high);
}

// Text::setText marks the text, and lays it out again, only for a new string.
void RangeSlider::updateValueText()
{
    if(!m_valueTexts[0])
        return;

    if(m_textMode == ValueTextMode::Joined)
    {
        m_valueTexts[0]->setText(joinedText(m_values[0], m_values[1]));
        return;
    }

    for(int i=0; i<2; ++i)
        m_valueTexts[i]->setText(Slider::formatValue(m_valueFormat, m_values[i]));
}

void RangeSlider::syncValueTextControls()
{
    const size_t needed=m_valueFormat.empty()?0:(m_textMode == ValueTextMode::Joined?1:2);

    for(size_t i=0; i<2; ++i)
    {
        if((i < needed) && !m_valueTexts[i])
        {
            // after the thumbs, so they are numbered, and drawn, over them
            m_valueTexts[i]=newChild<internal::Text>();
            m_valueTexts[i]->setPlacement(Placement::Owner);
            // a value of the same width redraws the glyphs and lays nothing out
            m_valueTexts[i]->setSameSizeKeepsLayout(true);
        }
        else if((i >= needed) && m_valueTexts[i])
        {
            delete m_valueTexts[i];
            m_valueTexts[i]=nullptr;
        }
    }

    if(!needed)
        m_valueColumn=0;
    updateColors();
    updateValueText();
    // the column (vertical) or the least height (horizontal) is the format's
    invalidate();
}

bool RangeSlider::setValueFormat(const String &format)
{
    if(format == m_valueFormat)
        return true;

    if(!format.empty() && !Slider::validValueFormat(format))
    {
        std::cerr<<"gweni: RangeSlider::setValueFormat: \""<<format<<"\" refused: it must hold one "
            "conversion of a double (%f, %e, %g or %a, with flags, a width and a precision)\n";
        return false;
    }

    m_valueFormat=format;
    syncValueTextControls();
    return true;
}

void RangeSlider::setValueTextMode(ValueTextMode mode)
{
    if(mode == m_textMode)
        return;

    m_textMode=mode;
    // the old texts go, so the new ones start from their own strings
    const String format=m_valueFormat;

    m_valueFormat.clear();
    syncValueTextControls();
    m_valueFormat=format;
    syncValueTextControls();
}

internal::Text *RangeSlider::getValueTextControl(int thumb) const
{
    if(m_textMode == ValueTextMode::Joined)
        return m_valueTexts[0];
    return m_valueTexts[thumb?1:0];
}

String RangeSlider::getValueText(int thumb) const
{
    internal::Text *text=getValueTextControl(thumb);

    return text?text->getText():String();
}

void RangeSlider::setDisabled(bool disabled)
{
    ParentClass::setDisabled(disabled);

    for(internal::RangeSliderThumb *thumb:m_thumbs)
    {
        thumb->setDisabled(disabled);
        thumb->setKeyboardInputEnabled(!disabled);
        thumb->setTabable(!disabled);
        if(disabled && (KeyboardFocus == thumb))
            thumb->unfocus();
    }

    if(disabled && (m_dragged >= 0))
        thumbPressed(m_dragged, Point(), false);
    updateColors();
}

// A Slider's: the label colour, the disabled button colour while disabled.
void RangeSlider::updateColors()
{
    for(internal::Text *text:m_valueTexts)
    {
        if(!text)
            continue;
        if(isDisabled())
            text->setTextColor(getSkin()->Colors.Button.Disabled);
        else
            text->setSkinTextColor(internal::SkinTextColor::LabelDefault);
    }
}

void RangeSlider::setFront(int thumb)
{
    if(thumb == m_front)
        return;

    m_front=thumb;
    m_thumbs[0]->redraw();
    m_thumbs[1]->redraw();
}

void RangeSlider::startDrag(int thumb, int hold)
{
    m_dragged=thumb;
    m_hold=hold;
    gweni::MouseFocus=m_thumbs[thumb];
    m_thumbs[thumb]->setDepressed(true);
    setFront(thumb);
}

void RangeSlider::thumbPressed(int thumb, Point local, bool down)
{
    if(!down)
    {
        if(m_dragged >= 0)
            m_thumbs[m_dragged]->setDepressed(false);
        if((m_dragged >= 0) && (gweni::MouseFocus == m_thumbs[m_dragged]))
            gweni::MouseFocus=nullptr;
        m_dragged=-1;
        m_pending=false;
        return;
    }

    if(isDisabled())
        return;

    // two thumbs at one value: the drag's first move picks
    m_pending=(m_values[0] == m_values[1]);
    m_pressAt=along(local);
    const Rect rect=getThumbRect(thumb);

    startDrag(thumb, along(local)-(m_horizontal?rect.x:rect.y));
}

void RangeSlider::thumbMoved(int thumb, Point local)
{
    if((m_dragged < 0) || (thumb != m_dragged))
        return;

    const int at=along(local);

    if(m_pending)
    {
        const int moved=at-m_pressAt;

        if(moved == 0)
            return;

        // toward the minimum: left, or down a vertical slider
        const bool towardMin=m_horizontal?(moved < 0):(moved > 0);
        const int pick=towardMin?0:1;

        m_pending=false;
        if(pick != m_dragged)
        {
            m_thumbs[m_dragged]->setDepressed(false);
            startDrag(pick, m_hold);
            m_thumbs[pick]->focus();
        }
    }

    moveThumb(m_dragged, valueAt(at-m_hold));
}

void RangeSlider::onMouseClickLeft(int x, int y, bool down)
{
    if(!down)
    {
        if(m_dragged >= 0)
            thumbPressed(m_dragged, Point(), false);
        return;
    }

    if(isDisabled())
        return;

    const int at=along(canvasPosToLocal(Point(x, y)));
    // the thumb centred on the press
    const double value=snap(valueAt(at-ThumbLength/2));
    const double lowDistance=std::abs(value-m_values[0]);
    const double highDistance=std::abs(value-m_values[1]);
    int pick;

    if(lowDistance < highDistance)
        pick=0;
    else if(highDistance < lowDistance)
        pick=1;
    else if(value < m_values[0])
        pick=0;
    else if(value > m_values[1])
        pick=1;
    else if(m_values[0] == m_values[1])
    {
        // on both thumbs, at their value: the drag's first move picks
        m_pressAt=at;
        m_pending=true;
        startDrag(m_front, ThumbLength/2);
        m_thumbs[m_front]->focus();
        return;
    }
    else
        // halfway between them: the low one, if it can get there
        pick=(value <= getUpperLimit(0))?0:1;

    m_pending=false;
    moveThumb(pick, value);
    m_pressAt=at;
    startDrag(pick, ThumbLength/2);
    m_thumbs[pick]->focus();
}

void RangeSlider::thumbStepped(int thumb, int steps)
{
    if(isDisabled())
        return;
    moveThumb(thumb, m_values[thumb?1:0]+steps*getKeyStep());
}

void RangeSlider::thumbToLimit(int thumb, bool upper)
{
    if(isDisabled())
        return;
    moveThumb(thumb, upper?getUpperLimit(thumb):getLowerLimit(thumb));
}

void RangeSlider::thumbFocused(int thumb)
{
    setFront(thumb);
}

}//namespace controls
}//namespace gweni
