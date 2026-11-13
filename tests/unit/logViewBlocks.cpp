/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  LogView's row kinds on a real Canvas with the StubSkin, layout
 *  verification on. A block row shows "title (4 of 120 lines)" and its
 *  first lines while collapsed and "title (120 lines)" and every line once
 *  expanded (a block of no more lines than the preview shows them all), with
 *  a chevron beside its title line, right while collapsed and down while
 *  expanded, in a left padding a plain row does not have. A press on its
 *  title line toggles it and one below the title does not; a toggle changes
 *  that row's height alone: the rows above it stay where they are and the
 *  rows below move by its change in height. A row of an app's kind (the
 *  row-kind hook) shows the kind's content, made once per pooled row and
 *  bound with the row, inside the row's padding at the kind's measure, the
 *  text hidden; given a plain row again, the row hides the content and shows
 *  the text. A kind with no handler shows its text, and a kind registered
 *  again makes its content with the new factory, the old content deleted. A
 *  new preview length measures nothing itself and the rows on screen alone
 *  in the next layout, whatever the log's length. Every row lays out at its
 *  measure, and every frame is one layout pass.
 */
#include "headlessFixture.h"

#include <gweni/controls/logView.h>
#include <gweni/controls/text.h>
#include <gweni/inputHandler.h>

#include <doctest/doctest.h>

#include <map>
#include <string>
#include <vector>

using namespace gweni;

namespace
{

using RowId=controls::LogView::RowId;

std::vector<String> blockLines(size_t count)
{
    std::vector<String> lines;

    for(size_t i=0; i < count; ++i)
        lines.push_back("line "+std::to_string(i));
    return lines;
}

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::LogView *log=nullptr;
    bool verify=false;
    size_t verifyFailures=0;

    explicit Scene(int height=300)
    {
        verify=controls::layoutVerifyEnabled();
        controls::setLayoutVerify(true);
        verifyFailures=controls::layoutVerifyFailures();

        canvas=new controls::Canvas(&skin);
        canvas->setSize(500, 700);

        log=canvas->newChild<controls::LogView>();
        log->setPos(0, 0);
        log->setSize(300, height);
        log->setRemeasureBudget(0);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        controls::setLayoutVerify(verify);
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();

        const controls::LayoutStats &layout=canvas->getLayoutStats();

        CHECK(layout.requests == 0);
        CHECK(layout.passes <= 1);
        CHECK(!layout.renderLayout);
        CHECK(controls::layoutVerifyFailures() == verifyFailures);
    }

    //! The canvas point at (x, y) in the row's coordinates.
    Point rowPoint(RowId id, int x, int y) const
    {
        controls::Base *row=log->rowControl(id);

        REQUIRE(row != nullptr);
        return row->localPosToCanvas(Point(x, y));
    }

    void press(const Point &at)
    {
        log->onMouseClickLeft(at.x, at.y, true);
        log->onMouseClickLeft(at.x, at.y, false);
    }

    //! Every row on screen, by id: its top and height.
    std::map<RowId, std::pair<int, int>> shown() const
    {
        std::map<RowId, std::pair<int, int>> rows;

        for(size_t i=0; i < log->getRowCount(); ++i)
        {
            if(controls::Base *row=log->rowForItem(i))
                rows[log->rowIdAt(i)]={row->getBounds().y, row->getBounds().h};
        }
        return rows;
    }
};

}//namespace

