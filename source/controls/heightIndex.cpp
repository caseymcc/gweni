/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/heightIndex.h>

#include <algorithm>
#include <cassert>
#include <climits>
#include <utility>

namespace gweni
{
namespace controls
{

namespace
{

constexpr uint32_t NoGeneration=UINT32_MAX;

}//namespace

//! A leaf holds items, an inner node children, all at one depth below it.
//! count, sum and minGen are the subtree's, kept by recompute() after every
//! change below the node.
struct HeightIndex::Node
{
    struct Item
    {
        int32_t height;
        uint32_t generation;
    };

    using Ptr=std::unique_ptr<Node>;

    bool leaf=true;
    size_t count=0;
    int64_t sum=0;
    uint32_t minGen=NoGeneration;
    std::vector<Item> items;
    std::vector<Ptr> children;

    size_t entries() const { return leaf?items.size():children.size(); }
    size_t maxEntries() const { return leaf?MaxLeaf:MaxInner; }
    size_t minEntries() const { return maxEntries()/4; }

    void recompute()
    {
        count=0;
        sum=0;
        minGen=NoGeneration;
        if(leaf)
        {
            count=items.size();
            for(const Item &item:items)
            {
                sum+=item.height;
                minGen=std::min(minGen, item.generation);
            }
            return;
        }
        for(const Ptr &child:children)
        {
            count+=child->count;
            sum+=child->sum;
            minGen=std::min(minGen, child->minGen);
        }
    }

    //! Cuts a node over its maximum into pieces of near equal size: it keeps
    //! the first, the others are appended to split in order.
    void splitIfOver(std::vector<Ptr> &split)
    {
        size_t total=entries();

        if(total <= maxEntries())
        {
            recompute();
            return;
        }

        size_t pieces=(total+maxEntries()-1)/maxEntries();
        size_t first=total/pieces+(total%pieces?1:0);
        size_t start=first;

        for(size_t piece=1; piece < pieces; ++piece)
        {
            size_t size=total/pieces+(piece < total%pieces?1:0);
            Ptr node=std::make_unique<Node>();

            node->leaf=leaf;
            if(leaf)
                node->items.assign(items.begin()+start, items.begin()+start+size);
            else
            {
                node->children.reserve(size);
                for(size_t i=start; i < start+size; ++i)
                    node->children.push_back(std::move(children[i]));
            }
            node->recompute();
            split.push_back(std::move(node));
            start+=size;
        }
        if(leaf)
            items.resize(first);
        else
            children.resize(first);
        recompute();
    }

    //! The child holding item index (relative to this node), and index made
    //! relative to it; an index at a child's end goes to the next child, or
    //! to the last one when it is the end of the node.
    size_t childFor(size_t &index) const
    {
        size_t last=children.size()-1;

        for(size_t i=0; i < last; ++i)
        {
            if(index < children[i]->count)
                return i;
            index-=children[i]->count;
        }
        return last;
    }

    void insert(size_t at, size_t n, Item item, std::vector<Ptr> &split)
    {
        if(leaf)
        {
            items.insert(items.begin()+at, n, item);
            splitIfOver(split);
            return;
        }

        size_t c=childFor(at);
        std::vector<Ptr> childSplit;

        children[c]->insert(at, n, item, childSplit);
        children.insert(children.begin()+c+1,
            std::make_move_iterator(childSplit.begin()), std::make_move_iterator(childSplit.end()));
        splitIfOver(split);
    }

    //! Merges child i+1 into child i, splitting the result again when it is
    //! over the maximum.
    void mergeChildren(size_t i)
    {
        Node &left=*children[i];
        Node &right=*children[i+1];

        if(left.leaf)
            left.items.insert(left.items.end(), right.items.begin(), right.items.end());
        else
        {
            size_t junction=left.children.size();

            left.children.insert(left.children.end(),
                std::make_move_iterator(right.children.begin()), std::make_move_iterator(right.children.end()));
            // an erase can leave a child under its minimum at either side of
            // the junction, each the only child of its parent until now
            left.rebalance(junction > 0?junction-1:0);
        }
        children.erase(children.begin()+i+1);

        std::vector<Ptr> split;

        left.splitIfOver(split);
        children.insert(children.begin()+i+1,
            std::make_move_iterator(split.begin()), std::make_move_iterator(split.end()));
    }

