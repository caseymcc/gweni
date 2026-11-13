/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A skin config that cannot be used degrades the textured skin visibly
 *  instead of leaving it half initialised: TexturedSkin::init reports the file
 *  on one stderr line, returns false, and leaves every tokyonight surface in
 *  colors::Missing (magenta, with a MissingDark border) and the text colours
 *  at their compiled defaults, so labels stay readable over it. A config whose
 *  atlas texture does not load is reported and fails init too, but the rest of
 *  it (the theme included) still applies.
 *
 *  A theme with "strict": true starts every surface colour field and every
 *  textColorRegistry() colour as magenta, so only what it sets shows its
 *  colour, and names what it left out on one stderr line per kind. Its
 *  "metrics" object sets the skin's sizes (skin::Base::Metrics) over Tokyo
 *  Night's compiled ones: a ScrollBarThickness of 6-64 applies, anything else
 *  (out of range, not an integer, an unknown name, "metrics" not an object)
 *  keeps 12 with one stderr line.
 *
 *  The unusable configs, over a temp dir: no DefaultSkin.json, a directory of
 *  that name, an empty one, one that does not parse and one whose root is not
 *  an object. Each fails with one stderr line naming it, every surface magenta
 *  with its radius and border width kept, and the compiled text colours. The
 *  atlas case uses StubRenderer::textureStatus, and its line names the
 *  texture. A strict theme's gaps are per field, with the radius and border
 *  width compiled and a rejected value counting as not set; init is still
 *  true, with exactly its two summary lines, sorted (one line when it sets
 *  everything). Without strict, with false or with a non-boolean (one
 *  diagnostic), omitted values keep their compiled defaults. A case that
 *  changes the global surfaces restores them through test::SurfaceSnapshot.
 */
#include "headlessFixture.h"

#include <gweni/platforms/commonPlatform.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>
#include <gweni/text/tokenizer.h>
#include <gweni/utils/filesystem.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>

using namespace gweni;

namespace tokyonight=gweni::skin::textured::tokyonight;

namespace
{

//! A fresh directory under the temp dir, removed again on destruction.
struct TempDir
{
    TempDir():
        path(fs::temp_directory_path()/("gweni_skinConfig_"+std::to_string(getpid())))
    {
        fs::remove_all(path);
        fs::create_directories(path);
    }

    ~TempDir()
    {
        std::error_code ec;
        fs::remove_all(path, ec);
    }

    void write(const String &name, const String &text) const
    {
        std::ofstream file(path/name);
        file<<text;
    }

    fs::path path;
};

//! Captures what is written to std::cerr while it is alive.
struct CerrCapture
{
    CerrCapture(): old(std::cerr.rdbuf(buffer.rdbuf())) {}
    ~CerrCapture() { std::cerr.rdbuf(old); }

    std::ostringstream buffer;
    std::streambuf *old;
};

//! What one init() did: its result and what it printed on stderr.
struct InitResult
{
    bool loaded;
    String message;
};

InitResult initSkin(skin::textured::TexturedSkin &skin)
{
    CerrCapture capture;
    const bool loaded=skin.init("DefaultSkin.json");

    return {loaded, capture.buffer.str()};
}

size_t lines(const String &text)
{
    return std::count(text.begin(), text.end(), '\n');
}

//! Every registered surface is magenta, its radius and border width as \p before.
void checkAllMissing(const test::SurfaceSnapshot &before)
{
    for(const auto &entry : tokyonight::surfaceRegistry())
    {
        const tokyonight::Surface &s=*entry.second;
        const tokyonight::Surface &was=before.saved.at(entry.first);

        INFO("surface ", entry.first);
        CHECK((s.top == colors::Missing));
        CHECK((s.bottom == colors::Missing));
        CHECK((s.border == colors::MissingDark));
        CHECK(s.radius == was.radius);
        CHECK(s.borderWidth == was.borderWidth);
    }
}

//! \p skin's text colours are the compiled ones a skin's setThemeColors() gives.
void checkCompiledTextColors(skin::textured::TexturedSkin &skin, test::StubRenderer &renderer)
{
    skin::textured::TexturedSkin compiled(&renderer);

    compiled.setThemeColors();
    CHECK((skin.Colors.Label.Default == compiled.Colors.Label.Default));
    CHECK((skin.Colors.Button.Normal == compiled.Colors.Button.Normal));
    CHECK_FALSE((skin.Colors.Label.Default == colors::Missing));
}

}//namespace

