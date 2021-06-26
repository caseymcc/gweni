#ifndef _gweni_skins_texturedControls_h_
#define _gweni_skins_texturedControls_h_

#include "gweni/skins/texturedPrimitives.h"
#include "gweni/skins/texturedSkin.h"

namespace gweni
{
namespace skin
{
namespace textured
{

struct Horizontal
{
    inline static const std::string_view value="Horizontal";
};
struct Vertical
{
    inline static const std::string_view value="Vertical";
};

class Control
{
public:
    virtual void generate(renderer::Base *render, controls::Base *control)=0;
    virtual void remove(renderer::Base *render, controls::Base *control)=0;

    virtual void update(renderer::Base *render, controls::Base *control)=0;

protected:
    std::vector<int> m_primitives;
};

template<typename _Type>
class ButtonTemplate:public Control
{
public:
    static void staticInit(TexturedSkin *skin)
    {
        Normal=skin->getTextureEntryId(_Type::value+"Button.Normal");
        Hover=skin->getTextureEntryId(_Type::value+"Button.Hover");
        Down=skin->getTextureEntryId(_Type::value+"Button.Down");
        Disabled=skin->getTextureEntryId(_Type::value+"Button.Disabled");
    }

    void generate(renderer::Base *renderer, controls::Base *control)
    {}

    void update(renderer::Base *renderer, controls::Base *control)
    {}

    void remove(renderer::Base *renderer, controls::Base *control)
    {}

private:
    static size_t Normal;
    static size_t Hover;
    static size_t Down;
    static size_t Disabled;
};

class Button:public Control
{
public:
    static void staticInit(TexturedSkin *skin);

    void generate(renderer::Base *renderer, controls::Base *control) override;
    void update(renderer::Base *renderer, controls::Base *control) override;
    void remove(renderer::Base *renderer, controls::Base *control) override;

    inline static const std::string_view name="Button";

private:
    static size_t Normal;
    static size_t Hover;
    static size_t Down;
    static size_t Disabled;
};

class CategoryList:public Control
{
public:
    static void staticInit(TexturedSkin *skin);

    void generate(renderer::Base *renderer, controls::Base *control) override;
    void update(renderer::Base *renderer, controls::Base *control) override;
    void remove(renderer::Base *renderer, controls::Base *control) override;

    inline static const std::string_view name="CategoryList";

    static size_t Outer;
    static size_t Inner;
    static size_t Header;
};

class Checkbox:public Control
{
public:
    static void staticInit(TexturedSkin *skin);

    void generate(renderer::Base *renderer, controls::Base *control) override;
    void update(renderer::Base *renderer, controls::Base *control) override;
    void remove(renderer::Base *renderer, controls::Base *control) override;

    inline static const std::string_view name="Checkbox";

    static size_t ActiveNormal;
    static size_t ActiveChecked;

    static size_t DisabledNormal;
    static size_t DisabledChecked;
};

class ComboBox:public Control
{
public:
    static void staticInit(TexturedSkin *skin);

    void generate(renderer::Base *renderer, controls::Base *control) override;
    void update(renderer::Base *renderer, controls::Base *control) override;
    void remove(renderer::Base *renderer, controls::Base *control) override;

    inline static const std::string_view name="ComboBox";

    static size_t Normal;
    static size_t Hover;
    static size_t Down;
    static size_t Disabled;
} ComboBox;

class GroupBox:public Control
{
public:
    static void staticInit(TexturedSkin *skin);

    void generate(renderer::Base *renderer, controls::Base *control) override;
    void update(renderer::Base *renderer, controls::Base *control) override;
    void remove(renderer::Base *renderer, controls::Base *control) override;

    inline static const std::string_view name="GroupBox";
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

class Input:public Control
{
public:
    static void staticInit(TexturedSkin *skin);

    void generate(renderer::Base *renderer, controls::Base *control) override;
    void update(renderer::Base *renderer, controls::Base *control) override;
    void remove(renderer::Base *renderer, controls::Base *control) override;

    inline static const std::string_view name="Input";

    ButtonTemplate<InputButton> m_buttonInput;
    ButtonTemplate<InputButtonUp> m_buttonUp;
    ButtonTemplate<InputButtonDown> m_buttonDown;
};

class ListBox:public Control
{
public:
    static void staticInit(TexturedSkin *skin);

