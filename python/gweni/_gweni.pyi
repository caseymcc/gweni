"""
gweni, a retained-mode GUI library: the extension the gweni package wraps.
"""

from collections.abc import Callable, Sequence
import enum
from typing import TypeVar


class ControlDeletedError(ReferenceError):
    pass

class ThreadError(RuntimeError):
    pass

class Dock(enum.Enum):
    """Where a control docks in its parent (DockPosition)."""

    NONE = 0

    LEFT = 2

    RIGHT = 4

    TOP = 8

    BOTTOM = 16

    CENTER = 32

class SizeFlag(enum.Enum):
    """How a control's size follows its parent's room on one axis (SizeFlag)."""

    FIXED = 0

    EXPAND = 1

    SHRINK = 2

    ELASTIC = 3

    BLOAT = 4

class Layout(enum.Enum):
    """
    How a control stacks its children (controls::VerticalLayout, HorizontalLayout).
    """

    NONE = 0

    VERTICAL = 1

    HORIZONTAL = 2

class Control:
    """
    A handle of a control in the C++ tree, which owns it. Every use looks the
    control up by its id: a deleted one raises ControlDeletedError, and
    dropping a handle deletes nothing.
    """

    @property
    def id(self) -> int:
        """The control's id (Base::getControlId), never reused."""

    @property
    def alive(self) -> bool:
        """Whether the control still exists."""

    @property
    def type_name(self) -> str:
        """The C++ type name (Base::getTypeName)."""

    @property
    def parent(self) -> Control | None:
        """The parent's handle, or None."""

    @property
    def children(self) -> list[Control]:
        """
        The children's handles, in order (an inner panel's, where there is one).
        """

    def add(self, type: type[ControlT], /, **props: object) -> ControlT:
        """
        Makes a child of \\p type, sets each keyword as a property and returns its handle.
        """

    def delete_later(self) -> None:
        """Deletes the control in the next frame's update (Base::delayedDelete)."""

    @property
    def dock(self) -> Dock:
        """Where it docks in its parent."""

    @dock.setter
    def dock(self, arg: Dock, /) -> None: ...

    @property
    def layout(self) -> Layout:
        """
        How it stacks its children: VERTICAL or HORIZONTAL (a LayoutItem), or NONE.
        """

    @layout.setter
    def layout(self, arg: Layout, /) -> None: ...

    def __eq__(self, arg: object, /) -> bool: ...

    def __hash__(self) -> int: ...

    def __repr__(self) -> str: ...

    @property
    def size(self) -> tuple[int, int]:
        """
        (width, height) in logical units; a dock or a layout may size it again.
        """

    @size.setter
    def size(self, arg: tuple[int, int], /) -> None: ...

    @property
    def size_flags(self) -> tuple[SizeFlag, SizeFlag]:
        """(horizontal, vertical) SizeFlags."""

    @size_flags.setter
    def size_flags(self, arg: tuple[SizeFlag, SizeFlag], /) -> None: ...

    @property
    def enabled(self) -> bool:
        """Whether it takes input (a disabled control draws its disabled look)."""

    @enabled.setter
    def enabled(self, arg: bool, /) -> None: ...

    @property
    def tooltip(self) -> str:
        """
        The text the canvas's tooltip shows after the hover delay ("" for none).
        """

    @tooltip.setter
    def tooltip(self, arg: str, /) -> None: ...

    @property
    def position(self) -> tuple[int, int]:
        """
        (x, y) in its parent, in logical units; a dock or a layout may place it again.
        """

    @position.setter
    def position(self, arg: tuple[int, int], /) -> None: ...

    @property
    def margin(self) -> tuple[int, int, int, int]:
        """(left, top, right, bottom) space kept around it in its parent."""

    @margin.setter
    def margin(self, arg: tuple[int, int, int, int], /) -> None: ...

    @property
    def padding(self) -> tuple[int, int, int, int]:
        """(left, top, right, bottom) space kept inside it around its children."""

    @padding.setter
    def padding(self, arg: tuple[int, int, int, int], /) -> None: ...

    @property
    def hidden(self) -> bool:
        """Its own hidden flag (an ancestor's does not change it: see visible)."""

    @hidden.setter
    def hidden(self, arg: bool, /) -> None: ...

    @property
    def visible(self) -> bool:
        """Whether it is drawn: neither it nor an ancestor is hidden."""

    def show(self) -> None:
        """hidden = False."""

    def hide(self) -> None:
        """hidden = True."""

    @property
    def opacity(self) -> float | None:
        """
        Its own opacity, 0..1, for it and everything in it (None: the parent's).
        """

    @opacity.setter
    def opacity(self, arg: float | None, /) -> None: ...

    def set_opacity(self, surface: float, text: float) -> None:
        """Its fills and its text at levels of their own."""

    def focus(self) -> None:
        """Gives it the keyboard focus."""

    def unfocus(self) -> None:
        """Takes the keyboard focus from it, if it has it."""

    @property
    def focused(self) -> bool:
        """Whether it has the keyboard focus."""

    @property
    def canvas_bounds(self) -> tuple[int, int, int, int]:
        """(x, y, width, height) on the canvas, as drawn: where to click it."""

    @property
    def bounds(self) -> tuple[int, int, int, int]:
        """(x, y, width, height) in its parent, in logical units."""

    def on_hover_enter(self, callback: EventCallbackT, /) -> EventCallbackT:
        """The mouse came onto it."""

    def on_hover_leave(self, callback: EventCallbackT, /) -> EventCallbackT:
        """The mouse left it."""

