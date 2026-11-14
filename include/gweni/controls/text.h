/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_text_h_
#define _gweni_controls_text_h_

#include <gweni/platforms/baseRender.h>
#include <gweni/controls/base.h>
#include <gweni/text/wrap.h>

namespace gweni
{
namespace controls
{
namespace internal
{

//! Which of the skin's text colours a Text shows, so that a theme changed at
//! run time (tools::ThemeEditor) can give it the colour's new value: a Text
//! follows one until something sets it a colour of its own (setTextColor).
//! The colours a control picks for its state in updateColors() (a Button's,
//! a Selectable's) need none: updateColors() reads the skin again.
enum class SkinTextColor
{
    None,           //!< a colour of its own: nothing to follow
    LabelDefault,   //!< Colors.Label.Default, what a new Text shows
    LabelBright,    //!< Colors.Label.Bright
    LabelDark,      //!< Colors.Label.Dark
    LabelHighlight, //!< Colors.Label.Highlight
    Placeholder,    //!< Colors.PlaceholderText, a TextBox's hint
    ErrorText,      //!< Colors.ErrorText, a TextBox's error message
    HelperText,     //!< Colors.HelperText, a TextBox's helper text
    CodePlain       //!< Colors.CodeEditor's Plain token colour, a CodeEditor line's
};

class GWENI_EXPORT Text: public controls::Base
{
public:

    GWENI_CONTROL(Text, Base);

public:
    virtual ~Text();
    const gweni::Font &getFont() const;

    void setText(const String &str);

    //! With it on, a new string that measures as the old one did (unwrapped,
    //! on a canvas, the old one measured) is measured in setText() and lays
    //! nothing out: only the glyphs are redrawn. Off by default, where every
    //! new string lays the Text out again (a TextBox places its caret in the
    //! layout). A Chip turns it on.
    void setSameSizeKeepsLayout(bool keep) { m_sameSizeKeepsLayout=keep; }

//    void render(skin::Base *skin) override;
    void layout(skin::Base *skin) override;

    //! Wrapped, the children are this control's own lines, laid out inside
    //! it: they are clipped by the parent's region, as an unwrapped Text's own
    //! glyphs are, and a new wrap height re-stamps no line's clip box.
    bool shouldClip() override { return false; }

    Size refreshSize(bool update=true);

    void setFont(const gweni::Font &font);

    const String &getText() const
    {
        return m_string;
    }

    //! Where the string is drawn, in the parent's coordinates: inside this
    //! control's padding, which refreshSize() adds to the measured string.
    gweni::Point textOrigin() const
    {
        return gweni::Point(getBounds().x+getPadding().left, getBounds().y+getPadding().top);
    }

    gweni::Rect getCharacterPosition(unsigned int value);
    int getClosestCharacter(gweni::Point p);

    //! Return the encapsulating box of the given line.
    //! @remark This function is necessary to implement multiline selection.
    virtual gweni::Rect getLineBox(int i);

    int length() const
    {
        return static_cast<int>(m_string.size());
    }

    //! Wrapped, the lines take the colour too: they draw the string. A colour
    //! set here is the Text's own: it no longer follows a skin colour.
    virtual void setTextColor(const gweni::Color &col)
    {
        m_skinColor=SkinTextColor::None;
        applyTextColor(col);
    }

    //! Shows the skin's colour \p which and follows it: refreshSkinTextColor()
    //! reads it again. None only stops following, keeping the colour.
    void setSkinTextColor(SkinTextColor which);
    SkinTextColor getSkinTextColor() const { return m_skinColor; }
    //! Takes the current value of the skin colour it follows, if it follows
    //! one (marking StateChange_Text only if that is a new colour).
    void refreshSkinTextColor();

    virtual const gweni::Color &getTextColor()
    {
        return m_color;
    }

    virtual void setTextColorOverride(const gweni::Color &col)
    {
        m_colorOverride=col;

        for(auto &&line : m_lines)
            line->setTextColorOverride(col);
    }

    virtual const gweni::Color &getTextColorOverride()
    {
        return m_colorOverride;
    }

    inline const gweni::Color &TextColor() const
    {
        return m_color;
    }

    virtual void textChanged()
    {
        m_wrapStale=true;
        dropMeasures();
    }

    virtual bool wrap()
    {
        return m_wrap;
    }

    virtual void setWrap(bool b)
    {
        if(m_wrap == b)
            return;

        m_wrap=b; m_wrapStale=true; dropMeasures(); invalidate();
    }

