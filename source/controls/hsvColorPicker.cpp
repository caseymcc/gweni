/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/hsvColorPicker.h>
#include <gweni/controls/colorControls.h>
#include <gweni/controls/colorPicker.h>
#include <gweni/controls/textBox.h>
#include <gweni/controls/label.h>
#include <gweni/utility.h>

namespace gweni
{
namespace controls
{


GWENI_CONTROL_CONSTRUCTOR(HSVColorPicker)
{
    setMouseInputEnabled(true);
    setSize(256, 64);
    //    enableCacheToTexture();   // TODO - fix texture caching.

    m_lerpBox=newChild<controls::ColorLerpBox>();
    m_lerpBox->onSelectionChanged.add(this, &HSVColorPicker::colorBoxChanged);
    m_lerpBox->dock(Position::Left);

    m_colorSlider=newChild<controls::ColorSlider>();
    m_colorSlider->setPos(m_lerpBox->getWidth()+15, 5);
    m_colorSlider->onSelectionChanged.add(this, &HSVColorPicker::colorSliderChanged);
    m_colorSlider->dock(Position::Left);

    m_after=newChild<internal::ColorDisplay>();
    m_after->setSize(48, 24);
    m_after->setPos(m_colorSlider->getX()+m_colorSlider->getWidth()+15, 5);

    m_before=newChild<internal::ColorDisplay>();
    m_before->setSize(48, 24);
    m_before->setPos(m_after->getX(), 28);

    int x=m_before->getX();
    int y=m_before->getY()+30;
    {
        Label *label=newChild<Label>();
        label->setText("R:");
        label->sizeToContents();
        label->setPos(x, y);

        TextBoxNumeric *numeric=newChild<TextBoxNumeric>();
        numeric->setName("RedBox");
        numeric->setPos(x+15, y-1);
        numeric->setSize(26, 16);
        numeric->setSelectAllOnFocus(true);
        numeric->onTextChangedCaller.add(this, &HSVColorPicker::onNumericTyped);
    }
    y+=20;
    {
        Label *label=newChild<Label>();
        label->setText("G:");
        label->sizeToContents();
        label->setPos(x, y);

        TextBoxNumeric *numeric=newChild<TextBoxNumeric>();
        numeric->setName("GreenBox");
        numeric->setPos(x+15, y-1);
        numeric->setSize(26, 16);
        numeric->setSelectAllOnFocus(true);
        numeric->onTextChangedCaller.add(this, &HSVColorPicker::onNumericTyped);
    }
    y+=20;
    {
        Label *label=newChild<Label>();
        label->setText("B:");
        label->sizeToContents();
        label->setPos(x, y);

        TextBoxNumeric *numeric=newChild<TextBoxNumeric>();
        numeric->setName("BlueBox");
        numeric->setPos(x+15, y-1);
        numeric->setSize(26, 16);
        numeric->setSelectAllOnFocus(true);
        numeric->onTextChangedCaller.add(this, &HSVColorPicker::onNumericTyped);
    }
}

void HSVColorPicker::onNumericTyped(event::Info info)
{
    TextBoxNumeric *box=gweni_cast<TextBoxNumeric>(info.controlCaller);

    if(!box)
        return;

    if(box->getText() == "")
        return;

    int textValue=clamp(atoi(box->getText().c_str()), 0, 255);

    Color newColor=getColor();

    if(box->getName().find("Red") != String::npos)
        newColor.r=textValue;
    else if(box->getName().find("Green") != String::npos)
        newColor.g=textValue;
    else if(box->getName().find("Blue") != String::npos)
        newColor.b=textValue;
    else if(box->getName().find("Alpha") != String::npos)
        newColor.a=textValue;

    setColor(newColor);
}

void HSVColorPicker::updateControls(Color color)
{
    // What in the FUCK
    TextBoxNumeric *redBox=gweni_cast<TextBoxNumeric>(findChildByName("RedBox", false));

    // if you need to deal with pesky __int8 or char displaying as character instead of value, see:
    // https://stackoverflow.com/questions/19562103/uint8-t-cant-be-printed-with-cout/#21389821

    if(redBox)
        redBox->setText(toString(+color.r), false);

    TextBoxNumeric *greenBox=gweni_cast<TextBoxNumeric>(findChildByName("GreenBox", false));

    if(greenBox)
        greenBox->setText(toString(+color.g), false);

    TextBoxNumeric *blueBox=gweni_cast<TextBoxNumeric>(findChildByName("BlueBox", false));

    if(blueBox)
        blueBox->setText(toString(+color.b), false);

    m_after->setColor(color);
}

void HSVColorPicker::setColor(Color color, bool onlyHue, bool reset)
{
    updateControls(color);

    if(reset)
        m_before->setColor(color);

    m_colorSlider->setColor(color);
    m_lerpBox->setColor(color, onlyHue);
    m_after->setColor(color);
}

Color HSVColorPicker::getColor()
{
    return m_lerpBox->getSelectedColor();
}

void HSVColorPicker::colorBoxChanged(event::Info)
{
    onColorChanged.call(this);
    updateControls(getColor());
    invalidate();
}

void HSVColorPicker::colorSliderChanged(event::Info)
{
    if(m_lerpBox)
        m_lerpBox->setColor(m_colorSlider->getSelectedColor(), true);

    invalidate();
}

}//namespace controls
}//namespace gweni
