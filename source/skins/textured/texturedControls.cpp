#include "gweni/skins/textured/texturedControls.h"
#include "gweni/skins/textured/texturedSkin.h"

#include "gweni/controls/canvas.h"
#include "gweni/controls/progressBar.h"
#include "gweni/controls/comboBox.h"
#include "gweni/controls/checkBox.h"
#include "gweni/controls/scrollBarButton.h"
#include "gweni/controls/statusBar.h"
#include "gweni/controls/tabControl.h"
#include "gweni/controls/text.h"
#include "gweni/controls/collapsibleCategory.h"
#include "gweni/controls/listBox.h"
#include "gweni/controls/slider.h"
#include "gweni/controls/colorControls.h"

#include <algorithm>


namespace gweni
{
namespace skin
{
namespace textured
{

//Base
// Fallback renderer used by any control that has no skin renderer of its own.
// A plain controls::Base is a generic, transparent container (grouping/scroll
// holders, inner panels), so it draws nothing. Any *specific* control type that
// falls back to here is genuinely unskinned, so paint it bright red to stand out.
size_t Base::requiredPrimitives() { return 1; }
void Base::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    // Controls with no skin renderer of their own are generic grouping containers
    // (rows, inner panels, holders) and draw nothing, so the dark theme shows
    // through. (Earlier this painted unskinned controls red as a debug aid.)
    renderer->hidePrimitive(m_primitives[0]);
}


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
//6
size_t CategoryHeaderButton::Button;
BorderedPrimitive CategoryHeaderButton::ButtonPrimitive;

void CategoryHeaderButton::staticInit(TexturedSkin *skin)
{
    Button=skin->getTextureEntryId("CategoryList.Header");
    ButtonPrimitive.init(skin, Button);
    m_staticInit=true;
}

void CategoryHeaderButton::update(renderer::Base *renderer, controls::Base *baseControl)
{
    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, tokyonight::CategoryHeader);
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
    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, tokyonight::CategoryInner);
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
    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, tokyonight::CategoryList);
}

///////////////////////////////////////////////////////////////////////////
//CategoryButton
gweni::Color CategoryButton::Normal;
gweni::Color CategoryButton::Selected;
gweni::Color CategoryButton::Hover;
gweni::Color CategoryButton::AltNormal;
gweni::Color CategoryButton::AltSelected;
gweni::Color CategoryButton::AltHover;

void CategoryButton::staticInit(TexturedSkin *skin)
{
    Normal=skin->Colors.Category.Line.Button;
    Selected=skin->Colors.Category.Line.Button_Hover;
    Hover=skin->Colors.Category.Line.Button_Selected;
    AltNormal=skin->Colors.Category.LineAlt.Button;
    AltSelected=skin->Colors.Category.LineAlt.Button_Hover;
    AltHover=skin->Colors.Category.LineAlt.Button_Selected;

    m_staticInit=true;
}

void CategoryButton::update(renderer::Base *renderer, controls::Base *baseControl)
{
    controls::CategoryButton *control=static_cast<controls::CategoryButton *>(baseControl);

    const tokyonight::Surface *surface;
    if(control->isDepressed() || control->getToggleState())
        surface=&tokyonight::ListItemSel;
    else if(control->isHovered())
        surface=&tokyonight::ListItemHover;
    else
        surface=&tokyonight::ListItem;

    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, *surface);
}

///////////////////////////////////////////////////////////////////////////
//Canvas
void Canvas::staticInit(TexturedSkin *skin)
{
    m_staticInit=true;
}

void Canvas::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    // Full-window background: flat Tokyo Night base color (no rounding).
    renderer->setDrawColor(tokyonight::Bg);
    renderer->drawFilledRect(m_primitives[0], baseControl->getRenderBounds(), baseControl->getZIndex());
}

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
    if(m_primitives.size() < 2)
        return;

    controls::CheckBox *control=static_cast<controls::CheckBox *>(baseControl);

    // Dark box frame always; blue checkmark glyph overlaid when checked.
    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, tokyonight::CheckOff);

    if(baseControl->visible() && control->isChecked())
        renderer->drawIcon(m_primitives[1], baseControl->getRenderBounds(), 0, 0,
            tokyonight::Blue, baseControl->getZIndex()+1);
    else
        renderer->hidePrimitive(m_primitives[1]);
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
    const tokyonight::Surface *surface;

    if(baseControl->isDisabled())
        surface=&tokyonight::ButtonDisabled;
    else if(control->isDepressed() || control->isMenuOpen())
        surface=&tokyonight::ButtonDown;
    else if(baseControl->isHovered())
        surface=&tokyonight::ButtonHover;
    else
        surface=&tokyonight::ButtonNormal;

    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, *surface);

    // Down chevron on the right edge.
    if(m_primitives.size() >= 2)
    {
        const gweni::Rect r=baseControl->getRenderBounds();
        const int sz=12;
        const gweni::Rect arrow{ r.x+r.w-sz-6, r.y+(r.h-sz)/2, sz, sz };
        if(baseControl->visible())
            renderer->drawIcon(m_primitives[1], arrow, 1, 0, tokyonight::FgDark, baseControl->getZIndex()+1);
        else
            renderer->hidePrimitive(m_primitives[1]);
    }
}


