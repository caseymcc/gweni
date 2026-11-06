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

    Size refreshSize(bool update=true);

    void setFont(const gweni::Font &font);

    const String &getText() const
    {
        return m_string;
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

    virtual void setTextColor(const gweni::Color &col)
    {
        if(m_color == col)
            return;

        m_color=col;
        addStateChange(controls::StateChange_Text);
    }

    virtual const gweni::Color &getTextColor()
    {
        return m_color;
    }

    virtual void setTextColorOverride(const gweni::Color &col)
    {
        m_colorOverride=col;
    }

    virtual const gweni::Color &getTextColorOverride()
    {
        return m_colorOverride;
    }

    void onScaleChanged() override;

    inline const gweni::Color &TextColor() const
    {
        return m_color;
    }

    virtual void textChanged()
    {
        m_textChanged=true;
    }

    virtual bool wrap()
    {
        return m_wrap;
    }

    virtual void setWrap(bool b)
    {
        if(m_wrap == b)
            return;

        m_wrap=b; m_textChanged=true; invalidate();
    }

    //! Wrapped lines are child Texts whose strings have no '\n'; character
    //! indices are into getText(), which counts each '\n' in the line it ends.
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

    virtual Size RefreshSizeWrap(bool update=true);
    //! The width wrapped lines break at: the parent's, less its padding.
    int wrapWidth();
    //! The height of a wrapped line, the row each one is placed on.
    int lineHeight();

    gweni::String m_string;
    const gweni::Font *m_font;
    gweni::Color m_color;
    gweni::Color m_colorOverride;

    // Initialised here: the constructor's setWrap(false) reads m_wrap, and a
    // Text whose string is never set relies on m_textChanged to get measured.
    bool m_wrap=false;
    bool m_textChanged=true;
    //! The wrapWidth() the current lines were built at.
    int m_wrappedWidth=-1;
//    size_t m_textId;

    //! The line children, top to bottom, and the source length of each
    //! (WrappedLine::length), so a line can be updated in place.
    std::vector<Text *> m_lines;
    std::vector<int> m_lineLengths;
};

}//namespace internal
}//namespace controls
}//namespace gweni

#endif//_gweni_controls_text_h_
