/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#ifndef _gweni_controls_heightindex_h_
#define _gweni_controls_heightindex_h_

#include <gweni/export.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace gweni
{
namespace controls
{

//! The heights of a sequence of items, for a list whose rows are not all one
//! height (VirtualList's variable mode): a B+-tree of sums. Its leaves hold up
//! to MaxLeaf items, each a height and the generation it was measured at, and
//! every inner node its subtree's item count, height sum and lowest
//! generation, so with N items:
//! - append, insert, erase (a pop from the front included) and set cost
//!   O(log N), plus the items added or removed;
//! - offsetOf (item -> offset, the sum of the heights before it), itemAt
//!   (offset -> item) and firstStale cost O(log N);
//! - size, totalHeight and minGeneration cost O(1).
//! A Fenwick tree has neither an insert nor an erase in the middle, nor a pop
//! from the front, which a log's cap and an editor's line edits need.
//!
//! A generation is the caller's stamp of when an item was measured (the
//! list's, bumped when its width changes), so "is every item measured at the
//! current width" is minGeneration() == current, and the next item to
//! measure again is firstStale(from, current), without a walk of the items.
//! Heights may be 0 and are never negative.
class GWENI_EXPORT HeightIndex
{
public:
    //! Items a leaf holds at most, and children an inner node has at most.
    static constexpr size_t MaxLeaf=256;
    static constexpr size_t MaxInner=64;

    HeightIndex();
    ~HeightIndex();
    HeightIndex(HeightIndex &&other) noexcept;
    HeightIndex &operator=(HeightIndex &&other) noexcept;
    HeightIndex(const HeightIndex &)=delete;
    HeightIndex &operator=(const HeightIndex &)=delete;

    size_t size() const;
    bool empty() const { return size() == 0; }
    //! The sum of every item's height.
    int64_t totalHeight() const;
    //! The lowest generation of any item, UINT32_MAX when there is none.
    uint32_t minGeneration() const;

    void clear();
    //! Adds count items of the given height and generation at the end.
    void append(size_t count, int height, uint32_t generation);
    //! Adds count items of the given height and generation before item at
    //! (at == size() appends).
    void insert(size_t at, size_t count, int height, uint32_t generation);
    //! Removes the count items from at on (clipped to the end).
    void erase(size_t at, size_t count);
    //! Removes the first count items.
    void popFront(size_t count) { erase(0, count); }
    //! Gives item index a new height and generation.
    void set(size_t index, int height, uint32_t generation);

    int height(size_t index) const;
    uint32_t generation(size_t index) const;

    //! The sum of the heights of the items before index (index <= size();
    //! offsetOf(size()) is totalHeight()).
    int64_t offsetOf(size_t index) const;
    //! The item whose span [offsetOf(i), offsetOf(i)+height(i)) holds
    //! offset: the number of items that end at or above it, so an offset
    //! below 0 is item 0, one at or past totalHeight() is size(), and a
    //! 0-height item is the answer for no other offset. When itemTop is
    //! given it gets offsetOf() of the answer.
    size_t itemAt(int64_t offset, int64_t *itemTop=nullptr) const;
    //! The first item at or after from whose generation is below the given
    //! one, or size() if there is none.
    size_t firstStale(size_t from, uint32_t generation) const;

    //! Whether every node's count, sum and lowest generation are its
    //! children's, every leaf is at one depth and no node but the root is
    //! under a quarter full (for the tests).
    bool checkInvariants() const;

private:
    struct Node;

    std::unique_ptr<Node> m_root;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_heightindex_h_
