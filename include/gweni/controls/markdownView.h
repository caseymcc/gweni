/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_markdownView_h_
#define _gweni_controls_markdownView_h_

#include <gweni/controls/markdownDocument.h>
#include <gweni/controls/scrollControl.h>

namespace gweni
{
namespace controls
{

//! A MarkdownDocument in a vertical ScrollControl: the document as wide as the
//! viewport and as high as its blocks there, scrolled when taller. Its bar
//! always shows, disabled while the document fits (setAutoHideBars(false)): a
//! document's height follows its width, so a bar that came and went with the
//! height would change the width it is laid out at, and the stable gutter
//! keeps a layout to one pass (docs/layout.md -> "layout::Flow"). A code
//! block's vertical wheel scrolls the view, its horizontal one the code.
class GWENI_EXPORT MarkdownView: public ScrollControl
{
public:

    GWENI_CONTROL(MarkdownView, ScrollControl);

public:
    GWENI_DRAWS_NOTHING

    MarkdownDocument *getDocument() const { return m_document; }
    //! Its vertical scroll bar, which always shows
    BaseScrollBar *getScrollBar() const { return m_verticalScrollBar; }

    //! Its height is what its parent gives it: it asks for its requested
    //! height (none: 0), never its document's, which a ScrollControl measures
    //! before laying the document out at its width and so one width late
    //! (docs/layout.md -> "layout::Flow"). A host that wants the document's
    //! own height uses a MarkdownDocument, which has no scroll bar. Its
    //! width is its widest block's, as a ScrollControl's content's.
    bool sizedByRequest(Dim dim) const override
    {
        return (dim == Dim::Y) || ScrollControl::sizedByRequest(dim);
    }

    //! MarkdownDocument::setMarkdown: the controls of matching blocks kept,
    //! the same text again doing nothing
    void setMarkdown(const gweni::String &markdown) { m_document->setMarkdown(markdown); }
    const gweni::String &getMarkdown() const { return m_document->getMarkdown(); }
    void setCodeWrap(bool wrap) { m_document->setCodeWrap(wrap); }
    bool getCodeWrap() const { return m_document->getCodeWrap(); }
    void setCodeHighlighter(MarkdownHighlighter highlighter)
    {
        m_document->setCodeHighlighter(std::move(highlighter));
    }
    //! MarkdownDocument::setImageResolver
    void setImageResolver(MarkdownImageResolver resolver) { m_document->setImageResolver(std::move(resolver)); }

    //! Called with a link's url in info.string when one of the document's
    //! links is clicked (MarkdownDocument::onLinkClicked, info.control the
    //! view); gweni opens nothing
    event::Caller onLinkClicked;

protected:
    void onDocumentLink(event::Info info);

    MarkdownDocument *m_document=nullptr;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_markdownView_h_