///////////////////////////////////////////////////////////////////////////
//DockBase
void DockBase::staticInit(TexturedSkin *skin)
{

}

void DockBase::update(renderer::Base *renderer, controls::Base *baseControl)
{
    renderer->setDrawColor(gweni::Color(255, 100, 255, 20));

    if(baseControl->visible())
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
        if(!baseControl->visible())
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
    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, tokyonight::GroupBox);
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
//size_t ListBox::EvenLine;
//size_t ListBox::OddLine;
//size_t ListBox::EvenLineSelected;
//size_t ListBox::OddLineSelected;

BorderedPrimitive ListBox::BackgroundPrimitive;
BorderedPrimitive ListBox::HoveredPrimitive;

void ListBox::staticInit(TexturedSkin *skin)
{
    Background=skin->getTextureEntryId("ListBox.Background");
    Hovered=skin->getTextureEntryId("ListBox.Hovered");
//    EvenLine=skin->getTextureEntryId("ListBox.EvenLine");
//    OddLine=skin->getTextureEntryId("ListBox.OddLine");
//    EvenLineSelected=skin->getTextureEntryId("ListBox.EvenLineSelected");
//    OddLineSelected=skin->getTextureEntryId("ListBox.OddLineSelected");
    BackgroundPrimitive.init(skin, Background);
    HoveredPrimitive.init(skin, Hovered);

    m_staticInit=true;
}

void ListBox::update(renderer::Base *renderer, controls::Base *baseControl)
{
    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, tokyonight::PanelDark);
}

///////////////////////////////////////////////////////////////////////////
//ListBoxRow
size_t ListBoxRow::EvenLine;
size_t ListBoxRow::OddLine;
size_t ListBoxRow::EvenLineSelected;
size_t ListBoxRow::OddLineSelected;

BorderedPrimitive ListBoxRow::EvenLinePrimitive;
BorderedPrimitive ListBoxRow::OddLinePrimitive;
BorderedPrimitive ListBoxRow::EvenLineSelectedPrimitive;
BorderedPrimitive ListBoxRow::OddLineSelectedPrimitive;

void ListBoxRow::staticInit(TexturedSkin *skin)
{
    EvenLine=skin->getTextureEntryId("ListBox.EvenLine");
    OddLine=skin->getTextureEntryId("ListBox.OddLine");
    EvenLineSelected=skin->getTextureEntryId("ListBox.EvenLineSelected");
    OddLineSelected=skin->getTextureEntryId("ListBox.OddLineSelected");

    EvenLinePrimitive.init(skin, EvenLine);
    OddLinePrimitive.init(skin, OddLine);
    EvenLineSelectedPrimitive.init(skin, EvenLineSelected);
    OddLineSelectedPrimitive.init(skin, OddLineSelected);

    m_staticInit=true;
}

void ListBoxRow::update(renderer::Base *renderer, controls::Base *baseControl)
{
    controls::ListBoxRow *control=static_cast<controls::ListBoxRow *>(baseControl);

    const tokyonight::Surface *surface;
    if(control->isSelected())
        surface=&tokyonight::ListItemSel;
    else if(baseControl->isHovered())
        surface=&tokyonight::ListItemHover;
    else
        surface=&tokyonight::ListItem;

    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, *surface);
}

///////////////////////////////////////////////////////////////////////////
//Menu
size_t Menu::RightArrow;
size_t Menu::Check;

size_t Menu::Strip;
size_t Menu::Background;
size_t Menu::BackgroundWithMargin;
size_t Menu::Hover;

BorderedPrimitive Menu::BackgroundPrimitive;

void Menu::staticInit(TexturedSkin *skin)
{
    RightArrow=skin->getTextureEntryId("Menu.RightArrow");
    Check=skin->getTextureEntryId("Menu.Check");
    Strip=skin->getTextureEntryId("Menu.Strip");
    Background=skin->getTextureEntryId("Menu.Background");
    BackgroundWithMargin=skin->getTextureEntryId("Menu.BackgroundWithMargin");
    Hover=skin->getTextureEntryId("Menu.Hover");

    BackgroundPrimitive.init(skin, Background);

    m_staticInit=true;
}

void Menu::generate(renderer::Base *renderer, controls::Base *baseControl)
{
    generatePrimitives(renderer, requiredPrimitives());
}

void Menu::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, tokyonight::Menu);
}

void Menu::remove(renderer::Base *renderer, controls::Base *baseControl)
{
    releasePrimitives(renderer);
}

