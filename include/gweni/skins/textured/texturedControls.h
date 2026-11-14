#ifndef _gweni_skins_texturedControls_h_
#define _gweni_skins_texturedControls_h_

#include "gweni/skins/textured/texturedPrimitives.h"
#include "gweni/skins/textured/texturedSkin.h"
#include "gweni/skins/textured/tokyoNight.h"

#include "gweni/controls/button.h"
#include "gweni/controls/nodeGraph.h"
#include "gweni/controls/timeline.h"

#include <algorithm>
#include <vector>

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
        else if(control->isFocussed())
            surface=&tokyonight::ButtonFocus;
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
    static size_t capacity(size_t count) { return count ? count+std::max<size_t>(count/2, 4) : 0; }

    //! What a bar is drawn in: TimelineBar, its colour with TimelineBar's
    //! border, or the theme surface it names (magenta for a name no theme has).
    static tokyonight::Surface barSurface(const controls::TimelineBar &bar);

private:
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

    static std::vector<size_t> inUse(const SlotList &list);
    static void releaseList(renderer::Base *renderer, SlotList &list);
    void takeLists(renderer::Base *renderer, size_t lanes, size_t grid, const std::vector<size_t> &bars);

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
// with the caret, hidden otherwise), then a selection rect per visual row the
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
// segment holding the keyboard focus is drawn over its whole bounds instead,
// its border SegmentFocus's on every side, so it covers its left joint: it is
// in front of its left neighbour, and its right neighbour does not draw on
// their joint.
class SegmentedButton:public textured::ControlRegister<SegmentedButton>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }

    size_t requiredPrimitives() override { return 2; }
    // the check is drawn at getZIndex()+1, over the fill
    size_t depthLayers() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="SegmentedButton";
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
