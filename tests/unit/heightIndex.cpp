/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  controls::HeightIndex, VirtualList's B+-tree of item heights, against a
 *  plain vector of (height, generation): seeded random sequences of appends,
 *  inserts, erases, pops from the front and sets (single items and runs,
 *  0 heights among them), at sizes from empty through a few leaves to
 *  three levels, each followed by the tree's invariants (every node's count,
 *  sum and lowest generation its children's, one leaf depth, no node but the
 *  root under a quarter full) and its size, total and lowest generation, and
 *  then random height/generation reads, offsetOf (item -> offset), itemAt
 *  (offset -> item, with the item's top, at and around item boundaries and
 *  outside the content) and firstStale queries checked against the model,
 *  with every item compared now and then. Then the shapes a list makes: a
 *  log capped by popping its front while it appends, a bulk insert in the
 *  middle of a large index and an erase of nearly all of it, and a
 *  re-measure walk that firstStale steps through until minGeneration()
 *  reaches the new generation.
 */
#include <gweni/controls/heightIndex.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <random>
#include <vector>

using gweni::controls::HeightIndex;

namespace
{

struct ModelItem
{
    int height;
    uint32_t generation;
};

using Model=std::vector<ModelItem>;

int64_t modelOffset(const Model &model, size_t index)
{
    int64_t offset=0;

    for(size_t i=0; i < index && i < model.size(); ++i)
        offset+=model[i].height;
    return offset;
}

size_t modelItemAt(const Model &model, int64_t offset, int64_t &top)
{
    int64_t end=0;

    top=0;
    for(size_t i=0; i < model.size(); ++i)
    {
        end+=model[i].height;
        if(end > offset)
            return i;
        top=end;
    }
    return model.size();
}

size_t modelFirstStale(const Model &model, size_t from, uint32_t generation)
{
    for(size_t i=from; i < model.size(); ++i)
    {
        if(model[i].generation < generation)
            return i;
    }
    return model.size();
}

void checkSummary(const HeightIndex &index, const Model &model)
{
    REQUIRE(index.checkInvariants());
    REQUIRE(index.size() == model.size());
    REQUIRE(index.totalHeight() == modelOffset(model, model.size()));

    uint32_t minGen=UINT32_MAX;

    for(const ModelItem &item:model)
        minGen=std::min(minGen, item.generation);
    REQUIRE(index.minGeneration() == minGen);
}

void checkEveryItem(const HeightIndex &index, const Model &model)
{
    int64_t offset=0;

    for(size_t i=0; i < model.size(); ++i)
    {
        REQUIRE(index.height(i) == model[i].height);
        REQUIRE(index.generation(i) == model[i].generation);
        REQUIRE(index.offsetOf(i) == offset);
        offset+=model[i].height;
    }
    REQUIRE(index.offsetOf(model.size()) == offset);
}

void checkQueries(const HeightIndex &index, const Model &model, std::mt19937 &rng, uint32_t maxGeneration)
{
    int64_t total=modelOffset(model, model.size());

    for(int q=0; q < 8; ++q)
    {
        if(!model.empty())
        {
            size_t i=rng()%model.size();

            REQUIRE(index.height(i) == model[i].height);
            REQUIRE(index.generation(i) == model[i].generation);
        }

        size_t i=rng()%(model.size()+1);

        REQUIRE(index.offsetOf(i) == modelOffset(model, i));

        // an offset inside the content, one on an item's top and the pixel
        // above it, and a few outside it
        int64_t offsets[]={
            total > 0?int64_t(rng()%uint64_t(total)):0,
            modelOffset(model, i),
            modelOffset(model, i)-1,
            -int64_t(rng()%5)-1,
            total,
            total+int64_t(rng()%7),
        };

        for(int64_t offset:offsets)
        {
            int64_t top=-1;
            int64_t wantTop=0;
            size_t want=modelItemAt(model, offset, wantTop);

            CAPTURE(offset);
            REQUIRE(index.itemAt(offset, &top) == want);
            REQUIRE(top == wantTop);
            REQUIRE(index.itemAt(offset) == want);
        }

        size_t from=rng()%(model.size()+2);
        uint32_t generation=rng()%(maxGeneration+2);

        CAPTURE(from);
        CAPTURE(generation);
        REQUIRE(index.firstStale(from, generation) == modelFirstStale(model, from, generation));
    }
}

int randomHeight(std::mt19937 &rng)
{
    // mostly row-like heights, with 0 now and then
    return (rng()%10 == 0)?0:int(1+rng()%60);
}

//! One random edit on both, keeping the count near target.
void randomEdit(HeightIndex &index, Model &model, std::mt19937 &rng, size_t target, uint32_t maxGeneration)
{
    size_t size=model.size();
    int op=rng()%6;
    bool grow=size < target;
    size_t run=(rng()%4 == 0)?1+rng()%600:1+rng()%4;
    int height=randomHeight(rng);
    uint32_t generation=rng()%(maxGeneration+1);

    if(size == 0)
        op=grow?0:1;
    if(op == 0 || (op == 1 && grow && rng()%2))
    {
        index.append(run, height, generation);
        model.insert(model.end(), run, ModelItem{height, generation});
    }
    else if(op == 1)
    {
        size_t at=rng()%(size+1);

        index.insert(at, run, height, generation);
        model.insert(model.begin()+at, run, ModelItem{height, generation});
    }
    else if(op == 2 && !grow)
    {
        size_t at=rng()%size;
        // erase clips to the end, as the model does here
        size_t n=std::min(run, size-at);

        index.erase(at, run);
        model.erase(model.begin()+at, model.begin()+at+n);
    }
    else if(op == 3 && !grow)
    {
        size_t n=std::min(run, size);

        index.popFront(run);
        model.erase(model.begin(), model.begin()+n);
    }
    else
    {
        // a few sets, as a re-measure makes
        for(int k=0; k < 4 && size > 0; ++k)
        {
            size_t i=rng()%size;
            int h=randomHeight(rng);
            uint32_t g=rng()%(maxGeneration+1);

            index.set(i, h, g);
            model[i]=ModelItem{h, g};
        }
    }
}

void runRandom(unsigned seed, size_t target, int steps)
{
    std::mt19937 rng(seed);
    HeightIndex index;
    Model model;
    const uint32_t maxGeneration=5;

    CAPTURE(seed);
    for(int step=0; step < steps; ++step)
    {
        CAPTURE(step);
        // the target swings between empty and the given count
        size_t want=(step/200)%2?target:target/8;

        randomEdit(index, model, rng, want, maxGeneration);
        checkSummary(index, model);
        checkQueries(index, model, rng, maxGeneration);
        if(step%97 == 0)
            checkEveryItem(index, model);
    }
    checkEveryItem(index, model);
}

}//namespace

