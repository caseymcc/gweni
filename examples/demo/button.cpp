#include "demo/button.h"
#include <gweni/controls/button.h>
#include <gweni/align.h>

namespace gweni
{

ButtonDemo::ButtonDemo(gweni::controls::Base *parent, const gweni::String &name):
    Base(parent)
{
//    setText(name);

    // Normal button
    controls::Button *buttonA=new controls::Button(this);
    buttonA->setText("Event Tester");
    buttonA->onPressCaller.add(this, &ButtonDemo::onButtonA);
    {
        controls::Button *buttonA=new controls::Button(this);
        buttonA->setBounds(200, 30, 300, 200);
        buttonA->setText("Event Tester");
        buttonA->onPressCaller.add(this, &ButtonDemo::onButtonA);
    }
    // Unicode test
    controls::Button *buttonB=new controls::Button(this);
    buttonB->setText("Text");
    align::placeBelow(buttonB, buttonA, 10);
    // Image with text
    controls::Button *buttonC=new controls::Button(this);
    buttonC->setText("Image Button");
    buttonC->setImage("test16.png");
    align::placeBelow(buttonC, buttonB, 10);
    // Just image
    controls::Button *buttonD=new controls::Button(this);
    buttonD->setText("");
    buttonD->setImage("test16.png");
    buttonD->setSize(20, 20);
    align::placeBelow(buttonD, buttonC, 10);
    // Toggle button
    controls::Button *buttonE=new controls::Button(this);
    buttonE->setText("Toggle Me");
    buttonE->setIsToggle(true);
    buttonE->onToggleCaller.add(this, &ButtonDemo::onToggle);
    buttonE->onToggleOnCaller.add(this, &ButtonDemo::onToggleOn);
    buttonE->onToggleOffCaller.add(this, &ButtonDemo::onToggleOff);
    align::placeBelow(buttonE, buttonD, 10);
    // Disabled Button
    controls::Button *buttonF=new controls::Button(this);
    buttonF->setText("Disabled :D");
    buttonF->setDisabled(true);
    align::placeBelow(buttonF, buttonE, 10);
    // Tooltip Button
    controls::Button *buttonG=new controls::Button(this);
    buttonG->setText("With Tooltip");
    buttonG->setToolTipText("This is a tooltip!");
    align::placeBelow(buttonG, buttonF, 10);
}

void ButtonDemo::onButtonA(event::Info info)
{
}

void ButtonDemo::onToggle(event::Info info)
{
}

void ButtonDemo::onToggleOn(event::Info info)
{
}

void ButtonDemo::onToggleOff(event::Info info)
{
}

}//namespace gweni
