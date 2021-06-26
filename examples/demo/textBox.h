#ifndef _gweni_textBox_example_h_
#define _gweni_textBox_example_h_

#include <gweni/controls/base.h>

namespace gweni
{

class TextBoxDemo : public controls::Base
{
public:
    GWENI_CONTROL(TextBoxDemo, controls::Base);

    void onEdit(event::Info info);

    void onSubmit(event::Info info);

private:
    Font m_font;
};

}//namespace gweni

#endif//_gweni_textBox_example_h_
