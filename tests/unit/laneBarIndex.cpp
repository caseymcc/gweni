/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  controls::internal::LaneBarIndex, the search the Timeline's bars and the
 *  Sequencer's clips and keys share, on its own: the ranges sorted by start
 *  (equal starts in index order) with the running maximum of their ends, a
 *  range that is not searched left out of the order; each range's overlap
 *  level one more than the highest of the earlier ranges it overlaps, capped,
 *  ranges that only touch not overlapping; pick() finding the range on top
 *  (the highest level, then the earliest) among those starting before its
 *  high end whose ends reach its low end and that its accept takes, an early
 *  long range under later short ones still found between them; and on random
 *  ranges every pick is a brute force's.
 */

#include <doctest/doctest.h>

#include <gweni/controls/laneView.h>

#include <limits>
#include <random>
#include <utility>
#include <vector>

using gweni::controls::internal::LaneBarIndex;

namespace
{

typedef std::vector<std::pair<double, double>> Ranges;

void build(LaneBarIndex &index, const Ranges &ranges, int maxLevel=3)
{
    index.build(ranges.size(), [&ranges](size_t i, double &start, double &end)
    {
        start=ranges[i].first;
        end=ranges[i].second;
        return start == start && end == end;   // not NaN
    }, maxLevel);
}

// The range on top of those starting before x whose end reaches it: the
// walk visits every range whose running maximum reaches x, so accept() holds
// each to its own end.
bool pickAt(const LaneBarIndex &index, const Ranges &ranges, double x, size_t &found)
{
    return index.pick(x, x, [&ranges, x](size_t i) { return ranges[i].second >= x; }, found);
}

}//namespace

TEST_CASE("Lane bar index: sorted starts, the running maximum of the ends, levels")
{
    LaneBarIndex index;
    const double nan=std::numeric_limits<double>::quiet_NaN();

    build(index, {{5, 6}, {0, 10}, {2, 3}, {nan, 1}, {2, 4}, {10, 11}});
    CHECK(!index.dirty);
    CHECK(index.order == std::vector<size_t>{1, 2, 4, 0, 5});
    CHECK(index.starts == std::vector<double>{0, 2, 2, 5, 10});
    CHECK(index.maxEnds == std::vector<double>{10, 10, 10, 10, 11});
    // 1 overlaps 0; 2 overlaps 1; 4 overlaps 1 and 2; 5 only touches 1
    CHECK(index.levels == std::vector<int>{0, 1, 2, 0, 3, 0});

    // capped
    build(index, {{0, 10}, {1, 9}, {2, 8}, {3, 7}}, 2);
    CHECK(index.levels == std::vector<int>{0, 1, 2, 2});
}

TEST_CASE("Lane bar index: pick takes the range on top")
{
    LaneBarIndex index;
    size_t found=99;

    // a long early range under two short later ones
    const Ranges nested={{0, 100}, {10, 20}, {50, 60}};

    build(index, nested);
    REQUIRE(pickAt(index, nested, 15, found));
    CHECK(found == 1);
    REQUIRE(pickAt(index, nested, 30, found));
    CHECK(found == 0);
    REQUIRE(pickAt(index, nested, 55, found));
    CHECK(found == 2);
    CHECK(!pickAt(index, nested, 150, found));

    // past the cap two share the top level: the earlier is picked
    const Ranges stacked={{0, 10}, {1, 9}, {2, 8}};

    build(index, stacked, 1);
    REQUIRE(pickAt(index, stacked, 5, found));
    CHECK(found == 1);

    // accept() narrows the candidates
    build(index, {{0, 100}, {10, 20}});
    REQUIRE(index.pick(15, 15, [](size_t i) { return i == 0; }, found));
    CHECK(found == 0);
}

TEST_CASE("Lane bar index: random ranges pick what a brute force picks")
{
    std::mt19937 random(644);
    std::uniform_real_distribution<double> at(0.0, 1000.0);
    std::uniform_real_distribution<double> length(0.0, 80.0);

    for(int round=0; round < 20; ++round)
    {
        Ranges ranges(60);

        for(auto &range : ranges)
        {
            range.first=at(random);
            range.second=range.first+length(random);
        }

        LaneBarIndex index;

        build(index, ranges, 3);
        for(double x=0.0; x < 1100.0; x+=3.7)
        {
            bool any=false;
            size_t best=0;

            for(size_t i=0; i < ranges.size(); ++i)
            {
                if(!(ranges[i].first < x && ranges[i].second >= x))
                    continue;
                if(!any || index.levels[i] > index.levels[best] || (index.levels[i] == index.levels[best] && i < best))
                    best=i;
                any=true;
            }

            size_t found=0;
            const bool picked=pickAt(index, ranges, x, found);

            REQUIRE(picked == any);
            if(any)
                CHECK(found == best);
        }
    }
}