///////////////////////////////////////////////////////////////////////////
//Panel
size_t Panel::Normal;
size_t Panel::Bright;
size_t Panel::Dark;
size_t Panel::Highlight;

BorderedPrimitive Panel::NormalPrimitive;

void Panel::staticInit(TexturedSkin *skin)
{
    Normal=skin->getTextureEntryId("Panel.Normal");
    Bright=skin->getTextureEntryId("Panel.Bright");
    Dark=skin->getTextureEntryId("Panel.Dark");
    Highlight=skin->getTextureEntryId("Panel.Highlight");

    m_staticInit=true;
}

void Panel::generate(renderer::Base *renderer, controls::Base *baseControl)
{
    generatePrimitives(renderer, requiredPrimitives());
}

void Panel::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, tokyonight::Panel);
}

void Panel::remove(renderer::Base *renderer, controls::Base *baseControl)
{
    releasePrimitives(renderer);
}

///////////////////////////////////////////////////////////////////////////
// Simple container/panel renderers: each draws one SDF rounded surface.
#define GWENI_SIMPLE_PANEL(Class, surface)                                     \
size_t Class::Background;                                                       \
BorderedPrimitive Class::BackgroundPrimitive;                                   \
void Class::staticInit(TexturedSkin *skin)                                      \
{                                                                              \
    m_staticInit=true;                                                         \
}                                                                              \
void Class::update(renderer::Base *renderer, controls::Base *baseControl)      \
{                                                                              \
    if(m_primitives.empty())                                                   \
        return;                                                                \
    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, surface); \
}

GWENI_SIMPLE_PANEL(PageControl, tokyonight::Panel)
GWENI_SIMPLE_PANEL(TabControlInner, tokyonight::TabControl)
GWENI_SIMPLE_PANEL(CollapsibleList, tokyonight::CategoryList)
GWENI_SIMPLE_PANEL(CategoryInner, tokyonight::CategoryInner)
#undef GWENI_SIMPLE_PANEL


///////////////////////////////////////////////////////////////////////////
//ProgressBar
size_t ProgressBar::Back;
size_t ProgressBar::Front;

BorderedPrimitive ProgressBar::BackgroundPrimitive;
BorderedPrimitive ProgressBar::FrontPrimitive;

void ProgressBar::staticInit(TexturedSkin *skin)
{
    Back=skin->getTextureEntryId("ProgressBar.Back");
    Front=skin->getTextureEntryId("ProgressBar.Front");

    BackgroundPrimitive.init(skin, Back);
    FrontPrimitive.init(skin, Front);

    m_staticInit=true;
}

void ProgressBar::generate(renderer::Base *renderer, controls::Base *baseControl)
{
    generatePrimitives(renderer, requiredPrimitives());
}

void ProgressBar::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.size() < 2)
        return;

    controls::ProgressBar *progress=static_cast<controls::ProgressBar *>(baseControl);
    const gweni::Rect bounds=baseControl->getRenderBounds();
    const int zIndex=baseControl->getZIndex();
    const bool visible=baseControl->visible();

    if(visible)
    {
        const tokyonight::Surface &back=tokyonight::ProgressBack;
        renderer->drawRoundedRect(m_primitives[0], bounds, back.radius, back.borderWidth,
            back.top, back.bottom, back.border, zIndex);

        float p=progress->getProgress();
        p=(p < 0.0f) ? 0.0f : (p > 1.0f ? 1.0f : p);

        gweni::Rect front=bounds;

        if(progress->isHorizontal())
            front.w=(int)(bounds.w*p);
        else
        {
            // a vertical bar fills from the bottom up
            front.h=(int)(bounds.h*p);
            front.y=bounds.y+bounds.h-front.h;
        }

        // +1, over the track (depthLayers() reserves it)
        const tokyonight::Surface &fr=tokyonight::ProgressFront;
        renderer->drawRoundedRect(m_primitives[1], front, fr.radius, fr.borderWidth,
            fr.top, fr.bottom, fr.border, zIndex+1);
    }

    if((baseControl->getStateChange()&controls::StateChange_Visibility) ||
        (baseControl->getStateChange()&controls::StateChange_Created))
    {
        renderer->showPrimitive(visible?4:0, m_primitives[0]);
        renderer->showPrimitive(visible?4:0, m_primitives[1]);
    }
}

void ProgressBar::remove(renderer::Base *renderer, controls::Base *baseControl)
{
    releasePrimitives(renderer);
}


///////////////////////////////////////////////////////////////////////////
//RadioButton
size_t RadioButton::ActiveNormal;
size_t RadioButton::ActiveChecked;
size_t RadioButton::DisabledNormal;
size_t RadioButton::DisabledChecked;

BorderedPrimitive RadioButton::NormalPrimitive;

