/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#ifndef _gweni_controls_treetable_h_
#define _gweni_controls_treetable_h_

#include <gweni/controls/dataTable.h>

#include <unordered_map>

namespace gweni
{
namespace controls
{

//! A hierarchy of records shown through DataTable's source mode. Nodes are
//! data, not controls: only the visible, expanded tree is indexed, and only
//! the viewport's rows have pooled DataTableRows. Columns, the frozen header,
//! grips, stripes, borders and sort notifications are DataTable's.
//!
//! Build the hierarchy with addNode before showing it. App ids are unique,
//! stable size_t values, also used as handles; Root denotes no parent and
//! cannot be a node's id. Sorting and live hierarchy edits are not supported.
//! A header sort notification leaves the hierarchy in its original order.
class GWENI_EXPORT TreeTable: public DataTable
{
public:
    GWENI_CONTROL(TreeTable, DataTable);

public:
    using NodeId=size_t;
    static constexpr NodeId NoNode=static_cast<NodeId>(-1);
    static constexpr NodeId Root=NoNode;

    struct Node
    {
        NodeId id=NoNode;
        NodeId parent=Root;
        std::vector<NodeId> children;
        size_t depth=0;
        bool expanded=false;
        //! True even before children are supplied, if the app says so.
        bool hasChildren=false;
    };

    struct VisibleNode
    {
        NodeId id=NoNode;
        size_t depth=0;
    };

    //! Fills the cells of a pooled row for an app id and its tree depth.
    //! As with DataTable's binder, do not move or resize the row.
    using NodeBinder=std::function<void(DataTableRow *row, NodeId id, size_t depth)>;

    //! Adds a node at the end of its parent's children (Root: another root).
    //! The handle is appId. A duplicate/reserved id or missing parent returns
    //! NoNode and changes nothing. hasChildren may advertise unloaded children.
    NodeId addNode(NodeId parentId, NodeId appId, bool hasChildren=false);
    size_t getNodeCount() const { return m_nodes.size(); }
    //! Null for an unknown id. The pointer lasts until the next addNode.
    const Node *getNode(NodeId id) const;

    void setNodeBinder(NodeBinder binder);

    //! Changes expansion, firing onExpanded/onCollapsed once if it changed,
    //! even when the node is hidden by an ancestor. Leaves cannot expand.
    void setExpanded(NodeId id, bool expanded);
    bool isExpanded(NodeId id) const;
    //! Changes every parent, with one notification per changed node.
    void expandAll();
    void collapseAll();
    event::Caller onExpanded;
    event::Caller onCollapsed;

    //! Expanded nodes in display order, with roots at depth zero. This vector
    //! is the source-mode item index; insert/erase moves the suffix in memory.
    const std::vector<VisibleNode> &getVisibleNodes() const { return m_visible; }
    NodeId nodeForItem(size_t item) const;
    //! The visible item of id, or NoItem when unknown or hidden. O(visible N).
    size_t itemForNode(NodeId id) const;

private:
    //! The model owns the source count and its item-to-node binder.
    using DataTable::setItemCount;
    using DataTable::setRowBinder;

    void appendVisible(std::vector<VisibleNode> &into, const std::vector<NodeId> &children) const;
    size_t subtreeEnd(size_t item) const;
    void visibleChanged(size_t firstChanged);
    void setAllExpanded(bool expanded);
    void expansionEvent(NodeId id, bool expanded);

    std::vector<Node> m_nodes;
    std::unordered_map<NodeId, size_t> m_nodeIndex;
    std::vector<NodeId> m_roots;
    std::vector<VisibleNode> m_visible;
    NodeBinder m_nodeBinder;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_treetable_h_
