# gweni for Python

gweni ("GUI Without Extravagant Nonsense") is a retained-mode, change-tracked
C++ GUI library on OpenGL; this package is its Python module. Python builds the
control tree and handles the events; gweni lays it out and draws only what
changed, in a window it owns, in its Tokyo Night skin.

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

`gweni.Headless` renders the same tree offscreen (surfaceless EGL) on a
stepped clock: captures, pixels and per-frame counters for tests.

Linux x86_64, CPython 3.12 and newer (one `cp312-abi3` wheel), with a glvnd
OpenGL driver. See `docs/python.md` in the repository for the whole API, the
thread and Ctrl+C rules and how to build the wheel.
