/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios

 *  See license in Gweni.h
 */


#ifndef _gweni_skins_texturedSkin_h_
#define _gweni_skins_texturedSkin_h_

#include <gweni/skin.h>
#include <gweni/gweni.h>
#include <gweni/controls/base.h>
#include <gweni/skins/textured/texturedPrimitives.h>
#include <gweni/utils/json.h>

#include <glm/glm.hpp>
#include <glm/detail/type_vec4.hpp>
#include <imglib/simpleImage.h>

namespace gweni
{
namespace skin
{


struct TextureMap
{
    std::string name;
    size_t textureId;

//    std::vector<>
};

struct TextureEntry
{
    TextureEntry(){}
    TextureEntry(std::string name, glm::ivec4 boundingBox):
        name(name), boundingBox(boundingBox), margin(0, 0, 0, 0){}
    TextureEntry(std::string name, glm::ivec4 boundingBox, glm::ivec4 margin):
        name(name), boundingBox(boundingBox), margin(margin){}

    std::string name;

    glm::ivec4 boundingBox;
    glm::ivec4 margin;
};

namespace textured
{

class TexturedSkin;

template<typename T, T> struct forceInit{ };

template<typename _Class>
class ControlRegister:public Control
{
public:
    ControlRegister() { static_assert(&m_typeName==&m_typeName); }

    static std::unique_ptr<Control> create(TexturedSkin *skin)
    {
        if(!m_staticInit)
            _Class::staticInit(skin);

        return std::unique_ptr<Control>(new _Class);
    }

     std::string getTypeName() override { return m_typeName; }

     static bool isStaticInit() { return m_staticInit; }

protected:
    inline static bool m_staticInit=false;

private:
    static std::string m_typeName;
    static forceInit<std::string &,m_typeName> m_forceTypeName;//force m_typeName to be generated

    
};

typedef  std::unique_ptr<Control> (*ControlFactoryFunc)(TexturedSkin *);

struct ControlEntry
{
    ControlEntry(const std::string &name, ControlFactoryFunc func):
        name(name), func(func) {}

    std::string name;
    ControlFactoryFunc func;
};

class ControlFactory
{
public:
    static std::unique_ptr<Control> create(TexturedSkin *skin, std::string typeName)
    {
        auto iter=std::find_if(m_controlFuncs.begin(), m_controlFuncs.end(),
            [&typeName](ControlEntry &entry){return (entry.name==typeName); });

        if(iter == m_controlFuncs.end())
            return std::unique_ptr<Control>();

        std::unique_ptr<Control> control=iter->func(skin);

        if(control->getTypeName() != typeName)
            return std::unique_ptr<Control>();
        
        return std::move(control);
        
    }
    
