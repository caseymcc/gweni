#ifndef _gweni_codeEditor_example_h_
#define _gweni_codeEditor_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class CheckBoxWithLabel;
class CodeEditor;
class TextBox;
}

class CodeEditorDemo: public controls::Base
{
public:
    GWENI_CONTROL(CodeEditorDemo, controls::Base);

public:
    //! The lines of the YAML document the editor is given.
    static constexpr size_t LineCount=5000;

    //! A YAML document of `lines` lines, the same for the same count: a
    //! project's agents, each a block of keys, lists and a folded text.
    static String yamlDocument(size_t lines);

    controls::CodeEditor *m_editor=nullptr;
    //! "Wrap lines": the editor's setWrap.
    controls::CheckBoxWithLabel *m_wrapToggle=nullptr;

private:
    void onWrapChanged(event::Info info);
};

//! Plain auto-height CodeEditors in a form (CodeEditor::setPlain,
//! setAutoHeight): a title TextBox, a description from 3 to 8 rows and an
//! empty comment from 2 to 6.
class CodeEditorFormDemo: public controls::Base
{
public:
    GWENI_CONTROL(CodeEditorFormDemo, controls::Base);

public:
    //! The form's width, in units.
    static constexpr int FormWidth=420;

    controls::TextBox *m_title=nullptr;
    controls::CodeEditor *m_description=nullptr;
    controls::CodeEditor *m_comment=nullptr;
};

}//namespace gweni

#endif//_gweni_codeEditor_example_h_
