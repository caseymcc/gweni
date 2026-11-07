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


## Sizing a dock-None control

A dock-None control, placed by its parent's `arrangeHorizontal/Vertical` or by
a layout handler, is sized from three things only: the size it was **asked
for**, its preferred size and its size flags. It never reads the bounds an
earlier pass gave it, so a control that grew for a 1024-wide canvas shrinks
back at 640, and the frame is a function of the current state.

What counts as asked for: **only the app's calls**, plus a constructor's
default size on the control itself (a Slider's 32x128, a Label's 100x10),
which is the default the app overrides. A user dragging a window or a
resizable control asks too: it is persistent user state. Everything the
library computes is not a request, inside a layout pass or not (see
"Requested and computed bounds" below). `setPos`/`moveTo` ask for the position
only, `setSize` for the size, `setWidth`/`setHeight` for one axis, `setBounds`
for both. `getRequestedBounds()` returns it; it starts as the initial bounds,
and `hasRequestedSize(dim)` says whether a size was asked.

With r the request, p the preferred size and c a layout cell less the child's
margins (never below 0; the child is placed at the cell plus
`margin.left`/`margin.top`):

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

Along a `VerticalLayout`'s or `HorizontalLayout`'s stacking axis the cell is
the child's preferred size plus its margins, grown when the layout has room
left and the flag can grow, and cut when it has too little and the flag can
shrink; Fixed does neither. Room left goes to the Bloat children if there are
any, otherwise to Expand and Elastic; room short comes out of the Bloat
children if there are any, otherwise out of Shrink and Elastic. The rules are
the same in a row and in a column (`LayoutItem::arrangeAlong`). So a Shrink child
with a request bigger than its content is cut to the content, and a Fixed one
takes its own size and the next child starts after it and its margins. Use
Fixed for "exactly this size" in a layout (ButtonDemo's 20x20 button).

Across the stacking axis the cell is the child's preferred size plus its
margins, or the whole length inside the layout's padding when it is shorter
and the flag can grow (Expand, Elastic, Bloat), or longer and the flag can
shrink (Shrink, Elastic, Bloat) (`arrangeAcross`). A layout's padding
(`LayoutItem::setPadding`, 0 by default) insets its children on both axes and
is never added to a child's cell; its spacing (`setSpacing`, 2 by default) is
the gap between consecutive children. A hidden child takes no space and no
spacing and keeps the bounds it had, as in a dock layout; shown again it is
laid out as if it had never been hidden. A nested layout always counts.

A ScrollControl (ListBox, CollapsibleList, Menu, ...) takes p from its request
on an axis where a size was asked: the request is its outer size, the bars go
inside it and the content scrolls. Expand then gives r in a Base parent and
min(r, c) in a cell, and `placeContents()` shows the bar when the content does
not fit. On an axis with no request p is its content. So a 100x200 ListBox of
13 rows is 100x200 with a bar, and code that sizes a ScrollControl
incidentally must not ask for the axis it means to grow along: a Menu asks for
no height (a ComboBox opening its list sets the position and width only). A
subclass that never scrolls an axis opts out with `sizedByRequest()`: a
MenuStrip is as high as its items, its 200x22 only the least it gets.

Docked children keep the size their dock gives them.

## Requested and computed bounds

Each control keeps the bounds it is **asked for** (`getRequestedBounds()`)
apart from the bounds layout **computes** (`getBounds()`), and a preferred size
is made of requests and content only, never of what an earlier pass computed.
So the library writes bounds through two setters that record no request:

* `setArrangedBounds(x, y, w, h)`, protected: a control's own bounds. The
  generic arrange code (the dock branches, `policyResize`, a `LayoutItem`
  placing its control) writes through it, and so does a control computing its
  own size (a wrapped Text taking the height of its lines).
* `placeChild(child, rect)`, protected and static, so a subclass can call it on
  any child; `placeChildPos` keeps the child's size and `placeChildSize` its
  position: a control placing a child it owns. It also opens a
  `PlacementScope`: what it invalidates is placement, since the owner lays the
  child out next. A size change on a child the pass has already arranged on
  that axis still re-dirties, as any other.

A child that its control places itself is marked **owner-placed**,
`setPlacement(Placement::Owner)`, when the control creates or adopts it: in its
constructor, or wherever it takes the child in (`CrossSplitter::setPanel`,
`PropertyRow::setProperty`). It is declared up front because the parent's
`calculateSize` runs before its `arrange`, so it must already leave the child
out. The generic `arrangeHorizontal`/`arrangeVertical` skip an Owner child
(no dock, no alignment, no `policyResize`), and `sizeOfChildren` and
`childrenSize` do not count it: neither its position nor its size is part of
the parent's preferred size. It is still measured, and still arranged in turn
for its own subtree. A control that needs content size from an Owner child asks
for it in its own `calculateSize` (PropertyRow adds the split and its
property's preferred width; ColorPicker its Result box at x 180; a TextBox
its scrolled string). The children placed this way:

| owner | Owner children | placed in |
|---|---|---|
| CrossSplitter | its bars and panels | `layout()` |
| Properties, PropertyRow | the splitter bar; a row's label and property | `arrange()` |
| Slider | the thumb | `layout()`, a value change, a click |
| ScrollBar | the thumb | `layout()`, a scroll |
| ScrollControl | both bars and the inner panel | `placeContents()`, its Y arrange |
| TreeNode | the toggle | sized in `arrange()`, centred in `postLayout()` |
| ColorPicker | the Result box | `arrange()` |
| TextBox | its Text while the string is scrolled | `makeCaretVisible()` |
| TabControl, TabStrip | the overflow buttons; the drop marker over a tab | `layout()`; a drag hover |
| Text | its wrapped lines | its X arrange |

A `Dragger` (a splitter bar, a slider or scrollbar thumb) whose target is an
Owner child moves it as computed bounds, and its owner reads the new position
as its value; any other target (a window by its title bar) is moved with
`moveTo`, a request.

What still counts as a request, besides the app's own calls:

* **Defaults set where a control is made**: a constructor sizing itself or a
  part it creates, and the factory methods that create a part and set it up
  (`Menu::addItem`, `TabControl::addPage`, `CategoryInner::add`, a submenu's
  arrow in `MenuItem::getMenu`). They run once, outside any pass, and are the
  defaults the app overrides. A part created during a pass is Owner (a wrapped
  Text's lines), so its constructor's default is not read.
* **Where a popup was opened**: `Menu::openAt`, a ComboBox's list, a submenu,
  the tooltip box. That is the popup's state while it is open, like a window's
  position; keeping it inside the canvas is computed (`Menu::layout`, from
  that request each pass, so a canvas that grows again gives it back).
* **App-facing helpers**: `sizeToContents()`, `sizeToChildren()`,
  `ScrollControl::setInnerSize()` ask for the size they compute, as if the app
  had passed it. Library code does not call them from a pass: the parent
  gives a control the size it measures (`calculateSize`) instead.
* **What the user drags**: a window by its title bar, a `Resizer`.

A control whose content along an axis is the size it was asked to be says so
with `sizedByRequest(dim)`: `Base::calculateSize` then takes the request as its
content, not its children (a Slider, whose track its skin draws; a
ScrollControl, see above).

`doNotIncludeInSize()` is the one other way out of the parent's size, kept for
exactly one case: a child the generic arrange still places, by its alignment
and `policyResize` like any other, but that does not count in the parent's
size (TextBox's placeholder, a hint that must not widen a `Shrink` box). It is
not folded into `Placement`, because `Placement` says who places a child and
the placeholder is placed by the parent; making it Owner would have the
TextBox repeat the alignment the generic arrange already does for it.

`Canvas::getLayoutStats()` counts the requests recorded while a pass runs
(`requests`, and `requestedType`/`requestedId`, the first control that recorded
one), and debug builds print one line per control type that recorded one.
`ControlTest --stats` prints them on every frame line (`layout_requests=N
requested_by=<Type>#<id>|none`). Layout code
leaves it at 0; a request it records makes a preferred size depend on what an
earlier pass did (a CrossSplitter's bars asking for the canvas size they were
given). Every library control does: it is 0 on every frame of every
`ControlTest --list` demo, in the plain run, `--scenario resize` and
`--check-teardown`.

## Placing a dock-None control

Alignment is opt-in. A control has none by default (`Alignment::Absolute`),
and then it stays at the position it asked for: the x and y of its last
`setPos`, `moveTo` or `setBounds`, which only the app makes (or a default
where the control was made). Writes made by the arrange code and by the
library are not requests, so an earlier pass or a parent resize cannot move
it. A child its
parent places itself is owner-placed instead (see "Requested and computed
bounds").

An alignment places it inside the parent's padded content box: the parent's
size less its padding. The box does not depend on the docked siblings or on
the child's order among them, so a `Right`-aligned child sits at the far right
even over a `Right`-docked sibling; dock it too if it must stand aside. With
the box at x, w wide, and the child's margin m: `Left` puts it at x+m.left,
`Right` at x+w-width-m.right and `CenterH` at x+m.left+(w-width-m.left-m.right)/2;
`Top`, `Bottom` and `CenterV` do the same in y. Each axis is separate: a child
aligned only `CenterV` keeps its requested x.

`setAlignment` is only ever this placement in the parent. It never moves the
control's own content, and no control sets it on itself, so a Label, TextBox,
Button or any other control given a position keeps it. Where a Label's text
sits inside the label (and so a TextBox's, a Button's, ...) is its *content*
alignment, `setTextAlignment`: `Left|CenterV` by default (`Left|Top` once
`setWrap(true)`, and for a TextBoxMultiline), `Center` for a Button, read back
with `getTextAlignment()`. It is the inner Text's alignment, so the text is
placed by the same rules in the label's padded content box. A TextBox whose
string is wider than the box scrolls it instead: the inner Text is then
owner-placed, where the caret needs it (`TextBox::placeScrolledText`), and
aligned again once it fits. A text padding (`setTextPadding`)
is the inner Text's own padding: it is counted in the Text's size and the
string is drawn inside it (`Text::textOrigin()`).

## One pass per frame

A frame lays the canvas out in the update phase (`Canvas::doThink`, through
`Canvas::layoutCanvas`) in four steps over the whole tree: `calculateSize(X)`,
`arrange(X)`, `calculateSize(Y)`, `arrange(Y)`. **A control must converge in
that one pass**, the frame it is created in included: once the pass is over,
running it again must change nothing.

### What each step may read

* `calculateSize(X)` runs bottom-up before anything is placed: a control's
  preferred width may use its children's preferred widths and its own
  requested size, never its current bounds (an earlier pass's result).
* `arrange(X)` runs top-down. When it reaches a control, its parent has
  already given it its final width (and position); its height is still the
  previous pass's.
* `calculateSize(Y)` runs after every width is final, so a preferred height
  may depend on the control's width (wrapped text).
* `arrange(Y)` runs top-down again, each control getting its final height
  before it is arranged.

### When `layout()` runs

`Base::arrange` calls `layout()` at the start of the control's **X** arrange,
when its width is final, before it places its children; it is skipped when
nothing invalidated the control since the last pass. If the control is
invalidated again before its Y arrange (a new height from the Y pass does
that, as placement), `layout()` runs **again at the start of the Y arrange**,
with both its width and its height final. So:

* what `layout()` writes along X (children's x and width) must depend only on
  the width, so that the second run writes the same X geometry;
* what it computes from the height is right after the second run, which comes
  before the children are arranged in Y;
* it must not read a child's bounds the pass has not set yet: use the child's
  preferred size (`getPreferredSize()`), or the measured string
  (`Text::refreshSize(false)`), instead.

`postLayout()` runs at the end of the control's Y arrange, once its children
are placed. It may move a child (a position read by nothing else this pass,
TreeNode's toggle), not resize one.

### Patterns that keep a control to one pass

One rule decides who writes what (see "Requested and computed bounds"): the
app asks, through `setBounds`/`setSize`/`setPos`/`moveTo`; the library
computes, through `setArrangedBounds` for a control's own bounds and
`placeChild` for a child it places. The patterns below are that rule applied
to the order a pass runs in.

* **A child its control places itself is Owner.** Mark it
  `setPlacement(Placement::Owner)` where the control creates or adopts it, and
  write its bounds with `placeChild`/`placeChildPos`/`placeChildSize`: the
  generic arrange skips it and it does not count in the control's preferred
  size, so the size an earlier pass gave it cannot come back as what the
  control asks for. If the control's size must include it, add it in the
  control's own `calculateSize` from the child's preferred size (ColorPicker's
  Result box, a TextBox's scrolled string). The table in "Requested and
  computed bounds" lists them.
* **Handed its size before it is arranged.** An owner sizes an Owner child
  along an axis before the pass arranges the child along it: in `layout()`
  (the start of the owner's X arrange, and again in Y when the height
  changed) or in an `arrange(dim)` override before `ParentClass::arrange`
  (TreeNode's toggle, ColorPicker's Result box, Properties' bar). A size
  given after the child was arranged re-dirties the pass. A position may be
  set later, in `postLayout()`, when nothing laid out in this pass read it
  (TreeNode centring its toggle on the title).
* **A preferred size instead of a resize.** A control that wants to be as big
  as its content says so in `calculateSize` (ColorPicker, Menu, Properties,
  CollapsibleCategory, the Table under a ListBox), and its parent gives it that
  size. It never resizes itself from `layout()`/`postLayout()`, with
  `sizeToChildren()`, `sizeToContents()` or `setSize()`: those are requests
  (app helpers), and its parent has already placed it. A tab button is docked
  at the size it measures; it does not size itself to its label.
* **Children the generic arrange places are handed their inputs, not their
  bounds.** A TableRow sets its columns' preferred widths in its X arrange;
  the docks, margins, padding and visibility a control derives for its
  children are set at the start of its `calculateSize(X)`, before anything has
  measured them (TabStrip, TabButton, GroupBox, DockedTabControl, TreeNode).
  These are not bounds, so they are written under a `PlacementScope`, which is
  what it is still for: the invalidations they raise are placement, not a
  re-dirty. Settings that never change go in the constructor (ComboBox's
  arrow).
* **Outside a pass, the same rule.** A value change, a click or a drag on a
  part writes computed bounds (a slider's thumb, a splitter bar, a scrollbar
  thumb, a TabStrip's drop marker); only what the user moves as their own
  state (a window, a resized control) and where a popup was opened are
  requests.
* **Text** never resizes itself: it is the size of its string, which its
  parent gives it (`Text::policyResize`). Wrapped, it asks for the width the
  app asked its parent to be, less the parent's padding, or with no such
  request for its longest word (`Text::longestWordWidth`), so a parent that
  sizes to its content shrinks around it; it is laid out at its parent's
  width, its height is that of its lines there, and it updates its line
  children at the start of its X arrange when the string, the font (pointer
  or value), the wrap flag or that width changed (`Text::linesCurrent`). The
  wrap itself is data (`m_wrapped`: each line's text and how many characters
  of the string it holds), kept apart from the children: the char <-> line
  helpers (`getLineFromChar`, `getCharacterPosition`, `getClosestCharacter`,
  ...) read only that data, and re-wrap it on demand at the last layout's
  width when the string or font changed since (`Text::ensureWrapped`), so a
  key or click handled in the frame of an edit, before its layout, sees the
  edited string. That re-wrap touches no child; the X arrange then makes the
  children show the data, and does not wrap again for a width it already
  holds (`Text::wrapCount`). Before the first wrapped layout no width is
  known, and the helpers read the string unwrapped. The
  lines there are reused: each gets its new string and row, which writes
  nothing for a line that did not change, and only the extra lines are
  created or the surplus ones deleted. `setText` invalidates layout, so an
  edit is laid out by the next frame. Each line is an owner-placed child
  (`Placement::Owner`) placed at `(0, i * lineHeight)` and given that row
  height, an empty line included; its string has no `'\n'` (freetype-gl would break the run there),
  and the parent keeps how many characters of its string each line holds, the
  `'\n'` included (`getLineLength`), so caret indices still count every
  `'\n'`.

### What re-dirties a pass

Base tells the canvas when a control re-dirties layout while the pass runs:

* an explicit layout invalidation raised during the pass: `invalidate()`,
  `setHidden()`, `setDock()`, `setSizeFlags()`, `setMargin()`, a child added or
  removed, from a `layout()`, an `onChildBoundsChanged()` or anything else the
  pass calls;
* a new **width** for a control the pass has already arranged in X, or a new
  **height** for one it has already arranged in Y, e.g. a control resizing
  itself from its own `layout()` or `postLayout()`.

Placement does not count: the bounds a pass gives a control it has not reached
yet along that axis (the height the Y pass gives a control arranged in X
only), and the invalidations `setBounds()` raises for that control's children,
are inputs the same pass lays out when it gets there. Writes a control makes
to children it lays out next count as placement too: bounds through
`placeChild`, other inputs (visibility, docks, margins, a scroll range) under a
`PlacementScope`.

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
ctest holds every library control to it: `controltest_teardown` and
`controltest_resize` pass `--require-one-pass`, and `controltest_resize_each` and
`controltest_one_pass_each` run the resize scenario and the plain run for
each control in `--list` (`tests/eachControl.cmake`).

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

The inputs such a control writes into children it has not laid out yet count
as placement rather than as a re-dirty: their bounds through `placeChild` (the
bars and the inner panel are Owner), the rest (visibility, scroll range) under
a `Base::PlacementScope`. Keep the scope to those writes: anything a control
already laid out has read must still be reported.
