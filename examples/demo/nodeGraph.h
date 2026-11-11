/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#ifndef _gweni_nodeGraph_example_h_
#define _gweni_nodeGraph_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class Link;
class Node;
class NodeGraph;
class Pin;
}

//! A small shader-like node graph: a texture coordinate, a noise, a colour
//! ramp, a mix and a material output, their bodies holding a Label, a
//! DragNumber, a ComboBox and a TextBox, their pins in the three shapes and a
//! colour per kind of value, and a link of each style between them. The
//! wheel zooms about the cursor, a middle or right drag (or Space and a left
//! drag) pans. A right-click on empty space offers nodes to add there (as a
//! link dropped where no node is does, linking the new node), on a node to
//! delete it, on a link to restyle or delete it; Delete removes the
//! selection.
class NodeGraphDemo: public controls::Base
{
public:
    GWENI_CONTROL(NodeGraphDemo, controls::Base);

public:
    controls::NodeGraph *graph() const { return m_graph; }

    //! A node of the "Add node" menu's kinds at the graph point \p at, linked
    //! to \p from when it has a pin that may end a link from it.
    controls::Node *addNodeOfKind(int kind, const PointF &at, controls::Pin *from=nullptr);

private:
    //! The menus: the graph's "Add node", a node's "Delete node", a link's
    //! styles and "Delete link".
    void giveNodeMenu(controls::Node *node);
    void giveLinkMenu(controls::Link *link);

    void onGraphMenu(event::Info info);
    void onItemMenu(event::Info info);
    void onAddValue(event::Info info);
    void onAddMath(event::Info info);
    void onAddMix(event::Info info);
    void onDeleteItem(event::Info info);
    void onStraight(event::Info info);
    void onBezier(event::Info info);
    void onOrthogonal(event::Info info);
    void onLinkCreated(event::Info info);
    void onDroppedOnEmpty(event::Info info);

    controls::NodeGraph *m_graph=nullptr;
    //! the node or link whose menu opened last
    controls::Base *m_menuTarget=nullptr;
    //! the pin a link dropped on empty space came from, while its "Add node"
    //! menu is open
    controls::Pin *m_addFrom=nullptr;
};

}//namespace gweni

#endif//_gweni_nodeGraph_example_h_
