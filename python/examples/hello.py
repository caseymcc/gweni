#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
"""The gweni module's hello window: a Window whose label counts a button's
clicks, run in its idle loop until it is closed or Ctrl+C."""

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
