/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/controls/treeTable.h>
#include <gweni/controls/canvas.h>

#include <algorithm>
#include <climits>
#include <unordered_set>
#include <utility>

namespace gweni
{
namespace controls
{

namespace internal
{

///////////////////////////////////////////////////////////////////////////
//TreeTableExpander

// A glyph with no caption, as a TreeNode's toggle: a fixed square that takes
// the mouse but no keyboard focus.
GWENI_CONTROL_CONSTRUCTOR(TreeTableExpander)
{
    setTabable(false);
    setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    setSize(TreeTable::ExpanderSize, TreeTable::ExpanderSize);
}

void TreeTableExpander::onPress(event::Info info)
{
    ParentClass::onPress(info);

    if(m_row)
        m_row->expanderPressed();
}

void TreeTableExpander::setExpanded(bool expanded)
{
    if(m_expanded == expanded)
        return;

    m_expanded=expanded;
    // the chevron's direction, this button only
    addStateChange(StateChange_Background);
}

}//namespace internal

///////////////////////////////////////////////////////////////////////////
//TreeTableRow

// The expander is made with the row, so binding a parent to a pooled row
// later shows it and makes nothing. It is owner-placed in the tree cell.
GWENI_CONTROL_CONSTRUCTOR(TreeTableRow)
{
    m_expander=newChild<internal::TreeTableExpander>();
    m_expander->m_row=this;
    m_expander->setPlacement(Placement::Owner);
    m_expander->setHidden(true);
}

// The tree cell's text starts past the indent and the expander's column, a
// leaf's too, so siblings' text lines up whether they have children or not:
// a child's expander is under its parent's text.
void TreeTableRow::setTreeState(int column, size_t depth, bool hasChildren, bool expanded)
{
    if(column != m_treeColumn)
    {
        if(Label *previous=getCell(m_treeColumn))
            previous->setPadding(Padding(CellPaddingX, CellPaddingY, CellPaddingX, CellPaddingY));
        m_treeColumn=column;
        invalidate();
    }

    if(Label *cell=getCell(column))
    {
        const int left=CellPaddingX+static_cast<int>(depth)*TreeTable::Indent+TreeTable::Indent;

        cell->setPadding(Padding(left, CellPaddingY, CellPaddingX, CellPaddingY));
    }

    if(depth != m_depth)
    {
        m_depth=depth;
        // the expander's place
        invalidate();
    }

    m_expander->setHidden(!hasChildren);
    m_expander->setExpanded(expanded);
}

// Only a change marks anything: a node bound to a row that showed a node
// sets nothing here.
void TreeTableRow::setPlaceholder(bool placeholder)
{
    if(placeholder == m_placeholder)
        return;

    m_placeholder=placeholder;
    updateColors();
}

// DataTableRow's cell colours, with a placeholder's tree cell dimmer: set once
// per cell, so a colour that stays marks nothing.
void TreeTableRow::updateColors()
{
    Selectable::updateColors();

    const Color &color=getTextColor();

    for(int column=0; column < getCellCount(); ++column)
    {
        const bool dim=m_placeholder && column == m_treeColumn && getSkin();

        getCell(column)->setTextColor(dim?getSkin()->Colors.TreeTable.Placeholder:color);
    }
}

void TreeTableRow::onPress(event::Info info)
{
    if(m_placeholder)
        return;
    ParentClass::onPress(info);
}

void TreeTableRow::expanderPressed()
{
    if(!m_tree)
        return;

    m_tree->toggleExpanded(m_tree->nodeForItem(m_tree->itemForRow(this)));
}

void TreeTableRow::onMouseDoubleClickLeft(int x, int y)
{
    ParentClass::onMouseDoubleClickLeft(x, y);

    if(m_tree && !isDisabled())
        m_tree->rowDoubleClicked(this);
}

const Rect *TreeTableRow::getChildClipRect(const Base *child) const
{
    return child == m_expander?&m_expanderClip:ParentClass::getChildClipRect(child);
}

// The expander after the tree cell's padding and the indent, clipped to the
// cell: a column narrower than the indent cuts it, as it cuts the text. A new
// clip with the same bounds still re-stamps its slots.
void TreeTableRow::arrangeHorizontal(skin::Base *skin)
{
    ParentClass::arrangeHorizontal(skin);

    Rect clip(0, m_expanderClip.y, 0, m_expanderClip.h);
    int x=0;

    if(Label *cell=getCell(m_treeColumn))
    {
        clip.x=cell->getX();
        clip.w=cell->getWidth();
        x=cell->getX()+CellPaddingX+static_cast<int>(m_depth)*TreeTable::Indent;
    }

    placeChild(m_expander, x, m_expander->getY(), TreeTable::ExpanderSize, m_expander->getHeight());

    if(!(clip == m_expanderClip))
    {
        m_expanderClip=clip;
        m_expander->addStateChangeRecursive(StateChange_Geometry);
    }
}

void TreeTableRow::arrangeVertical(skin::Base *skin)
{
    ParentClass::arrangeVertical(skin);

    const int size=std::min(TreeTable::ExpanderSize, getHeight());
    const Rect clip(m_expanderClip.x, 0, m_expanderClip.w, getHeight());

    placeChild(m_expander, m_expander->getX(), (getHeight()-size)/2, m_expander->getWidth(), size);

    if(!(clip == m_expanderClip))
    {
        m_expanderClip=clip;
        m_expander->addStateChangeRecursive(StateChange_Geometry);
    }
}

///////////////////////////////////////////////////////////////////////////
//TreeTable

GWENI_CONTROL_CONSTRUCTOR(TreeTable)
{
    DataTable::setRowBinder([this](DataTableRow *row, size_t item)
        {
            if(item >= m_visible.size())
                return;

            const VisibleNode node=m_visible[item];
            TreeTableRow *treeRow=gweni_cast<TreeTableRow>(row);

            if(treeRow)
                showNode(treeRow, node);
            // a placeholder is no node: the app's binder is not asked
            if(node.id == NoNode)
                return;
            if(m_nodeBinder)
                m_nodeBinder(row, node.id, node.depth);
        });
}

DataTableRow *TreeTable::newSourceRow(Base *parent)
{
    TreeTableRow *row=parent->newChild<TreeTableRow>();

    row->m_tree=this;
    return row;
}

void TreeTable::showNode(TreeTableRow *row, const VisibleNode &visible)
{
    if(visible.id == NoNode)
    {
        showPlaceholder(row, visible);
        return;
    }

    const Node *node=getNode(visible.id);

    row->setPlaceholder(false);
    row->setTreeState(m_treeColumn, visible.depth, node && node->hasChildren, node && node->expanded);
}

// The placeholder is at its loading node's child depth, with no expander; the
// node binder may have filled every cell, so they are emptied.
void TreeTable::showPlaceholder(TreeTableRow *row, const VisibleNode &visible)
{
    row->setTreeState(m_treeColumn, visible.depth, false, false);
    for(int column=0; column < row->getCellCount(); ++column)
        row->setCellText(column, column == m_treeColumn?String(PlaceholderText):String());
    row->setPlaceholder(true);
    // a tree column changed under a row that already was one
    row->updateColors();
}

// The rows before the changed suffix keep their nodes, but one of them may be
// the node whose expansion changed: its expander shows the new state. The
// suffix's rows are bound again, which shows theirs.
void TreeTable::syncExpanders(size_t before)
{
    for(Base *control : getVirtualBody()->getRows())
    {
        const size_t item=getVirtualBody()->itemForRow(control);

        if(item == NoItem || item >= before || item >= m_visible.size())
            continue;
        if(TreeTableRow *row=gweni_cast<TreeTableRow>(control))
            showNode(row, m_visible[item]);
    }
}

void TreeTable::setTreeColumn(int column)
{
    if(column < 0 || column >= getColumnCount() || column == m_treeColumn)
        return;

    m_treeColumn=column;
    syncExpanders(m_visible.size());
}

void TreeTable::toggleExpanded(NodeId id)
{
    if(const Node *node=getNode(id))
        setExpanded(id, !node->expanded);
}

void TreeTable::rowDoubleClicked(TreeTableRow *row)
{
    const NodeId id=nodeForItem(itemForRow(row));
    const Node *node=getNode(id);

    if(!node)
        return;

    if(node->hasChildren)
        toggleExpanded(id);
    else
        activateNode(id);
}

void TreeTable::activateNode(NodeId id)
{
    event::Info info(this);

    info.integer=id;
    onNodeActivated.call(this, info);
}

const TreeTable::Node *TreeTable::getNode(NodeId id) const
{
    const auto found=m_nodeIndex.find(id);

    return found == m_nodeIndex.end()?nullptr:&m_nodes[found->second];
}

TreeTable::NodeId TreeTable::addNode(NodeId parentId, NodeId appId, bool hasChildren)
{
    const NodeId id=insertNode(parentId, NoItem, appId);

    if(id != NoNode && hasChildren)
    {
        m_nodes[m_nodeIndex.at(id)].hasChildren=true;
        refreshNode(id, false);
    }
    return id;
}

std::vector<TreeTable::NodeId> &TreeTable::childrenOf(NodeId parent)
{
    return parent == Root?m_roots:m_nodes[m_nodeIndex.at(parent)].children;
}

size_t TreeTable::insertionItem(NodeId parent, size_t index) const
{
    const Node *node=getNode(parent);
    const auto &children=parent == Root?m_roots:node->children;

    if(parent != Root && (!node->expanded || itemForNode(parent) == NoItem))
        return NoItem;

    if(index < children.size())
        return itemForNode(children[index]);
    if(parent == Root)
        return m_visible.size();
    return subtreeEnd(itemForNode(parent));
}

TreeTable::NodeId TreeTable::insertNode(NodeId parent, size_t index, NodeId appId)
{
    return insertNodes(parent, index, &appId, 1)?appId:NoNode;
}

bool TreeTable::insertNodes(NodeId parent, size_t index, const std::vector<NodeId> &ids)
{
    return insertNodes(parent, index, ids.data(), ids.size());
}

bool TreeTable::insertNodes(NodeId parent, size_t index, const NodeId *ids, size_t count)
{
    if(parent != Root && !getNode(parent))
        return false;

    // Validate the entire batch before modifying either the hierarchy or its
    // projection. The common one-node edit needs no duplicate-check set.
    std::unordered_set<NodeId> unique;

    for(size_t i=0; i < count; ++i)
    {
        const NodeId id=ids[i];

        if(id == NoNode || m_nodeIndex.count(id)
            || (count > 1 && !unique.insert(id).second))
            return false;
    }
    // The first insert under a loading node is the app's answer: its children
    // take the placeholder's place, or with none the expander goes.
    const bool answer=parent != Root && getNode(parent)->loading;
    size_t placeholder=NoItem;

    if(answer)
        placeholder=endLoading(m_nodes[m_nodeIndex.at(parent)], count == 0);
    if(count == 0)
        return true;

    index=std::min(index, childrenOf(parent).size());

    const bool priorEmpty=childrenOf(parent).empty();
    const size_t at=answer?placeholder:insertionItem(parent, index);
    const size_t replaced=answer?1:0;
    const size_t depth=parent == Root?0:getNode(parent)->depth+1;
    std::vector<VisibleNode> visible;

    if(at != NoItem && count > 1)
        visible.reserve(count);
    for(size_t i=0; i < count; ++i)
    {
        const NodeId id=ids[i];
        Node node;

        node.id=id;
        node.parent=parent;
        node.depth=depth;
        m_nodeIndex.emplace(id, m_nodes.size());
        m_nodes.push_back(std::move(node));
        if(at != NoItem && count > 1)
            visible.push_back({id, depth});
    }

    // The node store may have moved while the batch was appended.
    auto &siblings=childrenOf(parent);

    siblings.insert(siblings.begin()+index, ids, ids+count);
    if(parent != Root)
        m_nodes[m_nodeIndex.at(parent)].hasChildren=true;

    // Sorted: a group that had no children is sorted whole and shown as one
    // run (a lazy node's answer in its placeholder's place); otherwise each
    // node goes to its sorted place, a splice each.
    std::vector<NodeId> *order=isSorted()?currentSorted(parent):nullptr;

    if(isSorted() && priorEmpty)
    {
        m_sortedGroups.erase(parent);
        if(at != NoItem)
        {
            std::vector<VisibleNode> run;

            appendVisible(run, getShownChildren(parent));
            spliceVisible(at, replaced, run);
        }
    }
    else if(order)
    {
        for(size_t i=0; i < count; ++i)
        {
            const size_t pos=sortedPlace(parent, *order, ids[i]);

            order->insert(order->begin()+pos, ids[i]);
            if(at != NoItem)
            {
                const VisibleNode single{ids[i], depth};

                spliceVisible(shownChildItem(parent, pos+1), 0, &single, 1);
            }
        }
    }
    else if(at != NoItem)
    {
        if(count == 1)
        {
            const VisibleNode single{ids[0], depth};

            spliceVisible(at, replaced, &single, 1);
        }
        else
            spliceVisible(at, replaced, visible);
    }
    refreshNode(parent, false);
    return true;
}

// A parent whose last child went is a leaf: no expander, and not expanded
// (so Left and onCollapsed see a leaf). True when it was expanded, for the
// caller's onCollapsed once the edit is done. Its row is bound again by the
// caller's refresh.
bool TreeTable::collapseIfEmpty(NodeId parent)
{
    if(parent == Root)
        return false;

    Node &node=m_nodes[m_nodeIndex.at(parent)];

    if(!node.children.empty())
        return false;

    const bool wasExpanded=node.expanded;

    node.hasChildren=false;
    node.expanded=false;
    return wasExpanded;
}

bool TreeTable::isInSubtree(NodeId id, NodeId ancestor) const
{
    for(const Node *node=getNode(id); node; node=getNode(node->parent))
    {
        if(node->id == ancestor)
            return true;
    }
    return false;
}

bool TreeTable::removeNode(NodeId id)
{
    const Node *node=getNode(id);

    if(!node)
        return false;

    const NodeId parent=node->parent;
    auto &siblings=childrenOf(parent);
    const auto found=std::find(siblings.begin(), siblings.end(), id);
    // the siblings as shown: a removed selection takes the one next to it
    std::vector<NodeId> *order=currentSorted(parent);
    const std::vector<NodeId> &shown=order?*order:siblings;
    const size_t index=static_cast<size_t>(std::find(shown.begin(), shown.end(), id)-shown.begin());
    const bool selected=isInSubtree(m_selectedNode, id);

    if(selected)
        m_selectedNode=index+1 < shown.size()?shown[index+1]
            :index > 0?shown[index-1]:parent;
    if(order)
        order->erase(order->begin()+index);
    siblings.erase(found);

    const bool emptied=collapseIfEmpty(parent);

    const size_t item=itemForNode(id);

    if(item != NoItem)
        spliceVisible(item, subtreeEnd(item)-item, {});

    eraseRecords({id});
    syncVisibleSelection();
    refreshNode(parent, false);
    if(emptied)
        expansionEvent(parent, false);
    if(selected)
        selectionEvent();
    return true;
}

// Swap/pop just the removed subtrees' records, keeping every surviving id's
// store lookup current; neither a store rebuild nor a recursive walk.
void TreeTable::eraseRecords(std::vector<NodeId> pending)
{
    while(!pending.empty())
    {
        const NodeId next=pending.back();
        const size_t at=m_nodeIndex.at(next);

        pending.pop_back();
        const auto &children=m_nodes[at].children;

        pending.insert(pending.end(), children.begin(), children.end());
        if(at != m_nodes.size()-1)
        {
            m_nodes[at]=std::move(m_nodes.back());
            m_nodeIndex.at(m_nodes[at].id)=at;
        }
        m_nodes.pop_back();
        m_nodeIndex.erase(next);
        if(!m_sortedGroups.empty())
            m_sortedGroups.erase(next);
    }
}

void TreeTable::setHasChildren(NodeId id, bool hasChildren)
{
    const auto found=m_nodeIndex.find(id);

    if(found == m_nodeIndex.end())
        return;

    Node &node=m_nodes[found->second];

    // a node with children shows them whatever the app advertises
    if(!node.children.empty() || node.hasChildren == hasChildren)
        return;
    if(node.loading)
    {
        endLoading(node, true);
        return;
    }

    const bool wasExpanded=node.expanded;

    node.hasChildren=hasChildren;
    node.expanded=node.expanded && hasChildren;
    refreshNode(id, false);
    if(wasExpanded && !hasChildren)
        expansionEvent(id, false);
}

bool TreeTable::beginRequest(Node &node)
{
    if(!node.hasChildren || !node.children.empty() || node.childrenRequested)
        return false;

    node.childrenRequested=true;
    node.loading=true;
    return true;
}

void TreeTable::requestChildren(NodeId id)
{
    event::Info info(this);

    info.integer=id;
    onChildrenRequested.call(this, info);
}

// The placeholder is the row right after its shown, expanded node. An empty
// answer removes it and collapses the node, binding the node's row again for
// its expander; the reference is not used past the splice, whose binders may
// add nodes.
size_t TreeTable::endLoading(Node &node, bool empty)
{
    const NodeId id=node.id;
    size_t item=NoItem;

    node.loading=false;
    if(node.expanded)
    {
        const size_t at=itemForNode(id);

        if(at != NoItem)
            item=at+1;
    }
    if(!empty)
        return item;

    const bool wasExpanded=node.expanded;

    node.hasChildren=false;
    node.expanded=false;
    if(item != NoItem)
        spliceVisible(item, 1, {});
    refreshNode(id, false);
    if(wasExpanded)
        expansionEvent(id, false);
    return NoItem;
}

// One splice takes the node's visible subtree (its children or placeholder)
// away; then the records go, as a removeNode of each child would drop them.
void TreeTable::resetChildren(NodeId id)
{
    const auto found=m_nodeIndex.find(id);

    if(found == m_nodeIndex.end())
        return;

    const bool selectionMoved=m_selectedNode != id && isInSubtree(m_selectedNode, id);
    Node &node=m_nodes[found->second];
    const bool wasExpanded=node.expanded;
    const size_t item=wasExpanded?itemForNode(id):NoItem;
    std::vector<NodeId> children;

    children.swap(node.children);
    m_sortedGroups.erase(id);
    node.expanded=false;
    node.loading=false;
    node.childrenRequested=false;
    node.hasChildren=true;
    if(selectionMoved)
        m_selectedNode=id;
    if(item != NoItem)
    {
        const size_t end=subtreeEnd(item);

        if(end > item+1)
            spliceVisible(item+1, end-item-1, {});
    }
    eraseRecords(std::move(children));
    syncVisibleSelection();
    refreshNode(id, false);
    if(wasExpanded)
        expansionEvent(id, false);
    if(selectionMoved)
        selectionEvent();
}

bool TreeTable::moveNode(NodeId id, NodeId newParent, size_t index)
{
    const Node *node=getNode(id);

    if(!node || (newParent != Root && !getNode(newParent)) || isInSubtree(newParent, id))
        return false;

    const NodeId oldParent=node->parent;
    auto &old=childrenOf(oldParent);
    const auto found=std::find(old.begin(), old.end(), id);
    const size_t oldIndex=static_cast<size_t>(found-old.begin());
    const size_t finalSize=childrenOf(newParent).size()-(oldParent == newParent?1:0);

    index=std::min(index, finalSize);
    if(oldParent == newParent && oldIndex == index)
        return true;

    const Anchor anchor=captureAnchor();
    const size_t item=itemForNode(id);

    if(item != NoItem)
        spliceVisible(item, subtreeEnd(item)-item, {});
    if(std::vector<NodeId> *order=currentSorted(oldParent))
        order->erase(std::find(order->begin(), order->end(), id));
    old.erase(found);

    const bool emptied=collapseIfEmpty(oldParent);

    // sorted: the detach may have changed the old parent's key (a leaf now, a
    // child count dropped), so it is placed again before anything is searched
    // for among its siblings, the moved node among them if it goes there
    keepSorted(oldParent, false);

    // A move under a node still loading is its answer, as an insert is: the
    // moved subtree takes the placeholder's place (only after the detach,
    // which may have shifted it).
    const bool answer=newParent != Root && getNode(newParent)->loading;
    size_t at=answer?endLoading(m_nodes[m_nodeIndex.at(newParent)], false)
        :insertionItem(newParent, index);
    const size_t replaced=answer && at != NoItem?1:0;
    auto &siblings=childrenOf(newParent);

    siblings.insert(siblings.begin()+index, id);
    // sorted: shown at its sorted place, the app's index kept for the order
    // a sort's end gives back (a loading group had none to sort among)
    // (sorted again at once: the group is shown, and a later insert under it
    // needs its current order to find its place)
    if(answer)
    {
        m_sortedGroups.erase(newParent);
        if(isSorted())
            getShownChildren(newParent);
    }
    else if(std::vector<NodeId> *order=currentSorted(newParent))
    {
        const size_t pos=sortedPlace(newParent, *order, id);

        order->insert(order->begin()+pos, id);
        if(at != NoItem)
            at=shownChildItem(newParent, pos+1);
    }
    m_nodes[m_nodeIndex.at(id)].parent=newParent;
    if(newParent != Root)
        m_nodes[m_nodeIndex.at(newParent)].hasChildren=true;

    // A moved subtree's hidden nodes need the new depth too, so a later
    // expansion agrees with a tree built directly at the destination.
    std::vector<NodeId> pending{id};

    while(!pending.empty())
    {
        Node &next=m_nodes[m_nodeIndex.at(pending.back())];

        pending.pop_back();
        next.depth=next.parent == Root?0:getNode(next.parent)->depth+1;
        pending.insert(pending.end(), next.children.rbegin(), next.children.rend());
    }
    if(at != NoItem)
    {
        std::vector<VisibleNode> visible;

        appendVisible(visible, {id});
        spliceVisible(at, replaced, visible);
    }

    // insert/remove already anchor surviving rows. If the anchor itself was
    // in the moved subtree, follow its node at its new position as well.
    restoreAnchor(anchor);
    syncVisibleSelection();
    // the new parent's key changed with the insert: placed among siblings
    // whose keys are all current (the old parent's was placed above)
    if(oldParent != newParent)
        refreshNode(newParent, false);
    refreshNode(oldParent, false);
    if(emptied)
        expansionEvent(oldParent, false);
    return true;
}

void TreeTable::nodeChanged(NodeId id)
{
    refreshNode(id, true);
}

void TreeTable::refreshNode(NodeId id, bool verify)
{
    if(!getNode(id))
        return;
    // its sort key, or (folders first) whether it has children, may have
    // changed: its place among its siblings follows
    keepSorted(id, verify);

    // A hidden/offscreen node costs no layout and no binder call. In
    // particular, building children under an unseen parent never scans the
    // whole visible index just to discover that it has no row.
    for(Base *control : getVirtualBody()->getRows())
    {
        const size_t item=getVirtualBody()->itemForRow(control);

        if(item != NoItem && nodeForItem(item) == id)
        {
            rebindItem(item);
            if(Canvas *canvas=getCanvas())
                canvas->hoverMayHaveMoved();
            return;
        }
    }
}

///////////////////////////////////////////////////////////////////////////
// Sorting across the hierarchy

bool TreeTable::isSorted() const
{
    const int column=getSortColumn();

    return column >= 0 && static_cast<size_t>(column) < m_comparators.size()
        && m_comparators[column] && getSortDirection() != SortDirection::None;
}

void TreeTable::setSortComparator(int column, Comparator less)
{
    if(column < 0)
        return;
    if(static_cast<size_t>(column) >= m_comparators.size())
        m_comparators.resize(column+1);
    m_comparators[column]=std::move(less);
    if(column == getSortColumn())
        applySort();
}

void TreeTable::setFoldersFirst(bool foldersFirst)
{
    if(m_foldersFirst == foldersFirst)
        return;

    m_foldersFirst=foldersFirst;
    applySort();
}

void TreeTable::sortOrderChanged()
{
    applySort();
}

// Folders before leaves in either direction, then the column's order, its
// reverse when descending; std::stable_sort and sortedPlace keep ties in the
// app's order.
bool TreeTable::shownLess(NodeId a, NodeId b) const
{
    if(m_foldersFirst)
    {
        const bool folderA=getNode(a)->hasChildren;
        const bool folderB=getNode(b)->hasChildren;

        if(folderA != folderB)
            return folderA;
    }

    const Comparator &less=m_comparators[getSortColumn()];

    return getSortDirection() == SortDirection::Descending?less(b, a):less(a, b);
}

std::vector<TreeTable::NodeId> *TreeTable::currentSorted(NodeId parent)
{
    if(!isSorted())
        return nullptr;

    const auto found=m_sortedGroups.find(parent);

    return found != m_sortedGroups.end() && found->second.generation == m_sortGeneration?
        &found->second.order:nullptr;
}

// A stale group is sorted from the app's order the first time it is asked
// for, so a collapsed subtree costs nothing until it is shown.
const std::vector<TreeTable::NodeId> &TreeTable::getShownChildren(NodeId parent)
{
    static const std::vector<NodeId> none;

    if(parent != Root && !getNode(parent))
        return none;
    if(!isSorted())
        return childrenOf(parent);
    if(std::vector<NodeId> *order=currentSorted(parent))
        return *order;

    // (map references stay valid as other groups are added)
    SortedGroup &group=m_sortedGroups[parent];

    group.order=childrenOf(parent);
    std::stable_sort(group.order.begin(), group.order.end(), [this](NodeId a, NodeId b) { return shownLess(a, b); });
    group.generation=m_sortGeneration;
    return group.order;
}

// Where stable_sort would put it: after the nodes that sort before it and
// the equal ones before it in the app's order. A scan of the equal run only.
size_t TreeTable::sortedPlace(NodeId parent, const std::vector<NodeId> &order, NodeId id) const
{
    const auto less=[this](NodeId a, NodeId b) { return shownLess(a, b); };
    auto at=std::lower_bound(order.begin(), order.end(), id, less);
    const auto end=std::upper_bound(at, order.end(), id, less);

    if(at != end)
    {
        const std::vector<NodeId> &app=parent == Root?m_roots:getNode(parent)->children;
        const auto appIndex=[&app](NodeId node) { return std::find(app.begin(), app.end(), node)-app.begin(); };
        const auto own=appIndex(id);

        while(at != end && appIndex(*at) < own)
            ++at;
    }
    return static_cast<size_t>(at-order.begin());
}

size_t TreeTable::shownChildItem(NodeId parent, size_t pos)
{
    const std::vector<NodeId> &shown=getShownChildren(parent);

    if(pos < shown.size())
        return itemForNode(shown[pos]);
    return parent == Root?m_visible.size():subtreeEnd(itemForNode(parent));
}

TreeTable::Anchor TreeTable::captureAnchor()
{
    Anchor anchor;
    const int offset=sourceScrollOffset();

    anchor.node=nodeForItem(getVirtualBody()->firstVisibleItem());
    anchor.into=offset%getRowHeight();
    return anchor;
}

void TreeTable::restoreAnchor(const Anchor &anchor)
{
    const size_t item=itemForNode(anchor.node);

    if(item == NoItem)
        return;

    VirtualList *body=getVirtualBody();

    // a scrollToItem() not yet laid out is where the app wants the view
    if(body->getPendingItem() != NoItem)
        return;

    const size_t before=body->firstVisibleItem();
    const uint64_t target=static_cast<uint64_t>(item)*getRowHeight()+anchor.into;
    const int offset=static_cast<int>(std::min<uint64_t>(target, INT_MAX));

    if(offset == body->getVerticalScrollOffset())
        return;
    body->setVerticalScrollOffset(offset);
    m_stripePhase^=(before^body->firstVisibleItem())%2;
}

// Only a group sorted in this generation holds the node; a stale one is
// sorted whole when it is next shown.
void TreeTable::keepSorted(NodeId id, bool verify)
{
    const Node *node=getNode(id);

    if(!node)
        return;

    const NodeId parent=node->parent;
    std::vector<NodeId> *order=currentSorted(parent);

    if(!order)
        return;

    const auto found=std::find(order->begin(), order->end(), id);

    if(found == order->end())
        return;

    const size_t from=static_cast<size_t>(found-order->begin());

    order->erase(found);
    // a binary search needs the others in order: siblings renamed before
    // their own nodeChanged are not, so the group is sorted again whole
    if(verify && !std::is_sorted(order->begin(), order->end(),
        [this](NodeId a, NodeId b) { return shownLess(a, b); }))
    {
        resortGroup(parent);
        return;
    }

    const size_t to=sortedPlace(parent, *order, id);

    order->insert(order->begin()+to, id);
    if(to == from)
        return;

    const size_t item=itemForNode(id);

    if(item == NoItem)
        return;

    const Anchor anchor=captureAnchor();
    const size_t end=subtreeEnd(item);
    const std::vector<VisibleNode> moved(m_visible.begin()+item, m_visible.begin()+end);

    spliceVisible(item, end-item, {});
    spliceVisible(shownChildItem(parent, to+1), 0, moved);
    restoreAnchor(anchor);
    syncVisibleSelection();
}

// A hidden group is only made stale: it is sorted when next shown. A shown
// one keeps its entries, so its run is rewritten in place, no item count or
// row shifted, and only the rows whose node changed are bound again.
void TreeTable::resortGroup(NodeId parent)
{
    m_sortedGroups.erase(parent);

    size_t start=0;
    size_t end=m_visible.size();

    if(parent != Root)
    {
        const Node *node=getNode(parent);
        const size_t item=itemForNode(parent);

        if(!node || !node->expanded || node->loading || item == NoItem)
            return;
        start=item+1;
        end=subtreeEnd(item);
    }

    std::vector<VisibleNode> run;

    run.reserve(end-start);
    appendVisible(run, getShownChildren(parent));
    if(run.size() != end-start)
        return;

    const Anchor anchor=captureAnchor();
    std::vector<size_t> changed;

    for(Base *control : getVirtualBody()->getRows())
    {
        const size_t item=getVirtualBody()->itemForRow(control);

        if(item != NoItem && item >= start && item < end && m_visible[item].id != run[item-start].id)
            changed.push_back(item);
    }
    std::copy(run.begin(), run.end(), m_visible.begin()+start);
    for(size_t item : changed)
        rebindItem(item);
    restoreAnchor(anchor);
    syncVisibleSelection();
    if(!changed.empty())
    {
        if(Canvas *canvas=getCanvas())
            canvas->hoverMayHaveMoved();
    }
}

// A new generation makes every group stale; the shown ones are sorted as
// the projection is built again, which binds the pooled rows from the first
// item whose node changed on (those above it keep their binding). The
// first shown node goes back to the top and the selection,
// shown or not, stays on its node.
void TreeTable::applySort()
{
    ++m_sortGeneration;
    // every group is stale now; with no sort none is needed again
    if(!isSorted())
        m_sortedGroups.clear();
    if(!isSorted() && !m_projectionSorted)
        return;
    m_projectionSorted=isSorted();

    const Anchor anchor=captureAnchor();
    std::vector<VisibleNode> visible;

    visible.reserve(m_visible.size());
    appendVisible(visible, getShownChildren(Root));

    size_t first=0;

    while(first < visible.size() && first < m_visible.size() && visible[first].id == m_visible[first].id)
        ++first;
    if(first == visible.size() && first == m_visible.size())
        return;

    m_visible.swap(visible);

    bool rebound=false;

    for(Base *control : getVirtualBody()->getRows())
    {
        const size_t item=getVirtualBody()->itemForRow(control);

        if(item != NoItem && item >= first && item < m_visible.size())
        {
            rebindItem(item);
            rebound=true;
        }
    }
    // the anchor first: the stripe phase it moves is what the sync applies
    restoreAnchor(anchor);
    syncVisibleSelection();
    if(rebound)
    {
        if(Canvas *canvas=getCanvas())
            canvas->hoverMayHaveMoved();
    }
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
    if(!getNode(id))
        return NoItem;

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

void TreeTable::syncVisibleSelection()
{
    syncSourceRows(itemForNode(m_selectedNode), m_stripePhase);
}

void TreeTable::spliceVisible(size_t at, size_t removed, const std::vector<VisibleNode> &added)
{
    spliceVisible(at, removed, added.data(), added.size());
}

void TreeTable::spliceVisible(size_t at, size_t removed, const VisibleNode *added, size_t count)
{
    if(removed == 0 && count == 0)
        return;

    sourceScrollOffset();
    VirtualList *body=getVirtualBody();
    const size_t top=body->firstVisibleItem();

    m_visible.erase(m_visible.begin()+at, m_visible.begin()+at+removed);
    if(count > 0)
        m_visible.insert(m_visible.begin()+at, added, added+count);
    // A range replaced with the top row inside it (a placeholder answered)
    // shows its first new row at the top: inserted after the range, below
    // the top, and the range's removal then puts its start there. Otherwise
    // an insert at or above the top moves the view down with what it shows.
    if(removed > 0 && count > 0 && top >= at && top < at+removed)
    {
        body->insertItems(at+removed, count);
        body->removeItems(at, removed);
    }
    else
    {
        body->removeItems(at, removed);
        body->insertItems(at, count);
    }
    m_stripePhase^=(top^body->firstVisibleItem())%2;
    syncVisibleSelection();
}

// Iterative preorder, so a hierarchy deeper than the process stack still
// works. Only expanded descendants are visited, in the app's child order.
void TreeTable::appendVisible(std::vector<VisibleNode> &into,
    const std::vector<NodeId> &children)
{
    std::vector<NodeId> pending(children.rbegin(), children.rend());

    while(!pending.empty())
    {
        const NodeId id=pending.back();
        const Node &node=m_nodes[m_nodeIndex.at(id)];

        pending.pop_back();
        into.push_back({id, node.depth});
        if(node.expanded && node.loading)
            into.push_back({NoNode, node.depth+1});
        else if(node.expanded)
        {
            const std::vector<NodeId> &shown=getShownChildren(id);

            pending.insert(pending.end(), shown.rbegin(), shown.rend());
        }
    }
}

void TreeTable::appendSubtree(std::vector<VisibleNode> &into, NodeId id)
{
    const Node *node=getNode(id);

    if(!node)
        return;
    if(node->loading)
        into.push_back({NoNode, node->depth+1});
    else
        appendVisible(into, getShownChildren(id));
}

// The item count takes care of rows newly shown or hidden by a changed range.
// Pooled rows still showing an index in the suffix now show another node:
// refresh those alone. Rows before the insertion/removal keep their content.
// A row bound again in place may show or hide its expander under a resting
// mouse, so the hover is hit-tested again after the layout.
// Bulk expansion can replace the whole projection. Unlike a splice, its
// unchanged prefix keeps its indexes and only the suffix is bound again.
void TreeTable::visibleChanged(size_t firstChanged, NodeId selected)
{
    getVirtualBody()->setItemCount(m_visible.size());
    m_selectedNode=selected;
    syncSourceRows(itemForNode(selected), m_stripePhase);

    bool rebound=false;

    for(Base *control : getVirtualBody()->getRows())
    {
        const size_t item=getVirtualBody()->itemForRow(control);

        if(item != NoItem && item >= firstChanged && item < m_visible.size())
        {
            rebindItem(item);
            rebound=true;
        }
    }

    if(rebound)
    {
        if(Canvas *canvas=getCanvas())
            canvas->hoverMayHaveMoved();
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

    if(expanded && beginRequest(m_nodes[found->second]))
    {
        // Asked before the node expands, so an answer in the handler expands
        // it with its children, and an empty one leaves it a collapsed leaf.
        requestChildren(id);

        const Node *now=getNode(id);

        if(!now || now->expanded || !now->hasChildren)
            return;
    }

    Node &node=m_nodes[m_nodeIndex.at(id)];

    if(node.expanded == expanded || (expanded && !node.hasChildren))
        return;
    node.expanded=expanded;

    const size_t item=itemForNode(id);

    bool selectionMoved=false;

    if(item != NoItem)
    {
        syncExpanders(item+1);

        const size_t at=item+1;
        if(expanded)
        {
            std::vector<VisibleNode> children;

            appendSubtree(children, id);
            if(!children.empty())
            {
                spliceVisible(at, 0, children);
            }
        }
        else
        {
            const size_t end=subtreeEnd(item);

            if(end > at)
            {
                // a selected descendant hides: the collapsed node takes it
                if(m_selectedNode != id && isInSubtree(m_selectedNode, id))
                {
                    m_selectedNode=id;
                    selectionMoved=true;
                }
                spliceVisible(at, end-at, {});
            }
        }
    }
    expansionEvent(id, expanded);
    if(selectionMoved)
        selectionEvent();
}

// A bulk change builds the projection once and binds only its changed suffix,
// instead of performing one vector insert and one invalidation per parent.
void TreeTable::setAllExpanded(bool expanded)
{
    std::vector<NodeId> changed;
    std::vector<NodeId> requests;

    for(Node &node : m_nodes)
    {
        if(node.expanded != expanded && (!expanded || node.hasChildren))
        {
            if(expanded && beginRequest(node))
                requests.push_back(node.id);
            node.expanded=expanded;
            changed.push_back(node.id);
        }
    }
    if(changed.empty())
        return;

    std::vector<VisibleNode> visible;

    visible.reserve(expanded?m_nodes.size():m_roots.size());
    appendVisible(visible, getShownChildren(Root));

    size_t first=0;

    while(first < visible.size() && first < m_visible.size()
        && visible[first].id == m_visible[first].id)
        ++first;

    // A shown selection a collapse hides goes to the node shown in its place;
    // one hidden already (moved under a collapsed node) stays on its node.
    const NodeId selected=m_selectedNode;
    const bool wasShown=selected != NoNode && itemForNode(selected) != NoItem;
    const NodeId shown=wasShown?shownAncestor(selected):selected;

    if(first < visible.size() || first < m_visible.size())
    {
        m_visible.swap(visible);
        visibleChanged(first, shown);
    }
    else
        m_selectedNode=shown;
    syncExpanders(first);

    for(NodeId id : changed)
        expansionEvent(id, expanded);
    if(m_selectedNode != selected)
        selectionEvent();
    // a bulk expansion shows the placeholders first, then asks
    for(NodeId id : requests)
    {
        if(getNode(id))
            requestChildren(id);
    }
}

TreeTable::NodeId TreeTable::shownAncestor(NodeId id) const
{
    NodeId shown=id;

    for(const Node *node=getNode(id); node && node->parent != Root;)
    {
        node=getNode(node->parent);
        if(node && !node->expanded)
            shown=node->id;
    }
    return shown;
}

// Every node under it with children expanded, then its visible subtree built
// once and put in place of the one it had.
void TreeTable::expandSubtree(NodeId id)
{
    if(!getNode(id))
        return;

    std::vector<NodeId> changed;
    std::vector<NodeId> requests;
    std::vector<NodeId> pending{id};

    while(!pending.empty())
    {
        Node &node=m_nodes[m_nodeIndex.at(pending.back())];

        pending.pop_back();
        if(node.hasChildren && !node.expanded)
        {
            if(beginRequest(node))
                requests.push_back(node.id);
            node.expanded=true;
            changed.push_back(node.id);
        }
        pending.insert(pending.end(), node.children.rbegin(), node.children.rend());
    }
    if(changed.empty())
        return;

    const size_t item=itemForNode(id);

    if(item != NoItem)
    {
        syncExpanders(item+1);

        const size_t at=item+1;
        const size_t end=subtreeEnd(item);
        std::vector<VisibleNode> children;

        appendSubtree(children, id);

        // the subtree's shown rows go out and come back with the rest: a
        // first shown row among them stays at the top
        const Anchor anchor=captureAnchor();

        spliceVisible(at, end-at, children);
        restoreAnchor(anchor);
        syncVisibleSelection();
    }

    for(NodeId changedId : changed)
        expansionEvent(changedId, true);
    for(NodeId requested : requests)
    {
        if(getNode(requested))
            requestChildren(requested);
    }
}

bool TreeTable::setSelectedNode(NodeId id)
{
    if(id == NoNode)
    {
        DataTable::setSelectedItem(NoItem, false);
        m_selectedNode=NoNode;
        return true;
    }

    const size_t item=itemForNode(id);

    if(item == NoItem)
        return false;

    DataTable::setSelectedItem(item, false);
    m_selectedNode=id;
    return true;
}

// The item is the visible index: the node it shows is the selection.
void TreeTable::selectedItemChanged(DataTableRow * /*row*/, size_t item, bool doEvents)
{
    // A placeholder is no node: a key moving down passes over it to the row
    // after it, anything else (up, End onto it) lands on its loading node,
    // the row before it. The row after a placeholder is always a node.
    if(item < m_visible.size() && m_visible[item].id == NoNode)
    {
        const size_t from=itemForNode(m_selectedNode);
        const bool down=from != NoItem && from < item && item+1 < m_visible.size();
        const size_t to=down?item+1:item-1;

        DataTable::setSelectedItem(to, doEvents && nodeForItem(to) != m_selectedNode);
        return;
    }

    m_selectedNode=nodeForItem(item);

    if(doEvents)
        selectionEvent();
}

void TreeTable::selectionEvent()
{
    event::Info info(this);

    info.control=rowForItem(getSelectedItem());
    info.integer=m_selectedNode;
    onSelectionChange.call(this, info);
}

// The tree keys are the table's own: from a focused control in a cell (a
// TextBox) they go on to its parents, as a Selectable row's Return does.
bool TreeTable::onKeyRight(bool down)
{
    const Node *node=getNode(m_selectedNode);

    if(!node || KeyboardFocus != this)
        return false;
    if(!down)
        return true;

    if(node->hasChildren && !node->expanded)
        setExpanded(node->id, true);
    else if(node->expanded && !node->children.empty() && getSelectedItem() != NoItem)
        moveSelectionTo(getSelectedItem()+1);
    return true;
}

// The parent is the nearest row above at a lesser depth.
bool TreeTable::onKeyLeft(bool down)
{
    const Node *node=getNode(m_selectedNode);

    if(!node || KeyboardFocus != this)
        return false;
    if(!down)
        return true;

    if(node->expanded)
        setExpanded(node->id, false);
    else if(node->parent != Root && getSelectedItem() != NoItem)
    {
        // (a hidden selection, moved under a collapsed node, has no row)
        size_t item=getSelectedItem();

        while(item > 0 && m_visible[item].depth >= node->depth)
            --item;
        moveSelectionTo(item);
    }
    return true;
}

bool TreeTable::onKeyReturn(bool down)
{
    if(m_selectedNode == NoNode || KeyboardFocus != this)
        return false;
    if(down)
        activateNode(m_selectedNode);
    return true;
}

bool TreeTable::onChar(UnicodeChar c)
{
    if(c != '*' || m_selectedNode == NoNode || KeyboardFocus != this)
        return ParentClass::onChar(c);

    expandSubtree(m_selectedNode);
    return true;
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
