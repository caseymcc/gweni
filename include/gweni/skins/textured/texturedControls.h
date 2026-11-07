#ifndef _gweni_skins_texturedControls_h_
#define _gweni_skins_texturedControls_h_

#include "gweni/skins/textured/texturedPrimitives.h"
#include "gweni/skins/textured/texturedSkin.h"
#include "gweni/skins/textured/tokyoNight.h"

#include "gweni/controls/button.h"

namespace gweni
{
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

        if(control->isDisabled())
            surface=&tokyonight::ButtonDisabled;
        else if(control->isDepressed())
            surface=&tokyonight::ButtonDown;
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
    static void staticInit(TexturedSkin *skin);

    size_t requiredPrimitives() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="CheckBox";

    static size_t ActiveNormal;
    static size_t ActiveChecked;
    static size_t DisabledNormal;
    static size_t DisabledChecked;

    static BorderedPrimitive ActiveNormalPrimitive;
    static BorderedPrimitive ActiveCheckedPrimitive;
    static BorderedPrimitive DisabledNormalPrimitive;
    static BorderedPrimitive DisabledCheckedPrimitive;
};

class ComboBox:public textured::ControlRegister<ComboBox>
{
public:
    static void staticInit(TexturedSkin *skin);

    size_t requiredPrimitives() override { return 2; }

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
GWENI_TRANSPARENT_RENDERER(VerticalScrollBar)
GWENI_TRANSPARENT_RENDERER(HorizontalScrollBar)
GWENI_TRANSPARENT_RENDERER(ScrollBarBar)

// Visual controls this skin does not draw yet. Each is an empty renderer on
// purpose, so Base::setSkin() does not report the type as having none (#185),
// and each names the idea that tracks its real renderer: drop the line when
// that renderer lands. A drawsNothing() override is not the place for these,
// since the controls are meant to draw.
#define GWENI_PENDING_RENDERER(Class) GWENI_TRANSPARENT_RENDERER(Class)

GWENI_PENDING_RENDERER(TreeControl)      // #219: the Tree renderer is registered as "Tree"
GWENI_PENDING_RENDERER(TreeNode)         // #219: tree lines
GWENI_PENDING_RENDERER(PropertyTreeNode) // #219: category rail
GWENI_PENDING_RENDERER(PropertyRow)      // #219: separators, hover and editing highlight
GWENI_PENDING_RENDERER(RightArrow)       // #219: a MenuItem's submenu chevron
#undef GWENI_PENDING_RENDERER
#undef GWENI_TRANSPARENT_RENDERER

class ProgressBar:public textured::ControlRegister<ProgressBar>
{
public:
    static void staticInit(TexturedSkin *skin);

    // [0] the back (track), [1] the front (fill).
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

class RadioButton:public textured::ControlRegister<RadioButton>
{
public:
    static void staticInit(TexturedSkin *skin);

    size_t requiredPrimitives() override { return 2; }

    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="RadioButton";

    static size_t ActiveNormal;
    static size_t ActiveChecked;
    static size_t DisabledNormal;
    static size_t DisabledChecked;

    static BorderedPrimitive NormalPrimitive;
};

class ScrollBarButton:public textured::ControlRegister<ScrollBarButton>
{
public:
    static void staticInit(TexturedSkin *skin);

    size_t requiredPrimitives() override { return 2; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="ScrollBarButton";

    static size_t Normal[4];
    static size_t Hover[4];
    static size_t Down[4];
    static size_t Disable[4];

    static BorderedPrimitive NormalPrimitive[4];
    static BorderedPrimitive HoverPrimitive[4];
    static BorderedPrimitive DownPrimitive[4];
    static BorderedPrimitive DisablePrimitive[4];
};



struct ScrollerButton
{
    inline static JoinConstExpr std::string_view value="Scroller";
};

struct ScrollerHorizontal
{
    inline static JoinConstExpr std::string_view value="Scroller.Horizontal";
};

struct ScrollerVertical
{
    inline static JoinConstExpr std::string_view value="Scroller.Vertical";
};

class Scroller:public textured::ControlRegister<Scroller>
{
public:
    static void staticInit(TexturedSkin *skin);


    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="Scroller";

    ButtonTemplate<ScrollerButton> m_button;
    ButtonTemplate<ScrollerHorizontal> m_horizontalButton;
    ButtonTemplate<ScrollerVertical> m_verticalButton;

    static size_t TrackV;
    static size_t TrackH;

    static BorderedPrimitive TrackPrimitive;
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
// while the slider clamps to its notches). The tick count can change after
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

class Tree:public textured::ControlRegister<Tree>
{
public:
    static void staticInit(TexturedSkin *skin);


    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="Tree";

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

class SplitterBar:public textured::ControlRegister<SplitterBar>
{
public:
    static void staticInit(TexturedSkin *skin);

    size_t requiredPrimitives() override { return 0; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="SplitterBar";

    static size_t Background;
    static BorderedPrimitive BackgroundPrimitive;
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
    static size_t Close;
    static size_t Close_Hover;
    static size_t Close_Down;
    static size_t Maxi;
    static size_t Maxi_Hover;
    static size_t Maxi_Down;
    static size_t Mini;
    static size_t Mini_Hover;
    static size_t Mini_Down;
    static size_t Restore;
    static size_t Restore_Hover;
    static size_t Restore_Down;
    static BorderedPrimitive NormalPrimitive;
};

}//namespace textured
}//namespace skin
}//namespace gweni

#endif//_gweni_skins_texturedControls_h_
