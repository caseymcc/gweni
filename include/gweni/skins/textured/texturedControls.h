#ifndef _gweni_skins_texturedControls_h_
#define _gweni_skins_texturedControls_h_

#include "gweni/skins/textured/texturedPrimitives.h"
#include "gweni/skins/textured/texturedSkin.h"
#include "gweni/skins/textured/tokyoNight.h"

#include "gweni/controls/button.h"
#include "gweni/controls/curveEditor.h"
#include "gweni/controls/hexView.h"
#include "gweni/controls/nodeGraph.h"
#include "gweni/controls/timeline.h"

#include <algorithm>
#include <vector>

namespace gweni
{
namespace controls
{
class MenuItem;
namespace internal
{
class SliderBar;
}
}

namespace skin
{
namespace textured
{

class Base:public textured::ControlRegister<Base>
{
public:
    inline static const std::string_view name = "Base";

    static void staticInit(TexturedSkin *skin) {m_staticInit=true;}

    void update(renderer::Base *renderer, controls::Base *baseControl) override;
};

//! The opacity levels a skin Control's slots were last drawn at
//! (docs/architecture.md -> "Control opacity"). A Control that keeps what it
//! drew and draws again only what changed holds one, and draws every slot
//! again when moved() says its control is drawn at other levels now: a level
//! change marks its control StateChange_Background|StateChange_Text, which by
//! itself would redraw only what its own look says changed.
struct DrawnOpacity
{
    float surface=1.0f;
    float text=1.0f;

    //! Whether \p renderer's levels (the control's, as the drain set them)
    //! are not the ones last drawn at; takes them.
    bool moved(const renderer::Base *renderer)
    {
        const bool changed=(surface != renderer->surfaceOpacity()) || (text != renderer->textOpacity());

        surface=renderer->surfaceOpacity();
        text=renderer->textOpacity();
        return changed;
    }
};

struct Horizontal
{
    inline static const std::string_view value="Horizontal";
};
struct Vertical
{
    inline static const std::string_view value="Vertical";
};

//class Control
//{
//public:
//    virtual void generate(renderer::Base *render, controls::Base *baseControl)
//    {
//        generatePrimitives(renderer, m_requiredPrimitives);
//    };
//
//    virtual void remove(renderer::Base *render, controls::Base *baseControl)
//    {
//        releasePrimitives(renderer);
//    };
//
//    virtual void update(renderer::Base *render, controls::Base *baseControl)=0;
//
//protected:
//    void generatePrimitives(renderer::Base *renderer, size_t count)
//    {
//        m_primitives.resize(count);
//
//        for(size_t i=0; i<count; ++i)
//        {
//            renderer->generatePrimitive(m_primitiveIds[i]);
//        }
//    }
//    void releasePrimitives(renderer::Base *renderer, size_t count)
//    {
//        for(size_t i=0; i<m_primitiveIds.size(); ++i)
//        {
//            renderer->releasePrimitive(primitiveIds[i])
//        }
//    }
//
//    static m_requiredPrimitives;
//    std::vector<int> m_primitives;
//};

//! The Button surface for \p control's state: disabled (on or off), pressed
//! (Button::isDepressed()), on (a toggle's, hovered or not), hovered, focused
//! (\p focused: a SplitButton's parts draw the control's focus) or normal. Every renderer drawing a Button's look picks it here.
inline const tokyonight::Surface &buttonSurface(controls::Button *control, bool focused)
{
    // a toggle that is on keeps its look until a press or disabling it;
    // a plain button (LabelClickable included) is never on
    const bool on=control->isToggle() && control->getToggleState();

    if(control->isDisabled())
        return on?tokyonight::ButtonOnDisabled:tokyonight::ButtonDisabled;
    if(control->isDepressed())
        return tokyonight::ButtonDown;
    if(on)
        return control->isHovered()?tokyonight::ButtonOnHover:tokyonight::ButtonOn;
    if(control->isHovered())
        return tokyonight::ButtonHover;
    if(focused)
        return tokyonight::ButtonFocus;
    return tokyonight::ButtonNormal;
}

//! buttonSurface() with \p control's own focus.
inline const tokyonight::Surface &buttonSurface(controls::Button *control)
{
    return buttonSurface(control, control->isFocussed());
}

template<typename _Type>
class ButtonTemplate:public textured::ControlRegister<ButtonTemplate<_Type>>
{
public:
    size_t requiredPrimitives() override { return 1; }

    using ThisClass=textured::ControlRegister<ButtonTemplate<_Type>>;

    static void staticInit(TexturedSkin *skin)
    {
        ThisClass::m_staticInit=true;
    }

    void update(renderer::Base *renderer, controls::Base *baseControl) override
    {
        if(baseControl->getStateChange() == controls::StateChange_Nothing)
            return;

        controls::Button *control=static_cast<controls::Button *>(baseControl);

        tokyonight::paintSurface(renderer, ThisClass::m_primitives.data(), baseControl,
            buttonSurface(control));
    }

    static constexpr std::string_view ButtonLabel="Button";
    static inline auto name=join<_Type::value, ButtonLabel>;
};

struct DefaultButton
{
    inline static JoinConstExpr std::string_view value="";
};

//class Button:public textured::ControlRegister<Button>
class Button:public ButtonTemplate<DefaultButton>
{
    Button()
    {
        testName=name;
    }

    std::string testName;
};

//class Button:public textured::ControlRegister<Button>
//{
//public:
//    static void staticInit(TexturedSkin *skin);
//
//    size_t requiredPrimitives() override { return 9; }
//
//    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
//    void update(renderer::Base *renderer, controls::Base *baseControl) override;
//    void remove(renderer::Base *renderer, controls::Base *baseControl) override;
//
//    inline static const std::string_view name="Button";
//
//private:
//    static size_t Normal;
//    static size_t Hover;
//    static size_t Down;
//    static size_t Disabled;
//};

class Canvas:public textured::ControlRegister<Canvas>
{
public:
    static void staticInit(TexturedSkin *skin);

    size_t requiredPrimitives() override { return 1; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="Canvas";
};

class CategoryHeaderButton:public textured::ControlRegister<CategoryHeaderButton>
{
public:
    static void staticInit(TexturedSkin *skin);


    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="CategoryHeaderButton";

    static size_t Button;
    static BorderedPrimitive ButtonPrimitive;
};

class CollapsibleCategory:public textured::ControlRegister<CollapsibleCategory>
{
public:
    static void staticInit(TexturedSkin *skin);


    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="CollapsibleCategory";

    static size_t Background;

    static BorderedPrimitive BackgroundPrimitive;
};

class CategoryList:public textured::ControlRegister<CategoryList>
{
public:
    static void staticInit(TexturedSkin *skin);


    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="CategoryList";

    static size_t Outer;
    static size_t Inner;
    static size_t Header;

    static BorderedPrimitive OuterPrimitive;
    static BorderedPrimitive InnerPrimitive;
    static BorderedPrimitive HeaderPrimitive;
};

class CategoryButton:public textured::ControlRegister<CategoryButton>
{
public:
    static void staticInit(TexturedSkin *skin);

    size_t requiredPrimitives() override { return 1; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="CategoryButton";

    static Color Normal;
    static Color Selected;
    static Color Hover;
    static Color AltNormal;
    static Color AltSelected;
    static Color AltHover;
};

class CheckBox:public textured::ControlRegister<CheckBox>
{
public:
    static void staticInit(TexturedSkin *skin) {}

    size_t requiredPrimitives() override { return 2; }
    // the check icon is drawn at getZIndex()+1, over the box
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="CheckBox";
};

class ComboBox:public textured::ControlRegister<ComboBox>
{
public:
    static void staticInit(TexturedSkin *skin);

    size_t requiredPrimitives() override { return 2; }
    // the chevron is drawn at getZIndex()+1, over the box
    size_t depthLayers() override { return 2; }

    //void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    //void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="ComboBox";

    static size_t Normal;
    static size_t Hover;
    static size_t Down;
    static size_t Disabled;

    static BorderedPrimitive NormalPrimitive;
    static BorderedPrimitive HoverPrimitive;
    static BorderedPrimitive DownPrimitive;
    static BorderedPrimitive DisabledPrimitive;
};

// Drawn as a ComboBox (paintComboBox): the Button surface for its state and
// the chevron at z+1.
class SearchComboBox:public textured::ControlRegister<SearchComboBox>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }
    // the chevron is drawn at getZIndex()+1, over the box
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="SearchComboBox";
};

// A row of a SearchComboBox's list, drawn as a MenuItem: [0] the MenuHover
// surface while it shows the highlighted option, its slot hidden otherwise;
// [1] a check (icon kind 0) at the row's right end, at z+1, on an option a
// multi-select box has picked.
class SearchComboBoxRow:public textured::ControlRegister<SearchComboBoxRow>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }
    // the check is drawn at getZIndex()+1, over the highlight
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="SearchComboBoxRow";
};

class DockBase:public textured::ControlRegister<DockBase>
{
public:
    static void staticInit(TexturedSkin *skin);


    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="DockBase";
};

class GroupBox:public textured::ControlRegister<GroupBox>
{
public:
    static void staticInit(TexturedSkin *skin);


    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="GroupBox";

    static size_t Normal;
    static BorderedPrimitive NormalPrimitive;
};


struct InputButton
{
    inline static JoinConstExpr std::string_view value="Input.Button.";
};

struct InputButtonUp
{
    inline static JoinConstExpr std::string_view value="Input.UpDown.Up.";
};

struct InputButtonDown
{
    inline static JoinConstExpr std::string_view value="Input.UpDown.Down.";
};

class Input:public textured::ControlRegister<Input>
{
public:
    static void staticInit(TexturedSkin *skin);

    size_t requiredPrimitives() override { return 0; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="Input";

    ButtonTemplate<InputButton> m_buttonInput;
    ButtonTemplate<InputButtonUp> m_buttonUp;
    ButtonTemplate<InputButtonDown> m_buttonDown;
};

class ListBox:public textured::ControlRegister<ListBox>
{
public:
    static void staticInit(TexturedSkin *skin);


    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="ListBox";

    static size_t Background;
    static size_t Hovered;

//    static size_t EvenLine;
//    static size_t OddLine;
//    static size_t EvenLineSelected;
//    static size_t OddLineSelected;

    static BorderedPrimitive BackgroundPrimitive;
    static BorderedPrimitive HoveredPrimitive;
};

// A VirtualList's backdrop, the ListBox's; its rows draw themselves.
class VirtualList:public textured::ControlRegister<VirtualList>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="VirtualList";
};

class ListBoxRow:public textured::ControlRegister<ListBoxRow>
{
public:
    static void staticInit(TexturedSkin *skin);


    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="ListBoxRow";

    static size_t EvenLine;
    static size_t OddLine;
    static size_t EvenLineSelected;
    static size_t OddLineSelected;

    static BorderedPrimitive EvenLinePrimitive;
    static BorderedPrimitive OddLinePrimitive;
    static BorderedPrimitive EvenLineSelectedPrimitive;
    static BorderedPrimitive OddLineSelectedPrimitive;
};

class Menu:public textured::ControlRegister<Menu>
{
public:
    static void staticInit(TexturedSkin *skin);


    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="Menu";

    static size_t RightArrow;
    static size_t Check;

    static size_t Strip;
    static size_t Background;
    static size_t BackgroundWithMargin;
    static size_t Hover;

    static BorderedPrimitive BackgroundPrimitive;
};

// The bar a MenuStrip's items sit in: flat and square (tokyonight::MenuStrip),
// found before Menu's rounded panel by the type walk.
class MenuStrip:public textured::ControlRegister<MenuStrip>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="MenuStrip";
};

