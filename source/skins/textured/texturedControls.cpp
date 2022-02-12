#include "gweni/skins/textured/texturedControls.h"
#include "gweni/skins/textured/texturedSkin.h"

#include "gweni/controls/comboBox.h"
#include "gweni/controls/checkBox.h"
#include "gweni/controls/scrollBarButton.h"
#include "gweni/controls/tabControl.h"
#include "gweni/controls/text.h"


namespace gweni
{
namespace skin
{
namespace textured
{

/////////////////////////////////////////////////////////////////////////////
////Button
//size_t Button::Normal;
//size_t Button::Hover;
//size_t Button::Down;
//size_t Button::Disabled;
//
//void Button::staticInit(TexturedSkin *skin)
//{
//    Normal=skin->getTextureEntryId("Button.Normal");
//    Hover=skin->getTextureEntryId("Button.Hover");
//    Down=skin->getTextureEntryId("Button.Down");
//    Disabled=skin->getTextureEntryId("Button.Disabled");
//}
//
//void Button::generate(renderer::Base *renderer, controls::Base *baseControl)
//{
//}
//
//void Button::update(renderer::Base *renderer, controls::Base *baseControl)
//{
////    Button *button=(Button *)baseControl;
////
////    if(button->isDepressed())
////        Down::draw(renderer::Base *render, size_t *primitiveIds, Rect r,
////        const Color &col=colors::White, unsigned int draw=~0);
//}
//
//void Button::remove(renderer::Base *renderer, controls::Base *baseControl)
//{}

///////////////////////////////////////////////////////////////////////////
//CategoryHeaderButton
//
size_t CategoryHeaderButton::Button;
BorderedPrimitive CategoryHeaderButton::ButtonPrimitive;

void CategoryHeaderButton::staticInit(TexturedSkin *skin)
{
    Button=skin->getTextureEntryId("CollapsibleCategory.HeaderButton");
    ButtonPrimitive.init(skin, Button);
    m_staticInit=true;
}

void CategoryHeaderButton::update(renderer::Base *renderer, controls::Base *baseControl)
{
    controls::TabButton *control=static_cast<controls::TabButton *>(baseControl);

    ButtonPrimitive.draw(renderer, m_primitives.data(), baseControl->getRenderBounds(), baseControl->getZIndex());
    if(baseControl->getStateChange()&controls::StateChange_Visibility)
        ButtonPrimitive.show(renderer, m_primitives.data(), !baseControl->hidden());
}

///////////////////////////////////////////////////////////////////////////
//CollapsibleCategory
//
size_t CollapsibleCategory::Background;
BorderedPrimitive CollapsibleCategory::BackgroundPrimitive;

void CollapsibleCategory::staticInit(TexturedSkin *skin)
{
    Background=skin->getTextureEntryId("CategoryList.Inner");
    BackgroundPrimitive.init(skin, Background);
    m_staticInit=true;
}

void CollapsibleCategory::update(renderer::Base *renderer, controls::Base *baseControl)
{
    controls::TabButton *control=static_cast<controls::TabButton *>(baseControl);

    BackgroundPrimitive.draw(renderer, m_primitives.data(), baseControl->getRenderBounds(), baseControl->getZIndex());
    if((baseControl->getStateChange()&controls::StateChange_Visibility) ||
        (baseControl->getStateChange()&controls::StateChange_Created))
    {
        BackgroundPrimitive.show(renderer, m_primitives.data(), !baseControl->hidden());
    }
}

///////////////////////////////////////////////////////////////////////////
//CategoryList
size_t CategoryList::Outer;
size_t CategoryList::Inner;
size_t CategoryList::Header;

BorderedPrimitive CategoryList::OuterPrimitive;
BorderedPrimitive CategoryList::InnerPrimitive;
BorderedPrimitive CategoryList::HeaderPrimitive;

void CategoryList::staticInit(TexturedSkin *skin)
{
    Outer=skin->getTextureEntryId("CategoryList.Outer");
    Inner=skin->getTextureEntryId("CategoryList.Inner");
    Header=skin->getTextureEntryId("CategoryList.Header");

    OuterPrimitive.init(skin, Outer);
    InnerPrimitive.init(skin, Inner);
    HeaderPrimitive.init(skin, Header);
}

void CategoryList::update(renderer::Base *renderer, controls::Base *baseControl)
{
    controls::TabButton *control=static_cast<controls::TabButton *>(baseControl);

    OuterPrimitive.draw(renderer, m_primitives.data(), baseControl->getRenderBounds(), baseControl->getZIndex());
    if((baseControl->getStateChange()&controls::StateChange_Visibility) ||
        (baseControl->getStateChange()&controls::StateChange_Created))
    {
        OuterPrimitive.show(renderer, m_primitives.data(), !baseControl->hidden());
    }
}


///////////////////////////////////////////////////////////////////////////
//CategoryList
void Canvas::staticInit(TexturedSkin *skin)
{
}

void Canvas::update(renderer::Base *renderer, controls::Base *baseControl)
{}

///////////////////////////////////////////////////////////////////////////
//CheckBox
size_t CheckBox::ActiveNormal;
size_t CheckBox::ActiveChecked;
size_t CheckBox::DisabledNormal;
size_t CheckBox::DisabledChecked;

BorderedPrimitive CheckBox::ActiveNormalPrimitive;
BorderedPrimitive CheckBox::ActiveCheckedPrimitive;
BorderedPrimitive CheckBox::DisabledNormalPrimitive;
BorderedPrimitive CheckBox::DisabledCheckedPrimitive;

void CheckBox::staticInit(TexturedSkin *skin)
{
    ActiveNormal=skin->getTextureEntryId("CheckBox.Active.Normal");
    ActiveChecked=skin->getTextureEntryId("CheckBox.Active.Checked");
    DisabledNormal=skin->getTextureEntryId("CheckBox.Disabled.Normal");
    DisabledChecked=skin->getTextureEntryId("CheckBox.Disabled.Checked");
}

void CheckBox::update(renderer::Base *renderer, controls::Base *baseControl)
{
    controls::CheckBox *control=static_cast<controls::CheckBox *>(baseControl);
            
    BorderedPrimitive *primitive;

    if(control->isDisabled())
    {
        if(control->isChecked())
            primitive=&DisabledCheckedPrimitive;
        else
            primitive=&DisabledNormalPrimitive;
    }
    else
    {
        if(control->isChecked())
            primitive=&ActiveCheckedPrimitive;
        else
            primitive=&ActiveNormalPrimitive;
    }

    primitive->draw(renderer, m_primitives.data(), baseControl->getRenderBounds(), baseControl->getZIndex());
    if(baseControl->getStateChange()&controls::StateChange_Visibility)
        primitive->show(renderer, m_primitives.data(), !baseControl->hidden());
}


///////////////////////////////////////////////////////////////////////////
//ComboBox
size_t ComboBox::Normal;
size_t ComboBox::Hover;
size_t ComboBox::Down;
size_t ComboBox::Disabled;

BorderedPrimitive ComboBox::NormalPrimitive;
BorderedPrimitive ComboBox::HoverPrimitive;
BorderedPrimitive ComboBox::DownPrimitive;
BorderedPrimitive ComboBox::DisabledPrimitive;

void ComboBox::staticInit(TexturedSkin *skin)
{
    Normal=skin->getTextureEntryId("Input.ComboBox.Normal");
    Hover=skin->getTextureEntryId("Input.ComboBox.Hover");
    Down=skin->getTextureEntryId("Input.ComboBox.Down");
    Disabled=skin->getTextureEntryId("Input.ComboBox.Disabled");

    NormalPrimitive.init(skin, Normal);
    HoverPrimitive.init(skin, Hover);
    DownPrimitive.init(skin, Down);
    DisabledPrimitive.init(skin, Disabled);

    m_staticInit=true;
}

void ComboBox::update(renderer::Base *renderer, controls::Base *baseControl)
{
    controls::ComboBox *control=static_cast<controls::ComboBox *>(baseControl);
    BorderedPrimitive *primitive;

    if(baseControl->isDisabled())
        primitive=&DisabledPrimitive;
    else if(control->isDepressed() || control->isMenuOpen())
        primitive=&DownPrimitive;
    else if(baseControl->isHovered())
        primitive=&HoverPrimitive;
    else
        primitive=&NormalPrimitive;

    primitive->draw(renderer, m_primitives.data(), baseControl->getRenderBounds(), baseControl->getZIndex());
    if(baseControl->getStateChange()&controls::StateChange_Visibility)
        primitive->show(renderer, m_primitives.data(), !baseControl->hidden());
}


///////////////////////////////////////////////////////////////////////////
//DockBase
void DockBase::staticInit(TexturedSkin *skin)
{

}

void DockBase::update(renderer::Base *renderer, controls::Base *baseControl)
{
    renderer->setDrawColor(gweni::Color(255, 100, 255, 20));
    renderer->drawFilledRect(m_primitives[0], baseControl->getRenderBounds(), baseControl->getZIndex());

//    if(m_hoverRect.w == 0)
//        return;
//
//    render->setDrawColor(gweni::Color(255, 100, 255, 100));
//    render->drawFilledRect(m_primitiveIds[1], m_hoverRect, getZIndex());
//    render->setDrawColor(gweni::Color(255, 100, 255, 200));
//    render->drawLinedRect(&m_primitiveIds[2], m_hoverRect, getZIndex());

    if(baseControl->getStateChange()&controls::StateChange_Visibility)
    {
        if(baseControl->hidden())
            renderer->showPrimitive(0, m_primitives[0]);
        else
            renderer->showPrimitive(1, m_primitives[0]);
    }
}

///////////////////////////////////////////////////////////////////////////
//GroupBox
size_t GroupBox::Normal;
BorderedPrimitive GroupBox::NormalPrimitive;

void GroupBox::staticInit(TexturedSkin *skin)
{
    m_staticInit=true;

    Normal=skin->getTextureEntryId(name);
    NormalPrimitive.init(skin, Normal);
}

void GroupBox::update(renderer::Base *renderer, controls::Base *baseControl)
{
    NormalPrimitive.draw(renderer, m_primitives.data(), baseControl->getRenderBounds(), baseControl->getZIndex());
    if(baseControl->getStateChange()&controls::StateChange_Visibility)
    {
        NormalPrimitive.show(renderer, m_primitives.data(), !baseControl->hidden());
    }
}



///////////////////////////////////////////////////////////////////////////
//Input
void Input::staticInit(TexturedSkin *skin)
{
    ButtonTemplate<InputButton>::staticInit(skin);
    ButtonTemplate<InputButtonUp>::staticInit(skin);
    ButtonTemplate<InputButtonDown>::staticInit(skin);

    m_staticInit=true;
}

void Input::update(renderer::Base *renderer, controls::Base *baseControl)
{}


///////////////////////////////////////////////////////////////////////////
//ListBox
size_t ListBox::Background;
size_t ListBox::Hovered;

size_t ListBox::EvenLine;
size_t ListBox::OddLine;
size_t ListBox::EvenLineSelected;
size_t ListBox::OddLineSelected;

void ListBox::staticInit(TexturedSkin *skin)
{
    Background=skin->getTextureEntryId("ListBox.Background");
    Hovered=skin->getTextureEntryId("ListBox.Hovered");
    EvenLine=skin->getTextureEntryId("ListBox.EvenLine");
    OddLine=skin->getTextureEntryId("ListBox.OddLine");
    EvenLineSelected=skin->getTextureEntryId("ListBox.EvenLineSelected");
    OddLineSelected=skin->getTextureEntryId("ListBox.OddLineSelected");

    m_staticInit=true;
}

void ListBox::update(renderer::Base *renderer, controls::Base *baseControl)
{}


///////////////////////////////////////////////////////////////////////////
//Menu
size_t Menu::RightArrow;
size_t Menu::Check;

size_t Menu::Strip;
size_t Menu::Background;
size_t Menu::BackgroundWithMargin;
size_t Menu::Hover;

void Menu::staticInit(TexturedSkin *skin)
{
    RightArrow=skin->getTextureEntryId("Menu.RightArrow");
    Check=skin->getTextureEntryId("Menu.Check");
    Strip=skin->getTextureEntryId("Menu.Strip");
    Background=skin->getTextureEntryId("Menu.Background");
    BackgroundWithMargin=skin->getTextureEntryId("Menu.BackgroundWithMargin");
    Hover=skin->getTextureEntryId("Menu.Hover");

    m_staticInit=true;
}

void Menu::generate(renderer::Base *renderer, controls::Base *baseControl)
{}

void Menu::update(renderer::Base *renderer, controls::Base *baseControl)
{}

void Menu::remove(renderer::Base *renderer, controls::Base *baseControl)
{}

///////////////////////////////////////////////////////////////////////////
//Panel
size_t Panel::Normal;
size_t Panel::Bright;
size_t Panel::Dark;
size_t Panel::Highlight;

void Panel::staticInit(TexturedSkin *skin)
{
    Normal=skin->getTextureEntryId("Panel.Normal");
    Bright=skin->getTextureEntryId("Panel.Bright");
    Dark=skin->getTextureEntryId("Panel.Dark");
    Highlight=skin->getTextureEntryId("Panel.Highlight");

    m_staticInit=true;
}

void Panel::generate(renderer::Base *renderer, controls::Base *baseControl)
{}

void Panel::update(renderer::Base *renderer, controls::Base *baseControl)
{}

void Panel::remove(renderer::Base *renderer, controls::Base *baseControl)
{}


///////////////////////////////////////////////////////////////////////////
//ProgressBar
size_t ProgressBar::Back;
size_t ProgressBar::Front;

void ProgressBar::staticInit(TexturedSkin *skin)
{
    Back=skin->getTextureEntryId("ProgressBar.Back");
    Front=skin->getTextureEntryId("ProgressBar.Front");

    m_staticInit=true;
}

void ProgressBar::generate(renderer::Base *renderer, controls::Base *baseControl)
{}

void ProgressBar::update(renderer::Base *renderer, controls::Base *baseControl)
{}

void ProgressBar::remove(renderer::Base *renderer, controls::Base *baseControl)
{}


///////////////////////////////////////////////////////////////////////////
//RadioButton
size_t RadioButton::ActiveNormal;
size_t RadioButton::ActiveChecked;
size_t RadioButton::DisabledNormal;
size_t RadioButton::DisabledChecked;

void RadioButton::staticInit(TexturedSkin *skin)
{
    ActiveNormal=skin->getTextureEntryId("RadioButton.Active.Normal");
    DisabledNormal=skin->getTextureEntryId("RadioButton.Disabled.Normal");
    ActiveChecked=skin->getTextureEntryId("RadioButton.Active.Checked");
    DisabledChecked=skin->getTextureEntryId("RadioButton.Disabled.Checked");

    m_staticInit=true;
}

void RadioButton::generate(renderer::Base *renderer, controls::Base *baseControl)
{}

void RadioButton::update(renderer::Base *renderer, controls::Base *baseControl)
{}

void RadioButton::remove(renderer::Base *renderer, controls::Base *baseControl)
{}


///////////////////////////////////////////////////////////////////////////
//ScrollBarButton
size_t ScrollBarButton::Normal[4];
size_t ScrollBarButton::Hover[4];
size_t ScrollBarButton::Down[4];
size_t ScrollBarButton::Disable[4];

BorderedPrimitive ScrollBarButton::NormalPrimitive[4];
BorderedPrimitive ScrollBarButton::HoverPrimitive[4];
BorderedPrimitive ScrollBarButton::DownPrimitive[4];
BorderedPrimitive ScrollBarButton::DisablePrimitive[4];

void ScrollBarButton::staticInit(TexturedSkin *skin)
{
    std::vector<std::string> direction{"Left", "Right", "Up", "Down"};

    for(size_t i=0; i<4; ++i)
    {
        std::string name="ScrollBarButton."+direction[i];

        Normal[i]=skin->getTextureEntryId(name+".Normal");
        Hover[i]=skin->getTextureEntryId(name+".Hover");
        Down[i]=skin->getTextureEntryId(name+".Down");
        Disable[i]=skin->getTextureEntryId(name+".Disabled");

        NormalPrimitive[i].init(skin, Normal[i]);
        HoverPrimitive[i].init(skin, Hover[i]);
        DownPrimitive[i].init(skin, Down[i]);
        DisablePrimitive[i].init(skin, Disable[i]);
    }
 
    m_staticInit=true;
}

void ScrollBarButton::update(renderer::Base *renderer, controls::Base *baseControl)
{
    controls::internal::ScrollBarButton *control=static_cast<controls::internal::ScrollBarButton *>(baseControl);

    Position direction=control->getDirection();
    size_t index=0;

    if(direction == Position::Right)
        index=1;
    else if(direction == Position::Top)
        index=2;
    if(direction == Position::Bottom)
        index=3;

    BorderedPrimitive *primitive=&NormalPrimitive[index];

    if(control->isDisabled())
        primitive=&DisablePrimitive[index];
    else if(control->isDepressed())
        primitive=&DownPrimitive[index];
    else if(control->isHovered())
        primitive=&HoverPrimitive[index];
    
    primitive->draw(renderer, m_primitives.data(), baseControl->getRenderBounds(), baseControl->getZIndex());
    if((baseControl->getStateChange()&controls::StateChange_Visibility) ||
        (baseControl->getStateChange()&controls::StateChange_Created))
    {
        primitive->show(renderer, m_primitives.data(), !baseControl->hidden());
    }
}

///////////////////////////////////////////////////////////////////////////
//Scroller
size_t Scroller::TrackV;
size_t Scroller::TrackH;

void Scroller::staticInit(TexturedSkin *skin)
{
    ButtonTemplate<ScrollerButton>::staticInit(skin);
    ButtonTemplate<ScrollerHorizontal>::staticInit(skin);
    ButtonTemplate<ScrollerVertical>::staticInit(skin);

    TrackV=skin->getTextureEntryId("Scroller.TrackV");
    TrackV=skin->getTextureEntryId("Scroller.TrackH");

    m_staticInit=true;
}

void Scroller::generate(renderer::Base *renderer, controls::Base *baseControl)
{}

void Scroller::update(renderer::Base *renderer, controls::Base *baseControl)
{}

void Scroller::remove(renderer::Base *renderer, controls::Base *baseControl)
{}


///////////////////////////////////////////////////////////////////////////
//Shadow
size_t Shadow::Background;

void Shadow::staticInit(TexturedSkin *skin)
{
    Background=skin->getTextureEntryId("Shadow.Background");

    m_staticInit=true;
}

void Shadow::generate(renderer::Base *renderer, controls::Base *baseControl)
{}

void Shadow::update(renderer::Base *renderer, controls::Base *baseControl)
{}

void Shadow::remove(renderer::Base *renderer, controls::Base *baseControl)
{}


///////////////////////////////////////////////////////////////////////////
//Slider
size_t Slider::Horizontal_Normal;
size_t Slider::Horizontal_Hover;
size_t Slider::Horizontal_Down;
size_t Slider::Horizontal_Disabled;
size_t Slider::Vertical_Normal;
size_t Slider::Vertical_Hover;
size_t Slider::Vertical_Down;
size_t Slider::Vertical_Disabled;

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

