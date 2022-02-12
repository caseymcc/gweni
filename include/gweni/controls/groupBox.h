/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_groupbox_h_
#define _gweni_controls_groupbox_h_

#include <gweni/controls/base.h>
#include <gweni/controls/label.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>


namespace gweni
{
namespace controls
{
class GWENI_EXPORT GroupBox: public Label
{
public:

    GWENI_CONTROL(GroupBox, Label);

public:
    //void render(skin::Base *skin) override;
    void layout(skin::Base *skin) override;

    virtual void setInnerMargin(int i)
    {
        m_innerMargin=i;
    }

protected:

    int m_innerMargin;

};


}
}
#endif//_gweni_controls_groupbox_h_