    //! Erases [at, at+n) of this node's items, n > 0 and at+n <= count, and
    //! leaves no child under its minimum while there are two or more.
    void erase(size_t at, size_t n)
    {
        if(leaf)
        {
            items.erase(items.begin()+at, items.begin()+at+n);
            recompute();
            return;
        }

        size_t end=at+n;
        size_t start=0;

        for(Ptr &child:children)
        {
            size_t childEnd=start+child->count;

            if(childEnd > at && start < end)
            {
                if(at <= start && end >= childEnd)
                    child.reset();
                else
                {
                    size_t from=std::max(at, start);

                    child->erase(from-start, std::min(end, childEnd)-from);
                }
            }
            start=childEnd;
            if(start >= end)
                break;
        }
        children.erase(std::remove(children.begin(), children.end(), nullptr), children.end());
        rebalance(0);
        recompute();
    }

    //! Merges every child from the given one on that is under its minimum
    //! into a neighbour, while there are two or more.
    void rebalance(size_t from)
    {
        size_t i=from;

        while(i < children.size() && children.size() > 1)
        {
            if(children[i]->entries() >= children[i]->minEntries())
            {
                ++i;
                continue;
            }

            size_t left=(i+1 < children.size())?i:i-1;

            mergeChildren(left);
            i=left;
        }
    }

    void set(size_t index, Item item)
    {
        if(leaf)
        {
            Item &old=items[index];
            uint32_t oldGen=old.generation;

            sum+=int64_t(item.height)-old.height;
            old=item;
            if(item.generation < minGen)
                minGen=item.generation;
            else if(oldGen == minGen && item.generation != oldGen)
            {
                minGen=NoGeneration;
                for(const Item &each:items)
                    minGen=std::min(minGen, each.generation);
            }
            return;
        }

        size_t c=childFor(index);
        Node &child=*children[c];
        int64_t oldSum=child.sum;
        uint32_t oldMin=child.minGen;

        child.set(index, item);
        sum+=child.sum-oldSum;
        if(child.minGen < minGen)
            minGen=child.minGen;
        else if(oldMin == minGen && child.minGen != oldMin)
        {
            minGen=NoGeneration;
            for(const Ptr &each:children)
                minGen=std::min(minGen, each->minGen);
        }
    }

    const Item &get(size_t index) const
    {
        const Node *node=this;

        while(!node->leaf)
            node=node->children[node->childFor(index)].get();
        return node->items[index];
    }

    size_t firstStale(size_t from, uint32_t generation) const
    {
        if(minGen >= generation || from >= count)
            return count;
        if(leaf)
        {
            for(size_t i=from; i < items.size(); ++i)
            {
                if(items[i].generation < generation)
                    return i;
            }
            return count;
        }

        size_t base=0;

        for(const Ptr &child:children)
        {
            if(from < child->count)
            {
                size_t found=child->firstStale(from, generation);

                if(found < child->count)
                    return base+found;
                from=0;
            }
            else
                from-=child->count;
            base+=child->count;
        }
        return count;
    }

