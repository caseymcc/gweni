/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/controls/modalScrim.h>
#include <gweni/controls/canvas.h>

namespace gweni
{
namespace controls
{
namespace internal
{

GWENI_CONTROL_CONSTRUCTOR(ModalScrim)
{
    // placed and sized by itself, over the whole canvas, and shown by the
    // canvas while a modal is up
    setPlacement(Placement::Owner);
    setMouseInputEnabled(false);
    setKeyboardInputEnabled(false);
    setHidden(true);
}

void ModalScrim::arrange(skin::Base *skin, Dim dim)
{
    Canvas *canvas=getCanvas();

    if(canvas && (getParent() == canvas))
    {
        if(dim == Dim::X)
            setArrangedBounds(0, getY(), canvas->getWidth(), getHeight());
        else
            setArrangedBounds(getX(), 0, getWidth(), canvas->getHeight());
    }

    ParentClass::arrange(skin, dim);
}

}//namespace internal
}//namespace controls
}//namespace gweni
