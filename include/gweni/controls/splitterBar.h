/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_splitterbar_h_
#define _gweni_controls_splitterbar_h_

#include <gweni/gweni.h>
#include <gweni/controls/base.h>
#include <gweni/controls/dragger.h>

namespace gweni
{
namespace controls
{

class GWENI_EXPORT SplitterBar: public internal::Dragger
{
public:

    GWENI_CONTROL(SplitterBar, internal::Dragger);

public:
    void layout(skin::Base *skin) override;
};


}//namespace controls
}//namespace gweni

#endif//_gweni_controls_splitterbar_h_
