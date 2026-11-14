/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/switch.h>
#include <gweni/anim.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>

#include <algorithm>
#include <cmath>

namespace gweni
{
namespace controls
{

namespace
{

//! A switch's slide: one step a frame while the thumb moves.
class SwitchSlide: public anim::Animation
{
public:
    void think() override
    {
        gweni_cast<Switch>(m_control)->slideStep();
    }
};

}//namespace

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
    // A caller may have turned it back; that nested change owns its slide
    // and notifications, so the superseded call stops here.
    if(getToggleState() != on)
        return;
    moveThumb();

    onToggled.call(this);
    if(getToggleState() != on)
        return;
    if(on)
        onToggledOn.call(this);
    else
        onToggledOff.call(this);
}

// A slide from where the thumb is, as long as the part of the travel left
// takes, or a jump when nothing would show it: not visible(), or not drawn yet
// (the first frame draws the thumb where it ends).
void Switch::moveThumb()
{
    const float target=getToggleState()?1.0f:0.0f;

#if GWENI_ANIMATE
    if(visible() && m_drawn && getCanvas())
    {
        m_slideFrom=m_thumbPosition;
        m_slideStart=platform::getTimeInSeconds();
        m_slideLength=SlideSeconds*std::fabs(double(target-m_slideFrom));
        if(!m_slide)
        {
            m_slide=new SwitchSlide();
            anim::add(this, m_slide);
        }
        return;
    }
#endif
    stopSlide();
    placeThumb(target);
}

// Eased in and out (a half cosine), from m_slideFrom at m_slideStart to the
// state's end m_slideLength later. A switch not visible() ends where it goes.
void Switch::slideStep()
{
    const float target=getToggleState()?1.0f:0.0f;
    const double elapsed=platform::getTimeInSeconds()-m_slideStart;

    if(!visible() || (elapsed >= m_slideLength) || (m_slideLength <= 0.0))
    {
        stopSlide();
        placeThumb(target);
        return;
    }

    const double t=std::max(0.0, elapsed/m_slideLength);
    const double eased=0.5-0.5*std::cos(3.14159265358979323846*t);

    placeThumb(static_cast<float>(m_slideFrom+(target-m_slideFrom)*eased));
}

void Switch::stopSlide()
{
    if(!m_slide)
        return;

    anim::cancel(this, m_slide);
    m_slide=nullptr;
}

// A step of less than a unit leaves the thumb's rect, and its slot, as they
// are; the toggle itself marked the switch (its colours changed).
void Switch::placeThumb(float position)
{
    m_thumbPosition=position;

    const Rect thumb=getThumbRect();

    if(thumb == m_thumbMarked)
        return;

    m_thumbMarked=thumb;
    addStateChange(StateChange_Background);
}

void Switch::render(skin::Base *skin)
{
    ParentClass::render(skin);
    m_drawn=true;
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

void Switch::setDisabled(bool disabled)
{
    ParentClass::setDisabled(disabled);
    setCursor(disabled?cursor::Normal:cursor::Finger);
    setKeyboardInputEnabled(!disabled);
    setTabable(!disabled);
    if(disabled && (KeyboardFocus == this))
        unfocus();
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