// A menu row: nothing of its own while idle or disabled (slot [0] hidden, so the
// menu's backdrop shows), tokyonight::MenuHover while hovered, pressed or with
// its submenu open. Found before Button's by the type walk.
// Slot [1] is the check of a checkable, checked item: a style 5 checkmark
// (kind 0) at getZIndex()+1, in the item's text colour (so it follows hover and
// dims when disabled), centred in the icon column, the item's left text padding
// (Menu::onAddItem's 24, or Button::setImage's, where the image it hides was).
// Otherwise the slot is hidden. Two items have no column and so draw no check:
// those on a MenuStrip itself (onStrip()) and those in a menu with
// iconMarginDisabled() (a left text padding of 0).
class MenuItem:public textured::ControlRegister<MenuItem>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }
    // the check is drawn at getZIndex()+1, over the highlight
    size_t depthLayers() override { return 2; }

    //! The check's rect in the item's render bounds: a square of 6/10 of the
    //! icon column's smaller side, centred in it; empty when there is no column.
    static gweni::Rect checkRect(controls::MenuItem *item);

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="MenuItem";
};

//! Flat path links: one surface slot. The current, nonclickable item draws
//! no surface; Label's child Text supplies its caption.
class BreadcrumbItem:public textured::ControlRegister<BreadcrumbItem>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }
    size_t requiredPrimitives() override { return 1; }
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    inline static const std::string_view name="BreadcrumbItem";
};

//! The ellipsis in place of collapsed items: drawn as a path item (one
//! surface slot, BreadcrumbItemDown while its menu is open), no chevron.
class BreadcrumbEllipsis:public textured::ControlRegister<BreadcrumbEllipsis>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }
    size_t requiredPrimitives() override { return 1; }
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    inline static const std::string_view name="BreadcrumbEllipsis";
};

//! One right-chevron slot, hidden when the separator shows Label's text.
class BreadcrumbSeparator:public textured::ControlRegister<BreadcrumbSeparator>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }
    size_t requiredPrimitives() override { return 1; }
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    inline static const std::string_view name="BreadcrumbSeparator";
};

// A DropDownButton: the Button surface of its state in slot [0]
// (buttonSurface: its open menu is its pressed look), and in slot [1] the down
// chevron (style 5, kind 1, direction 0) in its text colour at
// DropDownButton::chevronRect, at getZIndex()+1 over the surface.
class DropDownButton:public textured::ControlRegister<DropDownButton>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }
    // the chevron is drawn at getZIndex()+1, over the surface
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="DropDownButton";
};

// A ToolBar's background: the ToolBar surface over its bounds.
class ToolBar:public textured::ControlRegister<ToolBar>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="ToolBar";
};

// A flat tool button: toolButtonSurface() of its state in its one slot, its
// image and text drawing themselves.
class ToolButton:public textured::ControlRegister<ToolButton>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="ToolButton";
};

// A toolbar's overflow button: [0] toolButtonSurface() of its state (pressed
// while its menu is open), [1] the down chevron (icon kind 1) centred in it in
// its text colour at getZIndex()+1.
class ToolBarOverflowButton:public textured::ControlRegister<ToolBarOverflowButton>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }
    // the chevron is drawn at getZIndex()+1, over the surface
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="ToolBarOverflowButton";
};

// A SplitButton draws the Divider-unit column between its parts in its one
// slot, the SplitButtonDivider surface's top colour (flat, square).
class SplitButton:public textured::ControlRegister<SplitButton>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 1; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="SplitButton";
};

// A SplitButton's main part: the Button surface of its state (the
// SplitButton's focus as its own) square on its right with no border there,
// so it joins the divider and the arrow part.
class SplitButtonMain:public textured::ControlRegister<SplitButtonMain>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 1; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="SplitButtonMain";
};

// A SplitButton's arrow part: the Button surface of its state (pressed while
// the menu is open) square on its left with no border there in slot [0], the
// down chevron centred in it in its text colour at getZIndex()+1 in slot [1].
class SplitButtonArrow:public textured::ControlRegister<SplitButtonArrow>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }
    // the chevron is drawn at getZIndex()+1, over the surface
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="SplitButtonArrow";
};

// The chevron a MenuItem with a submenu shows at its right: one style 5 icon
// pointing right, in the item's text colour (MenuItem::updateColors repaints it
// when that changes). It is the item's child, so it is numbered after the
// item's surface and needs no depth layer of its own.
class RightArrow:public textured::ControlRegister<RightArrow>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="RightArrow";
};

class Panel:public textured::ControlRegister<Panel>
{
public:
    static void staticInit(TexturedSkin *skin);


    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="Panel";

    static size_t Normal;
    static size_t Bright;
    static size_t Dark;
    static size_t Highlight;

    static BorderedPrimitive NormalPrimitive;
};

// Simple container/panel renderers: draw a single bordered background from the
// skin atlas. One per control type so each self-registers under its own name.
class PageControl:public textured::ControlRegister<PageControl>
{
public:
    static void staticInit(TexturedSkin *skin);
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    inline static const std::string_view name="PageControl";
    static size_t Background;
    static BorderedPrimitive BackgroundPrimitive;
};

class TabControlInner:public textured::ControlRegister<TabControlInner>
{
public:
    static void staticInit(TexturedSkin *skin);
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    inline static const std::string_view name="TabControlInner";
    static size_t Background;
    static BorderedPrimitive BackgroundPrimitive;
};

class CollapsibleList:public textured::ControlRegister<CollapsibleList>
{
public:
    static void staticInit(TexturedSkin *skin);
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    inline static const std::string_view name="CollapsibleList";
    static size_t Background;
    static BorderedPrimitive BackgroundPrimitive;
};

class CategoryInner:public textured::ControlRegister<CategoryInner>
{
public:
    static void staticInit(TexturedSkin *skin);
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    inline static const std::string_view name="CategoryInner";
    static size_t Background;
    static BorderedPrimitive BackgroundPrimitive;
};

// Renderers for controls that intentionally have no background (text labels,
// layout/splitter/strip containers). They register and draw nothing, so they
// don't trip the red "unskinned" fallback in the Base renderer.
#define GWENI_TRANSPARENT_RENDERER(Class)                                      \
class Class:public textured::ControlRegister<Class>                            \
{                                                                              \
public:                                                                        \
    static void staticInit(TexturedSkin *) { m_staticInit=true; }              \
    size_t requiredPrimitives() override { return 0; }                         \
    void update(renderer::Base *, controls::Base *) override {}                \
    inline static const std::string_view name=#Class;                          \
};

GWENI_TRANSPARENT_RENDERER(Label)
GWENI_TRANSPARENT_RENDERER(Resizer)
GWENI_TRANSPARENT_RENDERER(Table)
GWENI_TRANSPARENT_RENDERER(TabStrip)
// a source-mode DataTable's body, a VirtualList the table's backdrop shows
// through, as through the row-mode DataTableBody (which draws nothing)
GWENI_TRANSPARENT_RENDERER(DataTableVirtualBody)
// a SearchComboBox's options list, a VirtualList the Menu it fills paints
// the backdrop behind
GWENI_TRANSPARENT_RENDERER(SearchComboBoxOptions)
// a NodeGraph's layers and a Node's parts, which the Node's renderer draws
GWENI_TRANSPARENT_RENDERER(NodeGraphLayer)
GWENI_TRANSPARENT_RENDERER(NodeTitleBar)
GWENI_TRANSPARENT_RENDERER(NodePinColumn)
GWENI_TRANSPARENT_RENDERER(NodeBody)

// Visual controls this skin does not draw yet. Each is an empty renderer on
// purpose, so Base::setSkin() does not report the type as having none (#185),
// and each names the idea that tracks its real renderer: drop the line when
// that renderer lands. A drawsNothing() override is not the place for these,
// since the controls are meant to draw.
#define GWENI_PENDING_RENDERER(Class) GWENI_TRANSPARENT_RENDERER(Class)
// the Sequencer's clips, keys, playhead and ruler: #644's step 3
GWENI_PENDING_RENDERER(Sequencer)
#undef GWENI_PENDING_RENDERER
#undef GWENI_TRANSPARENT_RENDERER

// A TreeNode's connector lines, in the skin's Tree.Lines colour, at the node's
// own z, under its rows: [0] the stub, on a node that is not a root, from its
// parent's line across the indent to its toggle (to its title if it has no
// toggle), [1] on an open node with child nodes, the line down from under its
// toggle to its last child's stub. Each is an icon of kind 6, as a
// Separator's rule, so every line is equally thick at any scale; a line that
// is not drawn has its slot hidden.
class TreeNode:public textured::ControlRegister<TreeNode>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="TreeNode";
};

// A PropertyTree's category: no tree lines, but its rail in the skin's
// Properties.Border colour, [0] the column down its left, as wide as its
// properties are indented, [1] the band behind its title, right of the column.
class PropertyTreeNode:public textured::ControlRegister<PropertyTreeNode>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="PropertyTreeNode";
};

// A PlotHistogram: [0] its frame (PlotFrame, over the render bounds), then one
// slot per bar (PlotBar, PlotBarHover for the hovered one), each at
// getZIndex()+1, in front of the frame. update() resizes the slot list to the
// bar count itself (requiredPrimitives() only sizes the first generate()), and
// keeps what it last drew in each slot: on a frame that marks the control's
// geometry, visibility or creation it draws everything, otherwise only a bar
// whose rect, colour or z is not the one drawn, so a changed value rewrites its
// bar's slot alone. A bar with no rect is hidden.
class PlotHistogram:public textured::ControlRegister<PlotHistogram>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 1; }
    // the bars are drawn at getZIndex()+1, over the frame
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="PlotHistogram";

private:
    //! the levels its slots were drawn at
    DrawnOpacity m_drawnOpacity;
    //! What a slot holds, as last drawn.
    struct Drawn
    {
        bool valid=false;   //!< false for a new slot, which holds nothing known
        bool shown=false;
        Rect rect;
        tokyonight::Surface surface{};
        int zIndex=0;
    };

    void resizePrimitives(renderer::Base *renderer, size_t count);

    //! [0] the frame, [1+i] bar i
    std::vector<Drawn> m_drawn;
};

// A PlotLines: [0] its frame (PlotFrame, over the render bounds), then one
// slot per segment (drawLine, from sample i to sample i+1, in PlotLine's top
// colour and the plot's line thickness), each at getZIndex()+1, in front of
// the frame. As PlotHistogram's, update() resizes the slot list to the
// segment count itself and keeps what it last drew in each slot: on a frame
// that marks the control's geometry, visibility or creation it draws
// everything, otherwise only a segment whose endpoints, neighbours, thickness,
// colour or z are not the ones drawn. Each segment is drawn with its
// neighbours (drawLine's prev and next: samples i-1 and i+2, none past the
// series or across a gap), so a joint's pixels are blended once, and a changed
// sample rewrites the four segments around it (i-2..i+1). A segment that is not
// drawn (a NaN endpoint) is hidden, and the ones next to it are drawn again
// without that neighbour.
class PlotLines:public textured::ControlRegister<PlotLines>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 1; }
    // the segments are drawn at getZIndex()+1, over the frame
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="PlotLines";

private:
    //! the levels its slots were drawn at
    DrawnOpacity m_drawnOpacity;
    //! What a segment's slot holds, as last drawn.
    struct Segment
    {
        bool valid=false;   //!< false for a new slot, which holds nothing known
        bool shown=false;
        PointF p0;
        PointF p1;
        bool hasPrev=false; //!< drawn with the point before p0 (prev)
        bool hasNext=false; //!< and the one after p1 (next)
        PointF prev;
        PointF next;
        float thickness=0.0f;
        Color color;
        int zIndex=0;
    };

    //! What the frame's slot holds, as last drawn.
    struct Frame
    {
        bool valid=false;
        bool shown=false;
        Rect rect;
        tokyonight::Surface surface{};
        int zIndex=0;
    };

    Frame m_frame;
    //! [i] segment i, in slot [1+i]
    std::vector<Segment> m_segments;
};

