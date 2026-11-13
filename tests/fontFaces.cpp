/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Font families (IResourceLoader::setFontFamily, resources/DefaultSkin.json's
 *  "fonts"): a Font whose facename is a family's draws its bold, italic and
 *  bold italic text from the family's own faces, never an emboldened or
 *  slanted regular one. These cases pin, at a coverage size and an SDF size, at
 *  scales 1 and 2:
 *  - after TexturedSkin::init, OpenSans bold, italic and bold italic are drawn
 *    from OpenSans-Bold, -Italic and -BoldItalic.ttf, byte for byte what those
 *    files draw named as faces of their own, and unlike each other;
 *  - from a copy of resources/ without OpenSans-Bold.ttf, bold draws the
 *    regular face's pixels, the other styles keep theirs, and one log line
 *    names the missing file however many sizes ask for it;
 *  - a style a family has no face for, and bold of a face that is no family's,
 *    draw the regular face with one log line per face;
 *  - a family set after one of its fonts is loaded loads it again, so it then
 *    draws the family's face, and raises textLayoutSerial();
 *  - "mono" is DejaVu Sans Mono: it draws as that face does and the default
 *    glyph chain [DejaVuSansMono] is no chain of its own.
 *  Each case uses a renderer of its own over the shared view.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/platforms/commonPlatform.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/utils/filesystem.h>

#include <freetype-gl/texture-font.h>

#include <memory>
#include <string>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Font;
using gweni::FontFamily;
using gweni::Point;
using gweni::String;
using gweni::renderer::SlotRenderer;
using gweni::skin::textured::TexturedSkin;

const String sample="Hamburgefonstiv Quaffle";

//! Log lines written while one is alive
struct LogCapture
{
    LogCapture() { lines().clear(); previous=gweni::log::setLogCaller(&LogCapture::write); }
    ~LogCapture() { gweni::log::setLogCaller(previous); }

    static std::vector<std::string> &lines()
    {
        static std::vector<std::string> captured;
        return captured;
    }

    static void write(gweni::log::Level, const char *message) { lines().emplace_back(message); }

    //! The lines holding text
    static size_t count(const std::string &text)
    {
        size_t n=0;

        for(const std::string &line:lines())
            n+=(line.find(text) != std::string::npos)?1:0;
        return n;
    }

    gweni::log::LogCaller previous;
};

//! A renderer of its own over the shared view, loading from dir, and, if asked,
//! a textured skin on it init()'d from DefaultSkin.json
struct Fresh
{
    explicit Fresh(const String &dir, bool withSkin):
        paths(dir),
        owned(gweni::test::newRenderer(paths, gweni::Rect(Point(0, 0), gweni::test::viewSize()))),
        renderer(*owned)
    {
        renderer.init();
        if(withSkin)
        {
            skin=std::make_unique<TexturedSkin>(owned.get());
            skinInit=skin->init("DefaultSkin.json");
        }
        renderer.generateTextPrimitive(id);
    }

    ~Fresh()
    {
        renderer.releaseTextPrimitive(id);
        renderer.begin();
        renderer.end();
        renderer.setScale(1.0f);
        skin.reset();
    }

    //! One frame of text in white on black, read back
    std::vector<uint8_t> draw(const Font &font, const String &text=sample)
    {
        std::vector<uint8_t> pixels;

        gweni::test::clearView(Color(0, 0, 0, 255));
        renderer.begin();
        renderer.setDrawColor(Color(255, 255, 255, 255));
        renderer.renderText(id, font, Point(10, 10), 1, text);
        renderer.end();
        gweni::test::readView(pixels);
        return pixels;
    }

    //! The file the font's atlas was loaded from, "" if none
    std::string file(const Font &font) const
    {
        ftgl::texture_font_t *textureFont=renderer.getTextureFont(font);

        if((textureFont == nullptr) || (textureFont->location != ftgl::texture_font_t::TEXTURE_FONT_FILE))
            return std::string();
        return fs::path(textureFont->filename).filename().string();
    }

    gweni::platform::CurrentPath paths;
    std::unique_ptr<SlotRenderer> owned;
    SlotRenderer &renderer;
    std::unique_ptr<TexturedSkin> skin; //!< goes before its renderer
    bool skinInit=false;
    size_t id=0;
};

