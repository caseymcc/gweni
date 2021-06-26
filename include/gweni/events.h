/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_events_h_
#define _gweni_events_h_

#include <gweni/config.h>
#include <gweni/uiTypes.h>
#include <gweni/controlList.h>

#include <functional>
#include <variant>

namespace gweni
{

namespace controls
{

class Base;

}//namespace controls

namespace event
{

class Caller;

/**
    * When adding an event hook you can add a Packet too.
    * This will be passed in event::Info when you receive an event.
    */
class Packet
{
public:
    Packet(controls::Base *control=nullptr):
        value(control)
    {}

    bool isControl() { return (value.index() == 0); }
    controls::Base *getControl() { return std::get<controls::Base *>(value); }

private:
    std::variant<controls::Base *, String, int, float, unsigned long long> value;

    // TODO - union? variant?
//    controls::Base *control;
//    String stringValue;
//    int intValue;
//    float floatValue;
//    unsigned long long unsignedLongLong;
};


/**
    * Passed to an event caller.
    */
struct Info
{
    Info(controls::Base *ctrl):
        controlCaller(nullptr),
        packet(nullptr),
        control(ctrl),
        integer(0)
    {}

    /// This is set by the event Caller, it will always be
    /// the control which is calling the event.
    controls::Base *controlCaller;

    /// This is set by the event hook, e.g. :-
    /// ~~~
    /// onDoSomething.add( this, &ThisClass::MyFunction, event::Packet("Something") );
    /// ~~~
    event::Packet *packet;

    /// These are set by the event and may or may not be set.
    controls::Base *control;

    String string;
    Point point;
    int integer;
};

/**
    * A class wanting to receive events must be derived from this.
    */
class GWENI_EXPORT Handler
{
public:

    virtual ~Handler();

    void registerCaller(Caller *);
    void unRegisterCaller(Caller *);

protected:

    Handler();  // Class needs to be derived.

private:

    void cleanLinks();
    std::list<Caller *> m_callers;
};

/**
    * \brief Event caller.
    *
    * Event handler callbacks are registered with this to receive event notifications.
    *
    *     Control (event) -> Caller (call) -> Handler (callback)
    *
    */
class GWENI_EXPORT Caller
{
public:

    typedef void (Handler:: *EventCaller)(event::Info info);

    typedef std::function<void(Handler &, event::Info)> EventCallback;

    Caller();
    ~Caller();

    // add function object handler
    void addCallback(Handler *handler, EventCallback const &cb,
        const event::Packet &packet=event::Packet())
    {
        addInternal(handler, cb, packet);
    }

    // add class method handler
    template <typename T>
    void add(Handler *handler,
        void (T:: *f)(Info),
        const Packet &packet=Packet())
    {
        auto cb=[=](Handler &h, Info const &i) -> void {
            (static_cast<T &>(h).*f)(i);
        };
        addCallback(handler, EventCallback(cb), packet);
    }

    void removeHandler(event::Handler *object);

    void call(controls::Base *pThis);
    void call(controls::Base *pThis, event::Info info);

private:

    void addInternal(Handler *handler, EventCallback const &ecb,
        const event::Packet &packet);

    void cleanLinks();

    struct HandlerInstance
    {
        EventCallback callback;
        Packet packet;
        Handler *handler;
    };

    std::list<HandlerInstance> m_handlers;
};

} // namespace event
} // namespace gweni

#endif//_gweni_events_h_
