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

    //! Whether the row shows a lazy node's "Loading…" placeholder, which is
    //! no node: a press on it selects nothing.
    bool isPlaceholder() const { return m_placeholder; }

    //! A double-click toggles a parent and activates a leaf (onNodeActivated).
    void onMouseDoubleClickLeft(int x, int y) override;
    void onPress(event::Info info) override;
    //! A placeholder's tree cell in the skin's TreeTable.Placeholder colour.
    void updateColors() override;

    //! The expander is drawn and hit-tested inside the tree cell only; the
    //! other children get Label's rects. The row marks the expander itself
    //! when its rect changes, so a new row size marks no cell.
    const Rect *getChildClipRect(const Base *child) const override;

protected:
    void arrangeHorizontal(skin::Base *skin) override;
    void arrangeVertical(skin::Base *skin) override;

private:
    friend class TreeTable;
    friend class internal::TreeTableExpander;

    //! What the row shows of its node: the tree column, the depth, and
    //! whether it has an expander and in which state.
    void setTreeState(int column, size_t depth, bool hasChildren, bool expanded);
    void setPlaceholder(bool placeholder);
    void expanderPressed();

    TreeTable *m_tree=nullptr;
    internal::TreeTableExpander *m_expander=nullptr;
    int m_treeColumn=0;
    size_t m_depth=0;
    bool m_placeholder=false;
    Rect m_expanderClip;
};

