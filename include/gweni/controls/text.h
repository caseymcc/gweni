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

    GWENI_CONTROL(Text, controls::Base);

    virtual ~Text();
    const gweni::Font &getFont() const;

    void setString(const String &str);

    void render(skin::Base *skin) override;
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
        m_color=col;
    }

    virtual void setTextColorOverride(const gweni::Color &col)
    {
        m_colorOverride=col;
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

    virtual Text *getLine(int i);
    virtual int   getLineFromChar(int i);
    virtual int   getStartCharFromLine(int i);
    virtual int   getEndCharFromLine(int i);
    virtual int   getCharPosOnLine(int i);
    virtual unsigned numLines();

    void calculateSize(skin::Base *skin, Dim dim) override;
    void arrange(skin::Base *skin, Dim dim) override;

protected:

    virtual void splitWords(const gweni::String &s, std::vector<gweni::String> &elems);

private:

    virtual Size RefreshSizeWrap(bool update=true);

    gweni::String m_string;
    const gweni::Font *m_font;
    gweni::Color m_color;
    gweni::Color m_colorOverride;

    bool m_wrap;
    bool m_textChanged;
    size_t m_textId;

    typedef std::list<Text *> TextLines;
    TextLines m_lines;
};

}//namespace internal
}//namespace controls
}//namespace gweni

#endif//_gweni_controls_text_h_