// A Knob: [0] its body, a disc of KnobBody (KnobBodyHover, KnobBodyFocus,
// KnobBodyPressed or KnobBodyDisabled by state) at getZIndex(); [1] the track
// arc (KnobTrack's top) at z+1 and [2] the value arc over it (KnobValue's
// top, KnobValueDisabled's while disabled) at z+2, each one drawArc in
// controls::Knob::Geometry::arcRect, round-capped; [3] the indicator
// (KnobIndicator's top) on the body at z+1, and last one slot per tick
// (KnobTick's top) at z+1, so a new tick count resizes the list's tail
// alone; the parts are placed by controls::Knob::getGeometry().
//
// The value arc runs from the arc's origin to the value, hidden while they
// are one (a zero sweep would draw its two caps as a dot). Each slot keeps
// what it last drew (an arc by its angles' slot bits, arcStartBits and
// arcSweepBits): a frame that marks the control's geometry, visibility or
// creation, or draws at other opacity levels, draws every slot, any other
// only one whose arc, line or look is not the one drawn, so a value change
// writes the value arc and the indicator, and hover, press and focus
// rewrite the body alone.
class Knob:public textured::ControlRegister<Knob>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 1; }
    // the track, the ticks and the indicator at z+1, the value arc at z+2
    size_t depthLayers() override { return 3; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="Knob";

    //! What a line slot holds, as last drawn.
    struct LineSlot
    {
        bool valid=false;   //!< false for a new slot, which holds nothing known
        bool shown=false;
        PointF p0;
        PointF p1;
        bool hasPrev=false;
        bool hasNext=false;
        PointF prev;
        PointF next;
        float thickness=0.0f;
        Color color;
        int zIndex=0;
    };

private:
    //! Draws \p slot's line unless it holds it already (\p all: draws it).
    void drawLine(renderer::Base *renderer, size_t index, bool all, const PointF &p0,
        const PointF &p1, const PointF *prev, const PointF *next, float thickness,
        const Color &color, int z);
    //! Draws arc slot \p index (0 the track, 1 the value) unless it holds it
    //! already (\p all: draws it).
    void drawArc(renderer::Base *renderer, size_t index, bool all, const Rect &rect, float startTurns,
        float sweepTurns, float thickness, const Color &color, int z);
    void hideArc(renderer::Base *renderer, size_t index, bool all);

    DrawnOpacity m_drawnOpacity;
    struct Body
    {
        bool valid=false;
        bool shown=false;
        Rect rect;
        tokyonight::Surface surface{};
        int zIndex=0;
    };
    Body m_body;
    //! What an arc slot holds, as last drawn.
    struct ArcSlot
    {
        bool valid=false;
        bool shown=false;
        Rect rect;
        uint16_t start=0;   //!< arcStartBits
        uint16_t sweep=0;   //!< arcSweepBits, clockwise from the start
        float thickness=0.0f;
        Color color;
        int zIndex=0;
    };
    //! [0] the track, slot [1]; [1] the value arc, slot [2]
    ArcSlot m_arcs[2];
    //! [i] slot [3+i]'s line: the indicator, then the ticks
    std::vector<LineSlot> m_lines;
};

// A RichText: one text run per piece of its layout (text::RichLayout), in its
// span's font and colour, at getZIndex()+1, and a list of primitive slots that
// follows the pieces: each Code piece's background (RichTextCode over the
// piece's box) at getZIndex(), behind the glyphs, then each underlined piece's
// underline (a link's included) and each struck piece's line, icon kind 6
// rules in the piece's colour, at getZIndex()+2, over them, then a
// RichTextSelection rect per line of the selection (selectionRects()) at
// getZIndex()+1, behind the glyphs (a run's glyph i is at z+1+(i+1)/(n+1)) and
// over the code backgrounds, last so a selection change shifts no other slot.
// update() resizes
// both lists to what the layout holds (a run or slot past it given back, one
// new taken) and keeps what it last drew in each slot: on a frame that marks
// the control's geometry, visibility or creation it draws every slot,
// otherwise only one whose rect, look or z is not the one drawn, and a run is
// drawn again only when its string, font, colour, position or z changed (all
// of them on such a frame, for their clip boxes), so a changed span or a new
// width writes this paragraph's runs and slots alone, and of those only what
// moved.
class RichText:public textured::ControlRegister<RichText>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    // nothing up front: the lists follow the layout
    size_t requiredPrimitives() override { return 0; }
    // the code backgrounds at getZIndex(), the glyphs at getZIndex()+1, the
    // lines over them at getZIndex()+2
    size_t depthLayers() override { return 3; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;
    //! its slots are primitive ids, its runs text ids
    void appendIds(std::vector<size_t> &primitiveIds, std::vector<size_t> &textIds) const override
    {
        primitiveIds.insert(primitiveIds.end(), m_primitives.begin(), m_primitives.end());
        textIds.insert(textIds.end(), m_texts.begin(), m_texts.end());
    }

    inline static const std::string_view name="RichText";

private:
    //! the levels its slots were drawn at
    DrawnOpacity m_drawnOpacity;
    //! What a slot holds, as last drawn: a surface (a code background) or a rule
    struct Drawn
    {
        bool valid=false;   //!< false for a new slot, which holds nothing known
        bool shown=false;
        bool rule=false;
        Rect rect;
        tokyonight::Surface surface{};
        Color color;
        int zIndex=0;
    };

    //! What a run holds, as last drawn
    struct Run
    {
        bool valid=false;   //!< false for a new run, which holds nothing known
        bool shown=false;
        gweni::String text;
        gweni::Font font;
        Color color;
        gweni::Point pos;
        int zIndex=0;
    };

    std::vector<Drawn> m_drawn;
    //! a text id per piece, and what it holds
    std::vector<size_t> m_texts;
    std::vector<Run> m_runs;
};

// A Markdown horizontal rule (controls::internal::MarkdownRule): [0] an icon
// of kind 6 (a Separator's rule) across its bounds, through their middle, in
// MarkdownRule's top colour.
class MarkdownRule:public textured::ControlRegister<MarkdownRule>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="MarkdownRule";
};

// A Markdown block quote (controls::internal::MarkdownQuote): [0] its bar, the
// MarkdownQuoteBar surface down its left edge, MarkdownQuote::BarWidth units
// wide and as high as the quote.
class MarkdownQuote:public textured::ControlRegister<MarkdownQuote>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="MarkdownQuote";
};

// A Markdown code block (controls::internal::MarkdownCodeBlock): [0] its
// background, the MarkdownCodeBlock surface over its bounds.
class MarkdownCodeBlock:public textured::ControlRegister<MarkdownCodeBlock>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="MarkdownCodeBlock";
};

// A Markdown image paragraph (controls::internal::MarkdownImage): [0] its box,
// the MarkdownImagePlaceholder surface over MarkdownImage::getBoxSize() at its
// top-left, in the Box look only (hidden in the Alt look, whose alt text draws
// its own placeholder, and in the Image look, the ImagePanel drawing there).
class MarkdownImage:public textured::ControlRegister<MarkdownImage>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="MarkdownImage";
};

// A Markdown table (controls::internal::MarkdownTable): [0] the header row's
// background (MarkdownTableHeader) inside its lines, then a line per row edge
// across the columns and one per column edge down the rows, each an icon of
// kind 6 (a Separator's rule) in MarkdownTableBorder's top colour at
// getZIndex()+1, over the header. The slot list follows the row and column
// counts, and a slot is drawn again only when its rect, look or z moved, so a
// cell's edit that keeps the grid writes none of them.
class MarkdownTable:public textured::ControlRegister<MarkdownTable>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 0; }
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="MarkdownTable";

private:
    //! the levels its slots were drawn at
    DrawnOpacity m_drawnOpacity;
    struct Drawn
    {
        bool valid=false;
        bool shown=false;
        bool rule=false;
        bool vertical=false;
        Rect rect;
        tokyonight::Surface surface{};
        Color color;
        int zIndex=0;
    };
    std::vector<Drawn> m_drawn;
};

// A NodeGraph: [0] its backdrop (GraphGrid's fill, over the render bounds) and
// [1] its grid at getZIndex()+1 over it, one drawGrid over the render bounds: a
// line every NodeGraph::gridStep() graph units from the graph's origin on each
// axis, 1 unit thick at the canvas's scale whatever the zoom, in GraphGrid's
// border colour or, every NodeGraph::GridMajorEvery-th line, GraphGridMajor's
// top. Both slots are taken when the graph is made, before any of its
// content's, so they are drawn first: a node's anti-aliased edge drawn over a
// line blends with it whatever the zoom has made of the grid.
class NodeGraph:public textured::ControlRegister<NodeGraph>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }
    // the grid is drawn at getZIndex()+1, over the backdrop
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="NodeGraph";
};

// A Chart: [0] its frame (ChartFrame, over the render bounds, at its z), then
// slot lists it sizes itself to what it draws, each slot keeping what it last
// drew (as PlotLines' do):
// - the grid: a drawLine per grid line, at the ticks of each shown value axis
//   with its grid on and of the x axis with setXGrid(), in ChartGrid's top
//   colour, then the line along the plot's bottom in ChartAxis's, at z+1;
// - one list per series, at z+3+i: a bar or stacked bar series' bars (one
//   drawRoundedRect per point, its getBarRect() in ChartSeries<colour>, a point
//   with no bar hidden), or a line or step line's segments (one drawLine per
//   segment of its vertices, with its neighbours, in the surface's top colour
//   and the series' thickness; a segment beside a NaN hidden), or an area's
//   fill: one slot per getAreaSegment(), a drawTrapezoid in ChartArea<colour>'s
//   top colour from its line down to its base, or for a step area a
//   drawRoundedRect of the step's rect in whole units, ChartArea<colour>; a
//   segment with none hidden;
// - one list per area, at z+3+S+k for the k-th area of the S series: its line,
//   drawn as a line series' segments are, in front of every series' fill.
// The grid, spans, series and markers are drawn in the plot's clip region (the control's
// own region cut to getPlotBounds()), and a segment is cut on the CPU to a box
// a little wider than the plot first, so a point out of range is clipped,
// never clamped, and no quad reaches past what a vertex holds. A slot is drawn
// again only when what it would draw (its rect or endpoints and neighbours,
// colour, thickness, z, clip box) differs, unless the frame marks the chart's
// geometry, visibility or creation: a changed point rewrites its bar (its
// stack's bars above it when they move) or the segments around it, an
// appended one its own slots and its joint. A hidden series' slots are hidden.
// - the spans: one drawRoundedRect per span, its getSpanRect() in ChartSpan,
//   at z+2, under the series;
// - the markers: one vertical drawLine per marker across the plot
//   (getMarkerLine()), 1 unit thick in ChartMarker's top colour, at z+3+S,
//   in front of the series; a marker or span outside the plot is hidden;
// - the crosshair: one vertical drawLine across the plot at the hovered x
//   (getCrosshairLine()), 1 unit thick in ChartCrosshair's top colour, at
//   z+4+S, hidden with no hover.
// The slots draw in id order, and a partly covered pixel of a slot in front
// writes depth that a slot behind it, drawn later, cannot pass. So a list's
// ids come after the frame's and after every list behind it: the lists are
// taken back to front, each with capacity() slots for what it draws, the
// spare ones hidden, and when one needs more than it has (an appended point,
// a new series, more grid lines, spans or markers), or fewer than it was
// taken for (a series gone, fewer grid lines), every list is given back and
// taken again back to front, as a chart built with what it draws takes them,
// that frame drawing them all again.
// The z values are the frame's, the grid's (z+1), the spans' (z+2), one per
// series (z+3+i), one per area's line (z+3+S+k), the markers' (z+3+S+A) and
// the crosshair's (z+4+S+A), so depthLayersOf() is 5+S+A for S series of
// which A are areas; the chart calls zOrderChanged() when a series comes or
// goes. The lists are taken in that order: grid, spans, the series' lists,
// the areas' lines, markers, crosshair.
class Chart:public textured::ControlRegister<Chart>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    static constexpr int GridLayer=1;
    static constexpr int SpanLayer=2;
    static constexpr int SeriesLayer=3;

    size_t requiredPrimitives() override { return 1; }
    size_t depthLayersOf(const controls::Base *control) override;

    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;
    void appendIds(std::vector<size_t> &primitiveIds, std::vector<size_t> &textIds) const override;

    inline static const std::string_view name="Chart";

    //! The grid's slots in use, and series \p series's (empty past the
    //! series), as last drawn; a list's spare slots (see capacity()) are not
    //! in them.
    std::vector<size_t> gridPrimitives() const { return inUse(m_grid); }
    std::vector<size_t> spanPrimitives() const { return inUse(m_spans); }
    std::vector<size_t> markerPrimitives() const { return inUse(m_markers); }
    std::vector<size_t> crosshairPrimitives() const { return inUse(m_crosshair); }
    std::vector<size_t> seriesPrimitives(size_t series) const;
    //! An area's line's slots in use (empty for a series that is no area).
    std::vector<size_t> areaLinePrimitives(size_t series) const;

    //! The slots a list is taken with for \p count in use: half as many
    //! again and at least 4 spare, hidden, so a list growing a slot at a time
    //! is taken again (every list, see update()) once per half again. The
    //! crosshair's list is its one slot.
    static size_t capacity(size_t count) { return count ? count+std::max<size_t>(count/2, 4) : 0; }

    //! An area's trapezoid has its edges' heights held this many units above
    //! and below the plot (its quad would not fit the vertex's range for a
    //! point far out of a fixed range); only a point that far out bends the
    //! edge's slope on the plot.
    static constexpr double AreaReach=4096.0;

