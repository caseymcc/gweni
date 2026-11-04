/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/hook.h>

#if GWENI_HOOKSYSTEM

namespace gweni
{

namespace hook
{

static std::list<Base *> g_hookList;

void addHook(Base *hook)
{
    g_hookList.push_back(hook);
}

void removeHook(Base *hook)
{
    g_hookList.remove(hook);
}

HookList &getHookList()
{
    return g_hookList;
}

}//namespace gweni
}//namespace hook

#endif // if GWENI_HOOKSYSTEM
