/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Base::setExcludedFromStats: a subtree flagged on its root is left out of
 *  the frame counts, so a tool on the canvas it measures (the inspector) shows
 *  the app's work alone. Two canvases hold the same app tree, the first also a
 *  flagged panel (Labels, and an indeterminate ProgressBar, which animates
 *  every frame) as its last child; each frame does the same to both app trees
 *  and, on the first canvas only, changes the flagged panel too: new strings,
 *  a move, an added Label, an added and removed child, a deleted one. Every
 *  ChangeStats count, the canvas's LayoutStats (passes, visited, measures,
 *  renderLayout, requests, capped) and UpdateStats (zVisited, zChanged,
 *  animsRun) and the measureTextCalls() delta must be the same on both
 *  canvases: on the first frame, a settled one, frames that change the flagged
 *  panel alone (which then count nothing at all: no pass, no renumber), and
 *  frames that change both. statsExcluded() is true below the root, false
 *  elsewhere and once the root is unflagged or deleted.
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/controls/label.h>
#include <gweni/controls/progressBar.h>

#include <doctest/doctest.h>

#include <functional>
#include <vector>

using namespace gweni;

namespace
{

//! What one frame counted.
struct Counts
{
    controls::ChangeStats change;
    controls::LayoutStats layout;
    controls::UpdateStats update;
    size_t measures=0;
};

//! A canvas with two panels of three Labels (the app), and with `flagged` one
//! more panel, flagged, of three Labels and an indeterminate ProgressBar.
struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    std::vector<controls::Label *> labels;
    controls::Base *excluded=nullptr;
    std::vector<controls::Label *> excludedLabels;

    explicit Scene(bool flagged)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);

        for(int p=0; p<2; ++p)
        {
            controls::Base *panel=canvas->newChild<controls::Base>();
            panel->setBounds(0, p*60, 300, 60);

            for(int l=0; l<3; ++l)
            {
                controls::Label *label=panel->newChild<controls::Label>();
                label->setText("label");
                label->setPos(l*70, 0);
                labels.push_back(label);
            }
        }

        if(!flagged)
            return;

        excluded=canvas->newChild<controls::Base>();
        excluded->setExcludedFromStats(true);
        excluded->setBounds(0, 150, 300, 100);
        for(int l=0; l<3; ++l)
        {
            controls::Label *label=excluded->newChild<controls::Label>();
            label->setText("tool");
            label->setPos(l*70, 0);
            excludedLabels.push_back(label);
        }

        controls::ProgressBar *bar=excluded->newChild<controls::ProgressBar>();
        bar->setBounds(0, 40, 200, 12);
        bar->setIndeterminate(true);
    }

    ~Scene()
    {
        delete canvas;
    }

    Counts frame()
    {
        Counts counts;
        const size_t measures=controls::measureTextCalls();

        controls::resetChangeStats();
        canvas->updateCanvas();
        canvas->renderCanvas();
        counts.change=controls::getChangeStats();
        counts.layout=canvas->getLayoutStats();
        counts.update=canvas->getUpdateStats();
        counts.measures=controls::measureTextCalls()-measures;
        return counts;
    }
};

void checkSame(const Counts &a, const Counts &b)
{
    CHECK(a.change.visited == b.change.visited);
    CHECK(a.change.controls == b.change.controls);
    CHECK(a.change.created == b.change.created);
    CHECK(a.change.visibility == b.change.visibility);
    CHECK(a.change.geometry == b.change.geometry);
    CHECK(a.change.background == b.change.background);
    CHECK(a.change.text == b.change.text);
    CHECK(a.layout.passes == b.layout.passes);
    CHECK(a.layout.visited == b.layout.visited);
    CHECK(a.layout.measureCalls == b.layout.measureCalls);
    CHECK(a.layout.renderLayout == b.layout.renderLayout);
    CHECK(a.layout.requests == b.layout.requests);
    CHECK(a.layout.capped == b.layout.capped);
    CHECK(a.update.zVisited == b.update.zVisited);
    CHECK(a.update.zChanged == b.update.zChanged);
    CHECK(a.update.animsRun == b.update.animsRun);
    CHECK(a.measures == b.measures);
}

