/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/colorPicker.h>
#include <gweni/controls/horizontalSlider.h>
#include <gweni/controls/groupBox.h>
#include <gweni/controls/textBox.h>
#include <gweni/utility.h>

namespace gweni
{
namespace controls
{

static const int gc_pickChannelHeight=40;     // As small as boxes will go without clipping.


GWENI_CONTROL_CONSTRUCTOR(ColorPicker)
{
    setMouseInputEnabled(true);
    setSize(256, 150);
    createControls();
    setColor(gweni::Color(50, 60, 70, 255));
}

void ColorPicker::createColorControl(gweni::String name, int y)
{
    const int colorSize=10;

//    GroupBox *colorGroup=new GroupBox(this);
    GroupBox *colorGroup=newChild<GroupBox>();

    colorGroup->setPos(10, y);
    colorGroup->setText(name);
    colorGroup->setSize(160, gc_pickChannelHeight);
    colorGroup->setName(name+"groupbox");

//    internal::ColorDisplay *disp=new internal::ColorDisplay(colorGroup);
    internal::ColorDisplay *disp=colorGroup->newChild<internal::ColorDisplay>();

    disp->setName(name);
    disp->setBounds(0, 0, colorSize, colorSize);

//    TextBoxNumeric *numeric=new TextBoxNumeric(colorGroup);
    TextBoxNumeric *numeric=colorGroup->newChild<TextBoxNumeric>();

    numeric->setName(name+"Box");
    numeric->setPos(105, 0);
    numeric->setSize(26, 16);
    numeric->setSelectAllOnFocus(true);
    numeric->onTextChangedCaller.add(this, &ColorPicker::onNumericTyped);

//    HorizontalSlider *slider=new HorizontalSlider(colorGroup);
    HorizontalSlider *slider=colorGroup->newChild<HorizontalSlider>();

    slider->setPos(colorSize+5, 0);
    slider->setRange(0, 255);
    slider->setSize(80, std::max(colorSize, 15));
    slider->setName(name+"Slider");
    slider->onValueChanged.add(this, &ColorPicker::onSlidersMoved);
}

void ColorPicker::onNumericTyped(event::Info info)
{
    TextBoxNumeric *box=gweni_cast<TextBoxNumeric>(info.controlCaller);

    if(!box)
        return;

    if(box->getText() == "")
        return;

    const int textValue=clamp(atoi(box->getText().c_str()), 0, 255);

    if(box->getName().find("Red") != gweni::String::npos)
        setRed(textValue);

    if(box->getName().find("Green") != gweni::String::npos)
        setGreen(textValue);

    if(box->getName().find("Blue") != gweni::String::npos)
        setBlue(textValue);

    if(box->getName().find("Alpha") != gweni::String::npos)
        setAlpha(textValue);

    updateControls();
}

void ColorPicker::setColor(gweni::Color color)
{
    m_color=color;
    updateControls();
}

void ColorPicker::createControls()
{
    const int startY=0;

    createColorControl("Red", startY);
    createColorControl("Green", startY+gc_pickChannelHeight);
    createColorControl("Blue", startY+gc_pickChannelHeight*2);
    createColorControl("Alpha", startY+gc_pickChannelHeight*3);

//    GroupBox *finalGroup=new GroupBox(this);
    GroupBox *finalGroup=newChild<GroupBox>();

    finalGroup->setPos(180, 40);
    finalGroup->setSize(60, 60);
    finalGroup->setText("Result");
    finalGroup->setName("ResultGroupBox");

//    internal::ColorDisplay *disp=new internal::ColorDisplay(finalGroup);
    internal::ColorDisplay *disp=finalGroup->newChild<internal::ColorDisplay>();

    disp->setName("Result");
    disp->setBounds(0, 10, 32, 32);
    disp->setDrawCheckers(true);
    // updateControls();
}

void ColorPicker::updateColorControls(gweni::String name, gweni::Color col, int sliderVal)
{
    internal::ColorDisplay *disp=gweni_cast<internal::ColorDisplay>(findChildByName(name, true));
    disp->setColor(col);

    HorizontalSlider *slider=gweni_cast<HorizontalSlider>(findChildByName(name+"Slider", true));
    slider->setFloatValue(sliderVal);

    TextBoxNumeric *box=gweni_cast<TextBoxNumeric>(findChildByName(name+"Box", true));
    box->setText(gweni::toString(sliderVal));
}

void ColorPicker::updateControls()
{
    // This is a little weird, but whatever for now
    updateColorControls("Red", Color(getColor().r, 0, 0, 255), getColor().r);
    updateColorControls("Green", Color(0, getColor().g, 0, 255), getColor().g);
    updateColorControls("Blue", Color(0, 0, getColor().b, 255), getColor().b);
    updateColorControls("Alpha", Color(255, 255, 255, getColor().a), getColor().a);

    internal::ColorDisplay *disp=gweni_cast<internal::ColorDisplay>(findChildByName("Result", true));
    disp->setColor(Color(getColor().r, getColor().g, getColor().b, getColor().a));
    onColorChanged.call(this);
}

void ColorPicker::onSlidersMoved(event::Info info)
{
    /*
      *HorizontalSlider *redSlider      = gweni_cast<HorizontalSlider>(   findChildByName(
      *"RedSlider",   true ) );
      *HorizontalSlider *greenSlider    = gweni_cast<HorizontalSlider>(   findChildByName(
      *"GreenSlider", true ) );
      *HorizontalSlider *blueSlider = gweni_cast<HorizontalSlider>(   findChildByName(
      *"BlueSlider",  true ) );
      *HorizontalSlider *alphaSlider    = gweni_cast<HorizontalSlider>(   findChildByName(
      *"AlphaSlider", true ) );
     */
    HorizontalSlider *slider=gweni_cast<HorizontalSlider>(info.controlCaller);

    if(slider)
        setColorByName(getColorFromName(slider->getName()), slider->getFloatValue());

    updateControls();
    // setColor( gweni::Color( redSlider->getValue(), greenSlider->getValue(),
    // blueSlider->getValue(), alphaSlider->getValue() ) );
}

void ColorPicker::layout(skin::Base *skin)
{
    ParentClass::layout(skin);
    sizeToChildren(false, true);
    setSize(getWidth(), getHeight()+5);
    GroupBox *groupBox=gweni_cast<GroupBox>(findChildByName("ResultGroupBox", true));

    if(groupBox)
        groupBox->setPos(groupBox->getX(), getHeight()/2 - groupBox->getHeight()/2);

    updateControls();
}

int ColorPicker::getColorByName(gweni::String colorName)
{
    if(colorName == "Red")
        return getColor().r;
    else if(colorName == "Green")
        return getColor().g;
    else if(colorName == "Blue")
        return getColor().b;
    else if(colorName == "Alpha")
        return getColor().a;
    else
        return 0;
}

gweni::String ColorPicker::getColorFromName(gweni::String name)
{
    if(name.find("Red") != gweni::String::npos)
        return "Red";

    if(name.find("Green") != gweni::String::npos)
        return "Green";

    if(name.find("Blue") != gweni::String::npos)
        return "Blue";

    if(name.find("Alpha") != gweni::String::npos)
        return "Alpha";
    else
        return "";
}

void ColorPicker::setColorByName(gweni::String colorName, int colorValue)
{
    if(colorName == "Red")
        setRed(colorValue);
    else if(colorName == "Green")
        setGreen(colorValue);
    else if(colorName == "Blue")
        setBlue(colorValue);
    else if(colorName == "Alpha")
        setAlpha(colorValue);
}

void ColorPicker::setAlphaVisible(bool visible)
{
    GroupBox *groupBox=gweni_cast<GroupBox>(findChildByName("Alphagroupbox", true));

    groupBox->setHidden(!visible);
    invalidate();
}

}//namespace controls
}//namespace gweni