TEST_CASE("TexturedSkin::init with a config it cannot use paints every surface magenta and fails")
{
    TempDir dir;
    platform::CurrentPath paths(dir.path.string());
    test::StubRenderer renderer(paths);
    test::SurfaceSnapshot surfaces;
    const String configPath=(dir.path/"DefaultSkin.json").string();

    SUBCASE("no DefaultSkin.json")
    {
    }

    SUBCASE("a directory named DefaultSkin.json")
    {
        fs::create_directories(dir.path/"DefaultSkin.json");
    }

    SUBCASE("DefaultSkin.json that does not parse")
    {
        dir.write("DefaultSkin.json", "{\"theme\": {\"surfaces\": ");
    }

    SUBCASE("an empty DefaultSkin.json")
    {
        dir.write("DefaultSkin.json", "");
    }

    SUBCASE("DefaultSkin.json that parses but is not an object")
    {
        dir.write("DefaultSkin.json", "[1]\n");
    }

    skin::textured::TexturedSkin skin(&renderer);
    const InitResult result=initSkin(skin);

    INFO("stderr: ", result.message);
    CHECK_FALSE(result.loaded);
    CHECK(lines(result.message) == 1);
    CHECK(result.message.find(configPath) != String::npos);

    checkAllMissing(surfaces);
    checkCompiledTextColors(skin, renderer);

    // the rest of init still ran
    CHECK(skin.getDefaultFont().facename == "OpenSans");
    CHECK(skin.getDefaultFont().size == 11);
}

TEST_CASE("TexturedSkin::init with an atlas that does not load fails but still applies the theme")
{
    TempDir dir;
    platform::CurrentPath paths(dir.path.string());
    test::StubRenderer renderer(paths);
    test::SurfaceSnapshot surfaces;

    dir.write("DefaultSkin.json",
        "{\n"
        "    \"textureFileName\": \"NoSuchAtlas.png\",\n"
        "    \"theme\": {\"surfaces\": {\"ButtonNormal\": {\"top\": [1, 2, 3]}}}\n"
        "}\n");
    renderer.textureStatus=Texture::Status::ErrorFileNotFound;

    skin::textured::TexturedSkin skin(&renderer);
    const InitResult result=initSkin(skin);

    INFO("stderr: ", result.message);
    CHECK_FALSE(result.loaded);
    CHECK(lines(result.message) == 1);
    CHECK(result.message.find("Unable to load skin texture") != String::npos);
    CHECK(result.message.find(paths.getPath(ResourcePaths::Type::Texture, "NoSuchAtlas.png")) != String::npos);

    // the theme applied, and nothing went magenta
    CHECK((tokyonight::ButtonNormal.top == Color(1, 2, 3)));
    CHECK((tokyonight::ButtonNormal.bottom == surfaces.saved.at("ButtonNormal").bottom));

    for(const auto &entry : tokyonight::surfaceRegistry())
    {
        INFO("surface ", entry.first);
        CHECK_FALSE((entry.second->top == colors::Missing));
        CHECK_FALSE((entry.second->border == colors::MissingDark));
    }
    checkCompiledTextColors(skin, renderer);
}

TEST_CASE("TexturedSkin::init with a config and atlas that load succeeds silently")
{
    TempDir dir;
    platform::CurrentPath paths(dir.path.string());
    test::StubRenderer renderer(paths);
    test::SurfaceSnapshot surfaces;

    dir.write("DefaultSkin.json", "{\"textureFileName\": \"Atlas.png\"}\n");

    skin::textured::TexturedSkin skin(&renderer);
    const InitResult result=initSkin(skin);

    INFO("stderr: ", result.message);
    CHECK(result.loaded);
    CHECK(result.message.empty());
    CHECK((tokyonight::Canvas.top == surfaces.saved.at("Canvas").top));
}

namespace
{

//! A config whose theme is \p theme (the JSON text of the "theme" object).
String themeConfig(const String &theme)
{
    return "{\"theme\": "+theme+"}\n";
}

//! \p names sorted, joined with ", ".
String joined(std::vector<String> names)
{
    std::sort(names.begin(), names.end());

    String text;

    for(const String &name : names)
        text+=(text.empty()?"":", ")+name;
    return text;
}

}//namespace