private:
    //! the levels its slots were drawn at
    DrawnOpacity m_drawnOpacity;
    //! What a slot holds, as last drawn: a rect, a segment or a trapezoid.
    struct Slot
    {
        bool valid=false;   //!< false for a new slot, which holds nothing known
        bool shown=false;
        bool line=false;
        //! a trapezoid: its sides' x and its line's y at them in p0 and p1,
        //! its base's y at them in prev
        bool trapezoid=false;
        Rect rect;
        tokyonight::Surface surface{};
        PointF p0;
        PointF p1;
        bool hasPrev=false;
        bool hasNext=false;
        PointF prev;
        PointF next;
        float thickness=0.0f;
        Color color;
        int zIndex=0;
        Rect clip;
    };

    //! A list's slots: the first \p used drawn, the rest spare and hidden;
    //! \p taken the count it was taken for.
    struct SlotList
    {
        std::vector<size_t> ids;
        std::vector<Slot> drawn;
        size_t used=0;
        size_t taken=0;
    };

    static std::vector<size_t> inUse(const SlotList &list);
    static void releaseList(renderer::Base *renderer, SlotList &list);
    //! Every list given back and taken again back to front, each with
    //! capacity() for its count (\p series one per series), its ids above
    //! the frame's.
    void takeLists(renderer::Base *renderer, size_t grid, size_t spans, const std::vector<size_t> &series,
        const std::vector<size_t> &areaLines, size_t markers);

    Slot m_frame;
    SlotList m_grid;
    SlotList m_spans;
    std::vector<SlotList> m_series;
    //! per series, its line's list if it is an area (empty otherwise)
    std::vector<SlotList> m_areaLines;
    //! update()'s counts of each series' fill slots and area line slots,
    //! kept so a frame allocates nothing for them
    std::vector<size_t> m_seriesNeed;
    std::vector<size_t> m_lineNeed;
    SlotList m_markers;
    SlotList m_crosshair;
};

// What a LaneView's renderer (the Timeline's, the Sequencer's) shares: the
// slots of its lists, each keeping what it last drew (a rect or a line), and
// the lanes' stripes and the grid drawn from the view.
namespace lanes
{

//! What a slot holds, as last drawn: a rect or a line.
struct Slot
{
    bool valid=false;   //!< false for a new slot, which holds nothing known
    bool shown=false;
    bool line=false;
    Rect rect;
    tokyonight::Surface surface{};
    PointF p0;
    PointF p1;
    Color color;
    float thickness=1.0f;
    int zIndex=0;
    Rect clip;
};

struct SlotList
{
    std::vector<size_t> ids;
    std::vector<Slot> drawn;
    size_t used=0;
    size_t taken=0;
};

//! The slots in use as last drawn (the spare ones are not in them).
GWENI_EXPORT std::vector<size_t> inUse(const SlotList &list);
//! Every slot of \p list given back.
GWENI_EXPORT void releaseList(renderer::Base *renderer, SlotList &list);
//! The slots a list is taken with for \p count in use (Chart's).
inline size_t capacity(size_t count) { return count ? count+std::max<size_t>(count/2, 4) : 0; }

//! Takes lists back to front: each list's ids from the bank, which hands out
//! the lowest free id first, so what it gives back goes up from list to list;
//! one at or below \p above (the control's own first slot) is put aside until
//! finish() gives it back.
class GWENI_EXPORT Taker
{
public:
    Taker(renderer::Base *renderer, bool hasAbove, size_t above): m_renderer(renderer), m_hasAbove(hasAbove),
        m_above(above) {}
    //! \p list taken with \p count slots, \p used of them to be in use.
    void take(SlotList &list, size_t used, size_t count);
    void finish();

private:
    renderer::Base *m_renderer;
    bool m_hasAbove;
    size_t m_above;
    std::vector<size_t> m_below;
};

//! One update()'s drawing: a slot is drawn only when what it holds differs
//! (or with \p all), and hidden only when it showed.
class GWENI_EXPORT Painter
{
public:
    Painter(renderer::Base *renderer, bool all): m_renderer(renderer), m_all(all) {}

    void hide(size_t id, Slot &slot);
    //! The slots past the list's used ones.
    void hideSpare(SlotList &list);
    void drawRect(size_t id, Slot &slot, const Rect &rect, const tokyonight::Surface &surface, int zIndex,
        const Rect &clip);
    void drawSegment(size_t id, Slot &slot, const PointF &p0, const PointF &p1, const Color &color, int zIndex,
        const Rect &clip, float thickness=1.0f);

    //! The lanes' stripes (\p stripes, a slot per lane: an odd lane's
    //! TimelineLane across the caption column and the lane area, at
    //! z+\p laneLayer clipped to \p viewClip; an even lane's and one out of
    //! view hidden) and the grid (\p grid, ticks+2 slots: a vertical line per
    //! tick across the area in TimelineGrid's top colour, clipped to
    //! \p areaClip, then the axis lines along the area's top, over the caption
    //! column too, and its left edge in TimelineAxis's, at z+\p gridLayer), of
    //! \p view drawn at \p bounds (its render bounds).
    void paintLanes(const controls::LaneView &view, const Rect &bounds, int z, int laneLayer, int gridLayer,
        const Rect &areaClip, const Rect &viewClip, SlotList &stripes, SlotList &grid);

private:
    renderer::Base *m_renderer;
    bool m_all;
};

//! \p rect of a control at \p bounds, at the renderer's offset, cut to its
//! clip region.
GWENI_EXPORT Rect clipTo(renderer::Base *renderer, const Rect &bounds, const Rect &rect);

}//namespace lanes

// A Timeline: [0] its frame (TimelineFrame, over the render bounds, at its z),
// then slot lists it sizes to what it draws, each slot keeping what it last
// drew (as a Chart's):
// - the lanes: one slot per lane, an odd lane's TimelineLane stripe across
//   the caption column and the bar area (getLaneRect() widened left over the
//   column), at z+1, clipped to the two (the body's viewport); an even lane's
//   slot, and that of a lane out of view, hidden;
// - the grid: a vertical drawLine per time tick across the bar area in
//   TimelineGrid's top colour, then the axis lines along the area's top (over
//   the caption column too) and its left edge in TimelineAxis's, at z+2;
// - one list per lane of its bars: a drawRoundedRect per bar, its
//   getBarRect() in its surface (TimelineBar, the named one, or its colour
//   with TimelineBar's border), at z+3 plus its overlap level, clipped to the
//   bar area; a bar with no rect (out of range, its lane out of view) hidden;
// - the hover: one drawRoundedRect over getHoverRect(), the hovered bar's
//   fill with TimelineBarHover's blended over it by its alpha and
//   TimelineBarHover's border, at z+4+MaxOverlapLevel, hidden with no hover.
// The lanes, grid and bars are drawn again only when the timeline's
// getBarsSerial() moved, the bar area's clip box changed, or the frame marks
// its geometry, visibility, creation or text (a theme edit's mark,
// ThemeEditor::applyToCanvas(): the timeline itself marks no text, so a hover
// does not), and then each slot only when its
// rect, surface, z or clip differs: a hover writes the hover slot alone, a
// changed bar its own slot, a scroll the lanes and bars that moved, came into
// view or left it. Every slot is square (TimelineBar's radius is 0) at whole
// units, so it covers its device pixels fully, and the hover look is as
// opaque as its bar: no slot's order against another's changes a pixel of an
// opaque bar. A bar's slot follows its index in its lane: an added bar takes a
// spare slot of its lane's list.
// The lists are taken back to front (lanes, grid, each lane's bars, the
// hover), each with capacity() slots for what it draws, the spare ones
// hidden, and every list is given back and taken again when one needs more
// than it has, a lane's bars fewer than half what its list was taken for, the
// lanes or grid lines fewer than theirs (Chart's rule), or the lane count
// changes: an addLane() or removeLane() rewrites every slot. A list taken
// alone could get ids below the stripes' and the grid's, and a translucent
// bar drawn before them would hide them behind it (the depth test), so the
// order back to front is kept by taking them all; lanes come and go rarely
// (a host's agent slots).
class Timeline:public textured::ControlRegister<Timeline>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    static constexpr int LaneLayer=1;
    static constexpr int GridLayer=2;
    static constexpr int BarLayer=3;
    static constexpr int HoverLayer=BarLayer+controls::Timeline::MaxOverlapLevel+1;

    size_t requiredPrimitives() override { return 1; }
    // the frame, the lanes, the grid, a layer per overlap level and the hover
    size_t depthLayers() override { return size_t(HoverLayer)+1; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;
    void appendIds(std::vector<size_t> &primitiveIds, std::vector<size_t> &textIds) const override;

    inline static const std::string_view name="Timeline";

    //! The slots in use as last drawn (a list's spare slots are not in
    //! them): the lanes', the grid's, lane \p lane's bars' (bar i's slot is
    //! the i-th; empty past the lanes) and the hover's.
    std::vector<size_t> lanePrimitives() const { return inUse(m_lanes); }
    std::vector<size_t> gridPrimitives() const { return inUse(m_grid); }
    std::vector<size_t> barPrimitives(size_t lane) const;
    std::vector<size_t> hoverPrimitives() const { return inUse(m_hover); }

    //! The slots a list is taken with for \p count in use (Chart's).
    static size_t capacity(size_t count) { return lanes::capacity(count); }

    //! What a bar is drawn in: TimelineBar, its colour with TimelineBar's
    //! border, or the theme surface it names (magenta for a name no theme has).
    static tokyonight::Surface barSurface(const controls::TimelineBar &bar);

private:
    //! the levels its slots were drawn at
    DrawnOpacity m_drawnOpacity;
    typedef lanes::Slot Slot;
    typedef lanes::SlotList SlotList;

    static std::vector<size_t> inUse(const SlotList &list) { return lanes::inUse(list); }
    void takeLists(renderer::Base *renderer, size_t laneCount, size_t grid, const std::vector<size_t> &bars);

    Slot m_frame;
    SlotList m_lanes;
    SlotList m_grid;
    std::vector<SlotList> m_bars;
    SlotList m_hover;
    //! what the lanes, grid and bars were last drawn from
    bool m_drawnAll=false;
    unsigned m_drawnSerial=0;
    Rect m_drawnAreaClip;
    Rect m_drawnViewClip;
};

