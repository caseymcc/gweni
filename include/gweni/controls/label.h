/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_label_h_
#define _gweni_controls_label_h_

#include <gweni/controls/base.h>
#include <gweni/controls/text.h>

namespace gweni
{
namespace controls
{
//
//! A control that contains a text item.
//
class GWENI_EXPORT Label: public controls::Base
{
    GWENI_CONTROL(Label, controls::Base);

public:
    void preDelete(gweni::skin::Base *skin) override;

    virtual void setText(const String &str, bool doEvents=true);

    virtual const String &getText() const
    {
        return m_text->getText();
    }

//    void render(skin::Base *skin) override
//    {}

    void postLayout(skin::Base *skin) override;

    virtual void sizeToContents();

    bool hasAlignment() override { return true; }
//    virtual void setAlignment(Position area);
//    virtual Position getAlignment();

    virtual void setFont(gweni::String strFacename, int iSize, bool bBold);

    virtual void setFont(const gweni::Font &font)
    {
        m_text->setFont(font);
    }

    virtual const gweni::Font &getFont() const
    {
        return m_text->getFont();
    }

    virtual void setTextColor(const gweni::Color &col)
    {
        m_text->setTextColor(col);
    }

    virtual void setTextColorOverride(const gweni::Color &col)
    {
        m_text->setTextColorOverride(col);
    }

    inline const gweni::Color &getTextColor() const
    {
        return m_text->TextColor();
    }

    virtual int textWidth() { return m_text->getWidth(); }
    virtual int textRight() { return m_text->right(); }
    virtual int textHeight() { return m_text->getHeight(); }

    virtual int textX() { return m_text->getX(); }
    virtual int textY() { return m_text->getY(); }

    virtual int textLength() { return m_text->length(); }

    gweni::Rect getCharacterPosition(int value);

    virtual void setTextPadding(const Padding &padding)
    {
        m_text->setPadding(padding);
        invalidate();
        invalidateParent();
    }

    virtual const Padding &getTextPadding()
    {
        return m_text->getPadding();
    }

//    Alignment alignment() const
//    {
//        return m_align;
//    }

    virtual void makeColorNormal()
    {
        setTextColor(getSkin()->Colors.Label.Default);
    }

    virtual void makeColorBright()
    {
        setTextColor(getSkin()->Colors.Label.Bright);
    }

    virtual void makeColorDark()
    {
        setTextColor(getSkin()->Colors.Label.Dark);
    }

    virtual void makeColorHighlight()
    {
        setTextColor(getSkin()->Colors.Label.Highlight);
    }

    String getValue() override
    {
        return getText();
    }

    void setValue(const String &value) override
    {
        return setText(value);
    }

    virtual bool wrap()
    {
        return m_text->wrap();
    }

    virtual void setWrap(bool b)
    {
        m_text->setWrap(b);
    }

    void onBoundsChanged(gweni::Rect oldChildBounds) override;

protected:

    virtual void onTextChanged() {}

    gweni::Font *m_createdFont;
    internal::Text *m_text;
//    Alignment m_align;

};


}
}

#endif//_gweni_controls_label_h_
