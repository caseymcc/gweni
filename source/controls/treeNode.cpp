/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/treeNode.h>
#include <gweni/controls/treeControl.h>
#include <gweni/utility.h>

namespace gweni
{
namespace controls
{

class OpenToggleButton: public Button
{
    GWENI_CONTROL_INLINE(OpenToggleButton, Button)
    {
        setIsToggle(true);
        setTabable(false);
    }

    void renderFocus(skin::Base * /*skin*/) override
    {}

    void render(skin::Base *skin) override
    {
//        skin->drawTreeButton(this, skin::Generate, getToggleState());
        skin->drawControl(this);
    }

};


class TreeNodeText: public Button
{
    GWENI_CONTROL_INLINE(TreeNodeText, Button)
    {
        setAlignment(Position::Left|Position::CenterV);
        setShouldDrawBackground(false);
        setHeight(16);
    }

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
    m_toggleButton=new OpenToggleButton(this);
    m_toggleButton->setBounds(0, 0, 15, 15);
    m_toggleButton->onToggleCaller.add(this, &TreeNode::onToggleButtonPress);
    m_title=new TreeNodeText(this);
    m_title->dock(Position::Top);
    m_title->setMargin(Margin(16, 0, 0, 0));
    m_title->onDoubleClickCaller.add(this, &TreeNode::onDoubleClickName);
    m_title->onDownCaller.add(this, &TreeNode::onClickName);
    m_title->onRightPressCaller.add(this, &TreeNode::onRightPress);
    m_innerPanel=new Base(this);
    m_innerPanel->dock(Position::Top);
    m_innerPanel->setHeight(100);
    m_innerPanel->setMargin(Margin(TreeIndentation, 1, 0, 0));
    m_innerPanel->hide();
    m_root=false;
    m_selected=false;
    m_selectable=true;
}

void TreeNode::render(skin::Base *skin)
{
    int iBottom=0;

    if(m_innerPanel->getChildren().size() > 0)
        iBottom=m_innerPanel->getChildren().back()->getY() + m_innerPanel->getY();

    //skin->drawTreeNode(this, skin::Generate,
    //    m_innerPanel->visible(),
    //    isSelected(),
    //    m_title->getHeight(),
    //    m_title->textRight(),
    //    m_toggleButton->getY() + m_toggleButton->getHeight()/2,
    //    iBottom,
    //    getParent() == m_treeControl);
    skin->drawControl(this);
}

TreeNode *TreeNode::addNode(const String &label)
{
    TreeNode *node=new TreeNode(this);
    node->setText(label);
    node->dock(Position::Top);
    node->setRoot(gweni_cast<TreeControl>(this) != nullptr);
    node->setTreeControl(m_treeControl);

    if(m_treeControl)
        m_treeControl->onNodeAdded(node);

    return node;
}

void TreeNode::layout(skin::Base *skin)
{
    if(m_toggleButton)
    {
        if(m_title)
            m_toggleButton->setPos(0, (m_title->getHeight()-m_toggleButton->getHeight()) / 2);

        if(m_innerPanel->numChildren() == 0)
        {
            m_toggleButton->hide();
            m_toggleButton->setToggleState(false);
            m_innerPanel->hide();
        }
        else
        {
            m_toggleButton->show();
            m_innerPanel->sizeToChildren(false, true);
        }
    }

    ParentClass::layout(skin);
}

void TreeNode::postLayout(skin::Base * /*skin*/)
{
    if(sizeToChildren(false, true))
        invalidateParent();
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
    m_innerPanel->show();

    if(m_toggleButton)
        m_toggleButton->setToggleState(true);

    invalidate();
}

void TreeNode::close()
{
    m_innerPanel->hide();

    if(m_toggleButton)
        m_toggleButton->setToggleState(false);

    invalidate();
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