class Event:
    """What an event callback gets (event::Info)."""

    @property
    def control(self) -> Control | None:
        """The control the event is about, or None."""

    @property
    def string(self) -> str:
        """The event's text, where it has one."""

    @property
    def point(self) -> tuple[int, int]:
        """The event's point, where it has one."""

    @property
    def integer(self) -> int:
        """The event's number, where it has one."""

    def __repr__(self) -> str: ...

class Canvas(Control):
    """
    The root of the tree (controls::Canvas): an App's or Headless's canvas.
    """

    def post(self, callback: Callable[[], object]) -> bool:
        """
        Runs callback() on the UI thread at the top of the next update, from any
        thread, waking an idle App. False, dropping it, once the canvas is gone.
        """

    def notify(self, kind: ToastKind, text: str, timeout: float | None = None, title: str = '') -> Toast:
        """
        Shows a toast; timeout in seconds (None: the kind's default, 0: until dismissed).
        """

class Panel(Control):
    """A plain container (controls::Base) that draws nothing."""

class Label(Control):
    """A line of text (controls::Label)."""

    @property
    def text(self) -> str:
        """Its text."""

    @text.setter
    def text(self, arg: str, /) -> None: ...

class Button(Label):
    """A push button (controls::Button)."""

    @property
    def is_toggle(self) -> bool:
        """Whether it stays down until pressed again."""

    @is_toggle.setter
    def is_toggle(self, arg: bool, /) -> None: ...

    @property
    def toggled(self) -> bool:
        """
        A toggle button's state; setting it fires on_toggle and on_toggle_on/off.
        """

    @toggled.setter
    def toggled(self, arg: bool, /) -> None: ...

    def on_press(self, callback: EventCallbackT, /) -> EventCallbackT:
        """Pressed (clicked, or Space)."""

    def on_right_press(self, callback: EventCallbackT, /) -> EventCallbackT:
        """Pressed with the right button."""

    def on_down(self, callback: EventCallbackT, /) -> EventCallbackT:
        """The button went down."""

    def on_up(self, callback: EventCallbackT, /) -> EventCallbackT:
        """The button came up."""

    def on_double_click(self, callback: EventCallbackT, /) -> EventCallbackT:
        """Double-clicked."""

    def on_toggle(self, callback: EventCallbackT, /) -> EventCallbackT:
        """A toggle button changed state."""

    def on_toggle_on(self, callback: EventCallbackT, /) -> EventCallbackT:
        """A toggle button turned on."""

    def on_toggle_off(self, callback: EventCallbackT, /) -> EventCallbackT:
        """A toggle button turned off."""

