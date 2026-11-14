#include "gweni/skins/textured/texturedControls.h"
#include "gweni/skins/textured/texturedSkin.h"

#include "gweni/controls/canvas.h"
#include "gweni/controls/progressBar.h"
#include "gweni/controls/comboBox.h"
#include "gweni/controls/checkBox.h"
#include "gweni/controls/codeEditor.h"
#include "gweni/controls/logView.h"
#include "gweni/controls/scrollBarButton.h"
#include "gweni/controls/scrollBarBar.h"
#include "gweni/controls/statusBar.h"
#include "gweni/controls/tabControl.h"
#include "gweni/controls/text.h"
#include "gweni/controls/collapsibleCategory.h"
#include "gweni/controls/listBox.h"
#include "gweni/controls/slider.h"
#include "gweni/controls/spinner.h"
#include "gweni/controls/plotHistogram.h"
#include "gweni/controls/plotLines.h"
#include "gweni/controls/chart.h"
#include "gweni/controls/nodeGraph.h"
#include "gweni/controls/colorControls.h"
#include "gweni/controls/colorPicker.h"
#include "gweni/controls/colorButton.h"
#include "gweni/controls/dragNumber.h"
#include "gweni/controls/vectorField.h"
#include "gweni/controls/badge.h"
#include "gweni/controls/chip.h"
#include "gweni/controls/switch.h"
#include "gweni/controls/textBox.h"
#include "gweni/controls/separator.h"
#include "gweni/controls/segmentedButtons.h"
#include "gweni/controls/stepper.h"
#include "gweni/controls/selectable.h"
#include "gweni/controls/navigationRail.h"
#include "gweni/controls/splitterBar.h"
#include "gweni/controls/menuItem.h"
#include "gweni/controls/bullet.h"
#include "gweni/controls/windowButtons.h"
#include "gweni/controls/dialogs/dialog.h"
#include "gweni/controls/toast.h"
#include "gweni/controls/properties.h"
#include "gweni/controls/dataTable.h"
#include "gweni/controls/treeTable.h"
#include "gweni/controls/treeNode.h"
#include "gweni/controls/propertyTree.h"
#include "gweni/tools/inspector.h"

#include <algorithm>
#include <cmath>


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
// The frame of a CheckBox or RadioButton: disabled first, then hovered (the
// box itself or the label of its row), then at rest.
static const tokyonight::Surface &checkSurface(controls::CheckBox *control)
{
    if(control->isDisabled())
        return tokyonight::CheckDisabled;
    if(control->isHovered() || control->isLabelHovered())
        return tokyonight::CheckHover;
    return tokyonight::CheckOff;
}

void CheckBox::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.size() < 2)
        return;

    // getTypeName()=="CheckBox" is ambiguous, as for Text below: property::CheckBox
    // reports it too. That one is the row's container (its child CheckBox draws),
    // so it draws nothing here rather than be static_cast to a CheckBox.
    controls::CheckBox *control=dynamic_cast<controls::CheckBox *>(baseControl);

    if(!control)
    {
        for(size_t primitive : m_primitives)
            renderer->hidePrimitive(primitive);
        return;
    }

    // Dark box frame, lit while it or its row's label is hovered; blue
    // checkmark glyph overlaid when checked. Disabled: a faint frame and a
    // muted tick, and no hover.
    const bool disabled=control->isDisabled();

    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, checkSurface(control));

    if(baseControl->visible() && control->isChecked())
        renderer->drawIcon(m_primitives[1], baseControl->getRenderBounds(), 0, 0,
            disabled?tokyonight::Muted:tokyonight::Blue, baseControl->getZIndex()+1);
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
    // As for CheckBox: property::ComboBox reports "ComboBox" too, and its child
    // ComboBox is what draws.
    controls::ComboBox *control=dynamic_cast<controls::ComboBox *>(baseControl);
    const tokyonight::Surface *surface;

    if(!control)
    {
        for(size_t primitive : m_primitives)
            renderer->hidePrimitive(primitive);
        return;
    }

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
        // The column the control keeps for it as its right padding.
        const int sz=controls::ComboBox::ChevronSize;
        const gweni::Rect arrow{ r.x+r.w-sz-controls::ComboBox::ChevronInset, r.y+(r.h-sz)/2, sz, sz };
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
//VirtualList
void VirtualList::update(renderer::Base *renderer, controls::Base *baseControl)
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
//MenuStrip
void MenuStrip::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, tokyonight::MenuStrip);
}

///////////////////////////////////////////////////////////////////////////
//MenuItem
void MenuItem::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    controls::MenuItem *item=static_cast<controls::MenuItem *>(baseControl);
    const bool highlighted=!item->isDisabled() &&
        (item->isHovered() || item->isDepressed() || item->isMenuOpen());

    // an idle row draws nothing: a transparent rect would still write depth over
    // the backdrop, so the slot is hidden instead (drawRoundedRect shows it)
    if(highlighted && baseControl->visible())
    {
        const tokyonight::Surface &surface=tokyonight::MenuHover;

        renderer->drawRoundedRect(m_primitives[0], baseControl->getRenderBounds(), surface.radius,
            surface.borderWidth, surface.top, surface.bottom, surface.border, baseControl->getZIndex());
    }
    else
        renderer->hidePrimitive(m_primitives[0]);

    if(m_primitives.size() < 2)
        return;

    const gweni::Rect check=checkRect(item);

    if(baseControl->visible() && item->isCheckable() && item->isChecked() && check.w > 0)
        renderer->drawIcon(m_primitives[1], check, 0, 0, item->getTextColor(), baseControl->getZIndex()+1);
    else
        renderer->hidePrimitive(m_primitives[1]);
}

gweni::Rect MenuItem::checkRect(controls::MenuItem *item)
{
    if(item->onStrip())
        return gweni::Rect(0, 0, 0, 0);

    const gweni::Rect r=item->getRenderBounds();
    const int column=item->getTextPadding().left;
    const int sz=((column<r.h)?column:r.h)*6/10;

    if(sz <= 0)
        return gweni::Rect(0, 0, 0, 0);

    return gweni::Rect(r.x+(column-sz)/2, r.y+(r.h-sz)/2, sz, sz);
}

///////////////////////////////////////////////////////////////////////////
//RightArrow
void RightArrow::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    if(!baseControl->visible())
    {
        renderer->hidePrimitive(m_primitives[0]);
        return;
    }

    // the item's text colour, so the chevron follows its hover and disabled looks
    controls::Label *item=gweni_cast<controls::Label>(baseControl->getParent());
    const gweni::Color colour=item?item->getTextColor():tokyonight::Fg;

    // kind 1's directions: 0 down, 3 right
    const gweni::Rect r=baseControl->getRenderBounds();
    const int sz=((r.w<r.h)?r.w:r.h)*6/10;
    const gweni::Rect chevron{ r.x+(r.w-sz)/2, r.y+(r.h-sz)/2, sz, sz };

    renderer->drawIcon(m_primitives[0], chevron, 1, 3, colour, baseControl->getZIndex());
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

        gweni::Rect front=bounds;

        if(progress->isIndeterminate())
        {
            // the moving segment, already cut to the track: the bar's own
            // render() is clipped by its parent's region, not its own
            front=progress->indeterminateSegment(bounds);
        }
        else
        {
            float p=progress->getProgress();
            p=(p < 0.0f) ? 0.0f : (p > 1.0f ? 1.0f : p);

            if(progress->isHorizontal())
                front.w=(int)(bounds.w*p);
            else
            {
                // a vertical bar fills from the bottom up
                front.h=(int)(bounds.h*p);
                front.y=bounds.y+bounds.h-front.h;
            }
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
//CodeEditor
// A blink or a caret move (StateChange_Caret alone) rewrites the caret's slot
// alone; the field is painted again on any other change. The caret's rect is
// worked out from the editor's last layout every time, so a slot whose bytes
// stay the same is not written.
void CodeEditor::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.size() < 2)
        return;

    controls::CodeEditor *editor=static_cast<controls::CodeEditor *>(baseControl);

    if(editor->getStateChange() != controls::StateChange_Caret)
    {
        // plain (a form field): the text field's look
        const tokyonight::Surface &surface=editor->isPlain()
            ?(editor->isDisabled()?tokyonight::TextBoxDisabled
                :editor->isFocussed()?tokyonight::TextBoxFocus:tokyonight::TextBox)
            :(editor->isFocussed()?tokyonight::CodeEditorFocus:tokyonight::CodeEditor);

        tokyonight::paintSurface(renderer, m_primitives.data(), editor, surface);
    }

    const Rect caret=editor->getCaretBounds();
    const Rect &bounds=editor->getRenderBounds();

    if(!editor->visible() || !editor->isFocussed() || !editor->isCaretVisible() || caret.w <= 0 || caret.h <= 0)
    {
        renderer->hidePrimitive(m_primitives[1]);
        return;
    }

    renderer->setDrawColor(editor->getSkin()->Colors.Label.Default);
    renderer->drawFilledRect(m_primitives[1], Rect(bounds.x+caret.x, bounds.y+caret.y, caret.w, caret.h),
        editor->getOverlayZIndex());
}

///////////////////////////////////////////////////////////////////////////
//LogView
void LogView::update(renderer::Base *renderer, controls::Base *baseControl)
{
    const tokyonight::Surface &surface=baseControl->isFocussed()?tokyonight::LogViewFocus:tokyonight::LogView;

    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, surface);
}

///////////////////////////////////////////////////////////////////////////
//LogViewRow
// A slot per highlight rect, one at least: an unselected row's is hidden, not
// drawn transparent, which would still write depth over the field behind
// it; the slots past the rects are given back.
void LogViewRow::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    controls::internal::LogViewRow *row=static_cast<controls::internal::LogViewRow *>(baseControl);
    const tokyonight::Surface &selection=tokyonight::LogViewSelection;

    m_rects.clear();
    if(row->visible())
        row->getSelectionRects(m_rects);

    while(m_primitives.size() > std::max<size_t>(1, m_rects.size()))
    {
        renderer->releasePrimitive(m_primitives.back());
        m_primitives.pop_back();
    }
    while(m_primitives.size() < m_rects.size())
    {
        size_t id;

        renderer->generatePrimitive(id);
        m_primitives.push_back(id);
    }

    const Rect &bounds=row->getRenderBounds();

    for(size_t i=0; i < m_rects.size(); ++i)
    {
        const Rect &local=m_rects[i];

        renderer->drawRoundedRect(m_primitives[i], Rect(bounds.x+local.x, bounds.y+local.y, local.w, local.h),
            selection.radius, selection.borderWidth, selection.top, selection.bottom, selection.border,
            row->getTextZIndex());
    }
    if(m_rects.empty())
        renderer->hidePrimitive(m_primitives[0]);
}

///////////////////////////////////////////////////////////////////////////
//LogViewChevron
// kind 1's directions: 0 down, 3 right; the chevron a square of 6/10 of the
// smaller side, centred, as a menu item's.
void LogViewChevron::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    if(!baseControl->visible())
    {
        renderer->hidePrimitive(m_primitives[0]);
        return;
    }

    controls::internal::LogViewChevron *chevron=static_cast<controls::internal::LogViewChevron *>(baseControl);
    const gweni::Rect r=baseControl->getRenderBounds();
    const int sz=((r.w < r.h)?r.w:r.h)*6/10;
    const gweni::Rect icon{r.x+(r.w-sz)/2, r.y+(r.h-sz)/2, sz, sz};

    renderer->drawIcon(m_primitives[0], icon, 1, chevron->isExpanded()?0:3, chevron->getColor(),
        baseControl->getZIndex());
}

///////////////////////////////////////////////////////////////////////////
//CodeGutter
void CodeGutter::update(renderer::Base *renderer, controls::Base *baseControl)
{
    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, tokyonight::CodeGutter);
}

///////////////////////////////////////////////////////////////////////////
//CodeLine
// The line background is drawn only on the current line, and the selection
// only where the line has one: an idle row's slots are hidden, not drawn
// transparent, which would still write depth over the field behind them.
void CodeLine::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.size() < 2)
        return;

    controls::CodeLine *line=static_cast<controls::CodeLine *>(baseControl);
    const tokyonight::Surface &current=tokyonight::CodeCurrentLine;

    if(line->visible() && line->isCurrentLine())
        renderer->drawRoundedRect(m_primitives[0], line->getRenderBounds(), current.radius, current.borderWidth,
            current.top, current.bottom, current.border, line->getZIndex());
    else
        renderer->hidePrimitive(m_primitives[0]);

    const tokyonight::Surface &selection=tokyonight::CodeSelection;

    m_rects.clear();
    if(line->visible())
        line->getSelectionRects(m_rects);

    // a slot per rect, two at least: the ones past them given back
    while(m_primitives.size() > std::max<size_t>(2, 1+m_rects.size()))
    {
        renderer->releasePrimitive(m_primitives.back());
        m_primitives.pop_back();
    }
    while(m_primitives.size() < 1+m_rects.size())
    {
        size_t id;

        renderer->generatePrimitive(id);
        m_primitives.push_back(id);
    }

    const Rect &bounds=line->getRenderBounds();

    for(size_t i=0; i < m_rects.size(); ++i)
    {
        const Rect &local=m_rects[i];

        renderer->drawRoundedRect(m_primitives[1+i], Rect(bounds.x+local.x, bounds.y+local.y, local.w, local.h),
            selection.radius, selection.borderWidth, selection.top, selection.bottom, selection.border,
            line->getTextZIndex());
    }
    if(m_rects.empty())
        renderer->hidePrimitive(m_primitives[1]);
}

///////////////////////////////////////////////////////////////////////////
//Selectable
void Selectable::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    controls::Selectable *row=static_cast<controls::Selectable *>(baseControl);
    const tokyonight::Surface *surface=nullptr;

    if(row->isDisabled())
    {
        if(row->isSelected())
            surface=&tokyonight::SelectableSelectedDisabled;
    }
    else if(row->isDepressed())
        surface=&tokyonight::SelectableDown;
    else if(row->isSelected())
        surface=row->isHovered()?&tokyonight::SelectableSelectedHover:&tokyonight::SelectableSelected;
    else if(row->isHovered())
        surface=&tokyonight::SelectableHover;

    // an idle row draws nothing: a transparent rect would still write depth over
    // what lies behind it, so the slot is hidden instead (drawRoundedRect shows it)
    if(surface && baseControl->visible())
        renderer->drawRoundedRect(m_primitives[0], baseControl->getRenderBounds(), surface->radius,
            surface->borderWidth, surface->top, surface->bottom, surface->border, baseControl->getZIndex());
    else
        renderer->hidePrimitive(m_primitives[0]);
}

///////////////////////////////////////////////////////////////////////////
//NavigationRail / NavigationItem
void NavigationRail::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;
    const auto &surface=tokyonight::NavigationRail;
    const bool same=m_surfaceCurrent && m_surface.top == surface.top && m_surface.bottom == surface.bottom
        && m_surface.border == surface.border && m_surface.radius == surface.radius
        && m_surface.borderWidth == surface.borderWidth;
    if(same && !(baseControl->getStateChange()
        &(controls::StateChange_Created|controls::StateChange_Geometry|controls::StateChange_Visibility)))
        return;
    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, surface);
    m_surface=surface;
    m_surfaceCurrent=true;
}

void NavigationItem::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;
    auto *item=static_cast<controls::NavigationItem *>(baseControl);
    const tokyonight::Surface *surface=nullptr;
    const bool focused=item->isNavigationFocused();
    if(item->isDisabled())
    {
        if(item->isSelected())
            surface=&tokyonight::NavItemSelectedDisabled;
    }
    else if(item->isSelected())
        surface=focused?(item->isHovered()?&tokyonight::NavItemSelectedFocusHover:&tokyonight::NavItemSelectedFocus):
            (item->isHovered()?&tokyonight::NavItemSelectedHover:&tokyonight::NavItemSelected);
    else if(item->isDepressed())
        surface=&tokyonight::NavItemDown;
    else if(focused)
        surface=item->isHovered()?&tokyonight::NavItemFocusHover:&tokyonight::NavItemFocus;
    else if(item->isHovered())
        surface=&tokyonight::NavItemHover;

    if(surface && item->visible())
    {
        const uint8_t edges=item->isSelected()?
            renderer::SquareTopLeft|renderer::SquareBottomLeft
                |renderer::NoBorderTop|renderer::NoBorderRight|renderer::NoBorderBottom:0;
        tokyonight::paintSurface(renderer, m_primitives.data(), item, *surface, item->getRenderBounds(), edges);
    }
    else
        renderer->hidePrimitive(m_primitives[0]);
}

///////////////////////////////////////////////////////////////////////////
//SegmentedButton
void SegmentedButton::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.size() < 3)
        return;

    controls::SegmentedButton *segment=static_cast<controls::SegmentedButton *>(baseControl);
    controls::SegmentedButtons *row=segment->getSegmentedButtons();
    const bool disabled=segment->isDisabled();
    const bool selected=segment->isSelected();
    const tokyonight::Surface *surface=&tokyonight::Segment;

    if(disabled)
        surface=selected?&tokyonight::SegmentOnDisabled:&tokyonight::SegmentDisabled;
    else if(segment->isDepressed())
        surface=&tokyonight::SegmentDown;
    else if(selected)
        surface=segment->isHovered()?&tokyonight::SegmentOnHover:&tokyonight::SegmentOn;
    else if(segment->isHovered())
        surface=&tokyonight::SegmentHover;

    const int index=row?row->indexOf(segment):0;
    const int last=row?row->segmentCount()-1:0;
    gweni::Rect rect=baseControl->getRenderBounds();
    uint8_t edges=0;

    // a disabled segment keeps the focus it had, without its look
    const bool focused=segment->isFocussed() && !disabled;
    tokyonight::Surface look=*surface;

    if(focused)
        look.border=tokyonight::SegmentFocus.border;

    // the left neighbour draws the shared column; the segment starts a Joint
    // past it, with no border on its left side
    if(index > 0)
    {
        edges|=renderer::SquareTopLeft|renderer::SquareBottomLeft|renderer::NoBorderLeft;
        rect.x+=controls::SegmentedButtons::Joint;
        rect.w-=controls::SegmentedButtons::Joint;
    }
    if(index < last)
        edges|=renderer::SquareTopRight|renderer::SquareBottomRight;

    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, look, rect, edges);

    // Focused after the first, its left side is the neighbour's border column
    // painted over in the focus colour: a rect of no width at the column's
    // right edge reaching over it by the border's width, so it covers exactly
    // the device pixels the neighbour's border has, at any scale (a 1-unit
    // joint is 1 or 2 px at 1.5, by where it lands, and the border always 2).
    if(baseControl->visible() && focused && (index > 0))
    {
        renderer->drawRoundedRect(m_primitives[2], gweni::Rect(rect.x, rect.y, 0, rect.h), 0.0f, 0.0f,
            look.border, look.border, look.border, baseControl->getZIndex(),
            renderer::SquareTopLeft|renderer::SquareTopRight|renderer::SquareBottomLeft|
            renderer::SquareBottomRight|renderer::NoBorderLeft, look.borderWidth);
        renderer->showPrimitive(4, m_primitives[2]);
    }
    else
        renderer->hidePrimitive(m_primitives[2]);

    if(baseControl->visible() && selected && segment->hasCheckRoom())
    {
        const gweni::Rect &bounds=baseControl->getRenderBounds();
        const gweni::Rect &check=segment->getCheckRect();
        const auto &colors=baseControl->getSkin()->Colors.SegmentedButton;

        renderer->drawIcon(m_primitives[1], gweni::Rect(bounds.x+check.x, bounds.y+check.y, check.w, check.h), 0,
            0, disabled?colors.Disabled:colors.Check, baseControl->getZIndex()+1);
    }
    else
        renderer->hidePrimitive(m_primitives[1]);
}

