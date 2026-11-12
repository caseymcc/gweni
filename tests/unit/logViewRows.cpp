/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  LogView's row store on a real Canvas with the StubSkin: append() numbers
 *  the rows up from 0, prepend() below every id given so far (never an id
 *  the cap dropped), and through appends, the cap (dropping the oldest past
 *  it on the next append, not when it is set), prepends and clear() every id
 *  ever given finds its row's position, or none once it is gone, exactly as
 *  a plain model of the rows says, with the list holding one item per row.
 *  replace() of a dropped, cleared or never given id returns false and
 *  changes nothing; of a held one it re-binds the row on screen. The rows on
 *  screen show their rows' text, and a prepend or a cap's drop above them
 *  leaves them where they were.
 */
#include "headlessFixture.h"

#include <gweni/controls/logView.h>
#include <gweni/inputHandler.h>

#include <doctest/doctest.h>

#include <random>
#include <string>
#include <vector>

using namespace gweni;

namespace
{

using RowId=controls::LogView::RowId;

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::LogView *log=nullptr;

    //! The rows as the app expects them, oldest first.
    struct Model
    {
        RowId id;
        String text;
    };
    std::vector<Model> model;

    explicit Scene(int height=200)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 600);

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
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    static String text(RowId id)
    {
        return "row "+std::to_string(id);
    }

    //! The id the next append() gives.
    RowId nextId=0;

    RowId append()
    {
        const RowId id=log->append(controls::LogRow(text(nextId)));

        CHECK(id == nextId);
        ++nextId;

        // the cap drops what is past it before the new row
        const size_t cap=log->getMaxRows();

        if(cap != 0 && model.size() >= cap)
            model.erase(model.begin(), model.begin()+static_cast<std::ptrdiff_t>(model.size()-(cap-1)));
        model.push_back(Model{id, text(id)});
        return id;
    }

    RowId prepend(size_t count)
    {
        std::vector<controls::LogRow> rows;

        for(size_t i=0; i < count; ++i)
            rows.emplace_back("older "+std::to_string(i));

        const RowId first=log->prepend(rows);

        for(size_t i=count; i-- > 0;)
            model.insert(model.begin(), Model{first+static_cast<RowId>(i), rows[i].text});
        return first;
    }

    //! Every id from below the lowest to past the highest given finds what
    //! the model says, and the list holds one item per row.
    void checkMap(RowId lowest, RowId highest) const
    {
        REQUIRE(log->getRowCount() == model.size());
        CHECK(log->getItemCount() == model.size());
        CHECK(log->firstRowId() == (model.empty()?controls::LogView::NoRow:model.front().id));
        CHECK(log->lastRowId() == (model.empty()?controls::LogView::NoRow:model.back().id));

        for(size_t i=0; i < model.size(); ++i)
        {
            CHECK(log->rowIdAt(i) == model[i].id);
            REQUIRE(log->getRow(model[i].id) != nullptr);
            CHECK(log->getRow(model[i].id)->text == model[i].text);
        }
        CHECK(log->rowIdAt(model.size()) == controls::LogView::NoRow);

        for(RowId id=lowest-2; id <= highest+2; ++id)
        {
            size_t expected=controls::VirtualList::NoItem;

            for(size_t i=0; i < model.size(); ++i)
            {
                if(model[i].id == id)
                    expected=i;
            }
            CAPTURE(id);
            CHECK(log->rowPosition(id) == expected);
            CHECK(log->hasRow(id) == (expected != controls::VirtualList::NoItem));
        }
    }

    //! The pooled rows that show and the row ids they show.
    std::vector<std::pair<RowId, int>> shownRows() const
    {
        std::vector<std::pair<RowId, int>> shown;

        for(size_t i=0; i < log->getRowCount(); ++i)
        {
            if(controls::Base *row=log->rowForItem(i))
                shown.emplace_back(log->rowIdAt(i), row->getBounds().y);
        }
        return shown;
    }
};

}//namespace

TEST_CASE("LogView: appended ids run up from 0 and each finds its row")
{
    Scene scene;

    for(int i=0; i < 50; ++i)
        CHECK(scene.append() == i);
    scene.checkMap(0, 49);

    scene.frame();

    // the rows on screen show their rows' text
    const auto shown=scene.shownRows();

    REQUIRE(!shown.empty());
    for(const auto &row : shown)
    {
        controls::internal::LogViewRow *control=scene.log->rowControl(row.first);

        REQUIRE(control != nullptr);
        CHECK(control->getText() == Scene::text(row.first));
    }
}

TEST_CASE("LogView: the cap drops the oldest rows on the next append and their ids are gone")
{
    Scene scene;

    for(int i=0; i < 10; ++i)
        scene.append();

    // setting the cap drops nothing yet
    scene.log->setMaxRows(4);
    CHECK(scene.log->getRowCount() == 10);
    CHECK(scene.log->getItemCount() == 10);

    scene.append();
    CHECK(scene.log->getRowCount() == 4);
    CHECK(scene.log->firstRowId() == 7);
    scene.checkMap(0, 10);

    for(int i=0; i < 20; ++i)
        scene.append();
    CHECK(scene.log->getRowCount() == 4);
    CHECK(scene.log->firstRowId() == 27);
    scene.checkMap(0, 30);

    // a dropped id: false, and nothing changes
    const String before=scene.log->getRow(27)->text;

    CHECK_FALSE(scene.log->replace(26, controls::LogRow("replaced")));
    CHECK_FALSE(scene.log->replace(3, controls::LogRow("replaced")));
    CHECK_FALSE(scene.log->replace(31, controls::LogRow("never given")));
    CHECK(scene.log->getRow(27)->text == before);
    scene.checkMap(0, 30);

    // a cap of 0 keeps every row again
    scene.log->setMaxRows(0);
    for(int i=0; i < 10; ++i)
        scene.append();
    CHECK(scene.log->getRowCount() == 14);
    scene.checkMap(0, 40);
}

