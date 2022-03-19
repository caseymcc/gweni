#ifndef _gweni_skins_texturedControls_h_
#define _gweni_skins_texturedControls_h_

#include "gweni/skins/textured/texturedPrimitives.h"
#include "gweni/skins/textured/texturedSkin.h"

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
    static void staticInit(TexturedSkin *skin) {m_staticInit=true;}

    size_t requiredPrimitives() override { return 0; }
    inline static const std::string_view name="Base";
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
    size_t requiredPrimitives() override { return 9; }

    inline static constexpr std::string_view ButtonNormal="Button.Normal";
    inline static constexpr std::string_view ButtonHover="Button.Hover";
    inline static constexpr std::string_view ButtonDown="Button.Down";
    inline static constexpr std::string_view ButtonDisabled="Button.Disabled";

    inline static auto ControlButtonNormal=join<_Type::value, ButtonNormal>;
    inline static auto ControlButtonHover=join<_Type::value, ButtonHover>;
    inline static auto ControlButtonDown=join<_Type::value, ButtonDown>;
    inline static auto ControlButtonDisabled=join<_Type::value, ButtonDisabled>;

    static void staticInit(TexturedSkin *skin)
    {
        Normal=skin->getTextureEntryId(ControlButtonNormal);
        Hover=skin->getTextureEntryId(ControlButtonHover);
        Down=skin->getTextureEntryId(ControlButtonDown);
        Disabled=skin->getTextureEntryId(ControlButtonDisabled);

        NormalPrimitive.init(skin, Normal);
        HoverPrimitive.init(skin, Hover);
        DownPrimitive.init(skin, Down);
        DisabledPrimitive.init(skin, Disabled);

        m_staticInit=true;
    }

    void update(renderer::Base *renderer, controls::Base *baseControl) override
    {
        if(baseControl->getStateChange() != controls::StateChange_Nothing)
        {
            controls::Button *control=static_cast<controls::Button *>(baseControl);
            BorderedPrimitive *primitive;

//            if(control->isDisabled())
//                DisabledPrimitive.draw(renderer, m_primitives.data(), baseControl->getRenderBounds(), baseControl->getZIndex());
//            else if(control->isDepressed())
//                HoverPrimitive.draw(renderer, m_primitives.data(), baseControl->getRenderBounds(), baseControl->getZIndex());
//            else if(control->isHovered())
//                DownPrimitive.draw(renderer, m_primitives.data(), baseControl->getRenderBounds(), baseControl->getZIndex());
//            else
//                NormalPrimitive.draw(renderer, m_primitives.data(), baseControl->getRenderBounds(), baseControl->getZIndex());

            if(control->isDisabled())
                primitive=&DisabledPrimitive;
            else if(control->isDepressed())
                primitive=&HoverPrimitive;
            else if(control->isHovered())
                primitive=&DownPrimitive;
            else
                primitive=&NormalPrimitive;

            if(!control->hidden())
                primitive->draw(renderer, m_primitives.data(), baseControl->getRenderBounds(), baseControl->getZIndex());

            if((baseControl->getStateChange()&controls::StateChange_Visibility) ||
                (baseControl->getStateChange()&controls::StateChange_Created))
            {
                primitive->show(renderer, m_primitives.data(), !control->hidden());
                return;
            }
        }
    }

//    void remove(renderer::Base *renderer, controls::Base *baseControl)
//    {
//        releasePrimitives(renderer);
//    }

    static constexpr std::string_view ButtonLabel="Button";
    static inline auto name=join<_Type::value, ButtonLabel>;

private:
    static size_t Normal;
    static size_t Hover;
    static size_t Down;
    static size_t Disabled;

    static BorderedPrimitive NormalPrimitive;
    static BorderedPrimitive HoverPrimitive;
    static BorderedPrimitive DownPrimitive;
    static BorderedPrimitive DisabledPrimitive;
};

template<typename _Type> size_t ButtonTemplate<_Type>::Normal;
template<typename _Type> size_t ButtonTemplate<_Type>::Hover;
template<typename _Type> size_t ButtonTemplate<_Type>::Down;
template<typename _Type> size_t ButtonTemplate<_Type>::Disabled;
template<typename _Type> BorderedPrimitive ButtonTemplate<_Type>::NormalPrimitive;
template<typename _Type> BorderedPrimitive ButtonTemplate<_Type>::HoverPrimitive;
template<typename _Type> BorderedPrimitive ButtonTemplate<_Type>::DownPrimitive;
template<typename _Type> BorderedPrimitive ButtonTemplate<_Type>::DisabledPrimitive;

struct DefaultButton
{
    inline static const std::string_view value="";
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

    size_t requiredPrimitives() override { return 9; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="CategoryHeaderButton";

    static size_t Button;
    static BorderedPrimitive ButtonPrimitive;
};

class CollapsibleCategory:public textured::ControlRegister<CollapsibleCategory>
{
public:
    static void staticInit(TexturedSkin *skin);

    size_t requiredPrimitives() override { return 9; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="CollapsibleCategory";

    static size_t Background;

    static BorderedPrimitive BackgroundPrimitive;
};

class CategoryList:public textured::ControlRegister<CategoryList>
{
public:
    static void staticInit(TexturedSkin *skin);

    size_t requiredPrimitives() override { return 9; }

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

    size_t requiredPrimitives() override { return 9; }

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

    size_t requiredPrimitives() override { return 9; }

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