///////////////////////////////////////////////////////////////////////////
//StepperStep
void StepperStep::generate(renderer::Base *renderer, controls::Base *baseControl)
{
    ControlRegister<StepperStep>::generate(renderer, baseControl);

    if(!m_hasText)
    {
        renderer->generateTextPrimitive(m_textId);
        m_hasText=true;
        m_textShown=true;
    }
}

void StepperStep::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.size() < 2 || !m_hasText)
        return;

    controls::StepperStep *step=static_cast<controls::StepperStep *>(baseControl);
    const controls::StepLook look=step->getLook();
    const bool clickable=step->getMouseInputEnabled() && !step->isDisabled();
    const bool hovered=clickable && (step->isHovered() || step->isDepressed());
    const tokyonight::Surface *surface=&tokyonight::StepperIndicator;

    switch(look)
    {
    case controls::StepLook::Current:
        surface=hovered?&tokyonight::StepperIndicatorCurrentHover:&tokyonight::StepperIndicatorCurrent;
        break;
    case controls::StepLook::Done:
        surface=hovered?&tokyonight::StepperIndicatorDoneHover:&tokyonight::StepperIndicatorDone;
        break;
    case controls::StepLook::Error:
        surface=hovered?&tokyonight::StepperIndicatorErrorHover:&tokyonight::StepperIndicatorError;
        break;
    case controls::StepLook::Disabled:
        surface=&tokyonight::StepperIndicatorDisabled;
        break;
    default:
        surface=hovered?&tokyonight::StepperIndicatorHover:&tokyonight::StepperIndicator;
        break;
    }

    const Rect &bounds=step->getRenderBounds();
    const Rect indicator=step->getIndicatorRect();
    const Rect circle(bounds.x+indicator.x, bounds.y+indicator.y, indicator.w, indicator.h);
    tokyonight::Surface circleSurface=*surface;

    if(clickable && step->isFocussed())
    {
        circleSurface.border=tokyonight::StepperIndicatorFocus.border;
        circleSurface.borderWidth=tokyonight::StepperIndicatorFocus.borderWidth;
    }
    circleSurface.radius=std::min(circleSurface.radius, 0.5f*float(std::min(circle.w, circle.h)));
    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, circleSurface, circle);

    const bool visible=baseControl->visible();
    // a disabled step's number muted as its title is
    const gweni::Color &color=(look == controls::StepLook::Disabled)?
        step->getSkin()->Colors.Stepper.TitleDisabled:step->getSkin()->Colors.Stepper.Number;
    const int z=baseControl->getZIndex()+1;

    if(visible && (look == controls::StepLook::Done))
    {
        // the check in the middle half of the circle
        const int inset=circle.w/4;

        renderer->drawIcon(m_primitives[1], Rect(circle.x+inset, circle.y+inset, circle.w-2*inset,
            circle.h-2*inset), 0, 0, color, z);
    }
    else
        renderer->hidePrimitive(m_primitives[1]);

    const bool textShown=visible && (look != controls::StepLook::Done);

    if(textShown)
    {
        renderer->setDrawColor(color);
        renderer->renderText(m_textId, step->getFont(), step->indicatorTextOrigin(), z, step->getIndicatorText());
    }
    const unsigned int stateChange=baseControl->getStateChange();

    if((textShown != m_textShown) || (stateChange & controls::StateChange_Visibility)
        || (stateChange & controls::StateChange_Created))
    {
        if(textShown)
            renderer->showText(m_textId);
        else
            renderer->hideText(m_textId);
        m_textShown=textShown;
    }
}

void StepperStep::remove(renderer::Base *renderer, controls::Base *baseControl)
{
    ControlRegister<StepperStep>::remove(renderer, baseControl);

    if(m_hasText)
    {
        renderer->releaseTextPrimitive(m_textId);
        m_hasText=false;
    }
}

///////////////////////////////////////////////////////////////////////////
//StepperConnector
void StepperConnector::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    controls::StepperConnector *connector=static_cast<controls::StepperConnector *>(baseControl);
    const Rect &bounds=connector->getRenderBounds();
    const tokyonight::Surface &surface=connector->isDone()?tokyonight::StepperConnectorDone:
        tokyonight::StepperConnector;

    if(connector->visible() && (bounds.w > 0) && (bounds.h > 0))
        renderer->drawIcon(m_primitives[0], bounds, 6, connector->isVertical()?1:0, surface.top,
            connector->getZIndex());
    else
        renderer->hidePrimitive(m_primitives[0]);
}

///////////////////////////////////////////////////////////////////////////
//ChartLegendRow
void ChartLegendRow::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.size() < 2)
        return;

    controls::internal::ChartLegendRow *row=static_cast<controls::internal::ChartLegendRow *>(baseControl);
    const tokyonight::Surface *surface=nullptr;

    if(!row->isDisabled())
    {
        if(row->isDepressed())
            surface=&tokyonight::SelectableDown;
        else if(row->isHovered())
            surface=&tokyonight::SelectableHover;
    }

    if(!baseControl->visible())
    {
        renderer->hidePrimitive(m_primitives[0]);
        renderer->hidePrimitive(m_primitives[1]);
        return;
    }

    const gweni::Rect &bounds=baseControl->getRenderBounds();
    const int z=baseControl->getZIndex();

    // an idle row draws nothing: its slot is hidden, as a Selectable's
    if(surface)
        renderer->drawRoundedRect(m_primitives[0], bounds, surface->radius, surface->borderWidth, surface->top,
            surface->bottom, surface->border, z);
    else
        renderer->hidePrimitive(m_primitives[0]);

    tokyonight::Surface swatch=tokyonight::chartSeries(row->getColorIndex());

    if(!row->isSeriesShown())
    {
        swatch.top.a=static_cast<unsigned char>(swatch.top.a/4);
        swatch.bottom.a=static_cast<unsigned char>(swatch.bottom.a/4);
        swatch.border.a=static_cast<unsigned char>(swatch.border.a/4);
    }

    gweni::Rect rect=row->getSwatchRect();

    rect.x+=bounds.x;
    rect.y+=bounds.y;
    renderer->drawRoundedRect(m_primitives[1], rect, std::min(swatch.radius, 2.0f), swatch.borderWidth,
        swatch.top, swatch.bottom, swatch.border, z+1);
}

///////////////////////////////////////////////////////////////////////////
//TreeNodeText
void TreeNodeText::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    // the label is a private Button subclass in treeNode.cpp
    controls::Button *label=static_cast<controls::Button *>(baseControl);
    const tokyonight::Surface *surface=nullptr;

    // a press selects at once (TreeNode::onClickName), so there is no pressed look
    if(!label->isDisabled())
    {
        if(label->getToggleState())
            surface=&tokyonight::ListItemSel;
        else if(label->isHovered())
            surface=&tokyonight::ListItemHover;
    }

    // at rest the slot is hidden, as a Selectable's: a transparent rect would
    // still write depth over the tree's lines and the band behind it
    if(surface && baseControl->visible())
        renderer->drawRoundedRect(m_primitives[0], baseControl->getRenderBounds(), surface->radius,
            surface->borderWidth, surface->top, surface->bottom, surface->border, baseControl->getZIndex());
    else
        renderer->hidePrimitive(m_primitives[0]);
}

///////////////////////////////////////////////////////////////////////////
//PropertyRow
void PropertyRow::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.size() < 3)
        return;

    controls::PropertyRow *row=dynamic_cast<controls::PropertyRow *>(baseControl);
    const gweni::Rect bounds=baseControl->getRenderBounds();

    if(!row || !baseControl->visible() || bounds.w < 2 || bounds.h < 2)
    {
        for(size_t primitive : m_primitives)
            renderer->hidePrimitive(primitive);
        return;
    }

    const auto &colors=baseControl->getSkin()->Colors.Properties;
    const tokyonight::Surface *surface=&tokyonight::PropertyColumn;
    gweni::Color line=colors.Line_Normal;

    if(row->isEditing())
    {
        surface=&tokyonight::PropertyColumnEditing;
        line=colors.Line_Selected;
    }
    else if(row->isHovered())
    {
        surface=&tokyonight::PropertyColumnHover;
        line=colors.Line_Hover;
    }

    // the label is placed in the row; its bounds are in the row's
    const gweni::Rect label=row->getLabel()->getBounds();
    const int separatorX=std::min(bounds.x+label.x+label.w, bounds.x+bounds.w)-1;
    const int fillWidth=separatorX-bounds.x;
    const int z=baseControl->getZIndex();

    if(fillWidth > 0)
        renderer->drawRoundedRect(m_primitives[0], gweni::Rect(bounds.x, bounds.y, fillWidth, bounds.h-1),
            surface->radius, surface->borderWidth, surface->top, surface->bottom, surface->border, z);
    else
        renderer->hidePrimitive(m_primitives[0]);

    // as a Separator's rule: each quad reaches one unit past its line on both
    // sides across it, and the stroke is drawn through its middle
    if(separatorX >= bounds.x)
        renderer->drawIcon(m_primitives[1], gweni::Rect(separatorX-1, bounds.y, 3, bounds.h), 6, 1, line, z+1);
    else
        renderer->hidePrimitive(m_primitives[1]);

    renderer->drawIcon(m_primitives[2], gweni::Rect(bounds.x, bounds.y+bounds.h-2, bounds.w, 3), 6, 0, line, z+1);
}

///////////////////////////////////////////////////////////////////////////
//DataTable
void DataTable::update(renderer::Base *renderer, controls::Base *baseControl)
{
    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, tokyonight::PanelDark);
}

///////////////////////////////////////////////////////////////////////////
//DataTableHeader
void DataTableHeader::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.size() < 2)
        return;

    const gweni::Rect bounds=baseControl->getRenderBounds();

    if(!baseControl->visible() || bounds.w <= 0 || bounds.h <= 0)
    {
        for(size_t primitive : m_primitives)
            renderer->hidePrimitive(primitive);
        return;
    }

    const tokyonight::Surface &surface=tokyonight::DataTableHeader;
    const int z=baseControl->getZIndex();

    renderer->drawRoundedRect(m_primitives[0], bounds, surface.radius, surface.borderWidth,
        surface.top, surface.bottom, surface.border, z);
    // as a Separator's rule: the quad reaches one unit past the line on both
    // sides across it, and the stroke is drawn through its middle
    renderer->drawIcon(m_primitives[1], gweni::Rect(bounds.x, bounds.y+bounds.h-2, bounds.w, 3), 6, 0,
        tokyonight::Separator.top, z+1);
}

///////////////////////////////////////////////////////////////////////////
//DataTableHeaderCell
void DataTableHeaderCell::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.size() < 2)
        return;

    controls::internal::DataTableHeaderCell *cell=
        static_cast<controls::internal::DataTableHeaderCell *>(baseControl);
    const gweni::Rect r=baseControl->getRenderBounds();
    const bool visible=baseControl->visible() && r.w > 0 && r.h > 0;
    const tokyonight::Surface *surface=nullptr;

    if(!cell->isDisabled())
    {
        if(cell->isDepressed())
            surface=&tokyonight::DataTableHeaderDown;
        else if(cell->isHovered())
            surface=&tokyonight::DataTableHeaderHover;
    }

    // at rest the header's fill shows: a hidden slot, not a transparent rect
    // that would write depth over it
    if(visible && surface)
        renderer->drawRoundedRect(m_primitives[0], r, surface->radius, surface->borderWidth,
            surface->top, surface->bottom, surface->border, baseControl->getZIndex());
    else
        renderer->hidePrimitive(m_primitives[0]);

    const controls::SortDirection sort=cell->getSortDirection();

    if(visible && sort != controls::SortDirection::None)
    {
        // kind 1's directions: 0 down, 1 up
        const int size=controls::internal::DataTableHeaderCell::ChevronSize;
        const int inset=controls::internal::DataTableHeaderCell::ChevronInset;
        const gweni::Rect chevron{ r.x+r.w-size-inset, r.y+(r.h-size)/2, size, size };

        renderer->drawIcon(m_primitives[1], chevron, 1, sort == controls::SortDirection::Ascending?1:0,
            tokyonight::FgDark, baseControl->getZIndex()+1);
    }
    else
        renderer->hidePrimitive(m_primitives[1]);
}

///////////////////////////////////////////////////////////////////////////
//DataTableColumnGrip
void DataTableColumnGrip::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    controls::internal::DataTableColumnGrip *grip=
        static_cast<controls::internal::DataTableColumnGrip *>(baseControl);
    const gweni::Rect bounds=baseControl->getRenderBounds();
    // the rule a few units in from the cell's top and bottom
    const int inset=std::min(4, bounds.h/4);

    if(!baseControl->visible() || bounds.w <= 0 || bounds.h-2*inset <= 0)
    {
        renderer->hidePrimitive(m_primitives[0]);
        return;
    }

    const gweni::Color color=(grip->isDragging() || grip->isHovered())?
        tokyonight::Blue:tokyonight::Separator.top;
    const int x=bounds.x+bounds.w-1;

    // the stroke through the middle of a quad one unit past the line on both
    // sides; the cell clips the unit past its right edge
    renderer->drawIcon(m_primitives[0], gweni::Rect(x-1, bounds.y+inset, 3, bounds.h-2*inset), 6, 1,
        color, baseControl->getZIndex());
}

///////////////////////////////////////////////////////////////////////////
//DataTableRow
void DataTableRow::resizePrimitives(renderer::Base *renderer, size_t count)
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

void DataTableRow::update(renderer::Base *renderer, controls::Base *baseControl)
{
    controls::DataTableRow *row=static_cast<controls::DataTableRow *>(baseControl);
    const controls::DataTable *table=row->getTable();
    const bool columnRules=table && table->hasColumnBorders();
    const int cells=row->getCellCount();
    const size_t rules=(columnRules && cells > 1)?size_t(cells-1):0;

    resizePrimitives(renderer, 2+rules);

    const gweni::Rect bounds=baseControl->getRenderBounds();

    if(!baseControl->visible() || bounds.w <= 0 || bounds.h <= 0)
    {
        for(size_t primitive : m_primitives)
            renderer->hidePrimitive(primitive);
        return;
    }

    const tokyonight::Surface *surface=nullptr;

    if(row->isDisabled())
    {
        if(row->isSelected())
            surface=&tokyonight::SelectableSelectedDisabled;
    }
    else if(row->isDepressed())
        surface=&tokyonight::SelectableDown;
    else if(row->isSelected())
        surface=row->isHovered()?&tokyonight::SelectableSelectedHover:&tokyonight::SelectableSelected;
    else if(row->isHovered())
        surface=&tokyonight::SelectableHover;

    if(!surface && row->isStriped())
        surface=&tokyonight::DataTableStripe;

    const int z=baseControl->getZIndex();

    // an idle row that is not striped draws nothing: the slot is hidden, not
    // drawn transparent (it would write depth over the backdrop)
    if(surface)
        renderer->drawRoundedRect(m_primitives[0], bounds, surface->radius, surface->borderWidth,
            surface->top, surface->bottom, surface->border, z);
    else
        renderer->hidePrimitive(m_primitives[0]);

    const gweni::Color line=tokyonight::Separator.top;

    // as a Separator's rule: each quad reaches one unit past its line on both
    // sides across it, and the stroke is drawn through its middle
    if(table && table->hasRowBorders())
        renderer->drawIcon(m_primitives[1], gweni::Rect(bounds.x, bounds.y+bounds.h-2, bounds.w, 3), 6, 0,
            line, z+1);
    else
        renderer->hidePrimitive(m_primitives[1]);

    for(size_t i=0; i < rules; ++i)
    {
        // the cells are placed in the row; their bounds are in its
        const gweni::Rect cell=row->getCell(int(i))->getBounds();
        const int x=bounds.x+cell.x+cell.w-1;

        renderer->drawIcon(m_primitives[2+i], gweni::Rect(x-1, bounds.y, 3, bounds.h), 6, 1, line, z+1);
    }
}

///////////////////////////////////////////////////////////////////////////
//TreeTableExpander
void TreeTableExpander::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.size() < 2)
        return;

    controls::internal::TreeTableExpander *expander=
        static_cast<controls::internal::TreeTableExpander *>(baseControl);
    const auto &colors=baseControl->getSkin()->Colors.TreeTable;
    const gweni::Rect r=baseControl->getRenderBounds();
    const bool visible=baseControl->visible() && r.w > 0 && r.h > 0;
    const tokyonight::Surface *surface=nullptr;
    gweni::Color glyph=colors.Expander;

    if(expander->isDisabled())
        glyph=colors.ExpanderDisabled;
    else if(expander->isDepressed())
    {
        surface=&tokyonight::TreeTableExpanderDown;
        glyph=colors.ExpanderHover;
    }
    else if(expander->isHovered())
    {
        surface=&tokyonight::TreeTableExpanderHover;
        glyph=colors.ExpanderHover;
    }

    // at rest there is no surface: a hidden slot, not a transparent rect
    // that would write depth over the row's fill
    if(visible && surface)
        tokyonight::paintSurface(renderer, &m_primitives[0], baseControl, *surface);
    else
        renderer->hidePrimitive(m_primitives[0]);

    if(visible)
    {
        // kind 1's directions: 0 down, 3 right
        const int sz=std::min(r.w, r.h)*6/10;
        const gweni::Rect chevron{ r.x+(r.w-sz)/2, r.y+(r.h-sz)/2, sz, sz };

        renderer->drawIcon(m_primitives[1], chevron, 1, expander->isExpanded()?0:3, glyph,
            baseControl->getZIndex()+1);
    }
    else
        renderer->hidePrimitive(m_primitives[1]);
}