TEST_CASE("TexturedSkin::textColorRegistry names the theme's 59 text colours in this skin")
{
    test::StubRenderer renderer;
    skin::textured::TexturedSkin skin(&renderer);
    skin::textured::TexturedSkin other(&renderer);

    auto registry=skin.textColorRegistry();

    CHECK(registry.size() == 59);
    CHECK((registry.at("LabelDefault") == &skin.Colors.Label.Default));
    CHECK((registry.at("SelectableDisabled") == &skin.Colors.Selectable.Disabled));
    CHECK((registry.at("CodeLineNumber") == &skin.Colors.CodeEditor.LineNumber));
    CHECK((registry.at("CodeLineNumberCurrent") == &skin.Colors.CodeEditor.LineNumberCurrent));
    CHECK((registry.at("ChartAxisText") == &skin.Colors.Chart.AxisText));
    CHECK((registry.at("ChartMarkerText") == &skin.Colors.Chart.MarkerText));
    // a colour per token kind, each by its kind's name
    CHECK((registry.at("CodePlain") == &skin.Colors.CodeEditor.Tokens[0]));
    CHECK((registry.at("CodeKeyword") == &skin.Colors.CodeEditor.Tokens[size_t(text::TokenKind::Keyword)]));
    CHECK((registry.at("CodeMeta") == &skin.Colors.CodeEditor.Tokens[size_t(text::TokenKind::Meta)]));
    for(size_t kind=0; kind < size_t(text::TokenKind::Count); ++kind)
    {
        String name=text::tokenKindName(text::TokenKind(kind));

        name[0]=static_cast<char>(toupper(name[0]));
        CAPTURE(name);
        CHECK((registry.at("Code"+name) == &skin.Colors.CodeEditor.Tokens[kind]));
    }
    CHECK((registry.at("TimelineAxisText") == &skin.Colors.Timeline.AxisText));
    CHECK((registry.at("TimelineCaptionText") == &skin.Colors.Timeline.CaptionText));
    CHECK((registry.at("BadgeText") == &skin.Colors.BadgeText));
    CHECK((registry.at("ChipDangerText") == &skin.Colors.Chip.Danger));
    CHECK((registry.at("ChipPalette7Text") == &skin.Colors.Chip.Palette[7]));
    CHECK((registry.at("ToastText") == &skin.Colors.Toast.Text));
    CHECK((registry.at("ToastNegative") == &skin.Colors.Toast.Negative));
    CHECK((other.textColorRegistry().at("LabelDefault") == &other.Colors.Label.Default));
}

