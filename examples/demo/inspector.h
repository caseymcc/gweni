#ifndef _gweni_inspector_example_h_
#define _gweni_inspector_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class Button;
}
namespace tools
{
class Inspector;
}

//! A small app part (a GroupBox of a Button, a check box and a Label) and the
//! inspector open beside it, the Button selected and so highlighted.
class InspectorDemo: public controls::Base
{
public:
    GWENI_CONTROL(InspectorDemo, controls::Base);

public:
    tools::Inspector *inspector() const { return m_inspector; }
    //! The Button the inspector selects.
    controls::Button *button() const { return m_button; }

private:
    tools::Inspector *m_inspector=nullptr;
    controls::Button *m_button=nullptr;
};

}//namespace gweni

#endif//_gweni_inspector_example_h_