///////////////////////////////////////////////////////////////////////////
//Separator
void Separator::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.size() < 2)
        return;

    controls::Separator *separator=static_cast<controls::Separator *>(baseControl);
    const gweni::Rect bounds=baseControl->getRenderBounds();
    const bool horizontal=separator->isHorizontal();
    gweni::Rect parts[2];
    size_t count=1;

    // one unit past the separator across the rule, on both sides: the stroke is
    // centred in the quad, which then always has room for it
    if(horizontal)
    {
        const int y=bounds.y-1;
        const int h=bounds.h+2;

        if(separator->captionShown())
        {
            // the caption is placed in the separator; the bounds are in its parent
            gweni::Rect caption=separator->getCaption()->getBounds();

            caption.x+=bounds.x;
            const int leftEnd=caption.x-separator->captionGap();
            const int rightStart=caption.x+caption.w+separator->captionGap();

            parts[0]=gweni::Rect(bounds.x, y, leftEnd-bounds.x, h);
            parts[1]=gweni::Rect(rightStart, y, bounds.x+bounds.w-rightStart, h);
            count=2;
        }
        else
            parts[0]=gweni::Rect(bounds.x, y, bounds.w, h);
    }
    else
        parts[0]=gweni::Rect(bounds.x-1, bounds.y, bounds.w+2, bounds.h);

    const bool visible=baseControl->visible();

    for(size_t i=0; i < 2; ++i)
    {
        if(visible && i < count && parts[i].w > 0 && parts[i].h > 0)
            renderer->drawIcon(m_primitives[i], parts[i], 6, horizontal?0:1,
                tokyonight::Separator.top, baseControl->getZIndex());
        else
            renderer->hidePrimitive(m_primitives[i]);
    }
}


///////////////////////////////////////////////////////////////////////////
//InspectorHighlight
void InspectorHighlight::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if((m_primitives.size() < 4) || (baseControl->getStateChange() == controls::StateChange_Nothing))
        return;

    if(!baseControl->visible())
    {
        for(size_t id : m_primitives)
            renderer->hidePrimitive(id);
        return;
    }

    const gweni::Rect &r=baseControl->getRenderBounds();
    const int e=tools::InspectorHighlight::Edge;
    const int z=baseControl->getZIndex();

    renderer->setDrawColor(tokyonight::Orange);
    renderer->drawFilledRect(m_primitives[0], gweni::Rect(r.x, r.y, r.w, e), z);
    renderer->drawFilledRect(m_primitives[1], gweni::Rect(r.x, r.y+r.h-e, r.w, e), z);
    renderer->drawFilledRect(m_primitives[2], gweni::Rect(r.x, r.y+e, e, r.h-2*e), z);
    renderer->drawFilledRect(m_primitives[3], gweni::Rect(r.x+r.w-e, r.y+e, e, r.h-2*e), z);
}

///////////////////////////////////////////////////////////////////////////
//StatsOverlay
void StatsOverlay::generate(renderer::Base *renderer, controls::Base *baseControl)
{
    generatePrimitives(renderer, requiredPrimitives());
}

void StatsOverlay::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, tokyonight::StatsOverlay);
}

void StatsOverlay::remove(renderer::Base *renderer, controls::Base *baseControl)
{
    releasePrimitives(renderer);
}

//Bullet
void Bullet::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    controls::Bullet *bullet=static_cast<controls::Bullet *>(baseControl);
    const bool visible=baseControl->visible();

    if(visible)
    {
        // bulletRect() is in the bullet; its render bounds are in its parent
        const gweni::Rect &bounds=baseControl->getRenderBounds();
        gweni::Rect circle=bullet->bulletRect();

        circle.x+=bounds.x;
        circle.y+=bounds.y;
        const gweni::Color color=bullet->bulletColor();

        renderer->drawRoundedRect(m_primitives[0], circle, circle.w*0.5f, 0.0f,
            color, color, color, baseControl->getZIndex());
    }

    const unsigned int stateChange=baseControl->getStateChange();

    if((stateChange&controls::StateChange_Visibility) || (stateChange&controls::StateChange_Created))
        renderer->showPrimitive(visible?4:0, m_primitives[0]);
}


///////////////////////////////////////////////////////////////////////////
//RadioButton
size_t RadioButton::ActiveNormal;
size_t RadioButton::ActiveChecked;

BorderedPrimitive RadioButton::NormalPrimitive;

void RadioButton::staticInit(TexturedSkin *skin)
{
    ActiveNormal=skin->getTextureEntryId("RadioButton.Active.Normal");
    ActiveChecked=skin->getTextureEntryId("RadioButton.Active.Checked");

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

    tokyonight::Surface ring=checkSurface(control);
    ring.radius=1000.0f;
    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, ring);

    // Inner blue dot when selected (a small centered circle), muted when
    // disabled.
    const gweni::Rect r=baseControl->getRenderBounds();
    if(baseControl->visible() && control->isChecked())
    {
        const int dw=r.w*4/10, dh=r.h*4/10;
        const gweni::Rect dot{ r.x+(r.w-dw)/2, r.y+(r.h-dh)/2, dw, dh };
        if(control->isDisabled())
            renderer->drawRoundedRect(m_primitives[1], dot, 1000.0f, 0.0f,
                tokyonight::Muted, tokyonight::Muted, tokyonight::Muted, baseControl->getZIndex()+1);
        else
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
void ScrollBarButton::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.size() < 2)
        return;

    controls::internal::ScrollBarButton *control=static_cast<controls::internal::ScrollBarButton *>(baseControl);
    const tokyonight::Surface *surface=nullptr;
    gweni::Color glyph=tokyonight::FgDark;

    if(control->isDisabled())
        glyph=tokyonight::Muted;
    else if(control->isDepressed())
        surface=&tokyonight::ScrollThumbDown;
    else if(control->isHovered())
        surface=&tokyonight::ScrollThumbHover;

    const bool visible=baseControl->visible();

    // at rest there is no surface: a hidden slot, not a transparent rect that
    // would write depth over the track behind it
    if(visible && surface)
        tokyonight::paintSurface(renderer, &m_primitives[0], baseControl, *surface);
    else
        renderer->hidePrimitive(m_primitives[0]);

    if(visible)
    {
        // kind 1's directions: 0 down, 1 up, 2 left, 3 right
        const Alignment direction=control->getDirection();
        int dir=0;
        if(direction == Alignment::Left)       dir=2;
        else if(direction == Alignment::Right) dir=3;
        else if(direction == Alignment::Top)   dir=1;

        const gweni::Rect r=baseControl->getRenderBounds();
        const int sz=((r.w<r.h)?r.w:r.h)*6/10;
        const gweni::Rect arrow{ r.x+(r.w-sz)/2, r.y+(r.h-sz)/2, sz, sz };

        renderer->drawIcon(m_primitives[1], arrow, 1, dir, glyph, baseControl->getZIndex()+1);
    }
    else
        renderer->hidePrimitive(m_primitives[1]);
}

///////////////////////////////////////////////////////////////////////////
//BaseScrollBar
void BaseScrollBar::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, tokyonight::ScrollTrack);
}

///////////////////////////////////////////////////////////////////////////
//ScrollBarBar
void ScrollBarBar::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    controls::internal::ScrollBarBar *bar=static_cast<controls::internal::ScrollBarBar *>(baseControl);
    const tokyonight::Surface *surface=&tokyonight::ScrollThumb;

    if(bar->isDepressed())
        surface=&tokyonight::ScrollThumbDown;
    else if(bar->isHovered())
        surface=&tokyonight::ScrollThumbHover;

    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, *surface);
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

    // The track runs between the thumb's centre at either end of its travel,
    // in the slider's track column (all of it but a vertical value text's).
    const gweni::Rect r=slider->getTrackBounds();
    const gweni::Rect bar=slider->getSliderBar()->getBounds();
    const bool horizontal=slider->isHorizontal();
    // With a value text a horizontal track is a field the thumb's height and
    // the slider's length, the text centred in it (imgui's SliderFloat).
    const bool field=horizontal && slider->getValueTextControl();
    const int tickLength=4;
    gweni::Rect track;

    if(field)
    {
        int h=std::min(r.h, ThumbThickness);

        // the ticks go below the field, never at its depth inside it
        if(ticks > 0)
            h=std::min(h, r.h-2*(tickLength+1));

        h=std::max(h, 0);
        track=gweni::Rect(r.x, r.y+(r.h-h)/2, r.w, h);
    }
    else if(horizontal)
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
    int offset=std::min(std::min(crossSize, ThumbThickness)/2+2, crossSize/2-tickLength);

    offset=std::max(offset, TrackThickness/2+1);

    // In a field they run along the thumb's travel, its centre at either end.
    const int tickStart=field?r.x+bar.w/2:(horizontal?track.x:track.y);
    const int tickSpan=field?std::max(r.w-bar.w, 0):(horizontal?track.w:track.h);

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
            const int x=tickStart+int(i)*tickSpan/notches;
            const int y=field?track.y+track.h+1:r.y+r.h/2+offset;

            tick=gweni::Rect(x-1, y, 2, tickLength);
        }
        else
        {
            const int y=tickStart+int(i)*tickSpan/notches;
            tick=gweni::Rect(r.x+r.w/2+offset, y-1, tickLength, 2);
        }

        renderer->drawRoundedRect(id, tick, notch.radius, notch.borderWidth,
            notch.top, notch.bottom, notch.border, baseControl->getZIndex());
    }
}


///////////////////////////////////////////////////////////////////////////
//PlotHistogram
// A plot's slot list, grown or shrunk to \p count: the slots past it given
// back, new ones taken (shared by PlotHistogram and PlotLines).
static void resizePlotPrimitives(renderer::Base *renderer, std::vector<size_t> &primitives, size_t count)
{
    while(primitives.size() > count)
    {
        renderer->releasePrimitive(primitives.back());
        primitives.pop_back();
    }

    while(primitives.size() < count)
    {
        size_t id;

        renderer->generatePrimitive(id);
        primitives.push_back(id);
    }
}

void PlotHistogram::resizePrimitives(renderer::Base *renderer, size_t count)
{
    resizePlotPrimitives(renderer, m_primitives, count);

    // a slot new to the list holds nothing known
    m_drawn.resize(count);
}

static bool sameSurface(const tokyonight::Surface &a, const tokyonight::Surface &b)
{
    return (a.top == b.top) && (a.bottom == b.bottom) && (a.border == b.border)
        && (a.radius == b.radius) && (a.borderWidth == b.borderWidth);
}

void PlotHistogram::update(renderer::Base *renderer, controls::Base *baseControl)
{
    controls::PlotHistogram *plot=static_cast<controls::PlotHistogram *>(baseControl);
    const size_t bars=plot->getValueCount();

    resizePrimitives(renderer, 1+bars);

    // A new rect, z, clip region or visibility reaches every slot: such a
    // frame marks the control's geometry (a move, a resize, a renumber or an
    // ancestor's bounds), its visibility or its creation. Any other draws only
    // what differs from what the slot holds.
    const unsigned int sc=baseControl->getStateChange();
    const bool all=(sc&(controls::StateChange_Created|controls::StateChange_Visibility
        |controls::StateChange_Geometry)) != 0;

    if(!baseControl->visible())
    {
        if(all)
        {
            for(size_t i=0; i<m_primitives.size(); ++i)
            {
                renderer->hidePrimitive(m_primitives[i]);
                m_drawn[i].valid=true;
                m_drawn[i].shown=false;
            }
        }
        else
        {
            // drawn again once shown, which marks the visibility
            for(Drawn &drawn : m_drawn)
                drawn.valid=false;
        }
        return;
    }

    const int z=baseControl->getZIndex();
    // a slot is drawn again unless it holds this already
    auto draw=[&](size_t i, const gweni::Rect &rect, const tokyonight::Surface &surface, int zIndex)
    {
        Drawn &drawn=m_drawn[i];

        if(!all && drawn.valid && drawn.shown && (drawn.rect == rect) && (drawn.zIndex == zIndex)
            && sameSurface(drawn.surface, surface))
            return;

        renderer->drawRoundedRect(m_primitives[i], rect, surface.radius, surface.borderWidth,
            surface.top, surface.bottom, surface.border, zIndex);
        drawn.valid=true;
        drawn.shown=true;
        drawn.rect=rect;
        drawn.surface=surface;
        drawn.zIndex=zIndex;
    };

    draw(0, baseControl->getRenderBounds(), tokyonight::PlotFrame, z);

    const int hovered=plot->getHoveredBar();
    // the bars are in the plot's own coordinates, drawn at its render origin
    const gweni::Rect &origin=baseControl->getRenderBounds();

    for(size_t i=0; i<bars; ++i)
    {
        gweni::Rect rect=plot->getBarRect(i);

        rect.x+=origin.x;
        rect.y+=origin.y;

        if((rect.w <= 0) || (rect.h <= 0))
        {
            Drawn &drawn=m_drawn[1+i];

            if(all || !drawn.valid || drawn.shown)
                renderer->hidePrimitive(m_primitives[1+i]);
            drawn.valid=true;
            drawn.shown=false;
            continue;
        }

        draw(1+i, rect, (int(i) == hovered)?tokyonight::PlotBarHover:tokyonight::PlotBar, z+1);
    }
}


///////////////////////////////////////////////////////////////////////////
//PlotLines
void PlotLines::update(renderer::Base *renderer, controls::Base *baseControl)
{
    controls::PlotLines *plot=static_cast<controls::PlotLines *>(baseControl);
    const size_t segments=plot->getSegmentCount();

    resizePlotPrimitives(renderer, m_primitives, 1+segments);
    // a slot new to the list holds nothing known
    m_segments.resize(segments);

    // As PlotHistogram's: a new rect, z, clip region or visibility reaches
    // every slot, and such a frame marks the control's geometry, its
    // visibility or its creation. Any other draws only what differs.
    const unsigned int sc=baseControl->getStateChange();
    const bool all=(sc&(controls::StateChange_Created|controls::StateChange_Visibility
        |controls::StateChange_Geometry)) != 0;

    if(!baseControl->visible())
    {
        if(all)
        {
            for(size_t id : m_primitives)
                renderer->hidePrimitive(id);
            m_frame.valid=true;
            m_frame.shown=false;
            for(Segment &segment : m_segments)
            {
                segment.valid=true;
                segment.shown=false;
            }
        }
        else
        {
            // drawn again once shown, which marks the visibility
            m_frame.valid=false;
            for(Segment &segment : m_segments)
                segment.valid=false;
        }
        return;
    }

    const int z=baseControl->getZIndex();
    const gweni::Rect &bounds=baseControl->getRenderBounds();
    const tokyonight::Surface &frame=tokyonight::PlotFrame;

    if(all || !m_frame.valid || !m_frame.shown || !(m_frame.rect == bounds) || (m_frame.zIndex != z)
        || !sameSurface(m_frame.surface, frame))
    {
        renderer->drawRoundedRect(m_primitives[0], bounds, frame.radius, frame.borderWidth,
            frame.top, frame.bottom, frame.border, z);
        m_frame.valid=true;
        m_frame.shown=true;
        m_frame.rect=bounds;
        m_frame.surface=frame;
        m_frame.zIndex=z;
    }

    const Color color=tokyonight::PlotLine.top;
    const float thickness=plot->getLineThickness();
    // the points are in the plot's own coordinates, drawn at its render origin
    const PointF origin(float(bounds.x), float(bounds.y));
    bool colorSet=false;

    // the sample points at the render origin, a NaN one a gap
    std::vector<PointF> points(segments > 0 ? segments+1 : 0);
    std::vector<char> shown(points.size());

    for(size_t i=0; i<points.size(); ++i)
    {
        const PointF point=plot->getSamplePoint(i);

        points[i]=PointF(origin.x+point.x, origin.y+point.y);
        shown[i]=!std::isnan(point.y);
    }

    for(size_t i=0; i<segments; ++i)
    {
        Segment &drawn=m_segments[i];

        if(!shown[i] || !shown[i+1])
        {
            if(all || !drawn.valid || drawn.shown)
                renderer->hidePrimitive(m_primitives[1+i]);
            drawn.valid=true;
            drawn.shown=false;
            continue;
        }

        const PointF &p0=points[i];
        const PointF &p1=points[i+1];
        // the segments before and after it, where they are drawn
        const bool hasPrev=(i > 0) && shown[i-1];
        const bool hasNext=(i+2 < points.size()) && shown[i+2];
        const PointF prev=hasPrev ? points[i-1] : PointF();
        const PointF next=hasNext ? points[i+2] : PointF();

        if(!all && drawn.valid && drawn.shown && (drawn.p0 == p0) && (drawn.p1 == p1)
            && (drawn.hasPrev == hasPrev) && (drawn.prev == prev) && (drawn.hasNext == hasNext)
            && (drawn.next == next) && (drawn.thickness == thickness) && (drawn.color == color)
            && (drawn.zIndex == z+1))
            continue;

        if(!colorSet)
        {
            renderer->setDrawColor(color);
            colorSet=true;
        }
        renderer->drawLine(m_primitives[1+i], p0, p1, thickness, z+1, hasPrev ? &prev : nullptr,
            hasNext ? &next : nullptr);
        drawn.valid=true;
        drawn.shown=true;
        drawn.p0=p0;
        drawn.p1=p1;
        drawn.hasPrev=hasPrev;
        drawn.prev=prev;
        drawn.hasNext=hasNext;
        drawn.next=next;
        drawn.thickness=thickness;
        drawn.color=color;
        drawn.zIndex=z+1;
    }
}


///////////////////////////////////////////////////////////////////////////
//NodeGraph
void NodeGraph::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(baseControl->getStateChange() == controls::StateChange_Nothing)
        return;

    controls::NodeGraph *graph=static_cast<controls::NodeGraph *>(baseControl);
    const unsigned int sc=baseControl->getStateChange();

    if(!baseControl->visible())
    {
        if((sc&controls::StateChange_Visibility) || (sc&controls::StateChange_Created))
        {
            renderer->hidePrimitive(m_primitives[0]);
            renderer->hidePrimitive(m_primitives[1]);
        }
        return;
    }

    const gweni::Rect &bounds=baseControl->getRenderBounds();
    const tokyonight::Surface &backdrop=tokyonight::GraphGrid;
    const int z=baseControl->getZIndex();

    renderer->drawRoundedRect(m_primitives[0], bounds, 0.0f, 0.0f, backdrop.top, backdrop.bottom,
        backdrop.border, z);

    // the graph's origin is drawn at pan() from the graph's, the lines a step apart
    const PointF &pan=graph->pan();

    renderer->drawGrid(m_primitives[1], bounds, PointF(bounds.x+pan.x, bounds.y+pan.y),
        graph->gridStep()*graph->zoom(), controls::NodeGraph::GridMajorEvery, backdrop.border,
        tokyonight::GraphGridMajor.top, z+1);
}


///////////////////////////////////////////////////////////////////////////
//Link
size_t Link::slotsFor(controls::LinkStyle style)
{
    switch(style)
    {
    case controls::LinkStyle::Straight:
        return 1;
    case controls::LinkStyle::Orthogonal:
        return 5;
    case controls::LinkStyle::Bezier:
        break;
    }
    return controls::Link::MaxSegments;
}

void Link::generate(renderer::Base *renderer, controls::Base *baseControl)
{
    const controls::Link *link=static_cast<const controls::Link *>(baseControl);

    generatePrimitives(renderer, slotsFor(link->getStyle()));
    m_segments.assign(m_primitives.size(), Segment());
}