void RadioButton::staticInit(TexturedSkin *skin)
{
    ActiveNormal=skin->getTextureEntryId("RadioButton.Active.Normal");
    DisabledNormal=skin->getTextureEntryId("RadioButton.Disabled.Normal");
    ActiveChecked=skin->getTextureEntryId("RadioButton.Active.Checked");
    DisabledChecked=skin->getTextureEntryId("RadioButton.Disabled.Checked");

    m_staticInit=true;
}

void RadioButton::generate(renderer::Base *renderer, controls::Base *baseControl)
{
    generatePrimitives(renderer, requiredPrimitives());
}

void RadioButton::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.size() < 2)
        return;

    // RadioButton derives from CheckBox, so isChecked() applies. A very large
    // radius is clamped to half the box by the SDF shader -> a perfect circle.
    controls::CheckBox *control=static_cast<controls::CheckBox *>(baseControl);

    tokyonight::Surface ring=tokyonight::CheckOff;
    ring.radius=1000.0f;
    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, ring);

    // Inner blue dot when selected (a small centered circle).
    const gweni::Rect r=baseControl->getRenderBounds();
    if(baseControl->visible() && control->isChecked())
    {
        const int dw=r.w*4/10, dh=r.h*4/10;
        const gweni::Rect dot{ r.x+(r.w-dw)/2, r.y+(r.h-dh)/2, dw, dh };
        renderer->drawRoundedRect(m_primitives[1], dot, 1000.0f, 0.0f,
            tokyonight::Blue, tokyonight::DarkBlue, tokyonight::Blue, baseControl->getZIndex()+1);
    }
    else
        renderer->hidePrimitive(m_primitives[1]);
}

void RadioButton::remove(renderer::Base *renderer, controls::Base *baseControl)
{
    releasePrimitives(renderer);
}


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

    const tokyonight::Surface *surface=&tokyonight::ScrollThumb;

    if(control->isDepressed())
        surface=&tokyonight::ScrollThumbDown;
    else if(control->isHovered())
        surface=&tokyonight::ScrollThumbHover;

    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, *surface);

    // Directional chevron centered on the button.
    if(m_primitives.size() >= 2)
    {
        const Alignment direction=control->getDirection();
        int dir=0; // Bottom -> down
        if(direction == Alignment::Left)       dir=2;
        else if(direction == Alignment::Right) dir=3;
        else if(direction == Alignment::Top)   dir=1;

        const gweni::Rect r=baseControl->getRenderBounds();
        const int sz=((r.w<r.h)?r.w:r.h)*6/10;
        const gweni::Rect arrow{ r.x+(r.w-sz)/2, r.y+(r.h-sz)/2, sz, sz };
        if(baseControl->visible())
            renderer->drawIcon(m_primitives[1], arrow, 1, dir, tokyonight::Fg, baseControl->getZIndex()+1);
        else
            renderer->hidePrimitive(m_primitives[1]);
    }
}

///////////////////////////////////////////////////////////////////////////
//Scroller
size_t Scroller::TrackV;
size_t Scroller::TrackH;
BorderedPrimitive Scroller::TrackPrimitive;

void Scroller::staticInit(TexturedSkin *skin)
{
    ButtonTemplate<ScrollerButton>::staticInit(skin);
    ButtonTemplate<ScrollerHorizontal>::staticInit(skin);
    ButtonTemplate<ScrollerVertical>::staticInit(skin);

    TrackV=skin->getTextureEntryId("Scroller.TrackV");
    TrackH=skin->getTextureEntryId("Scroller.TrackH");

    TrackPrimitive.init(skin, TrackV);

    m_staticInit=true;
}

void Scroller::generate(renderer::Base *renderer, controls::Base *baseControl)
{
    generatePrimitives(renderer, requiredPrimitives());
}

void Scroller::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, tokyonight::ScrollTrack);
}

void Scroller::remove(renderer::Base *renderer, controls::Base *baseControl)
{
    releasePrimitives(renderer);
}


///////////////////////////////////////////////////////////////////////////
//Shadow
size_t Shadow::Background;
BorderedPrimitive Shadow::BackgroundPrimitive;

void Shadow::staticInit(TexturedSkin *skin)
{
    Background=skin->getTextureEntryId("Shadow.Background");

    BackgroundPrimitive.init(skin, Background);

    m_staticInit=true;
}

void Shadow::generate(renderer::Base *renderer, controls::Base *baseControl)
{
    generatePrimitives(renderer, requiredPrimitives());
}

void Shadow::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    // Soft translucent dark drop shadow behind elevated controls.
    static const tokyonight::Surface shadow{ Color(0,0,0,90), Color(0,0,0,90),
        Color(0,0,0,0), tokyonight::RadiusLg, 0.0f };
    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, shadow);
}

void Shadow::remove(renderer::Base *renderer, controls::Base *baseControl)
{
    releasePrimitives(renderer);
}


///////////////////////////////////////////////////////////////////////////
//Slider
void Slider::resizePrimitives(renderer::Base *renderer, size_t count)
{
    while(m_primitives.size() > count)
    {
        renderer->releasePrimitive(m_primitives.back());
        m_primitives.pop_back();
    }

    while(m_primitives.size() < count)
    {
        size_t id;

        renderer->generatePrimitive(id);
        m_primitives.push_back(id);
    }
}