TEST_CASE("A strict theme leaves every colour it does not set magenta and lists them once")
{
    TempDir dir;
    platform::CurrentPath paths(dir.path.string());
    test::StubRenderer renderer(paths);
    test::SurfaceSnapshot surfaces;

    // Canvas fully set, ButtonNormal only top/bottom, TextBox's top rejected
    dir.write("DefaultSkin.json", themeConfig(
        "{\"strict\": true,"
        " \"surfaces\": {"
        "  \"Canvas\": {\"top\": [1, 2, 3], \"bottom\": [4, 5, 6], \"border\": [7, 8, 9, 10]},"
        "  \"ButtonNormal\": {\"top\": [11, 12, 13], \"bottom\": [14, 15, 16]},"
        "  \"TextBox\": {\"top\": \"red\"}},"
        " \"textColors\": {\"LabelDefault\": [21, 22, 23], \"ButtonHover\": [300, 0, 0]}}"));

    skin::textured::TexturedSkin skin(&renderer);
    const InitResult result=initSkin(skin);

    INFO("stderr: ", result.message);
    // strict gaps are not a failure
    CHECK(result.loaded);

    // what the file sets
    CHECK((tokyonight::Canvas.top == Color(1, 2, 3)));
    CHECK((tokyonight::Canvas.bottom == Color(4, 5, 6)));
    CHECK((tokyonight::Canvas.border == Color(7, 8, 9, 10)));
    CHECK((tokyonight::ButtonNormal.top == Color(11, 12, 13)));
    CHECK((tokyonight::ButtonNormal.bottom == Color(14, 15, 16)));
    // per field: the border it does not give is magenta, radius and width compiled
    CHECK((tokyonight::ButtonNormal.border == colors::MissingDark));
    CHECK(tokyonight::ButtonNormal.radius == surfaces.saved.at("ButtonNormal").radius);
    CHECK(tokyonight::ButtonNormal.borderWidth == surfaces.saved.at("ButtonNormal").borderWidth);

    // everything else magenta, the transparent-at-rest WindowButton included,
    // its radius and border width kept
    for(const auto &entry : tokyonight::surfaceRegistry())
    {
        if(entry.first == "Canvas" || entry.first == "ButtonNormal")
            continue;

        const tokyonight::Surface &s=*entry.second;
        const tokyonight::Surface &was=surfaces.saved.at(entry.first);

        INFO("surface ", entry.first);
        CHECK((s.top == colors::Missing));
        CHECK((s.bottom == colors::Missing));
        CHECK((s.border == colors::MissingDark));
        CHECK(s.radius == was.radius);
        CHECK(s.borderWidth == was.borderWidth);
    }

    for(const auto &entry : skin.textColorRegistry())
    {
        INFO("text color ", entry.first);
        if(entry.first == "LabelDefault")
            CHECK((*entry.second == Color(21, 22, 23)));
        else
            CHECK((*entry.second == colors::Missing));
    }

    // the two rejected values, then one summary line per kind
    std::vector<String> surfaceGaps;
    std::vector<String> textGaps;

    for(const auto &entry : tokyonight::surfaceRegistry())
    {
        if(entry.first == "ButtonNormal")
            surfaceGaps.push_back("ButtonNormal.border");
        else if(entry.first != "Canvas")
            surfaceGaps.push_back(entry.first);
    }
    for(const auto &entry : skin.textColorRegistry())
    {
        if(entry.first != "LabelDefault")
            textGaps.push_back(entry.first);
    }

    const size_t surfaceCount=tokyonight::surfaceRegistry().size();
    const String expected=
        "theme surface \"TextBox\": \"top\" must be an array of 3-4 integers 0-255\n"
        "theme textColor \"ButtonHover\" must be an array of 3-4 integers 0-255\n"
        "theme strict: "+std::to_string(surfaceCount-1)+" of "+std::to_string(surfaceCount)
            +" surfaces not set: "+joined(surfaceGaps)+"\n"
        "theme strict: 58 of 59 text colors not set: "+joined(textGaps)+"\n";

    CHECK(result.message == expected);
}

TEST_CASE("A strict theme that sets everything says so on one line")
{
    TempDir dir;
    platform::CurrentPath paths(dir.path.string());
    test::StubRenderer renderer(paths);
    test::SurfaceSnapshot surfaces;
    skin::textured::TexturedSkin skin(&renderer);

    bool allTextColors=true;

    SUBCASE("surfaces and text colours") {}
    SUBCASE("surfaces only") { allTextColors=false; }

    String surfaceJson;
    String textJson;

    for(const auto &entry : tokyonight::surfaceRegistry())
        surfaceJson+=String(surfaceJson.empty()?"":",")+"\""+entry.first
            +"\": {\"top\": [0,0,0,0], \"bottom\": [0,0,0,0], \"border\": [0,0,0,0]}";
    for(const auto &entry : skin.textColorRegistry())
    {
        if(allTextColors || entry.first != "TooltipText")
            textJson+=String(textJson.empty()?"":",")+"\""+entry.first+"\": [1, 1, 1]";
    }
    dir.write("DefaultSkin.json", themeConfig(
        "{\"strict\": true, \"surfaces\": {"+surfaceJson+"}, \"textColors\": {"+textJson+"}}"));

    const InitResult result=initSkin(skin);
    const String count=std::to_string(tokyonight::surfaceRegistry().size());

    INFO("stderr: ", result.message);
    CHECK(result.loaded);
    if(allTextColors)
        CHECK(result.message == "theme strict: all "+count+" surfaces and 59 text colors set\n");
    else
    {
        CHECK(result.message == "theme strict: all "+count+" surfaces set\n"
            "theme strict: 1 of 59 text colors not set: TooltipText\n");
        CHECK((skin.Colors.TooltipText == colors::Missing));
    }

    // transparent, as the theme said
    CHECK((tokyonight::WindowButton.top == Color(0, 0, 0, 0)));
}

