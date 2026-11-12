#ifndef _gweni_textBox_example_h_
#define _gweni_textBox_example_h_

#include <gweni/controls/base.h>
#include <gweni/controls/textBox.h>

namespace gweni
{

class TextBoxDemo : public controls::Base
{
public:
    GWENI_CONTROL(TextBoxDemo, controls::Base);

public:
    void onEdit(event::Info info);

    void onSubmit(event::Info info);

    //! the Copy button: the first box's text onto the clipboard
    void onCopy(event::Info info);

private:
    Font m_font;
    controls::TextBox *m_first=nullptr;
};

}//namespace gweni

#endif//_gweni_textBox_example_h_