    void generate(renderer::Base *renderer, controls::Base *control) override;
    void update(renderer::Base *renderer, controls::Base *control) override;
    void remove(renderer::Base *renderer, controls::Base *control) override;

    inline static const std::string_view name="ListBox";

    static size_t Background;
    static size_t Hovered;

    static size_t EvenLine;
    static size_t OddLine;
    static size_t EvenLineSelected;
    static size_t OddLineSelected;

};

class Menu:public Control
{
public:
    static void staticInit(TexturedSkin *skin);

    void generate(renderer::Base *renderer, controls::Base *control) override;
    void update(renderer::Base *renderer, controls::Base *control) override;
    void remove(renderer::Base *renderer, controls::Base *control) override;

    inline static const std::string_view name="Menu";

    static size_t RightArrow;
    static size_t Check;

    static size_t Strip;
    static size_t Background;
    static size_t BackgroundWithMargin;
    static size_t Hover;

};

class Panel:public Control
{
public:
    static void staticInit(TexturedSkin *skin);

    void generate(renderer::Base *renderer, controls::Base *control) override;
    void update(renderer::Base *renderer, controls::Base *control) override;
    void remove(renderer::Base *renderer, controls::Base *control) override;

    inline static const std::string_view name="Panel";

    static size_t Normal;
    static size_t Bright;
    static size_t Dark;
    static size_t Highlight;
};

class ProgressBar:public Control
{
public:
    static void staticInit(TexturedSkin *skin);

    void generate(renderer::Base *renderer, controls::Base *control) override;
    void update(renderer::Base *renderer, controls::Base *control) override;
    void remove(renderer::Base *renderer, controls::Base *control) override;

    inline static const std::string_view name="ProgressBar";

    static size_t Back;
    static size_t Front;

};

class RadioButton:public Control
{
public:
    static void staticInit(TexturedSkin *skin);

    void generate(renderer::Base *renderer, controls::Base *control) override;
    void update(renderer::Base *renderer, controls::Base *control) override;
    void remove(renderer::Base *renderer, controls::Base *control) override;

    inline static const std::string_view name="RadioButton";

    static size_t ActiveNormal;
    static size_t ActiveChecked;
    static size_t DisabledNormal;
    static size_t DisabledChecked;
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

class Scroller:public Control
{
public:
    static void staticInit(TexturedSkin *skin);

    void generate(renderer::Base *renderer, controls::Base *control) override;
    void update(renderer::Base *renderer, controls::Base *control) override;
    void remove(renderer::Base *renderer, controls::Base *control) override;

    inline static const std::string_view name="Scroller";

    ButtonTemplate<ScrollerButton> m_button;
    ButtonTemplate<ScrollerHorizontal> m_horizontalButton;
    ButtonTemplate<ScrollerVertical> m_verticalButton;

    static size_t TrackV;
    static size_t TrackH;
};

class Shadow:public Control
{
public:
    static void staticInit(TexturedSkin *skin);

    void generate(renderer::Base *renderer, controls::Base *control) override;
    void update(renderer::Base *renderer, controls::Base *control) override;
    void remove(renderer::Base *renderer, controls::Base *control) override;

    inline static const std::string_view name="Shadow";

    static size_t Background;
};

class Slider:public Control
{
public:
    static void staticInit(TexturedSkin *skin);

    void generate(renderer::Base *renderer, controls::Base *control) override;
    void update(renderer::Base *renderer, controls::Base *control) override;
    void remove(renderer::Base *renderer, controls::Base *control) override;

    inline static const std::string_view name="Slider";

    static size_t Horizontal_Normal;
    static size_t Horizontal_Hover;
    static size_t Horizontal_Down;
    static size_t Horizontal_Disabled;
    static size_t Vertical_Normal;
    static size_t Vertical_Hover;
    static size_t Vertical_Down;
    static size_t Vertical_Disabled;
};

class StatusBar:public Control
{
public:
    static void staticInit(TexturedSkin *skin);

    void generate(renderer::Base *renderer, controls::Base *control) override;
    void update(renderer::Base *renderer, controls::Base *control) override;
    void remove(renderer::Base *renderer, controls::Base *control) override;

    inline static const std::string_view name="StatusBar";