TEST_CASE("LogView: prepend numbers below every id given and never reuses one the cap dropped")
{
    Scene scene;

    for(int i=0; i < 3; ++i)
        scene.append();
    CHECK(scene.prepend(2) == -2);
    scene.checkMap(-2, 2);
    CHECK(scene.log->getRow(-2)->text == "older 0");
    CHECK(scene.log->getRow(-1)->text == "older 1");

    // the cap drops the oldest, the prepended rows first
    scene.log->setMaxRows(4);
    scene.append();
    CHECK(scene.log->firstRowId() == 0);
    scene.checkMap(-2, 3);

    // the next prepend goes below -2, which was given and dropped
    scene.log->setMaxRows(0);
    CHECK(scene.prepend(2) == -4);
    scene.checkMap(-4, 3);
    CHECK_FALSE(scene.log->hasRow(-2));
    CHECK_FALSE(scene.log->replace(-2, controls::LogRow("dropped")));

    // a run of prepended ids with its oldest dropped, then another prepend:
    // -8, -6..-3, 0..4, an id in the middle run is neither its distance from
    // the last row nor from the first
    CHECK(scene.prepend(3) == -7);
    scene.log->setMaxRows(9);
    scene.append();
    CHECK(scene.log->firstRowId() == -6);
    scene.log->setMaxRows(0);
    CHECK(scene.prepend(1) == -8);
    scene.checkMap(-8, 4);
    CHECK(scene.log->rowPosition(-4) == 3);

    CHECK(scene.log->prepend({}) == controls::LogView::NoRow);
    scene.checkMap(-4, 3);
}

TEST_CASE("LogView: clear removes every row and the ids go on")
{
    Scene scene;

    for(int i=0; i < 5; ++i)
        scene.append();
    scene.prepend(1);
    scene.frame();

    scene.log->clear();
    scene.model.clear();
    scene.checkMap(-1, 4);
    CHECK_FALSE(scene.log->replace(2, controls::LogRow("cleared")));
    CHECK(scene.log->getItemCount() == 0);

    CHECK(scene.append() == 5);
    CHECK(scene.prepend(1) == -2);
    scene.checkMap(-2, 5);
    scene.frame();
    CHECK(scene.log->rowControl(5) != nullptr);
}

TEST_CASE("LogView: random appends caps prepends and clears keep every id's position")
{
    Scene scene;
    std::mt19937 random(594);
    RowId lowest=0;

    for(int step=0; step < 400; ++step)
    {
        const unsigned op=random()%20;

        if(op < 12)
            scene.append();
        else if(op < 15)
            lowest=std::min(lowest, scene.prepend(1+random()%4));
        else if(op < 18)
            scene.log->setMaxRows((random()%3 == 0)?0:1+random()%12);
        else if(op < 19)
        {
            if(!scene.model.empty())
            {
                const auto &row=scene.model[random()%scene.model.size()];
                const String text="replaced "+std::to_string(step);

                CHECK(scene.log->replace(row.id, controls::LogRow(text)));
                for(auto &model : scene.model)
                {
                    if(model.id == row.id)
                        model.text=text;
                }
            }
        }
        else
        {
            scene.log->clear();
            scene.model.clear();
        }
        if(step%25 == 0)
            scene.frame();

        const RowId highest=(scene.log->lastRowId() == controls::LogView::NoRow)?0:scene.log->lastRowId();

        CAPTURE(step);
        scene.checkMap(lowest, std::max<RowId>(highest, 0));
    }
}

TEST_CASE("LogView: replace re-binds the row on screen and edits above leave the screen in place")
{
    Scene scene;

    for(int i=0; i < 100; ++i)
        scene.append();
    scene.frame();

    // the middle of the log at the top of the viewport
    scene.log->scrollToItem(60);
    scene.frame();
    scene.frame();

    const auto before=scene.shownRows();

    REQUIRE(before.size() > 2);

    // a row on screen gets its new text
    const RowId onScreen=before[1].first;

    CHECK(scene.log->replace(onScreen, controls::LogRow("a new text")));
    scene.frame();
    CHECK(scene.log->rowControl(onScreen)->getText() == "a new text");
    CHECK(scene.shownRows() == before);

    // a row above, older rows prepended and the cap's drop move nothing
    CHECK(scene.log->replace(5, controls::LogRow("above the view, now three times as long as it was before, "
        "so that it wraps onto more lines than it did at the view's width")));
    scene.frame();
    CHECK(scene.shownRows() == before);

    scene.prepend(7);
    scene.frame();
    CHECK(scene.shownRows() == before);

    scene.log->setMaxRows(90);
    scene.append();
    scene.frame();

    auto after=scene.shownRows();

    // the new last row is below the view; the rows on screen are the same
    CHECK(after == before);
}