class ScrollControl(Control):
    """A viewport scrolling its children (controls::ScrollControl)."""

    def set_scroll(self, horizontal: bool, vertical: bool) -> None:
        """Which axes scroll."""

    def set_auto_hide_bars(self, hide: bool) -> None:
        """Whether a bar hides while its content fits."""

    @property
    def scroll_offset(self) -> tuple[int, int]:
        """(x, y) the content is scrolled by, in units."""

    @scroll_offset.setter
    def scroll_offset(self, arg: tuple[int, int], /) -> None: ...

    @property
    def max_scroll_offset(self) -> tuple[int, int]:
        """The largest scroll_offset."""

    def scroll_to_top(self) -> None: ...

    def scroll_to_bottom(self) -> None: ...

class GroupBox(Label):
    """
    A frame with a caption (its text) around its children (controls::GroupBox).
    """

class TabButton(Button):
    """A TabControl's tab (controls::TabButton): made by add_page()."""

    @property
    def page(self) -> Panel:
        """The page it shows: add the page's controls to it."""

    @property
    def active(self) -> bool:
        """Whether it is the selected tab."""

    @property
    def closable(self) -> bool:
        """Whether it shows a close button (the TabControl's on_close)."""

    @closable.setter
    def closable(self, arg: bool, /) -> None: ...

    def select(self) -> None:
        """Makes it the selected tab, showing its page."""

class TabControl(Control):
    """Pages selected by a strip of tabs (controls::TabControl)."""

    def add_page(self, text: str) -> TabButton:
        """Adds a tab and its page; returns the TabButton (its page is .page)."""

    @property
    def tab_count(self) -> int: ...

    def tab(self, index: int) -> TabButton:
        """The index-th TabButton."""

    @property
    def current(self) -> TabButton | None:
        """The selected TabButton, or None."""

    def close_tab(self, tab: Control) -> None:
        """Closes tab without asking (on_close is not fired)."""

    def set_strip_position(self, side: Dock) -> None:
        """The side the tabs are on."""

    def set_tab_list_button(self, show: bool) -> None:
        """A button listing every tab when they overflow."""

    def on_add_tab(self, callback: EventCallbackT, /) -> EventCallbackT:
        """A tab was added."""

    def on_lose_tab(self, callback: EventCallbackT, /) -> EventCallbackT:
        """A tab was removed."""

    def on_close(self, callback: EventCallbackT, /) -> EventCallbackT:
        """
        A closable tab's close button was pressed (event.control the TabButton); it closes unless vetoed.
        """

    def veto_close(self) -> None:
        """From on_close: keeps the tab open."""

class Window(Control):
    """
    A movable, resizable window with a title bar (controls::WindowControl).
    """

    @property
    def title(self) -> str: ...

    @title.setter
    def title(self, arg: str, /) -> None: ...

    def set_closable(self, closable: bool) -> None:
        """Whether it has a close button."""

    def set_delete_on_close(self, on: bool) -> None:
        """Deleted when closed (else hidden)."""

    def make_modal(self) -> None:
        """
        Puts it on the canvas's modal stack: it takes the input from everything under it.
        """

    def destroy_modal(self) -> None:
        """Takes it off the modal stack."""

    @property
    def modal(self) -> bool: ...

    def on_closed(self, callback: EventCallbackT, /) -> EventCallbackT:
        """Its close button closed it."""