    template<typename _Class> static const std::string registerControl()
    {
        size_t index=m_controlFuncs.size();
        std::string name{_Class::name.data(), _Class::name.size()};

        m_controlFuncs.emplace_back(name, &_Class::create);
        return name;
    }

private:
    static std::vector<ControlEntry> m_controlFuncs;
};

template<typename _Class> std::string ControlRegister<_Class>::m_typeName=\
ControlFactory::registerControl<_Class>();
template<typename _Class> std::string ControlRegister<_Class>::m_forceTypeName;

//struct Textures_t
//{
//    texturing::Bordered StatusBar;
//    texturing::Bordered Selection;
//    texturing::Bordered Shadow;
//    texturing::Bordered Tooltip;
//
//    struct Panel_t
//    {
//        texturing::Bordered Normal;
//        texturing::Bordered Bright;
//        texturing::Bordered Dark;
//        texturing::Bordered Highlight;
//
//    } Panel;
//
//    struct Window_t
//    {
//        texturing::Bordered Normal;
//        texturing::Bordered Inactive;
//        texturing::Single Close;
//        texturing::Single Close_Hover;
//        texturing::Single Close_Down;
//        texturing::Single Maxi;
//        texturing::Single Maxi_Hover;
//        texturing::Single Maxi_Down;
//        texturing::Single Mini;
//        texturing::Single Mini_Hover;
//        texturing::Single Mini_Down;
//        texturing::Single Restore;
//        texturing::Single Restore_Hover;
//        texturing::Single Restore_Down;
//
//    } Window;
//
//    struct Checkbox_t
//    {
//        struct Active_t
//        {
//            texturing::Single Normal;
//            texturing::Single Checked;
//
//        } Active;
//
//        struct Disabled_t
//        {
//            texturing::Single Normal;
//            texturing::Single Checked;
//
//        } Disabled;
//
//    } CheckBox;
//
//    struct RadioButton_t
//    {
//        struct Active_t
//        {
//            texturing::Single Normal;
//            texturing::Single Checked;
//
//        } Active;
//
//        struct Disabled_t
//        {
//            texturing::Single Normal;
//            texturing::Single Checked;
//
//        } Disabled;
//
//    } RadioButton;
//
//    struct TextBox_t
//    {
//        texturing::Bordered Normal;
//        texturing::Bordered Focus;
//        texturing::Bordered Disabled;
//
//    } TextBox;
//
//    struct Tree_t
//    {
//        texturing::Bordered Background;
//        texturing::Single Minus;
//        texturing::Single Plus;
//
//    } Tree;
//
//
//    struct ProgressBar_t
//    {
//        texturing::Bordered Back;
//        texturing::Bordered Front;
//
//    } ProgressBar;
//
//    struct Scroller_t
//    {
//        texturing::Bordered TrackV;
//        texturing::Bordered ButtonV_Normal;
//        texturing::Bordered ButtonV_Hover;
//        texturing::Bordered ButtonV_Down;
//        texturing::Bordered ButtonV_Disabled;
//        texturing::Bordered TrackH;
//        texturing::Bordered ButtonH_Normal;
//        texturing::Bordered ButtonH_Hover;
//        texturing::Bordered ButtonH_Down;
//        texturing::Bordered ButtonH_Disabled;
//
//        struct Button_t
//        {
//            texturing::Bordered Normal[4];
//            texturing::Bordered Hover[4];
//            texturing::Bordered Down[4];
//            texturing::Bordered Disabled[4];
//
//        } Button;
//
//    } Scroller;
//
//    struct Menu_t
//    {
//        texturing::Single RightArrow;
//        texturing::Single Check;
//
//        texturing::Bordered Strip;
//        texturing::Bordered Background;
//        texturing::Bordered BackgroundWithMargin;
//        texturing::Bordered Hover;
//
//    } Menu;
//
//    struct Input_t
//    {
//        struct Button_t
//        {
//            texturing::Bordered Normal;
//            texturing::Bordered Hovered;
//            texturing::Bordered Disabled;
//            texturing::Bordered Pressed;
//
//        } Button;
//
//        struct ListBox_t
//        {
//            texturing::Bordered Background;
//            texturing::Bordered Hovered;
//
//            texturing::Bordered EvenLine;
//            texturing::Bordered OddLine;
//            texturing::Bordered EvenLineSelected;
//            texturing::Bordered OddLineSelected;
//
//        } ListBox;
//
//        struct UpDown_t
//        {
//            struct Up_t
//            {
//                texturing::Single Normal;
//                texturing::Single Hover;
//                texturing::Single Down;
//                texturing::Single Disabled;
//
//            } Up;
//
//            struct Down_t
//            {
//                texturing::Single Normal;
//                texturing::Single Hover;
//                texturing::Single Down;
//                texturing::Single Disabled;
//
//            } Down;
//
//        } UpDown;
//
//        struct ComboBox_t
//        {
//            texturing::Bordered Normal;
//            texturing::Bordered Hover;
//            texturing::Bordered Down;
//            texturing::Bordered Disabled;
//
//            struct Button_t
//            {
//                texturing::Single Normal;
//                texturing::Single Hover;
//                texturing::Single Down;
//                texturing::Single Disabled;
//
//            } Button;
//
//        } ComboBox;
//
//        struct Slider_t
//        {
//            struct H_t
//            {
//                texturing::Single Normal;
//                texturing::Single Hover;
//                texturing::Single Down;
//                texturing::Single Disabled;
//            } H;
//
//            struct V_t
//            {
//                texturing::Single Normal;
//                texturing::Single Hover;
//                texturing::Single Down;
//                texturing::Single Disabled;
//            } V;
//
//        } Slider;
//
//    } Input;
//
//    struct Tab_t
//    {
//        struct Bottom_t
//        {
//            texturing::Bordered Active;
//            texturing::Bordered Inactive;
//        } Bottom;
//
//        struct Top_t
//        {
//            texturing::Bordered Active;
//            texturing::Bordered Inactive;
//        } Top;
//
//        struct Left_t
//        {
//            texturing::Bordered Active;
//            texturing::Bordered Inactive;
//        } Left;
//
//        struct Right_t
//        {
//            texturing::Bordered Active;
//            texturing::Bordered Inactive;
//        } Right;
//
//        texturing::Bordered Control;
//        texturing::Bordered HeaderBar;
//
//    } Tab;
//
//    struct CategoryList_t
//    {
//        texturing::Bordered Outer;
//        texturing::Bordered Inner;
//        texturing::Bordered Header;
//
//    } CategoryList;
//
//    texturing::Bordered GroupBox;
//
//} Textures;

class TexturedSkin: public gweni::skin::Base
{
public:

    TexturedSkin(gweni::renderer::Base *renderer)
        : gweni::skin::Base(renderer)
    {}

    ~TexturedSkin()
    {
        m_render->getLoader().freeTexture(m_texture);
    }

    void init(std::string fileName);

    std::unique_ptr<Control> getControlRenderer(std::string typeName) override;

    void generateControl();
    void drawControl(controls::Base *controls) override;

    size_t getColorId(std::string name);
    size_t getTextureEntryId(std::string_view name);

    void loadColors(const json::Value &jsonColors);
    bool loadConfig(std::string configFileName);
    void loadTextureMap(const json::Value &jsonMap);
    
    typedef glm::tvec4<unsigned char> Color;
    typedef std::unordered_map<std::string, size_t> IndexMap;

    IndexMap m_colorIndex;
    std::vector<Color> m_colors;

    imglib::SimpleImage m_textureImage;

    Texture m_texture;
    IndexMap  m_textureEntryIndex;
    std::vector<TextureEntry> m_textureEntries;
};

}//namespace textured
}//namespace skin
}//namespace gweni

#endif // _gweni_skins_texturedSkin_h_
