/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  tools::ThemeEditor on a real Canvas with the real (un-init()'d)
 *  TexturedSkin over the StubRenderer, and the theme file it saves and loads.
 *
 *  The editor has a node per tokyonight surface, sorted by name, then one of
 *  the skin's text colours; a node's rows are made when it is first opened: a
 *  surface's top, bottom, border (ColorEdits), radius and borderWidth
 *  (DragNumbers), one ColorEdit per text colour. An edit in a row writes the
 *  global (or the skin's text colour) and redraws the canvas once: every
 *  control is then marked StateChange_Background|StateChange_Text, a Label
 *  that follows the skin's label colour takes the new one, a Button's text
 *  takes its new colour from updateColors(), and a Label given a colour of its
 *  own keeps it; an edit that changes nothing redraws nothing, and the frame
 *  after the redraw draws nothing. Revert gives back every value the editor
 *  was opened with, and shows them in the rows.
 *
 *  Save writes the whole theme: TexturedSkin::loadTheme() of the written JSON,
 *  and loadThemeFile() of the file, give back every value, and the Load button
 *  shows the loaded values in the rows. A second save of the same theme writes
 *  the same bytes. Into a skin config, the "theme" object is replaced and the
 *  rest of the file kept byte for byte (CRLF too), one is added to a config
 *  with none, a standalone theme file is replaced whole, and a file that does
 *  not parse is left as it was, with save() false and one stderr line.
 *
 *  internal::Text's SkinTextColor: a new Text follows Label.Default,
 *  setTextColor() stops it following, setSkinTextColor() starts it again, and
 *  refreshSkinTextColor() reads the skin's value again.
 *
 *  Every case restores the global surfaces it changes (test::SurfaceSnapshot).
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/colorEdit.h>
#include <gweni/controls/dragNumber.h>
#include <gweni/controls/label.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>
#include <gweni/tools/themeEditor.h>
#include <gweni/utils/filesystem.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>

using namespace gweni;

namespace tokyonight=gweni::skin::textured::tokyonight;
using gweni::skin::textured::TexturedSkin;
using gweni::tools::ThemeEditor;
using gweni::tools::ThemeEditorNode;
using gweni::tools::ThemeSurfaceField;

namespace
{

//! A fresh directory under the temp dir, removed again on destruction.
struct TempDir
{
    TempDir():
        path(fs::temp_directory_path()/("gweni_themeEditor_"+std::to_string(getpid())))
    {
        fs::remove_all(path);
        fs::create_directories(path);
    }

    ~TempDir()
    {
        std::error_code ec;
        fs::remove_all(path, ec);
    }

    std::string file(const std::string &name) const
    {
        return (path/name).string();
    }

    fs::path path;
};

void writeFile(const std::string &path, const std::string &text)
{
    std::ofstream file(path, std::ios::binary);
    file<<text;
}

std::string readFile(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);
    std::ostringstream text;

    text<<file.rdbuf();
    return text.str();
}

//! Captures what is written to std::cerr while it is alive.
struct CerrCapture
{
    CerrCapture(): old(std::cerr.rdbuf(buffer.rdbuf())) {}
    ~CerrCapture() { std::cerr.rdbuf(old); }

    std::ostringstream buffer;
    std::streambuf *old;
};

struct Scene
{
    test::SurfaceSnapshot surfaces;
    test::StubRenderer renderer;
    TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::Label *label=nullptr;
    controls::Label *ownColor=nullptr;
    controls::Button *button=nullptr;
    ThemeEditor *editor=nullptr;

    Scene()
    {
        skin.setThemeColors();
        canvas=new controls::Canvas(&skin);
        canvas->setSize(1024, 768);

        label=canvas->newChild<controls::Label>();
        label->setText("follows the skin");
        label->setPos(500, 10);

        ownColor=canvas->newChild<controls::Label>();
        ownColor->setText("its own colour");
        ownColor->setTextColor(Color(1, 2, 3));
        ownColor->setPos(500, 40);

        button=canvas->newChild<controls::Button>();
        button->setText("Button");
        button->setPos(500, 70);

        editor=ThemeEditor::open(canvas);
        frame();
        frame();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    //! The primitive and glyph slots the last frame wrote.
    size_t written() const
    {
        const renderer::FrameCounts counts=renderer.frameStats();

        return counts.primitiveSlotsWritten+counts.textSlotsWritten;
    }

    //! The row of node \p name labelled \p rowName, the node opened.
    controls::PropertyRow *row(ThemeEditorNode *node, const std::string &rowName)
    {
        node->open();
        for(controls::PropertyRow *candidate : node->rows())
        {
            if(candidate->getLabel()->getText() == rowName)
                return candidate;
        }
        return nullptr;
    }

    controls::ColorEdit *colorEdit(ThemeEditorNode *node, const std::string &rowName)
    {
        controls::PropertyRow *found=row(node, rowName);
        auto *property=found?gweni_cast<tools::ThemeColorProperty>(found->getProperty()):nullptr;

        return property?property->colorEdit():nullptr;
    }

    controls::DragNumber *dragNumber(ThemeEditorNode *node, const std::string &rowName)
    {
        controls::PropertyRow *found=row(node, rowName);
        auto *property=found?gweni_cast<tools::ThemeNumberProperty>(found->getProperty()):nullptr;

        return property?property->dragNumber():nullptr;
    }
};

//! Whether every control under \p control has all of \p flags pending.
bool allMarked(controls::Base *control, int flags)
{
    if((control->getStateChange()&flags) != flags)
        return false;
    for(controls::Base *child : control->getActualChildren())
    {
        if(!allMarked(child, flags))
            return false;
    }
    return true;
}

//! A theme unlike Tokyo Night in several surfaces, text colours and a metric.
TexturedSkin::ThemeValues changedTheme(const TexturedSkin::ThemeValues &from)
{
    TexturedSkin::ThemeValues changed=from;

    changed.surfaces.at("ButtonNormal").top=Color(10, 20, 30);
    changed.surfaces.at("ButtonNormal").border=Color(40, 50, 60, 70);
    changed.surfaces.at("Panel").radius=2.5f;
    changed.surfaces.at("TextBox").borderWidth=3.0f;
    changed.surfaces.at("WindowButton").bottom=Color(1, 2, 3, 0);
    changed.textColors.at("LabelDefault")=Color(200, 100, 50);
    changed.textColors.at("TooltipText")=Color(9, 8, 7, 128);
    changed.metrics.at("ScrollBarThickness")=20;
    return changed;
}

}//namespace

TEST_CASE("ThemeEditor has a node per surface sorted by name then the text colours")
{
    Scene scene;
    const auto &registry=tokyonight::surfaceRegistry();
    const std::vector<ThemeEditorNode *> &nodes=scene.editor->nodes();

    REQUIRE(nodes.size() == registry.size()+1);

    std::vector<std::string> names;

    for(const auto &entry : registry)
        names.push_back(entry.first);
    std::sort(names.begin(), names.end());

    for(size_t i=0; i < names.size(); ++i)
    {
        INFO("node ", i);
        CHECK(nodes[i]->name() == names[i]);
        CHECK(nodes[i]->getText() == names[i]);
        CHECK_FALSE(nodes[i]->isTextColors());
        CHECK(scene.editor->surfaceNode(names[i]) == nodes[i]);
    }
    CHECK(nodes.back() == scene.editor->textColorsNode());
    CHECK(nodes.back()->isTextColors());
    CHECK(nodes.back()->getText() == ThemeEditor::TextColorsCaption);

    // no rows until a node is opened
    for(ThemeEditorNode *node : nodes)
    {
        CHECK_FALSE(node->populated());
        CHECK(node->rows().empty());
    }

    ThemeEditorNode *button=scene.editor->surfaceNode("ButtonNormal");

    button->open();
    scene.frame();

    const std::vector<controls::PropertyRow *> rows=button->rows();
    const char *const fields[]={"top", "bottom", "border", "radius", "borderWidth"};

    REQUIRE(rows.size() == 5);
    for(size_t i=0; i < 5; ++i)
    {
        INFO("row ", fields[i]);
        CHECK(rows[i]->getLabel()->getText() == fields[i]);
        if(i < 3)
            CHECK(gweni_cast<tools::ThemeColorProperty>(rows[i]->getProperty()) != nullptr);
        else
            CHECK(gweni_cast<tools::ThemeNumberProperty>(rows[i]->getProperty()) != nullptr);
    }
    CHECK((scene.colorEdit(button, "top")->getColor() == tokyonight::ButtonNormal.top));
    CHECK((scene.colorEdit(button, "border")->getColor() == tokyonight::ButtonNormal.border));
    CHECK(scene.dragNumber(button, "radius")->getNumber() == doctest::Approx(tokyonight::ButtonNormal.radius));
    CHECK(scene.dragNumber(button, "borderWidth")->getNumber()
        == doctest::Approx(tokyonight::ButtonNormal.borderWidth));

    ThemeEditorNode *text=scene.editor->textColorsNode();

    text->open();

    const auto textColors=scene.skin.textColorRegistry();
    std::vector<std::string> textNames;

    for(const auto &entry : textColors)
        textNames.push_back(entry.first);
    std::sort(textNames.begin(), textNames.end());

    const std::vector<controls::PropertyRow *> textRows=text->rows();

    REQUIRE(textRows.size() == textNames.size());
    for(size_t i=0; i < textNames.size(); ++i)
    {
        INFO("text colour ", textNames[i]);
        CHECK(textRows[i]->getLabel()->getText() == textNames[i]);
        CHECK((scene.colorEdit(text, textNames[i])->getColor() == *textColors.at(textNames[i])));
    }
}

TEST_CASE("A ThemeEditor row edit writes the value and redraws the whole canvas once")
{
    Scene scene;
    ThemeEditorNode *button=scene.editor->surfaceNode("ButtonNormal");
    controls::ColorEdit *top=scene.colorEdit(button, "top");

    scene.frame();
    scene.frame();
    REQUIRE(scene.written() == 0);

    const size_t redraws=scene.editor->redraws();
    const Color newTop(12, 34, 56);

    top->setColor(newTop);

    CHECK((tokyonight::ButtonNormal.top == newTop));
    CHECK(scene.editor->redraws() == redraws+1);
    CHECK(allMarked(scene.canvas, controls::StateChange_Background|controls::StateChange_Text));

    scene.frame();
    CHECK(scene.written() > 0);
    scene.frame();
    CHECK(scene.written() == 0);

    // the same colour again: nothing
    top->setColor(newTop);
    CHECK(scene.editor->redraws() == redraws+1);
    scene.frame();
    CHECK(scene.written() == 0);

    // a number row
    controls::DragNumber *radius=scene.dragNumber(button, "radius");

    radius->setNumber(3.5);
    CHECK(tokyonight::ButtonNormal.radius == doctest::Approx(3.5f));
    CHECK(scene.editor->redraws() == redraws+2);
    scene.frame();
    scene.frame();
    CHECK(scene.written() == 0);

    // a text colour row: the Label following the skin's colour and the Button
    // take theirs, the Label with its own keeps it
    ThemeEditorNode *text=scene.editor->textColorsNode();
    const Color labelColor(200, 10, 20);
    const Color buttonColor(20, 200, 10);

    scene.colorEdit(text, "LabelDefault")->setColor(labelColor);
    CHECK((scene.skin.Colors.Label.Default == labelColor));
    CHECK(scene.editor->redraws() == redraws+3);
    CHECK((scene.label->getTextColor() == labelColor));
    CHECK((scene.ownColor->getTextColor() == Color(1, 2, 3)));

    scene.colorEdit(text, "ButtonNormal")->setColor(buttonColor);
    CHECK((scene.skin.Colors.Button.Normal == buttonColor));
    CHECK((scene.button->getTextColor() == buttonColor));
    CHECK((scene.ownColor->getTextColor() == Color(1, 2, 3)));
    scene.frame();
    scene.frame();
    CHECK(scene.written() == 0);

    // through the API, as a row does
    CHECK(scene.editor->setSurfaceColor("Panel", ThemeSurfaceField::Border, Color(1, 1, 1)));
    CHECK((tokyonight::Panel.border == Color(1, 1, 1)));
    CHECK(scene.editor->redraws() == redraws+5);
    CHECK_FALSE(scene.editor->setSurfaceColor("NoSuchSurface", ThemeSurfaceField::Top, Color(1, 1, 1)));
    CHECK_FALSE(scene.editor->setTextColor("NoSuchColour", Color(1, 1, 1)));
    CHECK(scene.editor->redraws() == redraws+5);
}

TEST_CASE("ThemeEditor revert gives back every value it was opened with")
{
    Scene scene;
    ThemeEditorNode *button=scene.editor->surfaceNode("ButtonNormal");
    const TexturedSkin::ThemeValues opened=scene.editor->openedWith();

    REQUIRE(opened == scene.skin.captureTheme());

    scene.colorEdit(button, "top")->setColor(Color(1, 2, 3));
    scene.dragNumber(button, "borderWidth")->setNumber(4.0);
    scene.editor->setTextColor("LabelDefault", Color(4, 5, 6));
    scene.editor->setSurfaceNumber("Window", ThemeSurfaceField::Radius, 1.0f);
    REQUIRE(scene.skin.captureTheme() != opened);

    scene.editor->revertButton()->onPressCaller.call(scene.editor->revertButton());

    CHECK(scene.skin.captureTheme() == opened);
    CHECK((scene.colorEdit(button, "top")->getColor() == opened.surfaces.at("ButtonNormal").top));
    CHECK(scene.dragNumber(button, "borderWidth")->getNumber()
        == doctest::Approx(opened.surfaces.at("ButtonNormal").borderWidth));
    CHECK((scene.label->getTextColor() == opened.textColors.at("LabelDefault")));
    CHECK(scene.editor->statusLine()->getText().find("Reverted") == 0);
}

TEST_CASE("A saved theme loads back every value through loadTheme and loadThemeFile")
{
    Scene scene;
    TempDir dir;
    const TexturedSkin::ThemeValues opened=scene.skin.captureTheme();
    const TexturedSkin::ThemeValues changed=changedTheme(opened);
    const std::string path=dir.file("theme.json");

    scene.skin.applyTheme(changed);
    REQUIRE(scene.skin.captureTheme() == changed);

    scene.editor->setThemePath(path);
    REQUIRE(scene.editor->save());
    CHECK(scene.editor->statusLine()->getText() == "Saved "+path);

    const std::string written=readFile(path);

    // loadTheme() of the written JSON
    scene.skin.applyTheme(opened);
    REQUIRE(scene.skin.captureTheme() == opened);
    {
        json::Document document;

        document.Parse(written.c_str());
        REQUIRE_FALSE(document.HasParseError());
        REQUIRE(document.IsObject());
        CHECK(document.HasMember("surfaces"));
        CHECK(document.HasMember("textColors"));
        CHECK(document.HasMember("metrics"));
        CHECK(document["surfaces"].MemberCount() == tokyonight::surfaceRegistry().size());
        CHECK(document["textColors"].MemberCount() == scene.skin.textColorRegistry().size());

        CerrCapture capture;

        scene.skin.loadTheme(document);
        CHECK(capture.buffer.str().empty());
    }
    CHECK(scene.skin.captureTheme() == changed);

    // the Load button: loadThemeFile(), the rows showing what it loaded
    ThemeEditorNode *button=scene.editor->surfaceNode("ButtonNormal");
    controls::ColorEdit *border=scene.colorEdit(button, "border");

    scene.skin.applyTheme(opened);
    scene.editor->revert();
    REQUIRE(scene.skin.captureTheme() == opened);

    const size_t redraws=scene.editor->redraws();

    scene.editor->loadButton()->onPressCaller.call(scene.editor->loadButton());
    CHECK(scene.skin.captureTheme() == changed);
    CHECK((border->getColor() == Color(40, 50, 60, 70)));
    CHECK(scene.editor->redraws() == redraws+1);
    CHECK(scene.editor->statusLine()->getText() == "Loaded "+path);
    CHECK((scene.label->getTextColor() == Color(200, 100, 50)));

    // a file that is not there
    scene.editor->setThemePath(dir.file("missing.json"));
    {
        CerrCapture capture;

        CHECK_FALSE(scene.editor->load());
        CHECK(capture.buffer.str().find("missing.json") != std::string::npos);
    }
    CHECK(scene.editor->statusLine()->getText().find("Could not load") == 0);
    CHECK(scene.skin.captureTheme() == changed);
}

TEST_CASE("Saving an unchanged theme again writes the same bytes")
{
    Scene scene;
    TempDir dir;
    const std::string path=dir.file("theme.json");

    scene.skin.applyTheme(changedTheme(scene.skin.captureTheme()));
    REQUIRE(scene.skin.saveThemeFile(path));

    const std::string first=readFile(path);

    REQUIRE(scene.skin.saveThemeFile(path));
    CHECK(readFile(path) == first);
    CHECK(scene.skin.themeJson()+"\n" == first);

    // and a theme with a fractional value and a translucent colour reads back
    // as the same floats and bytes
    TexturedSkin::ThemeValues fractional=scene.skin.captureTheme();

    fractional.surfaces.at("Menu").radius=0.1f;
    fractional.surfaces.at("Menu").borderWidth=1.25f;
    fractional.surfaces.at("Menu").top=Color(1, 2, 3, 4);
    scene.skin.applyTheme(fractional);
    REQUIRE(scene.skin.saveThemeFile(path));
    scene.skin.applyTheme(changedTheme(fractional));
    REQUIRE(scene.skin.loadThemeFile(path));
    CHECK(scene.skin.captureTheme() == fractional);
}

TEST_CASE("Saving into a skin config replaces its theme object and keeps the rest of the file")
{
    Scene scene;
    TempDir dir;
    const std::string theme=scene.skin.themeJson("kept");

    SUBCASE("a config with a theme")
    {
        const std::string head="{\n    \"textureFileName\": \"DefaultSkin.png\",\n    \"theme\":\n    ";
        const std::string tail="\n    ,\"map\": [ 1, 2 ]\n}\n";
        const std::string path=dir.file("skin.json");

        writeFile(path, head+"{ \"_comment\": \"kept\", \"surfaces\": { \"Panel\": { \"radius\": 1 } } }"+tail);
        REQUIRE(scene.skin.saveThemeFile(path));

        const std::string saved=readFile(path);

        REQUIRE(saved.size() > head.size()+tail.size());
        CHECK(saved.compare(0, head.size(), head) == 0);
        CHECK(saved.compare(saved.size()-tail.size(), tail.size(), tail) == 0);

        // the theme indented as its '{' is, its comment kept
        std::string indented;

        for(char c : theme)
        {
            indented+=c;
            if(c == '\n')
                indented+="    ";
        }
        CHECK(saved == head+indented+tail);

        json::Document document;

        document.Parse(saved.c_str());
        REQUIRE_FALSE(document.HasParseError());
        CHECK(document["theme"]["surfaces"].MemberCount() == tokyonight::surfaceRegistry().size());

        REQUIRE(scene.skin.saveThemeFile(path));
        CHECK(readFile(path) == saved);
    }

    SUBCASE("a config with a theme, CRLF")
    {
        const std::string head="{\r\n    \"colors\": {},\r\n    \"theme\": ";
        const std::string tail="\r\n}\r\n";
        const std::string path=dir.file("skin.json");

        writeFile(path, head+"{ \"_comment\": \"kept\" }"+tail);
        REQUIRE(scene.skin.saveThemeFile(path));

        const std::string saved=readFile(path);

        CHECK(saved.compare(0, head.size(), head) == 0);
        CHECK(saved.compare(saved.size()-tail.size(), tail.size(), tail) == 0);
        // every newline is CRLF
        for(size_t at=saved.find('\n'); at != std::string::npos; at=saved.find('\n', at+1))
            CHECK(saved[at-1] == '\r');

        REQUIRE(scene.skin.saveThemeFile(path));
        CHECK(readFile(path) == saved);
    }

    SUBCASE("a config with no theme")
    {
        const std::string path=dir.file("skin.json");
        const std::string original="{\n    \"textureFileName\": \"DefaultSkin.png\"\n}\n";

        writeFile(path, original);
        REQUIRE(scene.skin.saveThemeFile(path));

        const std::string saved=readFile(path);

        // the config up to its last value kept, then the theme after a comma
        const std::string kept=original.substr(0, original.find("\n}"));

        CHECK(saved.compare(0, kept.size()+1, kept+",") == 0);

        json::Document document;

        document.Parse(saved.c_str());
        REQUIRE_FALSE(document.HasParseError());
        CHECK(document["textureFileName"] == "DefaultSkin.png");
        CHECK(document["theme"]["metrics"]["ScrollBarThickness"].IsInt());

        REQUIRE(scene.skin.saveThemeFile(path));
        CHECK(readFile(path) == saved);
    }

    SUBCASE("a standalone theme file")
    {
        const std::string path=dir.file("theme.json");

        writeFile(path, "{ \"_comment\": \"kept\", \"textColors\": { \"LabelDefault\": [1,2,3] } }");
        REQUIRE(scene.skin.saveThemeFile(path));
        CHECK(readFile(path) == theme+"\n");
    }

    SUBCASE("a file that does not parse is left as it was")
    {
        const std::string path=dir.file("broken.json");
        const std::string broken="{ \"theme\": { \"surfaces\": ";

        writeFile(path, broken);

        CerrCapture capture;

        CHECK_FALSE(scene.skin.saveThemeFile(path));
        CHECK(readFile(path) == broken);
        const std::string message=capture.buffer.str();

        CHECK(std::count(message.begin(), message.end(), '\n') == 1);
        CHECK(message.find("broken.json") != std::string::npos);
    }

    SUBCASE("a theme that is not an object is left as it was")
    {
        const std::string path=dir.file("skin.json");
        const std::string original="{ \"theme\": 3 }";

        writeFile(path, original);

        CerrCapture capture;

        CHECK_FALSE(scene.skin.saveThemeFile(path));
        CHECK(readFile(path) == original);
    }
}

TEST_CASE("loadThemeFile reads a skin config's theme or a standalone theme")
{
    Scene scene;
    TempDir dir;

    writeFile(dir.file("skin.json"),
        "{ \"map\": [], \"theme\": { \"surfaces\": { \"Panel\": { \"top\": [1,2,3] } } } }");
    REQUIRE(scene.skin.loadThemeFile(dir.file("skin.json")));
    CHECK((tokyonight::Panel.top == Color(1, 2, 3)));

    writeFile(dir.file("theme.json"), "{ \"textColors\": { \"LabelDefault\": [4,5,6,7] } }");
    REQUIRE(scene.skin.loadThemeFile(dir.file("theme.json")));
    CHECK((scene.skin.Colors.Label.Default == Color(4, 5, 6, 7)));

    for(const char *bad : {"[]", "{ \"theme\": [] }", "{ nope"})
    {
        INFO(bad);
        writeFile(dir.file("bad.json"), bad);

        CerrCapture capture;

        CHECK_FALSE(scene.skin.loadThemeFile(dir.file("bad.json")));
        const std::string message=capture.buffer.str();

        CHECK(std::count(message.begin(), message.end(), '\n') == 1);
    }
}

TEST_CASE("A Text follows the skin's label colour until it is given its own")
{
    Scene scene;
    controls::Label *label=scene.canvas->newChild<controls::Label>();
    controls::internal::Text *text=nullptr;

    for(controls::Base *child : label->getActualChildren())
    {
        if(!text)
            text=gweni_cast<controls::internal::Text>(child);
    }

    REQUIRE(text);
    CHECK(text->getSkinTextColor() == controls::internal::SkinTextColor::LabelDefault);

    scene.skin.Colors.Label.Default=Color(9, 9, 9);
    text->refreshSkinTextColor();
    CHECK((label->getTextColor() == Color(9, 9, 9)));

    label->setTextColor(Color(1, 1, 1));
    CHECK(text->getSkinTextColor() == controls::internal::SkinTextColor::None);
    scene.skin.Colors.Label.Default=Color(8, 8, 8);
    text->refreshSkinTextColor();
    CHECK((label->getTextColor() == Color(1, 1, 1)));

    label->makeColorBright();
    CHECK(text->getSkinTextColor() == controls::internal::SkinTextColor::LabelBright);
    CHECK((label->getTextColor() == scene.skin.Colors.Label.Bright));
    scene.skin.Colors.Label.Bright=Color(7, 7, 7);
    ThemeEditor::applyToCanvas(scene.canvas);
    CHECK((label->getTextColor() == Color(7, 7, 7)));
}
