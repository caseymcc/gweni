# gweni from Python

The `gweni` package is gweni's Python module: Python builds the control tree
and handles the events, and the C++ library lays it out and draws only what
changed, in a window it owns, in the textured Tokyo Night skin. The update and
render phases and their change tracking are the library's own, so Python adds
the cost of the callbacks it runs and nothing per frame.

```python
import gweni

app = gweni.App(title="Hello", size=(800, 600))
win = app.canvas.add(gweni.Window, title="Settings")
label = win.add(gweni.Label, text="0 clicks", dock=gweni.Dock.TOP)
btn = win.add(gweni.Button, text="Click me", dock=gweni.Dock.TOP)
clicks = 0

@btn.on_press
def _(event: gweni.Event) -> None:
    global clicks
    clicks += 1
    label.text = f"{clicks} clicks"

app.run()
```

It is Linux x86_64 only, for CPython 3.12 and newer (one `cp312-abi3` wheel),
and needs a glvnd OpenGL driver (`libOpenGL.so.0`, `libGLX.so.0`,
`libEGL.so.1`), as the container-built exes do.

## Building it

Everything runs in the build container (`AGENTS.md` → "Building"). The image
has a uv-managed CPython 3.12 at `/opt/python/3.12` and a venv on it,
`/opt/python/venv` (`GWENI_PYTHON`), with pinned pytest, mypy, nanobind,
scikit-build-core, auditwheel and patchelf, and their wheels in
`/opt/python/wheelhouse` (`GWENI_WHEELHOUSE`), so a build and an install need
no network. Jammy's own `python3` (3.10) stays for `tools/*.py`.

The module is part of the desktop Linux debug, release and profile trees
(`GWENI_BUILD_PYTHON`, which `./generate.sh` turns on there, with
`-DPython_EXECUTABLE=$GWENI_PYTHON`); the GLES, Vulkan-only, ASAN and web trees
leave it out. Every build stages the package as it is installed, in
`<tree>/python/gweni/`:

```bash
./generate.sh linux
cmake --build build_linux_x64_debug -j
PYTHONPATH=build_linux_x64_debug/python $GWENI_PYTHON python/examples/button_demo.py /tmp/b.png
```

In a container from an image older than the module, `GWENI_PYTHON` points
`generate.sh` at another 3.12 (one with nanobind installed); without one the
configure goes on with no module and the Python tests fail, saying so.

### The wheel

```bash
./tools/build_wheel.sh                     # dist/gweni-0.1.0-cp312-abi3-linux_x86_64.whl
./tools/build_wheel.sh --out /tmp/wheels --vcpkg-installed build_linux_x64_release/vcpkg_installed --offline
```

`tools/build_wheel.sh` runs `uv build --wheel --no-build-isolation` on
`GWENI_PYTHON` with the configure arguments `./generate.sh linux release
--print-cmake-args` prints (the one place the configuration is decided), with
Vulkan, the examples and the tests off and the module on, in the persistent
`build_linux_x64_release_wheel/` (a second build is incremental).
`pyproject.toml` (scikit-build-core) reads the version from `CMakeLists.txt`'s
`project(gweni VERSION ...)` and installs the CMake component `python` into the
wheel: the module, its stub, `py.typed`, `resources/` (the skin, its atlas and
the fonts) and `licenses/` (the licence of every library the module links
statically; `python/THIRD_PARTY_NOTICES.md` lists them). The wheel is
`linux_x86_64`; the manylinux repair and publishing are a later task's.

Install it into any 3.12+ environment, with no compiler, vcpkg or build tree:

```bash
python3.12 -m venv /tmp/venv && /tmp/venv/bin/pip install dist/gweni-*.whl
uv pip install --python /tmp/venv/bin/python dist/gweni-*.whl
```

A wheel is installer-agnostic: poetry and `uv add` read the same metadata (not
tested here). An older Python's pip finds no wheel it can install.

## The API by example

### Building a tree

`parent.add(Type, **props)` makes a child of the bound class `Type` and sets
each keyword as a property; it is typed to return a `Type`.

```python
panel = app.canvas.add(gweni.Panel, dock=gweni.Dock.CENTER, layout=gweni.Layout.VERTICAL)
name = panel.add(gweni.TextBox, placeholder="Your name")
ok = panel.add(gweni.Button, text="OK", tooltip="Save it", enabled=False)
group = panel.add(gweni.RadioButtonController, size=(200, 60))
small = group.add_option("Small", "s")
group.add_option("Large", "l")
small.select()
```

A keyword that is no property of `Type` raises `AttributeError` and makes
nothing; a value of the wrong type raises `TypeError` and the half-made child
is deleted.

### Events

Every event is a method taking a callable of one `Event` (`control`, `string`,
`point`, `integer`, from `event::Info`); it gives the callable back, so it is a
decorator too:

```python
@name.on_text_changed
def _(event: gweni.Event) -> None:
    ok.enabled = bool(name.text)

ok.on_press(lambda event: gweni.message(app.canvas, "Saved", f"Hello {name.text}"))
```

