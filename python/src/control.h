/*
 *  Gweni - control handles (python/src): a gweni.Control is a proxy holding
 *  its control's id, resolved through Base::getControlById on every use (an
 *  O(1) lookup; ids are never reused), so a deleted control raises
 *  gweni.ControlDeletedError and never crashes, and Python's garbage
 *  collector never deletes a control: the C++ tree owns them all.
 *
 *  Each bound class is a struct deriving from Control with no state of its
 *  own, registered with the control type it shows (registerControl), so a
 *  control C++ made comes back as its bound class by type name (wrap), and
 *  parent.add(Type, **props) makes one (createChild).
 *
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */
#ifndef _gweni_python_control_h_
#define _gweni_python_control_h_

#include "session.h"

#include <gweni/controls/base.h>

#include <nanobind/nanobind.h>

#include <cstddef>
#include <functional>
#include <string>
#include <type_traits>
#include <typeinfo>

namespace gweni
{
namespace python
{

struct Control
{
    size_t id=0;
};

//! The live control behind \p handle: ThreadError off the session's thread,
//! ControlDeletedError once it is deleted.
controls::Base *resolveBase(const Control &handle);

template<typename T>
T *resolve(const Control &handle)
{
    controls::Base *base=resolveBase(handle);

    if constexpr(std::is_same_v<T, controls::Base>)
        return base;
    else
    {
        T *control=dynamic_cast<T *>(base);

        if(control == nullptr)
            throw std::logic_error(std::string("gweni: control #")+std::to_string(handle.id)+" is a "
                +base->getTypeName()+", not the type its handle was made for");
        return control;
    }
}

//! The handle of \p control as its bound class: the class registered for its
//! type name, else for the nearest parent type that has one (but Base's,
//! Panel, which only a plain Base gets), else Control. None for null.
nanobind::object wrap(controls::Base *control);

//! wrap() annotated as \p H for the stubs (a bound handle class, or
//! std::optional of one where it may be None); the object is wrap()'s.
template<typename H>
using Typed=nanobind::typed<nanobind::object, H>;

template<typename H>
Typed<H> wrapAs(controls::Base *control)
{
    return nanobind::borrow<Typed<H>>(wrap(control));
}

using CreateChild=std::function<controls::Base *(controls::Base *parent)>;
using MakeHandle=std::function<nanobind::object(size_t id)>;

void registerControlType(const char *typeName, nanobind::handle pyType, MakeHandle make, CreateChild create);

//! Registers \p Handle, bound as \p pyType, for control type \p T: wrap()
//! gives it to a T, and add(pyType, ...) makes a T (when \p Creatable).
//! \p typeName is the control's getTypeName() where T has no
//! getTypeNameStatic() of its own (Canvas).
template<typename Handle, typename T, bool Creatable=true>
void registerControl(nanobind::handle pyType, const char *typeName=T::getTypeNameStatic())
{
    CreateChild create;

    if constexpr(Creatable)
        create=[](controls::Base *parent) -> controls::Base * { return parent->newChild<T>(); };
    registerControlType(typeName, pyType,
        [](size_t id) { Handle handle; handle.id=id; return nanobind::cast(handle); }, create);
}

void bindControl(nanobind::module_ &m);

}//namespace python
}//namespace gweni

#endif//_gweni_python_control_h_