Font font(const String &face, float size, bool bold=false, bool italic=false)
{
    Font made;

    made.facename=face;
    made.size=size;
    made.bold=bold;
    made.italic=italic;
    return made;
}

struct Style
{
    const char *name;
    bool bold;
    bool italic;
    const char *face; //!< its file in resources/, ".ttf" left off
};

const Style styles[]={
    { "regular", false, false, "OpenSans" },
    { "bold", true, false, "OpenSans-Bold" },
    { "italic", false, true, "OpenSans-Italic" },
    { "bold italic", true, true, "OpenSans-BoldItalic" },
};

//! A copy of resources/ less one file
fs::path resourcesWithout(const std::string &name, const std::string &missing)
{
    const fs::path dir=fs::temp_directory_path()/name;

    fs::remove_all(dir);
    fs::create_directories(dir);
    for(const auto &entry:fs::directory_iterator("resources"))
    {
        if(entry.path().filename() != missing)
            fs::copy(entry.path(), dir/entry.path().filename(), fs::copy_options::recursive);
    }
    return dir;
}

}//namespace

TEST_CASE("Font faces: the skin's OpenSans family draws bold, italic and bold italic from their own files")
{
    Fresh fresh("resources", true);

    REQUIRE(fresh.skinInit);

    const FontFamily *family=fresh.renderer.fontFamily("OpenSans");

    REQUIRE(static_cast<bool>(family));
    CHECK(family->bold == "OpenSans-Bold");
    CHECK(family->italic == "OpenSans-Italic");
    CHECK(family->boldItalic == "OpenSans-BoldItalic");

    LogCapture log;

    for(float size:{ 13.0f, 26.0f })
    for(float scale:{ 1.0f, 2.0f })
    {
        CAPTURE(size);
        CAPTURE(scale);
        fresh.renderer.setScale(scale);

        std::vector<std::vector<uint8_t>> drawn;

        for(const Style &style:styles)
        {
            CAPTURE(style.name);

            const Font styled=font("OpenSans", size, style.bold, style.italic);
            const Font asFile=font(style.face, size);

            CHECK(fresh.renderer.fontFaceName(styled) == String(style.face));
            REQUIRE(static_cast<int>(fresh.renderer.loadFont(styled)) == static_cast<int>(Font::Status::Loaded));
            CHECK(fresh.file(styled) == std::string(style.face)+".ttf");
            drawn.push_back(fresh.draw(styled));

            // the face's own file, named as a face, draws the same pixels
            REQUIRE(static_cast<int>(fresh.renderer.loadFont(asFile)) == static_cast<int>(Font::Status::Loaded));
            CHECK((fresh.draw(asFile) == drawn.back()));
            fresh.renderer.freeFont(asFile);
            fresh.renderer.freeFont(styled);
        }
        for(size_t a=0; a < drawn.size(); ++a)
        {
            for(size_t b=a+1; b < drawn.size(); ++b)
            {
                CAPTURE(styles[a].name);
                CAPTURE(styles[b].name);
                CHECK((drawn[a] != drawn[b]));
            }
        }
    }
    CHECK(log.lines().empty());
}

TEST_CASE("Font faces: without OpenSans-Bold.ttf, bold draws the regular face and one line names the file")
{
    const fs::path dir=resourcesWithout("gweni-fontFaces-nobold", "OpenSans-Bold.ttf");

    {
        LogCapture log;
        Fresh fresh(dir.string(), true);

        // a style's file is looked for when a font of it loads: init does not fail
        CHECK(fresh.skinInit);
        CHECK(log.lines().empty());

        for(float size:{ 13.0f, 26.0f })
        {
            CAPTURE(size);

            const Font regular=font("OpenSans", size);
            const Font bold=font("OpenSans", size, true);
            const Font italic=font("OpenSans", size, false, true);

            REQUIRE(static_cast<int>(fresh.renderer.loadFont(bold)) == static_cast<int>(Font::Status::Loaded));
            CHECK(fresh.file(bold) == "OpenSans.ttf");
            CHECK((fresh.draw(bold) == fresh.draw(regular)));
            CHECK((fresh.draw(italic) != fresh.draw(regular)));
            CHECK(fresh.file(italic) == "OpenSans-Italic.ttf");
            fresh.renderer.freeFont(bold);
        }
        CHECK(log.count("OpenSans-Bold.ttf") == 1);
        CHECK(log.lines().size() == 1);
    }

    fs::remove_all(dir);
}