//! A frame of both scenes: `app` on each, then `tool` on the flagged one's
//! panel alone. Each scene's changes are made just before its own frame, so
//! neither drain finds the other's controls queued.
void frameBoth(Scene &flagged, Scene &plain, const std::function<void(Scene &)> &app,
    const std::function<void(Scene &)> &tool, Counts *out=nullptr)
{
    if(app)
        app(flagged);
    if(tool)
        tool(flagged);

    const Counts a=flagged.frame();

    if(app)
        app(plain);

    const Counts b=plain.frame();

    checkSame(a, b);
    if(out)
        *out=b;
}

}//namespace

TEST_CASE("Stats exclusion: a flagged subtree counts in no frame count")
{
    Scene flagged(true);
    Scene plain(false);
    Counts counts;

    CHECK(flagged.excluded->statsExcluded());
    CHECK(flagged.excludedLabels[0]->statsExcluded());
    CHECK_FALSE(flagged.labels[0]->statsExcluded());
    CHECK_FALSE(flagged.canvas->statsExcluded());

    INFO("the first frame");
    frameBoth(flagged, plain, nullptr, nullptr, &counts);
    CHECK(counts.layout.passes > 0);
    CHECK(counts.change.created > 0);

    INFO("settled, but for the flagged ProgressBar's animation");
    frameBoth(flagged, plain, nullptr, nullptr, &counts);
    CHECK(counts.layout.passes == 0);
    CHECK(counts.change.controls == 0);

    INFO("the flagged panel alone: new strings, a move");
    frameBoth(flagged, plain, nullptr, [](Scene &s)
        {
            s.excludedLabels[0]->setText("a longer string");
            s.excludedLabels[1]->setPos(10, 20);
        }, &counts);
    CHECK(counts.layout.passes == 0);
    CHECK(counts.layout.visited == 0);
    CHECK(counts.change.visited == 0);

    INFO("the flagged panel alone: a Label added (a z renumber)");
    frameBoth(flagged, plain, nullptr, [](Scene &s)
        {
            controls::Label *label=s.excluded->newChild<controls::Label>();

            label->setText("added");
            label->setPos(0, 70);
            s.excludedLabels.push_back(label);
        }, &counts);
    CHECK(counts.update.zVisited == 0);
    CHECK(counts.change.created == 0);

    INFO("the app and the flagged panel in the same frame");
    frameBoth(flagged, plain, [](Scene &s) { s.labels[2]->setText("app text"); },
        [](Scene &s)
        {
            s.excludedLabels[2]->setText("tool text");
            s.excluded->newChild<controls::Label>()->setText("another");
        }, &counts);
    CHECK(counts.layout.passes == 1);
    CHECK(counts.change.text > 0);

    INFO("the app adds a Label while the flagged panel deletes one");
    frameBoth(flagged, plain, [](Scene &s)
        {
            controls::Label *label=s.labels[0]->getParent()->newChild<controls::Label>();

            label->setText("app added");
        }, [](Scene &s)
        {
            delete s.excludedLabels[0];
            s.excludedLabels.erase(s.excludedLabels.begin());
        }, &counts);
    CHECK(counts.update.zVisited > 0);

    INFO("the app alone");
    frameBoth(flagged, plain, [](Scene &s) { s.labels[4]->setPos(5, 5); }, nullptr);

    INFO("settled again");
    frameBoth(flagged, plain, nullptr, nullptr, &counts);
    CHECK(counts.layout.passes == 0);
}

TEST_CASE("Stats exclusion: statsExcluded() follows the flag and the root's life")
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=new controls::Canvas(&skin);
    controls::Base *root=canvas->newChild<controls::Base>();
    controls::Base *child=root->newChild<controls::Base>();
    controls::Base *other=canvas->newChild<controls::Base>();

    CHECK_FALSE(child->statsExcluded());
    root->setExcludedFromStats(true);
    CHECK(root->isExcludedFromStats());
    CHECK(child->statsExcluded());
    CHECK_FALSE(other->statsExcluded());

    // reparented out of the subtree: counted again
    child->setParent(other);
    CHECK_FALSE(child->statsExcluded());
    child->setParent(root);
    CHECK(child->statsExcluded());

    root->setExcludedFromStats(false);
    CHECK_FALSE(child->statsExcluded());

    // a flagged root deleted takes its flag with it
    root->setExcludedFromStats(true);
    child->setParent(other);
    delete root;
    CHECK_FALSE(child->statsExcluded());
    CHECK_FALSE(other->statsExcluded());

    delete canvas;
}
