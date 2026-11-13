/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  See license in gweni.h
 */


#include <gweni/controls/codeView.h>
#include <gweni/gweni.h>
#include <gweni/platforms/platform.h>
#include <gweni/text/tokenizer.h>

#include <algorithm>

namespace gweni
{
namespace controls
{

namespace internal
{

GWENI_CONTROL_CONSTRUCTOR(CodeViewCopyButton)
{
    // takes the focus by Tab while it shows, and Space presses it then
    setKeyboardInputEnabled(true);
    setTabable(true);
    setText(CodeView::CopyLabel);
}

// A press gives the pressed control the keyboard focus (the input handler
// does, before this runs): the focus goes on to the view, as a press anywhere
// else in it gives it, so the button, shown while it has the focus, hides once
// the mouse has left. Only Tab focuses it.
void CodeViewCopyButton::onMouseClickLeft(int x, int y, bool down)
{
    ParentClass::onMouseClickLeft(x, y, down);

    if(down && isFocussed() && getParent())
        getParent()->focus();
}

}//namespace internal

GWENI_CONTROL_CONSTRUCTOR(CodeView)
{
    setReadOnly(true);
    // its content's size, cut to a layout cell or given by a dock: the list's
    // default size is no request of the app's
    setSizeFlags({SizeFlag::Shrink, SizeFlag::Shrink});
    m_widthRequested=false;
    m_heightRequested=false;
}

// As auto-height counts the bar (CodeEditor::calculateSize), but for any
// height: the view is laid out at its preferred height, so no vertical bar
// narrows the viewport below what the width gives.
void CodeView::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    if(dim != Dim::Y || hasAutoHeight() || isWrapped() || sizedByRequest(Dim::Y))
        return;

    const Margin &inset=m_innerPanel->getMargin();
    const Margin &barMargin=m_horizontalScrollBar->getMargin();
    const int viewport=getWidth()-m_padding.left-m_padding.right-inset.left-inset.right;

    if(contentSize().width <= viewport)
        return;

    const int height=contentSize().height+inset.top+inset.bottom+m_padding.top+m_padding.bottom
        +m_barSize.height+barMargin.top+barMargin.bottom;

    m_preferredSize.height=preferredFromContent(height, Dim::Y);
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

void CodeView::setCopyButton(bool show)
{
    if(m_copyButtonOn == show)
        return;

    m_copyButtonOn=show;

    if(show && !m_copyButton)
    {
        // the view's own child, after the inner panel (which a new child
        // would otherwise go into, and scroll with the content): numbered
        // after every row
        Base *inner=m_innerPanel;
        PartScope part;

        m_innerPanel=nullptr;
        m_copyButton=newChild<internal::CodeViewCopyButton>();
        m_innerPanel=inner;
        m_copyButton->setPlacement(Placement::Owner);
        m_copyButton->setHidden(true);
        m_copyButton->onPressCaller.add(this, &CodeView::onCopyPressed);
    }
    updateCopyButton();
}

String CodeView::copyText() const
{
    return hasSelection()?getSelectedText():getText();
}

void CodeView::copy()
{
    platform::setClipboardText(copyText());

    if(!m_copyButton)
        return;

    Canvas *canvas=getCanvas();

    if(!canvas)
        return;

    if(m_copied)
        canvas->cancel(m_copied);
    m_copied=canvas->after(CopiedSeconds, [this]() { endCopied(); }, this);
    m_copyButton->setText(CopiedLabel);
}

void CodeView::endCopied()
{
    m_copied=TimerHandle();
    if(m_copyButton)
        m_copyButton->setText(CopyLabel);
}

void CodeView::onCopyPressed(event::Info)
{
    copy();
}

bool CodeView::hoverInside() const
{
    for(const Base *control=HoveredControl; control; control=control->getParent())
    {
        if(control == this)
            return true;
    }
    return false;
}

void CodeView::updateCopyButton()
{
    if(!m_copyButton)
        return;

    const bool show=m_copyButtonOn && (hoverInside() || m_copyButton->isFocussed());

    if(m_copyButton->hidden() == !show)
        return;

    m_copyButton->setHidden(!show);
}

// Base's, but no redraw: the view's look does not follow the hover, so the
// hover writes the button's slots alone.
void CodeView::onMouseEnter()
{
    onHoverEnter.call(this);
    notifyAncestorsOfInteraction();
    updateCopyButton();
}

void CodeView::onMouseLeave()
{
    onHoverLeave.call(this);
    notifyAncestorsOfInteraction();
    updateCopyButton();
}

// The hover moved inside the view (onto the button, a bar, or back), or the
// button took or lost the focus.
void CodeView::onDescendantInteraction(Base *control)
{
    ParentClass::onDescendantInteraction(control);
    updateCopyButton();
}

// After the rows, whose placement the viewport is final for: the button at
// the viewport's top-right, measured here (it was hidden when the X measuring
// ran, or its text changed).
void CodeView::contentsPlaced(skin::Base *skin)
{
    ParentClass::contentsPlaced(skin);

    if(!m_copyButton || m_copyButton->hidden())
        return;

    m_copyButton->doCalculateSize(skin, Dim::X);
    m_copyButton->doCalculateSize(skin, Dim::Y);

    const Size &size=m_copyButton->getPreferredSize();
    const Rect &viewport=getViewport();

    int y=viewport.y+CopyInset;

    // a viewport too short for it (a one-line command): centred on the view
    // above its horizontal bar, which it may cover to the view's border
    if(y+size.height > viewport.y+viewport.h)
    {
        const int bottom=m_horizontalScrollBar->hidden()?getHeight():m_horizontalScrollBar->getY();

        y=std::max(0, (bottom-size.height)/2);
    }

    const int x=std::max(0, viewport.x+viewport.w-CopyInset-size.width);

    placeChild(m_copyButton, x, y, size.width, size.height);
}

}//namespace controls
}//namespace gweni