class MenuItem(Button):
    """An item of a Menu or a MenuStrip (controls::MenuItem)."""

    @property
    def checkable(self) -> bool: ...

    @checkable.setter
    def checkable(self, arg: bool, /) -> None: ...

    @property
    def checked(self) -> bool: ...

    @checked.setter
    def checked(self, arg: bool, /) -> None: ...

    @property
    def menu(self) -> Menu:
        """Its submenu, made on first use: add items to it."""

    def set_accelerator(self, accelerator: str) -> None:
        """The shortcut it shows ("Ctrl+S")."""

    def on_selected(self, callback: EventCallbackT, /) -> EventCallbackT:
        """It was chosen."""

    def on_check_change(self, callback: EventCallbackT, /) -> EventCallbackT:
        """A checkable item was checked or unchecked."""

class Menu(ScrollControl):
    """
    A list of items (controls::Menu): a context menu, a submenu or a canvas menu.
    """

    def add_item(self, text: str, accelerator: str = '') -> MenuItem:
        """Adds an item; returns its MenuItem."""

    def add_divider(self) -> None: ...

    def clear_items(self) -> None: ...

    def open_at(self, at: tuple[int, int]) -> None:
        """Opens it at (x, y) on the canvas, with the keyboard on its first item."""

    def close(self) -> None: ...

    @property
    def is_open(self) -> bool: ...

class MenuStrip(Menu):
    """
    A menu bar (controls::MenuStrip): dock it TOP, add_item() its menus, and fill each
    item's .menu.
    """

class DialogResult(enum.Enum):
    """What closed a dialog (dialogs::Result)."""

    OK = 0

    YES = 1

    NO = 2

    CANCEL = 3

class DialogButtons(enum.Enum):
    """A confirm()'s buttons (dialogs::Buttons)."""

    YES_NO = 0

    OK_CANCEL = 1

class Dialog(Control):
    """
    A modal dialog (dialogs::Dialog): made by message(), confirm() or prompt(), and
    deleted once closed, so its handle goes stale then.
    """

    @property
    def title(self) -> str: ...

    @title.setter
    def title(self, arg: str, /) -> None: ...

    @property
    def is_open(self) -> bool: ...

    @property
    def content(self) -> ScrollControl:
        """
        The ScrollControl between the text and the buttons: add controls to it.
        """

    def add_button(self, label: str, result: DialogResult | None = None) -> Button:
        """
        Adds a footer button; with a result its press closes the dialog with it.
        """

    def close(self, result: DialogResult | None = None) -> None:
        """Closes it with result (its cancel result by default)."""

    def on_close(self, callback: EventCallbackT, /) -> EventCallbackT:
        """It closed."""

class PromptDialog(Dialog):
    """prompt()'s dialog, with its text field."""

    @property
    def field(self) -> TextBox:
        """Its TextBox."""

def message(canvas: Canvas, title: str, text: str, on_result: Callable[[DialogResult, str], object] | None = None) -> Dialog:
    """
    Opens a dialog with text and an OK button; on_result(result, text) once it closes.
    """

def confirm(canvas: Canvas, title: str, text: str, buttons: DialogButtons = ..., on_result: Callable[[DialogResult, str], object] | None = None) -> Dialog:
    """
    Opens a dialog asking Yes/No (or OK/Cancel); on_result(result, text) once it closes.
    """

def prompt(canvas: Canvas, title: str, text: str, initial: str = '', on_result: Callable[[DialogResult, str], object] | None = None) -> PromptDialog:
    """
    Opens a dialog with a text field; on_result(result, text) gets the text with OK.
    """

class ToastKind(enum.Enum):
    """A toast's kind (ToastKind)."""

    INFO = 0

    POSITIVE = 1

    WARNING = 2

    NEGATIVE = 3

class Toast(Control):
    """
    A notice in the canvas's top-right corner (controls::Toast): made by
    canvas.notify(), deleted once it goes, so its handle goes stale then.
    """

    @property
    def kind(self) -> ToastKind: ...

    @property
    def text(self) -> str: ...

    @property
    def title(self) -> str: ...

    @property
    def dismissed(self) -> bool: ...

    def dismiss(self) -> None:
        """Takes it off the stack now (deleted in the next frame)."""