    size_t requiredPrimitives() override { return 3; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="DockBase";
};

class GroupBox:public textured::ControlRegister<GroupBox>
{
public:
    static void staticInit(TexturedSkin *skin);

    size_t requiredPrimitives() override { return 9; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="GroupBox";

    static size_t Normal;
    static BorderedPrimitive NormalPrimitive;
};


struct InputButton
{
    inline static const std::string_view value="Input.Button.";
};

struct InputButtonUp
{
    inline static const std::string_view value="Input.UpDown.Up.";
};

struct InputButtonDown
{
    inline static const std::string_view value="Input.UpDown.Down.";
};

class Input:public textured::ControlRegister<Input>
{
public:
    static void staticInit(TexturedSkin *skin);

    size_t requiredPrimitives() override { return 9; }

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

    size_t requiredPrimitives() override { return 9; }

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

    size_t requiredPrimitives() override { return 9; }

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

    size_t requiredPrimitives() override { return 9; }

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

    size_t requiredPrimitives() override { return 9; }

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

class ProgressBar:public textured::ControlRegister<ProgressBar>
{
public:
    static void staticInit(TexturedSkin *skin);

    size_t requiredPrimitives() override { return 9; }

    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="ProgressBar";

    static size_t Back;
    static size_t Front;

    static BorderedPrimitive BackgroundPrimitive;

};

class RadioButton:public textured::ControlRegister<RadioButton>
{
public:
    static void staticInit(TexturedSkin *skin);

    size_t requiredPrimitives() override { return 9; }

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

    size_t requiredPrimitives() override { return 9; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="ScrollBarButton";

    static size_t ScrollBarButton::Normal[4];
    static size_t ScrollBarButton::Hover[4];
    static size_t ScrollBarButton::Down[4];
    static size_t ScrollBarButton::Disable[4];

    static BorderedPrimitive NormalPrimitive[4];
    static BorderedPrimitive HoverPrimitive[4];
    static BorderedPrimitive DownPrimitive[4];
    static BorderedPrimitive DisablePrimitive[4];
};



struct ScrollerButton
{
    inline static const std::string_view value="Scroller";
};

struct ScrollerHorizontal
{
    inline static const std::string_view value="Scroller.Horizontal";
};

struct ScrollerVertical
{
    inline static const std::string_view value="Scroller.Vertical";
};

class Scroller:public textured::ControlRegister<Scroller>
{
public:
    static void staticInit(TexturedSkin *skin);

    size_t requiredPrimitives() override { return 9; }

    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="Scroller";

    ButtonTemplate<ScrollerButton> m_button;
    ButtonTemplate<ScrollerHorizontal> m_horizontalButton;
    ButtonTemplate<ScrollerVertical> m_verticalButton;

    static size_t TrackV;
    static size_t TrackH;
};

class Shadow:public textured::ControlRegister<Shadow>
{
public:
    static void staticInit(TexturedSkin *skin);

    size_t requiredPrimitives() override { return 9; }

    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="Shadow";

    static size_t Background;
    static BorderedPrimitive BackgroundPrimitive;
};

class Slider:public textured::ControlRegister<Slider>
{
public:
    static void staticInit(TexturedSkin *skin);

    size_t requiredPrimitives() override { return 9; }

    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="Slider";

    static size_t Horizontal_Normal;
    static size_t Horizontal_Hover;
    static size_t Horizontal_Down;
    static size_t Horizontal_Disabled;
    static size_t Vertical_Normal;
    static size_t Vertical_Hover;
    static size_t Vertical_Down;
    static size_t Vertical_Disabled;
    static BorderedPrimitive NormalPrimitive;
};

class StatusBar:public textured::ControlRegister<StatusBar>
{
public:
    static void staticInit(TexturedSkin *skin);

    size_t requiredPrimitives() override { return 9; }

    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

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
    size_t requiredPrimitives() override { return 9; }

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

    size_t requiredPrimitives() override { return 9; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="TabButton";

    static size_t Button;
    static BorderedPrimitive ButtonPrimitive;
};

class TabControl:public textured::ControlRegister<TabControl>
{
public:
    static void staticInit(TexturedSkin *skin);

    size_t requiredPrimitives() override { return 9; }

    
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

    size_t requiredPrimitives() override { return 9; }

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

    size_t requiredPrimitives() override { return 9; }

    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="TextBox";

    static size_t Normal;
    static size_t Focus;
    static size_t Disabled;
    static BorderedPrimitive NormalPrimitive;
};

class Tooltip:public textured::ControlRegister<Tooltip>
{
public:
    static void staticInit(TexturedSkin *skin);

    size_t requiredPrimitives() override { return 9; }

    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="Tooltip";

    static size_t Background;
    static BorderedPrimitive BackgroundPrimitive;

};

class Tree:public textured::ControlRegister<Tree>
{
public:
    static void staticInit(TexturedSkin *skin);

    size_t requiredPrimitives() override { return 9; }

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

    size_t requiredPrimitives() override { return 9; }

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

    size_t requiredPrimitives() override { return 9; }

    void update(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="SplitterBar";

    static size_t Background;
    static BorderedPrimitive BackgroundPrimitive;
};

class Window:public textured::ControlRegister<Window>
{
public:
    static void staticInit(TexturedSkin *skin);

    size_t requiredPrimitives() override { return 9; }

    void generate(renderer::Base *renderer, controls::Base *baseControl) override;
    void update(renderer::Base *renderer, controls::Base *baseControl) override;
    void remove(renderer::Base *renderer, controls::Base *baseControl) override;

    inline static const std::string_view name="Window";

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
