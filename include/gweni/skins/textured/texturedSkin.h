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
#include <gweni/skins/textured/tokyoNight.h>
#include <gweni/utils/json.h>

#include <glm/glm.hpp>
#include <glm/detail/type_vec4.hpp>
#include <imglib/simpleImage.h>

#include <map>
#include <set>
#include <string>
#include <unordered_map>

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
        {
            _Class::staticInit(skin);
            m_staticInit=true;
        }

        return std::unique_ptr<Control>(new _Class);
    }

     std::string getTypeName() override { return m_typeName; }

     // Most textured renderers paint one SDF surface (tokyonight::paintSurface,
     // which writes ids[0] only), so one slot is the default; a renderer that
     // draws more (an overlay icon, a fill) or nothing overrides it.
     size_t requiredPrimitives() override { return 1; }

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
        auto &funcs=controlFuncs();
        auto iter=std::find_if(funcs.begin(), funcs.end(),
            [&typeName](ControlEntry &entry){return (entry.name==typeName); });

        if(iter == funcs.end())
            return std::unique_ptr<Control>();

        std::unique_ptr<Control> control=iter->func(skin);

        if(control->getTypeName() != typeName)
            return std::unique_ptr<Control>();
        
        return control;
        
    }
    
    template<typename _Class> static const std::string registerControl()
    {
        std::string name{_Class::name.data(), _Class::name.size()};

        controlFuncs().emplace_back(name, &_Class::create);
        return name;
    }

private:
    // Construct-on-first-use: guarantees the registry is alive before any
    // static self-registration initializer (in another TU) touches it,
    // avoiding the static-initialization-order fiasco that otherwise lets a
    // later-constructed registry wipe entries already inserted into it.
    static std::vector<ControlEntry> &controlFuncs()
    {
        static std::vector<ControlEntry> funcs;
        return funcs;
    }
};

template<typename _Class> std::string ControlRegister<_Class>::m_typeName=\
ControlFactory::registerControl<_Class>();
template<typename _Class> forceInit<std::string &,ControlRegister<_Class>::m_typeName> ControlRegister<_Class>::m_forceTypeName;

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

    //! Load the skin config, a name resolved through the renderer's
    //! ResourcePaths (e.g. "DefaultSkin.json"). Returns false when the config
    //! or its atlas texture cannot be loaded, having printed one line naming
    //! the resolved path to stderr. The skin is still usable then, degraded:
    //! a config that is missing, unreadable, does not parse or is not a JSON
    //! object leaves every tokyonight surface in colors::Missing (magenta)
    //! with the compiled text colours; a missing atlas still applies the rest
    //! of the config.
    bool init(std::string fileName);

    //! Name -> text colour of this skin's Colors that a theme's "textColors"
    //! can set (LabelDefault, ButtonNormal, ...), the text counterpart of
    //! tokyonight::surfaceRegistry(). The pointers are into this skin, so the
    //! map is built for each call and is only valid while the skin lives.
    std::unordered_map<std::string, gweni::Color*> textColorRegistry();

    //! One of the skin's sizes a theme's "metrics" can set, and its range.
    struct MetricEntry
    {
        int *value;
        int minimum;
        int maximum;
    };
    //! Name -> size of this skin's Metrics a theme's "metrics" can set
    //! (ScrollBarThickness), its pointers into this skin as for
    //! textColorRegistry().
    std::unordered_map<std::string, MetricEntry> metricRegistry();

    //! Everything a theme sets, by name: every tokyonight surface (process
    //! globals, shared by every skin), and this skin's text colours and
    //! metrics. Sorted maps, so two are compared and written in one order.
    struct ThemeValues
    {
        std::map<std::string, tokyonight::Surface> surfaces;
        std::map<std::string, gweni::Color> textColors;
        std::map<std::string, int> metrics;

        bool operator==(const ThemeValues &other) const;
        bool operator!=(const ThemeValues &other) const { return !(*this == other); }
    };

    //! The theme as it is now.
    ThemeValues captureTheme();
    //! Sets every value \p values names (an unknown name is ignored). Nothing
    //! is redrawn: the controls read the new values when they draw again
    //! (tools::ThemeEditor::applyToCanvas() makes a canvas do that).
    void applyTheme(const ThemeValues &values);

    //! The theme as it is now as a "theme" object loadTheme() reads: every
    //! surface (top, bottom, border, radius, borderWidth), every text colour
    //! and every metric, each sorted by name, a colour as [r,g,b] or, when not
    //! opaque, [r,g,b,a]. All of them, not only what differs from the compiled
    //! defaults, so loading it sets the whole theme whatever was set before.
    //! The same theme always gives the same text. \p comment, if not empty,
    //! is written first as its "_comment".
    std::string themeJson(const std::string &comment=std::string());

    //! Applies the theme in a file at \p path (as given, not through the
    //! ResourcePaths): a skin config, whose "theme" object is applied, or a
    //! standalone theme file, an object of "surfaces", "textColors" and
    //! "metrics" itself, as loadTheme() reads them. What the file does not set
    //! keeps its current value. Returns false, with one stderr line naming the
    //! path, if the file cannot be read or parsed, or is no such object.
    bool loadThemeFile(const std::string &path);
    //! Writes themeJson() to \p path: replacing the "theme" object of a skin
    //! config there (the rest of the file kept as it is, byte for byte),
    //! adding one to a skin config that has none, or as the whole file when
    //! there is none or it is a standalone theme file. Saving the same theme
    //! again writes the same bytes. Returns false, with one stderr line, if
    //! the file there cannot be parsed (it is left untouched) or written.
    bool saveThemeFile(const std::string &path);
    //! \p document (a skin config or a standalone theme, or empty) with its
    //! theme replaced by \p theme (a themeJson()), as saveThemeFile() writes
    //! it. Returns false, setting \p error, if \p document does not parse as
    //! a JSON object or its "theme" is not an object.
    static bool replaceTheme(const std::string &document, const std::string &theme,
        std::string &result, std::string &error);

    std::unique_ptr<Control> getControlRenderer(std::string typeName) override;

    void generateControl();
    void drawControl(controls::Base *controls) override;

    size_t getColorId(std::string name);
    size_t getTextureEntryId(std::string_view name);

    void loadColors(const json::Value &jsonColors);
    bool loadConfig(std::string configFileName);
    //! The config's "monoFont" (a face name, setDefaultMonoFont's when the file is
    //! there, else the default font's face) and "fontFallback" (an object of face
    //! name -> array of face names, "default" for every face without its own),
    //! applied before the default font is loaded, which it then loads: the chains
    //! go to the renderer (IResourceLoader::setGlyphFallback). A face the config
    //! names whose file is missing is reported once on stderr and left out; false
    //! if one was.
    bool loadFonts(const json::Value &jsonDoc);
    void loadTextureMap(const json::Value &jsonMap);
    void setThemeColors();
    void loadTheme(const json::Value &jsonTheme);
    void reportStrictTheme(const std::set<std::string> &surfaceFieldsSet,
        const std::set<std::string> &textColorsSet);
    
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
