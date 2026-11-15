/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_modalScrim_h_
#define _gweni_controls_modalScrim_h_

#include <gweni/controls/base.h>
#include <gweni/gweni.h>

namespace gweni
{
namespace controls
{
namespace internal
{

//! Canvas::setModalScrim's backdrop: one slot over the whole canvas in the
//! translucent theme surface ModalScrim, shown while a modal is up and kept just
//! under the top modal (Canvas::keepModalsInPlace), so the app and the modals
//! below the top one show through it dimmed, their text included. It takes no
//! mouse or keyboard input: the modal stack already blocks the app.
class GWENI_EXPORT ModalScrim: public Base
{
public:

    GWENI_CONTROL(ModalScrim, Base);

public:
    //! The canvas's bounds, one axis per arrange (Placement::Owner).
    void arrange(skin::Base *skin, Dim dim) override;
};

}//namespace internal
}//namespace controls
}//namespace gweni

#endif//_gweni_controls_modalScrim_h_
