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

namespace internal
{

void ColorDisplay::setColor(gweni::Color color)
{
    if(color == m_color)
        return;

    m_color=color;
    redraw();
}

void ColorDisplay::setRed(int red)
{
    gweni::Color color=m_color;

    color.r=red;
    setColor(color);
}

void ColorDisplay::setGreen(int green)
{
    gweni::Color color=m_color;

    color.g=green;
    setColor(color);
}

void ColorDisplay::setBlue(int blue)
{
    gweni::Color color=m_color;

    color.b=blue;
    setColor(color);
}

void ColorDisplay::setAlpha(int alpha)
{
    gweni::Color color=m_color;

    color.a=alpha;
    setColor(color);
}

void ColorDisplay::setDrawCheckers(bool should)
{
    if(should == m_drawCheckers)
        return;

    m_drawCheckers=should;
    redraw();
}

}//namespace internal

namespace
{

// The channel a name such as "Red" or "RedBox" refers to, set to value.
void setChannel(gweni::Color &color, const gweni::String &name, int value)
{
    if(name.find("Red") != gweni::String::npos)
        color.r=value;
    else if(name.find("Green") != gweni::String::npos)
        color.g=value;
    else if(name.find("Blue") != gweni::String::npos)
        color.b=value;
    else if(name.find("Alpha") != gweni::String::npos)
        color.a=value;
}

}


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

    colorGroup->setAlignment(Alignment::Absolute);
    colorGroup->setPos(10, y);
    colorGroup->setText(name);
    colorGroup->setSize(160, gc_pickChannelHeight);
    colorGroup->setName(name+"groupbox");

//    internal::ColorDisplay *disp=new internal::ColorDisplay(colorGroup);
    internal::ColorDisplay *disp=colorGroup->newChild<internal::ColorDisplay>();

    disp->setName(name);
    disp->setAlignment(Alignment::Absolute);
    disp->setBounds(0, 0, colorSize, colorSize);

//    TextBoxNumeric *numeric=new TextBoxNumeric(colorGroup);
    TextBoxNumeric *numeric=colorGroup->newChild<TextBoxNumeric>();

    numeric->setName(name+"Box");
    numeric->setAlignment(Alignment::Absolute);
    numeric->setPos(105, 0);
    numeric->setSize(26, 16);
    numeric->setSelectAllOnFocus(true);
    numeric->onTextChangedCaller.add(this, &ColorPicker::onNumericTyped);

//    HorizontalSlider *slider=new HorizontalSlider(colorGroup);
    HorizontalSlider *slider=colorGroup->newChild<HorizontalSlider>();

    slider->setAlignment(Alignment::Absolute);
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
    gweni::Color color=m_color;

    setChannel(color, box->getName(), textValue);
    setColor(color);
}

void ColorPicker::setColor(gweni::Color color)
{
    m_color=color;
    updateControls();
}

void ColorPicker::setRed(int red)
{
    gweni::Color color=m_color;

    color.r=red;
    if(!(color == m_color))
        setColor(color);
}

void ColorPicker::setGreen(int green)
{
    gweni::Color color=m_color;

    color.g=green;
    if(!(color == m_color))
        setColor(color);
}

void ColorPicker::setBlue(int blue)
{
    gweni::Color color=m_color;

    color.b=blue;
    if(!(color == m_color))
        setColor(color);
}

void ColorPicker::setAlpha(int alpha)
{
    gweni::Color color=m_color;

    color.a=alpha;
    if(!(color == m_color))
        setColor(color);
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

    finalGroup->setAlignment(Alignment::Absolute);
    finalGroup->setPos(180, 40);
    finalGroup->setSize(60, 60);
    finalGroup->setText("Result");
    finalGroup->setName("ResultGroupBox");

//    internal::ColorDisplay *disp=new internal::ColorDisplay(finalGroup);
    internal::ColorDisplay *disp=finalGroup->newChild<internal::ColorDisplay>();

    disp->setName("Result");
    disp->setAlignment(Alignment::Absolute);
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
    gweni::Color color=m_color;

    if(slider)
        setChannel(color, slider->getName(), slider->getFloatValue());

    setColor(color);
    // setColor( gweni::Color( redSlider->getValue(), greenSlider->getValue(),
    // blueSlider->getValue(), alphaSlider->getValue() ) );
}

// As high as the channel boxes reach, plus 5: a preferred height, which the
// parent gives this control, rather than a resize from inside its layout.
void ColorPicker::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    if(dim == Dim::Y)
        m_preferredSize.height+=5;
}

// The result box is centred vertically once this control has its height,
// before the Y pass places it.
void ColorPicker::arrange(skin::Base *skin, Dim dim)
{
    if(dim == Dim::Y)
    {
        GroupBox *groupBox=gweni_cast<GroupBox>(findChildByName("ResultGroupBox", true));

        if(groupBox)
        {
            PlacementScope placement;

            groupBox->setPos(groupBox->getX(), getHeight()/2-groupBox->getPreferredSize().height/2);
        }
    }

    ParentClass::arrange(skin, dim);
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