    //! Wrapped lines are child Texts whose strings have no '\n'; character
    //! indices are into getText(), which counts each '\n' in the line it ends.
    //! The helpers below read the wrap of the current string, re-wrapped at
    //! the last layout's width when it changed since, so they are right
    //! between an edit and the next layout. The line children are brought up
    //! to date only by layout: getLine() is the child, valid after it.
    virtual Text *getLine(int i);
    virtual int   getLineFromChar(int i);
    virtual int   getStartCharFromLine(int i);
    //! One past the line's last character in getText(), its '\n' included.
    virtual int   getEndCharFromLine(int i);
    //! The last caret position on the line: before its '\n', or before its
    //! last character when it wraps (after it is the next line's start), or
    //! the end of the text on the last line.
    virtual int   getLastCaretOnLine(int i);
    //! The characters of getText() the line holds, its '\n' included.
    virtual int   getLineLength(int i);
    virtual int   getCharPosOnLine(int i);
    virtual unsigned numLines();

    //! How many times the string has been wrapped (wrapLines run) by this
    //! control, for tests: once per edit, however many helpers ask.
    size_t wrapCount() const
    {
        return m_wrapCount;
    }

    void calculateSize(skin::Base *skin, Dim dim) override;
    void arrange(skin::Base *skin, Dim dim) override;
    //! Wrapped, it is as wide as its parent's requested inner width, and its
    //! lines break at the parent's inner width.
    bool readsParentLayout() const override { return m_wrap; }
    void policyResize(const Size &preferred, Dim dim) override;

private:

    //! A wrapped line of m_string (text::wrapLines).
    using WrappedLine=text::WrappedLine;

    //! Breaks s into lines no wider than w ('\n' always breaks one), appended
    //! to lines: text::wrapLines, measuring in this Text's font through
    //! measureText(), as Base::wrappedTextLines() does.
    void wrapLines(const gweni::String &s, int w, std::vector<WrappedLine> &lines);

    //! Makes m_wrapped the wrap of this string and font at w, wrapping again
    //! only when one of them changed (wrapCurrent()): only the edited
    //! paragraphs when the string is all that changed (wrapEdit()), the whole
    //! string otherwise. Touches no child.
    void wrapAt(int w);
    //! Records the edit setText makes, old into the new string, as the bytes
    //! that differ (common prefix and suffix), merged with one not wrapped yet.
    void noteEdit(const gweni::String &oldString, const gweni::String &newString);
    //! Wraps again the paragraphs of the pending edit (m_edit*) and splices
    //! their lines into m_wrapped in place of the old ones, recording the
    //! lines the children no longer show (m_sync*).
    void wrapEdit(int w);
    //! Makes the line children show m_wrapped: only the lines m_sync* names,
    //! and the rows below them when their count changed, if that is all that
    //! changed since the children last showed the wrap (syncLines()), or
    //! every line by index.
    void syncLines(int lineHeight);
    //! Gives line child i m_wrapped[i]'s string, size and row; unless always,
    //! not when it shows that string on that row already.
    void syncLine(size_t i, int lineHeight, bool always);
    //! A new line child, inserted into m_lines at index, and among the
    //! children before the line it is inserted before.
    Text *insertLine(size_t index);
    //! Whether m_wrapped is the wrap of this string and font at w.
    bool wrapCurrent(int w) const;
    //! For the char <-> line helpers: whether they read m_wrapped (wrapped
    //! and laid out once, so a width is known), re-wrapped first if the
    //! string or font changed since. False: they read m_string unwrapped.
    bool ensureWrapped();

    //! Wraps the string at wrapWidth(); with update, makes the line children
    //! match m_wrapped, reusing the ones there are. Does nothing while the
    //! lines are of the wrap at that width (linesCurrent()).
    virtual Size RefreshSizeWrap(bool update=true);
    //! Whether the line children show the wrap of this string and font at width.
    bool linesCurrent(int width) const;
    //! Drops the line children past the first count.
    void removeLines(size_t count);
    //! The width wrapped lines break at: the parent's, less its padding.
    int wrapWidth();
    //! The measured width of the widest word of the string (a wrapped line
    //! never breaks inside one unless it is wider than the line): what a
    //! wrapped Text asks for when its parent has no width asked by the app.
    int longestWordWidth();
    //! The height of a wrapped line, the row each one is placed on: the
    //! renderer's lineHeight() of the font, kept as measureKeyCurrent() says.
    int lineHeight();