TEST_CASE("LogView blocks: the collapsed text shows the first lines and the count")
{
    controls::LogRow row=controls::LogRow::block("Bash ls", blockLines(120));

    CHECK(controls::LogView::blockText(row, 4) == "Bash ls (4 of 120 lines)\nline 0\nline 1\nline 2\nline 3");
    row.collapsed=false;

    const String expanded=controls::LogView::blockText(row, 4);

    CHECK(expanded.rfind("Bash ls (120 lines)\nline 0\nline 1\n", 0) == 0);
    CHECK(expanded.size() > 120*6);
    CHECK(expanded.substr(expanded.size()-9) == "\nline 119");

    // no more lines than the preview: all of them, collapsed or not
    controls::LogRow small=controls::LogRow::block("Read", blockLines(3));

    CHECK(controls::LogView::blockText(small, 4) == "Read (3 lines)\nline 0\nline 1\nline 2");
    small.lines.resize(1);
    CHECK(controls::LogView::blockText(small, 4) == "Read (1 line)\nline 0");
    small.lines.clear();
    CHECK(controls::LogView::blockText(small, 4) == "Read");
}

TEST_CASE("LogView blocks: a block row has a chevron in its padding and a plain row has none")
{
    Scene scene;
    const RowId plain=scene.log->append(controls::LogRow("plain text"));
    const RowId block=scene.log->append(controls::LogRow::block("Bash ls", blockLines(120)));
    const RowId after=scene.log->append(controls::LogRow("after the block"));

    scene.frame();

    controls::internal::LogViewRow *blockRow=scene.log->rowControl(block);

    REQUIRE(blockRow != nullptr);
    CHECK(blockRow->getText() == controls::LogView::blockText(*scene.log->getRow(block), 4));
    REQUIRE(blockRow->getChevron() != nullptr);
    CHECK(!blockRow->getChevron()->hidden());
    CHECK(!blockRow->getChevron()->isExpanded());
    CHECK(blockRow->getChevron()->getBounds().x == controls::LogView::RowPadX);
    CHECK(blockRow->getChevron()->getBounds().y == controls::LogView::RowPadY);
    CHECK(blockRow->getChevron()->getBounds().h == scene.log->getLineHeight());
    CHECK(blockRow->getPadding().left == controls::LogView::RowPadX+controls::LogView::ChevronColumn);

    for(RowId id : {plain, after})
    {
        controls::internal::LogViewRow *row=scene.log->rowControl(id);

        REQUIRE(row != nullptr);
        CHECK(row->getPadding().left == controls::LogView::RowPadX);
        CHECK((!row->getChevron() || row->getChevron()->hidden()));
    }

    // 5 lines of text at the block's height: the measure is what it lays out at
    CHECK(blockRow->getBounds().h == 5*scene.log->getLineHeight()+2*controls::LogView::RowPadY);
}

TEST_CASE("LogView blocks: a press on the title line toggles the block and changes only its height")
{
    Scene scene(240);
    std::vector<RowId> ids;

    for(int i=0; i < 60; ++i)
    {
        if(i%10 == 5)
            ids.push_back(scene.log->append(controls::LogRow::block("Block "+std::to_string(i), blockLines(30))));
        else
            ids.push_back(scene.log->append(controls::LogRow("row "+std::to_string(i))));
    }
    scene.frame();

    // the view a little past the top, block 25 on screen below a few rows
    CHECK(scene.log->scrollToRow(ids[22]));
    scene.frame();
    scene.log->setVerticalScrollOffset(scene.log->getVerticalScrollOffset()+3);
    scene.frame();
    scene.frame();

    const RowId block=ids[25];
    const auto before=scene.shown();

    REQUIRE(before.count(block) == 1);
    REQUIRE(before.begin()->first < block);

    // a press below the title line is not a toggle
    scene.press(scene.rowPoint(block, 40, controls::LogView::RowPadY+scene.log->getLineHeight()+2));
    scene.frame();
    CHECK(scene.log->getRow(block)->collapsed);
    CHECK(scene.shown() == before);

    // on the title, the chevron's column included: expanded, taller by 26 lines
    scene.press(scene.rowPoint(block, controls::LogView::RowPadX+3, controls::LogView::RowPadY+2));
    scene.frame();
    CHECK_FALSE(scene.log->getRow(block)->collapsed);
    CHECK(scene.log->rowControl(block)->getChevron()->isExpanded());

    const auto expanded=scene.shown();
    const int grown=26*scene.log->getLineHeight();

    REQUIRE(expanded.count(block) == 1);
    CHECK(expanded.at(block).first == before.at(block).first);
    CHECK(expanded.at(block).second == before.at(block).second+grown);
    for(const auto &row : before)
    {
        CAPTURE(row.first);
        if(row.first < block)
            CHECK(expanded.at(row.first) == row.second);
        else if(row.first > block && expanded.count(row.first))
            CHECK(expanded.at(row.first).first == row.second.first+grown);
    }

    // the title line's text toggles it back: the frame before
    scene.press(scene.rowPoint(block, 60, controls::LogView::RowPadY+1));
    scene.frame();
    CHECK(scene.log->getRow(block)->collapsed);
    CHECK(scene.shown() == before);

    // a plain row's press toggles nothing
    scene.press(scene.rowPoint(ids[24], 20, 2));
    scene.frame();
    CHECK(scene.shown() == before);

    CHECK(scene.log->setBlockExpanded(block, true));
    CHECK_FALSE(scene.log->setBlockExpanded(ids[24], true));
    CHECK_FALSE(scene.log->toggleBlock(controls::LogView::NoRow));
}

