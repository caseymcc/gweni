#ifndef _gweni_processBar_example_h_
#define _gweni_processBar_example_h_

#include <gweni/controls/base.h>

#include <vector>

namespace gweni
{
namespace controls
{
class ProgressBar;
}

class ProgressBarDemo : public controls::Base
{
public:
    GWENI_CONTROL(ProgressBarDemo, controls::Base);

    void onToggleIndeterminate(event::Info info);

private:
    std::vector<controls::ProgressBar *> m_indeterminate;
};

}//namespace gweni

#endif//_gweni_processBar_example_h_