TEST_CASE("A theme that is not strict keeps the compiled default for what it omits")
{
    TempDir dir;
    platform::CurrentPath paths(dir.path.string());
    test::StubRenderer renderer(paths);
    test::SurfaceSnapshot surfaces;
    String strict;
    String expectedMessage;

    SUBCASE("no strict key") {}
    SUBCASE("strict false") { strict="\"strict\": false,"; }
    SUBCASE("strict not a boolean")
    {
        strict="\"strict\": \"yes\",";
        expectedMessage="theme \"strict\" must be a boolean\n";
    }

    dir.write("DefaultSkin.json", themeConfig(
        "{"+strict+" \"surfaces\": {\"ButtonNormal\": {\"top\": [11, 12, 13]}},"
        " \"textColors\": {\"LabelDefault\": [21, 22, 23]}}"));

    skin::textured::TexturedSkin skin(&renderer);
    const InitResult result=initSkin(skin);

    INFO("stderr: ", result.message);
    CHECK(result.loaded);
    CHECK(result.message == expectedMessage);

    CHECK((tokyonight::ButtonNormal.top == Color(11, 12, 13)));
    for(const auto &entry : tokyonight::surfaceRegistry())
    {
        const tokyonight::Surface &s=*entry.second;
        const tokyonight::Surface &was=surfaces.saved.at(entry.first);

        INFO("surface ", entry.first);
        if(entry.first != "ButtonNormal")
            CHECK((s.top == was.top));
        CHECK((s.bottom == was.bottom));
        CHECK((s.border == was.border));
    }

    skin::textured::TexturedSkin compiled(&renderer);
    auto compiledColors=compiled.textColorRegistry();

    compiled.setThemeColors();
    for(const auto &entry : skin.textColorRegistry())
    {
        INFO("text color ", entry.first);
        if(entry.first == "LabelDefault")
            CHECK((*entry.second == Color(21, 22, 23)));
        else
            CHECK((*entry.second == *compiledColors.at(entry.first)));
    }
}

TEST_CASE("A theme's \"metrics\" sets the scroll bar thickness, and a bad value keeps Tokyo Night's 12")
{
    TempDir dir;
    platform::CurrentPath paths(dir.path.string());
    test::StubRenderer renderer(paths);
    test::SurfaceSnapshot surfaces;

    // the compiled default, for any skin, before a theme is applied
    CHECK(skin::textured::TexturedSkin(&renderer).Metrics.ScrollBarThickness == 15);

    String theme;
    int expected=12;
    String expectedMessage;

    SUBCASE("no metrics") { theme="{}"; }
    SUBCASE("a valid thickness")
    {
        theme="{\"metrics\": {\"_comment\": \"skipped\", \"ScrollBarThickness\": 20}}";
        expected=20;
    }
    SUBCASE("the range's ends")
    {
        SUBCASE("6") { theme="{\"metrics\": {\"ScrollBarThickness\": 6}}"; expected=6; }
        SUBCASE("64") { theme="{\"metrics\": {\"ScrollBarThickness\": 64}}"; expected=64; }
    }
    SUBCASE("rejected")
    {
        const String outOfRange="theme metric \"ScrollBarThickness\" must be an integer 6-64\n";

        SUBCASE("too small") { theme="{\"metrics\": {\"ScrollBarThickness\": 3}}"; expectedMessage=outOfRange; }
        SUBCASE("too large") { theme="{\"metrics\": {\"ScrollBarThickness\": 100}}"; expectedMessage=outOfRange; }
        SUBCASE("not an integer") { theme="{\"metrics\": {\"ScrollBarThickness\": 12.5}}"; expectedMessage=outOfRange; }
        SUBCASE("a string") { theme="{\"metrics\": {\"ScrollBarThickness\": \"12\"}}"; expectedMessage=outOfRange; }
        SUBCASE("an unknown name")
        {
            theme="{\"metrics\": {\"ScrollbarThickness\": 20}}";
            expectedMessage="theme metric \"ScrollbarThickness\" is not a known metric\n";
        }
        SUBCASE("metrics not an object")
        {
            theme="{\"metrics\": [20]}";
            expectedMessage="theme \"metrics\" must be an object\n";
        }
    }

    dir.write("DefaultSkin.json", themeConfig(theme));

    skin::textured::TexturedSkin skin(&renderer);
    const InitResult result=initSkin(skin);

    INFO("theme: ", theme);
    INFO("stderr: ", result.message);
    CHECK(result.loaded);
    CHECK(result.message == expectedMessage);
    CHECK(lines(result.message) == (expectedMessage.empty()?0:1));
    CHECK(skin.Metrics.ScrollBarThickness == expected);
}
