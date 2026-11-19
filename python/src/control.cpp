/*
 *  Gweni - control handles (control.h).
 *
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#include "control.h"

#include <gweni/controls/layout/layout.h>
#include <gweni/uiTypes.h>

#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <unordered_map>
#include <vector>

namespace nb=nanobind;

namespace gweni
{
namespace python
{

namespace
{

struct ControlType
{
    nb::handle pyType;
    MakeHandle make;
    CreateChild create;
};

//! by control type name
std::unordered_map<std::string, ControlType> &typesByName()
{
    static std::unordered_map<std::string, ControlType> types;
    return types;
}

//! by the Python class, for add()
std::unordered_map<PyObject *, ControlType> &typesByClass()
{
    static std::unordered_map<PyObject *, ControlType> types;
    return types;
}

enum class LayoutKind
{
    None,
    Vertical,
    Horizontal
};

}//namespace

controls::Base *resolveBase(const Control &handle)
{
    checkThread();

    controls::Base *control=controls::Base::getControlById(handle.id);

    if(control == nullptr)
        throw ControlDeleted("gweni: control #"+std::to_string(handle.id)+" has been deleted");
    return control;
}

void registerControlType(const char *typeName, nb::handle pyType, MakeHandle make, CreateChild create)
{
    ControlType type{pyType, std::move(make), std::move(create)};

    typesByName()[typeName]=type;
    typesByClass()[pyType.ptr()]=type;
}

nb::object wrap(controls::Base *control)
{
    if(control == nullptr)
        return nb::none();

    const size_t id=control->getControlId();
    auto &types=typesByName();
    auto found=types.find(control->getTypeName());

    if(found != types.end())
        return found->second.make(id);
    // the nearest bound parent type; a type derived from a plain Base that is
    // bound to nothing is a Control, not a Panel
    for(size_t index=0;; ++index)
    {
        const char *parent=(index == 0) ? control->getParentTypeName() : control->getParentTypeName(index-1);

        if(parent == nullptr || std::string(parent) == controls::Base::getTypeNameStatic())
            break;
        found=types.find(parent);
        if(found != types.end())
            return found->second.make(id);
    }

    Control handle;

    handle.id=id;
    return nb::cast(handle);
}

void bindControl(nb::module_ &m)
{
    nb::enum_<DockPosition>(m, "Dock", "Where a control docks in its parent (DockPosition).")
        .value("NONE", DockPosition::None)
        .value("LEFT", DockPosition::Left)
        .value("RIGHT", DockPosition::Right)
        .value("TOP", DockPosition::Top)
        .value("BOTTOM", DockPosition::Bottom)
        .value("CENTER", DockPosition::Center);

    nb::enum_<LayoutKind>(m, "Layout", "How a control stacks its children (controls::VerticalLayout, HorizontalLayout).")
        .value("NONE", LayoutKind::None)
        .value("VERTICAL", LayoutKind::Vertical)
        .value("HORIZONTAL", LayoutKind::Horizontal);

    nb::class_<Control>(m, "Control",
        "A handle of a control in the C++ tree, which owns it. Every use looks the\n"
        "control up by its id: a deleted one raises ControlDeletedError, and\n"
        "dropping a handle deletes nothing.")
        .def_prop_ro("id", [](const Control &self) { return self.id; },
            "The control's id (Base::getControlId), never reused.")
        .def_prop_ro("alive", [](const Control &self) {
                checkThread();
                return controls::Base::getControlById(self.id) != nullptr;
            }, "Whether the control still exists.")
        .def_prop_ro("type_name", [](const Control &self) {
                return std::string(resolveBase(self)->getTypeName());
            }, "The C++ type name (Base::getTypeName).")
        .def_prop_ro("parent", [](const Control &self) {
                return wrap(resolveBase(self)->getParent());
            }, "The parent's handle, or None.")
        .def_prop_ro("children", [](const Control &self) {
                std::vector<nb::object> children;

                for(controls::Base *child : resolveBase(self)->getChildren())
                    children.push_back(wrap(child));
                return children;
            }, "The children's handles, in order (an inner panel's, where there is one).")
        .def("add", [](const Control &self, nb::type_object type, nb::kwargs props) {
                controls::Base *parent=resolveBase(self);
                auto found=typesByClass().find(type.ptr());

                if(found == typesByClass().end() || !found->second.create)
                    throw nb::type_error(("gweni: "+nb::cast<std::string>(nb::str(type))
                        +" is not a control type add() can make").c_str());
                for(auto item : props)
                {
                    if(!nb::hasattr(type, item.first))
                        throw nb::attribute_error(("gweni: "+nb::cast<std::string>(type.attr("__name__"))
                            +" has no property "+nb::cast<std::string>(nb::str(item.first))).c_str());
                }

                controls::Base *child=found->second.create(parent);
                nb::object handle=found->second.make(child->getControlId());

                try
                {
                    for(auto item : props)
                        nb::setattr(handle, item.first, item.second);
                }
                catch(...)
                {
                    child->delayedDelete();
                    throw;
                }
                return handle;
            }, nb::arg("type"), nb::arg("props"),
            "Makes a child of \\p type, sets each keyword as a property and returns its handle.")
        .def("delete_later", [](const Control &self) { resolveBase(self)->delayedDelete(); },
            "Deletes the control in the next frame's update (Base::delayedDelete).")
        .def_prop_rw("dock",
            [](const Control &self) { return resolveBase(self)->getDock(); },
            [](const Control &self, DockPosition dock) { resolveBase(self)->setDock(dock); },
            "Where it docks in its parent.")
        .def_prop_rw("layout",
            [](const Control &self) {
                controls::LayoutItem *layout=resolveBase(self)->getLayout();

                if(layout == nullptr)
                    return LayoutKind::None;
                return layout->getType() == controls::LayoutType::Vertical ? LayoutKind::Vertical : LayoutKind::Horizontal;
            },
            [](const Control &self, LayoutKind kind) {
                controls::Base *control=resolveBase(self);

                if(kind == LayoutKind::None)
                    throw nb::value_error("gweni: a layout cannot be taken off a control");
                if(kind == LayoutKind::Vertical)
                    control->setLayout(new controls::VerticalLayout());
                else
                    control->setLayout(new controls::HorizontalLayout());
                control->invalidate();
            },
            "How it stacks its children: VERTICAL or HORIZONTAL (a LayoutItem), or NONE.")
        .def("__eq__", [](const Control &self, nb::handle other) {
                return nb::isinstance<Control>(other) && nb::cast<const Control &>(other).id == self.id;
            })
        .def("__hash__", [](const Control &self) { return std::hash<size_t>()(self.id); })
        .def("__repr__", [](const Control &self) {
                const Session &s=session();
                controls::Base *control=(s.active && s.thread != std::this_thread::get_id())
                    ? nullptr : controls::Base::getControlById(self.id);
                std::string name=control ? control->getTypeName() : "deleted";

                return "<gweni control #"+std::to_string(self.id)+" "+name+">";
            });
}

}//namespace python
}//namespace gweni