TEST_CASE("Font faces: a style with no face, and bold of a face that is no family's, draw regular with one line each")
{
    LogCapture log;
    Fresh fresh("resources", false);
    FontFamily mono;

    mono.regular="DejaVuSansMono";
    fresh.renderer.setFontFamily("mono", mono);

    for(float size:{ 13.0f, 26.0f })
    {
        CAPTURE(size);

        const Font monoBold=font("mono", size, true);
        const Font liberationBold=font("LiberationSans-Regular", size, true);

        REQUIRE(static_cast<int>(fresh.renderer.loadFont(monoBold)) == static_cast<int>(Font::Status::Loaded));
        REQUIRE(static_cast<int>(fresh.renderer.loadFont(liberationBold)) == static_cast<int>(Font::Status::Loaded));
        CHECK(fresh.file(monoBold) == "DejaVuSansMono.ttf");
        CHECK(fresh.file(liberationBold) == "LiberationSans-Regular.ttf");
        CHECK((fresh.draw(monoBold) == fresh.draw(font("DejaVuSansMono", size))));
        CHECK((fresh.draw(liberationBold) == fresh.draw(font("LiberationSans-Regular", size))));
        fresh.renderer.freeFont(monoBold);
        fresh.renderer.freeFont(liberationBold);
    }
    CHECK(log.count("No bold face for mono") == 1);
    CHECK(log.count("No bold face for LiberationSans-Regular") == 1);
    CHECK(log.lines().size() == 2);
}

TEST_CASE("Font faces: a family set after its font is loaded loads it again in the family's face")
{
    Fresh fresh("resources", false);
    const Font bold=font("OpenSans", 13.0f, true);

    {
        LogCapture log;

        REQUIRE(static_cast<int>(fresh.renderer.loadFont(bold)) == static_cast<int>(Font::Status::Loaded));
        CHECK(fresh.file(bold) == "OpenSans.ttf");
        CHECK(log.count("No bold face for OpenSans") == 1);
    }

    const std::vector<uint8_t> before=fresh.draw(bold);
    const unsigned serial=fresh.renderer.textLayoutSerial();
    FontFamily family;

    family.regular="OpenSans";
    family.bold="OpenSans-Bold";
    fresh.renderer.setFontFamily("OpenSans", family);

    CHECK(fresh.renderer.textLayoutSerial() != serial);
    CHECK(fresh.file(bold) == "OpenSans-Bold.ttf");

    const std::vector<uint8_t> after=fresh.draw(bold);

    CHECK((after != before));

    // the same family again changes nothing
    const unsigned settled=fresh.renderer.textLayoutSerial();

    fresh.renderer.setFontFamily("OpenSans", family);
    CHECK(fresh.renderer.textLayoutSerial() == settled);
    fresh.renderer.freeFont(bold);
}

TEST_CASE("Font faces: \"mono\" is DejaVu Sans Mono and the default chain is none of its own")
{
    Fresh fresh("resources", true);

    REQUIRE(fresh.skinInit);
    CHECK(fresh.renderer.glyphFallback("mono").empty());
    CHECK(fresh.renderer.glyphFallback("DejaVuSansMono").empty());
    CHECK(fresh.renderer.glyphFallback("OpenSans") == std::vector<String>{ "DejaVuSansMono" });

    const Font mono=font("mono", 13.0f);

    REQUIRE(static_cast<int>(fresh.renderer.loadFont(mono)) == static_cast<int>(Font::Status::Loaded));
    CHECK(fresh.file(mono) == "DejaVuSansMono.ttf");
    CHECK((fresh.draw(mono) == fresh.draw(font("DejaVuSansMono", 13.0f))));
    fresh.renderer.freeFont(mono);
}
