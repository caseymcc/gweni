#ifndef _gweni_codeView_example_h_
#define _gweni_codeView_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class CodeView;
}

//! Three read-only CodeViews, as a dashboard shows them, each with a copy
//! button: a unified diff of a prompt template (its rows in the diff
//! backgrounds), filling the left column and scrolling, an agent's YAML
//! config with line numbers, as tall as its lines, and a one-line install
//! command, as tall and as wide as its line.
class CodeViewDemo: public controls::Base
{
public:
    GWENI_CONTROL(CodeViewDemo, controls::Base);

public:
    //! The left column's width, in units.
    static constexpr int DiffWidth=380;

    //! The texts the views are given.
    static const char *diffText();
    static const char *configText();
    static const char *installText();

    controls::CodeView *m_diff=nullptr;
    controls::CodeView *m_config=nullptr;
    controls::CodeView *m_install=nullptr;
};

}//namespace gweni

#endif//_gweni_codeView_example_h_
