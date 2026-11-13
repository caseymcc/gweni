/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  RichText: one paragraph of styled spans (bold, italic, code, coloured,
 *  struck, underlined, links), laid out as one by text::RichLayout and drawn by
 *  the textured skin's real RichText renderer on a recording StubRenderer
 *  (StubControls for every other type; a byte is 4.4 units wide at the
 *  default 11, a line 11 high). It checks:
 *  - each piece is drawn as a run of its own, in its span's font (the
 *    family's bold or italic face, the mono face for code) and colour (its
 *    own, the skin's Label.Default, a link's RichText.Link), at the piece's
 *    place and getZIndex()+1, and the control is as wide as its text on one
 *    line and as high as its lines at the width it is given;
 *  - a bold or italic font given to setFont draws every span bold or italic,
 *    each span's own style added to it;
 *  - a code piece's background (RichTextCode over its box) at getZIndex(), and
 *    a link's underline and a struck piece's line as rules (icon kind 6) in
 *    its colour at getZIndex()+2, and the next control numbered past those;
 *  - a new width wraps the paragraph again, drawing only its own runs and
 *    slots, and a settled frame draws nothing;
 *  - a changed span draws only its paragraph's runs and slots (another
 *    RichText's none), measures the spans once and lays out the RichText and
 *    its ancestors only;
 *  - hidden, every run and slot is hidden, and shown again, drawn; deleted,
 *    every run and slot is given back; given fewer spans while hidden (its
 *    stale pieces naming spans taken away), it draws nothing until shown, and
 *    then the new spans alone;
 *  - the theme registries name RichTextCode, RichTextLink and
 *    RichTextLinkHover;
 *  - a hovered link is drawn in RichText.LinkHover with cursor::Finger, the
 *    frame drawing only its runs and its underline, a move inside it nothing
 *    and a move off it the same runs and line back in RichText.Link with the
 *    text cursor (Beam); a link of several spans (one url) lights up whole;
 *  - a press and a release on one link call onLinkClicked once, with its url
 *    and first span; a release off it, or a press off any link, call nothing,
 *    even with nothing selected (either side of the link's edge, one byte),
 *    each press a second after the last on a stepped clock, so none is a
 *    double-click.
 */
#include "richTextFixture.h"

#include <gweni/controls/richText.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <doctest/doctest.h>

#include <map>
#include <set>
#include <vector>

using namespace gweni;
using controls::RichSpan;
using controls::RichText;
using test::DrawRecorder;
using test::RichSkin;

namespace
{

std::vector<RichSpan> sampleSpans()
{
    return {
        RichSpan::plain("Some "),
        RichSpan::plain("bold", RichSpan::Bold),
        RichSpan::plain(" and "),
        RichSpan::plain("italic", RichSpan::Italic),
        RichSpan::plain(" with "),
        RichSpan::plain("code()", RichSpan::Code),
        RichSpan::plain(", a "),
        RichSpan::link("link", "https://example.com/"),
        RichSpan::plain(" and "),
        RichSpan::colored("struck red", Color(255, 0, 0, 255), RichSpan::Strike),
        RichSpan::plain(" text."),
    };
}

struct Scene
{
    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(600, 300);
        // placed apart, so one's new size moves nothing of the other
        rich=canvas->newChild<RichText>();
        rich->setSizeFlags({ SizeFlag::Fixed, SizeFlag::Elastic });
        rich->setPos(0, 0);
        rich->setWidth(600);
        rich->setSpans(sampleSpans());
        other=canvas->newChild<RichText>();
        other->setSizeFlags({ SizeFlag::Fixed, SizeFlag::Elastic });
        other->setPos(0, 200);
        other->setWidth(600);
        other->setSpans({ RichSpan::plain("Another paragraph with "), RichSpan::plain("code", RichSpan::Code) });
        frame();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        // the hovered and mouse-focused controls are process-global
        gweni::HoveredControl=nullptr;
        gweni::MouseFocus=nullptr;
        delete canvas;
    }

    void frame()
    {
        renderer.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    std::set<size_t> ids(RichText *control) const
    {
        std::vector<size_t> primitives, texts;

        control->getSkinControl()->appendIds(primitives, texts);

        std::set<size_t> all(primitives.begin(), primitives.end());

        all.insert(texts.begin(), texts.end());
        return all;
    }

    std::set<size_t> textIds(RichText *control) const
    {
        std::vector<size_t> primitives, texts;

        control->getSkinControl()->appendIds(primitives, texts);
        return std::set<size_t>(texts.begin(), texts.end());
    }

    std::set<size_t> slotIds(RichText *control) const
    {
        std::vector<size_t> primitives, texts;

        control->getSkinControl()->appendIds(primitives, texts);
        return std::set<size_t>(primitives.begin(), primitives.end());
    }

    //! Whether this frame drew only control's runs and slots
    bool drewOnly(RichText *control) const
    {
        const std::set<size_t> texts=textIds(control), slots=slotIds(control);

        for(const DrawRecorder::TextDraw &draw:renderer.texts)
        {
            if(texts.count(draw.id) == 0)
                return false;
        }
        for(const DrawRecorder::SlotDraw &draw:renderer.slots)
        {
            if(slots.count(draw.id) == 0)
                return false;
        }
        return true;
    }

    DrawRecorder renderer;
    RichSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    RichText *rich=nullptr;
    RichText *other=nullptr;
};

const DrawRecorder::TextDraw *drawOf(const DrawRecorder &renderer, const String &text)
{
    for(const DrawRecorder::TextDraw &draw:renderer.texts)
    {
        if(draw.text == text)
            return &draw;
    }
    return nullptr;
}

}//namespace

TEST_CASE("RichText: each piece a run in its span's font and colour, the control its lines' size")
{
    Scene scene;
    RichText *rich=scene.rich;
    const text::RichLayout &layout=rich->getLayout();

    // drawn whole on the first frame: one run per span, all on one line at 600
    scene.renderer.clear();
    rich->addStateChange(controls::StateChange_Geometry);
    scene.frame();
    REQUIRE(layout.lines().size() == 1);
    REQUIRE(layout.pieces().size() == rich->getSpanCount());
    CHECK(rich->getBounds().w == 600);
    CHECK(rich->getBounds().h == 11);
    // asked for no width, it is its text's on one line
    {
        Scene own;

        own.rich->setSizeFlags({ SizeFlag::Elastic, SizeFlag::Elastic });
        own.frame();
        CHECK(own.rich->getBounds().w == own.rich->getLayout().size().x);
        CHECK(own.rich->getLayout().lines().size() == 1);
        CHECK(own.rich->getBounds().w < 600);
    }
    CHECK(rich->getText() == "Some bold and italic with code(), a link and struck red text.");

    const DrawRecorder::TextDraw *bold=drawOf(scene.renderer, "bold");
    const DrawRecorder::TextDraw *italic=drawOf(scene.renderer, "italic");
    const DrawRecorder::TextDraw *code=drawOf(scene.renderer, "code()");
    const DrawRecorder::TextDraw *link=drawOf(scene.renderer, "link");
    const DrawRecorder::TextDraw *struck=drawOf(scene.renderer, "struck red");
    const DrawRecorder::TextDraw *plain=drawOf(scene.renderer, "Some ");

    REQUIRE(bold);
    REQUIRE(italic);
    REQUIRE(code);
    REQUIRE(link);
    REQUIRE(struck);
    REQUIRE(plain);
    CHECK(plain->font.facename == scene.skin.getDefaultFont().facename);
    CHECK_FALSE(plain->font.bold);
    CHECK(bold->font.bold);
    CHECK_FALSE(bold->font.italic);
    CHECK(italic->font.italic);
    CHECK(code->font.facename == scene.skin.getDefaultMonoFont().facename);
    CHECK(code->font.size == scene.skin.getDefaultFont().size);
    CHECK((plain->color == scene.skin.Colors.Label.Default));
    CHECK((link->color == scene.skin.Colors.RichText.Link));
    CHECK((struck->color == Color(255, 0, 0, 255)));
    for(const DrawRecorder::TextDraw &draw:scene.renderer.texts)
        CHECK(draw.zIndex == rich->getZIndex()+1);

    // each piece's run at its place in the layout
    std::vector<size_t> primitives, runs;

    rich->getSkinControl()->appendIds(primitives, runs);
    REQUIRE(runs.size() == layout.pieces().size());
    for(size_t i=0; i < runs.size(); ++i)
    {
        const text::RichPiece &piece=layout.pieces()[i];
        const DrawRecorder::TextDraw *draw=nullptr;

        for(const DrawRecorder::TextDraw &each:scene.renderer.texts)
        {
            if(each.id == runs[i])
                draw=&each;
        }
        REQUIRE(draw);
        CHECK(draw->text == piece.text);
        CHECK(draw->pos.x == rich->getRenderBounds().x+piece.pos.x);
        CHECK(draw->pos.y == rich->getRenderBounds().y+piece.pos.y);
    }
}

TEST_CASE("RichText: a bold or italic font given to setFont is every span's, a span's style added to it")
{
    Scene scene;
    RichText *rich=scene.rich;
    Font font=scene.skin.getDefaultFont();

    font.bold=true;
    rich->setFont(font);
    for(size_t i=0; i < rich->getSpanCount(); ++i)
    {
        CAPTURE(i);

        const gweni::Font drawn=rich->spanFont(i);
        const int style=rich->getSpans()[i].style;

        CHECK(drawn.bold);
        CHECK(drawn.italic == ((style&RichSpan::Italic) != 0));
    }

    font.bold=false;
    font.italic=true;
    rich->setFont(font);
    for(size_t i=0; i < rich->getSpanCount(); ++i)
    {
        CAPTURE(i);

        const gweni::Font drawn=rich->spanFont(i);

        CHECK(drawn.italic);
        CHECK(drawn.bold == ((rich->getSpans()[i].style&RichSpan::Bold) != 0));
    }
}

TEST_CASE("RichText: code backgrounds behind the glyphs, link and strike rules over them")
{
    Scene scene;
    RichText *rich=scene.rich;

    scene.renderer.clear();
    rich->addStateChange(controls::StateChange_Geometry);
    scene.frame();

    const text::RichLayout &layout=rich->getLayout();
    const int z=rich->getZIndex();
    size_t backgrounds=0, rules=0;

    for(const DrawRecorder::SlotDraw &slot:scene.renderer.slots)
    {
        if(scene.slotIds(rich).count(slot.id) == 0)
            continue;
        if(!slot.rule)
        {
            ++backgrounds;
            CHECK(slot.zIndex == z);
            CHECK((slot.color == skin::textured::tokyonight::RichTextCode.top));
            // the code piece's box: its text and the padding either side
            const text::RichPiece &code=layout.pieces()[5];

            CHECK(slot.rect.x == code.pos.x-RichText::CodePadding);
            CHECK(slot.rect.w == code.width+2*RichText::CodePadding);
            CHECK(slot.rect.h == code.metrics.lineHeight);
        }
        else
        {
            ++rules;
            CHECK(slot.zIndex == z+2);
        }
    }
    CHECK(backgrounds == 1);
    // the link's underline and the struck span's line
    CHECK(rules == 2);

    const text::RichPiece &link=layout.pieces()[7];
    bool underline=false;

    for(const DrawRecorder::SlotDraw &slot:scene.renderer.slots)
    {
        if(slot.rule && (slot.rect.x == link.pos.x) && (slot.rect.w == link.width))
        {
            underline=true;
            CHECK((slot.color == scene.skin.Colors.RichText.Link));
            CHECK(slot.rect.y == link.pos.y+link.metrics.ascender);
        }
    }
    CHECK(underline);

    // the next control is numbered past the rules
    CHECK(scene.other->getZIndex() >= z+3);
}

TEST_CASE("RichText: a new width wraps the paragraph again and draws only its own runs and slots")
{
    Scene scene;
    RichText *rich=scene.rich;

    // settled: nothing drawn
    scene.frame();
    CHECK(scene.renderer.texts.empty());
    CHECK(scene.renderer.slots.empty());

    const size_t measured=rich->measureCount();
    const int oneLine=rich->getBounds().h;

    rich->setWidth(120);
    scene.frame();

    CHECK(rich->getBounds().w == 120);
    CHECK(rich->getLayout().lines().size() > 1);
    CHECK(rich->getBounds().h == rich->getLayout().size().y);
    CHECK(rich->getBounds().h > oneLine);
    CHECK_FALSE(scene.renderer.texts.empty());
    CHECK(scene.drewOnly(rich));
    // laid out at a new width without measuring the spans again
    CHECK(rich->measureCount() == measured);
    for(const text::RichLine &line:rich->getLayout().lines())
        CHECK(line.width <= 120+1);

    scene.frame();
    CHECK(scene.renderer.texts.empty());
    CHECK(scene.renderer.slots.empty());
}

TEST_CASE("RichText: a changed span draws its paragraph's runs alone and lays out only it and its ancestors")
{
    Scene scene;
    RichText *rich=scene.rich;

    scene.frame();

    const size_t measured=rich->measureCount();
    const size_t otherMeasured=scene.other->measureCount();

    rich->setSpanText(1, "heavy");
    scene.frame();

    CHECK(rich->measureCount() == measured+1);
    CHECK(scene.other->measureCount() == otherMeasured);
    CHECK(scene.drewOnly(rich));
    REQUIRE(drawOf(scene.renderer, "heavy"));
    CHECK(drawOf(scene.renderer, "heavy")->font.bold);
    // the pieces after it moved; the ones before it did not
    CHECK(drawOf(scene.renderer, "Some ") == nullptr);

    // the same text again changes nothing
    rich->setSpanText(1, "heavy");
    scene.frame();
    CHECK(scene.renderer.texts.empty());
    CHECK(rich->measureCount() == measured+1);
}

TEST_CASE("RichText: hidden every run and slot hidden, shown drawn again, deleted given back")
{
    const test::IdBankCounts before=test::IdBankCounts::now();

    {
        Scene scene;
        RichText *rich=scene.rich;

        scene.frame();
        rich->hide();
        scene.frame();

        const std::set<size_t> texts=scene.textIds(rich), slots=scene.slotIds(rich);

        CHECK(std::set<size_t>(scene.renderer.hiddenTexts.begin(), scene.renderer.hiddenTexts.end()) == texts);
        CHECK(std::set<size_t>(scene.renderer.hiddenSlots.begin(), scene.renderer.hiddenSlots.end()) == slots);
        CHECK(texts.size() == rich->getLayout().pieces().size());
        CHECK(slots.size() == 3);

        rich->show();
        scene.frame();
        CHECK(scene.renderer.slots.size() == 3);
        CHECK(scene.renderer.texts.size() == rich->getLayout().pieces().size());
    }

    const test::IdBankCounts after=test::IdBankCounts::now();

    CHECK(after.livePrimitives() == before.livePrimitives());
    CHECK(after.liveTexts() == before.liveTexts());
}

TEST_CASE("RichText: hidden and given fewer spans, nothing drawn until shown, then the new spans")
{
    const test::IdBankCounts before=test::IdBankCounts::now();

    {
        Scene scene;
        RichText *rich=scene.rich;
        const size_t pieces=rich->getLayout().pieces().size();

        REQUIRE(pieces > 1);

        // hidden in the frame the spans go: not laid out, so its pieces still
        // name the spans taken away
        rich->setSpans({ RichSpan::plain("x") });
        rich->hide();
        scene.frame();
        CHECK(rich->getLayout().pieces().size() == pieces);
        CHECK(scene.renderer.texts.empty());
        CHECK(scene.renderer.slots.empty());
        CHECK(scene.renderer.hiddenTexts.size() == pieces);
        CHECK(scene.renderer.hiddenSlots.size() == 3);

        // and again while hidden, with fewer still: nothing drawn or hidden
        rich->setSpans({});
        scene.frame();
        CHECK(scene.renderer.texts.empty());
        CHECK(scene.renderer.slots.empty());
        CHECK(scene.renderer.hiddenTexts.empty());
        CHECK(scene.renderer.hiddenSlots.empty());

        rich->setSpans({ RichSpan::plain("shown", RichSpan::Bold) });
        rich->show();
        scene.frame();
        REQUIRE(rich->getLayout().pieces().size() == 1);
        REQUIRE(scene.renderer.texts.size() == 1);
        CHECK(scene.renderer.texts[0].text == "shown");
        CHECK(scene.renderer.slots.empty());
        CHECK(scene.textIds(rich).size() == 1);
        CHECK(scene.slotIds(rich).empty());
    }

    const test::IdBankCounts after=test::IdBankCounts::now();

    CHECK(after.livePrimitives() == before.livePrimitives());
    CHECK(after.liveTexts() == before.liveTexts());
}

TEST_CASE("RichText: the theme names its code surface and link colours")
{
    test::StubRenderer renderer;
    skin::textured::TexturedSkin skin(&renderer);

    CHECK(skin::textured::tokyonight::surfaceRegistry().count("RichTextCode") == 1);

    const auto colors=skin.textColorRegistry();

    REQUIRE(colors.count("RichTextLink") == 1);
    REQUIRE(colors.count("RichTextLinkHover") == 1);
    CHECK((colors.at("RichTextLink") == &skin.Colors.RichText.Link));
    CHECK((colors.at("RichTextLinkHover") == &skin.Colors.RichText.LinkHover));
}

namespace
{

//! The canvas point at the centre of piece \p index's box
Point pieceCentre(RichText *rich, size_t index)
{
    const text::RichPiece &piece=rich->getLayout().pieces().at(index);
    const Rect box=piece.box();

    return rich->localPosToCanvas(Point(rich->contentOrigin().x+box.x+box.w/2,
        rich->contentOrigin().y+box.y+box.h/2));
}

void moveTo(Scene &scene, const Point &at)
{
    scene.canvas->inputMouseMoved(at.x, at.y, 0, 0);
    scene.frame();
}

size_t runOf(RichText *rich, size_t piece)
{
    std::vector<size_t> primitives, runs;

    rich->getSkinControl()->appendIds(primitives, runs);
    return runs.at(piece);
}

}//namespace

TEST_CASE("RichText: a hovered link is drawn in LinkHover with the Finger cursor, its runs and line alone")
{
    Scene scene;
    RichText *rich=scene.rich;
    const Color link=scene.skin.Colors.RichText.Link;
    const Color hover=scene.skin.Colors.RichText.LinkHover;

    REQUIRE_FALSE((link == hover));
    moveTo(scene, pieceCentre(rich, 0));
    CHECK(gweni::HoveredControl == rich);
    CHECK(rich->getHoveredLink() == -1);
    CHECK(rich->getCursor() == cursor::Beam);

    moveTo(scene, pieceCentre(rich, 7));
    CHECK(rich->getHoveredLink() == 7);
    CHECK(rich->getCursor() == cursor::Finger);
    REQUIRE(scene.renderer.texts.size() == 1);
    CHECK(scene.renderer.texts[0].id == runOf(rich, 7));
    CHECK((scene.renderer.texts[0].color == hover));
    REQUIRE(scene.renderer.slots.size() == 1);
    CHECK(scene.renderer.slots[0].rule);
    CHECK((scene.renderer.slots[0].color == hover));
    CHECK(scene.drewOnly(rich));

    // inside the same link: nothing
    const Point inside=pieceCentre(rich, 7);

    moveTo(scene, Point(inside.x+1, inside.y));
    CHECK(scene.renderer.texts.empty());
    CHECK(scene.renderer.slots.empty());

    // off it: the same run and line back in the link colour
    moveTo(scene, pieceCentre(rich, 8));
    CHECK(rich->getHoveredLink() == -1);
    CHECK(rich->getCursor() == cursor::Beam);
    REQUIRE(scene.renderer.texts.size() == 1);
    CHECK(scene.renderer.texts[0].id == runOf(rich, 7));
    CHECK((scene.renderer.texts[0].color == link));
    REQUIRE(scene.renderer.slots.size() == 1);
    CHECK((scene.renderer.slots[0].color == link));

    // leaving the control unhovers it too
    moveTo(scene, pieceCentre(rich, 7));
    CHECK(rich->getHoveredLink() == 7);
    scene.canvas->inputMouseMoved(300, 150, 0, 0);
    scene.frame();
    CHECK(rich->getHoveredLink() == -1);
}

TEST_CASE("RichText: a link of several spans with one url is hovered whole")
{
    Scene scene;
    RichText *rich=scene.rich;

    rich->setSpans({ RichSpan::plain("see "), RichSpan::link("the ", "doc"),
        RichSpan::link("docs", "doc", RichSpan::Bold), RichSpan::link(" other", "elsewhere") });
    scene.frame();
    REQUIRE(rich->getLayout().pieces().size() == 4);

    moveTo(scene, pieceCentre(rich, 2));
    CHECK(rich->getHoveredLink() == 1);
    CHECK(rich->isLinkHovered(1));
    CHECK(rich->isLinkHovered(2));
    CHECK_FALSE(rich->isLinkHovered(3));
    CHECK(scene.renderer.texts.size() == 2);

    moveTo(scene, pieceCentre(rich, 3));
    CHECK(rich->getHoveredLink() == 3);
    // the two spans of the first link and the one of the second
    CHECK(scene.renderer.texts.size() == 3);
}

TEST_CASE("RichText: a press and release on one link call onLinkClicked with its url")
{
    // a stepped clock, a second between the presses, so none is a double-click;
    // given back however the case ends (a failed REQUIRE throws)
    double clock=100.0;
    struct ClockGuard
    {
        ~ClockGuard() { platform::setTimeSource(nullptr); }
    } clockGuard;

    platform::setTimeSource([&clock]() { return clock; });
    {
        Scene scene;
        RichText *rich=scene.rich;
        std::vector<String> urls;
        std::vector<size_t> firsts;
        const text::RichPiece &link=rich->getLayout().pieces().at(7);

        REQUIRE(rich->getLayout().pieces().at(6).offset+rich->getLayout().pieces().at(6).text.size() == link.offset);
        // either side of the link's left edge, both at the link's first byte,
        // so a press at one and a release at the other select nothing
        const Point onEdge=rich->localPosToCanvas(Point(rich->contentOrigin().x+link.box().x+1,
            rich->contentOrigin().y+link.box().y+link.box().h/2));
        const Point offEdge(onEdge.x-2, onEdge.y);
        auto click=[&](const Point &down, const Point &up)
        {
            clock+=1.0;
            moveTo(scene, down);
            scene.canvas->inputMouseButton(0, true);
            scene.frame();
            moveTo(scene, up);
            scene.canvas->inputMouseButton(0, false);
            scene.frame();
        };

        rich->onLinkClicked.addCallback(scene.canvas,
            [&](event::Handler &, event::Info info) { urls.push_back(info.string); firsts.push_back(info.integer); });

        // on the link
        click(pieceCentre(rich, 7), pieceCentre(rich, 7));
        REQUIRE(urls.size() == 1);
        CHECK(urls[0] == "https://example.com/");
        CHECK(firsts[0] == 7);

        // pressed on it, let go off it, nothing selected
        click(onEdge, offEdge);
        CHECK_FALSE(rich->hasSelection());
        CHECK(urls.size() == 1);

        // pressed off it, let go on it, nothing selected
        click(offEdge, onEdge);
        CHECK_FALSE(rich->hasSelection());
        CHECK(urls.size() == 1);

        // and at the edge's link side alone, a click
        click(onEdge, onEdge);
        CHECK(urls.size() == 2);
    }
}