    m_staticInit=true;
}

void Slider::generate(renderer::Base *renderer, controls::Base *baseControl)
{}

void Slider::update(renderer::Base *renderer, controls::Base *baseControl)
{}

void Slider::remove(renderer::Base *renderer, controls::Base *baseControl)
{}


///////////////////////////////////////////////////////////////////////////
//StatusBar
size_t StatusBar::Background;
size_t StatusBar::Selection;

void StatusBar::staticInit(TexturedSkin *skin)
{
    Background=skin->getTextureEntryId("StatusBar.Background");
    Selection=skin->getTextureEntryId("StatusBar.Selection");

    m_staticInit=true;
}

void StatusBar::generate(renderer::Base *renderer, controls::Base *baseControl)
{}

void StatusBar::update(renderer::Base *renderer, controls::Base *baseControl)
{}

void StatusBar::remove(renderer::Base *renderer, controls::Base *baseControl)
{}


///////////////////////////////////////////////////////////////////////////
//TabButton
//
size_t TabButton::Button;
BorderedPrimitive TabButton::ButtonPrimitive;

void TabButton::staticInit(TexturedSkin *skin)
{
    Button=skin->getTextureEntryId("TabButton");
    ButtonPrimitive.init(skin, Button);
    m_staticInit=true;
}

void TabButton::update(renderer::Base *renderer, controls::Base *baseControl)
{
    controls::TabButton *control=static_cast<controls::TabButton *>(baseControl);

    ButtonPrimitive.draw(renderer, m_primitives.data(), baseControl->getRenderBounds(), baseControl->getZIndex());
    if(baseControl->getStateChange()&controls::StateChange_Visibility)
        ButtonPrimitive.show(renderer, m_primitives.data(), !baseControl->hidden());
}

///////////////////////////////////////////////////////////////////////////
//TabControl
size_t TabControl::Control;
size_t TabControl::HeaderBar;
BorderedPrimitive TabControl::ControlPrimitive;
BorderedPrimitive TabControl::HeaderBarPrimitive;

void TabControl::staticInit(TexturedSkin *skin)
{
    TabItem<TabTop>::staticInit(skin);
    TabItem<TabBottom>::staticInit(skin);
    TabItem<TabLeft>::staticInit(skin);
//    TabItem<TabRight>::staticInit(skin);

    Control=skin->getTextureEntryId("TabControl.Control");
    HeaderBar=skin->getTextureEntryId("TabControl.HeaderBar");

    ControlPrimitive.init(skin, Control);
    HeaderBarPrimitive.init(skin, HeaderBar);

    m_staticInit=true;
}

void TabControl::update(renderer::Base *renderer, controls::Base *baseControl)
{
    controls::TabControl *control=static_cast<controls::TabControl *>(baseControl);

    ControlPrimitive.draw(renderer, m_primitives.data(), baseControl->getRenderBounds(), baseControl->getZIndex());
    if(baseControl->getStateChange()&controls::StateChange_Visibility)
        ControlPrimitive.show(renderer, m_primitives.data(), !baseControl->hidden());
}

///////////////////////////////////////////////////////////////////////////
//Text
void Text::staticInit(TexturedSkin *skin)
{}

void Text::generate(renderer::Base *renderer, controls::Base *baseControl)
{
    m_primitives.resize(1);

    renderer->generateTextPrimitive(m_primitives[0]);
}

void Text::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(baseControl->getStateChange() == controls::StateChange_Nothing)
        return;
    