void Slider::update(renderer::Base *renderer, controls::Base *baseControl)
{
    controls::Slider *slider=static_cast<controls::Slider *>(baseControl);
    const int notches=slider->getClampToNotches()?slider->getNotchCount():0;
    const size_t ticks=(notches > 0)?size_t(notches)+1:0;

    resizePrimitives(renderer, 1+ticks);

    // The track runs between the thumb's centre at either end of its travel.
    const gweni::Rect r=baseControl->getRenderBounds();
    const gweni::Rect bar=slider->getSliderBar()->getBounds();
    const bool horizontal=slider->isHorizontal();
    gweni::Rect track;

    if(horizontal)
    {
        track=gweni::Rect(r.x+bar.w/2, r.y+(r.h-TrackThickness)/2, r.w-bar.w, TrackThickness);
        track.w=std::max(track.w, 0);
    }
    else
    {
        track=gweni::Rect(r.x+(r.w-TrackThickness)/2, r.y+bar.h/2, TrackThickness, r.h-bar.h);
        track.h=std::max(track.h, 0);
    }

    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, tokyonight::SliderTrack, track);

    // Ticks beside the thumb (below it, or right of it), clear of the track so
    // they never share its depth, and pulled in to stay inside a thin slider.
    const bool visible=baseControl->visible();
    const tokyonight::Surface &notch=tokyonight::SliderNotch;
    const int crossSize=horizontal?r.h:r.w;
    const int tickLength=4;
    int offset=std::min(std::min(crossSize, ThumbThickness)/2+2, crossSize/2-tickLength);

    offset=std::max(offset, TrackThickness/2+1);

    for(size_t i=0; i<ticks; ++i)
    {
        const size_t id=m_primitives[1+i];

        if(!visible)
        {
            renderer->hidePrimitive(id);
            continue;
        }

        gweni::Rect tick;

        if(horizontal)
        {
            const int x=track.x+int(i)*track.w/notches;
            tick=gweni::Rect(x-1, r.y+r.h/2+offset, 2, tickLength);
        }
        else
        {
            const int y=track.y+int(i)*track.h/notches;
            tick=gweni::Rect(r.x+r.w/2+offset, y-1, tickLength, 2);
        }

        renderer->drawRoundedRect(id, tick, notch.radius, notch.borderWidth,
            notch.top, notch.bottom, notch.border, baseControl->getZIndex());
    }
}


///////////////////////////////////////////////////////////////////////////
//SliderBar
void SliderBar::update(renderer::Base *renderer, controls::Base *baseControl)
{
    controls::internal::SliderBar *bar=static_cast<controls::internal::SliderBar *>(baseControl);
    const tokyonight::Surface *surface;

    if(bar->isDisabled())
        surface=&tokyonight::SliderThumbDisabled;
    else if(bar->isDepressed())
        surface=&tokyonight::SliderThumbDown;
    else if(bar->isHovered())
        surface=&tokyonight::SliderThumbHover;
    else
        surface=&tokyonight::SliderThumb;

    // The bar spans the whole slider across its axis (that is its hit area);
    // the thumb drawn in it is at most Slider::ThumbThickness thick, centred.
    gweni::Rect thumb=baseControl->getRenderBounds();

    if(bar->isHorizontal())
    {
        const int h=std::min(thumb.h, Slider::ThumbThickness);
        thumb.y+=(thumb.h-h)/2;
        thumb.h=h;
    }
    else
    {
        const int w=std::min(thumb.w, Slider::ThumbThickness);
        thumb.x+=(thumb.w-w)/2;
        thumb.w=w;
    }

    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, *surface, thumb);
}


///////////////////////////////////////////////////////////////////////////
//ColorLerpBox
void ColorLerpBox::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.size() < 2)
        return;

    controls::ColorLerpBox *box=static_cast<controls::ColorLerpBox *>(baseControl);
    const gweni::Rect r=baseControl->getRenderBounds();
    const bool visible=baseControl->visible();
    // the field in the lower slot, drawn first, so the marker's AA rim blends
    // over it (see ColorSlider::update)
    const size_t field=std::min(m_primitives[0], m_primitives[1]);
    const size_t cursorMarker=std::max(m_primitives[0], m_primitives[1]);

    if(visible)
    {
        const tokyonight::Surface &surface=tokyonight::ColorField;
        const tokyonight::Surface &marker=tokyonight::ColorMarker;
        const gweni::Color white(255, 255, 255, 255);
        const gweni::Color black(0, 0, 0, 255);

        renderer->drawBilinearRect(field, r, baseControl->getZIndex(), white, box->getHueColor(),
            black, black, surface.radius, surface.borderWidth, surface.border);

        // centred on the cursor, kept inside the box
        const gweni::Point &cursor=box->getCursorPos();
        const int size=std::max(std::min({MarkerSize, r.w, r.h}), 0);
        const gweni::Rect rect(r.x+clamp(cursor.x-size/2, 0, r.w-size),
            r.y+clamp(cursor.y-size/2, 0, r.h-size), size, size);

        renderer->drawRoundedRect(cursorMarker, rect, marker.radius, marker.borderWidth,
            marker.top, marker.bottom, marker.border, baseControl->getZIndex()+1);
    }

    const unsigned int sc=baseControl->getStateChange();
    if((sc&controls::StateChange_Visibility) || (sc&controls::StateChange_Created))
    {
        renderer->showPrimitive(visible?6:0, field);
        renderer->showPrimitive(visible?4:0, cursorMarker);
    }
}


