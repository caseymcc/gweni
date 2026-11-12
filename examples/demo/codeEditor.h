#ifndef _gweni_codeEditor_example_h_
#define _gweni_codeEditor_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class CodeEditor;
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
};

}//namespace gweni

#endif//_gweni_codeEditor_example_h_