    //! The depth of this node's leaves, or -1 if something is wrong below it.
    int check(bool root) const
    {
        if(!root && entries() < minEntries())
            return -1;
        if(entries() > maxEntries())
            return -1;

        size_t wantCount=0;
        int64_t wantSum=0;
        uint32_t wantMin=NoGeneration;
        int depth=0;

        if(leaf)
        {
            if(!children.empty())
                return -1;
            wantCount=items.size();
            for(const Item &item:items)
            {
                if(item.height < 0)
                    return -1;
                wantSum+=item.height;
                wantMin=std::min(wantMin, item.generation);
            }
        }
        else
        {
            if(!items.empty() || children.empty())
                return -1;
            for(size_t i=0; i < children.size(); ++i)
            {
                int childDepth=children[i]->check(false);

                if(childDepth < 0 || (i > 0 && childDepth != depth-1))
                    return -1;
                depth=childDepth+1;
                wantCount+=children[i]->count;
                wantSum+=children[i]->sum;
                wantMin=std::min(wantMin, children[i]->minGen);
            }
        }
        if(wantCount != count || wantSum != sum || wantMin != minGen)
            return -1;
        return depth;
    }
};

HeightIndex::HeightIndex():
    m_root(std::make_unique<Node>())
{}

HeightIndex::~HeightIndex()=default;

HeightIndex::HeightIndex(HeightIndex &&other) noexcept:
    m_root(std::move(other.m_root))
{}

HeightIndex &HeightIndex::operator=(HeightIndex &&other) noexcept
{
    m_root.swap(other.m_root);
    return *this;
}

size_t HeightIndex::size() const
{
    return m_root?m_root->count:0;
}

int64_t HeightIndex::totalHeight() const
{
    return m_root?m_root->sum:0;
}

uint32_t HeightIndex::minGeneration() const
{
    return m_root?m_root->minGen:NoGeneration;
}

void HeightIndex::clear()
{
    m_root=std::make_unique<Node>();
}

void HeightIndex::append(size_t count, int height, uint32_t generation)
{
    insert(size(), count, height, generation);
}

void HeightIndex::insert(size_t at, size_t count, int height, uint32_t generation)
{
    if(!m_root)
        m_root=std::make_unique<Node>();
    assert(at <= size());
    at=std::min(at, size());
    if(count == 0)
        return;

    std::vector<Node::Ptr> split;

    m_root->insert(at, count, Node::Item{std::max(height, 0), generation}, split);
    while(!split.empty())
    {
        Node::Ptr root=std::make_unique<Node>();

        root->leaf=false;
        root->children.reserve(split.size()+1);
        root->children.push_back(std::move(m_root));
        for(Node::Ptr &node:split)
            root->children.push_back(std::move(node));
        split.clear();
        root->splitIfOver(split);
        m_root=std::move(root);
    }
}

void HeightIndex::erase(size_t at, size_t count)
{
    size_t total=size();

    if(at >= total || count == 0)
        return;
    count=std::min(count, total-at);
    if(count == total)
    {
        clear();
        return;
    }

    m_root->erase(at, count);
    while(!m_root->leaf && m_root->children.size() == 1)
    {
        Node::Ptr child=std::move(m_root->children.front());

        m_root=std::move(child);
    }
}

void HeightIndex::set(size_t index, int height, uint32_t generation)
{
    assert(index < size());
    if(index >= size())
        return;
    m_root->set(index, Node::Item{std::max(height, 0), generation});
}

int HeightIndex::height(size_t index) const
{
    assert(index < size());
    return m_root->get(index).height;
}

uint32_t HeightIndex::generation(size_t index) const
{
    assert(index < size());
    return m_root->get(index).generation;
}

int64_t HeightIndex::offsetOf(size_t index) const
{
    if(index >= size())
        return totalHeight();

    const Node *node=m_root.get();
    int64_t offset=0;

    while(!node->leaf)
    {
        size_t i=0;

        while(index >= node->children[i]->count)
        {
            index-=node->children[i]->count;
            offset+=node->children[i]->sum;
            ++i;
        }
        node=node->children[i].get();
    }
    for(size_t i=0; i < index; ++i)
        offset+=node->items[i].height;
    return offset;
}

size_t HeightIndex::itemAt(int64_t offset, int64_t *itemTop) const
{
    if(offset < 0)
    {
        if(itemTop)
            *itemTop=0;
        return 0;
    }
    if(offset >= totalHeight())
    {
        if(itemTop)
            *itemTop=totalHeight();
        return size();
    }

    const Node *node=m_root.get();
    size_t index=0;
    int64_t top=0;

    while(!node->leaf)
    {
        size_t i=0;

        while(offset >= node->children[i]->sum)
        {
            offset-=node->children[i]->sum;
            top+=node->children[i]->sum;
            index+=node->children[i]->count;
            ++i;
        }
        node=node->children[i].get();
    }

    size_t i=0;

    while(offset >= node->items[i].height)
    {
        offset-=node->items[i].height;
        top+=node->items[i].height;
        ++i;
    }
    if(itemTop)
        *itemTop=top;
    return index+i;
}

size_t HeightIndex::firstStale(size_t from, uint32_t generation) const
{
    if(!m_root)
        return 0;
    return m_root->firstStale(from, generation);
}

bool HeightIndex::checkInvariants() const
{
    return m_root && m_root->check(true) >= 0;
}

}//namespace controls
}//namespace gweni
