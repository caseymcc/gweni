gweni is based on a 2 part layout system

## Docking

By default all controls support a dock layout

* DockPosition::None
  * Child controls are positioned based on either absolute location or via Layout handler
* DockPosition::Top
  * Docked at the top of the parent control
* DockPosition::Bottom
    * Docked at the bottom of the parent control
* DockPosition::Left
    * Docked at the left of the parent control
* DockPosition::Right
    * Docked at the right of the parent control
* DockPosition::Center
    * Docked in the center dock and expands to the size of the parent



## Layout handlers

* Horizontal
* Vertical


setPosition()


## One pass per frame

A frame lays the canvas out in the update phase (`Canvas::doThink`, through
`Canvas::layoutCanvas`): `calculateSize` and `arrange` in X, then in Y, each
control's `layout()` running once, at the start of its X arrange. **A control
must converge in that one pass**, the frame it is created in included: once the
pass is over, running it again must change nothing.

What breaks that is a control that re-dirties layout while the pass runs. Base
tells the canvas when that happens:

* an explicit layout invalidation raised during the pass: `invalidate()`,
  `setHidden()`, `setDock()`, `setSizeFlags()`, a child added or removed, from a
  `layout()`, an `onChildBoundsChanged()` or anything else the pass calls;
* a new size for a control the pass has already laid out, e.g. a height set in
  the Y pass after the control's `layout()` ran in the X pass, or a control
  resizing itself from its own `layout()`.

Placement does not count: the bounds a pass gives a control it has not reached
yet, and the invalidations `setBounds()` raises for that control's children,
are inputs the same pass lays out when it gets there.

### The safety net

An invalidation raised during a pass is not lost: `recurseLayout` clears the
canvas's `m_updateLayout` before the pass, not after it, and a pass that
re-dirtied layout is followed by another in the same frame, at most
`Canvas::MaxLayoutPasses` (4). When the cap is hit a one-line warning names the
control (once per control type) and what is still dirty carries over:
`renderCanvas`'s own `recurseLayout` picks it up. Debug builds also print, once
per control type, every control that re-dirtied layout, so an app author sees
which of their controls lean on the loop.

The loop is for app controls the harness does not cover. Library controls are
held to one pass: `Canvas::getLayoutStats()` reports, per frame, the
update-phase passes, the first control that re-dirtied layout (type and control
id) and whether the render phase laid out again. `ControlTest --stats` prints
them on every frame line (`layout_passes=N redirtied_by=<Type>#<id>|none
render_layout=0|1`), and `ControlTest --require-one-pass` fails the run on any
frame with `layout_passes>1` or `render_layout=1`, naming the control, the
frame and the culprit on stderr. It works with `--control X`, any `--scenario`
and `--check-teardown`.
