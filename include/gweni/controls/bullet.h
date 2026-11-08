/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_bullet_h_
#define _gweni_controls_bullet_h_

#include <gweni/controls/label.h>

namespace gweni
{
namespace controls
{

//! A bullet point: a small filled circle in the text colour, followed by the
//! label's text if it has any (imgui's Bullet and BulletText are this with and
//! without a string). The circle sits in a column on the left as wide as the
//! font's size plus columnGap(), so bullets of one font stacked in a column
//! line up, and it is centred on the first text line.
class GWENI_EXPORT Bullet: public Label
{
public:

    GWENI_CONTROL(Bullet, Label);

public:

    void setFont(gweni::String strFacename, int iSize, bool bBold) override;
    void setFont(const gweni::Font &font) override;

    void setTextColor(const gweni::Color &col) override;
    void setTextColorOverride(const gweni::Color &col) override;
    void setSkinTextColor(internal::SkinTextColor which) override;

    //! Logical units between the bullet's column of font size and the text.
    void setColumnGap(int gap);
    int columnGap() const { return m_columnGap; }

    //! The circle, in this control's coordinates: its diameter is 40 % of the
    //! font size (at least 3 units), centred in the font-size wide column and
    //! on the first line of the text.
    gweni::Rect bulletRect();

    //! The colour the text is drawn in (its override, if one is set), which
    //! the circle takes too.
    gweni::Color bulletColor();

    void onChildBoundsChanged(gweni::Rect oldChildBounds, Base *child) override;

protected:

    //! Makes the left padding the bullet's column for the current font.
    void updateColumn();

    int m_columnGap=4;
};

}
}
#endif//_gweni_controls_bullet_h_