class Flow(Control):
    """
    Cards in as many columns as fit its own width (controls::layout::Flow).
    """

    @property
    def gap(self) -> int: ...

    @gap.setter
    def gap(self, arg: int, /) -> None: ...

    @property
    def min_card_width(self) -> int: ...

    @min_card_width.setter
    def min_card_width(self, arg: int, /) -> None: ...

    @property
    def max_columns(self) -> int: ...

    @max_columns.setter
    def max_columns(self, arg: int, /) -> None: ...

    def set_breakpoints(self, breakpoints: Sequence[tuple[int, int]]) -> None:
        """[(below width, columns), ...]: the first its width is below wins."""

    @property
    def columns(self) -> int:
        """The columns its last layout used."""

class CheckBox(Button):
    """A box that is checked or not (controls::CheckBox)."""

    @property
    def checked(self) -> bool:
        """
        Whether it is checked; setting it fires on_check_changed and on_checked/on_unchecked.
        """

    @checked.setter
    def checked(self, arg: bool, /) -> None: ...

    def on_checked(self, callback: EventCallbackT, /) -> EventCallbackT:
        """It was checked."""

    def on_unchecked(self, callback: EventCallbackT, /) -> EventCallbackT:
        """It was unchecked."""

    def on_check_changed(self, callback: EventCallbackT, /) -> EventCallbackT:
        """It was checked or unchecked."""

class CheckBoxWithLabel(Control):
    """
    A CheckBox with a clickable label beside it (controls::CheckBoxWithLabel).
    """

    @property
    def text(self) -> str:
        """The label's text."""

    @text.setter
    def text(self, arg: str, /) -> None: ...

    @property
    def checked(self) -> bool:
        """Whether its box is checked."""

    @checked.setter
    def checked(self, arg: bool, /) -> None: ...

    @property
    def label_on_left(self) -> bool:
        """The text on the left of the box (default: right)."""

    @label_on_left.setter
    def label_on_left(self, arg: bool, /) -> None: ...

    @property
    def check_box(self) -> CheckBox:
        """Its CheckBox."""

    def on_check_changed(self, callback: EventCallbackT, /) -> EventCallbackT:
        """Its box was checked or unchecked (the event's control is the row)."""

    def on_checked(self, callback: EventCallbackT, /) -> EventCallbackT:
        """Its box was checked."""

    def on_unchecked(self, callback: EventCallbackT, /) -> EventCallbackT:
        """Its box was unchecked."""

class RadioButton(Control):
    """
    One option of a RadioButtonController (controls::LabeledRadioButton): made by
    the controller's add_option().
    """

    @property
    def text(self) -> str:
        """Its label."""

    @text.setter
    def text(self, arg: str, /) -> None: ...

    @property
    def selected(self) -> bool:
        """Whether it is the controller's selected option."""

    def select(self) -> None:
        """
        Makes it the selected option (fires the controller's on_selection_change).
        """

class RadioButtonController(Control):
    """
    A group of RadioButtons of which one at most is selected (controls::RadioButtonController).
    """

    def add_option(self, text: str, name: str = '') -> RadioButton:
        """Adds an option; returns its RadioButton."""

    @property
    def selected(self) -> RadioButton | None:
        """The selected RadioButton, or None."""

    @property
    def selected_name(self) -> str:
        """The selected option's name ("" with none)."""

    def on_selection_change(self, callback: EventCallbackT, /) -> EventCallbackT:
        """Another option was selected."""

