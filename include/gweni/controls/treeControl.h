/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_treecontrol_h_
#define _gweni_controls_treecontrol_h_

#include <gweni/controls/base.h>
#include <gweni/controls/treeNode.h>

namespace gweni
{
namespace controls
{
class GWENI_EXPORT TreeControl: public TreeNode
{
public:

    GWENI_CONTROL(TreeControl, TreeNode);

public:
    //void render(skin::Base *skin) override;

    void onChildBoundsChanged(gweni::Rect oldChildBounds, Base *child) override;

    ScrollControl *scroller()
    {
        return m_scrollControl;
    }

    virtual void clear();

    void layout(skin::Base *skin) override;
    void postLayout(skin::Base *skin) override;

    virtual void allowMultiSelect(bool b)
    {
        m_allowMultipleSelection=b;
    }

    virtual void onNodeAdded(TreeNode *node);

private:

    void OnNodeSelection(event::Info info);

    ScrollControl *m_scrollControl;
    bool m_allowMultipleSelection;

};


}
}
#endif//_gweni_controls_treecontrol_h_