// A Link: one slot per segment of its polyline (Link::getPoints()), a drawLine
// at the link's z in its colour (LinkHover's top while hovered, NodeSelected's
// border while selected, else the link's own or the from pin's) and its
// thickness, each drawn with its neighbours, so its joints are blended once. The
// slots are taken when the link is first drawn, as many as its style can draw
// (Link::MaxSegments for a Bezier, 5 for an Orthogonal link, 1 for a Straight
// one), so the count a zoom picks for a Bezier never takes a slot after its
// nodes'; those it does not use are hidden. Before drawLine, each segment is cut
// on the CPU to the graph's visible rect (its view in graph units, a few units
// wider), so no quad reaches past the +/-32 K device px a vertex holds and a
// link to a node far off screen still draws its visible part; a segment wholly
// outside is hidden, and a cut joint is drawn without that neighbour. As
// PlotLines', only a segment whose endpoints, neighbours, thickness, colour or z
// are not the ones its slot holds is drawn, unless the frame marks the link's
// geometry, visibility or creation.
class Link:public textured::ControlRegister<Link>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="Link";

    //! The slots a link of \p style draws its segments in.
    static size_t slotsFor(controls::LinkStyle style);

private:
    //! the levels its slots were drawn at
    DrawnOpacity m_drawnOpacity;
    //! What a segment's slot holds, as last drawn.
    struct Segment
    {
        bool valid=false;   //!< false for a new slot, which holds nothing known
        bool shown=false;
        PointF p0;
        PointF p1;
        bool hasPrev=false;
        bool hasNext=false;
        PointF prev;
        PointF next;
        float thickness=0.0f;
        Color color;
        int zIndex=0;
    };

    std::vector<Segment> m_segments;
};

// A NodeGraph's marquee outline: [0..3] the rect's sides
// (NodeGraphMarquee::getRect()), drawLines in Marquee's border colour at the
// marquee's z over everything in the graph, each other's neighbours, 1 canvas
// unit thick on screen at any zoom; hidden while the rect is empty.
class NodeGraphMarquee:public textured::ControlRegister<NodeGraphMarquee>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 4; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="NodeGraphMarquee";
};

// The marquee's fill: [0] its rect in Marquee's top and bottom, translucent,
// with no border, under the nodes; hidden while the rect is empty.
class NodeGraphMarqueeFill:public textured::ControlRegister<NodeGraphMarqueeFill>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="NodeGraphMarqueeFill";
};

// A CurveEditor's plot (controls::internal::CurvePlot), everything its editor
// shows, in the plot's clip box (its bounds cut to the editor's region), in
// slot lists taken back to front, each with spare hidden slots:
// - [0] the background, CurveEditorBackground over the render bounds, at z;
// - the grid: a vertical drawLine per time tick and a horizontal one per
//   value tick, at the minor step (axis::niceStep for lines GridMinGap units
//   apart) in CurveEditorGrid's top colour and the major step (LabelGapX and
//   LabelGapY apart) in CurveEditorGridMajor's, LineThickness thick (as the
//   curves: 2 device px at scale 1), at z+1; a
//   label per major tick (axis::formatValue), its own text run in the skin's
//   CurveEditor.TickText, the time labels along the bottom edge and the value
//   labels along the left one, at z+1, so the grid is behind them and the
//   curves in front;
// - the marquee's fill (CurveMarquee's top, translucent) at z+2;
// - per curve, at z+3+its index, its extensions (a horizontal drawLine from
//   the plot's left edge to its first key and from its last key to the right
//   edge, at their values) and its spans: each span the mode's most segments
//   (slotsForShape(): a Hermite MaxHermiteSegments, a Linear 1, a Constant
//   2), using as many as its on-screen length asks (segmentsFor()), the rest
//   hidden; each a drawLine of LineThickness in the curve's surface's top
//   colour with its neighbours along the curve (across spans and the
//   extensions too), cut to the plot on the CPU first (a cut joint drawn
//   without that neighbour, a segment wholly outside hidden);
// - per curve, a disc per key (KeySize, CurveKey, CurveKeyHover or
//   CurveKeySelected), at z+3+curves; a key outside the plot is hidden;
// - the tangent handles of a key that shows them: two stems (drawLine in
//   CurveTangent's top colour) at z+4+curves and two discs (HandleSize,
//   CurveTangent) at z+5+curves;
// - the marquee's outline, its four sides in CurveMarquee's border colour, at
//   z+6+curves.
// What changed comes from the editor's log (CurveEditor::pendingEdits()):
// a span or key splice moves the slot lists with it (a span or key keeps its
// slots while it is kept, a new one takes spare ones, a gone one's are hidden
// and kept spare) and marks what it inserted and the spans next to it, a look
// change marks its key; only what is marked is drawn, and each slot only
// when what it would hold differs from what it holds, so a key drag writes
// the segments of the spans it changed, its disc and its handles. A frame
// that marks the plot's geometry, visibility, creation or text (a theme
// edit, ThemeEditor::applyToCanvas()), a new view (viewSerial()), opacity
// levels or the editor's pendingAll() draws every
// slot of what it reaches. When a list needs more than its spare slots, or a
// curve comes or goes, every list is given back and taken again (Chart's
// rule), so the slots stay in id order back to front: slots draw in id order,
// and one taken after a slot in front of it would lose the pixels under that
// slot's anti-aliased edge.
class CurvePlot:public textured::ControlRegister<CurvePlot>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    static constexpr int MaxHermiteSegments=16;
    static constexpr int MinHermiteSegments=4;
    //! A Hermite span's segments: one per this many units of its chord.
    static constexpr float SegmentLength=10.0f;
    static constexpr float LineThickness=1.5f;
    static constexpr float GridMinGap=12.0f;
    static constexpr float LabelGapX=64.0f;
    static constexpr float LabelGapY=32.0f;
    //! Layers above z: the grid, the fill, then the curves', then the keys,
    //! the stems, the handles' discs and the outline.
    static constexpr size_t FixedLayers=7;

    size_t requiredPrimitives() override { return 1; }
    size_t depthLayersOf(const controls::Base *control) override;

    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;
    void appendIds(std::vector<size_t> &primitiveIds, std::vector<size_t> &textIds) const override;

    inline static const std::string_view name="CurvePlot";

    static size_t slotsForShape(Curve::SpanShape shape);
    //! The segments a span of \p shape whose ends are \p chord units apart
    //! on screen is drawn in.
    static int segmentsFor(Curve::SpanShape shape, float chord);

    //! For tests: span \p span's slots (all of them, used or not), key \p
    //! key's disc, its handles' four (none while it shows no handles), the
    //! grid's, the marquee's, the labels' text ids; every list's ids in the
    //! order taken.
    std::vector<size_t> spanPrimitives(size_t curve, size_t span) const;
    std::vector<size_t> extensionPrimitives(size_t curve) const;
    size_t keyPrimitive(size_t curve, size_t key) const;
    std::vector<size_t> handlePrimitives(size_t curve, size_t key) const;
    std::vector<size_t> gridPrimitives() const { return m_grid.ids; }
    std::vector<size_t> marqueePrimitives() const;
    std::vector<size_t> labelTexts() const;
    std::vector<size_t> takenOrder() const;
    //! How often every list was given back and taken again.
    size_t takeCount() const { return m_takes; }

private:
    DrawnOpacity m_drawnOpacity;

    //! What a slot holds, as last drawn: a rect or a segment.
    struct Slot
    {
        bool valid=false;   //!< false for a new slot, which holds nothing known
        bool shown=false;
        bool line=false;
        Rect rect;
        tokyonight::Surface surface{};
        PointF p0;
        PointF p1;
        bool hasPrev=false;
        bool hasNext=false;
        PointF prev;
        PointF next;
        float thickness=0.0f;
        Color color;
        int zIndex=0;
        Rect clip;
    };

    struct Span
    {
        std::vector<size_t> ids;
        std::vector<Slot> drawn;
    };

    struct Key
    {
        size_t id=0;
        Slot drawn;
        //! the handle set it holds (m_handles), or none
        size_t handles=SIZE_MAX;
    };

    struct CurveSlots
    {
        size_t ext[2]={0, 0};
        Slot extDrawn[2];
        std::vector<Span> spans;
        std::vector<Key> keys;
        //! hidden slots of the curve's segment and key lists
        std::vector<size_t> segmentSpare;
        std::vector<size_t> keySpare;
    };

    struct HandleSet
    {
        size_t ids[4]={0, 0, 0, 0};
        Slot drawn[4];
        bool used=false;
    };

    struct List
    {
        std::vector<size_t> ids;
        std::vector<Slot> drawn;
    };

    struct Label
    {
        size_t id=0;
        bool shown=false;
        bool valid=false;
        String text;
        Point pos;
        Rect clip;
        Color color;
    };

    //! Every id given back and every list taken again for \p editor.
    void takeAll(renderer::Base *renderer, const controls::CurveEditor *editor);
    void releaseAll(renderer::Base *renderer);
    //! The editor's log replayed onto curve \p index's lists; false when a
    //! list ran out of spare slots.
    bool replay(renderer::Base *renderer, const controls::CurveEditor *editor, size_t index,
        std::vector<size_t> &spans, std::vector<size_t> &keys);
    //! Span \p span's slot count set for its shape from the curve's spare ones.
    bool fitSpan(renderer::Base *renderer, CurveSlots &slots, const Curve &curve, size_t span);

    Slot m_background;
    List m_grid;
    size_t m_fill=0;
    Slot m_fillDrawn;
    std::vector<CurveSlots> m_curves;
    std::vector<HandleSet> m_handles;
    size_t m_outline[4]={0, 0, 0, 0};
    Slot m_outlineDrawn[4];
    std::vector<Label> m_labels;
    bool m_taken=false;
    size_t m_takes=0;
    uint64_t m_viewSerial=0;
    Point m_plotSize;
    //! the ids in the order takeAll() took them
    std::vector<size_t> m_order;
    //! update()'s scratch lists, kept so a frame allocates nothing for them
    std::vector<std::vector<size_t>> m_dirtySpans;
    std::vector<std::vector<size_t>> m_dirtyKeys;
    std::vector<CurvePoint> m_samples;
    std::vector<PointF> m_points;
    std::vector<PointF> m_before;
    std::vector<PointF> m_after;
};

// A Node: [0] its frame (NodeBody, NodeSelected while it is selected, over
// Node::getFrameRect()), [1] its title bar at getZIndex()+1 over the frame's
// top, NodeTitle's fill or the node's title colour (its bottom a shade darker),
// with the frame's border, radius and border width, square and borderless at
// its bottom, so frame and title are one outline.
class Node:public textured::ControlRegister<Node>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }
    // the title bar is drawn at getZIndex()+1, over the frame
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="Node";
};

// A Pin: [0] its marker over Pin::getMarkerRect(), in the pin's colour (PinIn's
// or PinOut's top unless the app gave it one): a circle (a rounded rect of half
// its side's radius) or a square (PinIn's or PinOut's radius), filled while
// connected and otherwise a ring of their borderWidth around their bottom
// fill; a triangle pointing right (icon kind 7, or 8, its outline, until it is
// connected). Hovered, the marker is a unit larger each way and its colour a
// third of the way to white.
class Pin:public textured::ControlRegister<Pin>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="Pin";
};

class ProgressBar:public textured::ControlRegister<ProgressBar>
{
public:
    static void staticInit(TexturedSkin *skin);

    // [0] the back (track), [1] the front (the fill, or an indeterminate
    // bar's moving segment).
    size_t requiredPrimitives() override { return 2; }
    // the fill is drawn at getZIndex()+1, over the track
    size_t depthLayers() override { return 2; }

    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="ProgressBar";

    static size_t Back;
    static size_t Front;

    static BorderedPrimitive BackgroundPrimitive;
    static BorderedPrimitive FrontPrimitive;

};

