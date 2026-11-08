#ifndef _gweni_dragnumber_example_h_
#define _gweni_dragnumber_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class DragNumber;
}

class DragNumberDemo: public controls::Base
{
public:
    GWENI_CONTROL(DragNumberDemo, controls::Base);

public:
    //! The integer field, for a test to drag.
    controls::DragNumber *integerField() const { return m_integer; }
    //! The float field with the range 0..1.
    controls::DragNumber *rangedField() const { return m_ranged; }

private:
    controls::DragNumber *m_integer=nullptr;
    controls::DragNumber *m_ranged=nullptr;
};

}//namespace gweni

#endif//_gweni_dragnumber_example_h_
