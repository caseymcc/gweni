/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_rectangle_h_
#define _gweni_controls_rectangle_h_

#include <gweni/controls/base.h>
#include <gweni/controls/label.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>


namespace gweni
{
namespace controls
{
class GWENI_EXPORT Rectangle: public controls::Base
{
public:

    GWENI_CONTROL(Rectangle, controls::Base);

public:
    void render(skin::Base *skin) override;

    const gweni::Color &getColor()
    {
        return m_color;
    }

    void setColor(const gweni::Color &col)
    {
        m_color=col;
    }

protected:

    gweni::Color m_color;

};


}
}
#endif//_gweni_controls_rectangle_h_
