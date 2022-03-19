/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/skin.h>
#include <gweni/controls/button.h>
#include <gweni/controls/imagePanel.h>

namespace gweni
{
namespace controls
{


GWENI_CONTROL_CONSTRUCTOR(Button)
{
    m_image=nullptr;
    m_depressed=false;
    m_centerImage=false;
    setSize(100, 20);
    setMouseInputEnabled(true);
    setIsToggle(false);
//    setAlignment(Position::Center);
    setAlignment(Alignment::Center);
    setTextPadding(Padding(3, 0, 3, 0));
    m_toggleStatus=false;
    setKeyboardInputEnabled(false);
    setTabable(false);

//    setText(getTypeName());
}


void Button::render(skin::Base *skin)
{
    Base::render(skin);

//    if(getStateChange() & StateChange_Created)
//    {
//        if(m_skinControl)
//        {
//            const std::vector<size_t> &primitives=m_skinControl->getPrimitives();
//
//            if(!primitives.empty())
//            {
//                std::string name=getTypeName();
//                
//                name+=" "+std::to_string(primitives[0]);
//                setText(name);
//            }
//        }
//    }
}

void Button::onMouseClickLeft(int /*x*/, int /*y*/, bool down)
{
    if(isDisabled())
        return;

    if(down)
    {
        setDepressed(true);
        MouseFocus=this;
        onDownCaller.call(this);
    }
    else
    {
        if(isHovered() && m_depressed)
        {
            onPress(event::Info(this));
        }

        setDepressed(false);
        MouseFocus=nullptr;
        onUpCaller.call(this);
    }
}

void Button::onMouseClickRight(int /*x*/, int /*y*/, bool down)
{
    if(isDisabled())
        return;

    if(down)
    {
        setDepressed(true);
        MouseFocus=this;
        onDownCaller.call(this);
    }
    else
    {
        if(isHovered() && m_depressed)
        {
            onRightPress(event::Info(this));
        }

        setDepressed(false);
        MouseFocus=nullptr;
        onUpCaller.call(this);
    }
}

void Button::setDepressed(bool b)
{
    if(m_depressed == b)
        return;

    m_depressed=b;
    redraw();
}

void Button::onRightPress(event::Info)
{
    onRightPressCaller.call(this);
}

void Button::onPress(event::Info)
{
    if(isToggle())
        setToggleState(!getToggleState());

    onPressCaller.call(this);
}

void Button::setImage(const String &name, bool center)
{
    if(name.empty())
    {
        if(m_image)
        {
            delete m_image;
            m_image=nullptr;
        }

        return;
    }

    if(!m_image)
        m_image=newChild<ImagePanel>();

    m_image->setImage(name);
    m_image->sizeToContents();
    m_image->setMargin(Margin(2, 0, 2, 0));
    m_centerImage=center;
    // Ugh.
    Padding padding=getTextPadding();
    padding.left=m_image->right()+2;
    setTextPadding(padding);
}

void Button::setToggleState(bool b)
{
    if(m_toggleStatus == b)
        return;

    m_toggleStatus=b;
    onToggleCaller.call(this);

    if(m_toggleStatus)
        onToggleOnCaller.call(this);
    else
        onToggleOffCaller.call(this);

    redraw();
}

void Button::sizeToContents()
{
    ParentClass::sizeToContents();

    if(m_image)
    {
        int height=m_image->getHeight()+4;

        if(getHeight() < height)
            setHeight(height);
    }
}

bool Button::onKeySpace(bool down)
{
    if(down)
    {
        onPress(event::Info(this));
    }

    return true;
}

void Button::acceleratePressed()
{
    onPress(event::Info(this));
}

void Button::updateColors()
{
    if(isDisabled())
        return setTextColor(getSkin()->Colors.Button.Disabled);

    if(isDepressed() || getToggleState())
        return setTextColor(getSkin()->Colors.Button.Down);

    if(isHovered())
        return setTextColor(getSkin()->Colors.Button.Hover);

    setTextColor(getSkin()->Colors.Button.Normal);
}

void Button::postLayout(skin::Base *skin)
{
    ParentClass::postLayout(skin);

    if(m_image)
    {
        if(m_centerImage)
            m_image->setPosition(Position::Center);
        else
            m_image->setPosition(Position::Left|Position::CenterV);
    }
}

void Button::onMouseDoubleClickLeft(int x, int y)
{
    if(isDisabled())
        return;

    onMouseClickLeft(x, y, true);
    onDoubleClickCaller.call(this);
}

void Button::setImageAlpha(float f)
{
    if(!m_image)
        return;

    m_image->setDrawColor(Color(255, 255, 255, 255.0f*f));
}

void Button::setAction(event::Handler *object, event::Caller::EventCaller function,
    const event::Packet &packet)
{
    onPressCaller.add(object, function, packet);
}


}//namespace controls
}//namespace gweni

