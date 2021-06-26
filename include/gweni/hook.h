/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_hook_h_
#define _gweni_hook_h_

#include <gweni/controls/base.h>

 // Enable the hook system.
#define GWENI_HOOKSYSTEM 1

#if GWENI_HOOKSYSTEM

#include <list>

namespace gweni
{
namespace hook
{

class GWENI_EXPORT Base
{
public:

    virtual bool onControlClicked(gweni::controls::Base *, int /*iMouseX*/, int /*iMouseY*/)
    {
        return false;
    }

};


typedef std::list<Base *> HookList;

GWENI_EXPORT HookList &getHookList();

GWENI_EXPORT void addHook(Base *hook);
GWENI_EXPORT void removeHook(Base *hook);

template <typename fnc>
bool callHook(fnc f)
{
    for(auto &&hook : getHookList())
    {
        if((hook->*f)())
            return true;
    }

    return false;
}

template <typename fnc, typename AA>
bool callHook(fnc f, AA a)
{
    for(auto &&hook : getHookList())
    {
        if((hook->*f)(a))
            return true;
    }

    return false;
}

template <typename fnc, typename AA, typename AB>
bool callHook(fnc f, AA a, AB b)
{
    for(auto &&hook : getHookList())
    {
        if((hook->*f)(a, b))
            return true;
    }

    return false;
}

template <typename fnc, typename AA, typename AB, typename AC>
bool callHook(fnc f, AA a, AB b, AC c)
{
    for(auto &&hook : getHookList())
    {
        if((hook->*f)(a, b, c))
            return true;
    }

    return false;
}

}//namespace hook
}//namespace gweni

#endif//if GWENI_HOOKSYSTEM
#endif//_gweni_hook_h_