namespace
{
//! A rect in fractional units: a link's view.
struct ViewBox
{
    float x, y, w, h;
};
}

// The part of the segment a-b inside \p box (Liang-Barsky): false if none, else
// the parameters t0 <= t1 of its ends, 0 and 1 where it is not cut.
static bool clipSegment(const PointF &a, const PointF &b, const ViewBox &box, float &t0, float &t1)
{
    const float dx=b.x-a.x;
    const float dy=b.y-a.y;
    const float p[4]={-dx, dx, -dy, dy};
    const float q[4]={a.x-box.x, box.x+box.w-a.x, a.y-box.y, box.y+box.h-a.y};

    t0=0.0f;
    t1=1.0f;
    for(int i=0; i<4; ++i)
    {
        if(p[i] == 0.0f)
        {
            if(q[i] < 0.0f)
                return false;
            continue;
        }

        const float t=q[i]/p[i];

        if(p[i] < 0.0f)
            t0=std::max(t0, t);
        else
            t1=std::min(t1, t);
    }
    return t0 <= t1;
}

void Link::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(baseControl->getStateChange() == controls::StateChange_Nothing)
        return;

    controls::Link *link=static_cast<controls::Link *>(baseControl);
    const size_t slots=slotsFor(link->getStyle());

    resizePlotPrimitives(renderer, m_primitives, slots);
    m_segments.resize(slots);

    const unsigned int sc=baseControl->getStateChange();
    const bool all=(sc&(controls::StateChange_Created|controls::StateChange_Visibility
        |controls::StateChange_Geometry)) != 0;
    auto hide=[&](size_t i)
    {
        Segment &drawn=m_segments[i];

        if(all || !drawn.valid || drawn.shown)
            renderer->hidePrimitive(m_primitives[i]);
        drawn.valid=true;
        drawn.shown=false;
    };

    controls::NodeGraph *graph=link->getGraph();
    std::vector<PointF> points;

    if(baseControl->visible() && graph)
        link->getPoints(points);

    if(points.size() < 2)
    {
        for(size_t i=0; i<slots; ++i)
            hide(i);
        return;
    }

    // graph units to the link's own: the link's top-left in the graph, whose
    // content space the graph's inner panel is placed in
    PointF origin(0.0f, 0.0f);

    for(const controls::Base *control=link; control && (control != graph);
        control=control->getActualParent())
    {
        origin.x+=float(control->getX());
        origin.y+=float(control->getY());
    }

    const gweni::Rect &bounds=baseControl->getRenderBounds();
    const float thickness=link->getThickness();
    const float zoom=graph->zoom();

    // the graph's view in the link's units, wider by the stroke and a few
    // canvas units, so a cut end is off screen
    const PointF viewMin=graph->localToGraph(PointF(0.0f, 0.0f));
    const PointF viewMax=graph->localToGraph(PointF(float(graph->getWidth()), float(graph->getHeight())));
    const float margin=thickness+4.0f/std::max(zoom, controls::ZoomPanel::MinZoomLimit);
    const ViewBox view{viewMin.x-origin.x-margin, viewMin.y-origin.y-margin,
        viewMax.x-viewMin.x+2.0f*margin, viewMax.y-viewMin.y+2.0f*margin};

    for(PointF &point : points)
        point=PointF(point.x-origin.x, point.y-origin.y);

    const size_t segments=std::min(points.size()-1, slots);

    struct Cut
    {
        bool shown=false;
        float t0=0.0f;
        float t1=1.0f;
        PointF p0;
        PointF p1;
    };
    std::vector<Cut> cuts(segments);

    for(size_t i=0; i<segments; ++i)
    {
        Cut &cut=cuts[i];
        const PointF &a=points[i];
        const PointF &b=points[i+1];

        cut.shown=clipSegment(a, b, view, cut.t0, cut.t1);
        // the original points where it is not cut, so joints are exact
        cut.p0=(cut.t0 == 0.0f)?a:PointF(a.x+(b.x-a.x)*cut.t0, a.y+(b.y-a.y)*cut.t0);
        cut.p1=(cut.t1 == 1.0f)?b:PointF(a.x+(b.x-a.x)*cut.t1, a.y+(b.y-a.y)*cut.t1);
    }

    Color color;

    if(link->isHovered())
        color=tokyonight::LinkHover.top;
    else if(link->isSelected())
        color=tokyonight::NodeSelected.border;
    else if(link->hasColor())
        color=link->getColor();
    else if(controls::Pin *from=link->getFrom())
        color=from->hasColor()?from->getColor():((from->getDirection() == controls::PinDirection::Input)?
            tokyonight::PinIn.top:tokyonight::PinOut.top);

    const int z=baseControl->getZIndex();
    const PointF at(float(bounds.x), float(bounds.y));
    bool colorSet=false;

    for(size_t i=0; i<slots; ++i)
    {
        if((i >= segments) || !cuts[i].shown)
        {
            hide(i);
            continue;
        }

        const Cut &cut=cuts[i];
        // a neighbour shares the joint only where neither is cut there
        const bool hasPrev=(i > 0) && cuts[i-1].shown && (cuts[i-1].t1 == 1.0f) && (cut.t0 == 0.0f);
        const bool hasNext=(i+1 < segments) && cuts[i+1].shown && (cuts[i+1].t0 == 0.0f) && (cut.t1 == 1.0f);
        const PointF p0(at.x+cut.p0.x, at.y+cut.p0.y);
        const PointF p1(at.x+cut.p1.x, at.y+cut.p1.y);
        const PointF prev=hasPrev?PointF(at.x+cuts[i-1].p0.x, at.y+cuts[i-1].p0.y):PointF();
        const PointF next=hasNext?PointF(at.x+cuts[i+1].p1.x, at.y+cuts[i+1].p1.y):PointF();
        Segment &drawn=m_segments[i];

        if(!all && drawn.valid && drawn.shown && (drawn.p0 == p0) && (drawn.p1 == p1)
            && (drawn.hasPrev == hasPrev) && (drawn.prev == prev) && (drawn.hasNext == hasNext)
            && (drawn.next == next) && (drawn.thickness == thickness) && (drawn.color == color)
            && (drawn.zIndex == z))
            continue;

        if(!colorSet)
        {
            renderer->setDrawColor(color);
            colorSet=true;
        }
        renderer->drawLine(m_primitives[i], p0, p1, thickness, z, hasPrev?&prev:nullptr,
            hasNext?&next:nullptr);
        drawn.valid=true;
        drawn.shown=true;
        drawn.p0=p0;
        drawn.p1=p1;
        drawn.hasPrev=hasPrev;
        drawn.prev=prev;
        drawn.hasNext=hasNext;
        drawn.next=next;
        drawn.thickness=thickness;
        drawn.color=color;
        drawn.zIndex=z;
    }
}


///////////////////////////////////////////////////////////////////////////
//Timeline
tokyonight::Surface Timeline::barSurface(const controls::TimelineBar &bar)
{
    tokyonight::Surface surface=tokyonight::TimelineBar;

    if(bar.hasColor)
    {
        surface.top=bar.color;
        surface.bottom=bar.color;
        return surface;
    }
    if(bar.surface.empty())
        return surface;

    const std::unordered_map<std::string, tokyonight::Surface *> &surfaces=tokyonight::surfaceRegistry();
    const auto found=surfaces.find(bar.surface);

    if(found != surfaces.end())
        return *found->second;

    // a name no theme knows draws as a missing surface
    surface.top=colors::Missing;
    surface.bottom=colors::Missing;
    surface.border=colors::MissingDark;
    return surface;
}

std::vector<size_t> Timeline::barPrimitives(size_t lane) const
{
    return (lane < m_bars.size()) ? inUse(m_bars[lane]) : std::vector<size_t>();
}

std::vector<size_t> Timeline::inUse(const SlotList &list)
{
    return std::vector<size_t>(list.ids.begin(), list.ids.begin()+std::min(list.used, list.ids.size()));
}

void Timeline::releaseList(renderer::Base *renderer, SlotList &list)
{
    for(size_t id : list.ids)
        renderer->releasePrimitive(id);
    list.ids.clear();
    list.drawn.clear();
    list.used=0;
    list.taken=0;
}

void Timeline::takeLists(renderer::Base *renderer, size_t lanes, size_t grid, const std::vector<size_t> &bars)
{
    releaseList(renderer, m_lanes);
    releaseList(renderer, m_grid);
    for(SlotList &list : m_bars)
        releaseList(renderer, list);
    releaseList(renderer, m_hover);
    m_bars.resize(bars.size());

    // The bank hands out the lowest free id first, so what it gives back
    // goes up from list to list; one at or below the frame's is put aside
    // until every list is taken.
    const size_t frame=m_primitives.empty() ? 0 : m_primitives[0];
    std::vector<size_t> below;
    auto take=[&](SlotList &list, size_t used, size_t count)
    {
        list.taken=used;
        list.ids.resize(count);
        // a slot new to the list holds nothing known
        list.drawn.assign(count, Slot());
        for(size_t &id : list.ids)
        {
            renderer->generatePrimitive(id);
            while(!m_primitives.empty() && (id <= frame))
            {
                below.push_back(id);
                renderer->generatePrimitive(id);
            }
        }
    };

    take(m_lanes, lanes, capacity(lanes));
    take(m_grid, grid, capacity(grid));
    for(size_t i=0; i<bars.size(); ++i)
        take(m_bars[i], bars[i], capacity(bars[i]));
    // the hover's one slot, in front of every bar
    take(m_hover, 1, 1);

    for(size_t id : below)
        renderer->releasePrimitive(id);
}

void Timeline::remove(renderer::Base *renderer, controls::Base * /*baseControl*/)
{
    releaseList(renderer, m_lanes);
    releaseList(renderer, m_grid);
    for(SlotList &list : m_bars)
        releaseList(renderer, list);
    m_bars.clear();
    releaseList(renderer, m_hover);
    m_frame=Slot();
    m_drawnAll=false;
    releasePrimitives(renderer);
}

void Timeline::appendIds(std::vector<size_t> &primitiveIds, std::vector<size_t> &textIds) const
{
    ControlRegister<Timeline>::appendIds(primitiveIds, textIds);
    primitiveIds.insert(primitiveIds.end(), m_lanes.ids.begin(), m_lanes.ids.end());
    primitiveIds.insert(primitiveIds.end(), m_grid.ids.begin(), m_grid.ids.end());
    for(const SlotList &list : m_bars)
        primitiveIds.insert(primitiveIds.end(), list.ids.begin(), list.ids.end());
    primitiveIds.insert(primitiveIds.end(), m_hover.ids.begin(), m_hover.ids.end());
}

void Timeline::update(renderer::Base *renderer, controls::Base *baseControl)
{
    controls::Timeline *timeline=static_cast<controls::Timeline *>(baseControl);
    const unsigned int sc=baseControl->getStateChange();
    // A new rect, z, clip region or visibility reaches every slot: such a
    // frame marks the control's geometry, its visibility or its creation.
    const bool all=(sc&(controls::StateChange_Created|controls::StateChange_Visibility
        |controls::StateChange_Geometry)) != 0;
    const gweni::Rect &bounds=baseControl->getRenderBounds();
    const gweni::Rect area=timeline->getBarArea();
    const gweni::Rect column=timeline->getCaptionColumnBounds();
    const PointF origin(float(bounds.x), float(bounds.y));
    const size_t laneCount=timeline->getLaneCount();
    const std::vector<controls::axis::Tick> &ticks=timeline->getTicks();
    // a line per tick, the top and the left axis lines
    const size_t gridCount=ticks.size()+2;

    // Nothing is taken while there is nothing to draw (hidden, or before a
    // layout gave it a bar area): a timeline built hidden takes its slots in
    // the order one built shown does.
    const bool drawing=baseControl->visible() && (area.w > 0) && (area.h > 0);
    bool retaken=false;

    if(drawing)
    {
        std::vector<size_t> barsNeed(laneCount);

        for(size_t i=0; i<laneCount; ++i)
            barsNeed[i]=timeline->getBarCount(i);

        auto outgrown=[](const SlotList &list, size_t count)
        {
            return (count > list.ids.size()) || (count < list.taken);
        };
        // a lane's bars shrink by half before its list is taken again, so a
        // bar removed now and then rewrites nothing else
        auto barsOutgrown=[](const SlotList &list, size_t count)
        {
            return (count > list.ids.size()) || (count*2 < list.taken);
        };
        bool retake=m_hover.ids.empty() || (m_bars.size() != laneCount)
            || outgrown(m_lanes, laneCount) || outgrown(m_grid, gridCount);

        for(size_t i=0; i<laneCount && !retake; ++i)
            retake=barsOutgrown(m_bars[i], barsNeed[i]);

        if(retake)
        {
            takeLists(renderer, laneCount, gridCount, barsNeed);
            retaken=true;
        }

        m_lanes.used=laneCount;
        m_grid.used=gridCount;
        for(size_t i=0; i<laneCount; ++i)
            m_bars[i].used=barsNeed[i];
        m_hover.used=1;
    }

    auto hide=[&](size_t id, Slot &slot)
    {
        if(all || !slot.valid || slot.shown)
            renderer->hidePrimitive(id);
        slot.valid=true;
        slot.shown=false;
    };
    auto hideSpare=[&](SlotList &list)
    {
        for(size_t i=list.used; i<list.ids.size(); ++i)
            hide(list.ids[i], list.drawn[i]);
    };
    // every slot of the lists, the frame's with \p frame
    auto each=[&](bool frame, auto &&func)
    {
        if(frame)
            func(m_primitives[0], m_frame);
        for(SlotList *list : {&m_lanes, &m_grid, &m_hover})
        {
            for(size_t i=0; i<list->ids.size(); ++i)
                func(list->ids[i], list->drawn[i]);
        }
        for(SlotList &list : m_bars)
        {
            for(size_t i=0; i<list.ids.size(); ++i)
                func(list.ids[i], list.drawn[i]);
        }
    };

    if(!baseControl->visible())
    {
        if(all)
            each(true, hide);
        else
        {
            // drawn again once shown, which marks the visibility
            each(true, [](size_t, Slot &slot) { slot.valid=false; });
        }
        m_drawnAll=false;
        return;
    }

    const gweni::Rect region=renderer->clipRegion();
    const gweni::Point &offset=renderer->getRenderOffset();
    // a rect of the control at the render offset, cut to its own region
    auto clipTo=[&](const gweni::Rect &rect)
    {
        const int x0=std::max(region.x, offset.x+bounds.x+rect.x);
        const int y0=std::max(region.y, offset.y+bounds.y+rect.y);
        const int x1=std::min(region.x+region.w, offset.x+bounds.x+rect.x+rect.w);
        const int y1=std::min(region.y+region.h, offset.y+bounds.y+rect.y+rect.h);

        return gweni::Rect(x0, y0, std::max(0, x1-x0), std::max(0, y1-y0));
    };
    const gweni::Rect areaClip=clipTo(area);
    // the caption column and the bar area: the body's viewport
    const gweni::Rect viewClip=clipTo(gweni::Rect(column.x, area.y, area.x+area.w-column.x, area.h));

    auto drawRect=[&](size_t id, Slot &slot, const gweni::Rect &rect, const tokyonight::Surface &surface,
        int zIndex, const gweni::Rect &clip)
    {
        if(!all && slot.valid && slot.shown && !slot.line && (slot.rect == rect) && (slot.zIndex == zIndex)
            && sameSurface(slot.surface, surface) && (slot.clip == clip))
            return;

        renderer->setClipRegion(clip);
        renderer->drawRoundedRect(id, rect, surface.radius, surface.borderWidth, surface.top, surface.bottom,
            surface.border, zIndex);
        slot=Slot();
        slot.valid=true;
        slot.shown=true;
        slot.rect=rect;
        slot.surface=surface;
        slot.zIndex=zIndex;
        slot.clip=clip;
    };
    auto drawSegment=[&](size_t id, Slot &slot, const PointF &p0, const PointF &p1, const Color &color,
        int zIndex, const gweni::Rect &clip)
    {
        if(!all && slot.valid && slot.shown && slot.line && (slot.p0 == p0) && (slot.p1 == p1)
            && (slot.color == color) && (slot.zIndex == zIndex) && (slot.clip == clip))
            return;

        renderer->setClipRegion(clip);
        renderer->setDrawColor(color);
        renderer->drawLine(id, p0, p1, 1.0f, zIndex);
        slot=Slot();
        slot.valid=true;
        slot.shown=true;
        slot.line=true;
        slot.p0=p0;
        slot.p1=p1;
        slot.color=color;
        slot.zIndex=zIndex;
        slot.clip=clip;
    };

    const int z=baseControl->getZIndex();

    drawRect(m_primitives[0], m_frame, bounds, tokyonight::TimelineFrame, z, region);

    // no bar area (not laid out yet, or too small): the frame alone
    if(!drawing)
    {
        each(false, hide);
        m_drawnAll=false;
        return;
    }

    // The lanes, the grid and the bars move only with what getBarsSerial()
    // counts or with the area's clip, and take new colours only with a theme
    // edit, which marks the text (ThemeEditor::applyToCanvas()): a hover,
    // which marks the background alone, leaves them as they are.
    const bool stale=all || retaken || !m_drawnAll || (timeline->getBarsSerial() != m_drawnSerial)
        || ((sc&controls::StateChange_Text) != 0)
        || !(areaClip == m_drawnAreaClip) || !(viewClip == m_drawnViewClip);

    if(stale)
    {
        for(size_t i=0; i<laneCount; ++i)
        {
            gweni::Rect lane;

            if((i%2 == 0) || !timeline->getLaneRect(i, lane))
            {
                hide(m_lanes.ids[i], m_lanes.drawn[i]);
                continue;
            }

            // across the caption column too
            const gweni::Rect stripe(bounds.x+column.x, bounds.y+lane.y, area.x+area.w-column.x, lane.h);

            drawRect(m_lanes.ids[i], m_lanes.drawn[i], stripe, tokyonight::TimelineLane, z+LaneLayer, viewClip);
        }
        hideSpare(m_lanes);

        const float left=origin.x+float(area.x);
        const float top=origin.y+float(area.y);
        const float right=left+float(area.w);
        const float bottom=top+float(area.h);

        for(size_t i=0; i<ticks.size(); ++i)
        {
            const float x=origin.x+std::min(std::max(float(ticks[i].position), float(area.x)+0.5f),
                float(area.x+area.w)-0.5f);

            drawSegment(m_grid.ids[i], m_grid.drawn[i], PointF(x, top), PointF(x, bottom),
                tokyonight::TimelineGrid.top, z+GridLayer, areaClip);
        }
        drawSegment(m_grid.ids[ticks.size()], m_grid.drawn[ticks.size()],
            PointF(origin.x+float(column.x), top+0.5f), PointF(right, top+0.5f),
            tokyonight::TimelineAxis.top, z+GridLayer, viewClip);
        drawSegment(m_grid.ids[ticks.size()+1], m_grid.drawn[ticks.size()+1],
            PointF(left+0.5f, top), PointF(left+0.5f, bottom), tokyonight::TimelineAxis.top, z+GridLayer,
            areaClip);
        hideSpare(m_grid);

        for(size_t l=0; l<laneCount; ++l)
        {
            SlotList &list=m_bars[l];

            for(size_t i=0; i<list.used; ++i)
            {
                gweni::Rect rect;

                if(!timeline->getBarRect(l, i, rect))
                {
                    hide(list.ids[i], list.drawn[i]);
                    continue;
                }

                rect.x+=bounds.x;
                rect.y+=bounds.y;
                drawRect(list.ids[i], list.drawn[i], rect, barSurface(timeline->getBar(l, i)),
                    z+BarLayer+timeline->getOverlapLevel(l, i), areaClip);
            }
            hideSpare(list);
        }

        m_drawnAll=true;
        m_drawnSerial=timeline->getBarsSerial();
        m_drawnAreaClip=areaClip;
        m_drawnViewClip=viewClip;
    }

    // the hover look over the hovered bar: its fill with the hover's blended
    // over it, as opaque as the bar, and the hover's border
    gweni::Rect hovered;

    if(timeline->getHoverRect(hovered))
    {
        const tokyonight::Surface &tint=tokyonight::TimelineBarHover;
        tokyonight::Surface look=barSurface(timeline->getBar(timeline->getHoverLane(), timeline->getHoverIndex()));
        auto over=[](const Color &under, const Color &top)
        {
            const int a=top.a;
            auto mix=[a](int u, int t) { return static_cast<unsigned char>((u*(255-a)+t*a+127)/255); };

            return Color(mix(under.r, top.r), mix(under.g, top.g), mix(under.b, top.b), under.a);
        };

        look.top=over(look.top, tint.top);
        look.bottom=over(look.bottom, tint.bottom);
        look.border=tint.border;
        look.borderWidth=tint.borderWidth;
        hovered.x+=bounds.x;
        hovered.y+=bounds.y;
        drawRect(m_hover.ids[0], m_hover.drawn[0], hovered, look, z+HoverLayer, areaClip);
    }
    else
        hide(m_hover.ids[0], m_hover.drawn[0]);
}