The callback is held by the control's C++ event caller with the control as
its handler, so it goes when the control does.

### Dialogs, toasts, menus, tabs

```python
gweni.confirm(app.canvas, "Delete", "Delete it?", gweni.DialogButtons.YES_NO,
              on_result=lambda result, text: print(result))      # DialogResult.YES or NO
gweni.prompt(app.canvas, "Rename", "New name:", "old",
             on_result=lambda result, text: print(result, text))
app.canvas.notify(gweni.ToastKind.POSITIVE, "Saved", title="Done", timeout=4.0)

strip = app.canvas.add(gweni.MenuStrip, dock=gweni.Dock.TOP)
file_menu = strip.add_item("File").menu
file_menu.add_item("Open", "Ctrl+O").on_selected(lambda e: print("open"))

tabs = app.canvas.add(gweni.TabControl, dock=gweni.Dock.CENTER)
page = tabs.add_page("First").page
page.add(gweni.Label, text="on the first page", dock=gweni.Dock.TOP)
```

### Headless: captures, pixels and counters

`gweni.Headless(size=(640, 480), scale=1.0, resources=None)` is the same
canvas offscreen (surfaceless EGL), driven as ControlTest's flags drive it, on
a stepped clock from 0 for each instance (1/60 s a frame, a second before each
press, so two presses are never a double-click), with the input handler's
pointer state reset. It is the module's test harness and capture API:

```python
with gweni.Headless(size=(320, 200)) as h:
    btn = h.canvas.add(gweni.Button, text="Press", dock=gweni.Dock.TOP)
    h.frame(3)
    x, y, w, hh = btn.canvas_bounds
    h.move(x + w // 2, y + hh // 2)
    h.frame()
    print(h.stats()["prim_slots_written"])   # the hover's one slot
    h.click(x + w // 2, y + hh // 2)
    h.key("ctrl+a"); h.type("text"); h.wheel(-60)
    h.frame()
    h.save_png("/tmp/shot.png")
    rgba = h.pixels()                         # top-down RGBA8 rows, pixel_size large
```

`stats()` is the last frame's counters as a dict keyed as `ControlTest --stats`
prints them (`frame`, `control`, `controls_changed`, `prim_slots_written`,
`text_slots_written`, `layout_passes`, ..., ints where they are numbers). Two
instances built the same way, one after the other in one process, draw and
count the same, byte for byte.

## Names: Python and C++

| Python | C++ |
|---|---|
| `App(title, size, resources, scale)`, `run()`, `quit()`, `close()` | `app::Window`, `Window::run()` (`Device::runWindowLoop`, idle) |
| `Headless(size, scale, resources)`, `frame/move/press/release/click/wheel/wheel_h/key/type`, `pixels`, `save_png`, `stats` | `app::Headless` and the same-named methods, `savePng`, `frameStatFields` |
| `Control` (a handle: `id`, `alive`) | a control id, `controls::Base::getControlById` |
| `Panel` | a plain `controls::Base` |
| `Canvas`, `canvas.post(fn)`, `canvas.notify(...)` | `controls::Canvas`, `Canvas::post`, `Canvas::notify` |
| `ctrl.add(Type, **props)` | `parent->newChild<T>()` and the setters |
| `ctrl.delete_later()` | `Base::delayedDelete()` |
| `dock`, `layout` | `setDock(DockPosition)`, `setLayout(VerticalLayout/HorizontalLayout)` |
| `position`, `size`, `bounds`, `canvas_bounds` | `setPos`/`getPos`, `setSize`, `getBounds`, `getCanvasBounds` |
| `size_flags`, `margin`, `padding` | `setSizeFlags(SizeFlags)`, `setMargin`, `setPadding` |
| `hidden`, `visible`, `show()`, `hide()` | `setHidden`/`hidden()`, `visible()`, `show()`, `hide()` |
| `enabled` | `!isDisabled()`, `setDisabled(!enabled)` |
| `tooltip` | `setToolTipText`/`getToolTipText` |
| `opacity`, `set_opacity(surface, text)` | `setOpacity`, `clearOpacity` (None), `ownSurfaceOpacity` |
| `focus()`, `unfocus()`, `focused` | `focus()`, `unfocus()`, `isFocussed()` |
| `on_press`, `on_toggle`, `on_hover_enter`, ... | `onPressCaller`, `onToggleCaller`, `onHoverEnter`, ... (`event::Caller::addCallback`) |
| `Event` (`control`, `string`, `point`, `integer`) | `event::Info` |
| `Label.text`, `Button.is_toggle`/`toggled` | `getText`/`setText`, `setIsToggle`, `getToggleState`/`setToggleState` |
| `CheckBox`, `CheckBoxWithLabel` (`checked`, `on_check_changed`) | `controls::CheckBox`, `CheckBoxWithLabel` (`onCheckChanged`) |
| `RadioButtonController.add_option()`, `RadioButton` | `RadioButtonController::addOption`, `LabeledRadioButton` |
| `TextBox`, `TextBoxMultiline`, `TextBoxNumeric`, `PasswordTextBox` | the same-named controls (`onTextChangedCaller`, `onReturnPressedCaller`) |
| `HorizontalSlider`, `VerticalSlider` (`value`, `range`) | `controls::HorizontalSlider`, `VerticalSlider` (`setFloatValue`, `setRange`) |
| `ComboBox`, `ListBox`, `ProgressBar`, `Switch`, `Spinner`, `Chip` | the same-named controls |
| `ScrollControl`, `GroupBox`, `TabControl`/`TabButton`, `Window` | the same-named controls, `WindowControl` |
| `MenuStrip`, `Menu`, `MenuItem` | the same-named controls |
| `message`, `confirm`, `prompt` (`on_result(result, text)`), `Dialog`, `PromptDialog`, `DialogResult` | `dialogs::message`, `confirm`, `prompt`, `ResultHandler`, `dialogs::Dialog`, `dialogs::Result` |
| `Toast`, `ToastKind` | `controls::Toast`, `ToastKind` |
| `Flow` | `controls::layout::Flow` |
| `ControlDeletedError` (a `ReferenceError`), `ThreadError` (a `RuntimeError`) | the module's own |

