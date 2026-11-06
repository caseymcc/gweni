/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Re-skinning a control tree (controls::Base::setSkin with a different skin)
 *  must give the old skin::Controls' primitive and text ids back to the id
 *  banks before the new skin's Controls take theirs. Dropping the old Control
 *  without remove() orphaned its ids, and the VBO slots behind them kept
 *  drawing; every swap grew the banks by the whole tree.
 */
#include "headlessFixture.h"

#include <doctest/doctest.h>

#include <algorithm>

using namespace gweni;

namespace
{

//! A canvas with a few nested children; three of them take a text id.
struct Tree
{
    explicit Tree(skin::Base *skin)
    {
        canvas=new controls::Canvas(skin);

        controls::Base *panel=canvas->newChild<controls::Base>("panel");
        panel->newChild<test::StubText>("text1");

        controls::Base *inner=panel->newChild<controls::Base>("inner");
        inner->newChild<test::StubText>("text2");
        inner->newChild<controls::Base>("leaf");

        canvas->newChild<test::StubText>("text3");
    }

    ~Tree()
    {
        delete canvas;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    //! Controls below the canvas. Canvas::render never generates, so the
    //! canvas's own Control takes no ids.
    static constexpr size_t controlCount=6;
    static constexpr size_t textCount=3;
    static constexpr size_t primitiveCount=controlCount*test::StubControl::primitivesPerControl;

    controls::Canvas *canvas;
};

size_t count(const std::vector<test::StubRenderer::Event> &events, test::StubRenderer::Op op)
{
    return std::count_if(events.begin(), events.end(),
        [op](const test::StubRenderer::Event &event){ return event.op == op; });
}

void checkNoDuplicateIds(const test::StubRenderer &renderer)
{
    CHECK(renderer.doubleAcquired == 0);
    CHECK(renderer.doubleReleased == 0);
    CHECK_FALSE(test::hasDuplicates(releasedPrimitiveIds()));
    CHECK_FALSE(test::hasDuplicates(releasedTextIds()));
}

}//namespace

TEST_CASE("the first setSkin acquires the tree's ids on the next render and setSkin(sameSkin) is a no-op")
{
    test::TwoSkins skins;
    const test::IdBankCounts baseline=test::IdBankCounts::now();

    Tree tree(&skins.skinA);

    // setSkin only creates the Controls; ids come with the first render
    CHECK(skins.renderer.events.empty());
    CHECK(test::IdBankCounts::now().livePrimitives() == baseline.livePrimitives());

    tree.frame();

    test::IdBankCounts afterFirst=test::IdBankCounts::now();
    CHECK(afterFirst.livePrimitives()-baseline.livePrimitives() == Tree::primitiveCount);
    CHECK(afterFirst.liveTexts()-baseline.liveTexts() == Tree::textCount);
    CHECK(count(skins.renderer.events, test::StubRenderer::Op::AcquirePrimitive) == Tree::primitiveCount);
    CHECK(count(skins.renderer.events, test::StubRenderer::Op::AcquireText) == Tree::textCount);
    CHECK(count(skins.renderer.events, test::StubRenderer::Op::ReleasePrimitive) == 0);
    CHECK(count(skins.renderer.events, test::StubRenderer::Op::ReleaseText) == 0);

    const size_t controlsA=skins.skinA.liveControls;
    skins.renderer.events.clear();

    tree.canvas->setSkin(&skins.skinA);
    tree.frame();

    test::IdBankCounts afterSame=test::IdBankCounts::now();
    CHECK(skins.renderer.events.empty());
    CHECK(skins.skinA.liveControls == controlsA);
    CHECK(afterSame.primitives == afterFirst.primitives);
    CHECK(afterSame.releasedPrimitives == afterFirst.releasedPrimitives);
    CHECK(afterSame.texts == afterFirst.texts);
    CHECK(afterSame.releasedTexts == afterFirst.releasedTexts);
    checkNoDuplicateIds(skins.renderer);
}

TEST_CASE("setSkin releases every id the old skin's Controls held before the new skin's acquire")
{
    test::TwoSkins skins;
    const test::IdBankCounts baseline=test::IdBankCounts::now();

    Tree tree(&skins.skinA);
    tree.frame();

    const std::set<size_t> primitivesA=skins.renderer.livePrimitives;
    const std::set<size_t> textsA=skins.renderer.liveTexts;
    REQUIRE(primitivesA.size() == Tree::primitiveCount);
    REQUIRE(textsA.size() == Tree::textCount);
    skins.renderer.events.clear();

    tree.canvas->setSkin(&skins.skinB);

    // all of A's ids are back in the bank before anything renders under B
    std::set<size_t> releasedPrimitives, releasedTexts;
    for(const test::StubRenderer::Event &event : skins.renderer.events)
    {
        if(event.op == test::StubRenderer::Op::ReleasePrimitive)
            releasedPrimitives.insert(event.id);
        else if(event.op == test::StubRenderer::Op::ReleaseText)
            releasedTexts.insert(event.id);
    }
    CHECK(releasedPrimitives == primitivesA);
    CHECK(releasedTexts == textsA);
    CHECK(skins.renderer.events.size() == Tree::primitiveCount+Tree::textCount);
    CHECK(skins.renderer.livePrimitives.empty());
    CHECK(skins.renderer.liveTexts.empty());
    CHECK(skins.skinA.liveControls == 0);

    skins.renderer.events.clear();
    tree.frame();

    // B's Controls only acquire, and every control renders through one of them
    CHECK(count(skins.renderer.events, test::StubRenderer::Op::AcquirePrimitive) == Tree::primitiveCount);
    CHECK(count(skins.renderer.events, test::StubRenderer::Op::AcquireText) == Tree::textCount);
    CHECK(skins.renderer.events.size() == Tree::primitiveCount+Tree::textCount);
    CHECK(skins.skinA.liveControls == 0);
    CHECK(skins.skinB.liveControls == Tree::controlCount+1);

    size_t generatedByB=0;
    test::forEachControl(tree.canvas, [&](controls::Base *control)
        {
            if(control == tree.canvas)
                return;

            auto found=skins.renderer.generatedBy.find(control);
            REQUIRE(found != skins.renderer.generatedBy.end());
            CHECK(found->second == &skins.skinB);
            ++generatedByB;
        });
    CHECK(generatedByB == Tree::controlCount);

    // nothing of A's is left live
    test::IdBankCounts afterSwap=test::IdBankCounts::now();
    CHECK(afterSwap.livePrimitives()-baseline.livePrimitives() == Tree::primitiveCount);
    CHECK(afterSwap.liveTexts()-baseline.liveTexts() == Tree::textCount);
    checkNoDuplicateIds(skins.renderer);
}

TEST_CASE("swapping skins back and forth recycles ids instead of appending")
{
    test::TwoSkins skins;
    const test::IdBankCounts baseline=test::IdBankCounts::now();

    Tree tree(&skins.skinA);
    tree.frame();

    const test::IdBankCounts first=test::IdBankCounts::now();

    for(int swap=1; swap <= 6; ++swap)
    {
        CAPTURE(swap);
        tree.canvas->setSkin((swap%2 == 1)?&skins.skinB:&skins.skinA);
        tree.frame();

        test::IdBankCounts now=test::IdBankCounts::now();
        CHECK(now.primitives == first.primitives);
        CHECK(now.texts == first.texts);
        CHECK(now.livePrimitives()-baseline.livePrimitives() == Tree::primitiveCount);
        CHECK(now.liveTexts()-baseline.liveTexts() == Tree::textCount);
        CHECK(skins.renderer.livePrimitives.size() == Tree::primitiveCount);
        CHECK(skins.renderer.liveTexts.size() == Tree::textCount);
        checkNoDuplicateIds(skins.renderer);
    }
}

TEST_CASE("destroying a re-skinned canvas returns every id the tree held")
{
    test::TwoSkins skins;
    const test::IdBankCounts baseline=test::IdBankCounts::now();

    {
        Tree tree(&skins.skinA);
        tree.frame();

        for(int swap=1; swap <= 4; ++swap)
        {
            tree.canvas->setSkin((swap%2 == 1)?&skins.skinB:&skins.skinA);
            tree.frame();
        }
    }

    test::IdBankCounts after=test::IdBankCounts::now();
    CHECK(after.livePrimitives() == baseline.livePrimitives());
    CHECK(after.liveTexts() == baseline.liveTexts());
    CHECK(skins.renderer.livePrimitives.empty());
    CHECK(skins.renderer.liveTexts.empty());
    CHECK(skins.skinA.liveControls == 0);
    CHECK(skins.skinB.liveControls == 0);
    checkNoDuplicateIds(skins.renderer);
}

TEST_CASE("a skin::Control's remove() then generate() releases each id once")
{
    test::TwoSkins skins;
    const test::IdBankCounts baseline=test::IdBankCounts::now();

    test::StubControl control(&skins.skinA, false);

    control.generate(&skins.renderer, nullptr);
    control.remove(&skins.renderer, nullptr);
    control.generate(&skins.renderer, nullptr);

    CHECK(test::IdBankCounts::now().livePrimitives()-baseline.livePrimitives()
        == test::StubControl::primitivesPerControl);
    CHECK(count(skins.renderer.events, test::StubRenderer::Op::ReleasePrimitive)
        == test::StubControl::primitivesPerControl);
    checkNoDuplicateIds(skins.renderer);

    control.remove(&skins.renderer, nullptr);
    CHECK(test::IdBankCounts::now().livePrimitives() == baseline.livePrimitives());
    checkNoDuplicateIds(skins.renderer);
}