///////////////////////////////////////////////////////////////////////////
//Chart
size_t Chart::depthLayersOf(const controls::Base *control)
{
    const controls::Chart *chart=gweni_cast<controls::Chart>(const_cast<controls::Base *>(control));

    // the frame, the grid, the spans, a layer per series, one per area's
    // line, the markers and the crosshair
    size_t layers=size_t(SeriesLayer)+2;

    if(chart)
    {
        for(size_t i=0; i<chart->getSeriesCount(); ++i)
            layers+=controls::Chart::isAreaType(chart->getSeriesType(i)) ? 2 : 1;
    }
    return layers;
}

std::vector<size_t> Chart::seriesPrimitives(size_t series) const
{
    return (series < m_series.size()) ? inUse(m_series[series]) : std::vector<size_t>();
}

std::vector<size_t> Chart::areaLinePrimitives(size_t series) const
{
    return (series < m_areaLines.size()) ? inUse(m_areaLines[series]) : std::vector<size_t>();
}

std::vector<size_t> Chart::inUse(const SlotList &list)
{
    return std::vector<size_t>(list.ids.begin(), list.ids.begin()+std::min(list.used, list.ids.size()));
}

void Chart::takeLists(renderer::Base *renderer, size_t grid, size_t spans, const std::vector<size_t> &series,
    const std::vector<size_t> &areaLines, size_t markers)
{
    releaseList(renderer, m_grid);
    releaseList(renderer, m_spans);
    for(SlotList &list : m_series)
        releaseList(renderer, list);
    for(SlotList &list : m_areaLines)
        releaseList(renderer, list);
    releaseList(renderer, m_markers);
    releaseList(renderer, m_crosshair);
    m_series.resize(series.size());
    m_areaLines.resize(areaLines.size());

    // The bank hands out the lowest free id first, so what it gives back
    // goes up from list to list; one at or below the frame's is put aside
    // until every list is taken.
    const size_t frame=m_primitives.empty() ? 0 : m_primitives[0];
    std::vector<size_t> below;
    auto take=[&](SlotList &list, size_t used)
    {
        const size_t count=(&list == &m_crosshair) ? 1 : capacity(used);

        list.taken=used;
        list.ids.resize(count);
        // a slot new to the list holds nothing known
        list.drawn.assign(count, Slot());
        for(size_t &id : list.ids)
        {
            renderer->generatePrimitive(id);
            while(!m_primitives.empty() && (id <= frame))
            {
                below.push_back(id);
                renderer->generatePrimitive(id);
            }
        }
    };

    take(m_grid, grid);
    take(m_spans, spans);
    for(size_t i=0; i<series.size(); ++i)
        take(m_series[i], series[i]);
    // the areas' lines, in front of every series' fill
    for(size_t i=0; i<areaLines.size(); ++i)
        take(m_areaLines[i], areaLines[i]);
    take(m_markers, markers);
    // the crosshair's one slot, in front of everything
    take(m_crosshair, 1);

    for(size_t id : below)
        renderer->releasePrimitive(id);
}

void Chart::releaseList(renderer::Base *renderer, SlotList &list)
{
    for(size_t id : list.ids)
        renderer->releasePrimitive(id);
    list.ids.clear();
    list.drawn.clear();
    list.used=0;
    list.taken=0;
}

void Chart::remove(renderer::Base *renderer, controls::Base * /*baseControl*/)
{
    releaseList(renderer, m_grid);
    releaseList(renderer, m_spans);
    for(SlotList &list : m_series)
        releaseList(renderer, list);
    m_series.clear();
    for(SlotList &list : m_areaLines)
        releaseList(renderer, list);
    m_areaLines.clear();
    releaseList(renderer, m_markers);
    releaseList(renderer, m_crosshair);
    m_frame=Slot();
    releasePrimitives(renderer);
}

void Chart::appendIds(std::vector<size_t> &primitiveIds, std::vector<size_t> &textIds) const
{
    ControlRegister<Chart>::appendIds(primitiveIds, textIds);
    primitiveIds.insert(primitiveIds.end(), m_grid.ids.begin(), m_grid.ids.end());
    primitiveIds.insert(primitiveIds.end(), m_spans.ids.begin(), m_spans.ids.end());
    for(const SlotList &list : m_series)
        primitiveIds.insert(primitiveIds.end(), list.ids.begin(), list.ids.end());
    for(const SlotList &list : m_areaLines)
        primitiveIds.insert(primitiveIds.end(), list.ids.begin(), list.ids.end());
    primitiveIds.insert(primitiveIds.end(), m_markers.ids.begin(), m_markers.ids.end());
    primitiveIds.insert(primitiveIds.end(), m_crosshair.ids.begin(), m_crosshair.ids.end());
}

void Chart::update(renderer::Base *renderer, controls::Base *baseControl)
{
    controls::Chart *chart=static_cast<controls::Chart *>(baseControl);
    const unsigned int sc=baseControl->getStateChange();
    // A new rect, z, clip region or visibility reaches every slot: such a
    // frame marks the control's geometry, its visibility or its creation.
    // Any other draws only what differs from what the slot holds.
    const bool all=(sc&(controls::StateChange_Created|controls::StateChange_Visibility
        |controls::StateChange_Geometry)) != 0;
    const gweni::Rect &bounds=baseControl->getRenderBounds();
    const gweni::Rect plot=chart->getPlotBounds();
    const PointF origin(float(bounds.x), float(bounds.y));
    const float left=origin.x+float(plot.x);
    const float top=origin.y+float(plot.y);
    const float right=left+float(plot.w);
    const float bottom=top+float(plot.h);
    const bool hasPlot=(plot.w > 0) && (plot.h > 0);

    // the grid's lines: each shown value axis's ticks with its grid on, the
    // x ticks with the x grid on, then the line along the plot's bottom,
    // each inside the plot's units
    struct GridLine
    {
        PointF p0;
        PointF p1;
        Color color;
    };
    std::vector<GridLine> lines;

    if(hasPlot)
    {
        const Color grid=tokyonight::ChartGrid.top;

        for(controls::ChartAxisSide side : {controls::ChartAxisSide::Left, controls::ChartAxisSide::Right})
        {
            if(!chart->isYAxisShown(side) || !chart->getYGrid(side))
                continue;
            for(const controls::axis::Tick &tick : chart->getYTicks(side))
            {
                const float y=origin.y+std::min(std::max(float(tick.position), float(plot.y)+0.5f),
                    float(plot.y+plot.h)-0.5f);

                // the axis line is there
                if(y > bottom-1.0f)
                    continue;
                lines.push_back({PointF(left, y), PointF(right, y), grid});
            }
        }
        if(chart->getXGrid())
        {
            for(const controls::axis::Tick &tick : chart->getXTicks())
            {
                const float x=origin.x+std::min(std::max(float(tick.position), float(plot.x)+0.5f),
                    float(plot.x+plot.w)-0.5f);

                lines.push_back({PointF(x, top), PointF(x, bottom), grid});
            }
        }
        lines.push_back({PointF(left, bottom-0.5f), PointF(right, bottom-0.5f), tokyonight::ChartAxis.top});
    }

    // the lists follow the chart: the grid's lines, and per series its bars
    // or its segments
    const size_t seriesCount=chart->getSeriesCount();

    // Taken back to front (grid, spans, series, markers, crosshair), so the
    // lists made on the frame the chart is first drawn draw what is behind
    // first: a translucent span over the grid, a line's anti-aliased rim over
    // both. A slot drawn earlier at a higher z writes depth that a slot
    // behind it, drawn later, cannot pass. So nothing is taken while there is
    // nothing to draw (hidden, or before a layout gave it a plot): a chart
    // built hidden takes its slots in the order one built shown does.
    const bool drawing=baseControl->visible() && hasPlot;

    if(drawing)
    {
        std::vector<size_t> &seriesNeed=m_seriesNeed;
        std::vector<size_t> &lineNeed=m_lineNeed;

        seriesNeed.assign(seriesCount, 0);
        lineNeed.assign(seriesCount, 0);

        for(size_t i=0; i<seriesCount; ++i)
        {
            const controls::ChartSeriesType type=chart->getSeriesType(i);
            const bool bars=(type == controls::ChartSeriesType::Bar) || (type == controls::ChartSeriesType::StackedBar);
            const size_t vertices=chart->getVertexCount(i);
            const size_t segments=vertices > 1 ? vertices-1 : 0;

            // an area's fill in its list, its line in its line's
            if(controls::Chart::isAreaType(type))
            {
                seriesNeed[i]=chart->getAreaSegmentCount(i);
                lineNeed[i]=segments;
            }
            else
                seriesNeed[i]=bars ? chart->getSeriesPoints(i).size() : segments;
        }

        // A new series' list, or one slot more than a list has, would come
        // after the lists in front of it; and a list needing fewer than it
        // was taken for (a series gone, a resize back to fewer grid lines)
        // keeps capacity it no longer needs. Either way every list is taken
        // again, as a chart built with what it draws now would take them, so
        // a round trip gives the same slots back. A list growing a slot at a
        // time, or one losing and gaining one (a sliding window), is taken
        // again only once per half again.
        auto outgrown=[](const SlotList &list, size_t count)
        {
            return (count > list.ids.size()) || (count < list.taken);
        };
        bool retake=m_crosshair.ids.empty() || (m_series.size() != seriesCount)
            || outgrown(m_grid, lines.size()) || outgrown(m_spans, chart->getSpanCount())
            || outgrown(m_markers, chart->getMarkerCount());

        for(size_t i=0; i<seriesCount && !retake; ++i)
            retake=outgrown(m_series[i], seriesNeed[i]) || outgrown(m_areaLines[i], lineNeed[i]);

        if(retake)
            takeLists(renderer, lines.size(), chart->getSpanCount(), seriesNeed, lineNeed, chart->getMarkerCount());

        m_grid.used=lines.size();
        m_spans.used=chart->getSpanCount();
        for(size_t i=0; i<seriesCount; ++i)
        {
            m_series[i].used=seriesNeed[i];
            m_areaLines[i].used=lineNeed[i];
        }
        m_markers.used=chart->getMarkerCount();
        m_crosshair.used=1;
    }

    auto hide=[&](size_t id, Slot &slot)
    {
        if(all || !slot.valid || slot.shown)
            renderer->hidePrimitive(id);
        slot.valid=true;
        slot.shown=false;
    };
    // a list's spare slots, past the ones in use
    auto hideSpare=[&](SlotList &list)
    {
        for(size_t i=list.used; i<list.ids.size(); ++i)
            hide(list.ids[i], list.drawn[i]);
    };

    // every slot of the lists, the frame's with \p frame
    auto each=[&](bool frame, auto &&func)
    {
        if(frame)
            func(m_primitives[0], m_frame);
        for(SlotList *list : {&m_grid, &m_spans, &m_markers, &m_crosshair})
        {
            for(size_t i=0; i<list->ids.size(); ++i)
                func(list->ids[i], list->drawn[i]);
        }
        for(std::vector<SlotList> *lists : {&m_series, &m_areaLines})
        {
            for(SlotList &list : *lists)
            {
                for(size_t i=0; i<list.ids.size(); ++i)
                    func(list.ids[i], list.drawn[i]);
            }
        }
    };

    if(!baseControl->visible())
    {
        if(all)
            each(true, hide);
        else
        {
            // drawn again once shown, which marks the visibility
            each(true, [](size_t, Slot &slot) { slot.valid=false; });
        }
        return;
    }

    const gweni::Rect region=renderer->clipRegion();
    const gweni::Point &offset=renderer->getRenderOffset();
    gweni::Rect plotClip;

    // the plot at the render offset, cut to the control's own region
    {
        const int x0=std::max(region.x, offset.x+bounds.x+plot.x);
        const int y0=std::max(region.y, offset.y+bounds.y+plot.y);
        const int x1=std::min(region.x+region.w, offset.x+bounds.x+plot.x+plot.w);
        const int y1=std::min(region.y+region.h, offset.y+bounds.y+plot.y+plot.h);

        plotClip=gweni::Rect(x0, y0, std::max(0, x1-x0), std::max(0, y1-y0));
    }

    auto drawRect=[&](size_t id, Slot &slot, const gweni::Rect &rect, const tokyonight::Surface &surface,
        int zIndex, const gweni::Rect &clip)
    {
        if(!all && slot.valid && slot.shown && !slot.line && !slot.trapezoid && (slot.rect == rect) && (slot.zIndex == zIndex)
            && sameSurface(slot.surface, surface) && (slot.clip == clip))
            return;

        renderer->setClipRegion(clip);
        renderer->drawRoundedRect(id, rect, surface.radius, surface.borderWidth, surface.top, surface.bottom,
            surface.border, zIndex);
        slot=Slot();
        slot.valid=true;
        slot.shown=true;
        slot.rect=rect;
        slot.surface=surface;
        slot.zIndex=zIndex;
        slot.clip=clip;
    };
    auto drawSegment=[&](size_t id, Slot &slot, const PointF &p0, const PointF &p1, const PointF *prev,
        const PointF *next, float thickness, const Color &color, int zIndex, const gweni::Rect &clip)
    {
        const bool hasPrev=(prev != nullptr);
        const bool hasNext=(next != nullptr);
        const PointF prevPoint=hasPrev ? *prev : PointF();
        const PointF nextPoint=hasNext ? *next : PointF();

        if(!all && slot.valid && slot.shown && slot.line && (slot.p0 == p0) && (slot.p1 == p1)
            && (slot.hasPrev == hasPrev) && (slot.prev == prevPoint) && (slot.hasNext == hasNext)
            && (slot.next == nextPoint) && (slot.thickness == thickness) && (slot.color == color)
            && (slot.zIndex == zIndex) && (slot.clip == clip))
            return;

        renderer->setClipRegion(clip);
        renderer->setDrawColor(color);
        renderer->drawLine(id, p0, p1, thickness, zIndex, prev, next);
        slot=Slot();
        slot.valid=true;
        slot.shown=true;
        slot.line=true;
        slot.p0=p0;
        slot.p1=p1;
        slot.hasPrev=hasPrev;
        slot.prev=prevPoint;
        slot.hasNext=hasNext;
        slot.next=nextPoint;
        slot.thickness=thickness;
        slot.color=color;
        slot.zIndex=zIndex;
        slot.clip=clip;
    };

    const int z=baseControl->getZIndex();

    drawRect(m_primitives[0], m_frame, bounds, tokyonight::ChartFrame, z, region);

    // no plot (not laid out yet, or too small): the frame alone
    if(!drawing)
    {
        each(false, hide);
        return;
    }

    for(size_t i=0; i<lines.size(); ++i)
        drawSegment(m_grid.ids[i], m_grid.drawn[i], lines[i].p0, lines[i].p1, nullptr, nullptr, 1.0f,
            lines[i].color, z+GridLayer, plotClip);
    hideSpare(m_grid);

    // the spans, under the series
    for(size_t i=0; i<m_spans.used; ++i)
    {
        gweni::Rect rect;

        if(!chart->getSpanRect(i, rect))
        {
            hide(m_spans.ids[i], m_spans.drawn[i]);
            continue;
        }
        rect.x+=bounds.x;
        rect.y+=bounds.y;
        drawRect(m_spans.ids[i], m_spans.drawn[i], rect, tokyonight::ChartSpan, z+SpanLayer, plotClip);
    }
    hideSpare(m_spans);

    // A line's (or an area's line's) segments in \p list, each drawn with its
    // neighbours, at \p zIndex in the series' colour and thickness.
    auto drawPolyline=[&](size_t s, SlotList &list, int zIndex, const Color &color)
    {
        // a line's segments, each cut to the plot widened by the stroke and a
        // unit, so a cut end lies outside the plot's clip; cut in doubles,
        // which hold a point far out of range exactly enough
        const size_t segments=list.used;
        const float thickness=chart->getSeriesLineThickness(s);
        const double margin=double(thickness)+2.0;
        const double box[4]={double(left)-margin, double(right)+margin, double(top)-margin, double(bottom)+margin};
        std::vector<double> xs(segments+1);
        std::vector<double> ys(segments+1);
        std::vector<char> finite(segments+1);

        for(size_t i=0; i<xs.size(); ++i)
        {
            finite[i]=chart->getVertex(s, i, xs[i], ys[i]) && std::isfinite(xs[i]);
            xs[i]+=origin.x;
            ys[i]+=origin.y;
        }

        struct Cut
        {
            bool shown=false;
            double t0=0.0;
            double t1=1.0;
            PointF p0;
            PointF p1;
        };
        std::vector<Cut> cuts(segments);

        for(size_t i=0; i<segments; ++i)
        {
            Cut &cut=cuts[i];

            if(!finite[i] || !finite[i+1])
                continue;

            // Liang-Barsky, as clipSegment, noting which side cuts each end
            const double dx=xs[i+1]-xs[i];
            const double dy=ys[i+1]-ys[i];
            const double p[4]={-dx, dx, -dy, dy};
            const double q[4]={xs[i]-box[0], box[1]-xs[i], ys[i]-box[2], box[3]-ys[i]};
            int side0=-1;
            int side1=-1;
            bool inside=true;

            for(int k=0; k<4 && inside; ++k)
            {
                if(p[k] == 0.0)
                {
                    inside=(q[k] >= 0.0);
                    continue;
                }

                const double t=q[k]/p[k];

                if(p[k] < 0.0 && t > cut.t0)
                {
                    cut.t0=t;
                    side0=k;
                }
                else if(p[k] > 0.0 && t < cut.t1)
                {
                    cut.t1=t;
                    side1=k;
                }
            }
            cut.shown=inside && (cut.t0 <= cut.t1);

            // A cut end is on its side, the other coordinate worked out from
            // the end nearer the plot: from a point far out of range, a + t*d
            // would cancel what a double holds of it.
            const bool fromFirst=std::max(std::fabs(xs[i]), std::fabs(ys[i]))
                <= std::max(std::fabs(xs[i+1]), std::fabs(ys[i+1]));
            const double ex=fromFirst ? xs[i] : xs[i+1];
            const double ey=fromFirst ? ys[i] : ys[i+1];
            auto onSide=[&](int side) -> PointF
            {
                if(side < 2)
                    return PointF(float(box[side]), float(ey+(box[side]-ex)*dy/dx));
                return PointF(float(ex+(box[side]-ey)*dx/dy), float(box[side]));
            };

            // the original points where it is not cut, so joints are exact
            cut.p0=(side0 < 0) ? PointF(float(xs[i]), float(ys[i])) : onSide(side0);
            cut.p1=(side1 < 0) ? PointF(float(xs[i+1]), float(ys[i+1])) : onSide(side1);
        }

        for(size_t i=0; i<segments; ++i)
        {
            const Cut &cut=cuts[i];

            if(!cut.shown)
            {
                hide(list.ids[i], list.drawn[i]);
                continue;
            }

            // a neighbour shares the joint only where neither is cut there
            const bool hasPrev=(i > 0) && cuts[i-1].shown && (cuts[i-1].t1 == 1.0) && (cut.t0 == 0.0);
            const bool hasNext=(i+1 < segments) && cuts[i+1].shown && (cuts[i+1].t0 == 0.0) && (cut.t1 == 1.0);
            const PointF prev=hasPrev ? cuts[i-1].p0 : PointF();
            const PointF next=hasNext ? cuts[i+1].p1 : PointF();

            drawSegment(list.ids[i], list.drawn[i], cut.p0, cut.p1, hasPrev ? &prev : nullptr,
                hasNext ? &next : nullptr, thickness, color, zIndex, plotClip);
        }
    };

    auto drawTrapezoid=[&](size_t id, Slot &slot, const float edges[6], const Color &color, int zIndex,
        const gweni::Rect &clip)
    {
        if(!all && slot.valid && slot.shown && slot.trapezoid && (slot.p0 == PointF(edges[0], edges[2]))
            && (slot.p1 == PointF(edges[1], edges[3])) && (slot.prev == PointF(edges[4], edges[5]))
            && (slot.color == color) && (slot.zIndex == zIndex) && (slot.clip == clip))
            return;

        renderer->setClipRegion(clip);
        renderer->drawTrapezoid(id, edges[0], edges[1], edges[2], edges[3], edges[4], edges[5], color, zIndex);
        slot=Slot();
        slot.valid=true;
        slot.shown=true;
        slot.trapezoid=true;
        slot.p0=PointF(edges[0], edges[2]);
        slot.p1=PointF(edges[1], edges[3]);
        slot.prev=PointF(edges[4], edges[5]);
        slot.color=color;
        slot.zIndex=zIndex;
        slot.clip=clip;
    };

    // an area's line's layer, in front of every series' fill
    size_t areaRank=0;

    for(size_t s=0; s<seriesCount; ++s)
    {
        SlotList &list=m_series[s];
        const bool shown=chart->isSeriesVisible(s) && hasPlot;
        const int zIndex=z+SeriesLayer+int(s);
        const tokyonight::Surface &surface=tokyonight::chartSeries(chart->getSeriesColorIndex(s));
        const controls::ChartSeriesType type=chart->getSeriesType(s);
        const bool area=controls::Chart::isAreaType(type);
        SlotList &lineList=m_areaLines[s];
        const int lineZ=z+SeriesLayer+int(seriesCount)+int(areaRank);

        if(area)
            ++areaRank;
        hideSpare(list);
        hideSpare(lineList);
        if(!shown)
        {
            for(SlotList *one : {&list, &lineList})
            {
                for(size_t i=0; i<one->used; ++i)
                    hide(one->ids[i], one->drawn[i]);
            }
            continue;
        }

        if((type == controls::ChartSeriesType::Bar) || (type == controls::ChartSeriesType::StackedBar))
        {
            for(size_t i=0; i<list.used; ++i)
            {
                gweni::Rect rect;

                if(!chart->getBarRect(s, i, rect))
                {
                    hide(list.ids[i], list.drawn[i]);
                    continue;
                }
                rect.x+=bounds.x;
                rect.y+=bounds.y;
                drawRect(list.ids[i], list.drawn[i], rect, surface, zIndex, plotClip);
            }
            continue;
        }

        if(!area)
        {
            drawPolyline(s, list, zIndex, surface.top);
            continue;
        }

        // an area's fill, a slot per segment: a step's rect in whole units cut
        // to the plot, or a trapezoid cut along its edges to a box a little
        // wider than the plot (exact: they are straight), its heights held
        // AreaReach units past the plot so its quad stays in range
        const tokyonight::Surface &fill=tokyonight::chartArea(chart->getSeriesColorIndex(s));
        const bool step=controls::Chart::isStepType(type);
        const double boxLeft=double(left)-2.0;
        const double boxRight=double(right)+2.0;
        const double reachTop=double(top)-AreaReach;
        const double reachBottom=double(bottom)+AreaReach;

        for(size_t i=0; i<list.used; ++i)
        {
            double x0, x1, top0, top1, base0, base1;

            if(!chart->getAreaSegment(s, i, x0, x1, top0, top1, base0, base1))
            {
                hide(list.ids[i], list.drawn[i]);
                continue;
            }
            x0+=origin.x;
            x1+=origin.x;
            top0+=origin.y;
            top1+=origin.y;
            base0+=origin.y;
            base1+=origin.y;

            if(step)
            {
                const auto unit=[](double v, double lo, double hi)
                {
                    return int(std::lround(std::min(std::max(v, lo), hi)));
                };
                const int rx0=unit(x0, left, right);
                const int rx1=unit(x1, left, right);
                const int ry0=unit(std::min(top0, base0), top, bottom);
                const int ry1=unit(std::max(top0, base0), top, bottom);

                if(rx1 <= rx0 || ry1 <= ry0)
                {
                    hide(list.ids[i], list.drawn[i]);
                    continue;
                }
                drawRect(list.ids[i], list.drawn[i], gweni::Rect(rx0, ry0, rx1-rx0, ry1-ry0), fill, zIndex,
                    plotClip);
                continue;
            }

            if(!(x1 > x0) || x1 <= boxLeft || x0 >= boxRight)
            {
                hide(list.ids[i], list.drawn[i]);
                continue;
            }

            // an end outside the box is moved to it along both edges
            const double dx=x1-x0;
            const double topSlope=(top1-top0)/dx;
            const double baseSlope=(base1-base0)/dx;

            if(x0 < boxLeft)
            {
                top0+=topSlope*(boxLeft-x0);
                base0+=baseSlope*(boxLeft-x0);
                x0=boxLeft;
            }
            if(x1 > boxRight)
            {
                top1-=topSlope*(x1-boxRight);
                base1-=baseSlope*(x1-boxRight);
                x1=boxRight;
            }

            const auto held=[&](double y) { return float(std::min(std::max(y, reachTop), reachBottom)); };
            const float edges[6]={float(x0), float(x1), held(top0), held(top1), held(base0), held(base1)};

            drawTrapezoid(list.ids[i], list.drawn[i], edges, fill.top, zIndex, plotClip);
        }
        drawPolyline(s, lineList, lineZ, surface.top);
    }

    // the markers, in front of every series
    const int markerZ=z+SeriesLayer+int(seriesCount)+int(areaRank);

    for(size_t i=0; i<m_markers.used; ++i)
    {
        PointF lineTop;
        PointF lineBottom;

        if(!hasPlot || !chart->getMarkerLine(i, lineTop, lineBottom))
        {
            hide(m_markers.ids[i], m_markers.drawn[i]);
            continue;
        }
        drawSegment(m_markers.ids[i], m_markers.drawn[i], PointF(origin.x+lineTop.x, origin.y+lineTop.y),
            PointF(origin.x+lineBottom.x, origin.y+lineBottom.y), nullptr, nullptr, 1.0f,
            tokyonight::ChartMarker.top, markerZ, plotClip);
    }
    hideSpare(m_markers);

    // the crosshair at the hovered x, in front of the markers
    {
        PointF lineTop;
        PointF lineBottom;

        if(!hasPlot || !chart->getCrosshairLine(lineTop, lineBottom))
            hide(m_crosshair.ids[0], m_crosshair.drawn[0]);
        else
            drawSegment(m_crosshair.ids[0], m_crosshair.drawn[0], PointF(origin.x+lineTop.x, origin.y+lineTop.y),
                PointF(origin.x+lineBottom.x, origin.y+lineBottom.y), nullptr, nullptr, 1.0f,
                tokyonight::ChartCrosshair.top, markerZ+1, plotClip);
    }

    renderer->setClipRegion(region);
}


