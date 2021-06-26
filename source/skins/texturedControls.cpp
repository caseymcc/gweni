#include "gweni/skins/texturedControls.h"

namespace gweni
{
namespace skin
{
namespace textured
{

///////////////////////////////////////////////////////////////////////////
//Button
void Button::staticInit(TexturedSkin *skin)
{
    Normal=skin->getTextureEntryId("Button.Normal");
    Hover=skin->getTextureEntryId("Button.Hover");
    Down=skin->getTextureEntryId("Button.Down");
    Disabled=skin->getTextureEntryId("Button.Disabled");
}

void Button::generate(renderer::Base *renderer, controls::Base *control)
{

}

void Button::update(renderer::Base *renderer, controls::Base *control)
{}

void Button::remove(renderer::Base *renderer, controls::Base *control)
{}

///////////////////////////////////////////////////////////////////////////
//CategoryList
void CategoryList::staticInit(TexturedSkin *skin)
{
    Outer=skin->getTextureEntryId("CategoryList.Outer");
    Inner=skin->getTextureEntryId("CategoryList.Inner");
    Header=skin->getTextureEntryId("CategoryList.Header");
}

void CategoryList::generate(renderer::Base *renderer, controls::Base *control)
{}

void CategoryList::update(renderer::Base *renderer, controls::Base *control)
{}

void CategoryList::remove(renderer::Base *renderer, controls::Base *control)
{}


///////////////////////////////////////////////////////////////////////////
//Checkbox
void Checkbox::staticInit(TexturedSkin *skin)
{
    ActiveNormal=skin->getTextureEntryId("CheckBox.Active.Normal");
    ActiveChecked=skin->getTextureEntryId("CheckBox.Active.Checked");
    DisabledNormal=skin->getTextureEntryId("CheckBox.Disabled.Normal");
    DisabledChecked=skin->getTextureEntryId("CheckBox.Disabled.Checked");
}

void Checkbox::generate(renderer::Base *renderer, controls::Base *control)
{}

void Checkbox::update(renderer::Base *renderer, controls::Base *control)
{}

void Checkbox::remove(renderer::Base *renderer, controls::Base *control)
{}


///////////////////////////////////////////////////////////////////////////
//ComboBox
void ComboBox::staticInit(TexturedSkin *skin)
{
    Normal=skin->getTextureEntryId("Input.ComboBox.Normal");
    Hover=skin->getTextureEntryId("Input.ComboBox.Hover");
    Down=skin->getTextureEntryId("Input.ComboBox.Down");
    Disabled=skin->getTextureEntryId("Input.ComboBox.Disabled");
}

void ComboBox::generate(renderer::Base *renderer, controls::Base *control)
{
//    Textures.Input.ComboBox.Normal.generate(renderer, control);
}

void ComboBox::update(renderer::Base *renderer, controls::Base *control)
{
//    if(control->isDisabled())
//        return Textures.Input.ComboBox.Disabled.draw(renderer, control, control->getRenderBounds());
//
//    if(down || menuOpen)
//        return Textures.Input.ComboBox.Down.draw(renderer, control, control->getRenderBounds());
//
//    if(control->isHovered())
//        return Textures.Input.ComboBox.Hover.draw(renderer, control, control->getRenderBounds());
//
//    Textures.Input.ComboBox.Normal.draw(renderer, control, control->getRenderBounds());
}

void ComboBox::remove(renderer::Base *renderer, controls::Base *control)
{
//    Textures.Input.ComboBox.Normal.remove(renderer, control);
}


///////////////////////////////////////////////////////////////////////////
//GroupBox
void GroupBox::staticInit(TexturedSkin *skin)
{}

void GroupBox::generate(renderer::Base *renderer, controls::Base *control)
{}

void GroupBox::update(renderer::Base *renderer, controls::Base *control)
{}

void GroupBox::remove(renderer::Base *renderer, controls::Base *control)
{}


///////////////////////////////////////////////////////////////////////////
//Input
void Input::staticInit(TexturedSkin *skin)
{
    ButtonTemplate<InputButton>::staticInit(skin);
    ButtonTemplate<InputButtonUp>::staticInit(skin);
    ButtonTemplate<InputButtonDown>::staticInit(skin);
}

void Input::generate(renderer::Base *renderer, controls::Base *control)
{}

void Input::update(renderer::Base *renderer, controls::Base *control)
{}

void Input::remove(renderer::Base *renderer, controls::Base *control)
{}


///////////////////////////////////////////////////////////////////////////
//ListBox
void ListBox::staticInit(TexturedSkin *skin)
{
    Background=skin->getTextureEntryId("ListBox.Background");
    Hovered=skin->getTextureEntryId("ListBox.Hovered");
    EvenLine=skin->getTextureEntryId("ListBox.EvenLine");
    OddLine=skin->getTextureEntryId("ListBox.OddLine");
    EvenLineSelected=skin->getTextureEntryId("ListBox.EvenLineSelected");
    OddLineSelected=skin->getTextureEntryId("ListBox.OddLineSelected");
}

void ListBox::generate(renderer::Base *renderer, controls::Base *control)
{}

void ListBox::update(renderer::Base *renderer, controls::Base *control)
{}

void ListBox::remove(renderer::Base *renderer, controls::Base *control)
{}


///////////////////////////////////////////////////////////////////////////
//Menu
void Menu::staticInit(TexturedSkin *skin)
{
    RightArrow=skin->getTextureEntryId("Menu.RightArrow");
    Check=skin->getTextureEntryId("Menu.Check");
    Strip=skin->getTextureEntryId("Menu.Strip");
    Background=skin->getTextureEntryId("Menu.Background");
    BackgroundWithMargin=skin->getTextureEntryId("Menu.BackgroundWithMargin");
    Hover=skin->getTextureEntryId("Menu.Hover");
}

void Menu::generate(renderer::Base *renderer, controls::Base *control)
{}

void Menu::update(renderer::Base *renderer, controls::Base *control)
{}

void Menu::remove(renderer::Base *renderer, controls::Base *control)
{}

///////////////////////////////////////////////////////////////////////////
//Panel
void Panel::staticInit(TexturedSkin *skin)
{
    Normal=skin->getTextureEntryId("Panel.Normal");
    Bright=skin->getTextureEntryId("Panel.Bright");
    Dark=skin->getTextureEntryId("Panel.Dark");
    Highlight=skin->getTextureEntryId("Panel.Highlight");
}

void Panel::generate(renderer::Base *renderer, controls::Base *control)
{}

void Panel::update(renderer::Base *renderer, controls::Base *control)
{}

void Panel::remove(renderer::Base *renderer, controls::Base *control)
{}


///////////////////////////////////////////////////////////////////////////
//ProgressBar
void ProgressBar::staticInit(TexturedSkin *skin)
{
    Back=skin->getTextureEntryId("ProgressBar.Back");
    Front=skin->getTextureEntryId("ProgressBar.Front");
}

void ProgressBar::generate(renderer::Base *renderer, controls::Base *control)
{}

void ProgressBar::update(renderer::Base *renderer, controls::Base *control)
{}

void ProgressBar::remove(renderer::Base *renderer, controls::Base *control)
{}


///////////////////////////////////////////////////////////////////////////
//RadioButton
void RadioButton::staticInit(TexturedSkin *skin)
{
    ActiveNormal=skin->getTextureEntryId("RadioButton.Active.Normal");
    DisabledNormal=skin->getTextureEntryId("RadioButton.Disabled.Normal");
    ActiveChecked=skin->getTextureEntryId("RadioButton.Active.Checked");
    DisabledChecked=skin->getTextureEntryId("RadioButton.Disabled.Checked");
}

void RadioButton::generate(renderer::Base *renderer, controls::Base *control)
{}

void RadioButton::update(renderer::Base *renderer, controls::Base *control)
{}

void RadioButton::remove(renderer::Base *renderer, controls::Base *control)
{}


///////////////////////////////////////////////////////////////////////////
//Scroller
void Scroller::staticInit(TexturedSkin *skin)
{
    ButtonTemplate<ScrollerButton>::staticInit(skin);
    ButtonTemplate<ScrollerHorizontal>::staticInit(skin);
    ButtonTemplate<ScrollerVertical>::staticInit(skin);

    TrackV=skin->getTextureEntryId("Scroller.TrackV");
    TrackV=skin->getTextureEntryId("Scroller.TrackH");
}

void Scroller::generate(renderer::Base *renderer, controls::Base *control)
{}

void Scroller::update(renderer::Base *renderer, controls::Base *control)
{}

void Scroller::remove(renderer::Base *renderer, controls::Base *control)
{}


///////////////////////////////////////////////////////////////////////////
//Slider
void Slider::staticInit(TexturedSkin *skin)
{
    Horizontal_Normal=skin->getTextureEntryId("Input.Slider.H.Normal");
    Horizontal_Hover=skin->getTextureEntryId("Input.Slider.H.Hover");
    Horizontal_Down=skin->getTextureEntryId("Input.Slider.H.Down");
    Horizontal_Disabled=skin->getTextureEntryId("Input.Slider.H.Disabled");
    Vertical_Normal=skin->getTextureEntryId("Input.Slider.V.Normal");
    Vertical_Hover=skin->getTextureEntryId("Input.Slider.V.Hover");
    Vertical_Down=skin->getTextureEntryId("Input.Slider.V.Down");
    Vertical_Disabled=skin->getTextureEntryId("Input.Slider.V.Disabled");
}

void Slider::generate(renderer::Base *renderer, controls::Base *control)
{}

void Slider::update(renderer::Base *renderer, controls::Base *control)
{}

void Slider::remove(renderer::Base *renderer, controls::Base *control)
{}


///////////////////////////////////////////////////////////////////////////
//Shadow
void Shadow::staticInit(TexturedSkin *skin)
{
    Background=skin->getTextureEntryId("Shadow.Background");
}

void Shadow::generate(renderer::Base *renderer, controls::Base *control)
{}

void Shadow::update(renderer::Base *renderer, controls::Base *control)
{}

void Shadow::remove(renderer::Base *renderer, controls::Base *control)
{}


///////////////////////////////////////////////////////////////////////////
//StatusBar
void StatusBar::staticInit(TexturedSkin *skin)
{
    Background=skin->getTextureEntryId("StatusBar.Background");
    Selection=skin->getTextureEntryId("StatusBar.Selection");
}

void StatusBar::generate(renderer::Base *renderer, controls::Base *control)
{}

void StatusBar::update(renderer::Base *renderer, controls::Base *control)
{}

void StatusBar::remove(renderer::Base *renderer, controls::Base *control)
{}


///////////////////////////////////////////////////////////////////////////
//Tab
void Tab::staticInit(TexturedSkin *skin)
{
    TabItem<TabTop>::staticInit(skin);
    TabItem<TabBottom>::staticInit(skin);
    TabItem<TabLeft>::staticInit(skin);
//    TabItem<TabRight>::staticInit(skin);

    Control=skin->getTextureEntryId("Tab.Control");
    HeaderBar=skin->getTextureEntryId("Tab.HeaderBar");
}

void Tab::generate(renderer::Base *renderer, controls::Base *control)
{}

void Tab::update(renderer::Base *renderer, controls::Base *control)
{}

void Tab::remove(renderer::Base *renderer, controls::Base *control)
{}


///////////////////////////////////////////////////////////////////////////
//TextBox
void TextBox::staticInit(TexturedSkin *skin)
{
    Normal=skin->getTextureEntryId("TextBox.Normal");
    Focus=skin->getTextureEntryId("TextBox.Focus");
    Disabled=skin->getTextureEntryId("TextBox.Disabled");
}

void TextBox::generate(renderer::Base *renderer, controls::Base *control)
{}

void TextBox::update(renderer::Base *renderer, controls::Base *control)
{}

void TextBox::remove(renderer::Base *renderer, controls::Base *control)
{}


///////////////////////////////////////////////////////////////////////////
//Tooltip
void Tooltip::staticInit(TexturedSkin *skin)
{
    Background=skin->getTextureEntryId("Tooltip.Background");
}

void Tooltip::generate(renderer::Base *renderer, controls::Base *control)
{}

void Tooltip::update(renderer::Base *renderer, controls::Base *control)
{}

void Tooltip::remove(renderer::Base *renderer, controls::Base *control)
{}


///////////////////////////////////////////////////////////////////////////
//Tree
void Tree::staticInit(TexturedSkin *skin)
{
    Background=skin->getTextureEntryId("Tree.Background");
    Plus=skin->getTextureEntryId("Tree.Plus");
    Minus=skin->getTextureEntryId("Tree.Minus");
}

void Tree::generate(renderer::Base *renderer, controls::Base *control)
{}

void Tree::update(renderer::Base *renderer, controls::Base *control)
{}

void Tree::remove(renderer::Base *renderer, controls::Base *control)
{}


///////////////////////////////////////////////////////////////////////////
//Window
void Window::staticInit(TexturedSkin *skin)
{
    Normal=skin->getTextureEntryId("Window.Normal");
    Inactive=skin->getTextureEntryId("Window.Inactive");
}

void Window::generate(renderer::Base *renderer, controls::Base *control)
{}

void Window::update(renderer::Base *renderer, controls::Base *control)
{}

void Window::remove(renderer::Base *renderer, controls::Base *control)
{}

}//namespace textured
}//namespace skin
}//namespace gweni