class TextBox(Label):
    """A line of editable text (controls::TextBox)."""

    @property
    def placeholder(self) -> str:
        """A hint shown while it is empty and unfocused."""

    @placeholder.setter
    def placeholder(self, arg: str, /) -> None: ...

    @property
    def editable(self) -> bool:
        """Whether typing changes it."""

    @editable.setter
    def editable(self, arg: bool, /) -> None: ...

    @property
    def max_length(self) -> int:
        """The most bytes it takes (-1: no limit)."""

    @max_length.setter
    def max_length(self, arg: int, /) -> None: ...

    @property
    def error(self) -> str:
        """The validation message under the field ("" for none)."""

    @error.setter
    def error(self, arg: str, /) -> None: ...

    @property
    def helper_text(self) -> str:
        """A hint under the field, shown while there is no error."""

    @helper_text.setter
    def helper_text(self, arg: str, /) -> None: ...

    @property
    def selection(self) -> str:
        """The selected text."""

    @property
    def cursor(self) -> int:
        """The caret, as a byte offset."""

    @cursor.setter
    def cursor(self, arg: int, /) -> None: ...

    def on_text_changed(self, callback: EventCallbackT, /) -> EventCallbackT:
        """Its text changed (typing, or setting text)."""

    def on_return_pressed(self, callback: EventCallbackT, /) -> EventCallbackT:
        """Return was pressed in it."""

    def on_lost_focus(self, callback: EventCallbackT, /) -> EventCallbackT:
        """It lost the keyboard focus."""

    def on_error_changed(self, callback: EventCallbackT, /) -> EventCallbackT:
        """Its error message changed."""

class TextBoxMultiline(TextBox):
    """Several lines of editable text (controls::TextBoxMultiline)."""

class TextBoxNumeric(TextBox):
    """A TextBox that takes a number only (controls::TextBoxNumeric)."""

    @property
    def value(self) -> float:
        """Its text as a number (0 for none)."""

class PasswordTextBox(TextBox):
    """
    A TextBox showing a mask character for each one typed (controls::PasswordTextBox).
    """

    @property
    def mask(self) -> str:
        """The character shown for each one of the password (one codepoint)."""

    @mask.setter
    def mask(self, arg: str, /) -> None: ...

class Slider(Control):
    """
    A value on a track with a thumb (controls::Slider); add a HorizontalSlider or a
    VerticalSlider.
    """

    @property
    def value(self) -> float:
        """Its value, in its range; setting it fires on_value_changed."""

    @value.setter
    def value(self, arg: float, /) -> None: ...

    @property
    def range(self) -> tuple[float, float]:
        """(minimum, maximum)."""

    @range.setter
    def range(self, arg: tuple[float, float], /) -> None: ...

    @property
    def notch_count(self) -> int:
        """How many notches the track shows."""

    @notch_count.setter
    def notch_count(self, arg: int, /) -> None: ...

    @property
    def clamp_to_notches(self) -> bool:
        """Whether a value snaps to the nearest notch."""

    @clamp_to_notches.setter
    def clamp_to_notches(self, arg: bool, /) -> None: ...

    @property
    def value_format(self) -> str:
        """A printf format showing the value as text ("%.2f"; "" for none)."""

    @value_format.setter
    def value_format(self, arg: str, /) -> None: ...

    def on_value_changed(self, callback: EventCallbackT, /) -> EventCallbackT:
        """Its value changed."""

class HorizontalSlider(Slider):
    """A Slider along x."""

class VerticalSlider(Slider):
    """A Slider along y."""

class ComboBox(Button):
    """A button opening a list to choose one item from (controls::ComboBox)."""

    def add_item(self, label: str, name: str = '') -> MenuItem:
        """Adds an item; returns its handle."""

    def select(self, name: str) -> None:
        """Selects the item of that name (fires on_selection)."""

    def clear_items(self) -> None:
        """Removes every item."""

    @property
    def selected_text(self) -> str:
        """The selected item's label ("" with none)."""

    @property
    def selected_name(self) -> str:
        """The selected item's name ("" with none)."""

    @property
    def is_open(self) -> bool:
        """Whether its list is open."""

    def on_selection(self, callback: EventCallbackT, /) -> EventCallbackT:
        """An item was chosen."""

class ListBox(ScrollControl):
    """A scrolling list of rows to select (controls::ListBox)."""

    def add_item(self, label: str, name: str = '') -> Control:
        """Adds a row; returns its handle."""

    def select(self, text: str) -> None:
        """Selects the row with that text (fires on_row_selected)."""

    def clear(self) -> None:
        """Removes every row."""

    @property
    def selected_name(self) -> str:
        """The selected row's name."""

    @property
    def multi_select(self) -> bool:
        """Whether several rows can be selected."""

    @multi_select.setter
    def multi_select(self, arg: bool, /) -> None: ...

    def on_row_selected(self, callback: EventCallbackT, /) -> EventCallbackT:
        """A row was selected."""

