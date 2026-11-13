/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  See license in gweni.h
 */


#include <gweni/controls/codeView.h>
#include <gweni/text/tokenizer.h>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(CodeView)
{
    setReadOnly(true);
    // its content's size, cut to a layout cell or given by a dock: the list's
    // default size is no request of the app's
    setSizeFlags({SizeFlag::Shrink, SizeFlag::Shrink});
    m_widthRequested=false;
    m_heightRequested=false;
}

bool CodeView::setFileName(const String &fileName)
{
    const String language=text::languageForFileName(fileName);

    setLanguage(language);
    return !language.empty();
}

// The registry's diff tokenizer, looked up each time: an app may register
// another under the name.
bool CodeView::showsDiffRows() const
{
    const text::Tokenizer *tokenizer=getTokenizer();

    return tokenizer && tokenizer == text::findLanguage("diff");
}

}//namespace controls
}//namespace gweni