///////////////////////////////////////////////////////////////////////////
//NodeGraphMarquee
void NodeGraphMarquee::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(baseControl->getStateChange() == controls::StateChange_Nothing)
        return;

    const controls::internal::NodeGraphMarquee *marquee=
        static_cast<const controls::internal::NodeGraphMarquee *>(baseControl);
    const gweni::Rect &rect=marquee->getRect();
    const controls::NodeGraph *graph=nullptr;

    for(controls::Base *parent=baseControl->getParent(); parent && !graph; parent=parent->getParent())
        graph=gweni_cast<controls::NodeGraph>(parent);

    // hidden between marquees: its slots hidden once, when it is hidden
    const unsigned int sc=baseControl->getStateChange();

    if(!baseControl->visible())
    {
        if(sc&(controls::StateChange_Visibility|controls::StateChange_Created))
        {
            for(size_t id : m_primitives)
                renderer->hidePrimitive(id);
        }
        return;
    }
    if(!graph || (rect.w <= 0) || (rect.h <= 0))
    {
        for(size_t id : m_primitives)
            renderer->hidePrimitive(id);
        return;
    }

    const gweni::Rect &origin=baseControl->getRenderBounds();
    const float x0=float(origin.x+rect.x);
    const float y0=float(origin.y+rect.y);
    const float x1=x0+float(rect.w);
    const float y1=y0+float(rect.h);
    // the corners round the rect, each side drawn with the ones it meets
    const PointF corners[4]={PointF(x0, y0), PointF(x1, y0), PointF(x1, y1), PointF(x0, y1)};
    // a canvas unit on screen at the graph's zoom
    const float thickness=1.0f/graph->zoom();
    const int z=baseControl->getZIndex();

    renderer->setDrawColor(tokyonight::Marquee.border);
    for(size_t i=0; i<4; ++i)
    {
        const PointF &prev=corners[(i+3)%4];
        const PointF &next=corners[(i+2)%4];

        renderer->drawLine(m_primitives[i], corners[i], corners[(i+1)%4], thickness, z, &prev, &next);
    }
}


///////////////////////////////////////////////////////////////////////////
//NodeGraphMarqueeFill
void NodeGraphMarqueeFill::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(baseControl->getStateChange() == controls::StateChange_Nothing)
        return;

    const controls::internal::NodeGraphMarquee *marquee=
        static_cast<const controls::internal::NodeGraphMarquee *>(baseControl);
    gweni::Rect rect=marquee->getRect();
    const unsigned int sc=baseControl->getStateChange();

    if(!baseControl->visible())
    {
        if(sc&(controls::StateChange_Visibility|controls::StateChange_Created))
            renderer->hidePrimitive(m_primitives[0]);
        return;
    }
    if((rect.w <= 0) || (rect.h <= 0))
    {
        renderer->hidePrimitive(m_primitives[0]);
        return;
    }

    const gweni::Rect &origin=baseControl->getRenderBounds();
    const tokyonight::Surface &surface=tokyonight::Marquee;

    rect.x+=origin.x;
    rect.y+=origin.y;
    renderer->drawRoundedRect(m_primitives[0], rect, 0.0f, 0.0f, surface.top, surface.bottom, surface.border,
        baseControl->getZIndex());
}


///////////////////////////////////////////////////////////////////////////
//Node
void Node::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(baseControl->getStateChange() == controls::StateChange_Nothing)
        return;

    controls::Node *node=static_cast<controls::Node *>(baseControl);
    const unsigned int sc=baseControl->getStateChange();

    if(!baseControl->visible())
    {
        if((sc&controls::StateChange_Visibility) || (sc&controls::StateChange_Created))
        {
            renderer->hidePrimitive(m_primitives[0]);
            renderer->hidePrimitive(m_primitives[1]);
        }
        return;
    }

    const gweni::Rect &origin=baseControl->getRenderBounds();
    gweni::Rect frame=node->getFrameRect();

    frame.x+=origin.x;
    frame.y+=origin.y;

    const tokyonight::Surface &body=node->isSelected()?tokyonight::NodeSelected:tokyonight::NodeBody;
    const int z=baseControl->getZIndex();

    renderer->drawRoundedRect(m_primitives[0], frame, body.radius, body.borderWidth,
        body.top, body.bottom, body.border, z);

    Color top=tokyonight::NodeTitle.top;
    Color bottom=tokyonight::NodeTitle.bottom;

    if(node->hasTitleColor())
    {
        top=node->getTitleColor();
        bottom=top;
        bottom.r=static_cast<unsigned char>(bottom.r*0.8f);
        bottom.g=static_cast<unsigned char>(bottom.g*0.8f);
        bottom.b=static_cast<unsigned char>(bottom.b*0.8f);
    }

    const gweni::Rect title(frame.x, frame.y, frame.w, std::min(frame.h, controls::Node::TitleHeight));

    renderer->drawRoundedRect(m_primitives[1], title, body.radius, body.borderWidth, top, bottom,
        body.border, z+1, renderer::SquareBottomLeft|renderer::SquareBottomRight|renderer::NoBorderBottom);
}


///////////////////////////////////////////////////////////////////////////
//Pin
void Pin::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(baseControl->getStateChange() == controls::StateChange_Nothing)
        return;

    controls::Pin *pin=static_cast<controls::Pin *>(baseControl);
    const unsigned int sc=baseControl->getStateChange();

    if(!baseControl->visible())
    {
        if((sc&controls::StateChange_Visibility) || (sc&controls::StateChange_Created))
            renderer->hidePrimitive(m_primitives[0]);
        return;
    }

    const tokyonight::Surface &surface=(pin->getDirection() == controls::PinDirection::Input)?
        tokyonight::PinIn:tokyonight::PinOut;
    Color color=pin->hasColor()?pin->getColor():surface.top;
    const gweni::Rect &origin=baseControl->getRenderBounds();
    gweni::Rect marker=pin->getMarkerRect();
    const int z=baseControl->getZIndex();

    marker.x+=origin.x;
    marker.y+=origin.y;

    // hovered: a unit larger each way, a third of the way to white
    if(pin->isHovered())
    {
        marker=gweni::Rect(marker.x-1, marker.y-1, marker.w+2, marker.h+2);
        color=Color(static_cast<unsigned char>(color.r+(255-color.r)/3),
            static_cast<unsigned char>(color.g+(255-color.g)/3),
            static_cast<unsigned char>(color.b+(255-color.b)/3), color.a);
    }

    if(pin->getShape() == controls::PinShape::Triangle)
    {
        // pointing right, the way a graph flows
        renderer->drawIcon(m_primitives[0], marker, pin->isConnected()?7:8, 3, color, z);
        return;
    }

    const float radius=(pin->getShape() == controls::PinShape::Circle)?marker.w*0.5f:surface.radius;

    if(pin->isConnected())
        renderer->drawRoundedRect(m_primitives[0], marker, radius, 0.0f, color, color, color, z);
    else
        renderer->drawRoundedRect(m_primitives[0], marker, radius, surface.borderWidth,
            surface.bottom, surface.bottom, color, z);
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
    // OpenGLCore draws one buffer in slot order. The field takes the lower
    // slot so it is drawn first: the marker's partly covered AA rim, at z+1,
    // then blends over the field, while a field drawn after it would be
    // rejected by the GL_LESS depth test under the rim, which would blend
    // over the background instead.
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

    // the six segments top to bottom in the lowest slots, the marker in the
    // highest, so it is drawn after the strip (see ColorLerpBox::update)
    size_t slots[7];

    std::copy(m_primitives.begin(), m_primitives.begin()+7, slots);
    std::sort(slots, slots+7);

    // getColorAtHeight() divides by the height
    if(visible && r.h > 0)
    {
        const int x=r.x+Inset;
        const int w=std::max(r.w-2*Inset, 0);
        const int z=baseControl->getZIndex();

        // Segment i is hue arc i, rows i*h/6 .. (i+1)*h/6, edge to edge with
        // its neighbours. Each stop is the strip's own colour at that edge,
        // the one a click there selects.
        for(int i=0; i<6; ++i)
        {
            const int top=i*r.h/6;
            const int bottom=(i+1)*r.h/6;

            renderer->drawRoundedRect(slots[i], gweni::Rect(x, r.y+top, w, bottom-top), 0.0f, 0.0f,
                slider->getColorAtHeight(top), slider->getColorAtHeight(bottom),
                gweni::Color(0, 0, 0, 0), z);
        }

        // the selection bar across the whole width, centred on the selected
        // row, in front of the strip
        const tokyonight::Surface &marker=tokyonight::ColorMarker;
        const int height=std::max(std::min(MarkerHeight, r.h), 0);
        const gweni::Rect rect(r.x, r.y+clamp(slider->getSelectedDist()-height/2, 0, r.h-height),
            r.w, height);

        renderer->drawRoundedRect(slots[6], rect, marker.radius, marker.borderWidth,
            marker.top, marker.bottom, marker.border, z+1);
    }

    const unsigned int sc=baseControl->getStateChange();
    if((sc&controls::StateChange_Visibility) || (sc&controls::StateChange_Created))
    {
        for(size_t i=0; i<7; ++i)
            renderer->showPrimitive(visible?4:0, slots[i]);
    }
}


