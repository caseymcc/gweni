/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/controls/textBox.h>
#include <gweni/controls/canvas.h>
#include <gweni/skin.h>
#include <gweni/anim.h>
#include <gweni/utility.h>
#include <gweni/platforms/platform.h>
#include <gweni/inputHandler.h>
#include <cmath>

namespace gweni
{
namespace controls
{

// Positions in a TextBox are byte offsets into its UTF-8 getText(), and every
// one of them is kept on a codepoint boundary: the editing steps below move by
// whole codepoints, and a position set from outside is snapped back to one.
static int snapPos(const String &str, int pos)
{
    return static_cast<int>(utf8Snap(str, static_cast<size_t>(std::max(pos, 0))));
}

static int nextPos(const String &str, int pos)
{
    return static_cast<int>(utf8Next(str, static_cast<size_t>(pos)));
}

static int prevPos(const String &str, int pos)
{
    return static_cast<int>(utf8Prev(str, static_cast<size_t>(pos)));
}

GWENI_CONTROL_CONSTRUCTOR(TextBox)
{
//    enlargePrimitiveIds(this, m_primitiveIds, 11);

    setSize(200, 20);
    // the box scrolls its text to the caret, so its size is the one asked for,
    // never its text's (a Label's Elastic)
    setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    setMouseInputEnabled(true);
    setKeyboardInputEnabled(true);
    setPadding(Padding(4, 2, 4, 2));
    m_cursorPos=0;
    m_cursorEnd=0;
    m_cursorLine=0;
    m_editable=true;
    m_selectAll=false;
    m_maxTextLength=NO_MAX_LENGTH;
    setSkinTextColor(internal::SkinTextColor::LabelDefault);
    setTabable(true);
    addAccelerator("Ctrl + C", &TextBox::onCopy);
    addAccelerator("Ctrl + X", &TextBox::onCut);
    addAccelerator("Ctrl + V", &TextBox::onPaste);
    addAccelerator("Ctrl + A", &TextBox::onSelectAll);
    addAccelerator("Ctrl + Z", &TextBox::onUndo);
    addAccelerator("Ctrl + Y", &TextBox::onRedo);
    addAccelerator("Ctrl + Shift + Z", &TextBox::onRedo);
}

// The message line's state, on the heap from its first use.
struct TextBox::MessageLine
{
    internal::Text *text=nullptr;
    //! The line's font: the box's one size smaller. A new one replaces it on
    //! every change, as the Text keeps a pointer and tells a new font by it.
    std::unique_ptr<gweni::Font> font;
    String error;
    String helper;
    Validator validator;
    ValidateOn validateOn=ValidateOn::FocusLoss;
    //! The app's padding; the box's own is this plus room at the bottom.
    Padding fieldPadding;
    bool reserved=false;
    int room=0;
};

//! How much smaller the message line's font is than the box's.
static constexpr float MessageFontStep=1.0f;
//! The gap between the field part and the message line's text.
static constexpr int MessageGap=2;

TextBox::~TextBox()=default;

void TextBox::preDelete(skin::Base *skin)
{
    if(m_message)
        m_message->text->setFont(skin->getDefaultFont());

    ParentClass::preDelete(skin);
}

bool TextBox::onChar(gweni::UnicodeChar c)
{
    if(c == '\t')
        return false;

    const String str=utf8Encode(c);

    if(str.empty())
        return false;

    EditKindScope kind(*this, text::EditKind::Insert);
    insertText(str);
    return true;
}

void TextBox::insertText(const gweni::String &strIn)
{
    if(!m_editable)
        return;

    // converted first, so the checks and the cap see what goes in; nothing
    // left (a paste of "\n") keeps the selection
    String converted;
    const String &strInsert=normalizeText(strIn, converted)?converted:strIn;

    if(strInsert.empty())
        return;

    // the selection's erase and the insert are one undo step
    text::UndoGroup group(m_undo, m_editKind);

    if(hasSelection())
        eraseSelection();

    m_cursorPos=snapPos(getText(), std::min(m_cursorPos, textLength()));

    if(!isTextAllowed(strInsert, m_cursorPos))
        return;

    int insertSize=static_cast<int>(strInsert.size());
    if(m_maxTextLength > NO_MAX_LENGTH && textLength() + insertSize > m_maxTextLength)
    {
        // The limit is in bytes; a codepoint that does not fit whole is left out.
        insertSize=snapPos(strInsert, m_maxTextLength - textLength());

        if(insertSize <= 0)
            return;
    }

    const text::Selection before=currentSelection();
    const int start=m_cursorPos;
    String str=getText();
    str.insert(m_cursorPos, strInsert, 0, insertSize);
    const bool own=setEditedText(str);
    m_cursorPos+=insertSize;
    m_cursorEnd=m_cursorPos;
    m_cursorLine=0;
    refreshCursorBounds();

    if(own)
        recordEdit(start, String(), strInsert.substr(0, insertSize), before);
}

text::Selection TextBox::currentSelection() const
{
    return {{0, static_cast<size_t>(m_cursorEnd)}, {0, static_cast<size_t>(m_cursorPos)}};
}

// The edit's own setText takes m_applyingEdit before onTextChanged fires, so
// a setText from a handler is someone else's and clears the history. Any text
// set nested in this one (a handler's setText, or an edit of its own) leaves a
// text the edit did not make: the history is cleared and the edit is not
// recorded.
bool TextBox::setEditedText(const String &str)
{
    const unsigned int sets=m_textSets;

    m_applyingEdit=true;
    setText(str);
    m_applyingEdit=false;

    if(m_textSets-sets <= 1)
        return true;

    m_undo.clear();
    return false;
}

void TextBox::textSetting()
{
    ++m_textSets;

    if(m_applyingEdit)
        m_applyingEdit=false;
    else
        m_undo.clear();
}

// An edit taken back or made again: \p out must be at \p at, and is replaced
// by \p in. False, with \p str unchanged, if the text there is not \p out.
static bool replaceEdited(String &str, size_t at, const String &out, const String &in)
{
    if(at > str.size() || str.compare(at, out.size(), out) != 0)
        return false;

    str.replace(at, out.size(), in);
    return true;
}

void TextBox::recordEdit(int start, const String &removed, const String &inserted, const text::Selection &before)
{
    text::Edit edit;

    edit.range={{0, static_cast<size_t>(start)}, {0, start+removed.size()}};
    edit.text=inserted;
    edit.removed=removed;
    edit.textEnd={0, start+inserted.size()};
    m_undo.record(std::move(edit), m_editKind, before, currentSelection());
}

// An undo step's edits taken back from the last to the first, or a redo's
// made again in order, on one copy of the text, which is then set once: one
// onTextChanged, and one new string for m_text, which re-wraps the paragraphs
// that differ as it does after any edit.
bool TextBox::undo()
{
    if(!m_editable)
        return false;

    const text::UndoStep *step=m_undo.undo();

    if(step == nullptr)
        return false;

    String str=getText();

    for(size_t i=step->edits.size(); i-- > 0;)
    {
        // the inverse: the inserted text replaced by the removed one
        const text::Edit &edit=step->edits[i];

        if(!replaceEdited(str, edit.range.begin.byte, edit.text, edit.removed))
            return historyLost();
    }
    applyUndoStep(str, step->before);
    return true;
}

// A step that does not fit the text (something changed it without clearing
// the history) is not applied: the history is dropped instead.
bool TextBox::historyLost()
{
    m_undo.clear();
    return false;
}

bool TextBox::redo()
{
    if(!m_editable)
        return false;

    const text::UndoStep *step=m_undo.redo();

    if(step == nullptr)
        return false;

    String str=getText();

    for(const text::Edit &edit:step->edits)
    {
        if(!replaceEdited(str, edit.range.begin.byte, edit.removed, edit.text))
            return historyLost();
    }
    applyUndoStep(str, step->after);
    return true;
}

// The selection is a copy: an onTextChanged handler that edits the box drops
// the steps that could be redone, the one it came from among them.
void TextBox::applyUndoStep(const String &str, text::Selection selection)
{
    setEditedText(str);
    m_cursorEnd=snapPos(getText(), std::min(static_cast<int>(selection.anchor.byte), textLength()));
    m_cursorPos=snapPos(getText(), std::min(static_cast<int>(selection.caret.byte), textLength()));
    m_cursorLine=0;
    refreshCursorBounds();
}

void TextBox::onUndo(event::Info)
{
    undo();
}

void TextBox::onRedo(event::Info)
{
    redo();
}

// Unfocused, the caret is not drawn and nothing here changes. Focused, each
// blink edge is one StateChange_Caret, which only shows or hides the caret
// (internal::CaretBlink, whose animation calls this).
void TextBox::updateCaretBlink()
{
    m_caretBlink.step(this);
}

void TextBox::setText(const String &str, bool doEvents)
{
    String converted;
    const String &newText=normalizeText(str, converted)?converted:str;

    // the same text again changes nothing, the history included
    if(newText != getText())
        textSetting();
    else
        m_applyingEdit=false;

    ParentClass::setText(newText, doEvents);
    updatePlaceholder();
}

bool TextBox::normalizeText(const String &str, String &converted) const
{
    // '\r', '\n' and '\t' never occur inside a UTF-8 sequence, so a byte
    // pass keeps every codepoint whole
    if(str.find_first_of("\r\n\t") == String::npos)
        return false;

    size_t end=str.size();

    while(end > 0 && (str[end-1] == '\n' || str[end-1] == '\r'))
        --end;

    converted.clear();
    converted.reserve(end);

    for(size_t i=0; i < end; ++i)
    {
        const char c=str[i];

        if(c == '\r' && i+1 < end && str[i+1] == '\n')
            continue;   // the "\n" after it is the one space

        converted+=(c == '\r' || c == '\n' || c == '\t')?' ':c;
    }
    return true;
}

// The hint is a Text of its own rather than a text id of the skin's: it is
// laid out, clipped, scaled and shown or hidden like m_text, and it does not
// count in the box's size (doNotIncludeInSize).
void TextBox::setPlaceholder(const String &str)
{
    if(!m_placeholder)
    {
        if(str.empty())
            return;

        m_placeholder=newChild<internal::Text>();
        m_placeholder->doNotIncludeInSize();
        m_placeholder->setFont(m_text->getFont());
        m_placeholder->setPadding(m_text->getPadding());
        m_placeholder->setAlignment(getTextAlignment());
        m_placeholder->setSkinTextColor(internal::SkinTextColor::Placeholder);
    }

    m_placeholder->setText(str);
    updatePlaceholder();
}

const String &TextBox::getPlaceholder() const
{
    static const String none;
    return m_placeholder?m_placeholder->getText():none;
}

void TextBox::updatePlaceholder()
{
    if(!m_placeholder)
        return;

    const bool show=!m_placeholder->getText().empty() && !hidden() && getText().empty() && !isFocussed();

    if(m_placeholder->hidden() == !show)
        return;

    m_placeholder->setHidden(!show);
}

// The line is placed by this box (Placement::Owner) and not in its size: its
// room is, as a fixed amount (requestedSizeExtra and the bottom padding), so a
// new message lays out the box and its Text alone.
TextBox::MessageLine &TextBox::messageLine()
{
    if(!m_message)
    {
        m_message.reset(new MessageLine());
        m_message->fieldPadding=m_padding;
        m_message->font.reset(new gweni::Font(m_text->getFont()));
        m_message->font->size=std::max(1.0f, m_message->font->size-MessageFontStep);

        internal::Text *text=newChild<internal::Text>();

        text->doNotIncludeInSize();
        text->setPlacement(Placement::Owner);
        text->setFont(*m_message->font);
        text->setSkinTextColor(internal::SkinTextColor::HelperText);
        m_message->text=text;
    }

    setMessageReserved(true);
    return *m_message;
}

void TextBox::setMessageReserved(bool reserved)
{
    if(m_message->reserved == reserved)
        return;

    m_message->reserved=reserved;
    // the room comes and goes with the Y measure (updateMessageRoom); the
    // box's size changes, as with a new padding
    noteSizeInputsChanging();
    invalidate();
    invalidateParent();
    invalidateParentReaders();
}

void TextBox::updateMessageRoom()
{
    if(!m_message)
        return;

    const int room=m_message->reserved?measureLineHeight(*m_message->font)+MessageGap:0;

    if(room == m_message->room)
        return;

    m_message->room=room;
    // inside the box's own measure, before anything is placed in it
    m_padding=m_message->fieldPadding;
    m_padding.bottom+=room;
}

int TextBox::requestedSizeExtra(Dim dim) const
{
    return (dim == Dim::Y && m_message)?m_message->room:0;
}

int TextBox::getMessageRoom() const
{
    return m_message?m_message->room:0;
}

const Padding &TextBox::getPadding() const
{
    return m_message?m_message->fieldPadding:m_padding;
}

void TextBox::setPadding(const Padding &padding)
{
    if(!m_message)
        return ParentClass::setPadding(padding);

    m_message->fieldPadding=padding;

    Padding withRoom=padding;

    withRoom.bottom+=m_message->room;
    ParentClass::setPadding(withRoom);
}

void TextBox::setError(const String &text)
{
    if(m_message?(m_message->error == text):text.empty())
        return;

    messageLine().error=text;
    refreshMessage();
    onErrorChanged.call(this);
}

bool TextBox::hasError() const
{
    return m_message && !m_message->error.empty();
}

const String &TextBox::getError() const
{
    static const String none;
    return m_message?m_message->error:none;
}

void TextBox::setHelperText(const String &text)
{
    if(m_message?(m_message->helper == text):text.empty())
        return;

    messageLine().helper=text;
    refreshMessage();
}

const String &TextBox::getHelperText() const
{
    static const String none;
    return m_message?m_message->helper:none;
}

void TextBox::setValidator(Validator validator, ValidateOn when)
{
    if(!validator)
    {
        if(m_message)
            m_message->validator=nullptr;
        return;
    }

    MessageLine &line=messageLine();

    line.validator=std::move(validator);
    line.validateOn=when;
}

bool TextBox::hasValidator() const
{
    return m_message && m_message->validator;
}

// The app's setError and the validator's result take the same path. A copy,
// as the validator may replace itself.
bool TextBox::validate()
{
    if(hasValidator())
    {
        const Validator validator=m_message->validator;

        setError(validator(getText()));
    }
    return !hasError();
}

void TextBox::setReserveMessageLine(bool reserve)
{
    if(reserve)
        messageLine();
    else if(m_message && getMessage().empty() && !m_message->validator)
        setMessageReserved(false);
}

bool TextBox::reservesMessageLine() const
{
    return m_message && m_message->reserved;
}

const internal::Text *TextBox::getMessageText() const
{
    return m_message?m_message->text:nullptr;
}

const String &TextBox::getMessage() const
{
    static const String none;

    if(!m_message)
        return none;

    return m_message->error.empty()?m_message->helper:m_message->error;
}

// The Text's string and colour, and the box's look (its error surfaces): a
// redraw() of the box, whose frame is one slot.
void TextBox::refreshMessage()
{
    internal::Text *text=m_message->text;

    text->setSkinTextColor(hasError()?internal::SkinTextColor::ErrorText:internal::SkinTextColor::HelperText);
    text->setText(getMessage());
    redraw();
}

bool TextBox::onMessageLine(int x, int y)
{
    if(!m_message || m_message->room == 0)
        return false;

    return canvasPosToLocal(gweni::Point(x, y)).y >= getFieldHeight();
}

void TextBox::updateColors()
{
    ParentClass::updateColors();
    updatePlaceholder();
}

void TextBox::setFont(const gweni::Font &font)
{
    ParentClass::setFont(font);

    if(m_placeholder)
        m_placeholder->setFont(font);

    if(m_message)
    {
        std::unique_ptr<gweni::Font> smaller(new gweni::Font(font));

        smaller->size=std::max(1.0f, smaller->size-MessageFontStep);

        if(*smaller != *m_message->font)
        {
            m_message->text->setFont(*smaller);
            m_message->font=std::move(smaller);
        }
    }
}

void TextBox::setTextPadding(const Padding &padding)
{
    ParentClass::setTextPadding(padding);

    if(m_placeholder)
        m_placeholder->setPadding(padding);
}

void TextBox::setTextAlignment(Alignment alignment)
{
    ParentClass::setTextAlignment(alignment);

    if(m_placeholder)
        m_placeholder->setAlignment(getTextAlignment());
}

void TextBox::setWrap(bool b)
{
    ParentClass::setWrap(b);

    if(m_placeholder)
        m_placeholder->setAlignment(getTextAlignment());
}

// The focus change itself repaints this box (Base::focus's redraw()), which
// shows the caret and selection.
void TextBox::onKeyboardFocus()
{
    ParentClass::onKeyboardFocus();
    keepCaretLit();
    m_caretBlink.start(this);
}

void TextBox::onLostKeyboardFocus()
{
    ParentClass::onLostKeyboardFocus();
    m_caretBlink.stop(this);
    // typing after the focus comes back is a step of its own
    m_undo.seal();

    if(hasValidator() && (m_message->validateOn & ValidateOn::FocusLoss))
        validate();

    onLostFocusCaller.call(this);
}

void TextBox::keepCaretLit()
{
    m_caretBlink.keepLit(this);
}

void TextBox::refreshCursorBounds()
{
    keepCaretLit();
    makeCaretVisible();
    updateCursorRects();
}

// TextBox-local, from where m_text is now: the skin adds the box's position.
// A wrapped text's selection spans lines, which the one selection rect cannot
// show, so it is left empty there.
void TextBox::updateCursorRects()
{
    const gweni::Rect pA=getCharacterPosition(displayPos(m_cursorPos));
    gweni::Rect selection;

    if(hasSelection() && !m_text->wrap())
    {
        const gweni::Rect pB=getCharacterPosition(displayPos(m_cursorEnd));

        selection.x=std::min(pA.x, pB.x);
        selection.y=pA.y;
        selection.w=std::max(pA.x, pB.x)-selection.x;
        selection.h=pA.h;
    }

    // one unit wide and as high as the line box renderText draws
    const gweni::Rect caret(pA.x, pA.y, 1, pA.h);

    if(caret == m_rectCaretBounds && selection == m_rectSelectionBounds)
        return;

    m_rectCaretBounds=caret;
    m_rectSelectionBounds=selection;

    // unfocused neither is drawn; the focus arriving repaints them
    if(isFocussed())
        addStateChange(StateChange_Background);
}

// The text may come later (the browser's paste event): the box is looked up
// again then, and an answer for a box deleted meanwhile does nothing.
void TextBox::onPaste(event::Info)
{
    const size_t id=getControlId();

    platform::requestClipboardText([id](const String &text)
    {
        TextBox *box=dynamic_cast<TextBox *>(Base::getControlById(id));

        if(!box)
            return;

        EditKindScope kind(*box, text::EditKind::Paste);
        box->insertText(text);
    });
}

void TextBox::onCopy(event::Info)
{
    if(!hasSelection())
        return;

    platform::setClipboardText(getSelection());
}

void TextBox::onCut(event::Info)
{
    if(!hasSelection())
        return;

    platform::setClipboardText(getSelection());

    EditKindScope kind(*this, text::EditKind::Cut);
    eraseSelection();
}

void TextBox::onSelectAll(event::Info)
{
    m_cursorEnd=0;
    m_cursorPos=textLength();
    m_cursorLine=0;
    refreshCursorBounds();
}

void TextBox::onMouseDoubleClickLeft(int x, int y)
{
    if(onMessageLine(x, y))
        return;

    onSelectAll(this);
}

String TextBox::getSelection()
{
    if(!hasSelection())
        return "";

    int iStart=std::min(m_cursorPos, m_cursorEnd);
    int iEnd=std::max(m_cursorPos, m_cursorEnd);
    const String &str=getText();
    return str.substr(iStart, iEnd-iStart);
}

bool TextBox::onKeyReturn(bool down)
{
    // The press goes on up to the parents: a dialog's default button
    // (dialogs::Dialog::onKeyReturn) takes it there. The release is the box's.
    if(down)
        return false;

    // Try to move to the next control, as if tab had been pressed
    onKeyTab(true);

    // If we still have focus, unfocus it.
    if(isFocussed())
        unfocus();

    // This is called AFTER the unfocusring so you can
    // refocus in your onReturnPressed hook.
    onEnter();
    return true;
}

bool TextBox::onKeyBackspace(bool down)
{
    if(!down)
        return true;

    EditKindScope kind(*this, text::EditKind::DeleteBackward);

    if(hasSelection())
    {
        eraseSelection();
        return true;
    }

    if(m_cursorPos == 0)
        return true;

    const int start=prevPos(getText(), m_cursorPos);
    deleteText(start, m_cursorPos-start);
    return true;
}

bool TextBox::onKeyDelete(bool down)
{
    if(!down)
        return true;

    EditKindScope kind(*this, text::EditKind::DeleteForward);

    if(hasSelection())
    {
        eraseSelection();
        return true;
    }

    if(m_cursorPos >= textLength())
        return true;

    deleteText(m_cursorPos, nextPos(getText(), m_cursorPos)-m_cursorPos);
    return true;
}

bool TextBox::onKeyLeft(bool down)
{
    if(!down)
        return true;

    m_cursorPos=prevPos(getText(), m_cursorPos);

    if(!gweni::input::isShiftDown())
        m_cursorEnd=m_cursorPos;

    refreshCursorBounds();
    return true;
}

bool TextBox::onKeyRight(bool down)
{
    if(!down)
        return true;

    if(m_cursorPos < textLength())
        m_cursorPos=nextPos(getText(), m_cursorPos);

    if(!gweni::input::isShiftDown())
        m_cursorEnd=m_cursorPos;

    refreshCursorBounds();
    return true;
}

bool TextBox::onKeyHome(bool down)
{
    if(!down)
        return true;

    m_cursorPos=0;

    if(!gweni::input::isShiftDown())
        m_cursorEnd=m_cursorPos;

    refreshCursorBounds();
    return true;
}

bool TextBox::onKeyend(bool /*down*/)
{
    m_cursorPos=textLength();

    if(!gweni::input::isShiftDown())
        m_cursorEnd=m_cursorPos;

    refreshCursorBounds();
    return true;
}

void TextBox::setCursorPos(int i)
{
    i=snapPos(getText(), i);

    if(m_cursorPos == i)
        return;

    m_cursorPos=i;
    m_cursorLine=0;
    refreshCursorBounds();
}

void TextBox::setCursorend(int i)
{
    i=snapPos(getText(), i);

    if(m_cursorEnd == i)
        return;

    m_cursorEnd=i;
    refreshCursorBounds();
}

void TextBox::deleteText(int iStartPos, int iLength)
{
    if(!m_editable)
        return;

    String str=getText();
    // Whole codepoints: both ends of the range are snapped back to a boundary.
    const int iEndPos=snapPos(str, iStartPos+iLength);
    iStartPos=snapPos(str, iStartPos);
    iLength=iEndPos-iStartPos;

    if(iLength <= 0)
        return;

    // Taken before setText, whose onTextChanged clamps it to the new length.
    const int cursorPos=m_cursorPos;
    const text::Selection before=currentSelection();
    const String removed=str.substr(iStartPos, iLength);

    str.erase(iStartPos, iLength);
    const bool own=setEditedText(str);

    if(cursorPos >= iEndPos)
        setCursorPos(cursorPos-iLength);
    else if(cursorPos > iStartPos)
        setCursorPos(iStartPos);

    setCursorend(m_cursorPos);

    if(own)
        recordEdit(iStartPos, removed, String(), before);
}

bool TextBox::hasSelection()
{
    return m_cursorPos != m_cursorEnd;
}

void TextBox::eraseSelection()
{
    int iStart=std::min(m_cursorPos, m_cursorEnd);
    int iEnd=std::max(m_cursorPos, m_cursorEnd);
    deleteText(iStart, iEnd-iStart);
    // Move the cursor to the start of the selection,
    // since the end is probably outside of the string now.
    m_cursorPos=iStart;
    m_cursorEnd=iStart;
}

void TextBox::onMouseClickLeft(int x, int y, bool down)
{
    // the message line is not content: a press on it moves no caret, and the
    // release of one there is no drag's end
    if(down && onMessageLine(x, y))
        return;

    if(m_selectAll)
    {
        onSelectAll(this);
        m_selectAll=false;
        return;
    }

    int value=realPos(m_text->getClosestCharacter(m_text->canvasPosToLocal(gweni::Point(x, y))));

    if(down)
    {
        setCursorPos(value);

        if(!gweni::input::isShiftDown())
            setCursorend(value);

        gweni::MouseFocus=this;
    }
    else
    {
        if(gweni::MouseFocus == this)
        {
            setCursorPos(value);
            gweni::MouseFocus=nullptr;
        }
    }
}

void TextBox::onMouseMoved(int x, int y, int /*deltaX*/, int /*deltaY*/)
{
    if(gweni::MouseFocus != this)
        return;

    int value=realPos(m_text->getClosestCharacter(m_text->canvasPosToLocal(gweni::Point(x, y))));
    setCursorPos(value);
}

// Reads the measured string, not m_text's bounds: in a layout pass this runs
// before m_text is placed. A string that fits is placed by the text alignment
// (setTextAlignment); a scrolled one is unaligned, so its position is kept.
void TextBox::makeCaretVisible()
{
    const Size text=m_text->refreshSize(false);
    const Alignment textAlignment=getTextAlignment();

    if(text.width < getWidth())
    {
        alignText(textAlignment);
    }
    else
    {
        int iCaretPos=m_text->getCharacterPosition(displayPos(m_cursorPos)).x;
        int iRealCaretPos=iCaretPos+m_text->getX();
        int iSlidingZone=m_text->getFont().size + 1;   // getWidth()*0.1f

        // If the carat is already in a semi-good position, leave it, but
        // clamped below: the box may have grown since the text was scrolled.
        int x=m_text->getX();

        if(iRealCaretPos > getWidth()-iSlidingZone)
            x=getWidth()-iCaretPos-iSlidingZone;

        if(iRealCaretPos < iSlidingZone)
            x=-iCaretPos+iSlidingZone;

        // Don't show too much whitespace to the right
        if(x+text.width < getWidth()-getPadding().right)
            x=-text.width+(getWidth()-getPadding().right);

        // Or the left
        if(x > getPadding().left)
            x=getPadding().left;

        int y=0;

        if(textAlignment & Alignment::Top)
            y=getPadding().top;

        if(textAlignment & Alignment::Bottom)
            y=getFieldHeight()-text.height-getPadding().bottom;

        if(textAlignment & Alignment::CenterV)
            y=(getFieldHeight()-text.height) / 2;

        placeScrolledText(x, y, text);
    }
}

void TextBox::alignText(Alignment textAlignment)
{
    m_text->setPlacement(Placement::Parent);
    m_text->setAlignment(textAlignment);
}

void TextBox::placeScrolledText(int x, int y, const Size &text)
{
    const int width=m_text->wrap()?std::max(0, getWidth()-getPadding().left-getPadding().right):text.width;

    m_text->setAlignment(Alignment::Absolute);
    m_text->setPlacement(Placement::Owner);
    placeChild(m_text, x, y, width, text.height);
}

bool TextBox::sizedByRequest(Dim dim) const
{
    if(!hasRequestedSize(dim))
        return false;

    return dim == Dim::X?!m_sizedByText:m_text->wrap();
}

void TextBox::setSizedByText(bool sizedByText)
{
    if(m_sizedByText == sizedByText)
        return;

    m_sizedByText=sizedByText;
    invalidate();
}

// On an axis sized by its request (sizedByRequest: a size was asked for, as
// the constructor's 200x20 is) Base::calculateSize has made the request the
// preferred size, whatever the flags, and the string adds nothing: it
// scrolls inside the box. On any other axis (no request, setSizedByText's X,
// the Y of a box whose text does not wrap) the box asks for its string, and
// its flags make the preferred size of it as of any content.
//
// A scrolled string is placed by this box (Placement::Owner), which leaves it
// out of sizeOfChildren; it counts as it does aligned, at its preferred size,
// so the box's size does not depend on whether it is scrolled. In X it counts
// beside the children docked Left or Right (a ColorSelector's swatch, a
// NumericUpDown's buttons), which sizeOfChildren takes the wider of with an
// aligned child: the string is aligned in the whole box, so it would run
// under them.
void TextBox::calculateSize(skin::Base *skin, Dim dim)
{
    // the message line's room is in the request and the padding the measure
    // below reads
    if(dim == Dim::Y)
        updateMessageRoom();

    ParentClass::calculateSize(skin, dim);

    if(sizedByRequest(dim) || m_text->hidden())
        return;

    const Size &text=m_text->getPreferredSize();
    const Margin &margin=m_text->getMargin();

    if(dim == Dim::X)
    {
        int docked=0;

        for(auto &&child : getChildren())
        {
            if(child == m_text || child->hidden() || !child->shouldIncludeInSize())
                continue;

            if(child->getDock() & (DockPosition::Left|DockPosition::Right))
                docked+=child->getPreferredSize().width+child->getMargin().left+child->getMargin().right;
        }

        m_preferredSize.width=std::max(m_preferredSize.width, preferredFromContent(
            text.width+margin.left+margin.right+m_padding.left+m_padding.right+docked, Dim::X));
    }
    else if(m_text->getPlacement() == Placement::Owner)
        m_preferredSize.height=std::max(m_preferredSize.height, preferredFromContent(
            text.height+margin.top+margin.bottom+m_padding.top+m_padding.bottom, Dim::Y));
}

// Where m_text goes (aligned, or scrolled to the caret) is decided before the
// pass places it, from this control's size and the measured string: in X with
// the final width, and again in Y with the final height. Both are placement of
// a child laid out next. The caret and selection rectangles are taken once it
// is placed.
void TextBox::layout(skin::Base *skin)
{
    ParentClass::layout(skin);

    PlacementScope placement;
    makeCaretVisible();
}

// The message line goes under the field part, from the field padding's left
// to the box's right edge (which cuts a longer message), at the line's height.
// Each axis is placed before the children are arranged along it.
void TextBox::arrange(skin::Base *skin, Dim dim)
{
    if(m_message)
    {
        PlacementScope placement;
        internal::Text *text=m_message->text;
        const Rect &bounds=text->getBounds();

        if(dim == Dim::X)
        {
            const int left=m_message->fieldPadding.left;

            placeChild(text, left, bounds.y, std::max(0, getWidth()-left), bounds.h);
        }
        else
        {
            const int room=m_message->room;

            placeChild(text, bounds.x, getHeight()-room+MessageGap, bounds.w, std::max(0, room-MessageGap));
        }
    }

    if(dim == Dim::Y)
    {
        PlacementScope placement;
        makeCaretVisible();
    }

    ParentClass::arrange(skin, dim);
}

void TextBox::postLayout(skin::Base *skin)
{
    updateCursorRects();
}

void TextBox::onTextChanged()
{
    m_cursorPos=snapPos(getText(), std::min(m_cursorPos, textLength()));
    m_cursorEnd=snapPos(getText(), std::min(m_cursorEnd, textLength()));

    if(hasValidator() && (m_message->validateOn & ValidateOn::Change))
        validate();

    onTextChangedCaller.call(this);
}

// Return is a change and a focus loss at once: the validator runs on the text
// as it is entered (a NumericUpDown's clamped), whichever ValidateOn is set.
void TextBox::onEnter()
{
    validate();
    onReturnPressedCaller.call(this);
}

void TextBox::moveCaretToend()
{
    m_cursorPos=textLength();
    m_cursorEnd=textLength();
    refreshCursorBounds();
}

void TextBox::moveCaretToStart()
{
    m_cursorPos=0;
    m_cursorEnd=0;
    refreshCursorBounds();
}

GWENI_CONTROL_CONSTRUCTOR(TextBoxMultiline)
{
    setWrap(true);
}

bool TextBoxMultiline::onKeyReturn(bool down)
{
    EditKindScope kind(*this, text::EditKind::Insert);

    if(down)
        insertText("\n");

    return true;
}

bool TextBoxMultiline::normalizeText(const String &str, String &converted) const
{
    if(str.find('\r') == String::npos)
        return false;

    converted.clear();
    converted.reserve(str.size());

    for(size_t i=0; i < str.size(); ++i)
    {
        if(str[i] != '\r')
            converted+=str[i];
        else if(i+1 >= str.size() || str[i+1] != '\n')
            converted+='\n';    // a lone "\r"; "\r\n"'s "\n" comes next
    }
    return true;
}

void TextBoxMultiline::makeCaretVisible()
{
    const Size text=m_text->refreshSize(false);
    const Alignment textAlignment=getTextAlignment();

    if(text.height < getFieldHeight())
    {
        alignText(textAlignment);
    }
    else
    {
        //const Rect &bounds = getInnerBounds();

        //if ( pos  &Alignment::Top ) y = bounds.y + ypadding;
        //if ( pos  &Alignment::Bottom ) y = bounds.y + ( bounds.h - getFieldHeight() - ypadding );
        //if ( pos  &Alignment::CenterV ) y = bounds.y + ( bounds.h - getFieldHeight() )   *0.5;

        Rect pos=m_text->getCharacterPosition(displayPos(m_cursorPos));
        int iCaretPos=pos.y; // + pos.h;
        int iRealCaretPos=iCaretPos+m_text->getY();
        //int iSlidingZone =  m_text->getFont()->size; //getWidth()*0.1f

        // If the carat is already in a semi-good position, leave it, but
        // clamped below: the box may have grown since the text was scrolled.
        int y=m_text->getY();

        // bottom of carat too low
        if(iRealCaretPos > getFieldHeight()-pos.h-getPadding().bottom)
        {
            //align bottom
            y=getFieldHeight()-iCaretPos-pos.h-getPadding().bottom;
        }

        // top of carat too low
        if(iRealCaretPos < getPadding().top)
            y=-iCaretPos+getPadding().top;

        // Don't show too much whitespace to the bottom
        if(y+text.height < getFieldHeight()-getPadding().bottom)
            y=-text.height+(getFieldHeight()-getPadding().bottom);

        // Or the top
        if(y > getPadding().top)
            y=getPadding().top;

        int x=0;
        if(textAlignment & Alignment::Left)
            x=getPadding().left;
        if(textAlignment & Alignment::Right)
            x=getWidth()-text.width-getPadding().right;
        if(textAlignment & Alignment::CenterH)
            x=(getWidth()-text.width)*0.5f;

        placeScrolledText(x, y, text);
    }
}

int TextBoxMultiline::getCurrentLine()
{
    return m_text->getLineFromChar(displayPos(m_cursorPos));
}

// Columns count codepoints, so Up and Down keep the caret over the same
// character count whatever the bytes of the lines are.
int TextBoxMultiline::columnOf(int pos)
{
    const String &str=getText();
    int column=0;

    for(int i=realPos(m_text->getStartCharFromLine(m_text->getLineFromChar(displayPos(pos)))); i < pos; i=nextPos(str, i))
        ++column;

    return column;
}

int TextBoxMultiline::columnOnLine(int line, int column)
{
    const String &str=getText();
    const int last=realPos(m_text->getLastCaretOnLine(line));
    int pos=realPos(m_text->getStartCharFromLine(line));

    for(; column > 0 && pos < last; --column)
        pos=nextPos(str, pos);

    return std::min(pos, last);
}

bool TextBoxMultiline::onKeyHome(bool down)
{
    if(!down)
        return true;

    int iCurrentLine=getCurrentLine();
    int value=realPos(m_text->getStartCharFromLine(iCurrentLine));
    m_cursorLine=0;
    m_cursorPos=value;

    if(!gweni::input::isShiftDown())
        m_cursorEnd=m_cursorPos;

    refreshCursorBounds();
    return true;
}

bool TextBoxMultiline::onKeyend(bool down)
{
    if(!down)
        return true;

    m_cursorLine=0;
    m_cursorPos=realPos(m_text->getLastCaretOnLine(getCurrentLine()));

    if(!gweni::input::isShiftDown())
        m_cursorEnd=m_cursorPos;

    refreshCursorBounds();
    return true;
}

bool TextBoxMultiline::onKeyUp(bool down)
{
    if(!down)
        return true;
    //if ( m_cursorLine == 0 )
    m_cursorLine=columnOf(m_cursorPos);

    int iLine=m_text->getLineFromChar(displayPos(m_cursorPos));

    if(iLine == 0)
        return true;

    // The same column on the line above, or that line's last caret position.
    m_cursorPos=columnOnLine(iLine-1, m_cursorLine);

    if(!gweni::input::isShiftDown())
        m_cursorEnd=m_cursorPos;

    refreshCursorBounds();
    return true;
}

bool TextBoxMultiline::onKeyDown(bool down)
{
    if(!down)
        return true;
    //if ( m_cursorLine == 0 )
    m_cursorLine=columnOf(m_cursorPos);

    int iLine=m_text->getLineFromChar(displayPos(m_cursorPos));
    int iLastLine=m_text->numLines()-1;
    if(iLine >= iLastLine || iLastLine < 1)
        return true;

    // The same column on the line below, or that line's last caret position.
    m_cursorPos=columnOnLine(iLine+1, m_cursorLine);

    if(!gweni::input::isShiftDown())
        m_cursorEnd=m_cursorPos;

    refreshCursorBounds();
    return true;
}

// Whether str is exactly one valid UTF-8 codepoint: its lead byte's length,
// continuation bytes after it and nothing more, encoded the shortest way (so
// no overlong form), and no surrogate or value past U+10FFFF.
static bool isOneCodepoint(const String &str)
{
    if(str.empty())
        return false;

    const unsigned char lead=static_cast<unsigned char>(str[0]);
    size_t length;
    char32_t c;

    if(lead < 0x80)
    {
        length=1;
        c=lead;
    }
    else if((lead & 0xE0) == 0xC0)
    {
        length=2;
        c=lead & 0x1F;
    }
    else if((lead & 0xF0) == 0xE0)
    {
        length=3;
        c=lead & 0x0F;
    }
    else if((lead & 0xF8) == 0xF0)
    {
        length=4;
        c=lead & 0x07;
    }
    else
        return false;

    if(str.size() != length)
        return false;

    for(size_t i=1; i < length; ++i)
    {
        const unsigned char byte=static_cast<unsigned char>(str[i]);

        if((byte & 0xC0) != 0x80)
            return false;

        c=(c << 6) | (byte & 0x3F);
    }

    return utf8Encode(c) == str;
}

GWENI_CONTROL_CONSTRUCTOR(PasswordTextBox)
{
    m_realText="";
    m_passwordChar="*";
}

void PasswordTextBox::setText(const String &strIn, bool doEvents)
{
    String converted;
    const String &str=normalizeText(strIn, converted)?converted:strIn;

    if(m_realText == str)
    {
        m_applyingEdit=false;
        return;
    }

    textSetting();
    m_realText=str;
    updateMask();
    redraw();
    updatePlaceholder();

    if(doEvents)
        onTextChanged();
}

void PasswordTextBox::updateMask()
{
    String mask;

    for(int i=0; i < textLength(); i=nextPos(m_realText, i))
        mask+=m_passwordChar;

    m_text->setText(mask);
}

bool PasswordTextBox::setPasswordChar(const String &c)
{
    if(!isOneCodepoint(c))
        return false;

    if(c == m_passwordChar)
        return true;

    m_passwordChar=c;
    updateMask();
    return true;
}

bool PasswordTextBox::setPasswordChar(const char c)
{
    if(static_cast<unsigned char>(c) >= 0x80)
        return false;

    return setPasswordChar(String(1, c));
}

int PasswordTextBox::displayPos(int realPos) const
{
    int codepoints=0;

    for(int i=0; i < realPos && i < static_cast<int>(m_realText.size()); i=nextPos(m_realText, i))
        ++codepoints;

    return codepoints*static_cast<int>(m_passwordChar.size());
}

int PasswordTextBox::realPos(int displayPos) const
{
    int pos=0;

    for(int k=displayPos/static_cast<int>(m_passwordChar.size()); k > 0 && pos < static_cast<int>(m_realText.size()); --k)
        pos=nextPos(m_realText, pos);

    return pos;
}

}//namespace controls
}//namespace gweni
