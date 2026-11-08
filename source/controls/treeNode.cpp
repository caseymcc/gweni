/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/treeNode.h>
#include <gweni/controls/treeControl.h>
#include <gweni/controls/canvas.h>
#include <gweni/utility.h>

namespace gweni
{
namespace controls
{

class TreeNodeText: public Button
{
    GWENI_CONTROL_INLINE(TreeNodeText, Button)
    {
        // Button centres its text; a tree label starts next to its expander.
        setTextAlignment(Alignment::Left|Alignment::CenterV);
        setShouldDrawBackground(false);
        setHeight(16);
    }

public:
    void updateColors() override
    {
        if(isDisabled())
            return setTextColor(getSkin()->Colors.Button.Disabled);

        if(isDepressed() || getToggleState())
            return setTextColor(getSkin()->Colors.Tree.Selected);

        if(isHovered())
            return setTextColor(getSkin()->Colors.Tree.Hover);

        setTextColor(getSkin()->Colors.Tree.Normal);
    }

};


const int TreeIndentation=14;

GWENI_CONTROL_CONSTRUCTOR(TreeNode)
{
    m_treeControl=nullptr;

    m_toggleButton=newChild<OpenToggleButton>();
    // Placed by this node: sized in arrange(), centred on the title in
    // postLayout().
    m_toggleButton->setPlacement(Placement::Owner);
    m_toggleButton->onToggleCaller.add(this, &TreeNode::onToggleButtonPress);

    m_title=newChild<TreeNodeText>();
    m_title->setDock(DockPosition::Top);
    m_title->setMargin(Margin(16, 0, 0, 0));
    m_title->onDoubleClickCaller.add(this, &TreeNode::onDoubleClickName);
    m_title->onDownCaller.add(this, &TreeNode::onClickName);
    m_title->onRightPressCaller.add(this, &TreeNode::onRightPress);

    // The indent is the panel's padding, not its margin: the panel then spans
    // the node's width, and a child's stub, which reaches left into the indent
    // to this node's line, is inside the region the panel clips its children to.
    m_innerPanel=newChild<Base>();
    m_innerPanel->setDock(DockPosition::Top);
    m_innerPanel->setHeight(100);
    m_innerPanel->setMargin(Margin(0, 1, 0, 0));
    m_innerPanel->setPadding(Padding(TreeIndentation, 0, 0, 0));
    m_innerPanel->hide();
    m_root=false;
    m_selected=false;
    m_selectable=true;
}

const Rect &TreeNode::getToggleBounds() const
{
    static const Rect none;

    return m_toggleButton?m_toggleButton->getBounds():none;
}

const Rect &TreeNode::getTitleBounds() const
{
    static const Rect none;

    return m_title?m_title->getBounds():none;
}

int TreeNode::lineX() const
{
    const Rect &toggle=getToggleBounds();

    return toggle.x+toggle.w/2;
}

int TreeNode::stubY() const
{
    const Rect &toggle=getToggleBounds();

    return toggle.y+toggle.h/2;
}

int TreeNode::childIndent() const
{
    return m_innerPanel->getX()+m_innerPanel->getPadding().left;
}

const TreeNode *TreeNode::lastChildNode() const
{
    const Base::List &children=m_innerPanel->getChildren();

    for(auto it=children.rbegin(); it != children.rend(); ++it)
    {
        const TreeNode *node=gweni_cast<TreeNode>(*it);

        if(node && !node->hidden())
            return node;
    }
    return nullptr;
}

TreeNode *TreeNode::addNode(const String &label)
{
    TreeNode *node=newChild<TreeNode>();
    node->setText(label);
    node->setDock(DockPosition::Top);
    node->setRoot(gweni_cast<TreeControl>(this) != nullptr);
    node->setTreeControl(m_treeControl);

    if(m_treeControl)
        m_treeControl->onNodeAdded(node);

    return node;
}

// A node with no child nodes shows neither its toggle nor its inner panel.
// Settled before the pass measures this node, so the sizes it takes count
// them as they will be drawn; nothing has read them yet.
void TreeNode::calculateSize(skin::Base *skin, Dim dim)
{
    if(dim == Dim::X && m_toggleButton)
    {
        PlacementScope placement;
        const bool hadToggle=hasToggle();

        if(m_innerPanel->numChildren() == 0)
        {
            m_toggleButton->hide();
            m_toggleButton->setToggleState(false);
            m_innerPanel->hide();
        }
        else
            m_toggleButton->show();

        // The skin's stub ends at the toggle, or at the title without one, and a
        // node without one draws no line down: neither need change its size.
        if(hasToggle() != hadToggle)
            addStateChange(StateChange_Geometry);
    }

    ParentClass::calculateSize(skin, dim);
}

// The toggle is a ToggleSize square, given along each axis before this node
// arranges it.
void TreeNode::arrange(skin::Base *skin, Dim dim)
{
    if(m_toggleButton && !m_toggleButton->hidden())
    {
        if(dim == Dim::X)
            placeChildSize(m_toggleButton, ToggleSize, m_toggleButton->getHeight());
        else
            placeChildSize(m_toggleButton, m_toggleButton->getWidth(), ToggleSize);
    }

    ParentClass::arrange(skin, dim);
}

// The title and the inner panel are docked Top, so this node's height is
// their preferred heights, which its parent gives it.
void TreeNode::layout(skin::Base *skin)
{
    ParentClass::layout(skin);
}

// The toggle is centred on the title once the title has its height. Only its
// position changes, which nothing laid out in this pass has read.
void TreeNode::postLayout(skin::Base * /*skin*/)
{
    if(m_toggleButton && m_title)
        placeChildPos(m_toggleButton, 0, (m_title->getHeight()-m_toggleButton->getHeight()) / 2);
}

void TreeNode::setText(const String &text)
{
    m_title->setText(text);
}

const String &TreeNode::getText()
{
    return m_title->getText();
}

void TreeNode::setImage(const String &text)
{
    m_title->setImage(text);
}

void TreeNode::open()
{
    if(m_innerPanel->hidden())
        childRowsMoved();

    m_innerPanel->show();

    if(m_toggleButton)
        m_toggleButton->setToggleState(true);

    invalidate();
}

void TreeNode::close()
{
    if(!m_innerPanel->hidden())
        childRowsMoved();

    m_innerPanel->hide();

    if(m_toggleButton)
        m_toggleButton->setToggleState(false);

    invalidate();
}

// Showing or hiding the child rows moves every row below them, and a click on
// the toggle is hit-tested again before the layout (the capture ends with the
// release), against the rows where they were: the hover goes to what is under
// the mouse once the layout is done.
void TreeNode::childRowsMoved()
{
    if(Canvas *canvas=getCanvas())
        canvas->hoverMayHaveMoved();
}

void TreeNode::expandAll()
{
    open();
    Base::List &children=GetChildNodes();

    for(auto &&childnode : children)
    {
        TreeNode *child=gweni_cast<TreeNode>(childnode);

        if(!child)
            continue;

        child->expandAll();
    }
}

Button *TreeNode::GetButton()
{
    return m_title;
}

void TreeNode::onToggleButtonPress(event::Info)
{
    if(m_toggleButton->getToggleState())
        open();
    else
        close();
}

void TreeNode::onDoubleClickName(event::Info)
{
    if(!m_toggleButton->visible())
        return;

    m_toggleButton->toggle();
}

void TreeNode::onClickName(event::Info)
{
    onNamePressCaller.call(this);
    setSelected(!isSelected());
}

void TreeNode::onRightPress(event::Info)
{
    onRightPressCaller.call(this);
}

void TreeNode::setSelected(bool b, bool FireEvents)
{
    if(!m_selectable)
        return;

    if(m_selected == b)
        return;

    m_selected=b;

    if(m_title)
        m_title->setToggleState(m_selected);

    if(FireEvents)
    {
        onSelectChangeCaller.call(this);

        if(m_selected)
            onSelectCaller.call(this);
        else
            onUnselectCaller.call(this);
    }

    redraw();
}

void TreeNode::deselectAll()
{
    m_selected=false;

    if(m_title)
        m_title->setToggleState(m_selected);

    Base::List &children=GetChildNodes();

    for(auto &&childnode : children)
    {
        TreeNode *child=gweni_cast<TreeNode>(childnode);

        if(!child)
            continue;

        child->deselectAll();
    }
}

controls::Base::List &TreeNode::GetChildNodes()
{
    return m_innerPanel->getChildren();
}

}//namespace controls
}//namespace gweni