TEST_CASE("LogView kinds: a row of an app kind shows the kind's content at its measure")
{
    Scene scene;
    constexpr controls::LogRow::Kind Card=controls::LogRow::FirstAppKind;
    std::map<controls::Base *, String> bound;
    int made=0;

    controls::LogView::RowKind card;

    card.factory=[&made](controls::Base *row) -> controls::Base *
        {
            ++made;
            return row->newChild<controls::Base>();
        };
    card.binder=[&bound](controls::Base *content, const controls::LogRow &row) { bound[content]=row.text; };
    card.measurer=[](const controls::LogRow &row, int /*width*/) { return 20+static_cast<int>(row.text.size()); };
    scene.log->setRowKind(Card, card);

    controls::LogRow cardRow("card one");

    cardRow.kind=Card;

    const RowId first=scene.log->append(controls::LogRow("plain"));
    const RowId cardId=scene.log->append(cardRow);
    controls::LogRow unknown("no handler for this kind");

    unknown.kind=Card+1;

    const RowId unknownId=scene.log->append(unknown);

    scene.frame();

    controls::internal::LogViewRow *row=scene.log->rowControl(cardId);

    REQUIRE(row != nullptr);
    CHECK(made == 1);
    REQUIRE(row->getKindContent() != nullptr);
    CHECK(bound.at(row->getKindContent()) == "card one");
    CHECK(row->getBounds().h == 28+2*controls::LogView::RowPadY);
    const Rect contentBounds=row->getKindContent()->getBounds();

    CHECK(contentBounds.x == controls::LogView::RowPadX);
    CHECK(contentBounds.y == controls::LogView::RowPadY);
    CHECK(contentBounds.w == row->getWidth()-2*controls::LogView::RowPadX);
    CHECK(contentBounds.h == 28);
    CHECK(!row->getKindContent()->hidden());
    CHECK(row->getText().empty());

    CHECK(scene.log->rowControl(first)->getKindContent() == nullptr);
    CHECK(scene.log->rowControl(unknownId)->getText() == "no handler for this kind");
    CHECK(scene.log->rowControl(unknownId)->getKindContent() == nullptr);

    // a plain row again: the content hides and the text shows
    controls::Base *content=row->getKindContent();

    CHECK(scene.log->replace(cardId, controls::LogRow("plain now")));
    scene.frame();
    row=scene.log->rowControl(cardId);
    REQUIRE(row != nullptr);
    CHECK(row->getKindContent() == nullptr);
    CHECK(content->hidden());
    CHECK(row->getText() == "plain now");

    // and the kind once more: the same content, not a second one
    cardRow.text="card again";
    CHECK(scene.log->replace(cardId, cardRow));
    scene.frame();
    row=scene.log->rowControl(cardId);
    REQUIRE(row != nullptr);
    CHECK(row->getKindContent() == content);
    CHECK(!content->hidden());
    CHECK(bound.at(content) == "card again");
    CHECK(made == 1);
    CHECK(row->getBounds().h == 30+2*controls::LogView::RowPadY);
}