class ProgressBar(Label):
    """A bar filled to a fraction (controls::ProgressBar)."""

    @property
    def progress(self) -> float:
        """The fraction filled, 0..1."""

    @progress.setter
    def progress(self, arg: float, /) -> None: ...

    @property
    def indeterminate(self) -> bool:
        """A segment moving along it instead of the fill."""

    @indeterminate.setter
    def indeterminate(self, arg: bool, /) -> None: ...

    @property
    def horizontal(self) -> bool:
        """Filled along x (else along y)."""

    @horizontal.setter
    def horizontal(self, arg: bool, /) -> None: ...

    def set_auto_label(self, on: bool) -> None:
        """Whether its text is the percentage."""

class Switch(Button):
    """An on/off toggle with a sliding thumb (controls::Switch)."""

    @property
    def on(self) -> bool:
        """Whether it is on; setting it fires on_toggled and on_toggled_on/off."""

    @on.setter
    def on(self, arg: bool, /) -> None: ...

    @property
    def label_on_left(self) -> bool:
        """Its text on the left of the track."""

    @label_on_left.setter
    def label_on_left(self, arg: bool, /) -> None: ...

    def on_toggled(self, callback: EventCallbackT, /) -> EventCallbackT:
        """It was turned on or off."""

    def on_toggled_on(self, callback: EventCallbackT, /) -> EventCallbackT:
        """It was turned on."""

    def on_toggled_off(self, callback: EventCallbackT, /) -> EventCallbackT:
        """It was turned off."""

class Spinner(Control):
    """A busy indicator, an arc turning round (controls::Spinner)."""

    def start(self) -> None:
        """Turns it."""

    def stop(self) -> None:
        """Stops it (hidden, unless hide_when_stopped is False)."""

    @property
    def spinning(self) -> bool: ...

    @property
    def speed(self) -> float:
        """Turns a second."""

    @speed.setter
    def speed(self, arg: float, /) -> None: ...

    @property
    def diameter(self) -> int:
        """Its diameter in units."""

    @diameter.setter
    def diameter(self, arg: int, /) -> None: ...

    @property
    def hide_when_stopped(self) -> bool:
        """Hidden while stopped (else a still ring)."""

    @hide_when_stopped.setter
    def hide_when_stopped(self, arg: bool, /) -> None: ...

    @property
    def show_track(self) -> bool:
        """A ring under the arc."""

    @show_track.setter
    def show_track(self, arg: bool, /) -> None: ...

class ChipTone(enum.Enum):
    """A Chip's colour (ChipTone)."""

    NEUTRAL = 0

    ACCENT = 1

    SUCCESS = 2

    WARNING = 3

    DANGER = 4

class Chip(Button):
    """A toned label, clickable or removable (controls::Chip)."""

    @property
    def tone(self) -> ChipTone: ...

    @tone.setter
    def tone(self, arg: ChipTone, /) -> None: ...

    @property
    def palette_index(self) -> int:
        """A palette colour (0..7) in place of the tone; -1 for the tone."""

    @palette_index.setter
    def palette_index(self, arg: int, /) -> None: ...

    @property
    def clickable(self) -> bool:
        """Whether it takes a press (on_press)."""

    @clickable.setter
    def clickable(self, arg: bool, /) -> None: ...

    @property
    def removable(self) -> bool:
        """Whether it has an x that fires on_remove (the app deletes it)."""

    @removable.setter
    def removable(self, arg: bool, /) -> None: ...

    def on_remove(self, callback: EventCallbackT, /) -> EventCallbackT:
        """
        Its x was clicked; the chip deletes nothing itself (call delete_later()).
        """

