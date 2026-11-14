/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/controls/treeTable.h>

#include <algorithm>
#include <utility>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(TreeTable)
{
    DataTable::setRowBinder([this](DataTableRow *row, size_t item)
        {
            if(item < m_visible.size() && m_nodeBinder)
            {
                const VisibleNode node=m_visible[item];

                m_nodeBinder(row, node.id, node.depth);
            }
        });
}

const TreeTable::Node *TreeTable::getNode(NodeId id) const
{
    const auto found=m_nodeIndex.find(id);

    return found == m_nodeIndex.end()?nullptr:&m_nodes[found->second];
}

TreeTable::NodeId TreeTable::addNode(NodeId parentId, NodeId appId, bool hasChildren)
{
    if(appId == NoNode || m_nodeIndex.count(appId))
        return NoNode;

    const Node *parent=getNode(parentId);

    if(parentId != Root && !parent)
        return NoNode;

    Node node;

    node.id=appId;
    node.parent=parentId;
    node.depth=parent?parent->depth+1:0;
    node.hasChildren=hasChildren;
    m_nodeIndex.emplace(appId, m_nodes.size());
    m_nodes.push_back(std::move(node));

    size_t at=m_visible.size();

    if(parentId == Root)
        m_roots.push_back(appId);
    else
    {
        // push_back above may have relocated the store.
        Node &entry=m_nodes[m_nodeIndex.at(parentId)];

        entry.children.push_back(appId);
        entry.hasChildren=true;
        if(!entry.expanded)
            return appId;

        const size_t item=itemForNode(parentId);

        if(item == NoItem)
            return appId;
        at=subtreeEnd(item);
    }

    m_visible.insert(m_visible.begin()+at, {appId, m_nodes.back().depth});
    visibleChanged(at);
    return appId;
}

void TreeTable::setNodeBinder(NodeBinder binder)
{
    m_nodeBinder=std::move(binder);
    rebind();
}

TreeTable::NodeId TreeTable::nodeForItem(size_t item) const
{
    return item < m_visible.size()?m_visible[item].id:NoNode;
}

size_t TreeTable::itemForNode(NodeId id) const
{
    const auto found=std::find_if(m_visible.begin(), m_visible.end(),
        [id](const VisibleNode &node) { return node.id == id; });

    return found == m_visible.end()?NoItem:static_cast<size_t>(found-m_visible.begin());
}

size_t TreeTable::subtreeEnd(size_t item) const
{
    const size_t depth=m_visible[item].depth;
    size_t end=item+1;

    while(end < m_visible.size() && m_visible[end].depth > depth)
        ++end;
    return end;
}

// Iterative preorder, so a hierarchy deeper than the process stack still
// works. Only expanded descendants are visited, in the app's child order.
void TreeTable::appendVisible(std::vector<VisibleNode> &into,
    const std::vector<NodeId> &children) const
{
    std::vector<NodeId> pending(children.rbegin(), children.rend());

    while(!pending.empty())
    {
        const NodeId id=pending.back();
        const Node &node=m_nodes[m_nodeIndex.at(id)];

        pending.pop_back();
        into.push_back({id, node.depth});
        if(node.expanded)
            pending.insert(pending.end(), node.children.rbegin(), node.children.rend());
    }
}

// The item count takes care of rows newly shown or hidden by a changed range.
// Pooled rows still showing an index in the suffix now show another node:
// refresh those alone. Rows before the insertion/removal keep their content.
void TreeTable::visibleChanged(size_t firstChanged)
{
    DataTable::setItemCount(m_visible.size());

    for(Base *control : getVirtualBody()->getRows())
    {
        const size_t item=getVirtualBody()->itemForRow(control);

        if(item != NoItem && item >= firstChanged && item < m_visible.size())
            rebindItem(item);
    }
}

bool TreeTable::isExpanded(NodeId id) const
{
    const Node *node=getNode(id);

    return node && node->expanded;
}

void TreeTable::expansionEvent(NodeId id, bool expanded)
{
    event::Info info(this);

    info.integer=id;
    (expanded?onExpanded:onCollapsed).call(this, info);
}

void TreeTable::setExpanded(NodeId id, bool expanded)
{
    const auto found=m_nodeIndex.find(id);

    if(found == m_nodeIndex.end())
        return;

    Node &node=m_nodes[found->second];

    if(node.expanded == expanded || (expanded && !node.hasChildren))
        return;
    node.expanded=expanded;

    const size_t item=itemForNode(id);

    if(item != NoItem)
    {
        const size_t at=item+1;

        if(expanded)
        {
            std::vector<VisibleNode> children;

            appendVisible(children, node.children);
            if(!children.empty())
            {
                m_visible.insert(m_visible.begin()+at, children.begin(), children.end());
                visibleChanged(at);
            }
        }
        else
        {
            const size_t end=subtreeEnd(item);

            if(end > at)
            {
                m_visible.erase(m_visible.begin()+at, m_visible.begin()+end);
                visibleChanged(at);
            }
        }
    }

    expansionEvent(id, expanded);
}

// A bulk change builds the projection once and binds only its changed suffix,
// instead of performing one vector insert and one invalidation per parent.
void TreeTable::setAllExpanded(bool expanded)
{
    std::vector<NodeId> changed;

    for(Node &node : m_nodes)
    {
        if(node.expanded != expanded && (!expanded || node.hasChildren))
        {
            node.expanded=expanded;
            changed.push_back(node.id);
        }
    }
    if(changed.empty())
        return;

    std::vector<VisibleNode> visible;

    visible.reserve(expanded?m_nodes.size():m_roots.size());
    appendVisible(visible, m_roots);

    size_t first=0;

    while(first < visible.size() && first < m_visible.size()
        && visible[first].id == m_visible[first].id)
        ++first;

    if(first < visible.size() || first < m_visible.size())
    {
        m_visible.swap(visible);
        visibleChanged(first);
    }

    for(NodeId id : changed)
        expansionEvent(id, expanded);
}

void TreeTable::expandAll()
{
    setAllExpanded(true);
}

void TreeTable::collapseAll()
{
    setAllExpanded(false);
}

}//namespace controls
}//namespace gweni
