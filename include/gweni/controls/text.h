/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_text_h_
#define _gweni_controls_text_h_

#include <gweni/platforms/baseRender.h>
#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
namespace internal
{

class GWENI_EXPORT Text: public controls::Base
{
public:

    GWENI_CONTROL(Text, Base);

public:
    virtual ~Text();
    const gweni::Font &getFont() const;

    void setText(const String &str);

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

    //! Wrapped, the lines take the colour too: they draw the string.
    virtual void setTextColor(const gweni::Color &col)
    {
        if(m_color == col)
            return;

        m_color=col;
        addStateChange(controls::StateChange_Text);

        for(auto &&line : m_lines)
            line->setTextColor(col);
    }

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
    }

    virtual bool wrap()
    {
        return m_wrap;
    }

    virtual void setWrap(bool b)
    {
        if(m_wrap == b)
            return;

        m_wrap=b; m_wrapStale=true; invalidate();
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
    void policyResize(const Size &preferred, Dim dim) override;

protected:

    //! Splits s into words (each with its trailing space), "\n" tokens and,
    //! when w > 0, pieces of words wider than w.
    virtual void splitWords(const gweni::String &s, int w, std::vector<gweni::String> &elems);

private:

    //! A wrapped line: what it shows, and how many characters of m_string it
    //! holds (the shown ones and the '\n' that ended it, if one did).
    struct WrappedLine
    {
        gweni::String text;
        int length;
    };

    //! Breaks m_string into lines no wider than w ('\n' always breaks one).
    void wrapLines(int w, std::vector<WrappedLine> &lines);

    //! Makes m_wrapped the wrap of this string and font at w, wrapping again
    //! only when one of them changed (wrapCurrent()). Touches no child.
    void wrapAt(int w);
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
    //! The height of a wrapped line, the row each one is placed on.
    int lineHeight();

    //! An unwrapped string's caret rect at byte value, and the codepoint
    //! boundary closest to p; a line child's own helpers, read here from a
    //! line's text so they need no child.
    gweni::Rect characterPositionIn(const gweni::String &str, unsigned int value);
    int closestCharacterIn(const gweni::String &str, gweni::Point p);

    gweni::String m_string;
    const gweni::Font *m_font;
    gweni::Color m_color;
    gweni::Color m_colorOverride;

    // Initialised here: the constructor's setWrap(false) reads m_wrap.
    bool m_wrap=false;
    //! The wrap of m_string, as data: what the helpers read, and what layout
    //! makes the line children show.
    std::vector<WrappedLine> m_wrapped;
    //! Set by setText, setFont, setWrap and textChanged: m_wrapped is not of
    //! this string. Then the width and font it was wrapped at; the font is
    //! kept by value as well, as one can change size in place (setDefaultFont).
    bool m_wrapStale=true;
    int m_wrapWidth=-1;
    const gweni::Font *m_wrapFont=nullptr;
    gweni::Font m_wrapFontValue;
    size_t m_wrapCount=0;

    //! The line children do not show m_wrapped (it was wrapped again since
    //! layout last made them). The width they were laid out at, -1 before
    //! the first wrapped layout: until then the helpers read m_string.
    bool m_linesStale=true;
    int m_wrappedWidth=-1;
//    size_t m_textId;

    //! The line children, top to bottom: m_wrapped's lines after layout,
    //! updated in place.
    std::vector<Text *> m_lines;
};

}//namespace internal
}//namespace controls
}//namespace gweni

#endif//_gweni_controls_text_h_
