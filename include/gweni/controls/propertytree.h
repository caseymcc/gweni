/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_propertytree_h_
#define _gweni_controls_propertytree_h_

#include <gweni/controls/base.h>
#include <gweni/controls/label.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>
#include <gweni/controls/treeControl.h>
#include <gweni/controls/properties.h>


namespace gweni
{
namespace controls
{
class PropertyTreeNode: public TreeNode
{
public:

    GWENI_CONTROL_INLINE(PropertyTreeNode, TreeNode)
    {
        m_title->setTextColorOverride(getSkin()->Colors.Properties.Title);
    }

    void render(skin::Base *skin) override
    {
//        skin->drawPropertyTreeNode(this, skin::Generate, m_innerPanel->getX(), m_innerPanel->getY());
        skin->drawControl(this);
    }

};


class PropertyTree: public TreeControl
{
public:

    GWENI_CONTROL_INLINE(PropertyTree, TreeControl)
    {}

    Properties *add(const String &text);
    Properties *find(const String &text);
};


}
}
#endif//_gweni_controls_propertytree_h_
