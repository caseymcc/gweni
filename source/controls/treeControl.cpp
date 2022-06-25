/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/treeControl.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/utility.h>
#include <gweni/inputHandler.h>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(TreeControl)
{
    m_treeControl=this;
    m_toggleButton->delayedDelete();
    m_toggleButton=nullptr;
    m_title->delayedDelete();
    m_title=nullptr;
    m_innerPanel->delayedDelete();
    m_innerPanel=nullptr;
    m_allowMultipleSelection=false;

    m_scrollControl=newChild<ScrollControl>();
    m_scrollControl->setDock(DockPosition::Center);
    m_scrollControl->setScroll(false, true);
    m_scrollControl->setAutoHideBars(true);
    m_scrollControl->setMargin(Margin(1, 1, 1, 1));
    m_innerPanel=m_scrollControl;
    m_scrollControl->setInnerSize(1000, 1000);
}

//void TreeControl::render(skin::Base *skin)
//{
//    if(shouldDrawBackground())
////        skin->drawTreeControl(this, skin::Generate);
//        skin->drawControl(this);
//}

void TreeControl::onChildBoundsChanged(gweni::Rect /*oldChildBounds*/, Base * /*child*/)
{
    m_scrollControl->updateScrollBars();
    invalidate();
}

void TreeControl::clear()
{
    m_scrollControl->clear();
}

void TreeControl::layout(skin::Base *skin)
{
    ParentClass::ParentClass::layout(skin);
}

void TreeControl::postLayout(skin::Base *skin)
{
    ParentClass::ParentClass::postLayout(skin);
}

void TreeControl::onNodeAdded(TreeNode *node)
{
    node->onNamePressCaller.add(this, &TreeControl::OnNodeSelection);
}

void TreeControl::OnNodeSelection(event::Info info)
{
    if(!m_allowMultipleSelection || !input::isKeyDown(keys::Control))
        deselectAll();
}

}//namespace controls
}//namespace gweni
