/*
 */


#ifndef _gweni_controls_container_h_
#define _gweni_controls_container_h_

namespace gweni
{
namespace controls
{

class GWENI_EXPORT Container: public Base
{
public:

    GWENI_CONTROL(Container, Base);

public:
    void render(skin::Base *skin) override;

protected:
};

}//controls
}//gweni

#endif//_gweni_controls_container_h_
