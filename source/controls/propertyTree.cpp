/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/controls/propertyTree.h>
#include <gweni/skin.h>

namespace gweni
{
namespace controls
{

Properties *PropertyTree::add(const String &text)
{
    TreeNode *node=new PropertyTreeNode(this);
    node->setText(text);
    node->dock(Position::Top);
    Properties *props=new Properties(node);
    props->dock(Position::Top);
    return props;
}

Properties *PropertyTree::find(const String &text)
{
    controls::Base::List &children=GetChildNodes();

    for(auto &&control : children)
    {
        PropertyTreeNode *child=gweni_cast<PropertyTreeNode>(control);

        if(!child)
            continue;

        if(child->getText() == text)
        {
            Base::List &nodechildren=child->getChildren();

            for(auto &&subcontrol : nodechildren)
            {
                Properties *propertyChild=gweni_cast<Properties>(subcontrol);

                if(!propertyChild)
                    continue;

                return propertyChild;
            }
        }
    }

    return nullptr;
}

} // namespace controls
} // namespace gweni
