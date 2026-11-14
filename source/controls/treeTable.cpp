/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/controls/treeTable.h>
#include <gweni/controls/canvas.h>

#include <algorithm>
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

            if(TreeTableRow *treeRow=gweni_cast<TreeTableRow>(row))
                showNode(treeRow, node);
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
    const Node *node=getNode(visible.id);

    row->setTreeState(m_treeColumn, visible.depth, node && node->hasChildren, node && node->expanded);
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

    const size_t selected=getSelectedItem();

    visibleChanged(at, (selected != NoItem && selected >= at)?selected+1:selected);
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
// A row bound again in place may show or hide its expander under a resting
// mouse, so the hover is hit-tested again after the layout.
// The selection's item is set on both sides of the count, so the count never
// finds it past its end (which would drop it, with an event): while the old
// count holds it, and again after (the item past the old count, or none).
void TreeTable::visibleChanged(size_t firstChanged, size_t selected)
{
    if(selected < getItemCount())
        DataTable::setSelectedItem(selected, false);
    DataTable::setItemCount(m_visible.size());
    DataTable::setSelectedItem(selected, false);

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

    Node &node=m_nodes[found->second];

    if(node.expanded == expanded || (expanded && !node.hasChildren))
        return;
    node.expanded=expanded;

    const size_t item=itemForNode(id);

    bool selectionMoved=false;

    if(item != NoItem)
    {
        syncExpanders(item+1);

        const size_t at=item+1;
        const size_t selected=getSelectedItem();

        if(expanded)
        {
            std::vector<VisibleNode> children;

            appendVisible(children, node.children);
            if(!children.empty())
            {
                const size_t count=children.size();

                m_visible.insert(m_visible.begin()+at, children.begin(), children.end());
                visibleChanged(at, (selected != NoItem && selected >= at)?selected+count:selected);
            }
        }
        else
        {
            const size_t end=subtreeEnd(item);

            if(end > at)
            {
                size_t moved=selected;

                // a selected descendant hides: the collapsed node takes it
                if(selected != NoItem && selected >= at && selected < end)
                {
                    moved=item;
                    selectionMoved=true;
                }
                else if(selected != NoItem && selected >= end)
                    moved=selected-(end-at);

                m_visible.erase(m_visible.begin()+at, m_visible.begin()+end);
                visibleChanged(at, moved);
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

    const NodeId selected=m_selectedNode;
    const NodeId shown=(selected == NoNode)?NoNode:shownAncestor(selected);

    if(first < visible.size() || first < m_visible.size())
    {
        m_visible.swap(visible);
        visibleChanged(first, (shown == NoNode)?NoItem:itemForNode(shown));
    }
    syncExpanders(first);

    for(NodeId id : changed)
        expansionEvent(id, expanded);
    if(shown != selected)
        selectionEvent();
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
    std::vector<NodeId> pending{id};

    while(!pending.empty())
    {
        Node &node=m_nodes[m_nodeIndex.at(pending.back())];

        pending.pop_back();
        if(node.hasChildren && !node.expanded)
        {
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
        const NodeId selectedNode=m_selectedNode;
        size_t selected=getSelectedItem();
        std::vector<VisibleNode> children;

        appendVisible(children, getNode(id)->children);
        m_visible.erase(m_visible.begin()+at, m_visible.begin()+end);
        m_visible.insert(m_visible.begin()+at, children.begin(), children.end());

        // a node shown before stays shown: one after the subtree shifts, one
        // inside it is found in the new rows
        if(selected != NoItem && selected >= end)
            selected=selected-(end-at)+children.size();
        else if(selected != NoItem && selected >= at)
        {
            for(size_t i=0; i < children.size(); ++i)
            {
                if(children[i].id == selectedNode)
                    selected=at+i;
            }
        }
        visibleChanged(at, selected);
    }

    for(NodeId changedId : changed)
        expansionEvent(changedId, true);
}

bool TreeTable::setSelectedNode(NodeId id)
{
    if(id == NoNode)
    {
        DataTable::setSelectedItem(NoItem, false);
        return true;
    }

    const size_t item=itemForNode(id);

    if(item == NoItem)
        return false;

    DataTable::setSelectedItem(item, false);
    return true;
}

// The item is the visible index: the node it shows is the selection.
void TreeTable::selectedItemChanged(DataTableRow * /*row*/, size_t item, bool doEvents)
{
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
    else if(node->expanded && !node->children.empty())
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
    else if(node->parent != Root)
    {
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