///////////////////////////////////////////////////////////////////////////
//ColorDisplay
void ColorDisplay::paint(renderer::Base *renderer, size_t slot, controls::Base *baseControl,
    const gweni::Color &color, bool checker, const tokyonight::Surface &frame)
{
    checker=checker && color.a < 255;
    const bool visible=baseControl->visible();
    // the two draws write styles 4 and 7 into the same slot, so a switch
    // between them changes the slot's style; showPrimitive below must give
    // back the one drawn, not always 4
    const int style=checker?7:4;

    if(visible)
    {
        const gweni::Rect r=baseControl->getRenderBounds();

        if(checker)
        {
            const tokyonight::Surface &cells=tokyonight::ColorChecker;

            renderer->drawCheckerRect(slot, r, baseControl->getZIndex(), color,
                cells.top, cells.bottom, frame.radius, frame.borderWidth, frame.border);
        }
        else
            renderer->drawRoundedRect(slot, r, frame.radius, frame.borderWidth,
                color, color, frame.border, baseControl->getZIndex());
    }

    const unsigned int sc=baseControl->getStateChange();
    if((sc&controls::StateChange_Visibility) || (sc&controls::StateChange_Created))
        renderer->showPrimitive(visible?style:0, slot);
}

void ColorDisplay::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    controls::internal::ColorDisplay *display=static_cast<controls::internal::ColorDisplay *>(baseControl);

    paint(renderer, m_primitives[0], baseControl, display->getColor(), display->getDrawCheckers(),
        tokyonight::ColorSwatch);
}

///////////////////////////////////////////////////////////////////////////
//ColorButton
void ColorButton::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    controls::ColorButton *button=static_cast<controls::ColorButton *>(baseControl);
    const tokyonight::Surface *frame=&tokyonight::ColorSwatch;

    if(button->isDisabled())
        frame=&tokyonight::ColorButtonDisabled;
    else if(button->isDepressed())
        frame=&tokyonight::ColorButtonDown;
    else if(button->isHovered())
        frame=&tokyonight::ColorButtonHover;

    ColorDisplay::paint(renderer, m_primitives[0], baseControl, button->getColor(), true, *frame);
}


///////////////////////////////////////////////////////////////////////////
//DragNumber
void DragNumber::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    controls::DragNumber *field=static_cast<controls::DragNumber *>(baseControl);

    // the edit box draws its own field over this one
    if(!baseControl->visible() || field->isEditing())
    {
        renderer->hidePrimitive(m_primitives[0]);
        return;
    }

    const tokyonight::Surface *surface=&tokyonight::DragNumber;

    if(field->isDisabled())
        surface=&tokyonight::DragNumberDisabled;
    else if(field->isDepressed())
        surface=&tokyonight::DragNumberDown;
    else if(field->isFocussed())
        surface=&tokyonight::DragNumberFocus;
    else if(field->isHovered())
        surface=&tokyonight::DragNumberHover;

    renderer->drawRoundedRect(m_primitives[0], baseControl->getRenderBounds(), surface->radius,
        surface->borderWidth, surface->top, surface->bottom, surface->border, baseControl->getZIndex());
}


///////////////////////////////////////////////////////////////////////////
//VectorFieldTag
void VectorFieldTag::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    if(!baseControl->visible())
    {
        renderer->hidePrimitive(m_primitives[0]);
        return;
    }

    static const tokyonight::Surface *const axes[]=
    {
        &tokyonight::VectorTagX, &tokyonight::VectorTagY, &tokyonight::VectorTagZ, &tokyonight::VectorTagW
    };
    controls::internal::VectorFieldTag *tag=static_cast<controls::internal::VectorFieldTag *>(baseControl);
    const tokyonight::Surface *surface=tag->isDisabled()?&tokyonight::DragNumberDisabled:axes[tag->getAxis()];

    renderer->drawRoundedRect(m_primitives[0], baseControl->getRenderBounds(), surface->radius,
        surface->borderWidth, surface->top, surface->bottom, surface->border, baseControl->getZIndex());
}


///////////////////////////////////////////////////////////////////////////
//Chip
namespace
{
// \p a moved toward \p b by \p t (0..1), channel by channel, alpha included.
gweni::Color towards(const gweni::Color &a, const gweni::Color &b, float t)
{
    const auto mix=[t](unsigned char from, unsigned char to)
        {
            return static_cast<unsigned char>(float(from)+(float(to)-float(from))*t+0.5f);
        };

    return gweni::Color(mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a));
}
}//namespace

void Chip::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    static const tokyonight::Surface *const tones[]=
    {
        &tokyonight::ChipNeutral, &tokyonight::ChipAccent, &tokyonight::ChipSuccess,
        &tokyonight::ChipWarning, &tokyonight::ChipDanger
    };
    static const tokyonight::Surface *const palette[controls::Chip::PaletteSize]=
    {
        &tokyonight::ChipPalette0, &tokyonight::ChipPalette1, &tokyonight::ChipPalette2,
        &tokyonight::ChipPalette3, &tokyonight::ChipPalette4, &tokyonight::ChipPalette5,
        &tokyonight::ChipPalette6, &tokyonight::ChipPalette7
    };
    controls::Chip *chip=static_cast<controls::Chip *>(baseControl);
    const int index=chip->getPaletteIndex();
    tokyonight::Surface surface=(index >= 0)?*palette[index]:*tones[static_cast<int>(chip->getTone())];

    if(chip->isDisabled())
    {
        // the disabled colours in the chip's own shape (ButtonDisabled's
        // radius is a button's)
        surface.top=tokyonight::ButtonDisabled.top;
        surface.bottom=tokyonight::ButtonDisabled.bottom;
        surface.border=tokyonight::ButtonDisabled.border;
    }
    else if(chip->showsInteraction() && (chip->isDepressed() || chip->isHovered()))
    {
        const float t=chip->isDepressed()?0.6f:0.35f;

        surface.top=towards(surface.top, surface.border, t);
        surface.bottom=towards(surface.bottom, surface.border, t);
    }

    const Rect &bounds=chip->getRenderBounds();

    surface.radius=std::min(surface.radius, 0.5f*float(std::min(bounds.w, bounds.h)));
    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, surface);
}


///////////////////////////////////////////////////////////////////////////
//Switch
void Switch::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.size() < 2)
        return;

    controls::Switch *control=static_cast<controls::Switch *>(baseControl);
    const bool on=control->isOn();
    const tokyonight::Surface *track;
    const tokyonight::Surface *thumb;

    if(control->isDisabled())
    {
        track=on?&tokyonight::SwitchTrackOnDisabled:&tokyonight::SwitchTrackDisabled;
        thumb=&tokyonight::SwitchThumbDisabled;
    }
    else
    {
        if(control->isHovered() || control->isDepressed())
            track=on?&tokyonight::SwitchTrackOnHover:&tokyonight::SwitchTrackOffHover;
        else
            track=on?&tokyonight::SwitchTrackOn:&tokyonight::SwitchTrackOff;
        thumb=on?&tokyonight::SwitchThumbOn:&tokyonight::SwitchThumbOff;
    }

    const Rect &bounds=control->getRenderBounds();
    Rect trackRect=control->getTrackRect();
    Rect thumbRect=control->getThumbRect();

    trackRect.x+=bounds.x;
    trackRect.y+=bounds.y;
    thumbRect.x+=bounds.x;
    thumbRect.y+=bounds.y;

    tokyonight::Surface trackSurface=*track;

    if(!control->isDisabled() && control->isFocussed())
    {
        trackSurface.border=tokyonight::SwitchFocus.border;
        trackSurface.borderWidth=tokyonight::SwitchFocus.borderWidth;
    }
    trackSurface.radius=std::min(trackSurface.radius, 0.5f*float(std::min(trackRect.w, trackRect.h)));
    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, trackSurface, trackRect);

    if(baseControl->visible())
    {
        const float radius=std::min(thumb->radius, 0.5f*float(std::min(thumbRect.w, thumbRect.h)));

        renderer->drawRoundedRect(m_primitives[1], thumbRect, radius, thumb->borderWidth,
            thumb->top, thumb->bottom, thumb->border, baseControl->getZIndex()+1);
    }
    else
        renderer->hidePrimitive(m_primitives[1]);
}


///////////////////////////////////////////////////////////////////////////
//Spinner
void Spinner::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.size() < 2)
        return;

    if(!baseControl->visible())
    {
        renderer->hidePrimitive(m_primitives[0]);
        renderer->hidePrimitive(m_primitives[1]);
        return;
    }

    controls::Spinner *control=static_cast<controls::Spinner *>(baseControl);
    const Rect &bounds=control->getRenderBounds();
    const float thickness=control->getThickness();
    const int z=baseControl->getZIndex();

    // stopped, the still ring covers the track: two rings on one circle would
    // show the track's edge round it
    if(control->showsTrack() && control->isSpinning())
        renderer->drawArc(m_primitives[0], bounds, 0.0f, 1.0f, thickness, tokyonight::SpinnerTrack.top, z);
    else
        renderer->hidePrimitive(m_primitives[0]);

    if(control->isSpinning())
        renderer->drawArc(m_primitives[1], bounds, control->getArcStart(), control->getArcSweep(), thickness,
            tokyonight::SpinnerArc.top, z+1);
    else
        renderer->drawArc(m_primitives[1], bounds, 0.0f, 1.0f, thickness, tokyonight::SpinnerStopped.top, z+1);
}


///////////////////////////////////////////////////////////////////////////
//Badge
void Badge::generate(renderer::Base *renderer, controls::Base *baseControl)
{
    ControlRegister<Badge>::generate(renderer, baseControl);

    if(!m_hasText)
    {
        renderer->generateTextPrimitive(m_textId);
        m_hasText=true;
    }
}

void Badge::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty() || !m_hasText)
        return;

    controls::Badge *badge=static_cast<controls::Badge *>(baseControl);
    const bool visible=badge->visible();

    if(visible)
    {
        const tokyonight::Surface &surface=tokyonight::Badge;
        const Rect &bounds=badge->getRenderBounds();
        const float radius=std::min(surface.radius, 0.5f*float(std::min(bounds.w, bounds.h)));
        const int z=badge->getAttachedZIndex();

        renderer->drawRoundedRect(m_primitives[0], bounds, radius, surface.borderWidth, surface.top,
            surface.bottom, surface.border, z);
        renderer->setDrawColor(badge->getSkin()->Colors.BadgeText);
        renderer->renderText(m_textId, badge->getFont(), badge->textOrigin(), z, badge->getText());
    }

    const int stateChange=badge->getStateChange();

    if((stateChange & controls::StateChange_Visibility) || (stateChange & controls::StateChange_Created))
    {
        renderer->showPrimitive(visible?4:0, m_primitives[0]);
        if(visible)
            renderer->showText(m_textId);
        else
            renderer->hideText(m_textId);
    }
}

void Badge::remove(renderer::Base *renderer, controls::Base *baseControl)
{
    ControlRegister<Badge>::remove(renderer, baseControl);

    if(m_hasText)
    {
        renderer->releaseTextPrimitive(m_textId);
        m_hasText=false;
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

// A notebook tab (tokyoNight.h, "Tabs"). Every tab is square on the corners
// facing the page and has no border on that side. The strip reaches into the
// page by TabStrip::PageOverlap, so every tab is drawn PageOverlap short of the
// page, up to the frame's outer edge. The selected tab draws the rest of its
// bounds in the frame's fill and reaches over the frame's border from there by
// that border's width in device px (drawRoundedRect's reach), so it covers
// exactly the frame's border rows (columns) and opens into the page at any
// scale: a logical unit of overlap snaps to 1 or 2 px by its position at a
// fractional one, while the border is max(1, snap(width)) px everywhere. An
// unselected tab is also drawn UnselectedOuterInset shorter on the outer side,
// so the frame's border runs on under it, and UnselectedEndInset in at each end
// along the row, so a tab after the selected one (drawn over it: tabs overlap by
// a unit and the later one is in front) leaves its border alone; only its own
// state picks what it draws, so a selection writes the two tabs alone. A
// disabled tab looks unselected and does not hover.
void TabButton::update(renderer::Base *renderer, controls::Base *baseControl)
{
    controls::TabButton *control=static_cast<controls::TabButton *>(baseControl);
    controls::TabControl *tabControl=control->getTabControl();
    const DockPosition side=tabControl?tabControl->getTabStrip()->getDock():DockPosition::Top;
    const bool disabled=control->isDisabled();
    const bool selected=control->isActive() && !disabled;
    const tokyonight::Surface &surface=selected?tokyonight::TabActive
        :(!disabled && control->isTabHovered())?tokyonight::TabHover:tokyonight::TabInactive;
    const int outer=selected?0:UnselectedOuterInset;
    const int inner=controls::TabStrip::PageOverlap;
    const float reach=selected?tokyonight::TabControl.borderWidth:0.0f;
    const int end=selected?0:UnselectedEndInset;
    Rect rect=control->getRenderBounds();
    uint8_t edges=0;

    switch(side)
    {
    case DockPosition::Bottom:
        edges=renderer::SquareTopLeft|renderer::SquareTopRight|renderer::NoBorderTop;
        rect=Rect(rect.x+end, rect.y+inner, rect.w-2*end, rect.h-inner-outer);
        break;
    case DockPosition::Left:
        edges=renderer::SquareTopRight|renderer::SquareBottomRight|renderer::NoBorderRight;
        rect=Rect(rect.x+outer, rect.y+end, rect.w-outer-inner, rect.h-2*end);
        break;
    case DockPosition::Right:
        edges=renderer::SquareTopLeft|renderer::SquareBottomLeft|renderer::NoBorderLeft;
        rect=Rect(rect.x+inner, rect.y+end, rect.w-inner-outer, rect.h-2*end);
        break;
    default:
        edges=renderer::SquareBottomLeft|renderer::SquareBottomRight|renderer::NoBorderBottom;
        rect=Rect(rect.x+end, rect.y+outer, rect.w-2*end, rect.h-outer-inner);
        break;
    }

    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, surface, rect, edges, reach);
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

// Draws nothing: the strip shows the parent's background, and the page frame
// (TabControlInner) is the only frame (requiredPrimitives() is 0).
void TabControl::update(renderer::Base * /*renderer*/, controls::Base * /*baseControl*/)
{
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
        control->textOrigin(), control->getZIndex(),
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
//CodeLineText: a CodeEditor line's run, drawn with its tokens' colour spans
//(renderText's span overload) in the Text's colour outside them. A renderer of
//its own, so the plain Text's stays the plain call alone.
void CodeLineText::staticInit(TexturedSkin *skin)
{
    m_staticInit=true;
}

void CodeLineText::generate(renderer::Base *renderer, controls::Base *baseControl)
{
    m_primitives.resize(1);

    renderer->generateTextPrimitive(m_primitives[0]);
}

void CodeLineText::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(baseControl->getStateChange() == controls::StateChange_Nothing)
        return;

    auto *control=dynamic_cast<controls::internal::CodeLineText *>(baseControl);

    if(!control)
        return;

    const gweni::Color &colorOverride=control->getTextColorOverride();

    renderer->setDrawColor((colorOverride.a == 0)?control->getTextColor():colorOverride);
    renderer->renderText(m_primitives[0], control->getFont(), control->textOrigin(), control->getZIndex(),
        control->getText(), control->getColorSpans());

    if((baseControl->getStateChange() & controls::StateChange_Visibility) ||
        (baseControl->getStateChange() & controls::StateChange_Created))
    {
        if(!baseControl->visible())
            renderer->hideText(m_primitives[0]);
        else
            renderer->showText(m_primitives[0]);
    }
}

void CodeLineText::remove(renderer::Base *renderer, controls::Base *baseControl)
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

// A rect in the box's own coordinates moved to its parent's (the render offset
// takes it on to the canvas) and cut to the box, which clips its text the same.
static Rect inBox(const Rect &local, const Rect &box)
{
    const int x0=std::max(box.x+local.x, box.x);
    const int y0=std::max(box.y+local.y, box.y);
    const int x1=std::min(box.x+local.x+local.w, box.x+box.w);
    const int y1=std::min(box.y+local.y+local.h, box.y+box.h);

    return Rect(x0, y0, std::max(x1-x0, 0), std::max(y1-y0, 0));
}

// [0] the box, [1] the selection at the text's z, behind every glyph
// (renderer::Base::renderText), [2] the caret at the overlay z, in front of
// the whole subtree, a multiline box's line children included. Both are drawn
// only while the box has the focus. A blink (StateChange_Caret alone) shows or
// hides [2] and writes nothing else; any other change rewrites what is shown,
// and a hidden primitive's geometry is rewritten when it is shown next.
void TextBox::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.size() < 3)
        return;

    controls::TextBox *box=dynamic_cast<controls::TextBox *>(baseControl);

    if(!box)
        return;

    const int stateChange=box->getStateChange();
    const bool blinkOnly=(stateChange == controls::StateChange_Caret);
    const bool focused=box->visible() && box->isFocussed();
    // the field part: the box less its message line (TextBox::getMessageRoom)
    Rect bounds=box->getRenderBounds();

    bounds.h=box->getFieldHeight();

    if(!blinkOnly)
    {
        const bool error=box->hasError();
        const tokyonight::Surface *surface=error?&tokyonight::TextBoxError:&tokyonight::TextBox;
        if(box->isDisabled())
            surface=error?&tokyonight::TextBoxErrorDisabled:&tokyonight::TextBoxDisabled;
        else if(box->isFocussed())
            surface=error?&tokyonight::TextBoxErrorFocus:&tokyonight::TextBoxFocus;

        tokyonight::paintSurface(renderer, m_primitives.data(), box, *surface, bounds);

        const Rect selection=inBox(box->getSelectionBounds(), bounds);
        const tokyonight::Surface &fill=tokyonight::TextSelection;

        if(focused && selection.w > 0 && selection.h > 0)
            renderer->drawRoundedRect(m_primitives[1], selection, fill.radius, fill.borderWidth,
                fill.top, fill.bottom, fill.border, box->getTextZIndex());
        else
            renderer->hidePrimitive(m_primitives[1]);

        m_caretStale=true;
    }

    const Rect caret=inBox(box->getCaretBounds(), bounds);

    if(!focused || !box->isCaretVisible() || caret.w <= 0 || caret.h <= 0)
    {
        renderer->hidePrimitive(m_primitives[2]);
        return;
    }

    if(m_caretStale)
    {
        renderer->setDrawColor(box->getSkin()->Colors.Label.Default);
        renderer->drawFilledRect(m_primitives[2], caret, box->getOverlayZIndex());
        m_caretStale=false;
    }
    else
        renderer->showPrimitive(1, m_primitives[2]);
}

void TextBox::remove(renderer::Base *renderer, controls::Base *baseControl)
{
    releasePrimitives(renderer);
}


///////////////////////////////////////////////////////////////////////////
//ToolTip
void ToolTip::staticInit(TexturedSkin *skin)
{
    m_staticInit=true;
}

void ToolTip::generate(renderer::Base *renderer, controls::Base *baseControl)
{
    generatePrimitives(renderer, requiredPrimitives());
}

