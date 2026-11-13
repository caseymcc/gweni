/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  CodeView's diff rows, on a real Canvas with the textured skin over a
 *  StubRenderer that records each primitive slot's last rounded rect or
 *  hide. In the diff language (setLanguage("diff"), setFileName of a *.patch
 *  or *.diff) each shown row's kind is its line's: Added for a '+' line in a
 *  hunk, Removed for a '-' one, Hunk for an "@@" header, None for context,
 *  file headers ("--- ", "+++ ") and the lines before the first hunk; the
 *  row's line-background slot is drawn in the DiffAdded, DiffRemoved and
 *  DiffHunk surfaces and hidden for None, and its run takes the Added and
 *  Removed token colours. Another language, no language, or a CodeEditor
 *  (not a view) in the diff language gives every row None and hides the
 *  slot. In a 1000-line diff, scrolling one row writes no primitive slot
 *  outside the view's rows and bars (not its own field), every row rebound for a new line
 *  draws that line's background, and a settled frame writes nothing.
 */
#include "headlessFixture.h"

#include <gweni/controls/codeView.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace gweni;
namespace tn=skin::textured::tokyonight;
using Kind=controls::CodeLine::DiffKind;

namespace
{

//! Each slot's last rounded rect's fill, and whether its last call hid it.
class SlotRecorder: public test::StubRenderer
{
public:
    using test::StubRenderer::drawRoundedRect;

    void drawRoundedRect(size_t primitiveId, Rect rect, float radius, float border, Color top,
        Color bottom, Color borderColor, int zIndex) override
    {
        fills[primitiveId]=top;
        hidden.erase(primitiveId);
        test::StubRenderer::drawRoundedRect(primitiveId, rect, radius, border, top, bottom,
            borderColor, zIndex);
    }

    void hidePrimitive(size_t primitiveId) override
    {
        hidden.insert(primitiveId);
        test::StubRenderer::hidePrimitive(primitiveId);
    }

    std::map<size_t, Color> fills;
    std::set<size_t> hidden;
};

const char *const g_diff=
    "diff --git a/prompt.md b/prompt.md\n"
    "--- a/prompt.md\n"
    "+++ b/prompt.md\n"
    "@@ -1,3 +1,3 @@ Intro\n"
    " context\n"
    "-old line\n"
    "+new line\n"
    " tail";

const Kind g_kinds[]={Kind::None, Kind::None, Kind::None, Kind::Hunk, Kind::None, Kind::Removed,
    Kind::Added, Kind::None};

//! \p hunks hunks of a context line, a removed and an added one each.
String longDiff(size_t hunks)
{
    String text="--- a/file\n+++ b/file";

    for(size_t i=0; i < hunks; ++i)
    {
        const std::string n=std::to_string(i);

        text+="\n@@ -"+n+",2 +"+n+",2 @@\n context "+n+"\n-removed "+n+"\n+added "+n;
    }
    return text;
}

template<typename View=controls::CodeView>
struct Scene
{
    SlotRecorder renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    View *view=nullptr;

    explicit Scene(const String &text, int height=0)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(600, 600);

        view=canvas->template newChild<View>();
        view->setPos(0, 0);
        if(height > 0)
            view->setSize(300, height);
        view->setText(text);
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

    //! Every shown row by the line it shows.
    std::map<size_t, controls::CodeLine *> shownRows() const
    {
        std::map<size_t, controls::CodeLine *> rows;

        for(controls::Base *row : view->getRows())
        {
            controls::CodeLine *line=gweni_cast<controls::CodeLine>(row);

            if(line && !row->hidden() && line->getLine() != controls::CodeLine::NoLine)
                rows[line->getLine()]=line;
        }
        return rows;
    }

    //! A row's line-background slot (its first).
    static size_t backgroundSlot(const controls::CodeLine *row)
    {
        std::vector<size_t> primitives, texts;

        row->getSkinControl()->appendIds(primitives, texts);
        REQUIRE(!primitives.empty());
        return primitives[0];
    }