// A Separator's rule, and so a MenuDivider's (it derives from Separator and has
// no renderer of its own): [0] the whole rule or, with a caption, the part
// before it, [1] the part after the caption. Each part is an icon of kind 6
// (drawIcon), stroked max(1, snap(1)) whole device px through the middle of its
// quad, so a rule is equally thick wherever it lands on the pixel grid; the
// quad reaches one unit past the separator on both sides across the rule,
// enough room for the stroke where the separator itself snaps thinner.
class Separator:public textured::ControlRegister<Separator>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="Separator";
};

// A PropertyRow: [0] the label column's fill, from the row's left to the
// label's right edge, less its last unit column and the bottom unit row,
// [1] the 1-unit separator in that last column, between the label and the
// property, [2] the 1-unit line along the row's bottom (the property's own
// frame covers it in the value column). The fill is a surface, PropertyColumn
// at rest, PropertyColumnHover while the row or anything in it is hovered and
// PropertyColumnEditing while its editor has the focus (Editing wins); the
// lines are the skin's Properties.Line_Normal/Line_Hover/Line_Selected. Each
// line is an icon of kind 6, as a Separator's rule, so every row's line is
// equally thick at any scale, drawn at getZIndex()+1, in front of the fill.
class PropertyRow:public textured::ControlRegister<PropertyRow>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 3; }
    // the lines are drawn at getZIndex()+1, over the fill
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="PropertyRow";
};

// A CodeEditor's field: [0] CodeEditor, CodeEditorFocus while it has the
// focus, [1] the caret at the overlay z, in front of every row, while the
// editor has the focus and the caret is lit and in view. Its rows draw the
// lines.
class CodeEditor:public textured::ControlRegister<CodeEditor>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }
    size_t overlayLayers() override { return 1; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="CodeEditor";
};

// A LogView's field: [0] LogView, LogViewFocus while it has the focus. Its
// rows draw the text, a block row's chevron its icon.
class LogView:public textured::ControlRegister<LogView>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="LogView";
};

// A LogView's row: a slot per rect of its selection highlight
// (LogViewRow::getSelectionRects(): its bounds when selected whole, else one
// per wrapped line the selection covers) at its text's z, behind every glyph
// (LogViewSelection); [0] hidden while the row has none, the slots past the
// rects given back. Its Text draws the text, a block's chevron its icon.
class LogViewRow:public textured::ControlRegister<LogViewRow>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="LogViewRow";

private:
    //! The highlight's rects, kept for their capacity.
    std::vector<gweni::Rect> m_rects;
};

// A LogView block row's chevron: [0] icon kind 1, right while the block is
// collapsed and down while expanded, in the row's text colour.
class LogViewChevron:public textured::ControlRegister<LogViewChevron>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="LogViewChevron";
};

// A CodeEditor's line-number gutter: [0] its backdrop (CodeGutter); its
// numbers draw themselves (Labels).
class CodeGutter:public textured::ControlRegister<CodeGutter>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="CodeGutter";
};

// A CodeEditor's row: [0] its line background (CodeCurrentLine on the line
// with the caret, else a CodeView diff row's DiffAdded, DiffRemoved or
// DiffHunk, hidden otherwise), then a selection rect per visual row the
// selection covers at the text's z, behind every glyph (CodeSelection; [1]
// hidden while the line has none, the slots past the rects given back); the
// row's Texts draw the line.
class CodeLine:public textured::ControlRegister<CodeLine>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="CodeLine";

private:
    //! The selection's rects, kept for their capacity.
    std::vector<gweni::Rect> m_rects;
};

// A HexView's row: per column (hex, then ASCII) a slot for each of its
// HexRowMarks pieces, behind the row's Texts: the highlights (at most
// HexRowMarks::MaxHighlights, at the row's z), the selection (z+1), the
// hovered byte (z+2) and the cursor's box (z+3), so the cursor shows over the
// hover over the selection over a highlight where they meet whatever the
// slots' ids; a piece the row has not is a hidden slot. A fixed count a row.
class HexViewRow:public textured::ControlRegister<HexViewRow>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    static constexpr size_t SlotsPerColumn=3+controls::HexRowMarks::MaxHighlights;
    size_t requiredPrimitives() override { return 2*SlotsPerColumn; }
    size_t depthLayers() override { return 4; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="HexViewRow";
};

// A Selectable's row: hidden while idle (or disabled and unselected), a
// surface for hover, press and selection; the label's Text draws the string.
class Selectable:public textured::ControlRegister<Selectable>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="Selectable";
};

class NavigationRail:public textured::ControlRegister<NavigationRail>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    inline static const std::string_view name="NavigationRail";

private:
    //! the levels its slots were drawn at
    DrawnOpacity m_drawnOpacity;
    tokyonight::Surface m_surface{};
    bool m_surfaceCurrent=false;
};

//! One SDF slot for the row and leading selection border. Idle is hidden.
//! The ImagePanel and Label are children, numbered above this background.
class NavigationItem:public textured::ControlRegister<NavigationItem>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    inline static const std::string_view name="NavigationItem";
};

// A SegmentedButtons segment: [0] its fill (Segment, SegmentHover,
// SegmentDown, SegmentOn, SegmentOnHover, SegmentDisabled,
// SegmentOnDisabled), rounded only at the row's ends; and [1] the check icon
// (kind 0) at getZIndex()+1 in getCheckRect(), shown while it is selected in a
// row that shows checks (SegmentCheck, SegmentDisabled's text colour while
// disabled). A segment after the first starts on its left neighbour's right
// border column, which the neighbour draws: it is drawn from a Joint further
// right, with no border on its left side, so every joint is one border column
// at any scale and a segment's state never repaints its neighbours. The
// segment holding the keyboard focus has SegmentFocus's border, and after the
// first [2] paints its left joint in it: a rect of no width at the joint's
// right edge with NoBorderLeft and the border's width as its reach, so it
// covers exactly the neighbour's border pixels at any scale (hidden
// otherwise).
class SegmentedButton:public textured::ControlRegister<SegmentedButton>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 3; }
    // the check is drawn at getZIndex()+1, over the fill
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="SegmentedButton";
};

// A Stepper's step: [0] its indicator circle (StepperIndicator, ...Current,
// ...Done, ...Error, ...Disabled by its look, each but Disabled's ...Hover
// while a clickable step is hovered, StepperIndicatorFocus's border while it
// has the focus), [1] the check (icon kind 0) at getZIndex()+1 while it shows
// as done, and a text run of its own at getZIndex()+1 for the number or "!"
// (StepperNumber), hidden while done. The title and captions are its Texts.
class StepperStep:public textured::ControlRegister<StepperStep>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }
    // the check and the number are drawn at getZIndex()+1, over the circle
    size_t depthLayers() override { return 2; }

    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;
    void appendIds(std::vector<size_t> &primitiveIds, std::vector<size_t> &textIds) const override
    {
        ControlRegister<StepperStep>::appendIds(primitiveIds, textIds);
        if(m_hasText)
            textIds.push_back(m_textId);
    }

    inline static const std::string_view name="StepperStep";

private:
    bool m_hasText=false;
    bool m_textShown=false;
    size_t m_textId=0;
};

// A Stepper's connector: one 1-unit rule (icon kind 6) through its middle,
// horizontal or vertical, in StepperConnector's or StepperConnectorDone's top
// colour.
class StepperConnector:public textured::ControlRegister<StepperConnector>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="StepperConnector";
};

// controls::Calendar's frame: one slot, the Calendar surface. Its header,
// weekday row and cells are children that draw themselves.
class Calendar:public textured::ControlRegister<Calendar>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="Calendar";
};

// A Calendar's title: hidden at rest, CalendarTitleHover and CalendarTitleDown
// while hovered and pressed; its Text draws the month.
class CalendarTitle:public textured::ControlRegister<CalendarTitle>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="CalendarTitle";
};

// A Calendar cell: one slot, hidden while the cell is idle (no hover, press,
// selection, range, today or cursor), else the surface its look picks (see
// tokyoNight.h's Calendar surfaces) inset a unit from the cell's edges but on
// the sides where a range joins it to its neighbour, which are square and
// reach the edge, so a range is one strip along each row. Today's and the
// cursor's rings are that slot's border, so a cell is one slot in any look and
// a hover writes it alone. Its Text draws the number.
class CalendarCell:public textured::ControlRegister<CalendarCell>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="CalendarCell";
};

// A Chart's legend row: [0] its background as a Selectable's (hidden at rest,
// SelectableHover and SelectableDown; a legend row is never selected), and [1]
// its swatch at getZIndex()+1, ChartLegendRow::getSwatchRect() in its series'
// ChartSeries<colour> (fill and border at a quarter of their alpha while the
// series is hidden). Its Text draws the name.
class ChartLegendRow:public textured::ControlRegister<ChartLegendRow>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }
    // the swatch is drawn at getZIndex()+1, over the background
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="ChartLegendRow";
};

// A TreeNode's label, a flat list row as a ListBox's: hidden at rest (and while
// disabled), ListItemHover while hovered, ListItemSel while selected (the
// title's toggle state); the Button's Text draws the string.
class TreeNodeText:public textured::ControlRegister<TreeNodeText>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="TreeNodeText";
};

// A DataTable's backdrop, the ListBox's; its header and rows draw themselves.
class DataTable:public textured::ControlRegister<DataTable>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="DataTable";
};

// A DataTable's header strip: [0] its fill (DataTableHeader), [1] the rule
// along its bottom, an icon of kind 6 as a Separator's, at getZIndex()+1.
class DataTableHeader:public textured::ControlRegister<DataTableHeader>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }
    // the rule is drawn at getZIndex()+1, over the fill
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="DataTableHeader";
};

// A column's header cell: [0] a surface while hovered or pressed
// (DataTableHeaderHover, DataTableHeaderDown), hidden at rest, [1] the sort
// chevron (icon kind 1, up ascending, down descending) in the cell's right
// column at getZIndex()+1, hidden while the column is not sorted.
class DataTableHeaderCell:public textured::ControlRegister<DataTableHeaderCell>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }
    // the chevron is drawn at getZIndex()+1, over the hover surface
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="DataTableHeaderCell";
};

// A header cell's resize grip: [0] the column's rule along the cell's right
// edge (a vertical icon of kind 6), in the Separator colour, Blue while hovered
// or dragged.
class DataTableColumnGrip:public textured::ControlRegister<DataTableColumnGrip>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="DataTableColumnGrip";
};

// A DataTable's body row: [0] its fill, the Selectable's surfaces for hover,
// press and selection and, idle, DataTableStripe on a striped odd row or
// nothing (the slot hidden), [1] the rule along its bottom with Rows borders,
// [2...] one rule at each column's right edge but the last with Columns
// borders. The rules are icons of kind 6 at getZIndex()+1, over the fill; the
// slot list follows the column count (update() resizes it).
class DataTableRow:public textured::ControlRegister<DataTableRow>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }
    // the rules are drawn at getZIndex()+1, over the fill
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="DataTableRow";

private:
    void resizePrimitives(renderer::Base *renderer, size_t count);
};

// A Bullet's circle, in its text colour; the label's Text draws the string.
class Bullet:public textured::ControlRegister<Bullet>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="Bullet";
};

class RadioButton:public textured::ControlRegister<RadioButton>
{
public:
    static void staticInit(TexturedSkin *skin);

    size_t requiredPrimitives() override { return 2; }
    // the dot is drawn at getZIndex()+1, over the ring
    size_t depthLayers() override { return 2; }

    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="RadioButton";

    static size_t ActiveNormal;
    static size_t ActiveChecked;

    static BorderedPrimitive NormalPrimitive;
};

// A scroll bar's arrow (and TabControl's overflow buttons): [0] the surface,
// hidden at rest so the thumb is the one filled shape on the bar, [1] the
// chevron at getZIndex()+1 pointing the way the button scrolls.
class ScrollBarButton:public textured::ControlRegister<ScrollBarButton>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }
    // the arrow is drawn at getZIndex()+1, over the button
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="ScrollBarButton";
};

