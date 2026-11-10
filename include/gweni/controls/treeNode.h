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
    //! The side of the square expand/collapse toggle, in logical units.
    static constexpr int ToggleSize=15;

    virtual TreeNode *addNode(const String &label);

    virtual void            setText(const String &text);
    virtual const String &getText();
    virtual void            setImage(const String &text);

    virtual void open();
    virtual void close();

    virtual void expandAll();

    virtual Button *GetButton();

    //void render(skin::Base *skin) override;
    void calculateSize(skin::Base *skin, Dim dim) override;
    void arrange(skin::Base *skin, Dim dim) override;
    void layout(skin::Base *skin) override;

    virtual void setRoot(bool b)
    {
        m_root=b;
    }

    // What the skin's connector lines are drawn from, in this node's
    // coordinates. A TreeControl has no toggle or title: empty bounds, 0.

    //! A node of the TreeControl itself, which gets no stub.
    bool isRoot() const { return m_root; }
    //! Its child rows are shown (it may have none).
    bool isOpen() const { return !m_innerPanel->hidden(); }
    //! It has child nodes, so its toggle shows.
    bool hasToggle() const { return m_toggleButton && !m_toggleButton->hidden(); }
    const Rect &getToggleBounds() const;
    const Rect &getTitleBounds() const;
    //! The x of the line down to its children: its toggle's centre.
    int lineX() const;
    //! The y of its stub: its toggle's centre.
    int stubY() const;
    //! The left edge of its children's rows (its inner panel's content).
    int childIndent() const;
    //! The y of its inner panel, under its title.
    int childPanelY() const { return m_innerPanel->getY(); }
    //! Its last child node that is not hidden, or nullptr.
    const TreeNode *lastChildNode() const;

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
    //! The rows below this node's children move: the canvas hit-tests the
    //! hover again after the layout.
    void childRowsMoved();

    TreeControl *m_treeControl;
    Button *m_toggleButton;
    Button *m_title;

    bool m_root;
    bool m_selected;
    bool m_selectable;
};

//! A TreeNode's expand/collapse toggle: a ToggleSize square whose toggle
//! state is whether the node is open. Its skin renderer draws that state.
class GWENI_EXPORT OpenToggleButton: public Button
{
    GWENI_CONTROL_INLINE(OpenToggleButton, Button)
    {
        setIsToggle(true);
        setTabable(false);
        // A glyph with no caption: the node's square, not an empty Text's size.
        setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        setSize(TreeNode::ToggleSize, TreeNode::ToggleSize);
    }

public:
    void renderFocus(skin::Base * /*skin*/) override
    {}
};


}
}
#endif//_gweni_controls_treenode_h_
