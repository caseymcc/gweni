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


## Sizing a dock-None control

A dock-None control, placed by its parent's `arrangeHorizontal/Vertical` or by
a layout handler, is sized from three things only: the size it was **asked
for**, its preferred size and its size flags. It never reads the bounds an
earlier pass gave it, so a control that grew for a 1024-wide canvas shrinks
back at 640, and the frame is a function of the current state.

What counts as asked for: every `setBounds`/`setSize`/`setPos` that does not
come from the generic arrange code (the dock branches, `policyResize`, a
`LayoutItem` placing its control, which all write through
`Base::setArrangedBounds`). App calls, constructors and a control's own
`layout()` placing its children all are requests. `setPos`/`moveTo` ask for the
position only, `setSize` for the size, `setWidth`/`setHeight` for one axis,
`setBounds` for both. `getRequestedBounds()` returns it; it starts as the
initial bounds, and `hasRequestedSize(dim)` says whether a size was asked.

With r the request, p the preferred size:

| flag | in a Base parent (`dockNoneSize`) | in a layout cell c (`layoutCellSize`) |
|---|---|---|
| Fixed | r | r |
| Expand | max(r, p) | min(max(r, p), c) |
| Shrink | min(r, p) | min(r, c) |
| Elastic, Bloat | p | min(max(r, p), c) |
| no size asked | p | c |

A Base parent has no cell, so Expand is not capped there: content larger than
the parent overflows it (a ScrollControl's panel). `Fixed`'s preferred, minimum
and maximum size are its request too. `sizeOfChildren` counts a dock-None
child at its requested position, not at the one the last pass gave it.

Docked children keep the size their dock gives them.

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

### Children whose X layout needs the parent's height

The X pass runs before any height is known, so a control whose children's
widths depend on its own height cannot lay them out in X. `ScrollControl` is
one: whether its vertical bar shows depends on its height, and the bar takes
width from the content. It leaves its subtree alone in the X pass and, at its
Y arrange, once its own size, the content's preferred size and the bars' are
all final, decides the bars in arithmetic, sizes the inner panel and the bars,
and then lays its children out X and then Y. Its preferred size is its
content's (inner panel plus margins), not counting the bars or the scroll
position, so it does not depend on what an earlier frame showed.

The inputs such a control writes into children it has not laid out yet
(visibility, scroll range, bounds) go through `Base::PlacementScope`, so the
invalidations they raise count as placement rather than as a re-dirty. Keep
the scope to those writes: anything a control already laid out has read must
still be reported.
