/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  See license in gweni.h
 */


#include <gweni/controls/codeView.h>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(CodeView)
{
    setReadOnly(true);
    // its content's size, cut to a layout cell or given by a dock: the list's
    // default size is no request of the app's
    setSizeFlags({SizeFlag::Shrink, SizeFlag::Shrink});
    m_widthRequested=false;
    m_heightRequested=false;
}

}//namespace controls
}//namespace gweni