    static size_t Background;
    static size_t Selection;
};


struct TabTop
{
//    inline static constexpr std::string_view value="Tab.Top.";
    inline static constexpr const char value[]="Tab.Top.";
};

struct TabBottom
{
//    inline static constexpr std::string_view value="Tab.Bottom.";
    inline static constexpr const char value[]="Tab.Bottom.";
};

struct TabLeft
{
//    inline static constexpr std::string_view value="Tab.Left.";
    inline static constexpr const char value[]="Tab.Left.";
};

struct TabRight
{
//    inline static constexpr std::string_view value="Tab.Right.";
    inline static constexpr const char value[]="Tab.Right.";
};

template<typename _Type>
class TabItem:public Control
{
public:
    static void staticInit(TexturedSkin *skin)
    {
//        static constexpr std::string_view ActiveLabel="Active";
//        static constexpr std::string_view ControlActiveLabel=_Type::value;
//        static constexpr auto label=join<ControlActiveLabel, ActiveLabel>;

        //

//        static constexpr std::string_view ActiveEntry=join<_Type::value, ActiveLabel>;
        
        Active=skin->getTextureEntryId(join(_Type::value, "Inactive").value);
//        Inactive=skin->getTextureEntryId(_Type::value+"Inactive");
    }

    void generate(renderer::Base *renderer, controls::Base *control) override
    {}

    void update(renderer::Base *renderer, controls::Base *control) override
    {}

    void remove(renderer::Base *renderer, controls::Base *control) override
    {}

//    inline static constexpr std::string_view TabLabel="Tab";
//    inline static constexpr std::string_view name=join<_Type::value, TabLabel>;
//    inline static constexpr std::string_view name=join(_Type::value, "Tab");
    inline static constexpr const char name[]=join(_Type::value, "Tab").value;

    static size_t Active;
    static size_t Inactive;
};

class Tab:public Control
{
public:
    static void staticInit(TexturedSkin *skin);

    void generate(renderer::Base *renderer, controls::Base *control) override;
    void update(renderer::Base *renderer, controls::Base *control) override;
    void remove(renderer::Base *renderer, controls::Base *control) override;

    inline static const std::string_view name="Tab";

    TabItem<TabTop> m_tabTop;
    TabItem<TabBottom> m_tabBottom;
    TabItem<TabLeft> m_tabLeft;
    TabItem<TabRight> m_tabRight;

    static size_t Control;
    static size_t HeaderBar;

};

class TextBox:public Control
{
public:
    static void staticInit(TexturedSkin *skin);

    void generate(renderer::Base *renderer, controls::Base *control) override;
    void update(renderer::Base *renderer, controls::Base *control) override;
    void remove(renderer::Base *renderer, controls::Base *control) override;

    inline static const std::string_view name="TextBox";

    static size_t Normal;
    static size_t Focus;
    static size_t Disabled;

};

class Tooltip:public Control
{
public:
    static void staticInit(TexturedSkin *skin);

    void generate(renderer::Base *renderer, controls::Base *control) override;
    void update(renderer::Base *renderer, controls::Base *control) override;
    void remove(renderer::Base *renderer, controls::Base *control) override;

    inline static const std::string_view name="Tooltip";

    static size_t Background;
};

class Tree:public Control
{
public:
    static void staticInit(TexturedSkin *skin);

    void generate(renderer::Base *renderer, controls::Base *control) override;
    void update(renderer::Base *renderer, controls::Base *control) override;
    void remove(renderer::Base *renderer, controls::Base *control) override;

    inline static const std::string_view name="Tree";

    static size_t Background;
    static size_t Minus;
    static size_t Plus;
};

class Spinbox:public Control
{
public:
    static void staticInit(TexturedSkin *skin);

    void generate(renderer::Base *renderer, controls::Base *control) override;
    void update(renderer::Base *renderer, controls::Base *control) override;
    void remove(renderer::Base *renderer, controls::Base *control) override;

    static size_t UpNormal;
    static size_t UpHover;
    static size_t UpDown;
    static size_t UpDisabled;
    static size_t DownNormal;
    static size_t DownHover;
    static size_t DownDown;
    static size_t DownDisabled;
};

class Window:public Control
{
public:
    static void staticInit(TexturedSkin *skin);

    void generate(renderer::Base *renderer, controls::Base *control) override;
    void update(renderer::Base *renderer, controls::Base *control) override;
    void remove(renderer::Base *renderer, controls::Base *control) override;

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

};

}//namespace textured
}//namespace skin
}//namespace gweni

#endif//_gweni_skins_texturedControls_h_