//! A hierarchy of records shown through DataTable's source mode. Nodes are
//! data, not controls: only the visible, expanded tree is indexed, and only
//! the viewport's rows have pooled DataTableRows. Columns, the frozen header,
//! grips, stripes, borders and sort notifications are DataTable's.
//!
//! App ids are unique, stable size_t values, also used as handles; Root
//! denotes no parent and cannot be a node's id. Live edits splice the visible
//! preorder and keep the first shown node anchored, with only changed or
//! newly shown nodes bound again. A node may load its children lazily
//! (setHasChildren, onChildrenRequested), showing a placeholder row until the
//! app answers. A column with a comparator sorts every sibling group, the
//! hierarchy kept (setSortComparator).
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
        //! Lazy children: onChildrenRequested has fired for this node (until
        //! resetChildren), and the app has not answered yet.
        bool childrenRequested=false;
        bool loading=false;
        //! The children in the active sort's order, valid while sortedFor is
        //! the table's sort generation (a collapsed subtree is sorted when it
        //! is next shown). `children` stays the app's order.
        std::vector<NodeId> sorted;
        uint32_t sortedFor=0;
    };

    //! A visible row. A lazy node waiting for its children (expanded and
    //! loading) has one placeholder row under it, id NoNode.
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
    //! Inserts before child index (Root: a root), clamped to the end.
    //! Same id/parent validation and return value as addNode.
    NodeId insertNode(NodeId parentId, size_t index, NodeId appId);
    //! Inserts the ids as one run. False on a missing parent, reserved or
    //! duplicate id (also within ids), changing nothing; empty ids succeeds
    //! with a valid parent. index is clamped to the end.
    bool insertNodes(NodeId parentId, size_t index, const std::vector<NodeId> &ids);
    //! Removes the node and its entire subtree. A removed selection takes
    //! this subtree's next sibling, previous sibling, then parent (or none),
    //! firing onSelectionChange once. False for an unknown id.
    bool removeNode(NodeId id);
    //! Moves the subtree to index in newParent's children after detaching it
    //! (Root: the roots), clamped to the end. Expansion and selection stay
    //! with their nodes, even when hidden at the destination. False for
    //! unknown nodes/parents or a move into the node or its descendants.
    bool moveNode(NodeId id, NodeId newParent, size_t index);
    //! The app's fields changed: bind this node again if on screen. While a
    //! sort is active the node also moves to its sorted place among its
    //! siblings (its visible subtree with it), so a rename re-sorts it.
    void nodeChanged(NodeId id);

    //! Lazy children. setHasChildren(id, true) gives a node with no children
    //! an expander; its first expand fires onChildrenRequested (the id as the
    //! info's `integer`) before the node expands. The app answers in that
    //! call or later with insertNodes/insertNode/addNode under it, which put
    //! the children in the place of its one "Loading…" placeholder row in a
    //! single splice; or with no children (insertNodes with none, or
    //! setHasChildren(id, false)), which takes the expander and the
    //! placeholder away and collapses the node (onCollapsed if it had
    //! expanded). A node with children keeps hasChildren whatever this says.
    //! The request fires once per node until resetChildren.
    void setHasChildren(NodeId id, bool hasChildren);
    //! Removes the node's children (their subtrees; a selection among them
    //! moves to the node, firing onSelectionChange once), collapses it
    //! (onCollapsed if it was expanded) and keeps its expander, so its next
    //! expand asks for the children again.
    void resetChildren(NodeId id);
    event::Caller onChildrenRequested;

    //! Sorting across the hierarchy. A column with a comparator (`less` over
    //! app ids; null removes it) sorts every sibling group by it, stably, on
    //! the header's sort cycle (DataTable's: ascending, then descending;
    //! setSortColumn(-1, None) gives back the app's order). Expansion, the
    //! selection and the first shown node are kept. Only the shown, expanded
    //! groups are sorted at once; a collapsed or hidden one is sorted when
    //! it is next shown. Inserts, moves and nodeChanged while sorted go to
    //! their sorted place, and the indexes insertNode and moveNode take are
    //! in the app's order (getNode()->children), not the shown one. Without
    //! a comparator for the sort column the order is the app's and the
    //! header only fires onSortChanged.
    using Comparator=std::function<bool(NodeId a, NodeId b)>;
    void setSortComparator(int column, Comparator less);
    //! With a sort active, nodes with children (advertised ones included)
    //! come before leaves in both directions, as in a file manager.
    void setFoldersFirst(bool foldersFirst);
    bool getFoldersFirst() const { return m_foldersFirst; }
    //! Whether a column with a comparator is the sort column.
    bool isSorted() const;
    //! The parent's children in the order shown (sorted, or the app's). A
    //! collapsed group is sorted by this call if it was stale.
    const std::vector<NodeId> &getShownChildren(NodeId parent);
    size_t getNodeCount() const { return m_nodes.size(); }
    //! Null for an unknown id. The pointer lasts until the next model edit.
    const Node *getNode(NodeId id) const;

    void setNodeBinder(NodeBinder binder);

    //! The units a level of depth indents the tree cell by, and the
    //! expander's side; the tree cell's text starts after both.
    static constexpr int Indent=16;
    //! A loading node's placeholder row's tree cell text.
    static constexpr const char *PlaceholderText="Loading\xE2\x80\xA6";
    static constexpr int ExpanderSize=15;
    //! The column the tree is drawn in (0 by default). Out of range does
    //! nothing.
    void setTreeColumn(int column);
    int getTreeColumn() const { return m_treeColumn; }

    //! Expands the node if it is collapsed, and collapses it if it is
    //! expanded (a leaf: nothing).
    void toggleExpanded(NodeId id);
    //! Fired on Return (any node) or a double-click on a node with no
    //! children, with the id as the info's `integer`.
    event::Caller onNodeActivated;

    //! Expands the node and every node under it that has children, with one
    //! onExpanded per node that changed, building the visible rows once.
    void expandSubtree(NodeId id);

    //! The selection is a node: it stays on it across expand, collapse and
    //! scroll and moves. A move under a collapsed parent keeps the selected
    //! node with NoItem until it shows again. Collapsing an ancestor moves
    //! it to that ancestor, firing
    //! onSelectionChange (DataTable's caller) with the id as the info's
    //! `integer` (NoNode for none) and the row showing it, if any, as its
    //! `control`. A click on a row and the row keys select its node, firing
    //! it too.
    NodeId getSelectedNode() const { return m_selectedNode; }
    //! Selects the node with no event (NoNode: none). A node that is not
    //! shown (an ancestor is collapsed) or unknown is refused, leaving the
    //! selection as it was; it returns whether the node is now selected.
    bool setSelectedNode(NodeId id);

    //! The tree keys, besides DataTable's row keys: Right expands a collapsed
    //! parent or else moves to its first child; Left collapses an expanded
    //! node or else moves to its parent; Return activates the selected node;
    //! `*` expands its whole subtree (onChar). With nothing selected they do
    //! nothing.
    bool onKeyRight(bool down) override;
    bool onKeyLeft(bool down) override;
    bool onKeyReturn(bool down) override;
    bool onChar(UnicodeChar c) override;

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
    void selectedItemChanged(DataTableRow *row, size_t item, bool doEvents) override;
    void sortOrderChanged() override;

