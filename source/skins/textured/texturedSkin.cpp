#include "gweni/skins/textured/texturedSkin.h"

#include "gweni/utils/json.h"
#include "gweni/utils/filesystem.h"

#include <imglib/imageIo.h>

namespace gweni
{
namespace skin
{
namespace textured
{

std::vector<ControlEntry> ControlFactory::m_controlFuncs;

void TexturedSkin::init(std::string fileName)
{
    loadConfig(fileName);

    m_defaultFont.facename="Microsoft Sans Serif";
    m_defaultFont.size=11;

//    m_texture.name=TextureName;
//    m_texture.readable=true;
//    gweni::renderer::Base *render=getRenderer();
//    render->getLoader().loadTexture(m_texture); // Readable texture
//    const TextureData &texData=render->getTextureData(m_texture);


//    Colors.Window.TitleActive=render->pixelColor(m_texture, 4 + 8 * 0, 508, Color(255, 0, 0));
//    Colors.Window.TitleInactive=render->pixelColor(m_texture, 4 + 8 * 1, 508, Color(255, 255, 0));
//    Colors.Button.Normal=render->pixelColor(m_texture, 4 + 8 * 2, 508, Color(255, 255, 0));
//    Colors.Button.Hover=render->pixelColor(m_texture, 4 + 8 * 3, 508, Color(255, 255, 0));
//    Colors.Button.Down=render->pixelColor(m_texture, 4 + 8 * 2, 500, Color(255, 255, 0));
//    Colors.Button.Disabled=render->pixelColor(m_texture, 4 + 8 * 3, 500, Color(255, 255, 0));
//    Colors.Tab.Active.Normal=render->pixelColor(m_texture, 4 + 8 * 4, 508, Color(255, 255, 0));
//    Colors.Tab.Active.Hover=render->pixelColor(m_texture, 4 + 8 * 5, 508, Color(255, 255, 0));
//    Colors.Tab.Active.Down=render->pixelColor(m_texture, 4 + 8 * 4, 500, Color(255, 255, 0));
//    Colors.Tab.Active.Disabled=render->pixelColor(m_texture, 4 + 8 * 5, 500, Color(255, 255, 0));
//    Colors.Tab.Inactive.Normal=render->pixelColor(m_texture, 4 + 8 * 6, 508, Color(255, 255, 0));
//    Colors.Tab.Inactive.Hover=render->pixelColor(m_texture, 4 + 8 * 7, 508, Color(255, 255, 0));
//    Colors.Tab.Inactive.Down=render->pixelColor(m_texture, 4 + 8 * 6, 500, Color(255, 255, 0));
//    Colors.Tab.Inactive.Disabled=render->pixelColor(m_texture, 4 + 8 * 7, 500, Color(255, 255, 0));
//    Colors.Label.Default=render->pixelColor(m_texture, 4 + 8 * 8, 508, Color(255, 255, 0));
//    Colors.Label.Bright=render->pixelColor(m_texture, 4 + 8 * 9, 508, Color(255, 255, 0));
//    Colors.Label.Dark=render->pixelColor(m_texture, 4 + 8 * 8, 500, Color(255, 255, 0));
//    Colors.Label.Highlight=render->pixelColor(m_texture, 4 + 8 * 9, 500, Color(255, 255, 0));
//    Colors.Tree.Lines=render->pixelColor(m_texture, 4 + 8 * 10, 508, Color(255, 255, 0));
//    Colors.Tree.Normal=render->pixelColor(m_texture, 4 + 8 * 11, 508, Color(255, 255, 0));
//    Colors.Tree.Hover=render->pixelColor(m_texture, 4 + 8 * 10, 500, Color(255, 255, 0));
//    Colors.Tree.Selected=render->pixelColor(m_texture, 4 + 8 * 11, 500, Color(255, 255, 0));
//    Colors.Properties.Line_Normal=render->pixelColor(m_texture, 4 + 8 * 12, 508, Color(255, 255, 0));
//    Colors.Properties.Line_Selected=render->pixelColor(m_texture, 4 + 8 * 13, 508, Color(255, 255, 0));
//    Colors.Properties.Line_Hover=render->pixelColor(m_texture, 4 + 8 * 12, 500, Color(255, 255, 0));
//    Colors.Properties.Title=render->pixelColor(m_texture, 4 + 8 * 13, 500, Color(255, 255, 0));
//    Colors.Properties.Column_Normal=render->pixelColor(m_texture, 4 + 8 * 14, 508, Color(255, 255, 0));
//    Colors.Properties.Column_Selected=render->pixelColor(m_texture, 4 + 8 * 15, 508, Color(255, 255, 0));
//    Colors.Properties.Column_Hover=render->pixelColor(m_texture, 4 + 8 * 14, 500, Color(255, 255, 0));
//    Colors.Properties.Border=render->pixelColor(m_texture, 4 + 8 * 15, 500, Color(255, 255, 0));
//    Colors.Properties.Label_Normal=render->pixelColor(m_texture, 4 + 8 * 16, 508, Color(255, 255, 0));
//    Colors.Properties.Label_Selected=render->pixelColor(m_texture, 4 + 8 * 17, 508, Color(255, 255, 0));
//    Colors.Properties.Label_Hover=render->pixelColor(m_texture, 4 + 8 * 16, 500, Color(255, 255, 0));
//    Colors.ModalBackground=render->pixelColor(m_texture, 4 + 8 * 18, 508, Color(255, 255, 0));
//    Colors.TooltipText=render->pixelColor(m_texture, 4 + 8 * 19, 508, Color(255, 255, 0));
//    Colors.Category.Header=render->pixelColor(m_texture, 4 + 8 * 18, 500, Color(255, 255, 0));
//    Colors.Category.Header_Closed=render->pixelColor(m_texture, 4 + 8 * 19, 500, Color(255, 255, 0));
//    Colors.Category.Line.Text=render->pixelColor(m_texture, 4 + 8 * 20, 508, Color(255, 255, 0));
//    Colors.Category.Line.Text_Hover=render->pixelColor(m_texture, 4 + 8 * 21, 508, Color(255, 255, 0));
//    Colors.Category.Line.Text_Selected=render->pixelColor(m_texture, 4 + 8 * 20, 500, Color(255, 255, 0));
//    Colors.Category.Line.Button=render->pixelColor(m_texture, 4 + 8 * 21, 500, Color(255, 255, 0));
//    Colors.Category.Line.Button_Hover=render->pixelColor(m_texture, 4 + 8 * 22, 508, Color(255, 255, 0));
//    Colors.Category.Line.Button_Selected=render->pixelColor(m_texture, 4 + 8 * 23, 508, Color(255, 255, 0));
//    Colors.Category.LineAlt.Text=render->pixelColor(m_texture, 4 + 8 * 22, 500, Color(255, 255, 0));
//    Colors.Category.LineAlt.Text_Hover=render->pixelColor(m_texture, 4 + 8 * 23, 500, Color(255, 255, 0));
//    Colors.Category.LineAlt.Text_Selected=render->pixelColor(m_texture, 4 + 8 * 24, 508, Color(255, 255, 0));
//    Colors.Category.LineAlt.Button=render->pixelColor(m_texture, 4 + 8 * 25, 508, Color(255, 255, 0));
//    Colors.Category.LineAlt.Button_Hover=render->pixelColor(m_texture, 4 + 8 * 24, 500, Color(255, 255, 0));
//    Colors.Category.LineAlt.Button_Selected=render->pixelColor(m_texture, 4 + 8 * 25, 500, Color(255, 255, 0));
//    Textures.Shadow.init(&m_texture, texData, 448, 0, 31, 31, Margin(8, 8, 8, 8));
//    Textures.Tooltip.init(&m_texture, texData, 128, 320, 127, 31, Margin(8, 8, 8, 8));
//    Textures.StatusBar.init(&m_texture, texData, 128, 288, 127, 31, Margin(8, 8, 8, 8));
//    Textures.Selection.init(&m_texture, texData, 384, 32, 31, 31, Margin(4, 4, 4, 4));
//    Textures.Panel.Normal.init(&m_texture, texData, 256, 0, 63, 63, Margin(16, 16, 16, 16));
//    Textures.Panel.Bright.init(&m_texture, texData, 256 + 64, 0, 63, 63, Margin(16, 16, 16, 16));
//    Textures.Panel.Dark.init(&m_texture, texData, 256, 64, 63, 63, Margin(16, 16, 16, 16));
//    Textures.Panel.Highlight.init(&m_texture, texData, 256 + 64, 64, 63, 63, Margin(16, 16, 16, 16));
//    Textures.Window.Normal.init(&m_texture, texData, 0, 0, 127, 127, Margin(8, 32, 8, 8));
//    Textures.Window.Inactive.init(&m_texture, texData, 128, 0, 127, 127, Margin(8, 32, 8, 8));
//    Textures.CheckBox.Active.Checked.init(&m_texture, texData, 448, 32, 15, 15);
//    Textures.CheckBox.Active.Normal.init(&m_texture, texData, 464, 32, 15, 15);
//    Textures.CheckBox.Disabled.Checked.init(&m_texture, texData, 448, 48, 15, 15);
//    Textures.CheckBox.Disabled.Normal.init(&m_texture, texData, 464, 48, 15, 15);
//    Textures.RadioButton.Active.Checked.init(&m_texture, texData, 448, 64, 15, 15);
//    Textures.RadioButton.Active.Normal.init(&m_texture, texData, 464, 64, 15, 15);
//    Textures.RadioButton.Disabled.Checked.init(&m_texture, texData, 448, 80, 15, 15);
//    Textures.RadioButton.Disabled.Normal.init(&m_texture, texData, 464, 80, 15, 15);
//    Textures.TextBox.Normal.init(&m_texture, texData, 0, 150, 127, 21, Margin(4, 4, 4, 4));
//    Textures.TextBox.Focus.init(&m_texture, texData, 0, 172, 127, 21, Margin(4, 4, 4, 4));
//    Textures.TextBox.Disabled.init(&m_texture, texData, 0, 193, 127, 21, Margin(4, 4, 4, 4));
//    Textures.Menu.Strip.init(&m_texture, texData, 0, 128, 127, 21, Margin(1, 1, 1, 1));
//    Textures.Menu.BackgroundWithMargin.init(&m_texture, texData, 128, 128, 127, 63, Margin(24, 8, 8, 8));
//    Textures.Menu.Background.init(&m_texture, texData, 128, 192, 127, 63, Margin(8, 8, 8, 8));
//    Textures.Menu.Hover.init(&m_texture, texData, 128, 256, 127, 31, Margin(8, 8, 8, 8));
//    Textures.Menu.RightArrow.init(&m_texture, texData, 464, 112, 15, 15);
//    Textures.Menu.Check.init(&m_texture, texData, 448, 112, 15, 15);
//    Textures.Tab.Control.init(&m_texture, texData, 0, 256, 127, 127, Margin(8, 8, 8, 8));
//    Textures.Tab.Bottom.Active.init(&m_texture, texData, 0, 416, 63, 31, Margin(8, 8, 8, 8));
//    Textures.Tab.Bottom.Inactive.init(&m_texture, texData, 0 + 128, 416, 63, 31, Margin(8, 8, 8, 8));
//    Textures.Tab.Top.Active.init(&m_texture, texData, 0, 384, 63, 31, Margin(8, 8, 8, 8));
//    Textures.Tab.Top.Inactive.init(&m_texture, texData, 0 + 128, 384, 63, 31, Margin(8, 8, 8, 8));
//    Textures.Tab.Left.Active.init(&m_texture, texData, 64, 384, 31, 63, Margin(8, 8, 8, 8));
//    Textures.Tab.Left.Inactive.init(&m_texture, texData, 64 + 128, 384, 31, 63, Margin(8, 8, 8, 8));
//    Textures.Tab.Right.Active.init(&m_texture, texData, 96, 384, 31, 63, Margin(8, 8, 8, 8));
//    Textures.Tab.Right.Inactive.init(&m_texture, texData, 96 + 128, 384, 31, 63, Margin(8, 8, 8, 8));
//    Textures.Tab.HeaderBar.init(&m_texture, texData, 128, 352, 127, 31, Margin(4, 4, 4, 4));
//    Textures.Window.Close.init(&m_texture, texData, 32, 448, 31, 31);
//    Textures.Window.Close_Hover.init(&m_texture, texData, 64, 448, 31, 31);
//    Textures.Window.Close_Down.init(&m_texture, texData, 96, 448, 31, 31);
//    Textures.Window.Maxi.init(&m_texture, texData, 32 + 96 * 2, 448, 31, 31);
//    Textures.Window.Maxi_Hover.init(&m_texture, texData, 64 + 96 * 2, 448, 31, 31);
//    Textures.Window.Maxi_Down.init(&m_texture, texData, 96 + 96 * 2, 448, 31, 31);
//    Textures.Window.Restore.init(&m_texture, texData, 32 + 96 * 2, 448 + 32, 31, 31);
//    Textures.Window.Restore_Hover.init(&m_texture, texData, 64 + 96 * 2, 448 + 32, 31, 31);
//    Textures.Window.Restore_Down.init(&m_texture, texData, 96 + 96 * 2, 448 + 32, 31, 31);
//    Textures.Window.Mini.init(&m_texture, texData, 32 + 96, 448, 31, 31);
//    Textures.Window.Mini_Hover.init(&m_texture, texData, 64 + 96, 448, 31, 31);
//    Textures.Window.Mini_Down.init(&m_texture, texData, 96 + 96, 448, 31, 31);
//    Textures.Tree.Background.init(&m_texture, texData, 256, 128, 127, 127, Margin(16, 16, 16, 16));
//    Textures.Tree.Plus.init(&m_texture, texData, 448, 96, 15, 15);
//    Textures.Tree.Minus.init(&m_texture, texData, 464, 96, 15, 15);
//    Textures.Input.Button.Normal.init(&m_texture, texData, 480, 0, 31, 31, Margin(8, 8, 8, 8));
//    Textures.Input.Button.Hovered.init(&m_texture, texData, 480, 32, 31, 31, Margin(8, 8, 8, 8));
//    Textures.Input.Button.Disabled.init(&m_texture, texData, 480, 64, 31, 31, Margin(8, 8, 8, 8));
//    Textures.Input.Button.Pressed.init(&m_texture, texData, 480, 96, 31, 31, Margin(8, 8, 8, 8));
//
//    for(int i=0; i < 4; i++)
//    {
//        Textures.Scroller.Button.Normal[i].init(&m_texture, texData, 464 + 0, 208 + i * 16, 15, 15, Margin(2, 2, 2, 2));
//        Textures.Scroller.Button.Hover[i].init(&m_texture, texData, 480, 208 + i * 16, 15, 15, Margin(2, 2, 2, 2));
//        Textures.Scroller.Button.Down[i].init(&m_texture, texData, 464, 272 + i * 16, 15, 15, Margin(2, 2, 2, 2));
//        Textures.Scroller.Button.Disabled[i].init(&m_texture, texData, 480 + 48, 272 + i * 16, 15, 15, Margin(2, 2, 2, 2));
//    }
//
//    Textures.Scroller.TrackV.init(&m_texture, texData, 384, 208, 15, 127, Margin(4, 4, 4, 4));
//    Textures.Scroller.ButtonV_Normal.init(&m_texture, texData, 384 + 16, 208, 15, 127, Margin(4, 4, 4, 4));
//    Textures.Scroller.ButtonV_Hover.init(&m_texture, texData, 384 + 32, 208, 15, 127, Margin(4, 4, 4, 4));
//    Textures.Scroller.ButtonV_Down.init(&m_texture, texData, 384 + 48, 208, 15, 127, Margin(4, 4, 4, 4));
//    Textures.Scroller.ButtonV_Disabled.init(&m_texture, texData, 384 + 64, 208, 15, 127, Margin(4, 4, 4, 4));
//    Textures.Scroller.TrackH.init(&m_texture, texData, 384, 128, 127, 15, Margin(4, 4, 4, 4));
//    Textures.Scroller.ButtonH_Normal.init(&m_texture, texData, 384, 128 + 16, 127, 15, Margin(4, 4, 4, 4));
//    Textures.Scroller.ButtonH_Hover.init(&m_texture, texData, 384, 128 + 32, 127, 15, Margin(4, 4, 4, 4));
//    Textures.Scroller.ButtonH_Down.init(&m_texture, texData, 384, 128 + 48, 127, 15, Margin(4, 4, 4, 4));
//    Textures.Scroller.ButtonH_Disabled.init(&m_texture, texData, 384, 128 + 64, 127, 15, Margin(4, 4, 4, 4));
//    Textures.Input.ListBox.Background.init(&m_texture, texData, 256, 256, 63, 127, Margin(8, 8, 8, 8));
//    Textures.Input.ListBox.Hovered.init(&m_texture, texData, 320, 320, 31, 31, Margin(8, 8, 8, 8));
//    Textures.Input.ListBox.EvenLine.init(&m_texture, texData, 352, 256, 31, 31, Margin(8, 8, 8, 8));
//    Textures.Input.ListBox.OddLine.init(&m_texture, texData, 352, 288, 31, 31, Margin(8, 8, 8, 8));
//    Textures.Input.ListBox.EvenLineSelected.init(&m_texture, texData, 320, 256, 31, 31, Margin(8, 8, 8, 8));
//    Textures.Input.ListBox.OddLineSelected.init(&m_texture, texData, 320, 288, 31, 31, Margin(8, 8, 8, 8));
//    Textures.Input.ComboBox.Normal.init(&m_texture, texData, 384, 336, 127, 31, Margin(8, 8, 32, 8));
//    Textures.Input.ComboBox.Hover.init(&m_texture, texData, 384, 336 + 32, 127, 31, Margin(8, 8, 32, 8));
//    Textures.Input.ComboBox.Down.init(&m_texture, texData, 384, 336 + 64, 127, 31, Margin(8, 8, 32, 8));
//    Textures.Input.ComboBox.Disabled.init(&m_texture, texData, 384, 336 + 96, 127, 31, Margin(8, 8, 32, 8));
//    Textures.Input.ComboBox.Button.Normal.init(&m_texture, texData, 496, 272, 15, 15);
//    Textures.Input.ComboBox.Button.Hover.init(&m_texture, texData, 496, 272 + 16, 15, 15);
//    Textures.Input.ComboBox.Button.Down.init(&m_texture, texData, 496, 272 + 32, 15, 15);
//    Textures.Input.ComboBox.Button.Disabled.init(&m_texture, texData, 496, 272 + 48, 15, 15);
//    Textures.Input.UpDown.Up.Normal.init(&m_texture, texData, 384, 112, 7, 7);
//    Textures.Input.UpDown.Up.Hover.init(&m_texture, texData, 384 + 8, 112, 7, 7);
//    Textures.Input.UpDown.Up.Down.init(&m_texture, texData, 384 + 16, 112, 7, 7);
//    Textures.Input.UpDown.Up.Disabled.init(&m_texture, texData, 384 + 24, 112, 7, 7);
//    Textures.Input.UpDown.Down.Normal.init(&m_texture, texData, 384, 120, 7, 7);
//    Textures.Input.UpDown.Down.Hover.init(&m_texture, texData, 384 + 8, 120, 7, 7);
//    Textures.Input.UpDown.Down.Down.init(&m_texture, texData, 384 + 16, 120, 7, 7);
//    Textures.Input.UpDown.Down.Disabled.init(&m_texture, texData, 384 + 24, 120, 7, 7);
//    Textures.ProgressBar.Back.init(&m_texture, texData, 384, 0, 31, 31, Margin(8, 8, 8, 8));
//    Textures.ProgressBar.Front.init(&m_texture, texData, 384 + 32, 0, 31, 31, Margin(8, 8, 8, 8));
//    Textures.Input.Slider.H.Normal.init(&m_texture, texData, 416, 32, 15, 15);
//    Textures.Input.Slider.H.Hover.init(&m_texture, texData, 416, 32 + 16, 15, 15);
//    Textures.Input.Slider.H.Down.init(&m_texture, texData, 416, 32 + 32, 15, 15);
//    Textures.Input.Slider.H.Disabled.init(&m_texture, texData, 416, 32 + 48, 15, 15);
//    Textures.Input.Slider.V.Normal.init(&m_texture, texData, 416 + 16, 32, 15, 15);
//    Textures.Input.Slider.V.Hover.init(&m_texture, texData, 416 + 16, 32 + 16, 15, 15);
//    Textures.Input.Slider.V.Down.init(&m_texture, texData, 416 + 16, 32 + 32, 15, 15);
//    Textures.Input.Slider.V.Disabled.init(&m_texture, texData, 416 + 16, 32 + 48, 15, 15);
//    Textures.CategoryList.Outer.init(&m_texture, texData, 256, 384, 63, 63, Margin(8, 8, 8, 8));
//    Textures.CategoryList.Inner.init(&m_texture, texData, 256 + 64, 384, 63, 63, Margin(8, 21, 8, 8));
//    Textures.CategoryList.Header.init(&m_texture, texData, 320, 352, 63, 31, Margin(8, 8, 8, 8));
//    Textures.GroupBox.init(&m_texture, texData, 0, 448, 31, 31, Margin(8, 8, 8, 8));
}

std::unique_ptr<Control> TexturedSkin::getControlRenderer(std::string typeName)
{
    return ControlFactory::create(this, typeName);
}

void TexturedSkin::drawControl(controls::Base *controls)
{

}

size_t TexturedSkin::getColorId(std::string name)
{
    IndexMap::iterator iter=m_colorIndex.find(name);

    if(iter == m_colorIndex.end())
        return std::numeric_limits<size_t>::max();

    return iter->second;
}

void TexturedSkin::loadColors(const json::Value &jsonColors)
{
    if(!jsonColors.IsArray())
        return;


    for(const json::Value &jsonColor:jsonColors.GetArray())
    {
        std::string name=rapidjson::getValue<std::string>(jsonColor, "name");
        std::vector<uint8_t> colors=rapidjson::getVector<uint8_t>(jsonColor, "color");

        if(colors.size() < 3)
            continue;

        m_colors.emplace_back(colors[0], colors[1], colors[2], 255);
        m_colorIndex[name]=m_colors.size();
    }
}

size_t TexturedSkin::getTextureEntryId(std::string_view name)
{
    IndexMap::iterator iter=m_textureEntryIndex.find(name.data());

    if(iter == m_textureEntryIndex.end())
        return std::numeric_limits<size_t>::max();

    return iter->second;
}

void TexturedSkin::loadTextureMap(const json::Value &jsonMap)
{
    if(!jsonMap.IsArray())
        return;

    for(const json::Value &jsonEntry:jsonMap.GetArray())
    {
        std::string name=rapidjson::getValue<std::string>(jsonEntry, "name");
        std::vector<uint64_t> boundingBox=rapidjson::getVector<uint64_t>(jsonEntry, "bbox");
        std::vector<uint64_t> margin;

        if(jsonEntry.HasMember("margin"))
            margin=rapidjson::getVector<uint64_t>(jsonEntry, "margin");

        if(margin.empty())
        {
            m_textureEntries.emplace_back(name, glm::ivec4{boundingBox[0], boundingBox[1], boundingBox[2], boundingBox[3]});
        }
        else
        {
            m_textureEntries.emplace_back(name,
                glm::ivec4{boundingBox[0], boundingBox[1], boundingBox[2], boundingBox[3]},
                glm::ivec4{margin[0], margin[1], margin[2], margin[3]}
            );
        }
        m_textureEntryIndex.emplace(name, m_textureEntries.size()-1);
    }
}

bool TexturedSkin::loadConfig(std::string configFileName)
{
    std::vector<char> fileBuffer;
    
    {
        if(!fs::exists(configFileName.c_str()))
        {
            std::cout<<"Unable to locate config file "<<configFileName<<"\n";
            return false;
        }

        FILE *configFile=fopen(configFileName.c_str(), "r");
    
        fseek(configFile, 0, SEEK_END);
        size_t configFileSize=(size_t)ftell(configFile);
        fseek(configFile, 0, SEEK_SET);
    
        fileBuffer.resize(configFileSize+1);
        fread(fileBuffer.data(), 1, configFileSize, configFile);
        fileBuffer.back()='\0';

        fclose(configFile);
    }

    json::Document jsonDoc;
    
    jsonDoc.ParseInsitu(fileBuffer.data());

    if(jsonDoc.HasParseError())
    {
        json::OffsetInfo offsetInfo=json::getOffsetInfo(fileBuffer, jsonDoc.GetErrorOffset());

        std::cout<<"Skin parse error(offset "<<offsetInfo.line<<", "<<offsetInfo.pos<<"): "<<json::GetParseError_En(jsonDoc.GetParseError())<<"\n";
        return false;
    }

    if(jsonDoc.HasMember("colors"))
        loadColors(jsonDoc["colors"]);

    if(jsonDoc.HasMember("textureFileName"))
    {
        std::string fileName=rapidjson::getValue<std::string>(jsonDoc, "textureFileName");

        m_texture.name=fileName;
        m_texture.readable=true;
        
        getRenderer()->getLoader().loadTexture(m_texture);
    }
        
    if(jsonDoc.HasMember("map"))
        loadTextureMap(jsonDoc["map"]);

    return true;
}


//void drawButton(gweni::controls::Base *control, State state, bool depressed, bool hovered, bool disabled) override
//{
//    if(state == State::Generate)
//    {
//        Textures.Input.Button.Normal.generate(getRenderer(), control);
//    }
//    else if(state == State::Update)
//    {
//        if(disabled)
//            return Textures.Input.Button.Disabled.draw(getRenderer(), control, control->getRenderBounds());
//
//        if(depressed)
//            return Textures.Input.Button.Pressed.draw(getRenderer(), control, control->getRenderBounds());
//
//        if(hovered)
//            return Textures.Input.Button.Hovered.draw(getRenderer(), control, control->getRenderBounds());
//
//        Textures.Input.Button.Normal.draw(getRenderer(), control, control->getRenderBounds());
//    }
//    else if(state == State::Remove)
//    {
//        Textures.Input.Button.Normal.remove(getRenderer(), control);
//    }
//}
//
//void drawMenuItem(gweni::controls::Base *control, State state, bool submenuOpen, bool checked) override
//{
//    if(state == State::Generate)
//    {
//        Textures.Menu.Check.generate(getRenderer(), control);
//    }
//    else if(state == State::Update)
//    {
//        const gweni::Rect &rect=control->getRenderBounds();
//
//        if(submenuOpen || control->isHovered())
//            Textures.Menu.Hover.draw(getRenderer(), control, rect);
//
//        if(checked)
//            Textures.Menu.Check.draw(getRenderer(), control, gweni::Rect(rect.x + 4, rect.y + 3, 15, 15));
//    }
//    else if(state == State::Remove)
//    {
//        Textures.Menu.Check.remove(getRenderer(), control);
//    }
//}
//
//void drawMenuStrip(gweni::controls::Base *control, State state) override
//{
//    if(state == State::Generate)
//    {
//        Textures.Menu.Strip.generate(getRenderer(), control);
//    }
//    else if(state == State::Update)
//    {
//        Textures.Menu.Strip.draw(getRenderer(), control, control->getRenderBounds());
//    }
//    else if(state == State::Remove)
//    {
//        Textures.Menu.Strip.remove(getRenderer(), control);
//    }
//}
//
//void drawMenu(gweni::controls::Base *control, State state, bool paddingDisabled) override
//{
//    if(state == State::Generate)
//    {
//        Textures.Menu.Strip.generate(getRenderer(), control);
//    }
//    else if(state == State::Update)
//    {
//        if(!paddingDisabled)
//        {
//            return Textures.Menu.BackgroundWithMargin.draw(getRenderer(), control, control->getRenderBounds());
//        }
//
//        Textures.Menu.Background.draw(getRenderer(), control, control->getRenderBounds());
//    }
//    else if(state == State::Remove)
//    {
//        Textures.Menu.Strip.remove(getRenderer(), control);
//    }
//}
//
//void drawMenuRightArrow(controls::Base *control, State state) override
//{
//    if(state == State::Generate)
//    {
//        Textures.Menu.RightArrow.generate(getRenderer(), control);
//    }
//    else if(state == State::Update)
//    {
//        Textures.Menu.RightArrow.draw(getRenderer(), control, control->getRenderBounds());
//    }
//    else if(state == State::Remove)
//    {
//        Textures.Menu.RightArrow.remove(getRenderer(), control);
//    }
//}
//
//void drawShadow(gweni::controls::Base *control, State state) override
//{
//    if(state == State::Generate)
//    {
//        Textures.Shadow.generate(getRenderer(), control);
//    }
//    else if(state == State::Update)
//    {
//        const gweni::Rect r=control->getRenderBounds() + Rect(-4, -4, 10, 10);
//        Textures.Shadow.draw(getRenderer(), control, r);
//    }
//    else if(state == State::Remove)
//    {
//        Textures.Shadow.remove(getRenderer(), control);
//    }
//}
//
//void drawRadioButton(gweni::controls::Base *control, State state, bool selected, bool depressed) override
//{
//    if(state == State::Generate)
//    {
//        Textures.RadioButton.Active.Normal.generate(getRenderer(), control);
//    }
//    else if(state == State::Update)
//    {
//        if(selected)
//        {
//            if(control->isDisabled())
//                Textures.RadioButton.Disabled.Checked.draw(getRenderer(), control, control->getRenderBounds());
//            else
//                Textures.RadioButton.Active.Checked.draw(getRenderer(), control, control->getRenderBounds());
//        }
//        else
//        {
//            if(control->isDisabled())
//                Textures.RadioButton.Disabled.Normal.draw(getRenderer(), control, control->getRenderBounds());
//            else
//                Textures.RadioButton.Active.Normal.draw(getRenderer(), control, control->getRenderBounds());
//        }
//    }
//    else if(state == State::Remove)
//    {
//        Textures.RadioButton.Active.Normal.remove(getRenderer(), control);
//    }
//}
//
//void drawCheckBox(gweni::controls::Base *control, State state, bool selected, bool depressed) override
//{
//    if(state == State::Generate)
//    {
//        Textures.CheckBox.Active.Normal.generate(getRenderer(), control);
//    }
//    else if(state == State::Update)
//    {
//        if(selected)
//        {
//            if(control->isDisabled())
//                Textures.CheckBox.Disabled.Checked.draw(getRenderer(), control, control->getRenderBounds());
//            else
//                Textures.CheckBox.Active.Checked.draw(getRenderer(), control, control->getRenderBounds());
//        }
//        else
//        {
//            if(control->isDisabled())
//                Textures.CheckBox.Disabled.Normal.draw(getRenderer(), control, control->getRenderBounds());
//            else
//                Textures.CheckBox.Active.Normal.draw(getRenderer(), control, control->getRenderBounds());
//        }
//    }
//    else if(state == State::Remove)
//    {
//        Textures.CheckBox.Active.Normal.remove(getRenderer(), control);
//    }
//}
//
//void drawGroupBox(gweni::controls::Base *control, State state, int textStart, int textHeight, int textWidth) override
//{
//    if(state == State::Generate)
//    {
//        Textures.GroupBox.generate(getRenderer(), control);
//    }
//    else if(state == State::Update)
//    {
//        gweni::Rect rect=control->getRenderBounds();
//        rect.y+=textHeight/2;
//        rect.h-=textHeight/2;
//        Textures.GroupBox.draw(getRenderer(), control, rect, gweni::colors::White, ~(1<<1));
//        rect.x+=textStart + textWidth - 4;
//        rect.w-=textStart + textWidth - 4;
//        Textures.GroupBox.draw(getRenderer(), control, rect, gweni::colors::White, (1<<1));
//    }
//    else if(state == State::Remove)
//    {
//        Textures.GroupBox.remove(getRenderer(), control);
//    }
//}
//
//void drawTextBox(gweni::controls::Base *control, State state) override
//{
//    if(state == State::Generate)
//    {
//        Textures.TextBox.Normal.generate(getRenderer(), control);
//    }
//    else if(state == State::Update)
//    {
//        if(control->isDisabled())
//            return Textures.TextBox.Disabled.draw(getRenderer(), control, control->getRenderBounds());
//
//        if(control->isFocussed())
//            Textures.TextBox.Focus.draw(getRenderer(), control, control->getRenderBounds());
//        else
//            Textures.TextBox.Normal.draw(getRenderer(), control, control->getRenderBounds());
//    }
//    else if(state == State::Remove)
//    {
//        Textures.TextBox.Normal.remove(getRenderer(), control);
//    }
//}
//
//virtual void drawActiveTabButton(gweni::controls::Base *control, State state, Position dir)
//{
//    if(state == State::Generate)
//    {
//        Textures.Tab.Right.Active.generate(getRenderer(), control);
//    }
//    else if(state == State::Update)
//    {
//        if(dir == Position::Bottom)
//            Textures.Tab.Bottom.Active.draw(getRenderer(), control, control->getRenderBounds() + Rect(0, -8, 0, 8));
//        else if(dir == Position::Top)
//            Textures.Tab.Top.Active.draw(getRenderer(), control, control->getRenderBounds() + Rect(0, 0, 0, 8));
//        else if(dir == Position::Left)
//            Textures.Tab.Left.Active.draw(getRenderer(), control, control->getRenderBounds() + Rect(0, 0, 8, 0));
//        else if(dir == Position::Right)
//            Textures.Tab.Right.Active.draw(getRenderer(), control, control->getRenderBounds() + Rect(-8, 0, 8, 0));
//    }
//    else if(state == State::Remove)
//    {
//        Textures.Tab.Right.Active.remove(getRenderer(), control);
//    }
//}
//
//void drawTabButton(gweni::controls::Base *control, State state, bool active, Position dir) override
//{
//    if(state == State::Generate)
//    {
//        Textures.Tab.Right.Inactive.generate(getRenderer(), control);
//    }
//    else if(state == State::Update)
//    {
//        if(active)
//        {
//            drawActiveTabButton(control, state, dir);
//            return;
//        }
//
//        if(dir == Position::Bottom)
//            Textures.Tab.Bottom.Inactive.draw(getRenderer(), control, control->getRenderBounds());
//        else if(dir == Position::Top)
//            Textures.Tab.Top.Inactive.draw(getRenderer(), control, control->getRenderBounds());
//        else if(dir == Position::Left)
//            Textures.Tab.Left.Inactive.draw(getRenderer(), control, control->getRenderBounds());
//        else if(dir == Position::Right)
//            Textures.Tab.Right.Inactive.draw(getRenderer(), control, control->getRenderBounds());
//    }
//    else if(state == State::Remove)
//    {
//        Textures.Tab.Right.Inactive.remove(getRenderer(), control);
//    }
//}
//
//void drawTabControl(gweni::controls::Base *control, State state) override
//{
//    if(state == State::Generate)
//    {
//        Textures.Tab.Control.generate(getRenderer(), control);
//    }
//    else if(state == State::Update)
//    {
//        Textures.Tab.Control.draw(getRenderer(), control, control->getRenderBounds());
//    }
//    else if(state == State::Remove)
//    {
//        Textures.Tab.Control.remove(getRenderer(), control);
//    }
//}
//
//void drawTabTitleBar(gweni::controls::Base *control, State state) override
//{
//    if(state == State::Generate)
//    {
//        Textures.Tab.HeaderBar.generate(getRenderer(), control);
//    }
//    else if(state == State::Update)
//    {
//        Textures.Tab.HeaderBar.draw(getRenderer(), control, control->getRenderBounds());
//    }
//    else if(state == State::Remove)
//    {
//        Textures.Tab.HeaderBar.remove(getRenderer(), control);
//    }
//}
//
//void drawGenericPanel(controls::Base *control, State state) override
//{
//    if(state == State::Generate)
//    {
//        Textures.Panel.Normal.generate(getRenderer(), control);
//    }
//    else if(state == State::Update)
//    {
//        Textures.Panel.Normal.draw(getRenderer(), control, control->getRenderBounds());
//    }
//    else if(state == State::Remove)
//    {
//        Textures.Panel.Normal.remove(getRenderer(), control);
//    }
//}
//
//void drawWindow(gweni::controls::Base *control, State state, int topHeight, bool inFocus) override
//{
//    if(state == State::Generate)
//    {
//        Textures.Window.Normal.generate(getRenderer(), control);
//    }
//    else if(state == State::Update)
//    {
//        if(inFocus)
//            Textures.Window.Normal.draw(getRenderer(), control, control->getRenderBounds());
//        else
//            Textures.Window.Inactive.draw(getRenderer(), control, control->getRenderBounds());
//    }
//    else if(state == State::Remove)
//    {
//        Textures.Window.Normal.remove(getRenderer(), control);
//    }
//}
//
//void drawHighlight(gweni::controls::Base *control, State state) override
//{
//    PrimitiveIds &primitiveIds=control->getPrimitiveIds();
//    if(primitiveIds.size() < 1)
//        enlargePrimitiveIds(control, primitiveIds, 1);
//
//    if(state == State::Generate)
//    {
//    }
//    else if(state == State::Update)
//    {
//        gweni::Rect rect=control->getRenderBounds();
//        getRenderer()->setDrawColor(gweni::Color(255, 100, 255, 255));
//        getRenderer()->drawFilledRect(primitiveIds[0], rect);
//    }
//    else if(state == State::Remove)
//    {
//    }
//}
//
//void drawScrollBar(gweni::controls::Base *control, State state, bool isHorizontal, bool depressed) override
//{
//    if(state == State::Generate)
//    {
//        Textures.Scroller.TrackH.generate(getRenderer(), control);
//    }
//    else if(state == State::Update)
//    {
//        if(isHorizontal)
//            Textures.Scroller.TrackH.draw(getRenderer(), control, control->getRenderBounds());
//        else
//            Textures.Scroller.TrackV.draw(getRenderer(), control, control->getRenderBounds());
//    }
//    else if(state == State::Remove)
//    {
//        Textures.Scroller.TrackH.remove(getRenderer(), control);
//    }
//}
//
//void drawScrollBarBar(controls::Base *control, State state, bool depressed, bool isHovered, bool isHorizontal) override
//{
//    if(state == State::Generate)
//    {
//        return Textures.Scroller.ButtonV_Normal.generate(getRenderer(), control);
//    }
//    else if(state == State::Update)
//    {
//        if(!isHorizontal)
//        {
//            if(control->isDisabled())
//                return Textures.Scroller.ButtonV_Disabled.draw(getRenderer(), control, control->getRenderBounds());
//
//            if(depressed)
//                return Textures.Scroller.ButtonV_Down.draw(getRenderer(), control, control->getRenderBounds());
//
//            if(isHovered)
//                return Textures.Scroller.ButtonV_Hover.draw(getRenderer(), control, control->getRenderBounds());
//
//            return Textures.Scroller.ButtonV_Normal.draw(getRenderer(), control, control->getRenderBounds());
//        }
//
//        if(control->isDisabled())
//            return Textures.Scroller.ButtonH_Disabled.draw(getRenderer(), control, control->getRenderBounds());
//
//        if(depressed)
//            return Textures.Scroller.ButtonH_Down.draw(getRenderer(), control, control->getRenderBounds());
//
//        if(isHovered)
//            return Textures.Scroller.ButtonH_Hover.draw(getRenderer(), control, control->getRenderBounds());
//
//        return Textures.Scroller.ButtonH_Normal.draw(getRenderer(), control, control->getRenderBounds());
//    }
//    else if(state == State::Remove)
//    {
//        return Textures.Scroller.ButtonV_Normal.remove(getRenderer(), control);
//    }
//}
//
//void drawProgressBar(gweni::controls::Base *control, State state, bool isHorizontal, float progress) override
//{
//    if(state == State::Generate)
//    {
//        Textures.ProgressBar.Front.generate(getRenderer(), control);
//    }
//    else if(state == State::Update)
//    {
//        gweni::Rect rect=control->getRenderBounds();
//        const gweni::Color FillColor(0, 211, 40, 255);
//
//        if(isHorizontal)
//        {
//            Textures.ProgressBar.Back.draw(getRenderer(), control, rect);
//            rect.w*=progress;
//
//            if(rect.w > 0)
//                Textures.ProgressBar.Front.draw(getRenderer(), control, rect);
//        }
//        else
//        {
//            Textures.ProgressBar.Back.draw(getRenderer(), control, rect);
//            const int inv_progress=rect.h * (1.f - progress);
//            rect.y+=inv_progress;
//            rect.h-=inv_progress;
//            Textures.ProgressBar.Front.draw(getRenderer(), control, rect);
//        }
//    }
//    else if(state == State::Remove)
//    {
//        Textures.ProgressBar.Front.remove(getRenderer(), control);
//    }
//}
//
//void drawListBox(gweni::controls::Base *control, State state) override
//{
//    if(state == State::Generate)
//    {
//        Textures.Input.ListBox.Background.generate(getRenderer(), control);
//    }
//    else if(state == State::Update)
//    {
//        return Textures.Input.ListBox.Background.draw(getRenderer(), control, control->getRenderBounds());
//    }
//    else if(state == State::Remove)
//    {
//        Textures.Input.ListBox.Background.remove(getRenderer(), control);
//    }
//}
//
//void drawListBoxLine(gweni::controls::Base *control, State state, bool selected, bool even) override
//{
//    if(state == State::Generate)
//    {
//        Textures.Input.ListBox.EvenLine.generate(getRenderer(), control);
//    }
//    else if(state == State::Update)
//    {
//        if(selected)
//        {
//            if(even)
//                return Textures.Input.ListBox.EvenLineSelected.draw(getRenderer(), control, control->getRenderBounds());
//            else
//                return Textures.Input.ListBox.OddLineSelected.draw(getRenderer(), control, control->getRenderBounds());
//        }
//
//        if(control->isHovered())
//            return Textures.Input.ListBox.Hovered.draw(getRenderer(), control, control->getRenderBounds());
//
//        if(even)
//            return Textures.Input.ListBox.EvenLine.draw(getRenderer(), control, control->getRenderBounds());
//
//        return Textures.Input.ListBox.OddLine.draw(getRenderer(), control, control->getRenderBounds());
//    }
//    else if(state == State::Remove)
//    {
//        Textures.Input.ListBox.EvenLine.remove(getRenderer(), control);
//    }
//}
//
//virtual void drawSliderNotchesH(gweni::controls::Base *control, State state, gweni::Rect rect, int numNotches, int dist)
//{
//    if(state == State::Generate)
//    {
//    }
//    else if(state == State::Update)
//    {
//        if(numNotches == 0)
//            return;
//
//        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
//
//        const float spacing=static_cast<float>(rect.w) / numNotches;
//
//        for(int i=0; i < numNotches + 1; ++i)
//        {
//            getRenderer()->drawFilledRect(primitiveIds[i], gweni::Rect(rect.x + spacing * i, rect.y + dist - 2, 1, 5));
//        }
//    }
//    else if(state == State::Remove)
//    {
//    }
//}
//
//virtual void drawSliderNotchesV(gweni::controls::Base *control, State state, gweni::Rect rect, int numNotches, int dist)
//{
//    if(state == State::Generate)
//    {
//    }
//    else if(state == State::Update)
//    {
//        if(numNotches == 0)
//            return;
//
//        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
//
//        const float spacing=static_cast<float>(rect.h) / numNotches;
//
//        for(int i=0; i < numNotches + 1; ++i)
//        {
//            getRenderer()->drawFilledRect(primitiveIds[i], gweni::Rect(rect.x + dist - 1, rect.y + spacing * i, 5, 1));
//        }
//    }
//    else if(state == State::Remove)
//    {
//    }
//}
//
//void drawSlider(gweni::controls::Base *control, State state, bool isHorizontal, int numNotches, int barSize) override
//{
//    PrimitiveIds &primitiveIds=control->getPrimitiveIds();
//    if(primitiveIds.size() < numNotches+1)
//        enlargePrimitiveIds(control, primitiveIds, numNotches+1);
//
//    if(state == State::Generate)
//    {
//    }
//    else if(state == State::Update)
//    {
//        if(isHorizontal)
//        {
//            gweni::Rect rect=control->getRenderBounds();
//            rect.x+=barSize/2;
//            rect.w-=barSize;
//            rect.y+=rect.h/2 - 1;
//            rect.h=1;
//            getRenderer()->setDrawColor(gweni::Color(0, 0, 0, 100));
//            drawSliderNotchesH(control, state, rect, numNotches, barSize/2);
//            return getRenderer()->drawFilledRect(primitiveIds[numNotches], rect);
//        }
//
//        gweni::Rect rect=control->getRenderBounds();
//        rect.y+=barSize/2;
//        rect.h-=barSize;
//        rect.x+=rect.w/2 - 1;
//        rect.w=1;
//        getRenderer()->setDrawColor(gweni::Color(0, 0, 0, 100));
//        drawSliderNotchesV(control, state, rect, numNotches, barSize * 0.4);
//        return getRenderer()->drawFilledRect(primitiveIds[numNotches], rect);
//    }
//    else if(state == State::Remove)
//    {
//    }
//}
//
//void drawComboBox(gweni::controls::Base *control, State state, bool down, bool menuOpen) override
//{
//    if(state == State::Generate)
//    {
//        Textures.Input.ComboBox.Normal.generate(getRenderer(), control);
//    }
//    else if(state == State::Update)
//    {
//        if(control->isDisabled())
//            return Textures.Input.ComboBox.Disabled.draw(getRenderer(), control, control->getRenderBounds());
//
//        if(down || menuOpen)
//            return Textures.Input.ComboBox.Down.draw(getRenderer(), control, control->getRenderBounds());
//
//        if(control->isHovered())
//            return Textures.Input.ComboBox.Hover.draw(getRenderer(), control, control->getRenderBounds());
//
//        Textures.Input.ComboBox.Normal.draw(getRenderer(), control, control->getRenderBounds());
//    }
//    else if(state == State::Remove)
//    {
//        Textures.Input.ComboBox.Normal.remove(getRenderer(), control);
//    }
//}
//
//void drawKeyboardHighlight(gweni::controls::Base *control, State state, const gweni::Rect &r, int offset) override
//{
//    if(state == State::Generate)
//    {
//    }
//    else if(state == State::Update)
//    {
//        gweni::Rect rect=r;
//        rect.x+=offset;
//        rect.y+=offset;
//        rect.w-=offset * 2;
//        rect.h-=offset * 2;
//        //draw the top and bottom
//        bool skip=true;
//
//        size_t primitves=rect.w+rect.h;
//        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
//        if(primitiveIds.size() < primitves)
//            enlargePrimitiveIds(control, primitiveIds, primitves);
//
//        size_t index=0;
//        for(int i=0; i < rect.w/2; i++)
//        {
//            m_render->setDrawColor(gweni::Color(0, 0, 0, 255));
//
//            if(!skip)
//            {
//                getRenderer()->drawPixel(&primitiveIds[index++], rect.x + (i * 2), rect.y);
//                getRenderer()->drawPixel(&primitiveIds[index++], rect.x + (i * 2), rect.y + rect.h - 1);
//            }
//            else
//            {
//                skip=!skip;
//            }
//        }
//
//        skip=false;
//
//        for(int i=0; i < rect.h/2; i++)
//        {
//            getRenderer()->setDrawColor(gweni::Color(0, 0, 0, 255));
//
//            if(!skip)
//            {
//                getRenderer()->drawPixel(&primitiveIds[index++], rect.x, rect.y + i * 2);
//                getRenderer()->drawPixel(&primitiveIds[index++], rect.x + rect.w - 1, rect.y + i * 2);
//            }
//            else
//            {
//                skip=!skip;
//            }
//        }
//    }
//    else if(state == State::Remove)
//    {
//    }
//}
//
//void drawTooltip(gweni::controls::Base *control, State state) override
//{
//    if(state == State::Generate)
//    {
//    }
//    else if(state == State::Update)
//    {
//        return Textures.Tooltip.draw(getRenderer(), control, control->getRenderBounds());
//    }
//    else if(state == State::Remove)
//    {
//    }
//}
//
//void drawScrollButton(gweni::controls::Base *control, State state, Position direction,
//    bool depressed, bool hovered, bool disabled) override
//{
//    if(state == State::Generate)
//    {
//    }
//    else if(state == State::Update)
//    {
//        int i=0;
//
//        if(direction == Position::Top) { i=1; }
//        else if(direction == Position::Right) { i=2; }
//        else if(direction == Position::Bottom) { i=3; }
//
//        if(disabled)
//            return Textures.Scroller.Button.Disabled[i].draw(getRenderer(), control, control->getRenderBounds());
//
//        if(depressed)
//            return Textures.Scroller.Button.Down[i].draw(getRenderer(), control, control->getRenderBounds());
//
//        if(hovered)
//            return Textures.Scroller.Button.Hover[i].draw(getRenderer(), control, control->getRenderBounds());
//
//        return Textures.Scroller.Button.Normal[i].draw(getRenderer(), control, control->getRenderBounds());
//    }
//    else if(state == State::Remove)
//    {
//    }
//}
//
//void drawComboDownArrow(gweni::controls::Base *control, State state, bool hovered, bool down,
//    bool menuOpen, bool disabled) override
//{
//    if(state == State::Generate)
//    {
//    }
//    else if(state == State::Update)
//    {
//        if(disabled)
//            return Textures.Input.ComboBox.Button.Disabled.draw(getRenderer(), control, control->getRenderBounds());
//
//        if(down || menuOpen)
//            return Textures.Input.ComboBox.Button.Down.draw(getRenderer(), control, control->getRenderBounds());
//
//        if(hovered)
//            return Textures.Input.ComboBox.Button.Hover.draw(getRenderer(), control, control->getRenderBounds());
//
//        Textures.Input.ComboBox.Button.Normal.draw(getRenderer(), control, control->getRenderBounds());
//    }
//    else if(state == State::Remove)
//    {
//    }
//}
//
//void drawNumericUpDownButton(gweni::controls::Base *control, State state, bool depressed, bool bUp) override
//{
//    if(state == State::Generate)
//    {
//    }
//    else if(state == State::Update)
//    {
//        if(bUp)
//        {
//            if(control->isDisabled())
//                return Textures.Input.UpDown.Up.Disabled.drawCenter(getRenderer(), control, control->getRenderBounds());
//
//            if(depressed)
//                return Textures.Input.UpDown.Up.Down.drawCenter(getRenderer(), control, control->getRenderBounds());
//
//            if(control->isHovered())
//                return Textures.Input.UpDown.Up.Hover.drawCenter(getRenderer(), control, control->getRenderBounds());
//
//            return Textures.Input.UpDown.Up.Normal.drawCenter(getRenderer(), control, control->getRenderBounds());
//        }
//
//        if(control->isDisabled())
//            return Textures.Input.UpDown.Down.Disabled.drawCenter(getRenderer(), control, control->getRenderBounds());
//
//        if(depressed)
//            return Textures.Input.UpDown.Down.Down.drawCenter(getRenderer(), control, control->getRenderBounds());
//
//        if(control->isHovered())
//            return Textures.Input.UpDown.Down.Hover.drawCenter(getRenderer(), control, control->getRenderBounds());
//
//        return Textures.Input.UpDown.Down.Normal.drawCenter(getRenderer(), control, control->getRenderBounds());
//    }
//    else if(state == State::Remove)
//    {
//    }
//}
//
//void drawStatusBar(controls::Base *control, State state) override
//{
//    if(state == State::Generate)
//    {
//    }
//    else if(state == State::Update)
//    {
//        Textures.StatusBar.draw(getRenderer(), control, control->getRenderBounds());
//    }
//    else if(state == State::Remove)
//    {
//    }
//}
//
//void drawTreeButton(controls::Base *control, State state, bool open) override
//{
//    if(state == State::Generate)
//    {
//    }
//    else if(state == State::Update)
//    {
//        const gweni::Rect &rect=control->getRenderBounds();
//
//        if(open)
//            Textures.Tree.Minus.draw(getRenderer(), control, rect);
//        else
//            Textures.Tree.Plus.draw(getRenderer(), control, rect);
//    }
//    else if(state == State::Remove)
//    {
//    }
//}
//
//void drawColorDisplay(controls::Base *control, State state, gweni::Color color) override
//{
//    PrimitiveIds &primitiveIds=control->getPrimitiveIds();
//    if(primitiveIds.size() < 8)
//        enlargePrimitiveIds(control, primitiveIds, 8);
//
//    if(state == State::Generate)
//    {
//    }
//    else if(state == State::Update)
//    {
//        const gweni::Rect &rect=control->getRenderBounds();
//
//        if(color.a != 255)
//        {
//            getRenderer()->setDrawColor(gweni::Color(255, 255, 255, 255));
//            getRenderer()->drawFilledRect(primitiveIds[0], rect);
//            getRenderer()->setDrawColor(gweni::Color(128, 128, 128, 128));
//            getRenderer()->drawFilledRect(primitiveIds[1], gweni::Rect(0, 0, rect.w/2, rect.h/2));
//            getRenderer()->drawFilledRect(primitiveIds[2], gweni::Rect(rect.w/2, rect.h/2, rect.w/2, rect.h/2));
//        }
//
//        getRenderer()->setDrawColor(color);
//        getRenderer()->drawFilledRect(primitiveIds[3], rect);
//        getRenderer()->setDrawColor(gweni::Color(0, 0, 0, 255));
//        getRenderer()->drawLinedRect(&primitiveIds[4], rect);
//    }
//    else if(state == State::Remove)
//    {
//    }
//}
//
//void drawModalControl(controls::Base *control, State state) override
//{
//    if(state == State::Generate)
//    {
//    }
//    else if(state == State::Update)
//    {
//        if(!control->shouldDrawBackground())
//            return;
//
//        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
//        if(primitiveIds.size() < 1)
//            enlargePrimitiveIds(control, primitiveIds, 1);
//
//        const gweni::Rect &rect=control->getRenderBounds();
//        getRenderer()->setDrawColor(Colors.ModalBackground);
//        getRenderer()->drawFilledRect(primitiveIds[0], rect);
//    }
//    else if(state == State::Remove)
//    {
//    }
//}
//
//void drawMenuDivider(controls::Base *control, State state) override
//{
//    PrimitiveIds &primitiveIds=control->getPrimitiveIds();
//    if(primitiveIds.size() < 1)
//        enlargePrimitiveIds(control, primitiveIds, 1);
//
//    if(state == State::Generate)
//    {
//    }
//    else if(state == State::Update)
//    {
//        const gweni::Rect &rect=control->getRenderBounds();
//        getRenderer()->setDrawColor(gweni::Color(0, 0, 0, 100));
//        getRenderer()->drawFilledRect(primitiveIds[0], rect);
//    }
//    else if(state == State::Remove)
//    {
//    }
//}
//
//void drawTreeControl(controls::Base *control, State state) override
//{
//    if(state == State::Generate)
//    {
//    }
//    else if(state == State::Update)
//    {
//        Textures.Tree.Background.draw(getRenderer(), control, control->getRenderBounds());
//    }
//    else if(state == State::Remove)
//    {
//    }
//}
//
//void drawWindowCloseButton(gweni::controls::Base *control, State state, bool depressed, bool hovered, bool disabled) override
//{
//    if(state == State::Generate)
//    {
//    }
//    else if(state == State::Update)
//    {
//        gweni::Rect r=gweni::Rect(control->getRenderBounds().x, control->getRenderBounds().y, 31, 31);
//
//        if(disabled)
//            return Textures.Window.Close.draw(getRenderer(), control, r, gweni::Color(255, 255, 255, 50));
//
//        if(depressed)
//            return Textures.Window.Close_Down.draw(getRenderer(), control, r);
//
//        if(hovered)
//            return Textures.Window.Close_Hover.draw(getRenderer(), control, r);
//
//        Textures.Window.Close.draw(getRenderer(), control, r);
//    }
//    else if(state == State::Remove)
//    {
//    }
//}
//
//void drawWindowMaximizeButton(gweni::controls::Base *control, State state, bool depressed,
//    bool hovered, bool disabled, bool bMaximized) override
//{
//    if(state == State::Generate)
//    {
//    }
//    else if(state == State::Update)
//    {
//        gweni::Rect r=gweni::Rect(control->getRenderBounds().x, control->getRenderBounds().y, 31, 31);
//
//        if(!bMaximized)
//        {
//            if(disabled)
//                return Textures.Window.Maxi.draw(getRenderer(), control, r, gweni::Color(255, 255, 255, 50));
//
//            if(depressed)
//                return Textures.Window.Maxi_Down.draw(getRenderer(), control, r);
//
//            if(hovered)
//                return Textures.Window.Maxi_Hover.draw(getRenderer(), control, r);
//
//            return Textures.Window.Maxi.draw(getRenderer(), control, r);
//        }
//
//        if(disabled)
//            return Textures.Window.Restore.draw(getRenderer(), control, r, gweni::Color(255, 255, 255, 50));
//
//        if(depressed)
//            return Textures.Window.Restore_Down.draw(getRenderer(), control, r);
//
//        if(hovered)
//            return Textures.Window.Restore_Hover.draw(getRenderer(), control, r);
//
//        return Textures.Window.Restore.draw(getRenderer(), control, r);
//    }
//    else if(state == State::Remove)
//    {
//    }
//}
//
//void drawWindowMinimizeButton(gweni::controls::Base *control, State state, bool depressed,
//    bool hovered, bool disabled) override
//{
//    if(state == State::Generate)
//    {
//    }
//    else if(state == State::Update)
//    {
//        const gweni::Rect r(control->getRenderBounds().x, control->getRenderBounds().y, 31, 31);
//
//        if(disabled)
//            return Textures.Window.Mini.draw(getRenderer(), control, r, gweni::Color(255, 255, 255, 100));
//
//        if(depressed)
//            return Textures.Window.Mini_Down.draw(getRenderer(), control, r);
//
//        if(hovered)
//            return Textures.Window.Mini_Hover.draw(getRenderer(), control, r);
//
//        Textures.Window.Mini.draw(getRenderer(), control, r);
//    }
//    else if(state == State::Remove)
//    {
//    }
//}
//
//
//void drawSlideButton(gweni::controls::Base *control, State state, bool depressed, bool horizontal) override
//{
//    if(state == State::Generate)
//    {
//    }
//    else if(state == State::Update)
//    {
//        if(!horizontal)
//        {
//            if(control->isDisabled())
//                return Textures.Input.Slider.V.Disabled.drawCenter(getRenderer(), control, control->getRenderBounds());
//
//            if(depressed)
//                return Textures.Input.Slider.V.Down.drawCenter(getRenderer(), control, control->getRenderBounds());
//
//            if(control->isHovered())
//                return Textures.Input.Slider.V.Hover.drawCenter(getRenderer(), control, control->getRenderBounds());
//
//            return Textures.Input.Slider.V.Normal.drawCenter(getRenderer(), control, control->getRenderBounds());
//        }
//
//        if(control->isDisabled())
//            return Textures.Input.Slider.H.Disabled.drawCenter(getRenderer(), control, control->getRenderBounds());
//
//        if(depressed)
//            return Textures.Input.Slider.H.Down.drawCenter(getRenderer(), control, control->getRenderBounds());
//
//        if(control->isHovered())
//            return Textures.Input.Slider.H.Hover.drawCenter(getRenderer(), control, control->getRenderBounds());
//
//        Textures.Input.Slider.H.Normal.drawCenter(getRenderer(), control, control->getRenderBounds());
//    }
//    else if(state == State::Remove)
//    {
//    }
//}
//
//void drawTreeNode(controls::Base *control, State state, bool open, bool selected,
//    int labelHeight, int labelWidth, int halfWay,
//    int lastBranch, bool isRoot) override
//{
//    if(state == State::Generate)
//    {
//    }
//    else if(state == State::Update)
//    {
//        if(selected)
//        {
//            Textures.Selection.draw(getRenderer(), control, gweni::Rect(17, 0, labelWidth+2, labelHeight-1));
//        }
//    }
//    else if(state == State::Remove)
//    {
//    }
//
////        Base::drawTreeNode(ctrl, open, selected, labelHeight, labelWidth,
////            halfWay, lastBranch, isRoot);
//}
//
//void drawCategoryHolder(controls::Base *control, State state) override
//{
//    if(state == State::Generate)
//    {
//    }
//    else if(state == State::Update)
//    {
//        Textures.CategoryList.Outer.draw(getRenderer(), control, control->getRenderBounds());
//    }
//    else if(state == State::Remove)
//    {
//    }
//}
//
//void drawCategoryInner(controls::Base *control, State state, bool collapsed) override
//{
//    if(state == State::Generate)
//    {
//    }
//    else if(state == State::Update)
//    {
//        if(collapsed)
//            return Textures.CategoryList.Header.draw(getRenderer(), control, control->getRenderBounds());
//
//        Textures.CategoryList.Inner.draw(getRenderer(), control, control->getRenderBounds());
//    }
//    else if(state == State::Remove)
//    {
//    }
//}


}//namespace textured
}//namespace skin
}//namespace gweni