// The window title-bar buttons (WindowControl's close button, WindowCanvas's
// minimise and maximise): [0] the surface, hidden while it has nothing to show
// (at rest and disabled), [1] the glyph, an icon of drawIcon's kinds 2-5 at
// getZIndex()+1 over a fixed GlyphSize square centred in the button. The three
// share WindowButtonPainter::paint() and differ in their surfaces and glyph.
class WindowButtonPainter
{
public:
    static constexpr int GlyphSize=10;

    static void paint(renderer::Base *renderer, const std::vector<size_t> &primitives,
        controls::Base *baseControl, const tokyonight::Surface &hover,
        const tokyonight::Surface &down, const gweni::Color &activeGlyph, int kind);
};

class WindowCloseButton:public textured::ControlRegister<WindowCloseButton>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }
    // the glyph is drawn at getZIndex()+1, its Text child after it
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="WindowCloseButton";
};

class WindowMaximizeButton:public textured::ControlRegister<WindowMaximizeButton>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="WindowMaximizeButton";
};

class WindowMinimizeButton:public textured::ControlRegister<WindowMinimizeButton>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="WindowMinimizeButton";
};

// A TreeNode's expand toggle: a surface only on hover or press, and a chevron
// pointing right while the node is closed and down while it is open.
class OpenToggleButton:public textured::ControlRegister<OpenToggleButton>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }
    // the chevron is drawn at getZIndex()+1, its Text child after it
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="OpenToggleButton";
};

// A TreeTable row's expander: [0] a surface only while hovered or pressed
// (TreeTableExpanderHover, TreeTableExpanderDown), [1] a chevron (icon kind
// 1) at getZIndex()+1, right while the node is collapsed and down while it is
// expanded, in the TreeTableExpander* text colours.
class TreeTableExpander:public textured::ControlRegister<TreeTableExpander>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }
    // the chevron is drawn at getZIndex()+1, its Text child after it
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="TreeTableExpander";
};

// A DateEdit's calendar button: [0] a surface only while hovered or pressed
// (DateEditButtonHover, DateEditButtonDown), [1] a down chevron (icon
// kind 1) at getZIndex()+1 in the DateEditButton* text colours.
class DateEditButton:public textured::ControlRegister<DateEditButton>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }
    // the chevron is drawn at getZIndex()+1
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="DateEditButton";
};


// The track of both bars (VerticalScrollBar and HorizontalScrollBar find it
// through their parent type): one ScrollTrack surface the whole length of the
// bar, under its arrows and thumb.
class BaseScrollBar:public textured::ControlRegister<BaseScrollBar>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="BaseScrollBar";
};

// A scroll bar's thumb: ScrollThumb, ScrollThumbHover while hovered and
// ScrollThumbDown while it is dragged, in one slot.
class ScrollBarBar:public textured::ControlRegister<ScrollBarBar>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="ScrollBarBar";
};

class Shadow:public textured::ControlRegister<Shadow>
{
public:
    static void staticInit(TexturedSkin *skin);


    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="Shadow";

    static size_t Background;
    static BorderedPrimitive BackgroundPrimitive;
};

// The track and one tick per notch position (getNotchCount()+1 of them, only
// while the slider clamps to its notches). With a value text
// (controls::Slider::setValueFormat) a horizontal track is a field the
// thumb's height and the slider's length, the ticks below it, and a vertical
// one is drawn in the slider's track column (getTrackBounds()); the text is
// the slider's own Text child. The tick count can change after
// generate(), so update() grows or shrinks this control's own slot list to
// 1+ticks itself; requiredPrimitives() is only the track.
class Slider:public textured::ControlRegister<Slider>
{
public:
    static void staticInit(TexturedSkin *skin) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 1; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="Slider";

    //! Thumb size across the slider's axis (it is never drawn thicker than its bounds).
    static constexpr int ThumbThickness=20;
    static constexpr int TrackThickness=4;
    static constexpr int TickLength=4;

    //! The track rect in a track column `r` with a thumb `bar` (a field the
    //! thumb's height with `field`); RangeSlider's renderer draws its track
    //! with it too.
    static gweni::Rect trackRect(const gweni::Rect &r, const gweni::Rect &bar, bool horizontal, bool field,
        size_t ticks);
    //! The `ticks` notch ticks in `ids`, for `notches` notches.
    static void paintTicks(renderer::Base *renderer, const size_t *ids, controls::Base *baseControl,
        const gweni::Rect &r, const gweni::Rect &track, const gweni::Rect &bar, bool horizontal, bool field,
        int notches, size_t ticks);

private:
    void resizePrimitives(renderer::Base *renderer, size_t count);
};

// The slider's thumb, controls::internal::SliderBar: its own child control, so
// it sits one z above the track it moves along.
class SliderBar:public textured::ControlRegister<SliderBar>
{
public:
    static void staticInit(TexturedSkin *skin) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 1; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="SliderBar";

    //! Shared Slider/RangeSlider thumb look and cross-axis centring.
    static const tokyonight::Surface &thumbSurface(controls::internal::SliderBar *bar);
    static Rect thumbRect(Rect bounds, bool horizontal);
};

// controls::RangeSlider: a Slider's track (a field with a value text, as a
// Slider's), the range between its thumbs' centres filled over it at z+1
// (RangeSliderFill, RangeSliderFillDisabled), and a tick per notch position
// while clamped to them, the slot list grown or shrunk to 2+ticks in
// update(). The two thumbs are their own controls (RangeSliderThumb below)
// but draw in this control's z+2 and z+3, the front thumb
// (controls::RangeSlider::getFrontThumb(), the one dragged or focused last) at
// z+3, so it is over the other where they overlap; depthLayers() is 4.
class RangeSlider:public textured::ControlRegister<RangeSlider>
{
public:
    static void staticInit(TexturedSkin *skin) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }
    size_t depthLayers() override { return 4; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="RangeSlider";
};

// A RangeSlider's thumb: the SliderBar's surfaces by state, a focused thumb's
// border in RangeSliderThumbFocus's colour and width, drawn at its slider's
// z+2, or z+3 while it is the front thumb.
class RangeSliderThumb:public textured::ControlRegister<RangeSliderThumb>
{
public:
    static void staticInit(TexturedSkin *skin) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 1; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="RangeSliderThumb";
};

// controls::ColorLerpBox, the saturation/value box: one bilinear fill (white
// top-left, the hue top-right, black along the bottom) in the ColorField
// surface, and the cursor marker one z above it.
class ColorLerpBox:public textured::ControlRegister<ColorLerpBox>
{
public:
    static void staticInit(TexturedSkin *skin) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }
    // the cursor marker is drawn at getZIndex()+1, over the field
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="ColorLerpBox";

    static constexpr int MarkerSize=8;
};

// controls::ColorSlider, the vertical hue strip: hue->RGB is linear over each
// 60 degree arc, so the strip is six 2-stop vertical gradients (red, yellow,
// green, cyan, blue, magenta, red) edge to edge at getZIndex(), each stop the
// strip's colour at that edge, plus the selection bar at getZIndex()+1.
class ColorSlider:public textured::ControlRegister<ColorSlider>
{
public:
    static void staticInit(TexturedSkin *skin) { m_staticInit=true; }

    // [0..5] the segments, top to bottom, [6] the selection bar (by slot order)
    size_t requiredPrimitives() override { return 7; }
    // the bar is drawn at getZIndex()+1, over the strip
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="ColorSlider";

    static constexpr int Inset=5;
    static constexpr int MarkerHeight=6;
};

// controls::internal::ColorDisplay, the colour swatch: one primitive in the
// ColorSwatch surface's radius and border, a flat fill of the colour, or (alpha
// below 255, checkers on) the colour over a ColorChecker checkerboard. Both are
// opaque, so the swatch does not depend on what is drawn behind it.
class ColorDisplay:public textured::ControlRegister<ColorDisplay>
{
public:
    static void staticInit(TexturedSkin *skin) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 1; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="ColorDisplay";

    // The swatch in \p slot: \p color, over a ColorChecker checkerboard when
    // \p checker and translucent, in \p frame's radius, border width and
    // border colour. Shared with ColorButton.
    static void paint(renderer::Base *renderer, size_t slot, controls::Base *baseControl,
        const gweni::Color &color, bool checker, const tokyonight::Surface &frame);
};

// controls::ColorButton: ColorDisplay's swatch in one primitive, its border
// ColorSwatch's at rest and ColorButtonHover/Down/Disabled's otherwise, so
// a hover or a press rewrites that one slot.
class ColorButton:public textured::ControlRegister<ColorButton>
{
public:
    static void staticInit(TexturedSkin *skin) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 1; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="ColorButton";
};

// A GradientEditor's bar: [0] ColorChecker's cells under the whole bar,
// framed in GradientBarFrame's border (a checker of a transparent fill), then
// at getZIndex()+1, each the bar's whole height and drawing the frame's
// border on its top and bottom and on a side that meets the bar's edge
// (drawBilinearRect's edge mask: square and borderless where two spans
// meet), so the border is max(1, snap(width)) device px at any scale,
// [1] a flat span of the
// first stop's colour from the bar's left to it and [2] one of the last
// stop's colour from it to the bar's right (each hidden when it has no
// width), and [3+i] the span from stop i to stop i+1, a bilinear fill of
// stop i's colour on its left edge to stop i+1's on its right (flat in stop
// i's colour in Constant mode, hidden when the two stops share a column). A
// stop at t is at the bar's left plus round(t*width), the column its handle
// is centred on. update() resizes the slot list to the stop count (two stops
// a span) and draws every slot each time the bar is marked; a slot whose
// bytes stay is not written, so moving or recolouring a stop writes the spans
// on its sides (and the flat end when it is the first or the last) alone.
class GradientBar:public textured::ControlRegister<GradientBar>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 3; }
    // the spans are drawn at getZIndex()+1, over the checkerboard
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="GradientBar";
};

// A GradientEditor stop's handle: [0] its marker, a filled triangle (icon
// kind 7) pointing up at the bar over the handle's top MarkerHeight units, in
// the state surface's top colour, and [1] under it a swatch of the stop's
// colour over ColorChecker's cells, bordered in the state surface's border:
// GradientStop at rest, GradientStopHover hovered, GradientStopSelected for
// the selected stop (or while dragged), GradientStopFocus with the keyboard
// focus, GradientStopDisabled in a disabled editor. A hover, a selection or a
// new colour rewrites the handle's own slots alone.
class GradientStopHandle:public textured::ControlRegister<GradientStopHandle>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="GradientStopHandle";
};

// controls::DragNumber: its field in one primitive (DragNumber at rest,
// DragNumberHover hovered, DragNumberFocus with the keyboard focus,
// DragNumberDown while pressed, DragNumberDisabled), hidden while its edit
// box covers it; the label's Text draws the value. A hover, a press or the
// focus rewrites that one slot.
class DragNumber:public textured::ControlRegister<DragNumber>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 1; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="DragNumber";
};

// controls::internal::VectorFieldTag: its block in one primitive, the axis's
// VectorTagX..VectorTagW surface (DragNumberDisabled while disabled); the
// label's Text draws the letter.
class VectorFieldTag:public textured::ControlRegister<VectorFieldTag>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 1; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="VectorFieldTag";
};

// controls::Badge: its bubble in one primitive (the Badge surface, as round as
// its height allows) and its number in a text run of its own, both at the
// badge's getAttachedZIndex(), so they are in front of its anchor's later
// siblings; a dot is the bubble alone.
// controls::Chip: one primitive, its tone's or palette entry's surface (the
// ChipNeutral .. ChipPalette7 surfaces), as round as its height allows; a
// clickable chip's hover and press move the fill toward the border, a
// disabled one is ButtonDisabled. Its Label's Text draws the text, its
// ChipRemoveButton the x.
class Chip:public textured::ControlRegister<Chip>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 1; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="Chip";
};

