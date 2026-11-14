/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/switch.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>

#include <algorithm>
#include <cmath>

namespace gweni
{
namespace controls
{

// Its preferred size (Elastic): the track and the label beside it, whatever a
// Button's default request says; the label centred down it.
GWENI_CONTROL_CONSTRUCTOR(Switch)
{
    setIsToggle(true);
    setSizeFlags({SizeFlag::Elastic, SizeFlag::Elastic});
    // no request of its own (a Button asks for 100x20): a layout cell takes
    // the larger of the request and the preferred size
    setSize(0, 0);
    setTextPadding(Padding(0, 0, 0, 0));
    setDefaultTextAlignment(Alignment::Left|Alignment::CenterV);
    setKeyboardInputEnabled(true);
    setTabable(true);
    setCursor(cursor::Finger);
    updatePadding();
}

void Switch::setToggleState(bool on)
{
    if(on == getToggleState())
        return;

    // Button's: the state, its toggle callers and a redraw of this control
    ParentClass::setToggleState(on);
    m_thumbPosition=on?1.0f:0.0f;

    onToggled.call(this);
    if(on)
        onToggledOn.call(this);
    else
        onToggledOff.call(this);
}

void Switch::setLabelOnLeft(bool left)
{
    if(left == m_labelOnLeft)
        return;

    m_labelOnLeft=left;
    setDefaultTextAlignment(left?(Alignment::Right|Alignment::CenterV):(Alignment::Left|Alignment::CenterV));
    updatePadding();
    // the track moves to the other side
    addStateChange(StateChange_Background);
}

void Switch::setText(const String &str, bool doEvents)
{
    ParentClass::setText(str, doEvents);
    updatePadding();
}

// The track sits in the padding on the label's other side; with no label it
// is the whole control.
void Switch::updatePadding()
{
    const int side=TrackWidth+(getText().empty()?0:Gap);
    Padding padding(0, 0, 0, 0);

    if(m_labelOnLeft)
        padding.right=side;
    else
        padding.left=side;
    setPadding(padding);
}

// At least the track's height.
void Switch::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    if(dim == Dim::Y)
        m_preferredSize.height=std::max(m_preferredSize.height, TrackHeight);
    else
        m_preferredSize.width=std::max(m_preferredSize.width, TrackWidth);
}

Rect Switch::getTrackRect() const
{
    const Rect &bounds=getBounds();
    const int x=m_labelOnLeft?bounds.w-TrackWidth:0;

    return Rect(x, (bounds.h-TrackHeight)/2, TrackWidth, TrackHeight);
}

Rect Switch::getThumbRect() const
{
    const Rect track=getTrackRect();
    const int diameter=TrackHeight-2*ThumbInset;
    const int travel=TrackWidth-2*ThumbInset-diameter;
    const int offset=static_cast<int>(std::floor(m_thumbPosition*float(travel)+0.5f));

    return Rect(track.x+ThumbInset+offset, track.y+ThumbInset, diameter, diameter);
}

void Switch::onPress(event::Info info)
{
    if(isDisabled())
        return;

    // Button's: the toggle and onPressCaller
    ParentClass::onPress(info);
}

// On the release: a held key repeats its press, which would toggle it on and
// off. The release goes to the control the press went to.
bool Switch::onKeySpace(bool down)
{
    if(!down && !isDisabled())
        onPress(event::Info(this));

    return true;
}

bool Switch::onKeyReturn(bool down)
{
    return onKeySpace(down);
}

void Switch::acceleratePressed()
{
    if(!isDisabled())
        ParentClass::acceleratePressed();
}

// The label reads as a label whatever the state: Button's would change its
// colour (writing its glyphs) on a hover, a press and the on state.
void Switch::updateColors()
{
    if(isDisabled())
        return setTextColor(getSkin()->Colors.Button.Disabled);

    setSkinTextColor(internal::SkinTextColor::LabelDefault);
}

}//namespace controls
}//namespace gweni