TEST_CASE("Height index: an empty index has no items, no height and no generation")
{
    HeightIndex index;

    CHECK(index.size() == 0);
    CHECK(index.empty());
    CHECK(index.totalHeight() == 0);
    CHECK(index.minGeneration() == UINT32_MAX);
    CHECK(index.offsetOf(0) == 0);
    CHECK(index.itemAt(0) == 0);
    CHECK(index.itemAt(10) == 0);
    CHECK(index.firstStale(0, 3) == 0);
    CHECK(index.checkInvariants());

    index.erase(0, 5);
    index.popFront(1);
    CHECK(index.size() == 0);
    CHECK(index.checkInvariants());
}

TEST_CASE("Height index: a few items give their offsets and are found by offset")
{
    HeightIndex index;

    index.append(1, 10, 1);
    index.append(1, 0, 1);
    index.append(1, 5, 2);
    index.insert(0, 2, 3, 0);
    // heights 3 3 10 0 5
    REQUIRE(index.size() == 5);
    CHECK(index.totalHeight() == 21);
    CHECK(index.minGeneration() == 0);
    CHECK(index.offsetOf(2) == 6);
    CHECK(index.offsetOf(4) == 16);
    CHECK(index.offsetOf(5) == 21);

    int64_t top=-1;

    CHECK(index.itemAt(-4, &top) == 0);
    CHECK(top == 0);
    CHECK(index.itemAt(5, &top) == 1);
    CHECK(top == 3);
    CHECK(index.itemAt(15, &top) == 2);
    CHECK(top == 6);
    // the 0-height item 3 is skipped: offset 16 is item 4's top
    CHECK(index.itemAt(16, &top) == 4);
    CHECK(top == 16);
    CHECK(index.itemAt(21, &top) == 5);
    CHECK(top == 21);

    CHECK(index.firstStale(0, 1) == 0);
    CHECK(index.firstStale(2, 1) == 5);
    CHECK(index.firstStale(2, 2) == 2);
    CHECK(index.firstStale(3, 2) == 3);
    CHECK(index.firstStale(4, 2) == 5);

    index.set(0, 4, 2);
    index.set(1, 4, 2);
    CHECK(index.minGeneration() == 1);
    CHECK(index.totalHeight() == 23);
    // a negative height counts as 0
    index.set(2, -7, 2);
    CHECK(index.height(2) == 0);
    CHECK(index.totalHeight() == 13);
    CHECK(index.checkInvariants());
}