///////////////////////////////////////////////////////////////////////////
//ColorSlider
void ColorSlider::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.size() < 7)
        return;

    controls::ColorSlider *slider=static_cast<controls::ColorSlider *>(baseControl);
    const gweni::Rect r=baseControl->getRenderBounds();
    const bool visible=baseControl->visible();

    // OpenGLCore draws one buffer in slot order, so the lowest slots are drawn
    // first: the three back segments take those, then the three front ones,
    // then the marker (see below for why the order matters).
    size_t slots[7];

    std::copy(m_primitives.begin(), m_primitives.begin()+7, slots);
    std::sort(slots, slots+7);

    // getColorAtHeight() divides by the height
    if(visible && r.h > 0)
    {
        const int x=r.x+Inset;
        const int w=std::max(r.w-2*Inset, 0);
        const int z=baseControl->getZIndex();

        // Segment i is hue arc i, joint i (between segments i-1 and i) at row
        // i*h/6. The odd segments are in front (z+1) and overlap the even ones
        // by a row at each joint, because OpenGLCore anti-aliases a rect's
        // outermost row to ~84% cover (#208): two rects that only abut leave a
        // darker seam row. The front segment's edge row then blends over the
        // segment behind it, not the background, and the one behind ends under
        // the front one. That needs the segments behind drawn first, since the
        // GL_LESS depth test rejects them where a nearer one already is.
        int tops[6];
        int bottoms[6];

        tops[0]=0;
        bottoms[5]=r.h;
        for(int i=1; i<6; ++i)
        {
            if(i&1)
            {
                // the segment behind, above, ends at the joint rounded up
                bottoms[i-1]=(i*r.h+5)/6;
                tops[i]=bottoms[i-1]-1;
            }
            else
            {
                // the segment behind, below, starts at the joint rounded down
                tops[i]=i*r.h/6;
                bottoms[i-1]=tops[i]+1;
            }
        }

        for(int i=0; i<6; ++i)
        {
            // OpenGLCore draws a rect 2*(h/2) rows tall, so an odd height grows
            // by a row, further under or over its neighbour
            if((bottoms[i]-tops[i])&1)
            {
                if(i < 5)
                    bottoms[i]=std::min(bottoms[i]+1, r.h);
                else
                    tops[i]=std::max(tops[i]-1, 0);
            }

            const bool front=(i&1) != 0;
            const int top=clamp(tops[i], 0, r.h);
            const int bottom=clamp(bottoms[i], top, r.h);

            // Each stop is the strip's own colour at that edge, the one a click
            // there selects. A segment that reaches a row into its neighbour's
            // arc then bends a little towards it instead of overshooting, so
            // the overlap row continues the ramp.
            renderer->drawRoundedRect(slots[front?3+i/2:i/2], gweni::Rect(x, r.y+top, w, bottom-top),
                0.0f, 0.0f, slider->getColorAtHeight(top), slider->getColorAtHeight(bottom),
                gweni::Color(0, 0, 0, 0), front?z+1:z);
        }

        // the selection bar across the whole width, centred on the selected
        // row, in front of the front segments
        const tokyonight::Surface &marker=tokyonight::ColorMarker;
        const int height=std::max(std::min(MarkerHeight, r.h), 0);
        const gweni::Rect rect(r.x, r.y+clamp(slider->getSelectedDist()-height/2, 0, r.h-height),
            r.w, height);

        renderer->drawRoundedRect(slots[6], rect, marker.radius, marker.borderWidth,
            marker.top, marker.bottom, marker.border, z+2);
    }

    const unsigned int sc=baseControl->getStateChange();
    if((sc&controls::StateChange_Visibility) || (sc&controls::StateChange_Created))
    {
        for(size_t i=0; i<7; ++i)
            renderer->showPrimitive(visible?4:0, slots[i]);
    }
}


///////////////////////////////////////////////////////////////////////////
//StatusBar
size_t StatusBar::Background;
size_t StatusBar::Selection;
BorderedPrimitive StatusBar::BackgroundPrimitive;