void ToolTip::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, tokyonight::Tooltip);
}

void ToolTip::remove(renderer::Base *renderer, controls::Base *baseControl)
{
    releasePrimitives(renderer);
}


///////////////////////////////////////////////////////////////////////////
//Popup
void Popup::staticInit(TexturedSkin *skin)
{
    m_staticInit=true;
}

void Popup::generate(renderer::Base *renderer, controls::Base *baseControl)
{
    generatePrimitives(renderer, requiredPrimitives());
}

void Popup::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, tokyonight::Menu);
}

void Popup::remove(renderer::Base *renderer, controls::Base *baseControl)
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
//TreeNode
void TreeNode::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.size() < 2)
        return;

    const controls::TreeNode *node=dynamic_cast<const controls::TreeNode *>(baseControl);

    if(!node || !baseControl->visible())
    {
        for(size_t primitive : m_primitives)
            renderer->hidePrimitive(primitive);
        return;
    }

    // a render bounds is in the parent's coordinates, which for a child node is
    // its parent node's inner panel: the panel spans that node's width
    const gweni::Rect bounds=baseControl->getRenderBounds();
    const gweni::Color line=baseControl->getSkin()->Colors.Tree.Lines;
    const int z=baseControl->getZIndex();
    // a node placed in the TreeControl without addNode() is a root too
    const controls::TreeNode *parent=node->isRoot()?nullptr:gweni_cast<controls::TreeNode>(node->getParent());

    if(parent && !gweni_cast<controls::TreeControl>(node->getParent()) && parent->getInnerPanel())
    {
        // from the parent's line, at its x in the panel, to one unit short of
        // the toggle's chevron (OpenToggleButton draws it 6/10 of the square,
        // centred), or of the title of a node with no toggle
        const int start=parent->lineX()-parent->getInnerPanel()->getX();
        const gweni::Rect &toggle=node->getToggleBounds();
        const int end=bounds.x+(node->hasToggle()
            ?toggle.x+(toggle.w-toggle.w*6/10)/2-1
            :node->getTitleBounds().x-1);
        const int y=bounds.y+node->stubY();

        // as a Separator's rule: the quad reaches one unit past the line on
        // both sides across it, and the stroke is drawn through its middle
        if(end > start)
            renderer->drawIcon(m_primitives[0], gweni::Rect(start, y-1, end-start, 3), 6, 0, line, z);
        else
            renderer->hidePrimitive(m_primitives[0]);
    }
    else
        renderer->hidePrimitive(m_primitives[0]);

    const controls::TreeNode *last=(node->isOpen() && node->hasToggle())?node->lastChildNode():nullptr;

    if(last)
    {
        // down from under the toggle, through the last child's stub row
        const gweni::Rect &toggle=node->getToggleBounds();
        const int x=bounds.x+node->lineX();
        const int top=bounds.y+toggle.y+toggle.h-2;
        const int bottom=bounds.y+node->childPanelY()+last->getY()+last->stubY()+1;

        if(bottom > top)
            renderer->drawIcon(m_primitives[1], gweni::Rect(x-1, top, 3, bottom-top), 6, 1, line, z);
        else
            renderer->hidePrimitive(m_primitives[1]);
    }
    else
        renderer->hidePrimitive(m_primitives[1]);
}

///////////////////////////////////////////////////////////////////////////
//PropertyTreeNode
void PropertyTreeNode::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.size() < 2)
        return;

    const controls::TreeNode *node=dynamic_cast<const controls::TreeNode *>(baseControl);
    const gweni::Rect bounds=baseControl->getRenderBounds();

    if(!node || !baseControl->visible() || bounds.w <= 0 || bounds.h <= 0)
    {
        for(size_t primitive : m_primitives)
            renderer->hidePrimitive(primitive);
        return;
    }

    const int column=std::min(node->childIndent(), bounds.w);
    const int band=std::min(node->childPanelY(), bounds.h);
    const int z=baseControl->getZIndex();

    renderer->setDrawColor(baseControl->getSkin()->Colors.Properties.Border);

    if(column > 0)
        renderer->drawFilledRect(m_primitives[0], gweni::Rect(bounds.x, bounds.y, column, bounds.h), z);
    else
        renderer->hidePrimitive(m_primitives[0]);

    if(band > 0 && bounds.w > column)
        renderer->drawFilledRect(m_primitives[1], gweni::Rect(bounds.x+column, bounds.y, bounds.w-column, band), z);
    else
        renderer->hidePrimitive(m_primitives[1]);
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
void SplitterBar::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    controls::SplitterBar *bar=static_cast<controls::SplitterBar *>(baseControl);

    // a bar that does not draw its background stays hidden on hover and drag
    // too; setShouldDrawBackground() marks only StateChange_Background, so the
    // slot is shown (drawRoundedRect) or hidden here on every update, not only
    // on a visibility change as paintSurface() does
    if(!bar->visible() || !bar->shouldDrawBackground())
    {
        renderer->hidePrimitive(m_primitives[0]);
        return;
    }

    const tokyonight::Surface *surface;

    if(bar->isDisabled())
        surface=&tokyonight::SplitterBarDisabled;
    else if(bar->isDepressed())
        surface=&tokyonight::SplitterBarDown;
    else if(bar->isHovered())
        surface=&tokyonight::SplitterBarHover;
    else
        surface=&tokyonight::SplitterBar;

    renderer->drawRoundedRect(m_primitives[0], bar->getRenderBounds(), surface->radius,
        surface->borderWidth, surface->top, surface->bottom, surface->border, bar->getZIndex());
}

///////////////////////////////////////////////////////////////////////////
//Window
size_t Window::Normal;
size_t Window::Inactive;
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

///////////////////////////////////////////////////////////////////////////
//Dialog, DialogTitleBar, DialogFooter
void Dialog::generate(renderer::Base *renderer, controls::Base *baseControl)
{
    generatePrimitives(renderer, requiredPrimitives());
}

void Dialog::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.size() < 2)
        return;

    const int shadow=dialogs::Dialog::ShadowSize;
    const gweni::Rect bounds=baseControl->getRenderBounds();
    const gweni::Rect panel(bounds.x, bounds.y, std::max(0, bounds.w-shadow), std::max(0, bounds.h-shadow));
    const gweni::Rect behind(panel.x+shadow, panel.y+shadow, panel.w, panel.h);
    const int zIndex=baseControl->getZIndex();
    const bool visible=baseControl->visible();

    if(visible)
    {
        const tokyonight::Surface &s=tokyonight::DialogShadow;
        renderer->drawRoundedRect(m_primitives[0], behind, s.radius, s.borderWidth, s.top, s.bottom,
            s.border, zIndex);

        // +1, over the shadow (depthLayers() reserves it)
        const tokyonight::Surface &p=tokyonight::Dialog;
        renderer->drawRoundedRect(m_primitives[1], panel, p.radius, p.borderWidth, p.top, p.bottom,
            p.border, zIndex+1);
    }

    const unsigned int changes=baseControl->getStateChange();
    if((changes&controls::StateChange_Visibility) || (changes&controls::StateChange_Created))
    {
        renderer->showPrimitive(visible?4:0, m_primitives[0]);
        renderer->showPrimitive(visible?4:0, m_primitives[1]);
    }
}

void Dialog::remove(renderer::Base *renderer, controls::Base *baseControl)
{
    releasePrimitives(renderer);
}

void DialogTitleBar::generate(renderer::Base *renderer, controls::Base *baseControl)
{
    generatePrimitives(renderer, requiredPrimitives());
}

void DialogTitleBar::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, tokyonight::DialogTitle,
        baseControl->getRenderBounds(), renderer::SquareBottomLeft|renderer::SquareBottomRight);
}

void DialogTitleBar::remove(renderer::Base *renderer, controls::Base *baseControl)
{
    releasePrimitives(renderer);
}

void DialogFooter::generate(renderer::Base *renderer, controls::Base *baseControl)
{
    generatePrimitives(renderer, requiredPrimitives());
}

void DialogFooter::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.empty())
        return;

    tokyonight::paintSurface(renderer, m_primitives.data(), baseControl, tokyonight::DialogFooter,
        baseControl->getRenderBounds(), renderer::SquareTopLeft|renderer::SquareTopRight);
}

void DialogFooter::remove(renderer::Base *renderer, controls::Base *baseControl)
{
    releasePrimitives(renderer);
}

///////////////////////////////////////////////////////////////////////////
//Toast
void Toast::generate(renderer::Base *renderer, controls::Base *baseControl)
{
    generatePrimitives(renderer, requiredPrimitives());
}

void Toast::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.size() < 2)
        return;

    controls::Toast *toast=static_cast<controls::Toast *>(baseControl);
    const gweni::Rect bounds=baseControl->getRenderBounds();
    const int zIndex=baseControl->getZIndex();
    const bool visible=baseControl->visible();

    if(visible)
    {
        const tokyonight::Surface &panel=tokyonight::Toast;
        renderer->drawRoundedRect(m_primitives[0], bounds, panel.radius, panel.borderWidth, panel.top,
            panel.bottom, panel.border, zIndex);

        const tokyonight::Surface *accent=&tokyonight::ToastInfo;
        switch(toast->getKind())
        {
        case controls::ToastKind::Info: accent=&tokyonight::ToastInfo; break;
        case controls::ToastKind::Positive: accent=&tokyonight::ToastPositive; break;
        case controls::ToastKind::Warning: accent=&tokyonight::ToastWarning; break;
        case controls::ToastKind::Negative: accent=&tokyonight::ToastNegative; break;
        }

        // inside the panel's border, its corners on the panel's side square
        const int inset=std::max(1, static_cast<int>(panel.borderWidth));
        const gweni::Rect strip(bounds.x+inset+1, bounds.y+inset+2, controls::Toast::AccentWidth,
            std::max(0, bounds.h-2*inset-4));
        renderer->drawRoundedRect(m_primitives[1], strip, accent->radius, accent->borderWidth, accent->top,
            accent->bottom, accent->border, zIndex+1);
    }

    const unsigned int changes=baseControl->getStateChange();
    if((changes&controls::StateChange_Visibility) || (changes&controls::StateChange_Created))
    {
        renderer->showPrimitive(visible?4:0, m_primitives[0]);
        renderer->showPrimitive(visible?4:0, m_primitives[1]);
    }
}

void Toast::remove(renderer::Base *renderer, controls::Base *baseControl)
{
    releasePrimitives(renderer);
}

///////////////////////////////////////////////////////////////////////////
//WindowCloseButton, WindowMaximizeButton, WindowMinimizeButton
void WindowButtonPainter::paint(renderer::Base *renderer, const std::vector<size_t> &primitives,
    controls::Base *baseControl, const tokyonight::Surface &hover,
    const tokyonight::Surface &down, const gweni::Color &activeGlyph, int kind)
{
    if(primitives.size() < 2)
        return;

    controls::Button *button=static_cast<controls::Button *>(baseControl);
    const tokyonight::Surface *surface=nullptr;
    gweni::Color glyph=tokyonight::FgDark;

    if(button->isDisabled())
        glyph=tokyonight::Muted;
    else if(button->isDepressed())
    {
        surface=&down;
        glyph=activeGlyph;
    }
    else if(button->isHovered())
    {
        surface=&hover;
        glyph=activeGlyph;
    }
    else
        surface=&tokyonight::WindowButton;

    // a transparent rect would still write depth over the title bar behind it, so
    // a surface with nothing to show hides its slot instead (drawRoundedRect shows it)
    const bool fill=surface && (surface->top.a || surface->bottom.a ||
        (surface->borderWidth > 0.0f && surface->border.a));
    const bool visible=baseControl->visible();
    const gweni::Rect bounds=baseControl->getRenderBounds();

    if(visible && fill)
        renderer->drawRoundedRect(primitives[0], bounds, surface->radius, surface->borderWidth,
            surface->top, surface->bottom, surface->border, baseControl->getZIndex());
    else
        renderer->hidePrimitive(primitives[0]);

    if(visible)
    {
        const gweni::Rect glyphRect(bounds.x+(bounds.w-GlyphSize)/2, bounds.y+(bounds.h-GlyphSize)/2,
            GlyphSize, GlyphSize);

        renderer->drawIcon(primitives[1], glyphRect, kind, 0, glyph, baseControl->getZIndex()+1);
    }
    else
        renderer->hidePrimitive(primitives[1]);
}

// Tokyo Night's red is light, so the cross turns dark on it
void WindowCloseButton::update(renderer::Base *renderer, controls::Base *baseControl)
{
    WindowButtonPainter::paint(renderer, m_primitives, baseControl, tokyonight::WindowCloseHover,
        tokyonight::WindowCloseDown, tokyonight::Bg, 2);
}

// a square, or the restore glyph (two offset squares) while maximized
void WindowMaximizeButton::update(renderer::Base *renderer, controls::Base *baseControl)
{
    controls::WindowMaximizeButton *button=static_cast<controls::WindowMaximizeButton *>(baseControl);

    WindowButtonPainter::paint(renderer, m_primitives, baseControl, tokyonight::ButtonHover,
        tokyonight::ButtonDown, tokyonight::Fg, button->isMaximized()?5:4);
}

void WindowMinimizeButton::update(renderer::Base *renderer, controls::Base *baseControl)
{
    WindowButtonPainter::paint(renderer, m_primitives, baseControl, tokyonight::ButtonHover,
        tokyonight::ButtonDown, tokyonight::Fg, 3);
}

///////////////////////////////////////////////////////////////////////////
//OpenToggleButton
void OpenToggleButton::update(renderer::Base *renderer, controls::Base *baseControl)
{
    if(m_primitives.size() < 2)
        return;

    controls::Button *button=static_cast<controls::Button *>(baseControl);
    const tokyonight::Surface *surface=nullptr;
    gweni::Color glyph=tokyonight::FgDark;

    if(button->isDisabled())
        glyph=tokyonight::Muted;
    else if(button->isDepressed())
    {
        surface=&tokyonight::ButtonDown;
        glyph=tokyonight::Fg;
    }
    else if(button->isHovered())
    {
        surface=&tokyonight::ButtonHover;
        glyph=tokyonight::Fg;
    }

    const bool visible=baseControl->visible();

    // at rest there is no surface: a hidden slot, not a transparent rect that
    // would write depth over the node behind it
    if(visible && surface)
        tokyonight::paintSurface(renderer, &m_primitives[0], baseControl, *surface);
    else
        renderer->hidePrimitive(m_primitives[0]);

    if(visible)
    {
        // kind 1's directions: 0 down, 3 right
        const int dir=button->getToggleState()?0:3;
        const gweni::Rect r=baseControl->getRenderBounds();
        const int sz=((r.w<r.h)?r.w:r.h)*6/10;
        const gweni::Rect chevron{ r.x+(r.w-sz)/2, r.y+(r.h-sz)/2, sz, sz };

        renderer->drawIcon(m_primitives[1], chevron, 1, dir, glyph, baseControl->getZIndex()+1);
    }
    else
        renderer->hidePrimitive(m_primitives[1]);
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
template class ControlRegister<VirtualList>;
template class ControlRegister<ListBoxRow>;
template class ControlRegister<Menu>;
template class ControlRegister<MenuStrip>;
template class ControlRegister<MenuItem>;
template class ControlRegister<RightArrow>;
template class ControlRegister<Panel>;
template class ControlRegister<PageControl>;
template class ControlRegister<TabControlInner>;
template class ControlRegister<CollapsibleList>;
template class ControlRegister<CategoryInner>;
template class ControlRegister<Label>;
template class ControlRegister<Resizer>;
template class ControlRegister<Table>;
template class ControlRegister<TabStrip>;
template class ControlRegister<DataTableVirtualBody>;
template class ControlRegister<ScrollBarBar>;
template class ControlRegister<ProgressBar>;
template class ControlRegister<CodeEditor>;
template class ControlRegister<CodeLine>;
template class ControlRegister<CodeGutter>;
template class ControlRegister<LogView>;
template class ControlRegister<LogViewChevron>;
template class ControlRegister<LogViewRow>;
template class ControlRegister<Selectable>;
template class ControlRegister<NavigationRail>;
template class ControlRegister<NavigationItem>;
template class ControlRegister<SegmentedButton>;
template class ControlRegister<StepperStep>;
template class ControlRegister<StepperConnector>;
template class ControlRegister<Separator>;
template class ControlRegister<Bullet>;
template class ControlRegister<InspectorHighlight>;
template class ControlRegister<StatsOverlay>;
template class ControlRegister<Dialog>;
template class ControlRegister<DialogTitleBar>;
template class ControlRegister<DialogFooter>;
template class ControlRegister<Toast>;
template class ControlRegister<RadioButton>;
template class ControlRegister<ScrollBarButton>;
template class ControlRegister<BaseScrollBar>;
template class ControlRegister<Shadow>;
template class ControlRegister<Slider>;
template class ControlRegister<SliderBar>;
template class ControlRegister<PlotHistogram>;
template class ControlRegister<PlotLines>;
template class ControlRegister<Chart>;
template class ControlRegister<ChartLegendRow>;
template class ControlRegister<Timeline>;
template class ControlRegister<ColorLerpBox>;
template class ControlRegister<ColorSlider>;
template class ControlRegister<ColorDisplay>;
template class ControlRegister<ColorButton>;
template class ControlRegister<DragNumber>;
template class ControlRegister<VectorFieldTag>;
template class ControlRegister<Badge>;
template class ControlRegister<Chip>;
template class ControlRegister<Switch>;
template class ControlRegister<Spinner>;
template class ControlRegister<StatusBar>;
// NOTE: TabItem<_Type> is itself a class template (TabItem<TabTop>, <TabBottom>,
// <TabLeft>, <TabRight>); register those specializations here if/when their
// per-position tab skinning is needed. Until then tabs fall back to Base.
template class ControlRegister<TabButton>;
template class ControlRegister<TabControl>;
template class ControlRegister<TabTitleBar>;
template class ControlRegister<Text>;
template class ControlRegister<TextBox>;
template class ControlRegister<ToolTip>;
template class ControlRegister<Popup>;
template class ControlRegister<Tree>;
template class ControlRegister<SpinBox>;
template class ControlRegister<SplitterBar>;
template class ControlRegister<Window>;
template class ControlRegister<WindowCloseButton>;
template class ControlRegister<WindowMaximizeButton>;
template class ControlRegister<WindowMinimizeButton>;
template class ControlRegister<OpenToggleButton>;
template class ControlRegister<TreeNode>;
template class ControlRegister<PropertyTreeNode>;
template class ControlRegister<PropertyRow>;
template class ControlRegister<DataTable>;
template class ControlRegister<DataTableHeader>;
template class ControlRegister<DataTableHeaderCell>;
template class ControlRegister<DataTableColumnGrip>;
template class ControlRegister<DataTableRow>;
template class ControlRegister<TreeTableExpander>;

}//namespace textured
}//namespace skin
}//namespace gweni