TEST_CASE("Height index: random edits and queries agree with a vector, within a few leaves")
{
    for(unsigned seed=1; seed <= 6; ++seed)
        runRandom(seed, 3*HeightIndex::MaxLeaf, 1200);
}

TEST_CASE("Height index: random edits and queries agree with a vector, over three levels")
{
    // more than MaxLeaf*MaxInner items, so the root's children are inner
    // nodes
    for(unsigned seed=11; seed <= 12; ++seed)
    {
        std::mt19937 rng(seed);
        HeightIndex index;
        Model model;
        const size_t large=HeightIndex::MaxLeaf*HeightIndex::MaxInner*2+123;

        for(size_t i=0; i < large; ++i)
        {
            int h=randomHeight(rng);

            model.push_back(ModelItem{h, 1});
        }
        for(const ModelItem &item:model)
            index.append(1, item.height, item.generation);
        checkSummary(index, model);
        checkEveryItem(index, model);

        for(int step=0; step < 600; ++step)
        {
            CAPTURE(seed);
            CAPTURE(step);
            randomEdit(index, model, rng, large, 3);
            checkSummary(index, model);
            if(step%25 == 0)
                checkQueries(index, model, rng, 3);
        }
        checkEveryItem(index, model);
    }
}

TEST_CASE("Height index: a capped log appends at its end and pops its front")
{
    HeightIndex index;
    Model model;
    std::mt19937 rng(21);
    const size_t cap=5000;

    for(int frame=0; frame < 2000; ++frame)
    {
        size_t rows=1+rng()%17;

        for(size_t r=0; r < rows; ++r)
        {
            int h=randomHeight(rng);

            index.append(1, h, 1);
            model.push_back(ModelItem{h, 1});
        }
        if(model.size() > cap)
        {
            size_t drop=model.size()-cap;

            index.popFront(drop);
            model.erase(model.begin(), model.begin()+drop);
        }
        if(frame%50 == 0)
        {
            checkSummary(index, model);
            checkQueries(index, model, rng, 1);
        }
    }
    checkSummary(index, model);
    checkEveryItem(index, model);
}

TEST_CASE("Height index: a bulk insert in the middle and an erase of nearly everything")
{
    HeightIndex index;
    Model model;
    std::mt19937 rng(31);

    index.append(100000, 20, 1);
    model.assign(100000, ModelItem{20, 1});
    index.insert(40000, 30000, 7, 2);
    model.insert(model.begin()+40000, 30000, ModelItem{7, 2});
    checkSummary(index, model);
    checkQueries(index, model, rng, 2);
    CHECK(index.offsetOf(40000) == 40000*20);
    CHECK(index.offsetOf(70000) == 40000*20+30000*7);
    CHECK(index.itemAt(40000*20+6) == 40000);
    CHECK(index.itemAt(40000*20+7) == 40001);

    index.erase(3, 129990);
    model.erase(model.begin()+3, model.begin()+3+129990);
    checkSummary(index, model);
    checkEveryItem(index, model);

    index.erase(0, 1000);
    model.clear();
    checkSummary(index, model);
}

TEST_CASE("Height index: a re-measure walk steps through the stale items until the generation is reached")
{
    HeightIndex index;
    std::mt19937 rng(41);
    const size_t count=20000;

    index.append(count, 20, 1);

    // a width change: generation 2. The rows on screen are measured first.
    const uint32_t generation=2;

    for(size_t i=5000; i < 5030; ++i)
        index.set(i, 33, generation);
    CHECK(index.minGeneration() == 1);

    // then the rest, a budget a frame, in order from a cursor
    size_t cursor=0;
    size_t measured=0;
    int frames=0;

    while(index.minGeneration() < generation)
    {
        for(int budget=0; budget < 64; ++budget)
        {
            size_t next=index.firstStale(cursor, generation);

            if(next == index.size())
                next=index.firstStale(0, generation);
            if(next == index.size())
                break;
            REQUIRE((next < 5000 || next >= 5030));
            index.set(next, 10+int(rng()%30), generation);
            cursor=next+1;
            ++measured;
        }
        ++frames;
        REQUIRE(frames < 1000);
    }
    CHECK(measured == count-30);
    CHECK(frames == int((count-30+63)/64));
    CHECK(index.firstStale(0, generation) == index.size());
    CHECK(index.checkInvariants());
}