private:
    friend class TreeTableRow;

    //! The model owns the source count and its item-to-node binder.
    using DataTable::setItemCount;
    using DataTable::setRowBinder;

    //! The row's tree cell, depth and expander for the node it shows.
    void showNode(TreeTableRow *row, const VisibleNode &node);
    //! The row shows a placeholder: the tree cell "Loading…", the others empty.
    void showPlaceholder(TreeTableRow *row, const VisibleNode &node);
    //! The loading node is answered: with no children its placeholder goes
    //! (and the node collapses), otherwise the caller splices the children
    //! into the placeholder's place. Returns the placeholder's item (NoItem
    //! when not shown) for that splice.
    size_t endLoading(Node &node, bool empty);
    //! Marks a lazy node with nothing yet as requested and loading, so its
    //! expansion shows the placeholder; false when it needs no request.
    static bool beginRequest(Node &node);
    void requestChildren(NodeId id);
    //! The node's children, or its placeholder while it is loading.
    void appendSubtree(std::vector<VisibleNode> &into, NodeId id);
    //! Sorting: the comparator of the sort, with folders first and the
    //! direction applied; where a node goes among its parent's sorted
    //! children (ties in the app's order); the visible item where the shown
    //! child at pos would start (the parent's visible end past the last).
    bool shownLess(NodeId a, NodeId b) const;
    size_t sortedPlace(NodeId parent, const std::vector<NodeId> &order, NodeId id) const;
    size_t shownChildItem(NodeId parent, size_t pos);
    //! The parent's sorted order if it is current, else null.
    std::vector<NodeId> *currentSorted(NodeId parent);
    //! Moves the node to its sorted place among its siblings, its visible
    //! subtree with it.
    void keepSorted(NodeId id);
    //! A new sort: the shown groups are sorted and the projection is built
    //! again, the first shown node and the selection kept.
    void applySort();
    //! The first shown node and how far into its row the view is, and
    //! putting it back at the top after an edit moved it.
    struct Anchor
    {
        NodeId node=NoNode;
        int into=0;
    };
    Anchor captureAnchor();
    void restoreAnchor(const Anchor &anchor);
    //! Drops the store records of these nodes and their whole subtrees.
    void eraseRecords(std::vector<NodeId> pending);
    //! Every pooled row shows its node's expansion again (the toggled node's
    //! row is before the changed suffix and is not bound again).
    void syncExpanders(size_t before);
    void rowDoubleClicked(TreeTableRow *row);
    void activateNode(NodeId id);

    //! The visible preorder of these nodes' subtrees, each expanded group in
    //! its shown order (sorted on the way when stale).
    void appendVisible(std::vector<VisibleNode> &into, const std::vector<NodeId> &children);
    size_t subtreeEnd(size_t item) const;
    std::vector<NodeId> &childrenOf(NodeId parent);
    //! Where child index of this parent starts in the visible preorder, or
    //! NoItem when the parent's children are hidden.
    size_t insertionItem(NodeId parent, size_t index) const;
    bool isInSubtree(NodeId id, NodeId ancestor) const;
    //! The single-node path uses a stack id, the batch path the app's ids.
    bool insertNodes(NodeId parent, size_t index, const NodeId *ids, size_t count);
    //! Replace only this range; the VirtualList shifts its row associations
    //! and anchor, then the selection and stripes follow without rebinding
    //! rows that kept their nodes.
    void spliceVisible(size_t at, size_t removed, const std::vector<VisibleNode> &added);
    void spliceVisible(size_t at, size_t removed, const VisibleNode *added, size_t count);
    void syncVisibleSelection();
    //! The visible rows changed from firstChanged on, and the selected node
    //! is now the item `selected` (or none): the item count, the selection's
    //! item and the pooled rows of the suffix follow.
    void visibleChanged(size_t firstChanged, size_t selected);
    //! The topmost collapsed ancestor of the node, or the node itself when
    //! every ancestor is expanded: the node that shows in its place.
    NodeId shownAncestor(NodeId id) const;
    //! Fires onSelectionChange for the node selected now.
    void selectionEvent();
    void setAllExpanded(bool expanded);
    void expansionEvent(NodeId id, bool expanded);

    std::vector<Node> m_nodes;
    std::unordered_map<NodeId, size_t> m_nodeIndex;
    std::vector<NodeId> m_roots;
    std::vector<VisibleNode> m_visible;
    NodeBinder m_nodeBinder;
    int m_treeColumn=0;
    NodeId m_selectedNode=NoNode;
    //! Keeps the stripes of anchored rows when an odd run changes above them.
    size_t m_stripePhase=0;
    std::vector<Comparator> m_comparators;
    bool m_foldersFirst=false;
    //! Raised by every change of the shown order; a group whose sortedFor
    //! differs is stale. The roots' sorted order is kept here.
    uint32_t m_sortGeneration=1;
    std::vector<NodeId> m_rootsSorted;
    uint32_t m_rootsSortedFor=0;
    //! Whether m_visible is in a sort's order (so a sort's end rebuilds it).
    bool m_projectionSorted=false;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_treetable_h_
