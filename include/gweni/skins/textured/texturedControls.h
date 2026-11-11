#ifndef _gweni_skins_texturedControls_h_
#define _gweni_skins_texturedControls_h_

#include "gweni/skins/textured/texturedPrimitives.h"
#include "gweni/skins/textured/texturedSkin.h"
#include "gweni/skins/textured/tokyoNight.h"

#include "gweni/controls/button.h"

namespace gweni
{
namespace controls
{
class MenuItem;
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
        const tokyonight::Surface *surface;
        // a toggle that is on keeps its look until a press or disabling it;
        // a plain button (LabelClickable included) is never on
        bool on=control->isToggle() && control->getToggleState();

        if(control->isDisabled())
            surface=on?&tokyonight::ButtonOnDisabled:&tokyonight::ButtonDisabled;
        else if(control->isDepressed())
            surface=&tokyonight::ButtonDown;
        else if(on)
            surface=control->isHovered()?&tokyonight::ButtonOnHover:&tokyonight::ButtonOn;
        else if(control->isHovered())
            surface=&tokyonight::ButtonHover;
        else
            surface=&tokyonight::ButtonNormal;

        tokyonight::paintSurface(renderer, ThisClass::m_primitives.data(), baseControl, *surface);
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
// a NodeGraph's layers and a Node's parts, which the Node's renderer draws
GWENI_TRANSPARENT_RENDERER(NodeGraphLayer)
GWENI_TRANSPARENT_RENDERER(NodeTitleBar)
GWENI_TRANSPARENT_RENDERER(NodePinColumn)
GWENI_TRANSPARENT_RENDERER(NodeBody)

// Visual controls this skin does not draw yet. Each is an empty renderer on
// purpose, so Base::setSkin() does not report the type as having none (#185),
// and each names the idea that tracks its real renderer: drop the line when
// that renderer lands. A drawsNothing() override is not the place for these,
// since the controls are meant to draw. (None is waiting now.)
#define GWENI_PENDING_RENDERER(Class) GWENI_TRANSPARENT_RENDERER(Class)
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

// A Node: [0] its frame (NodeBody, over Node::getFrameRect()), [1] its title
// bar at getZIndex()+1 over the frame's top, NodeTitle's fill or the node's
// title colour (its bottom a shade darker), with the frame's border, radius and
// border width, square and borderless at its bottom, so frame and title are one
// outline.
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
// connected).
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

// A Selectable's row: hidden while idle (or disabled and unselected), a
// surface for hover, press and selection; the label's Text draws the string.
class Selectable:public textured::ControlRegister<Selectable>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="Selectable";
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
