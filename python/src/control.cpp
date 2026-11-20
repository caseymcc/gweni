/*
 *  Gweni - control handles (control.h).
 *
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#include "control.h"
#include "events.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/layout/layout.h>
#include <gweni/uiTypes.h>

#include <nanobind/stl/string.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/vector.h>

#include <algorithm>
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
    m.def("_bound_type_names", []() {
            std::vector<std::string> names;

            for(const auto &type : typesByName())
                names.push_back(type.first);
            std::sort(names.begin(), names.end());
            return names;
        }, "The C++ control type names a bound class shows (python/tools/unbound_controls.py).");

    nb::enum_<DockPosition>(m, "Dock", "Where a control docks in its parent (DockPosition).")
        .value("NONE", DockPosition::None)
        .value("LEFT", DockPosition::Left)
        .value("RIGHT", DockPosition::Right)
        .value("TOP", DockPosition::Top)
        .value("BOTTOM", DockPosition::Bottom)
        .value("CENTER", DockPosition::Center);

    nb::enum_<SizeFlag>(m, "SizeFlag", "How a control's size follows its parent's room on one axis (SizeFlag).")
        .value("FIXED", SizeFlag::Fixed)
        .value("EXPAND", SizeFlag::Expand)
        .value("SHRINK", SizeFlag::Shrink)
        .value("ELASTIC", SizeFlag::Elastic)
        .value("BLOAT", SizeFlag::Bloat);

    nb::enum_<LayoutKind>(m, "Layout", "How a control stacks its children (controls::VerticalLayout, HorizontalLayout).")
        .value("NONE", LayoutKind::None)
        .value("VERTICAL", LayoutKind::Vertical)
        .value("HORIZONTAL", LayoutKind::Horizontal);

    auto control=nb::class_<Control>(m, "Control",
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
                return wrapAs<std::optional<Control>>(resolveBase(self)->getParent());
            }, "The parent's handle, or None.")
        .def_prop_ro("children", [](const Control &self) {
                std::vector<Typed<Control>> children;

                for(controls::Base *child : resolveBase(self)->getChildren())
                    children.push_back(wrapAs<Control>(child));
                return children;
            }, "The children's handles, in order (an inner panel's, where there is one).")
        .def("add", [](const Control &self, nb::type_object type, nb::kwargs props) {
                CallScope scope;
                controls::Base *parent=resolveBase(self);
                auto found=typesByClass().find(type.ptr());

                if(found == typesByClass().end() || !found->second.create)
                    throw nb::type_error(("gweni: "+nb::cast<std::string>(nb::str(type))
                        +" is not a control type add() can make").c_str());
                // every keyword a property it can set, checked before the
                // child is made: a read-only one (visible) or a method
                // (on_press) is refused as an unknown one is
                nb::object getStatic=nb::module_::import_("inspect").attr("getattr_static");

                for(auto item : props)
                {
                    const std::string typeName=nb::cast<std::string>(type.attr("__name__"));
                    const std::string name=nb::cast<std::string>(nb::str(item.first));

                    if(!nb::hasattr(type, item.first))
                        throw nb::attribute_error(("gweni: "+typeName+" has no property "+name).c_str());

                    nb::object attribute=getStatic(type, item.first);

                    if(!nb::isinstance(attribute, nb::module_::import_("builtins").attr("property")) ||
                        nb::getattr(attribute, "fset").is_none())
                        throw nb::attribute_error(("gweni: "+typeName+"."+name
                            +" is not a property add() can set (read-only, or a method)").c_str());
                }

                controls::Base *child=found->second.create(parent);
                nb::object handle=found->second.make(child->getControlId());

                session().deletable.insert(child->getControlId());

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
                scope.finish();
                return handle;
            }, nb::sig("def add(self, type: type[ControlT], /, **props: object) -> ControlT"),
            nb::arg("type"), nb::arg("props"),
            "Makes a child of \\p type, sets each keyword as a property and returns its handle.")
        .def("delete_later", [](const Control &self) {
                controls::Base *control=resolveBase(self);
                const Session &s=session();

                if(control == s.canvas)
                    throw nb::value_error("gweni: the canvas cannot be deleted (close the App or Headless)");
                // a part its owner made keeps a pointer there (a
                // CheckBoxWithLabel's box, a TabButton's page, a selected
                // option, a dialog's field): deleting it under the owner
                // would leave that pointer dangling
                if(s.deletable.count(self.id) == 0)
                    throw nb::value_error(("gweni: "+std::string(control->getTypeName())+" #"+std::to_string(self.id)
                        +" is a part its owner made, not a control add() made, so delete_later() cannot delete it:"
                        " delete its owner, or remove it the owner's way (TabControl.close_tab, Menu.clear_items,"
                        " ComboBox.clear_items, ListBox.clear, Toast.dismiss, Dialog.close)").c_str());
                control->delayedDelete();
            },
            "Deletes the control in the next frame's update (Base::delayedDelete): one add() made, a\n"
            "dialog, a TabButton (closed as close_tab closes it) or a Toast (dismissed). Any other\n"
            "part an owner made (a CheckBoxWithLabel's box, a page, an item, an option) and the\n"
            "canvas raise ValueError.")
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

                // off the session's thread the tree is not this thread's to read
                if(s.active && s.thread != std::this_thread::get_id())
                    return "<gweni control #"+std::to_string(self.id)+" (on the "+s.kind+"'s thread)>";

                controls::Base *control=controls::Base::getControlById(self.id);
                std::string name=control ? control->getTypeName() : "deleted";

                return "<gweni control #"+std::to_string(self.id)+" "+name+">";
            })
        .def_prop_rw("size",
            [](const Control &self) {
                const Rect &bounds=resolveBase(self)->getBounds();
                return std::make_tuple(bounds.w, bounds.h);
            },
            [](const Control &self, std::tuple<int, int> size) {
                inCall([&]() { resolveBase(self)->setSize(std::get<0>(size), std::get<1>(size)); });
            },
            "(width, height) in logical units; a dock or a layout may size it again.")
        .def_prop_rw("size_flags",
            [](const Control &self) {
                const SizeFlags flags=resolveBase(self)->getSizeFlags();
                return std::make_tuple(flags.horizontal, flags.vertical);
            },
            [](const Control &self, std::tuple<SizeFlag, SizeFlag> flags) {
                resolveBase(self)->setSizeFlags(SizeFlags(std::get<0>(flags), std::get<1>(flags)));
            },
            "(horizontal, vertical) SizeFlags.")
        .def_prop_rw("enabled",
            [](const Control &self) { return !resolveBase(self)->isDisabled(); },
            [](const Control &self, bool enabled) {
                inCall([&]() { resolveBase(self)->setDisabled(!enabled); });
            },
            "Whether it takes input (a disabled control draws its disabled look).")
        .def_prop_rw("tooltip",
            [](const Control &self) { return resolveBase(self)->getToolTipText(); },
            [](const Control &self, const std::string &text) { resolveBase(self)->setToolTipText(text); },
            "The text the canvas's tooltip shows after the hover delay (\"\" for none).")
        .def_prop_rw("position",
            [](const Control &self) {
                const Point pos=resolveBase(self)->getPos();
                return std::make_tuple(pos.x, pos.y);
            },
            [](const Control &self, std::tuple<int, int> pos) {
                inCall([&]() { resolveBase(self)->setPos(std::get<0>(pos), std::get<1>(pos)); });
            },
            "(x, y) in its parent, in logical units; a dock or a layout may place it again.")
        .def_prop_rw("margin",
            [](const Control &self) {
                const Margin &m=resolveBase(self)->getMargin();
                return std::make_tuple(m.left, m.top, m.right, m.bottom);
            },
            [](const Control &self, std::tuple<int, int, int, int> m) {
                resolveBase(self)->setMargin(Margin(std::get<0>(m), std::get<1>(m), std::get<2>(m), std::get<3>(m)));
            },
            "(left, top, right, bottom) space kept around it in its parent.")
        .def_prop_rw("padding",
            [](const Control &self) {
                const Padding &p=resolveBase(self)->getPadding();
                return std::make_tuple(p.left, p.top, p.right, p.bottom);
            },
            [](const Control &self, std::tuple<int, int, int, int> p) {
                resolveBase(self)->setPadding(Padding(std::get<0>(p), std::get<1>(p), std::get<2>(p), std::get<3>(p)));
            },
            "(left, top, right, bottom) space kept inside it around its children.")
        .def_prop_rw("hidden",
            [](const Control &self) { return resolveBase(self)->hidden(); },
            [](const Control &self, bool hidden) { inCall([&]() { resolveBase(self)->setHidden(hidden); }); },
            "Its own hidden flag (an ancestor's does not change it: see visible).")
        .def_prop_ro("visible", [](const Control &self) { return resolveBase(self)->visible(); },
            "Whether it is drawn: neither it nor an ancestor is hidden.")
        .def("show", [](const Control &self) { inCall([&]() { resolveBase(self)->show(); }); }, "hidden = False.")
        .def("hide", [](const Control &self) { inCall([&]() { resolveBase(self)->hide(); }); }, "hidden = True.")
        .def_prop_rw("opacity",
            [](const Control &self) -> std::optional<float> {
                controls::Base *control=resolveBase(self);

                if(!control->hasOwnOpacity())
                    return std::nullopt;
                return control->ownSurfaceOpacity();
            },
            [](const Control &self, std::optional<float> level) {
                controls::Base *control=resolveBase(self);

                if(level)
                    control->setOpacity(*level);
                else
                    control->clearOpacity();
            },
            "Its own opacity, 0..1, for it and everything in it (None: the parent's).")
        .def("set_opacity", [](const Control &self, float surface, float text) {
                resolveBase(self)->setOpacity(surface, text);
            }, nb::arg("surface"), nb::arg("text"), "Its fills and its text at levels of their own.")
        .def("focus", [](const Control &self) { inCall([&]() { resolveBase(self)->focus(); }); },
            "Gives it the keyboard focus.")
        .def("unfocus", [](const Control &self) { inCall([&]() { resolveBase(self)->unfocus(); }); },
            "Takes the keyboard focus from it, if it has it.")
        .def_prop_ro("focused", [](const Control &self) { return resolveBase(self)->isFocussed(); },
            "Whether it has the keyboard focus.")
        .def_prop_ro("canvas_bounds", [](const Control &self) {
                const Rect bounds=resolveBase(self)->getCanvasBounds();
                return std::make_tuple(bounds.x, bounds.y, bounds.w, bounds.h);
            }, "(x, y, width, height) on the canvas, as drawn: where to click it.")
        .def_prop_ro("bounds", [](const Control &self) {
                const Rect &bounds=resolveBase(self)->getBounds();
                return std::make_tuple(bounds.x, bounds.y, bounds.w, bounds.h);
            }, "(x, y, width, height) in its parent, in logical units.");
    bindEvent<decltype(control), controls::Base>(control, "on_hover_enter", &controls::Base::onHoverEnter,
        "The mouse came onto it.");
    bindEvent<decltype(control), controls::Base>(control, "on_hover_leave", &controls::Base::onHoverLeave,
        "The mouse left it.");
}

}//namespace python
}//namespace gweni
