/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_labelclickable_h_
#define _gweni_controls_labelclickable_h_

#include <gweni/controls/base.h>
#include <gweni/controls/button.h>

namespace gweni
{
namespace controls
{
class GWENI_EXPORT LabelClickable: public Button
{
public:

    GWENI_CONTROL(LabelClickable, Button);

    void render(skin::Base *skin) override;
    void updateColors() override;
};


}
}
#endif//_gweni_controls_labelclickable_h_