void StatusBar::staticInit(TexturedSkin *skin)
{
    Background=skin->getTextureEntryId("StatusBar.Background");
    BackgroundPrimitive.init(skin, Background);
    Selection=skin->getTextureEntryId("StatusBar.Selection");

    m_staticInit=true;
}

//void StatusBar::generate(renderer::Base *renderer, controls::Base *baseControl)
//{}

void StatusBar::update(renderer::Base *renderer, controls::Base *baseControl)
{
    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, tokyonight::StatusBar);
}

//void StatusBar::remove(renderer::Base *renderer, controls::Base *baseControl)
//{}


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

    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl,
        control->isActive()?tokyonight::TabActive:tokyonight::TabInactive);
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
    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, tokyonight::TabControl);
}

///////////////////////////////////////////////////////////////////////////
//TabTitleBar
//
size_t TabTitleBar::HeaderBar;
BorderedPrimitive TabTitleBar::HeaderBarPrimitive;

void TabTitleBar::staticInit(TexturedSkin *skin)
{
    HeaderBar=skin->getTextureEntryId("TabControl.HeaderBar");
    HeaderBarPrimitive.init(skin, HeaderBar);
    m_staticInit=true;
}

void TabTitleBar::update(renderer::Base *renderer, controls::Base *baseControl)
{
    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, tokyonight::Panel);
}

///////////////////////////////////////////////////////////////////////////
//Text
void Text::staticInit(TexturedSkin *skin)
{
    m_staticInit=true;
}

void Text::generate(renderer::Base *renderer, controls::Base *baseControl)
{
    m_primitives.resize(1);

    renderer->generateTextPrimitive(m_primitives[0]);
}

void Text::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(baseControl->getStateChange() == controls::StateChange_Nothing)
        return;

    // getTypeName()=="Text" is ambiguous: both controls::internal::Text and
    // controls::property::Text report it, so this renderer can be handed a
    // property::Text. dynamic_cast bails on anything that isn't really an
    // internal::Text instead of corrupting memory via a bad static_cast.
    controls::internal::Text *control=dynamic_cast<controls::internal::Text *>(baseControl);

    if(!control)
        return;

    const gweni::Color &color=control->getTextColor();
    const gweni::Color &colorOverride=control->getTextColorOverride();

    if(colorOverride.a == 0)
        renderer->setDrawColor(color);
    else
        renderer->setDrawColor(colorOverride);

//    renderer->renderText(m_primitives[0], control->getFont(),
//        gweni::Point(control->getPadding().left, control->getPadding().top), control->getZIndex(),
//        control->getText());

    // Wrapped, the string is drawn by the line children; drawn here as well it
    // would lie as one long line over the first of them.
    static const String noText;

    renderer->renderText(m_primitives[0], control->getFont(),
        gweni::Point(control->getBounds().x, control->getBounds().y), control->getZIndex(),
        control->wrap()?noText:control->getText());

    if((baseControl->getStateChange() & controls::StateChange_Visibility) || 
        (baseControl->getStateChange() & controls::StateChange_Created))
    {
        if(!baseControl->visible())
            renderer->hideText(m_primitives[0]);
        else
            renderer->showText(m_primitives[0]);
    }
}

void Text::remove(renderer::Base *renderer, controls::Base *baseControl)
{
    if(!m_primitives.empty())
        renderer->releaseTextPrimitive(m_primitives[0]);
}




///////////////////////////////////////////////////////////////////////////
//TextBox
size_t TextBox::Normal;
size_t TextBox::Focus;
size_t TextBox::Disabled;
BorderedPrimitive TextBox::NormalPrimitive;

void TextBox::staticInit(TexturedSkin *skin)
{
    Normal=skin->getTextureEntryId("TextBox.Normal");
    Focus=skin->getTextureEntryId("TextBox.Focus");
    Disabled=skin->getTextureEntryId("TextBox.Disabled");

    m_staticInit=true;
}

void TextBox::generate(renderer::Base *renderer, controls::Base *baseControl)
{
    generatePrimitives(renderer, requiredPrimitives());
}

void TextBox::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    const tokyonight::Surface *surface=&tokyonight::TextBox;
    if(baseControl->isDisabled())
        surface=&tokyonight::TextBoxDisabled;
    else if(baseControl->isFocussed())
        surface=&tokyonight::TextBoxFocus;

    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, *surface);
}

void TextBox::remove(renderer::Base *renderer, controls::Base *baseControl)
{
    releasePrimitives(renderer);
}


///////////////////////////////////////////////////////////////////////////
//Tooltip
size_t Tooltip::Background;
BorderedPrimitive Tooltip::BackgroundPrimitive;

void Tooltip::staticInit(TexturedSkin *skin)
{
    Background=skin->getTextureEntryId("Tooltip.Background");

    BackgroundPrimitive.init(skin, Background);

    m_staticInit=true;
}

void Tooltip::generate(renderer::Base *renderer, controls::Base *baseControl)
{
    generatePrimitives(renderer, requiredPrimitives());
}