ControlT = TypeVar("ControlT", bound=Control)

EventCallbackT = TypeVar("EventCallbackT", bound=Callable[[Event], object])

class Headless:
    """
    A canvas rendered offscreen (app::Headless): surfaceless EGL, the textured skin,
    a stepped clock from 0 (1/60 s a frame, a second before each press) and
    input as ControlTest's flags give it. One App or Headless is alive at a time.
    """

    def __init__(self, size: tuple[int, int] = (640, 480), scale: float = 1.0, resources: str | None = None) -> None:
        """
        size is the canvas in logical units, scale device px per unit, resources a
        directory to load the skin and fonts from (the package's by default).
        """

    def close(self) -> None:
        """Deletes the canvas and frees the device; the handles go stale."""

    @property
    def closed(self) -> bool: ...

    def __enter__(self) -> Headless: ...

    def __exit__(self, *args: object) -> None: ...

    @property
    def canvas(self) -> Canvas:
        """The canvas's handle."""

    @property
    def pixel_size(self) -> tuple[int, int]:
        """The target in device px."""

    @property
    def degraded(self) -> bool:
        """A resource did not load (its line is on stderr)."""

    @property
    def seconds(self) -> float:
        """The clock now."""

    @property
    def frame_count(self) -> int:
        """Frames drawn so far."""

    def frame(self, count: int = 1) -> None:
        """Draws count frames: the update, the render, the clock a step on."""

    def move(self, x: int, y: int) -> None:
        """The mouse to (x, y), in logical units."""

    def press(self, x: int, y: int, button: int = 0) -> None:
        """A button (0 left, 1 right) pressed at (x, y), the clock a second on."""

    def release(self, x: int, y: int, button: int = 0) -> None:
        """A button released at (x, y)."""

    def click(self, x: int, y: int, button: int = 0) -> None:
        """press() and release() at (x, y)."""

    def wheel(self, delta: int) -> bool:
        """
        A vertical wheel of delta (60 a notch, down negative); whether the UI took it.
        """

    def wheel_h(self, delta: int) -> bool:
        """A horizontal wheel of delta; whether the UI took it."""

    def key(self, chord: str) -> None:
        """
        A key or chord as ControlTest's --key takes it: "return", "ctrl+a", "x".
        """

    def type(self, text: str) -> None:
        """Text typed, one codepoint at a time."""

    def pixels(self) -> bytes:
        """The target as top-down RGBA8 rows, pixel_size large."""

    def save_png(self, path: str) -> None:
        """Writes the target as a PNG (after a frame)."""

    def stats(self) -> dict:
        """The last frame's counters, keyed as ControlTest --stats prints them."""

class App:
    """
    A canvas in a window gweni owns (app::Window), in the textured skin. run() is
    its idle loop: a frame only when the canvas has work. One App or Headless is
    alive at a time.
    """

    def __init__(self, title: str = 'gweni', size: tuple[int, int] = (800, 600), resources: str | None = None, scale: float = 1.0) -> None:
        """
        size is the window's framebuffer in device px, resources a directory to load
        the skin and fonts from (the package's by default), scale a factor on the OS
        content scale. Raises RuntimeError without a display.
        """

    def run(self) -> None:
        """
        Shows the window and runs its loop until it is closed or quit() is called,
        raising what a callback raised and KeyboardInterrupt on Ctrl+C. The main
        thread only.
        """

    def quit(self) -> None:
        """Ends run() after the current iteration (a callback may call it)."""

    def close(self) -> None:
        """Deletes the canvas and the window; the handles go stale."""

    @property
    def closed(self) -> bool: ...

    @property
    def frame_count(self) -> int:
        """Frames run() has drawn (read from any thread)."""

    def __enter__(self) -> App: ...

    def __exit__(self, *args: object) -> None: ...

    @property
    def canvas(self) -> Canvas:
        """The canvas's handle."""

    @property
    def degraded(self) -> bool:
        """A resource did not load (its line is on stderr)."""