    //! The measured size of the whole string, before padding: measureText()
    //! of it, kept until setText or the key below changes.
    gweni::Point measuredString();
    //! Whether the measures kept below were made by this renderer, in this
    //! font (the same object holding the same value: a Font can change size in
    //! place, setDefaultFont) and before the renderer last said text measures
    //! otherwise (textLayoutSerial(): a new text AA mode, textLayoutChanged()).
    //! The scale is no part of it: measureText() and lineHeight() are in canvas
    //! units at every scale, so a Canvas::setScale lays out without measuring.
    bool measureKeyCurrent();
    //! Records the key when it is not current, dropping every kept measure.
    void renewMeasureKey();
    //! Drops every kept measure: the next one asks the renderer.
    void dropMeasures();

    //! An unwrapped string's caret rect at byte value, and the codepoint
    //! boundary closest to p; a line child's own helpers, read here from a
    //! line's text so they need no child.
    gweni::Rect characterPositionIn(const gweni::String &str, unsigned int value);
    int closestCharacterIn(const gweni::String &str, gweni::Point p);

    gweni::String m_string;
    const gweni::Font *m_font;
    gweni::Color m_color;
    gweni::Color m_colorOverride;
    //! The skin colour m_color follows (setSkinTextColor), None for its own.
    SkinTextColor m_skinColor=SkinTextColor::LabelDefault;

    //! Sets m_color and the lines' colour, marking StateChange_Text for a new
    //! one; which skin colour it follows is the caller's.
    void applyTextColor(const gweni::Color &col);

    // Initialised here: the constructor's setWrap(false) reads m_wrap.
    bool m_wrap=false;
    //! The wrap of m_string, as data: what the helpers read, and what layout
    //! makes the line children show.
    std::vector<WrappedLine> m_wrapped;
    //! Set by setFont, setWrap and textChanged, and by setText before the
    //! first wrap: m_wrapped is to be wrapped again whole. Then the width and
    //! font it was wrapped at; the font is kept by value as well, as one can
    //! change size in place (setDefaultFont).
    bool m_wrapStale=true;
    //! Set by setText once there is a wrap: m_wrapped is of an earlier string,
    //! whose bytes [m_editStart, m_editOldEnd) are [m_editStart, m_editNewEnd)
    //! of m_string, everything before and after them the same. Merged over
    //! setTexts until the next wrap, which re-wraps only the paragraphs they
    //! touch.
    bool m_editPending=false;
    size_t m_editStart=0;
    size_t m_editOldEnd=0;
    size_t m_editNewEnd=0;
    int m_wrapWidth=-1;
    const gweni::Font *m_wrapFont=nullptr;
    gweni::Font m_wrapFontValue;
    size_t m_wrapCount=0;

    //! The measures a layout pass asks for again and again (every
    //! calculateSize, in both axes, and a ScrollControl's own re-measure of its
    //! children), each kept with a flag saying it is of the current string (or
    //! font alone, for the line height) under the key m_measure* records.
    //! setText drops the string's two; setFont, setWrap and textChanged all of
    //! them. Substring measures (caret places, wrap trials) are not kept.
    const gweni::Font *m_measureFont=nullptr;
    gweni::Font m_measureFontValue;
    const renderer::Base *m_measureRenderer=nullptr;
    unsigned m_measureSerial=0;
    bool m_stringMeasured=false;
    //! See setSameSizeKeepsLayout()
    bool m_sameSizeKeepsLayout=false;
    gweni::Point m_stringSize;
    bool m_lineHeightMeasured=false;
    int m_lineHeight=0;
    bool m_longestWordMeasured=false;
    int m_longestWord=0;

    //! The line children do not show m_wrapped (it was wrapped again since
    //! layout last made them). The width they were laid out at, -1 before
    //! the first wrapped layout: until then the helpers read m_string.
    bool m_linesStale=true;
    int m_wrappedWidth=-1;
    //! With m_linesStale, whether only edits made it so: line children
    //! [m_syncFirst, m_syncOldEnd) show what m_wrapped's [m_syncFirst,
    //! m_syncNewEnd) replaced, and every other one its line (moved down or up
    //! by the difference). Else every child is synced by index.
    bool m_linesPartial=false;
    size_t m_syncFirst=0;
    size_t m_syncOldEnd=0;
    size_t m_syncNewEnd=0;
//    size_t m_textId;

    //! The line children, top to bottom: m_wrapped's lines after layout,
    //! updated in place.
    std::vector<Text *> m_lines;
    //! The row height calculateSize last gave every line, -1 before it has.
    int m_linesRowHeight=-1;
};

}//namespace internal
}//namespace controls
}//namespace gweni

#endif//_gweni_controls_text_h_
