/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Base::setExcludedFromStats: a subtree flagged on its root is left out of
 *  the frame counts, so a tool on the canvas it measures (the inspector) shows
 *  the app's work alone. Two canvases hold the same app tree, the first also a
 *  flagged panel of a fixed size (Labels, and an indeterminate ProgressBar,
 *  which animates every frame): the canvas's last child, or the last child of
 *  an app panel, so that its changes dirty the way up to it through an app
 *  control (whose entries, made only for it, must not count either). Each
 *  frame does the same to both app trees and, on the first canvas only,
 *  changes the flagged panel too: new strings, a move, an added Label, an
 *  added and removed child, a deleted one. Every ChangeStats count, the
 *  canvas's LayoutStats (passes, visited, childrenScanned, measures,
 *  renderLayout, requests, capped) and UpdateStats (zVisited, zChanged,
 *  animsRun), the measureTextCalls() delta and the invalidateCalls() delta
 *  (over the frame's edits and the frame) must be the same on both canvases
 *  (the flagged panel is not one of the children its app parent's loops
 *  count, Base::countLayoutChildrenScanned): on the first frame, a settled one, frames that change the flagged panel alone (which
 *  then count nothing at all: no pass, no renumber), and frames that change
 *  both. statsExcluded() is true below the root, false elsewhere and once the
 *  root is unflagged or deleted.
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
    size_t invalidations=0;     //!< the edits' and the frame's
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

    //! nested: the flagged panel is the last child of the last app panel,
    //! not of the canvas, so its changes dirty the way up through that panel
    explicit Scene(bool flagged, bool nested=false)
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

        // last in the tree either way: a z label it takes moves no app
        // control's (the renumber's cascade reaches what follows it)
        controls::Base *under=nested?labels.back()->getParent():canvas;

        excluded=under->newChild<controls::Base>();
        excluded->setExcludedFromStats(true);
        // its size is its own: what it holds does not resize the app around it
        excluded->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        // and inside an app panel it lies within the panel's bounds, so the
        // panel is the size it would be without it
        if(nested)
            excluded->setBounds(0, 0, 300, 60);
        else
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

void checkSame(const char *what, const Counts &a, const Counts &b)
{
    INFO(std::string(what));
    CHECK(a.change.visited == b.change.visited);
    CHECK(a.change.controls == b.change.controls);
    CHECK(a.change.created == b.change.created);
    CHECK(a.change.visibility == b.change.visibility);
    CHECK(a.change.geometry == b.change.geometry);
    CHECK(a.change.background == b.change.background);
    CHECK(a.change.text == b.change.text);
    CHECK(a.layout.passes == b.layout.passes);
    CHECK(a.layout.visited == b.layout.visited);
    CHECK(a.layout.childrenScanned == b.layout.childrenScanned);
    CHECK(a.layout.measureCalls == b.layout.measureCalls);
    CHECK(a.layout.renderLayout == b.layout.renderLayout);
    CHECK(a.layout.requests == b.layout.requests);
    CHECK(a.layout.capped == b.layout.capped);
    CHECK(a.update.zVisited == b.update.zVisited);
    CHECK(a.update.zChanged == b.update.zChanged);
    CHECK(a.update.animsRun == b.update.animsRun);
    CHECK(a.measures == b.measures);
    CHECK(a.invalidations == b.invalidations);
}

//! A frame of both scenes: `app` on each, then `tool` on the flagged one's
//! panel alone. Each scene's changes are made just before its own frame, so
//! neither drain finds the other's controls queued.
void frameBoth(const char *what, Scene &flagged, Scene &plain, const std::function<void(Scene &)> &app,
    const std::function<void(Scene &)> &tool, Counts *out=nullptr)
{
    const size_t flaggedInvalidations=controls::invalidateCalls();

    if(app)
        app(flagged);
    if(tool)
        tool(flagged);

    Counts a=flagged.frame();

    a.invalidations=controls::invalidateCalls()-flaggedInvalidations;

    const size_t plainInvalidations=controls::invalidateCalls();

    if(app)
        app(plain);

    Counts b=plain.frame();

    b.invalidations=controls::invalidateCalls()-plainInvalidations;

    checkSame(what, a, b);
    if(out)
        *out=b;
}

}//namespace

//! The frames, with the flagged panel on the canvas or nested in an app panel.
void runFrames(bool nested)
{
    Scene flagged(true, nested);
    Scene plain(false);
    Counts counts;

    CAPTURE(nested);

    CHECK(flagged.excluded->statsExcluded());
    CHECK(flagged.excludedLabels[0]->statsExcluded());
    CHECK_FALSE(flagged.labels[0]->statsExcluded());
    CHECK_FALSE(flagged.canvas->statsExcluded());

        frameBoth("the first frame", flagged, plain, nullptr, nullptr, &counts);
    CHECK(counts.layout.passes > 0);
    CHECK(counts.change.created > 0);

        frameBoth("settled, but for the flagged ProgressBar's animation", flagged, plain, nullptr, nullptr, &counts);
    CHECK(counts.layout.passes == 0);
    CHECK(counts.change.controls == 0);

        frameBoth("the flagged panel alone: new strings, a move", flagged, plain, nullptr, [](Scene &s)
        {
            s.excludedLabels[0]->setText("a longer string");
            s.excludedLabels[1]->setPos(10, 20);
        }, &counts);
    CHECK(counts.layout.passes == 0);
    CHECK(counts.layout.visited == 0);
    CHECK(counts.change.visited == 0);

        frameBoth("the flagged panel alone: a Label added (a z renumber)", flagged, plain, nullptr, [](Scene &s)
        {
            controls::Label *label=s.excluded->newChild<controls::Label>();

            label->setText("added");
            label->setPos(0, 70);
            s.excludedLabels.push_back(label);
        }, &counts);
    CHECK(counts.update.zVisited == 0);
    CHECK(counts.change.created == 0);

        frameBoth("the app and the flagged panel in the same frame", flagged, plain, [](Scene &s) { s.labels[2]->setText("app text"); },
        [](Scene &s)
        {
            s.excludedLabels[2]->setText("tool text");
            s.excluded->newChild<controls::Label>()->setText("another");
        }, &counts);
    CHECK(counts.layout.passes == 1);
    CHECK(counts.change.text > 0);

        frameBoth("the app adds a Label while the flagged panel deletes one", flagged, plain, [](Scene &s)
        {
            controls::Label *label=s.labels[0]->getParent()->newChild<controls::Label>();

            label->setText("app added");
        }, [](Scene &s)
        {
            delete s.excludedLabels[0];
            s.excludedLabels.erase(s.excludedLabels.begin());
        }, &counts);
    CHECK(counts.update.zVisited > 0);

        frameBoth("the app alone", flagged, plain, [](Scene &s) { s.labels[4]->setPos(5, 5); }, nullptr);

        frameBoth("settled again", flagged, plain, nullptr, nullptr, &counts);
    CHECK(counts.layout.passes == 0);
}

TEST_CASE("Stats exclusion: a flagged subtree counts in no frame count")
{
    SUBCASE("the flagged panel on the canvas")
    {
        runFrames(false);
    }
    SUBCASE("the flagged panel inside an app panel")
    {
        runFrames(true);
    }
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
