/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  See license in Gweni.h
 */


#ifndef _gweni_window_provider_h_
#define _gweni_window_provider_h_

namespace gweni
{

class WindowProvider
{
public:

    virtual void *getWindow()=0;
};

}//namespace gweni

#endif //_gweni_window_provider_h_
