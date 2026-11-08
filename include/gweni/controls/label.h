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
    //! Frees a setFont font that preDelete did not (a delete that was not a
    //! delayedDelete, or the canvas deleting its children).
    ~Label() override;

    void preDelete(gweni::skin::Base *skin) override;

    virtual void setText(const String &str, bool doEvents=true);

    virtual const String &getText() const
    {
        return m_text->getText();
    }

    //! Alignment of the text within the label (the label's own alignment is
    //! how it is placed within its parent; this is where the glyphs sit).
    virtual void setTextAlignment(Alignment alignment)
    {
        setDefaultTextAlignment(alignment);
        m_textAlignmentSet=true;
    }

    //! Where the text sits in the label, as setTextAlignment() (or the
    //! default) set it.
    Alignment getTextAlignment() const { return m_textAlignment; }

//    void render(skin::Base *skin) override
//    {}

    void postLayout(skin::Base *skin) override;

    virtual void sizeToContents();

    bool hasAlignment() override { return true; }

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

    //! Shows the skin's colour \p which and follows it when a theme changes
    //! it (internal::SkinTextColor); setTextColor() gives it its own again.
    virtual void setSkinTextColor(internal::SkinTextColor which)
    {
        m_text->setSkinTextColor(which);
    }

    virtual void makeColorNormal()
    {
        setSkinTextColor(internal::SkinTextColor::LabelDefault);
    }

    virtual void makeColorBright()
    {
        setSkinTextColor(internal::SkinTextColor::LabelBright);
    }

    virtual void makeColorDark()
    {
        setSkinTextColor(internal::SkinTextColor::LabelDark);
    }

    virtual void makeColorHighlight()
    {
        setSkinTextColor(internal::SkinTextColor::LabelHighlight);
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

    //! Wrapped, the text reads from the top unless setTextAlignment() said
    //! otherwise: centred like a single line, a paragraph taller than the
    //! label would start above it.
    virtual void setWrap(bool b)
    {
        m_text->setWrap(b);
        if(!m_textAlignmentSet)
            setDefaultTextAlignment(b?(Alignment::Left|Alignment::Top):(Alignment::Left|Alignment::CenterV));
    }

    void onBoundsChanged(gweni::Rect oldChildBounds) override;

protected:

    virtual void onTextChanged() {}

    //! A subclass's own text alignment: as setTextAlignment(), but setWrap()
    //! may still change it.
    void setDefaultTextAlignment(Alignment alignment)
    {
        m_textAlignment=alignment;
        m_text->setAlignment(alignment);
    }

    gweni::Font *m_createdFont;
    internal::Text *m_text;
    //! setTextAlignment() was called, so setWrap() leaves the alignment alone
    bool m_textAlignmentSet=false;
    Alignment m_textAlignment=Alignment::Left|Alignment::CenterV;
//    Alignment m_align;

};


}
}

#endif//_gweni_controls_label_h_
