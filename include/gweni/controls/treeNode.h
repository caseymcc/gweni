/*
  * Gweni
  * Copyright (c) 2011 Facepunch Studios

  * See license in gweni.h
 */


#ifndef _gweni_controls_treenode_h_
#define _gweni_controls_treenode_h_

#include <gweni/controls/base.h>
#include <gweni/controls/button.h>
#include <gweni/controls/scrollControl.h>


namespace gweni
{
namespace controls
{
class TreeControl;

class GWENI_EXPORT TreeNode: public Base
{
public:

    GWENI_CONTROL(TreeNode, Base);

public:
    virtual TreeNode *addNode(const String &label);

    virtual void            setText(const String &text);
    virtual const String &getText();
    virtual void            setImage(const String &text);

    virtual void open();
    virtual void close();

    virtual void expandAll();

    virtual Button *GetButton();

    //void render(skin::Base *skin) override;
    void layout(skin::Base *skin) override;
    void postLayout(skin::Base *skin) override;

    virtual void setRoot(bool b)
    {
        m_root=b;
    }

    virtual void setTreeControl(TreeControl *ctrl)
    {
        m_treeControl=ctrl;
    }

    virtual void setSelectable(bool b)
    {
        m_selectable=b;
    }

    virtual bool isSelected()
    {
        return m_selected;
    }

    virtual void setSelected(bool b, bool FireEvents=true);

    virtual void deselectAll();

    virtual controls::Base::List &GetChildNodes();

    event::Caller onNamePressCaller;
    event::Caller onRightPressCaller;
    event::Caller onSelectChangeCaller;
    event::Caller onSelectCaller;
    event::Caller onUnselectCaller;

    /*protected:*/

    void onToggleButtonPress(event::Info);
    void onDoubleClickName(event::Info);
    void onRightPress(event::Info);
    void onClickName(event::Info);

protected:

    TreeControl *m_treeControl;
    Button *m_toggleButton;
    Button *m_title;

    bool m_root;
    bool m_selected;
    bool m_selectable;
};


}
}
#endif//_gweni_controls_treenode_h_
