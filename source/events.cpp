/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/events.h>

namespace gweni
{

namespace event
{

Handler::Handler()
{}

Handler::~Handler()
{
    cleanLinks();
}

void Handler::cleanLinks()
{
    // Tell all the callers that we're dead
    std::list<Caller *>::iterator iter=m_callers.begin();

    while(iter != m_callers.end())
    {
        Caller *Caller=*iter;
        unRegisterCaller(Caller);
        Caller->removeHandler(this);
        iter=m_callers.begin();
    }
}

void Handler::registerCaller(Caller *Caller)
{
    m_callers.push_back(Caller);
}

void Handler::unRegisterCaller(Caller *Caller)
{
    m_callers.remove(Caller);
}

Caller::Caller()
{}

Caller::~Caller()
{
    cleanLinks();
}

void Caller::cleanLinks()
{
    for(auto &&h : m_handlers)
    {
        h.handler->unRegisterCaller(this);
    }

    m_handlers.clear();
}

void Caller::call(controls::Base *base)
{
    event::Info info(base);

    info.control=base;
    call(base, info);
}

void Caller::call(controls::Base *base, event::Info information)
{
    event::Info info(nullptr);
    info=information;
    info.controlCaller=base;

    for(auto &&h : m_handlers)
    {
        info.packet=&h.packet;

        if(h.callback)
            h.callback(*h.handler, info);
    }
}

void Caller::addInternal(Handler *handler, EventCallback const &ecb,
    const gweni::event::Packet &packet)
{
    HandlerInstance h={ecb, packet, handler};
    m_handlers.push_back(h);
    handler->registerCaller(this);
}

void Caller::removeHandler(event::Handler *object)
{
    object->unRegisterCaller(this);
    std::list<HandlerInstance>::iterator iter=m_handlers.begin();

    while(iter != m_handlers.end())
    {
        HandlerInstance &h=*iter;

        if(h.handler == object)
            iter=m_handlers.erase(iter);
        else
            ++iter;
    }
}

}//namespace event
}//gweni