void Tooltip::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, tokyonight::Tooltip);
}

void Tooltip::remove(renderer::Base *renderer, controls::Base *baseControl)
{
    releasePrimitives(renderer);
}


///////////////////////////////////////////////////////////////////////////
//Tree
size_t Tree::Background;
size_t Tree::Minus;
size_t Tree::Plus;
BorderedPrimitive Tree::BackgroundPrimitive;

void Tree::staticInit(TexturedSkin *skin)
{
    Background=skin->getTextureEntryId("Tree.Background");
    Plus=skin->getTextureEntryId("Tree.Plus");
    Minus=skin->getTextureEntryId("Tree.Minus");

    BackgroundPrimitive.init(skin, Background);

    m_staticInit=true;
}

void Tree::generate(renderer::Base *renderer, controls::Base *baseControl)
{
    generatePrimitives(renderer, requiredPrimitives());
}

void Tree::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, tokyonight::PanelDark);
}

void Tree::remove(renderer::Base *renderer, controls::Base *baseControl)
{
    releasePrimitives(renderer);
}

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
BorderedPrimitive SpinBox::BackgroundPrimitive;

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
//SplitterBar
size_t SplitterBar::Background;
BorderedPrimitive SplitterBar::BackgroundPrimitive;

void SplitterBar::staticInit(TexturedSkin *skin)
{
//    Background=skin->getTextureEntryId("SplitterBar.Background");
    m_staticInit=true;
}

void SplitterBar::update(renderer::Base *renderer, controls::Base *baseControl)
{}

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
BorderedPrimitive Window::NormalPrimitive;

void Window::staticInit(TexturedSkin *skin)
{
    Normal=skin->getTextureEntryId("Window.Normal");
    Inactive=skin->getTextureEntryId("Window.Inactive");

    NormalPrimitive.init(skin, Normal);

    m_staticInit=true;
}

void Window::generate(renderer::Base *renderer, controls::Base *baseControl)
{
    generatePrimitives(renderer, requiredPrimitives());
}

void Window::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, tokyonight::Window);
}

void Window::remove(renderer::Base *renderer, controls::Base *baseControl)
{
    releasePrimitives(renderer);
}

// Force every ControlRegister<> specialization to instantiate so its static
// self-registration initializer actually runs at startup. Without this, the
// `forceInit`/m_forceTypeName trick never gets odr-used under modern toolchains
// (e.g. gcc 11), ControlFactory's table stays empty, every control fails to
// find a skin renderer, and nothing is drawn.
template class ControlRegister<Base>;
template class ControlRegister<ButtonTemplate<DefaultButton>>;
template class ControlRegister<Canvas>;
template class ControlRegister<CategoryHeaderButton>;
template class ControlRegister<CategoryButton>;
template class ControlRegister<CategoryList>;
template class ControlRegister<CollapsibleCategory>;
template class ControlRegister<CheckBox>;
template class ControlRegister<ComboBox>;
template class ControlRegister<DockBase>;
template class ControlRegister<GroupBox>;
template class ControlRegister<Input>;
template class ControlRegister<ListBox>;
template class ControlRegister<ListBoxRow>;
template class ControlRegister<Menu>;
template class ControlRegister<Panel>;
template class ControlRegister<PageControl>;
template class ControlRegister<TabControlInner>;
template class ControlRegister<CollapsibleList>;
template class ControlRegister<CategoryInner>;
template class ControlRegister<Label>;
template class ControlRegister<Resizer>;
template class ControlRegister<Table>;
template class ControlRegister<TabStrip>;
template class ControlRegister<VerticalScrollBar>;
template class ControlRegister<HorizontalScrollBar>;
template class ControlRegister<ScrollBarBar>;
template class ControlRegister<ProgressBar>;
template class ControlRegister<RadioButton>;
template class ControlRegister<ScrollBarButton>;
template class ControlRegister<Scroller>;
template class ControlRegister<Shadow>;
template class ControlRegister<Slider>;
template class ControlRegister<SliderBar>;
template class ControlRegister<ColorLerpBox>;
template class ControlRegister<ColorSlider>;
template class ControlRegister<StatusBar>;
// NOTE: TabItem<_Type> is itself a class template (TabItem<TabTop>, <TabBottom>,
// <TabLeft>, <TabRight>); register those specializations here if/when their
// per-position tab skinning is needed. Until then tabs fall back to Base.
template class ControlRegister<TabButton>;
template class ControlRegister<TabControl>;
template class ControlRegister<TabTitleBar>;
template class ControlRegister<Text>;
template class ControlRegister<TextBox>;
template class ControlRegister<Tooltip>;
template class ControlRegister<Tree>;
template class ControlRegister<SpinBox>;
template class ControlRegister<SplitterBar>;
template class ControlRegister<Window>;

}//namespace textured
}//namespace skin
}//namespace gweni