`python/tools/unbound_controls.py` lists the control types `controls.h`
declares that no Python class shows (the data and editor controls come later).

## The rules

**C++ owns the tree.** A `Control` is a handle holding the control's id, looked
up on every use (O(1); ids are never reused). A deleted control raises
`gweni.ControlDeletedError` and never crashes: after `delete_later()` and the
next frame, a closed dialog, a dismissed toast, a removed chip, or the App or
Headless closed. Dropping a handle, or garbage collection, deletes nothing.
A control C++ made (`event.control`, `children`, a dialog's buttons) comes back
as its bound class by type name, else as `Control`.

**Errors in callbacks.** An exception a callback raises never unwinds through
gweni: the event or frame finishes (the other callbacks of the event run too),
and the call into the module that ran it raises it afterwards: `run()`, the
`Headless` method, or the setter or `add()` whose change fired the event. Later
exceptions of the same call hang off the first one's `__context__` chain. The UI
stays usable.

**One thread.** Every call that touches the tree checks that it runs on the
thread that made the App or Headless and raises `gweni.ThreadError` otherwise.
`run()` releases the GIL for its whole loop, drawing and waiting, and takes it
only to run Python (a callback, a post, the signal check), so another Python
thread runs alongside it, also while an animation draws every iteration; it may
only call `canvas.post(fn)`, from any thread: `fn` runs
on the UI thread at the top of the next update, and the post wakes an idle
`run()`. `post` returns False once the canvas is gone.

**No re-entry.** `run()`, `close()`, `frame()` and every `Headless` driving
method called from inside a callback raise `RuntimeError`; setters and the tree
stay open to a callback, and `app.quit()` may be called from one.

**One instance.** At most one App or Headless is alive in a process; a second
raises `RuntimeError`. One after another works (pytest fixtures). One left open
at exit is closed by an atexit hook, so its callables are released while
Python can.

**The idle loop and Ctrl+C.** `run()` draws a frame only when the canvas has
work and otherwise blocks in GLFW's wait. Python's SIGINT
handler only sets a flag and writes the wakeup fd, and GLFW's wait retries on
`EINTR`, so `run()` points `signal.set_wakeup_fd` at a pipe of its own whose
helper thread wakes the loop (`glfwPostEmptyEvent`) and passes each byte on to
the fd set before (an asyncio loop's); `run()` gives that fd back when it
returns. After every iteration the signal handlers run: Ctrl+C ends `run()`
with `KeyboardInterrupt`. Python delivers signals to the main thread only, so
`run()` off it raises `RuntimeError`.

## Typing

The package ships `_gweni.pyi` and `py.typed`. `add()` is typed
`type[ControlT] -> ControlT` and every event method gives back the callable it
was given (`EventCallbackT`, bound to `Callable[[Event], object]`), so

```bash
mypy --strict python/examples/
```

passes. The stub is generated by the pinned nanobind's stubgen from the built
module and checked in; after a change to the bindings regenerate it with
`cmake --build <tree> --target gweni_python_stubs` (`python_stubs` fails while
it differs).

## Tests

All labelled `python`; the ones that make a context `gpu` as well, run under
the `render` lock (`AGENTS.md` → "Tests"):

| Test | What |
|---|---|
| `python_pytest` | `python/tests` without a context |
| `python_pytest_gpu` (`gpu`) | the Headless and App suite: handles, events, errors, threads, post, every control family, order independence |
| `python_exports` | the module exports `PyInit__gweni` alone |
| `python_stubs` | the checked-in stub is the built module's |
| `python_mypy` | `mypy --strict python/examples/` |
| `python_parity` (`gpu`) | `button_demo.py` against `ControlTest --headless --control ButtonDemo`: the same pixels and counters |
| `python_window_idle` (`gpu`) | `App.run()` under Xvfb: a silent idle, one frame per post, Ctrl+C, the wakeup fd, a worker's post landing while a Spinner turns |
| `python_wheel` | the release tree only: the wheel built offline, installed with pip and uv, smoke-tested from the installed package |
