#ifndef _gweni_vectorfield_example_h_
#define _gweni_vectorfield_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class VectorField;
}

class VectorFieldDemo: public controls::Base
{
public:
    GWENI_CONTROL(VectorFieldDemo, controls::Base);

public:
    //! The tagged float3, for a test to drag.
    controls::VectorField *taggedField() const { return m_tagged; }

private:
    controls::VectorField *m_tagged=nullptr;
};

}//namespace gweni

#endif//_gweni_vectorfield_example_h_
