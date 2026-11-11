#ifndef _gweni_nodeGraph_example_h_
#define _gweni_nodeGraph_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class NodeGraph;
}

//! A small shader-like node graph: a texture coordinate, a noise, a colour
//! ramp, a mix and a material output, their bodies holding a Label, a
//! DragNumber, a ComboBox and a TextBox, their pins in the three shapes and a
//! colour per kind of value, and a link of each style between them. The
//! wheel zooms about the cursor, a middle or right drag (or Space and a left
//! drag) pans.
class NodeGraphDemo: public controls::Base
{
public:
    GWENI_CONTROL(NodeGraphDemo, controls::Base);

public:
    controls::NodeGraph *graph() const { return m_graph; }

private:
    controls::NodeGraph *m_graph=nullptr;
};

}//namespace gweni

#endif//_gweni_nodeGraph_example_h_