    //! The row's background is \p kind's surface, or hidden for None.
    void checkBackground(const controls::CodeLine *row, Kind kind)
    {
        const size_t slot=backgroundSlot(row);
        const tn::Surface *surface=nullptr;

        switch(kind)
        {
        case Kind::Added: surface=&tn::DiffAdded; break;
        case Kind::Removed: surface=&tn::DiffRemoved; break;
        case Kind::Hunk: surface=&tn::DiffHunk; break;
        case Kind::None: break;
        }

        if(!surface)
        {
            CHECK(renderer.hidden.count(slot) == 1);
            return;
        }
        REQUIRE(renderer.hidden.count(slot) == 0);
        REQUIRE(renderer.fills.count(slot) == 1);

        const Color &fill=renderer.fills.at(slot);

        CHECK(fill.r == surface->top.r);
        CHECK(fill.g == surface->top.g);
        CHECK(fill.b == surface->top.b);
        CHECK(fill.a == surface->top.a);
    }
};

bool sameColor(const Color &a, const Color &b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

}//namespace

TEST_CASE("CodeView diff: each row's kind is its line's and its background that kind's surface")
{
    Scene<> scene(g_diff);

    REQUIRE(scene.view->setLanguage("diff"));
    scene.frame();

    const std::map<size_t, controls::CodeLine *> rows=scene.shownRows();

    REQUIRE(rows.size() == 8);
    for(const auto &entry : rows)
    {
        CAPTURE(entry.first);
        CHECK(entry.second->getDiffKind() == g_kinds[entry.first]);
        scene.checkBackground(entry.second, g_kinds[entry.first]);
    }

    // the '+' and '-' text in the token colours
    const auto &tokens=scene.skin.Colors.CodeEditor.Tokens;
    const std::vector<renderer::ColorSpan> &removed=rows.at(5)->getColorSpans();
    const std::vector<renderer::ColorSpan> &added=rows.at(6)->getColorSpans();

    REQUIRE(!removed.empty());
    REQUIRE(!added.empty());
    CHECK(sameColor(removed.front().color, tokens[static_cast<size_t>(text::TokenKind::Removed)]));
    CHECK(sameColor(added.front().color, tokens[static_cast<size_t>(text::TokenKind::Added)]));
}

TEST_CASE("CodeView diff: a *.patch name is the diff, another language or none has no diff rows")
{
    Scene<> scene(g_diff);

    REQUIRE(scene.view->setFileName("changes.patch"));
    scene.frame();
    CHECK(scene.shownRows().at(6)->getDiffKind() == Kind::Added);

    SUBCASE("yaml")
    {
        REQUIRE(scene.view->setLanguage("yaml"));
    }
    SUBCASE("no language")
    {
        REQUIRE(scene.view->setLanguage(""));
    }
    SUBCASE("a name of no language")
    {
        CHECK(!scene.view->setFileName("notes.txt"));
    }
    scene.frame();

    for(const auto &entry : scene.shownRows())
    {
        CAPTURE(entry.first);
        CHECK(entry.second->getDiffKind() == Kind::None);
        scene.checkBackground(entry.second, Kind::None);
    }

    // and back
    REQUIRE(scene.view->setFileName("a/b/changes.diff"));
    scene.frame();
    for(const auto &entry : scene.shownRows())
        scene.checkBackground(entry.second, g_kinds[entry.first]);
}

TEST_CASE("CodeView diff: a CodeEditor in the diff language keeps no diff rows")
{
    Scene<controls::CodeEditor> scene(g_diff, 200);

    REQUIRE(scene.view->setLanguage("diff"));
    scene.frame();
    for(const auto &entry : scene.shownRows())
        CHECK(entry.second->getDiffKind() == Kind::None);
}

TEST_CASE("CodeView diff: a scroll writes inside the rows and bars alone, each rebound row its line's background")
{
    Scene<> scene(longDiff(250), 200);

    REQUIRE(scene.view->setLanguage("diff"));
    scene.frame();
    scene.frame();

    const int lineHeight=scene.view->getLineHeight();

    REQUIRE(lineHeight > 0);
    REQUIRE(scene.view->getMaxVerticalScrollOffset() > 0);

    // a settled frame writes nothing
    scene.frame();
    CHECK(scene.renderer.writtenPrimitiveIds().empty());

    // the slots a scroll may write: the view's subtree's (its rows, the
    // gutter it has none of, the bars), not the view's own field
    std::vector<size_t> subtree, texts;
    std::function<void(controls::Base *)> collect=[&](controls::Base *control)
        {
            if(control->getSkinControl())
                control->getSkinControl()->appendIds(subtree, texts);
            for(controls::Base *child : control->getActualChildren())
                collect(child);
        };

    for(controls::Base *child : scene.view->getActualChildren())
        collect(child);

    for(int step=1; step <= 5; ++step)
    {
        CAPTURE(step);
        scene.view->setVerticalScrollOffset(step*lineHeight);
        scene.frame();

        for(size_t id : scene.renderer.writtenPrimitiveIds())
        {
            CAPTURE(id);
            CHECK(std::find(subtree.begin(), subtree.end(), id) != subtree.end());
        }

        // every shown row draws its line's background: hunk headers every
        // fourth line from line 2, then context, removed, added
        for(const auto &entry : scene.shownRows())
        {
            const size_t line=entry.first;
            Kind kind=Kind::None;

            if(line >= 2)
            {
                const size_t phase=(line-2)%4;

                kind=phase == 0?Kind::Hunk:phase == 2?Kind::Removed:phase == 3?Kind::Added:Kind::None;
            }
            CAPTURE(line);
            CHECK(entry.second->getDiffKind() == kind);
            scene.checkBackground(entry.second, kind);
        }

        scene.frame();
        CHECK(scene.renderer.writtenPrimitiveIds().empty());
    }
}