// controls::Switch: its track (the SwitchTrack* surfaces by state, its border
// SwitchFocus's while focused) and its thumb (SwitchThumbOff/On/Disabled, a
// circle at getZIndex()+1, over the track), each as round as its height
// allows. The thumb's look follows the state alone, so a hover writes the
// track's slot. Its Label's Text draws the label.
class Switch:public textured::ControlRegister<Switch>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }
    // the thumb is drawn at getZIndex()+1, over the track
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="Switch";
};

// controls::Spinner: its arc (SpinnerArc's top colour, or SpinnerStopped's as a
// whole still ring while stopped) at getZIndex()+1, and its track
// (setShowTrack: a whole ring in SpinnerTrack's top colour, while it spins)
// at getZIndex(),
// each one drawArc slot on the largest circle in its bounds, the stroke its
// getThickness(). Without a track that slot is hidden, so a spinning frame
// writes the arc's slot alone.
class Spinner:public textured::ControlRegister<Spinner>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }
    // the arc is drawn at getZIndex()+1, over the track
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="Spinner";
};

class Badge:public textured::ControlRegister<Badge>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 1; }

    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;
    void appendIds(std::vector<size_t> &primitiveIds, std::vector<size_t> &textIds) const override
    {
        ControlRegister<Badge>::appendIds(primitiveIds, textIds);
        if(m_hasText)
            textIds.push_back(m_textId);
    }

    inline static const std::string_view name="Badge";

private:
    bool m_hasText=false;
    size_t m_textId=0;
};

class StatusBar:public textured::ControlRegister<StatusBar>
{
public:
    static void staticInit(TexturedSkin *skin);


//    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
//    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="StatusBar";

    static size_t Background;
    static size_t Selection;
    static BorderedPrimitive BackgroundPrimitive;
};


struct TabTop
{
    static constexpr std::string_view value="Tab.Top.";
//    inline static constexpr const char value[]="Tab.Top.";
};

struct TabBottom
{
    static constexpr std::string_view value="Tab.Bottom.";
    //inline static constexpr const char value[]="Tab.Bottom.";
};

struct TabLeft
{
    static constexpr std::string_view value="Tab.Left.";
//    inline static constexpr const char value[]="Tab.Left.";
};

struct TabRight
{
    static constexpr std::string_view value="Tab.Right.";
    //inline static constexpr const char value[]="Tab.Right.";
};

template<typename _Type>
class TabItem:public textured::ControlRegister<TabItem<_Type>>
{
public:
    size_t requiredPrimitives() override { return 0; }

    static void staticInit(TexturedSkin *skin)
    {
        static constexpr std::string_view ActiveLabel="Active";
        static auto TabActiveLabel=join<_Type::value, ActiveLabel>;

        Active=skin->getTextureEntryId(TabActiveLabel);
//        Inactive=skin->getTextureEntryId(_Type::value+"Inactive");
    }

    void generate(renderer::Base *renderer, controls::Base *baseControl) override
    {}

    void update(renderer::Base *renderer, controls::Base *baseControl) override
    {}

    void remove(renderer::Base *renderer, controls::Base *baseControl) override
    {}

    static constexpr std::string_view TabLabel="Tab";
    static inline auto name=join<_Type::value, TabLabel>;

    static size_t Active;
    static size_t Inactive;
    static BorderedPrimitive ActivePrimitive;
};
template<typename _Type> size_t TabItem<_Type>::Active;
template<typename _Type> size_t TabItem<_Type>::Inactive;
template<typename _Type> BorderedPrimitive TabItem<_Type>::ActivePrimitive;


class TabButton:public textured::ControlRegister<TabButton>
{
public:
    static void staticInit(TexturedSkin *skin);


    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="TabButton";

    //! How much shorter an unselected tab is drawn on the side away from the
    //! page, and how far in at each end along the row.
    static constexpr int UnselectedOuterInset=2;
    static constexpr int UnselectedEndInset=1;

    static size_t Button;
    static BorderedPrimitive ButtonPrimitive;
};

class TabControl:public textured::ControlRegister<TabControl>
{
public:
    static void staticInit(TexturedSkin *skin);


    
//    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
//    void remove(renderer::Base *renderer, controls::Base *baseControl) override;
    size_t requiredPrimitives() override { return 0; }

    inline static const std::string_view name="TabControl";

    TabItem<TabTop> m_tabTop;
    TabItem<TabBottom> m_tabBottom;
    TabItem<TabLeft> m_tabLeft;
    TabItem<TabRight> m_tabRight;

    static size_t Control;
    static size_t HeaderBar;

    static BorderedPrimitive ControlPrimitive;
    static BorderedPrimitive HeaderBarPrimitive;

};

class TabTitleBar:public textured::ControlRegister<TabTitleBar>
{
public:
    static void staticInit(TexturedSkin *skin);


    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="TabTitleBar";

    static size_t HeaderBar;
    static BorderedPrimitive HeaderBarPrimitive;

};

class Text:public textured::ControlRegister<Text>
{
public:
    static void staticInit(TexturedSkin *skin);

    size_t requiredPrimitives() override { return 1; }

    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;
    //! its one id is a text id
    void appendIds(std::vector<size_t> &, std::vector<size_t> &textIds) const override
    {
        textIds.insert(textIds.end(), m_primitives.begin(), m_primitives.end());
    }

    inline static const std::string_view name="Text";
};

//! A CodeEditor line's run (controls::internal::CodeLineText): Text's, drawn
//! with the line's colour spans.
class CodeLineText:public textured::ControlRegister<CodeLineText>
{
public:
    static void staticInit(TexturedSkin *skin);

    size_t requiredPrimitives() override { return 1; }

    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;
    void appendIds(std::vector<size_t> &, std::vector<size_t> &textIds) const override
    {
        textIds.insert(textIds.end(), m_primitives.begin(), m_primitives.end());
    }

    inline static const std::string_view name="CodeLineText";
};

//! A HexView row's hex or ASCII run (controls::internal::HexViewText):
//! Text's, drawn with its colour spans (zero bytes, non-printable bytes).
class HexViewText:public textured::ControlRegister<HexViewText>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 1; }

    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;
    void appendIds(std::vector<size_t> &, std::vector<size_t> &textIds) const override
    {
        textIds.insert(textIds.end(), m_primitives.begin(), m_primitives.end());
    }

    inline static const std::string_view name="HexViewText";
};

class TextBox:public textured::ControlRegister<TextBox>
{
public:
    static void staticInit(TexturedSkin *skin);

    // the box, the selection and the caret; the caret is drawn in front of
    // the box's text, at the one overlay z after its subtree
    size_t requiredPrimitives() override { return 3; }
    size_t overlayLayers() override { return 1; }

    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="TextBox";

private:
    //! The caret's slot was not rewritten by the last change that moved it.
    bool m_caretStale=true;

public:

    static size_t Normal;
    static size_t Focus;
    static size_t Disabled;
    static BorderedPrimitive NormalPrimitive;
};

// A SearchComboBox's text field: a TextBox's selection and caret
// (paintTextBoxMarks) with no frame, the box's surface behind it.
class SearchComboBoxField:public textured::ControlRegister<SearchComboBoxField>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 3; }
    size_t overlayLayers() override { return 1; }

    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="SearchComboBoxField";

private:
    bool m_caretStale=true;
};

//! controls::ToolTip, the canvas's tooltip box: one tokyonight::Tooltip surface.
class ToolTip:public textured::ControlRegister<ToolTip>
{
public:
    static void staticInit(TexturedSkin *skin);


    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="ToolTip";
};

//! controls::Popup, the panel a canvas shows next to an anchor: one
//! tokyonight::Menu surface, a context menu's panel.
class Popup:public textured::ControlRegister<Popup>
{
public:
    static void staticInit(TexturedSkin *skin);


    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="Popup";
};

// TreeControl's panel; PropertyTree finds it through its parent type.
class Tree:public textured::ControlRegister<Tree>
{
public:
    static void staticInit(TexturedSkin *skin);


    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="TreeControl";

    static size_t Background;
    static size_t Minus;
    static size_t Plus;
    static BorderedPrimitive BackgroundPrimitive;
};

class SpinBox:public textured::ControlRegister<SpinBox>
{
public:
    static void staticInit(TexturedSkin *skin);

    size_t requiredPrimitives() override { return 0; }

    //void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    //void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="Spinbox";

    static size_t UpNormal;
    static size_t UpHover;
    static size_t UpDown;
    static size_t UpDisabled;
    static size_t DownNormal;
    static size_t DownHover;
    static size_t DownDown;
    static size_t DownDisabled;
    static BorderedPrimitive BackgroundPrimitive;
};

// controls::SplitterBar: one bar over its bounds, drawn only while the control
// draws its background (a CrossSplitter's bars are off until
// setSplittersVisible(true), a Properties' column split always is).
class SplitterBar:public textured::ControlRegister<SplitterBar>
{
public:
    static void staticInit(TexturedSkin *skin) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 1; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="SplitterBar";
};

// gweni::tools::Inspector's highlight box: four slots, its edges, filled in
// tokyonight::Orange, InspectorHighlight::Edge units thick, inside its render
// bounds (the target's rect grown by the edge). Not one rect with a
// transparent fill: a style 4 pixel writes depth even where it is
// transparent, over whatever it frames.
class InspectorHighlight:public textured::ControlRegister<InspectorHighlight>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 4; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="InspectorHighlight";
};

//! controls::internal::DropIndicator, a reorder drag's bar in the gap: one
//! tokyonight::DropIndicator surface.
class DropIndicator:public textured::ControlRegister<DropIndicator>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="DropIndicator";
};

//! controls::internal::DragSourceVeil, over a reorder drag's item where it was:
//! one tokyonight::DragSource surface.
class DragSourceVeil:public textured::ControlRegister<DragSourceVeil>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="DragSourceVeil";
};

//! gweni::tools::StatsOverlay, the stats panel in the canvas's corner: one
//! tokyonight::StatsOverlay surface.
class StatsOverlay:public textured::ControlRegister<StatsOverlay>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="StatsOverlay";
};

//! controls::internal::ModalScrim, Canvas::setModalScrim's backdrop: one
//! tokyonight::ModalScrim surface over the canvas.
class ModalScrim:public textured::ControlRegister<ModalScrim>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="ModalScrim";
};

//! dialogs::Dialog: [0] its shadow (DialogShadow) at getZIndex(), ShadowSize
//! units below and right of the panel, [1] the panel (Dialog) at +1 over it,
//! both inside its bounds.
class Dialog:public textured::ControlRegister<Dialog>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }
    // the panel is drawn at getZIndex()+1, over the shadow
    size_t depthLayers() override { return 2; }

    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="Dialog";
};

//! A Dialog's title bar: one DialogTitle strip, its top corners rounded.
class DialogTitleBar:public textured::ControlRegister<DialogTitleBar>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="DialogTitleBar";
};

//! A Dialog's footer: one DialogFooter strip, its bottom corners rounded.
class DialogFooter:public textured::ControlRegister<DialogFooter>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="DialogFooter";
};

//! controls::Toast: [0] its panel (Toast) at getZIndex(), [1] the accent strip
//! (ToastInfo, ToastPositive, ToastWarning or ToastNegative by its kind) down
//! its left edge, inside its border, at +1.
class Toast:public textured::ControlRegister<Toast>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }
    size_t depthLayers() override { return 2; }

    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="Toast";
};

class Window:public textured::ControlRegister<Window>
{
public:
    static void staticInit(TexturedSkin *skin);


    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="WindowControl";

    static size_t Normal;
    static size_t Inactive;
    static BorderedPrimitive NormalPrimitive;
};

}//namespace textured
}//namespace skin
}//namespace gweni

#endif//_gweni_skins_texturedControls_h_
