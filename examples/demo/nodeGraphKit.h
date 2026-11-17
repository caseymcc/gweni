/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#ifndef _gweni_nodeGraphKit_example_h_
#define _gweni_nodeGraphKit_example_h_

#include <gweni/controls/nodeGraph.h>
#include <gweni/events.h>

namespace gweni
{
namespace controls
{
class DragNumber;
}

//! What the demos' node graphs are built from: NodeGraphDemo's and the
//! showcase's "Canvas & graphs" page share it.
namespace nodegraphkit
{

//! a colour per kind of value, as a shader editor has them
extern const Color FloatPin;
extern const Color VectorPin;
extern const Color ColorPin;
extern const Color ShaderPin;

//! A pin of \p node in \p color and \p shape.
controls::Pin *addPin(controls::Node *node, controls::PinDirection direction, const String &name,
    const Color &color, controls::PinShape shape);

//! A DragNumber docked Top in \p node's body, shown with two decimals.
controls::DragNumber *addNumber(controls::Node *node, double value, double min, double max);

//! The menus of a graph: the graph's "Add node" (a value, a maths operation
//! or a mix, at the right-click), a node's "Delete node", a link's styles and
//! "Delete link"; a link dropped on empty space opens "Add node" there and
//! links the new node. Owned by the demo whose graph it is, which outlives
//! nothing it is called from.
class NodeGraphMenus: public event::Handler
{
public:
    //! The menus for \p graph and every node and link it has now; a link the
    //! user makes later gets its menu as it is made.
    explicit NodeGraphMenus(controls::NodeGraph *graph);

    //! A node of the "Add node" menu's kinds at the graph point \p at, linked
    //! to \p from when it has a pin that may end a link from it.
    controls::Node *addNodeOfKind(int kind, const PointF &at, controls::Pin *from=nullptr);

    void giveNodeMenu(controls::Node *node);
    void giveLinkMenu(controls::Link *link);

private:
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

    //! the pin the "Add node" menu's pick links the new node to: the drop's,
    //! or null once that pin is gone (its node deleted while the menu is open)
    controls::Pin *addFrom() const;
    //! the node or link whose menu opened last, null once it is gone
    controls::Base *menuTarget() const;

    //! what m_addFrom and m_menuTarget hold for no control: never an id
    static constexpr size_t NoControl=size_t(-1);

    controls::NodeGraph *m_graph;
    //! the control id of the node or link whose menu opened last; an id, not
    //! a pointer, since it is kept after that item is deleted
    size_t m_menuTarget=NoControl;
    //! the control id of the pin a link dropped on empty space came from,
    //! while its "Add node" menu is open; Delete may take its node meanwhile
    size_t m_addFrom=NoControl;
};

}//namespace nodegraphkit
}//namespace gweni

#endif//_gweni_nodeGraphKit_example_h_
