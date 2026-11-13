/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  CodeEditor's highlighting on a real Canvas with the textured skin over a
 *  StubRenderer (its CodeLineText renderer draws a row's run through
 *  renderText's colour-span overload, which the stub records):
 *  setLanguage("yaml") draws each shown row's run with one span per token of
 *  its line in the skin's Colors.CodeEditor colour of the token's kind, the
 *  Text's own colour the Plain one; an edit that keeps the state the next
 *  line starts in tokenizes and re-colours its line alone, marking no other
 *  row; one that
 *  opens a block scalar re-colours the rows on screen below it, the lines
 *  past the screen left stale until bound, and its undo gives every row's
 *  spans back; a new token colour reaches the spans through updateColors();
 *  an unknown language highlights nothing, "" and a null tokenizer none.
 */
#include "headlessFixture.h"

#include <gweni/controls/codeEditor.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/text/tokenizer.h>

#include <doctest/doctest.h>

#include <map>
#include <string>
#include <vector>

using namespace gweni;
using renderer::ColorSpan;

namespace
{

//! Sections of keys, one every \p every lines, their values a number and a
//! quoted string in turn
String document(size_t lines, size_t every)
{
    String text;

    for(size_t i=0; i < lines; ++i)
    {
        if(i > 0)
            text+='\n';
        if(i%every == 0)
            text+="section_"+std::to_string(i/every)+":  # part "+std::to_string(i/every);
        else
            text+="  key_"+std::to_string(i)+": "+((i%2)?std::to_string(i*7):"\"text\"");
    }
    return text;
}

struct Scene
{
    test::StubRenderer renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::CodeEditor *editor=nullptr;

    explicit Scene(size_t lines=200, int height=200, size_t sectionEvery=10)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(600, 600);

        editor=canvas->newChild<controls::CodeEditor>();
        editor->setPos(0, 0);
        editor->setSize(300, height);
        REQUIRE(editor->setLanguage("yaml"));
        editor->setText(document(lines, sectionEvery));
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

    //! Every shown row by the line it shows
    std::map<size_t, controls::CodeLine *> shownRows() const
    {
        std::map<size_t, controls::CodeLine *> rows;

        for(controls::Base *row : editor->getRows())
        {
            controls::CodeLine *line=gweni_cast<controls::CodeLine>(row);

            if(line && !row->hidden() && (line->getLine() != controls::CodeLine::NoLine))
                rows[line->getLine()]=line;
        }
        return rows;
    }

    //! The spans line \p line's tokens give in the skin's colours, tokenized
    //! afresh from the document's top
    std::vector<ColorSpan> expectedSpans(size_t line) const
    {
        const text::Tokenizer *yaml=text::findLanguage("yaml");
        text::TokenState state=yaml->initialState();
        std::vector<text::Token> tokens;

        for(size_t i=0; i <= line; ++i)
        {
            tokens.clear();
            state=yaml->tokenize(editor->getDocument().line(i), state, tokens);
        }

        std::vector<ColorSpan> spans;

        for(const text::Token &token : tokens)
            spans.push_back(ColorSpan{token.begin, token.end,
                skin.Colors.CodeEditor.Tokens[static_cast<size_t>(token.kind)]});
        return spans;
    }

    //! The text id a row's run is drawn under
    static size_t textId(const controls::CodeLine *row)
    {
        std::vector<size_t> primitives, texts;

        row->getTextControl()->getSkinControl()->appendIds(primitives, texts);
        REQUIRE(texts.size() == 1);
        return texts[0];
    }
};

bool sameSpans(const std::vector<ColorSpan> &a, const std::vector<ColorSpan> &b)
{
    return a == b;
}

}//namespace

TEST_CASE("CodeEditor highlighting: each shown row is drawn with its line's token spans")
{
    Scene scene;
    const auto rows=scene.shownRows();

    REQUIRE(rows.size() > 5);
    for(const auto &entry : rows)
    {
        CAPTURE(entry.first);
        CHECK(!entry.second->getColorSpans().empty());
        CHECK(sameSpans(entry.second->getColorSpans(), scene.expectedSpans(entry.first)));
        // drawn with them, under the row's text id
        CHECK(sameSpans(scene.renderer.textSpans[Scene::textId(entry.second)], entry.second->getColorSpans()));
        // the run's own colour, outside the spans, the Plain token colour
        CHECK((entry.second->getTextControl()->getTextColor() ==
            scene.skin.Colors.CodeEditor.Tokens[static_cast<size_t>(text::TokenKind::Plain)]));
    }
}

TEST_CASE("CodeEditor highlighting: an edit that keeps the state re-colours its line alone")
{
    Scene scene;
    const size_t before=scene.editor->getHighlightCache().tokenizedLines();

    // " 42" after line 3's value: a new number token on that line
    scene.editor->setCaret(text::Position{3, scene.editor->getDocument().line(3).size()});
    scene.editor->insertText(" 42");

    CHECK(scene.editor->getHighlightCache().tokenizedLines() == before+1);
    for(const auto &entry : scene.shownRows())
    {
        CAPTURE(entry.first);
        if(entry.first != 3)
            CHECK(entry.second->getTextControl()->getStateChange() == controls::StateChange_Nothing);
    }
    scene.frame();
    CHECK(sameSpans(scene.shownRows().at(3)->getColorSpans(), scene.expectedSpans(3)));
}

TEST_CASE("CodeEditor highlighting: an opened block scalar re-colours the rows on screen below it alone")
{
    // one section: the block opened on its line takes every line after it
    Scene scene(200, 200, 1000);
    const auto rows=scene.shownRows();
    const size_t lastShown=rows.rbegin()->first;
    std::map<size_t, std::vector<ColorSpan>> was;

    REQUIRE(lastShown > 4);
    REQUIRE(lastShown+1 < scene.editor->getLineCount());
    for(const auto &entry : rows)
        was[entry.first]=entry.second->getColorSpans();

    // "section_0: |": a block scalar every line below is in, its comment gone
    const String &line0=scene.editor->getDocument().line(0);

    scene.editor->setCaret(text::Position{0, line0.find(':')+1});
    scene.editor->setCaret(text::Position{0, line0.size()}, true);
    scene.editor->insertText(" |");

    // every row below it on screen has new spans (each line in the block now)
    // and is marked to draw them; the lines past the screen are not tokenized
    for(const auto &entry : scene.shownRows())
    {
        CAPTURE(entry.first);
        if(entry.first == 0)
            continue;
        CHECK(!sameSpans(entry.second->getColorSpans(), was[entry.first]));
        CHECK(sameSpans(entry.second->getColorSpans(), scene.expectedSpans(entry.first)));
        CHECK((entry.second->getTextControl()->getStateChange() & controls::StateChange_Text) != 0);
    }
    CHECK(!scene.editor->getHighlightCache().isFresh(lastShown+1));
    CHECK(scene.editor->getHighlightCache().frontier() <= lastShown+2);
    scene.frame();

    SUBCASE("undo gives every row's spans back")
    {
        REQUIRE(scene.editor->undo());
        scene.frame();
        for(const auto &entry : scene.shownRows())
        {
            CAPTURE(entry.first);
            CHECK(sameSpans(entry.second->getColorSpans(), was[entry.first]));
        }
    }
}

TEST_CASE("CodeEditor highlighting: a new token colour reaches the rows through updateColors()")
{
    Scene scene;
    const Color keyColor(1, 2, 3, 255);

    scene.skin.Colors.CodeEditor.Tokens[static_cast<size_t>(text::TokenKind::Key)]=keyColor;
    for(const auto &entry : scene.shownRows())
        entry.second->updateColors();
    for(const auto &entry : scene.shownRows())
    {
        CAPTURE(entry.first);
        CHECK(sameSpans(entry.second->getColorSpans(), scene.expectedSpans(entry.first)));
    }

    const controls::CodeLine *keyLine=scene.shownRows().at(1);
    bool found=false;

    for(const ColorSpan &span : keyLine->getColorSpans())
        found=found || (span.color == keyColor);
    CHECK(found);
}

TEST_CASE("CodeEditor highlighting: no language, no spans")
{
    Scene scene;

    CHECK(scene.editor->getLanguage() == "yaml");
    CHECK(!scene.editor->setLanguage("no-such-language"));
    CHECK(scene.editor->getTokenizer() == nullptr);
    for(const auto &entry : scene.shownRows())
        CHECK(entry.second->getColorSpans().empty());

    CHECK(scene.editor->setLanguage("json"));
    CHECK(scene.editor->getTokenizer() == text::findLanguage("json"));
    CHECK(scene.editor->setLanguage(""));
    CHECK(scene.editor->getLanguage().empty());
    for(const auto &entry : scene.shownRows())
        CHECK(entry.second->getColorSpans().empty());
}