// A new preview length is one measure generation: the call measures nothing,
// and the next layout measures the rows on screen alone (the idle re-measure,
// off here, would take the rest), whatever the log's length.
TEST_CASE("LogView blocks: a new preview length measures only the rows on screen")
{
    Scene scene;

    for(int i=0; i < 2000; ++i)
        scene.log->append(controls::LogRow::block("block "+std::to_string(i), blockLines(8)));

    // the first layout measures the rows on screen alone too: the yardstick
    const size_t first=controls::measureTextCalls();

    scene.frame();

    const size_t firstFrameCalls=controls::measureTextCalls()-first;
    const size_t before=controls::measureTextCalls();

    scene.log->setBlockPreviewLines(2);
    CHECK(controls::measureTextCalls() == before);
    scene.frame();

    const size_t frameCalls=controls::measureTextCalls()-before;

    CAPTURE(frameCalls);
    CAPTURE(firstFrameCalls);
    CHECK(frameCalls > 0);
    CHECK(frameCalls <= 2*firstFrameCalls);

    const RowId last=scene.log->lastRowId();
    controls::internal::LogViewRow *row=scene.log->rowControl(last);

    REQUIRE(row != nullptr);
    CHECK(row->getText() == controls::LogView::blockText(
        controls::LogRow::block("block 1999", blockLines(8)), 2));
}

// A kind registered again takes the new handler in full: the rows' content
// of the old factory goes, and the new factory makes the content the new
// binder fills.
TEST_CASE("LogView kinds: a kind registered again makes its content with the new factory")
{
    Scene scene;
    constexpr controls::LogRow::Kind Card=controls::LogRow::FirstAppKind;
    int madeOld=0;
    int madeNew=0;
    std::map<controls::Base *, String> boundNew;

    controls::LogView::RowKind oldKind;

    oldKind.factory=[&madeOld](controls::Base *row) -> controls::Base * { ++madeOld; return row->newChild<controls::Base>(); };
    oldKind.binder=[](controls::Base *, const controls::LogRow &) {};
    oldKind.measurer=[](const controls::LogRow &, int) { return 20; };
    scene.log->setRowKind(Card, oldKind);

    controls::LogRow cardRow("card");

    cardRow.kind=Card;

    const RowId cardId=scene.log->append(cardRow);

    scene.frame();

    controls::internal::LogViewRow *row=scene.log->rowControl(cardId);

    REQUIRE(row != nullptr);
    REQUIRE(madeOld == 1);

    const controls::Base *oldContent=row->getKindContent();

    REQUIRE(oldContent != nullptr);

    const size_t children=row->getChildren().size();

    controls::LogView::RowKind newKind;

    newKind.factory=[&madeNew](controls::Base *row) -> controls::Base * { ++madeNew; return row->newChild<controls::Base>(); };
    newKind.binder=[&boundNew](controls::Base *content, const controls::LogRow &row) { boundNew[content]=row.text; };
    newKind.measurer=[](const controls::LogRow &, int) { return 30; };
    scene.log->setRowKind(Card, newKind);
    scene.frame();

    row=scene.log->rowControl(cardId);
    REQUIRE(row != nullptr);
    CHECK(madeNew == 1);
    REQUIRE(row->getKindContent() != nullptr);
    CHECK(boundNew.count(row->getKindContent()) == 1);
    CHECK(row->getBounds().h == 30+2*controls::LogView::RowPadY);

    // the old content is gone (the new one may take its address): the row
    // holds as many children as before, not one more
    CHECK(row->getChildren().size() == children);
}