    controls::internal::Text *control=static_cast<controls::internal::Text *>(baseControl);

    const gweni::Color &color=control->getTextColor();
    const gweni::Color &colorOverride=control->getTextColorOverride();

    if(colorOverride.a == 0)
        renderer->setDrawColor(color);
    else
        renderer->setDrawColor(colorOverride);

    if(baseControl->getStateChange() & controls::StateChange_Visibility)
    {
        if(baseControl->hidden())
            renderer->hideText(m_primitives[0]);
        else
            renderer->showText(m_primitives[0]);
    }

    renderer->renderText(m_primitives[0], control->getFont(),
        gweni::Point(control->getPadding().left, control->getPadding().top), control->getZIndex(),
        control->getText());
}

void Text::remove(renderer::Base *renderer, controls::Base *baseControl)
{
    renderer->releaseTextPrimitive(m_primitives[0]);
}




///////////////////////////////////////////////////////////////////////////
//TextBox
size_t TextBox::Normal;
size_t TextBox::Focus;
size_t TextBox::Disabled;

void TextBox::staticInit(TexturedSkin *skin)
{
    Normal=skin->getTextureEntryId("TextBox.Normal");
    Focus=skin->getTextureEntryId("TextBox.Focus");
    Disabled=skin->getTextureEntryId("TextBox.Disabled");

    m_staticInit=true;
}

void TextBox::generate(renderer::Base *renderer, controls::Base *baseControl)
{}

void TextBox::update(renderer::Base *renderer, controls::Base *baseControl)
{}

void TextBox::remove(renderer::Base *renderer, controls::Base *baseControl)
{}


///////////////////////////////////////////////////////////////////////////
//Tooltip
size_t Tooltip::Background;

void Tooltip::staticInit(TexturedSkin *skin)
{
    Background=skin->getTextureEntryId("Tooltip.Background");

    m_staticInit=true;
}

void Tooltip::generate(renderer::Base *renderer, controls::Base *baseControl)
{}

void Tooltip::update(renderer::Base *renderer, controls::Base *baseControl)
{}

void Tooltip::remove(renderer::Base *renderer, controls::Base *baseControl)
{}


///////////////////////////////////////////////////////////////////////////
//Tree
size_t Tree::Background;
size_t Tree::Minus;
size_t Tree::Plus;

void Tree::staticInit(TexturedSkin *skin)
{
    Background=skin->getTextureEntryId("Tree.Background");
    Plus=skin->getTextureEntryId("Tree.Plus");
    Minus=skin->getTextureEntryId("Tree.Minus");

    m_staticInit=true;
}

void Tree::generate(renderer::Base *renderer, controls::Base *baseControl)
{}

void Tree::update(renderer::Base *renderer, controls::Base *baseControl)
{}

void Tree::remove(renderer::Base *renderer, controls::Base *baseControl)
{}

///////////////////////////////////////////////////////////////////////////
//SpinBox
size_t SpinBox::UpNormal;
size_t SpinBox::UpHover;
size_t SpinBox::UpDown;
size_t SpinBox::UpDisabled;
size_t SpinBox::DownNormal;
size_t SpinBox::DownHover;
size_t SpinBox::DownDown;
size_t SpinBox::DownDisabled;

void SpinBox::staticInit(TexturedSkin *skin)
{
//    Background=skin->getTextureEntryId("SpinBox.Background");
//    Plus=skin->getTextureEntryId("SpinBox.Plus");
//    Minus=skin->getTextureEntryId("SpinBox.Minus");
    m_staticInit=true;
}

//void SpinBox::generate(renderer::Base *renderer, controls::Base *baseControl)
//{}

void SpinBox::update(renderer::Base *renderer, controls::Base *baseControl)
{}

//void SpinBox::remove(renderer::Base *renderer, controls::Base *baseControl)
//{}


///////////////////////////////////////////////////////////////////////////
//Window
size_t Window::Normal;
size_t Window::Inactive;
size_t Window::Close;
size_t Window::Close_Hover;
size_t Window::Close_Down;
size_t Window::Maxi;
size_t Window::Maxi_Hover;
size_t Window::Maxi_Down;
size_t Window::Mini;
size_t Window::Mini_Hover;
size_t Window::Mini_Down;
size_t Window::Restore;
size_t Window::Restore_Hover;
size_t Window::Restore_Down;

void Window::staticInit(TexturedSkin *skin)
{
    Normal=skin->getTextureEntryId("Window.Normal");
    Inactive=skin->getTextureEntryId("Window.Inactive");

    m_staticInit=true;
}

void Window::generate(renderer::Base *renderer, controls::Base *baseControl)
{}

void Window::update(renderer::Base *renderer, controls::Base *baseControl)
{}

void Window::remove(renderer::Base *renderer, controls::Base *baseControl)
{}

}//namespace textured
}//namespace skin
}//namespace gweni
