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

class TreeTable;
class TreeTableRow;

namespace internal
{

//! A tree row's expand/collapse button: ExpanderSize square, in the tree
//! cell after the row's indent. Its skin renderer draws a surface while it is
//! hovered or pressed and a chevron, right while the node is collapsed and
//! down while it is expanded. A press toggles the node; it neither selects
//! the row nor takes the keyboard focus.
class GWENI_EXPORT TreeTableExpander: public Button
{
public:
    GWENI_CONTROL(TreeTableExpander, Button);

public:
    bool isExpanded() const { return m_expanded; }

    void onPress(event::Info info) override;
    void renderFocus(skin::Base * /*skin*/) override {}

private:
    friend class controls::TreeTableRow;

    //! The node's state as the row shows it: marks this button alone.
    void setExpanded(bool expanded);

    TreeTableRow *m_row=nullptr;
    bool m_expanded=false;
};

}//namespace internal

//! A TreeTable's pooled row: a DataTableRow whose tree cell is indented by
//! its node's depth and holds the expander (hidden for a node with no
//! children, which keeps the indent). A depth change gives the tree cell a
//! new left padding and lays out this row alone; an expansion change marks
//! the expander alone.
class GWENI_EXPORT TreeTableRow: public DataTableRow
{
public:
    GWENI_CONTROL(TreeTableRow, DataTableRow);

public:
    internal::TreeTableExpander *getExpander() const { return m_expander; }
    //! The depth of the node the row shows.
    size_t getDepth() const { return m_depth; }

    //! A double-click toggles a parent and activates a leaf (onNodeActivated).
    void onMouseDoubleClickLeft(int x, int y) override;

    //! The expander is drawn and hit-tested inside the tree cell only.
    const Rect *getChildClipRect(const Base *child) const override;
    bool hasChildClipRects() const override { return true; }

protected:
    void arrangeHorizontal(skin::Base *skin) override;
    void arrangeVertical(skin::Base *skin) override;

private:
    friend class TreeTable;
    friend class internal::TreeTableExpander;

    //! What the row shows of its node: the tree column, the depth, and
    //! whether it has an expander and in which state.
    void setTreeState(int column, size_t depth, bool hasChildren, bool expanded);
    void expanderPressed();

    TreeTable *m_tree=nullptr;
    internal::TreeTableExpander *m_expander=nullptr;
    int m_treeColumn=0;
    size_t m_depth=0;
    Rect m_expanderClip;
};

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

    //! The units a level of depth indents the tree cell by, and the
    //! expander's side; the tree cell's text starts after both.
    static constexpr int Indent=16;
    static constexpr int ExpanderSize=15;
    //! The column the tree is drawn in (0 by default). Out of range does
    //! nothing.
    void setTreeColumn(int column);
    int getTreeColumn() const { return m_treeColumn; }

    //! Expands the node if it is collapsed, and collapses it if it is
    //! expanded (a leaf: nothing).
    void toggleExpanded(NodeId id);
    //! Fired on Return or a double-click on a node with no children, with
    //! the id as the info's `integer`.
    event::Caller onNodeActivated;

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

protected:
    DataTableRow *newSourceRow(Base *parent) override;

private:
    friend class TreeTableRow;

    //! The model owns the source count and its item-to-node binder.
    using DataTable::setItemCount;
    using DataTable::setRowBinder;

    //! The row's tree cell, depth and expander for the node it shows.
    void showNode(TreeTableRow *row, const VisibleNode &node);
    //! Every pooled row shows its node's expansion again (the toggled node's
    //! row is before the changed suffix and is not bound again).
    void syncExpanders(size_t before);
    void rowDoubleClicked(TreeTableRow *row);
    void activateNode(NodeId id);

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
    int m_treeColumn=0;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_treetable_h_
