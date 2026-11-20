/*
 *  Gweni - the containers and the rest of v1 (python/src): ScrollControl,
 *  GroupBox, TabControl and its TabButtons, Window, Menu, MenuStrip and
 *  MenuItem, the standard dialogs (message, confirm, prompt) with their result
 *  callback, canvas.notify()'s toasts and layout::Flow.
 *
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#include "control.h"
#include "events.h"
#include "handles.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/dialogs/dialog.h>
#include <gweni/controls/dialogs/query.h>
#include <gweni/controls/groupBox.h>
#include <gweni/controls/layout/flow.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/menuItem.h>
#include <gweni/controls/menuStrip.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/tabButton.h>
#include <gweni/controls/tabControl.h>
#include <gweni/controls/textBox.h>
#include <gweni/controls/toast.h>
#include <gweni/controls/windowControl.h>

#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/vector.h>

#include <optional>
#include <vector>

namespace nb=nanobind;

namespace gweni
{
namespace python
{

namespace
{

//! A dialog's result handler calling \p callback(result, text) as a callback.
dialogs::ResultHandler resultHandler(nb::object callback)
{
    if(callback.is_none())
        return dialogs::ResultHandler();

    CallablePtr callable=std::make_shared<Callable>(callback);

    return [callable](dialogs::Result result, const String &text) { callBack(*callable, result, text); };
}

//! \p control, which delete_later() may delete (a dialog the module opened)
template<typename T>
T *deletable(T *control)
{
    session().deletable.insert(control->getControlId());
    return control;
}

controls::Canvas *canvasOf(const Control &handle)
{
    return resolve<controls::Canvas>(handle);
}

}//namespace

void bindContainers(nb::module_ &m)
{
    // ScrollControl
    auto scroll=nb::class_<ScrollControl, Control>(m, "ScrollControl",
        "A viewport scrolling its children (controls::ScrollControl).")
        .def("set_scroll", [](const Control &self, bool horizontal, bool vertical) {
                resolve<controls::ScrollControl>(self)->setScroll(horizontal, vertical);
            }, nb::arg("horizontal"), nb::arg("vertical"), "Which axes scroll.")
        .def("set_auto_hide_bars", [](const Control &self, bool hide) {
                resolve<controls::ScrollControl>(self)->setAutoHideBars(hide);
            }, nb::arg("hide"), "Whether a bar hides while its content fits.")
        .def_prop_rw("scroll_offset",
            [](const Control &self) {
                controls::ScrollControl *s=resolve<controls::ScrollControl>(self);
                return std::make_tuple(s->getHorizontalScrollOffset(), s->getVerticalScrollOffset());
            },
            [](const Control &self, std::tuple<int, int> offset) {
                inCall([&]() {
                    controls::ScrollControl *s=resolve<controls::ScrollControl>(self);

                    s->setHorizontalScrollOffset(std::get<0>(offset));
                    s->setVerticalScrollOffset(std::get<1>(offset));
                });
            }, "(x, y) the content is scrolled by, in units.")
        .def_prop_ro("max_scroll_offset", [](const Control &self) {
                controls::ScrollControl *s=resolve<controls::ScrollControl>(self);
                return std::make_tuple(s->getMaxHorizontalScrollOffset(), s->getMaxVerticalScrollOffset());
            }, "The largest scroll_offset.")
        .def("scroll_to_top", [](const Control &self) { resolve<controls::ScrollControl>(self)->scrollToTop(); })
        .def("scroll_to_bottom", [](const Control &self) { resolve<controls::ScrollControl>(self)->scrollToBottom(); });
    registerControl<ScrollControl, controls::ScrollControl>(scroll);

    // GroupBox
    auto groupBox=nb::class_<GroupBox, Label>(m, "GroupBox",
        "A frame with a caption (its text) around its children (controls::GroupBox).");
    registerControl<GroupBox, controls::GroupBox>(groupBox);

    // TabControl
    auto tabButton=nb::class_<TabButton, Button>(m, "TabButton",
        "A TabControl's tab (controls::TabButton): made by add_page().")
        .def_prop_ro("page", [](const Control &self) { return wrapAs<Panel>(resolve<controls::TabButton>(self)->getPage()); },
            "The page it shows: add the page's controls to it.")
        .def_prop_ro("active", [](const Control &self) { return resolve<controls::TabButton>(self)->isActive(); },
            "Whether it is the selected tab.")
        .def_prop_rw("closable",
            [](const Control &self) { return resolve<controls::TabButton>(self)->isClosable(); },
            [](const Control &self, bool closable) { resolve<controls::TabButton>(self)->setClosable(closable); },
            "Whether it shows a close button (the TabControl's on_close).")
        .def("select", [](const Control &self) {
                controls::TabButton *tab=resolve<controls::TabButton>(self);
                controls::TabControl *control=tab->getTabControl();

                if(control == nullptr)
                    throw std::runtime_error("gweni.TabButton.select: not in a TabControl");
                // a closed tab waits for its delete: selected, it would be the
                // TabControl's current tab once freed
                if(!control->holdsTab(tab))
                    throw std::runtime_error("gweni.TabButton.select: the tab is closed");

                event::Info info(tab);

                info.controlCaller=tab;
                inCall([&]() { control->onTabPressed(info); });
            }, "Makes it the selected tab, showing its page; raises RuntimeError once it is closed.")
        .def("delete_later", [](const Control &self) {
                controls::TabButton *tab=resolve<controls::TabButton>(self);
                controls::TabControl *control=tab->getTabControl();

                if(control == nullptr)
                    throw std::runtime_error("gweni.TabButton.delete_later: not in a TabControl");
                // the TabControl keeps the selected tab and every page: take
                // it out as a close does (the next tab selected, on_lose_tab);
                // a closed tab is left alone, as closeTab() leaves it
                inCall([&]() { control->closeTab(tab); });
            }, "Closes it and its page as TabControl.close_tab() does (on_close is not fired); "
               "a closed tab is left as it is.");
    registerControl<TabButton, controls::TabButton, false>(tabButton);

    auto tabControl=nb::class_<TabControl, Control>(m, "TabControl",
        "Pages selected by a strip of tabs (controls::TabControl).")
        .def("add_page", [](const Control &self, const std::string &text) {
                return inCall([&]() { return wrapAs<TabButton>(resolve<controls::TabControl>(self)->addPage(text)); });
            }, nb::arg("text"), "Adds a tab and its page; returns the TabButton (its page is .page).")
        .def_prop_ro("tab_count", [](const Control &self) { return resolve<controls::TabControl>(self)->tabCount(); })
        .def("tab", [](const Control &self, int index) {
                controls::TabControl *control=resolve<controls::TabControl>(self);

                if(index < 0 || index >= control->tabCount())
                    throw nb::index_error("gweni.TabControl.tab: no such tab");
                return wrapAs<TabButton>(control->getTab(index));
            }, nb::arg("index"), "The index-th TabButton.")
        .def_prop_ro("current", [](const Control &self) {
                return wrapAs<std::optional<TabButton>>(resolve<controls::TabControl>(self)->getCurrentButton());
            }, "The selected TabButton, or None.")
        .def("close_tab", [](const Control &self, const Control &tab) {
                inCall([&]() { resolve<controls::TabControl>(self)->closeTab(resolve<controls::TabButton>(tab)); });
            }, nb::arg("tab"), "Closes tab without asking (on_close is not fired); a tab closed already is left as it is.")
        .def("set_strip_position", [](const Control &self, DockPosition side) {
                resolve<controls::TabControl>(self)->setTabStripPosition(side);
            }, nb::arg("side"), "The side the tabs are on.")
        .def("set_tab_list_button", [](const Control &self, bool show) {
                resolve<controls::TabControl>(self)->setTabListButton(show);
            }, nb::arg("show"), "A button listing every tab when they overflow.");
    bindEvent<decltype(tabControl), controls::TabControl>(tabControl, "on_add_tab",
        &controls::TabControl::onAddTabCaller, "A tab was added.");
    bindEvent<decltype(tabControl), controls::TabControl>(tabControl, "on_lose_tab",
        &controls::TabControl::onLoseTabCaller, "A tab was removed.");
    bindEvent<decltype(tabControl), controls::TabControl>(tabControl, "on_close", &controls::TabControl::onClose,
        "A closable tab's close button was pressed (event.control the TabButton); it closes unless vetoed.");
    tabControl.def("veto_close", [](const Control &self) { resolve<controls::TabControl>(self)->vetoClose(); },
        "From on_close: keeps the tab open.");
    registerControl<TabControl, controls::TabControl>(tabControl);

    // Window
    auto window=nb::class_<Window, Control>(m, "Window",
        "A movable, resizable window with a title bar (controls::WindowControl). add() makes it\n"
        "300x200 at (0, 0) unless its size or bounds are given: the C++ control is Base's 100x40,\n"
        "in which a title bar leaves no room for what is docked under it.")
        .def_prop_rw("title",
            [](const Control &self) { return resolve<controls::WindowControl>(self)->getTitle(); },
            [](const Control &self, const std::string &title) { resolve<controls::WindowControl>(self)->setTitle(title); })
        .def("set_closable", [](const Control &self, bool closable) {
                resolve<controls::WindowControl>(self)->setClosable(closable);
            }, nb::arg("closable"), "Whether it has a close button.")
        .def("set_delete_on_close", [](const Control &self, bool on) {
                resolve<controls::WindowControl>(self)->setDeleteOnClose(on);
            }, nb::arg("on"), "Deleted when closed (else hidden).")
        .def("make_modal", [](const Control &self) {
                inCall([&]() { resolve<controls::WindowControl>(self)->makeModal(); });
            }, "Puts it on the canvas's modal stack: it takes the input from everything under it.")
        .def("destroy_modal", [](const Control &self) {
                inCall([&]() { resolve<controls::WindowControl>(self)->destroyModal(); });
            }, "Takes it off the modal stack.")
        .def_prop_ro("modal", [](const Control &self) { return resolve<controls::WindowControl>(self)->isModal(); });
    bindEvent<decltype(window), controls::WindowControl>(window, "on_closed", &controls::WindowControl::onWindowClosed,
        "Its close button closed it.");
    // add() gives it the C++ demos' size before the keywords are set, so a
    // size= or bounds= among them wins
    registerControlType(controls::WindowControl::getTypeNameStatic(), window,
        [](size_t id) { Window handle; handle.id=id; return nb::cast(handle); },
        [](controls::Base *parent) -> controls::Base * {
            controls::WindowControl *made=parent->newChild<controls::WindowControl>();

            made->setSize(300, 200);
            return made;
        });

    // Menu, MenuStrip, MenuItem
    auto menuItem=nb::class_<MenuItem, Button>(m, "MenuItem", "An item of a Menu or a MenuStrip (controls::MenuItem).")
        .def_prop_rw("checkable",
            [](const Control &self) { return resolve<controls::MenuItem>(self)->isCheckable(); },
            [](const Control &self, bool on) { resolve<controls::MenuItem>(self)->setCheckable(on); })
        .def_prop_rw("checked",
            [](const Control &self) { return resolve<controls::MenuItem>(self)->isChecked(); },
            [](const Control &self, bool on) { inCall([&]() { resolve<controls::MenuItem>(self)->setChecked(on); }); })
        .def_prop_ro("menu", [](const Control &self) { return wrapAs<Menu>(resolve<controls::MenuItem>(self)->getMenu()); },
            "Its submenu, made on first use: add items to it.")
        .def("set_accelerator", [](const Control &self, const std::string &accelerator) {
                resolve<controls::MenuItem>(self)->setAccelerator(accelerator);
            }, nb::arg("accelerator"), "The shortcut it shows (\"Ctrl+S\").");
    bindEvent<decltype(menuItem), controls::MenuItem>(menuItem, "on_selected",
        &controls::MenuItem::onMenuItemSelected, "It was chosen.");
    bindEvent<decltype(menuItem), controls::MenuItem>(menuItem, "on_check_change",
        &controls::MenuItem::onCheckChange, "A checkable item was checked or unchecked.");
    registerControl<MenuItem, controls::MenuItem, false>(menuItem);

    auto menu=nb::class_<Menu, ScrollControl>(m, "Menu",
        "A list of items (controls::Menu): a context menu, a submenu or a canvas menu;\n"
        "canvas.add() makes it closed (hidden) until open_at(); add() on another parent raises ValueError.")
        .def("add_item", [](const Control &self, const std::string &text, const std::string &accelerator) {
                return inCall([&]() { return wrapAs<MenuItem>(resolve<controls::Menu>(self)->addItem(text, "", accelerator)); });
            }, nb::arg("text"), nb::arg("accelerator")="", "Adds an item; returns its MenuItem.")
        .def("add_divider", [](const Control &self) { resolve<controls::Menu>(self)->addDivider(); })
        .def("clear_items", [](const Control &self) { inCall([&]() { resolve<controls::Menu>(self)->clearItems(); }); })
        .def("open_at", [](const Control &self, std::tuple<int, int> at) {
                inCall([&]() {
                    resolve<controls::Menu>(self)->openAt(Point(std::get<0>(at), std::get<1>(at)),
                        controls::MenuFocus::FirstItem);
                });
            }, nb::arg("at"), "Opens it at (x, y) on the canvas, with the keyboard on its first item.")
        .def("close", [](const Control &self) { inCall([&]() { resolve<controls::Menu>(self)->close(); }); })
        .def_prop_ro("is_open", [](const Control &self) { return resolve<controls::Menu>(self)->visible(); });
    // add() makes a Menu hidden, as a closed one is: open_at() shows it (a
    // MenuStrip, a bar on its parent, is made shown). Only on the canvas:
    // openAt() places a menu in canvas coordinates as the canvas's child, and
    // under another parent it would open in that parent's, clipped by it
    registerControlType(controls::Menu::getTypeNameStatic(), menu,
        [](size_t id) { Menu handle; handle.id=id; return nb::cast(handle); },
        [](controls::Base *parent) -> controls::Base * {
            if(parent != parent->getCanvas())
                throw nb::value_error("gweni: a Menu is made on the canvas (canvas.add(gweni.Menu)); "
                    "a MenuStrip goes on any parent");

            controls::Menu *made=parent->newChild<controls::Menu>();

            made->setHidden(true);
            return made;
        });

    auto menuStrip=nb::class_<MenuStrip, Menu>(m, "MenuStrip",
        "A menu bar (controls::MenuStrip): dock it TOP, add_item() its menus, and fill each\n"
        "item's .menu.");
    registerControl<MenuStrip, controls::MenuStrip>(menuStrip);

    // dialogs
    nb::enum_<dialogs::Result>(m, "DialogResult", "What closed a dialog (dialogs::Result).")
        .value("OK", dialogs::Result::Ok)
        .value("YES", dialogs::Result::Yes)
        .value("NO", dialogs::Result::No)
        .value("CANCEL", dialogs::Result::Cancel);
    nb::enum_<dialogs::Buttons>(m, "DialogButtons", "A confirm()'s buttons (dialogs::Buttons).")
        .value("YES_NO", dialogs::Buttons::YesNo)
        .value("OK_CANCEL", dialogs::Buttons::OkCancel);

    auto dialog=nb::class_<Dialog, Control>(m, "Dialog",
        "A modal dialog (dialogs::Dialog): made by message(), confirm() or prompt(), and\n"
        "deleted once closed, so its handle goes stale then.")
        .def_prop_rw("title",
            [](const Control &self) { return std::string(resolve<dialogs::Dialog>(self)->getTitle()); },
            [](const Control &self, const std::string &title) { resolve<dialogs::Dialog>(self)->setTitle(title); })
        .def_prop_ro("is_open", [](const Control &self) { return resolve<dialogs::Dialog>(self)->isOpen(); })
        .def_prop_ro("content", [](const Control &self) { return wrapAs<ScrollControl>(resolve<dialogs::Dialog>(self)->getContent()); },
            "The ScrollControl between the text and the buttons: add controls to it.")
        .def("add_button", [](const Control &self, const std::string &label, std::optional<dialogs::Result> result) {
                return inCall([&]() {
                    dialogs::Dialog *d=resolve<dialogs::Dialog>(self);
                    return wrapAs<Button>(result ? d->addButton(label, *result) : d->addButton(label));
                });
            }, nb::arg("label"), nb::arg("result")=nb::none(),
            "Adds a footer button; with a result its press closes the dialog with it.")
        .def("close", [](const Control &self, std::optional<dialogs::Result> result) {
                inCall([&]() {
                    dialogs::Dialog *d=resolve<dialogs::Dialog>(self);

                    if(result)
                        d->close(*result);
                    else
                        d->close();
                });
            }, nb::arg("result")=nb::none(), "Closes it with result (its cancel result by default).");
    bindEvent<decltype(dialog), dialogs::Dialog>(dialog, "on_close", &dialogs::Dialog::onClose, "It closed.");
    registerControl<Dialog, dialogs::Dialog, false>(dialog);

    auto promptDialog=nb::class_<PromptDialog, Dialog>(m, "PromptDialog", "prompt()'s dialog, with its text field.")
        .def_prop_ro("field", [](const Control &self) { return wrapAs<TextBox>(resolve<dialogs::PromptDialog>(self)->getField()); },
            "Its TextBox.");
    registerControl<PromptDialog, dialogs::PromptDialog, false>(promptDialog);

    m.def("message", [](const Control &canvas, const std::string &title, const std::string &text, nb::object onResult) {
            return inCall([&]() {
                return wrapAs<Dialog>(deletable(dialogs::message(canvasOf(canvas), title, text, resultHandler(onResult))));
            });
        }, nb::sig("def message(canvas: Canvas, title: str, text: str, on_result: "
            "collections.abc.Callable[[DialogResult, str], object] | None = None) -> Dialog"),
        nb::arg("canvas"), nb::arg("title"), nb::arg("text"), nb::arg("on_result")=nb::none(),
        "Opens a dialog with text and an OK button; on_result(result, text) once it closes.");
    m.def("confirm", [](const Control &canvas, const std::string &title, const std::string &text,
            dialogs::Buttons buttons, nb::object onResult) {
            return inCall([&]() {
                return wrapAs<Dialog>(deletable(dialogs::confirm(canvasOf(canvas), title, text, buttons, resultHandler(onResult))));
            });
        }, nb::sig("def confirm(canvas: Canvas, title: str, text: str, buttons: DialogButtons = ..., "
            "on_result: collections.abc.Callable[[DialogResult, str], object] | None = None) -> Dialog"),
        nb::arg("canvas"), nb::arg("title"), nb::arg("text"), nb::arg("buttons")=dialogs::Buttons::YesNo,
        nb::arg("on_result")=nb::none(),
        "Opens a dialog asking Yes/No (or OK/Cancel); on_result(result, text) once it closes.");
    m.def("prompt", [](const Control &canvas, const std::string &title, const std::string &text,
            const std::string &initial, nb::object onResult) {
            return inCall([&]() {
                return wrapAs<PromptDialog>(deletable(dialogs::prompt(canvasOf(canvas), title, text, initial, resultHandler(onResult))));
            });
        }, nb::sig("def prompt(canvas: Canvas, title: str, text: str, initial: str = '', "
            "on_result: collections.abc.Callable[[DialogResult, str], object] | None = None) -> PromptDialog"),
        nb::arg("canvas"), nb::arg("title"), nb::arg("text"), nb::arg("initial")="", nb::arg("on_result")=nb::none(),
        "Opens a dialog with a text field; on_result(result, text) gets the text with OK.");

    // toasts
    nb::enum_<controls::ToastKind>(m, "ToastKind", "A toast's kind (ToastKind).")
        .value("INFO", controls::ToastKind::Info)
        .value("POSITIVE", controls::ToastKind::Positive)
        .value("WARNING", controls::ToastKind::Warning)
        .value("NEGATIVE", controls::ToastKind::Negative);

    auto toast=nb::class_<Toast, Control>(m, "Toast",
        "A notice in the canvas's top-right corner (controls::Toast): made by\n"
        "canvas.notify(), deleted once it goes, so its handle goes stale then.")
        .def_prop_ro("kind", [](const Control &self) { return resolve<controls::Toast>(self)->getKind(); })
        .def_prop_ro("text", [](const Control &self) { return std::string(resolve<controls::Toast>(self)->getText()); })
        .def_prop_ro("title", [](const Control &self) { return std::string(resolve<controls::Toast>(self)->getTitle()); })
        .def_prop_ro("dismissed", [](const Control &self) { return resolve<controls::Toast>(self)->isDismissed(); })
        .def("dismiss", [](const Control &self) { inCall([&]() { resolve<controls::Toast>(self)->dismiss(); }); },
            "Takes it off the stack now (deleted in the next frame).")
        .def("delete_later", [](const Control &self) { inCall([&]() { resolve<controls::Toast>(self)->dismiss(); }); },
            "Dismisses it (the toast host keeps its toasts): deleted in the next frame.");
    registerControl<Toast, controls::Toast, false>(toast);

    // layout::Flow
    auto flow=nb::class_<Flow, Control>(m, "Flow",
        "Cards in as many columns as fit its own width (controls::layout::Flow).")
        .def_prop_rw("gap",
            [](const Control &self) { return resolve<controls::layout::Flow>(self)->getGap(); },
            [](const Control &self, int gap) { resolve<controls::layout::Flow>(self)->setGap(gap); })
        .def_prop_rw("min_card_width",
            [](const Control &self) { return resolve<controls::layout::Flow>(self)->getMinCardWidth(); },
            [](const Control &self, int width) { resolve<controls::layout::Flow>(self)->setMinCardWidth(width); })
        .def_prop_rw("max_columns",
            [](const Control &self) { return resolve<controls::layout::Flow>(self)->getMaxColumns(); },
            [](const Control &self, int columns) { resolve<controls::layout::Flow>(self)->setMaxColumns(columns); })
        .def("set_breakpoints", [](const Control &self, const std::vector<std::tuple<int, int>> &points) {
                std::vector<controls::layout::Flow::Breakpoint> breakpoints;

                for(const auto &point : points)
                    breakpoints.push_back({std::get<0>(point), std::get<1>(point)});
                resolve<controls::layout::Flow>(self)->setBreakpoints(std::move(breakpoints));
            }, nb::arg("breakpoints"), "[(below width, columns), ...]: the first its width is below wins.")
        .def_prop_ro("columns", [](const Control &self) { return resolve<controls::layout::Flow>(self)->getColumns(); },
            "The columns its last layout used.");
    registerControl<Flow, controls::layout::Flow>(flow);
}

//! canvas.notify(), added to the Canvas class once ToastKind is bound
void bindNotify(nb::module_ &)
{
    nb::handle canvas=nb::type<Canvas>();

    nb::setattr(canvas, "notify", nb::cpp_function([](const Control &self, controls::ToastKind kind,
            const std::string &text, std::optional<double> timeout, const std::string &title) {
            return inCall([&]() {
                controls::ToastOptions options;

                if(timeout)
                    options.timeout=*timeout;
                options.title=title;
                return wrapAs<Toast>(canvasOf(self)->notify(kind, text, options));
            });
        }, nb::is_method(), nb::scope(canvas), nb::name("notify"),
        nb::arg("kind"), nb::arg("text"), nb::arg("timeout")=nb::none(), nb::arg("title")="",
        "Shows a toast; timeout in seconds (None: the kind's default, 0: until dismissed)."));
}

}//namespace python
}//namespace gweni
