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
| PlotHistogram, PlotLines | its caption | `PlotBase::layout()` |
| DragNumber | its edit box | `layout()` |
| VectorField | its components' fields and axis tags | `layout()` |
| ScrollBar | the arrow buttons; the thumb | `layout()`; `layout()`, a scroll |
| ScrollControl | both bars and the inner panel | `placeContents()`, its Y arrange |
| TreeNode | the toggle | sized in `arrange()`, centred in `postLayout()` |
| ColorPicker | the Result box | `arrange()` |
| TextBox | its Text while the string is scrolled | `makeCaretVisible()` |
| TabControl, TabStrip | the overflow and tab-list buttons; the drop marker over a tab | `arrange()`, x in X and y in Y; a drag hover |
| Text | its wrapped lines | its X arrange |
| DataTable, its header and rows | the header and the body; the header's and each row's cells; a header cell's grip | the table's Y arrange (the body first, then the header at the body's content); `arrangeHorizontal`/`arrangeVertical` |

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
`Canvas::layoutCanvas`) in four steps: `calculateSize(X)`, `arrange(X)`,
`calculateSize(Y)`, `arrange(Y)`. Each step enters only the controls that are
dirty (see "Which controls a pass enters"). **A control must converge in that
one pass**, the frame it is created in included: once the pass is over,
running it again must change nothing.

### Which controls a pass enters

A pass enters the dirty paths and nothing else. A control is dirty
(`Base::layoutDirty()`, its `m_updateLayout`) when it or something under it
was invalidated, or when its bounds changed, since a pass last arranged it in
Y. An invalidation marks the control and every control holding it up to the
canvas, the inner panel a child is really held by included; a new bounds
marks the same path without invalidating the ancestors. Every walk over
children goes through `Base::doCalculateSize`/`Base::doArrange`, and they skip
a clean control. A clean control keeps its preferred size and its bounds, and
nothing under it is entered: one Label's new text enters the canvas, the
Label's ancestors and the Label, and a Window dragged over 10 000 Labels
enters the canvas and the Window (`tests/unit/layoutWalk.cpp`;
`layout_visited=` in `ControlTest --stats` and `gweni_bench` counts the
entries). A dirty parent does not even loop over its clean children when they
cannot have moved (see "Which children a pass enters"), so neither of those
looks at the other 10 000 Labels (`layout_children_scanned=` counts the
children the loops went over, entered or not).

A control stays dirty through all four steps, so a width the X arrange gives
it is measured in Y, and is cleared at the end of its Y arrange. It stays
dirty past that when it re-dirtied the pass, or when a shown child is still
dirty (a child moved in a `postLayout()`), so the next pass finds its way
down.

A pure move does not dirty the moved control's subtree: its children are
placed relative to it, and `onBoundsChanged` marks the subtree
`StateChange_Geometry` for the render walk. A new size invalidates the
subtree, which is laid out in it. The moved control's own `layout()` runs
again, since one may correct where its parent put it (a Menu kept inside the
canvas). A request (`setPos`, `setSize`, `setBounds`, `moveTo`) invalidates the
control, and so its parent, whether or not the bounds already match it.

Skipping a clean control is the same as running it only if its cached result
is still the one it would compute, so **a `calculateSize()` may read only**:

* the control's own state, every change of which invalidates it (its text,
  font, padding, size flags, requested size, the children it has);
* its children's preferred sizes, and whether they are hidden, which their
  own invalidation reaches it through;
* its parent's padding, dock or requested size, and only in a type whose
  `readsParentLayout()` says so: a change to those invalidates such children
  as well (`Base::invalidateParentReaders`). A wrapped `Text` (its parent's
  requested inner width) and a `TabButton` (its strip's dock) do;
* the canvas's size, which invalidates the whole tree when it changes, as a
  scale change and a `textLayoutSerial()` bump do.

It never reads its own current bounds or a sibling's. `arrange()` may read in
addition the bounds its parent has just given it, which enter it again when
they change. Anything else (an anchor elsewhere in the tree, as a Popup's
placement reads) needs an invalidation of its own (`Canvas::updatePopup`).

`GWENI_LAYOUT_VERIFY=1`, or ControlTest's `--verify-layout`, checks this:
once a frame's layout has converged, the canvas runs one more pass that enters
every shown control (`Canvas::verifyingLayout()`) and fails, naming the
control, the axis, the field and both values, on any bounds or preferred size
it changes. The `controltest_verify_layout_*` tests run every control and the
scenarios that change layout at run time with it.

### Which children a pass enters

Before #430 a dirty parent looped over all its children in every step:
`sizeOfChildren` measured them, `arrangeHorizontal`/`arrangeVertical` placed
them, `arrange` entered them and `doArrange` looked for a dirty one. Only
the dirty ones were entered, but the loops cost the parent's child count, so
one Label changing among 10 000 cost 10 000 placements. Now a parent goes
over only the children that may have changed, whenever the others cannot
have moved, and the bounds are exactly those of the full walk.

**The list.** A control of `Base::MinChangedWalk` (16) children or more keeps
its changed children (`m_changedChildren`): every child that became dirty,
moved, or had an input of its size term changed, since the control last
laid them out. `Base::markLayoutDirty` puts a child there when it goes from
clean to dirty, so a shown dirty child is always in it; the Y arrange's
dirty-child check (`Base::doArrange`) looks only at the list, keeps the
children still dirty and lets the others go. When the list reaches one in
`Base::ChangedShare` (16) of the children it stops (`m_changedOverflow`): the
walks then take every child, as before, and the next Y arrange looks at them
all and starts the list again. So a first frame, a resize, a scale change or
a whole page shown runs the full walk, and a pass that changes a few
children among many runs the short one. Below 16 children there is no list
and every walk is the full one.

**Placing (`Base::arrange`).** A docked child is placed in what the docked
children before it left of the parent, and a Center child in what they all
left, so a docked child that changes moves the children after it. A
dock-None child (free at its requested position, or aligned) is placed from
its own inputs (preferred size, margin, alignment, size flags, request) and
the parent's padded content box alone (`Base::placeDockNoneChild`, the same
code `arrangeHorizontal`/`arrangeVertical` run for it). So when

* no child in the list is docked,
* the parent's size along the axis and its padding did not change, and
* no docked child was removed, reordered, or had its dock or placement
  changed (`m_arrangeAll`, per axis),

every child outside the list is where the full walk would put it again, and
a clean one would not be entered: the parent places and enters the listed
children alone. Anything else places them all.

**Measuring (`Base::sizeOfChildren`).** A child adds a *term* to its parent's
content size (`Base::computeSizeTerm`, the full count's branches): a
dock-None child its preferred size, margins and requested position, which
counts towards the largest term, as Top/Bottom children do in X and
Left/Right in Y; the other docks towards the docks' sum; Center children
towards their own sum. A full count keeps the largest term, how many children
hold it, and the sums (`ChildSizes`). A pass with few changed children
measures only those and counts each again: its old term out, its new one in.
The old term is the one the count holds, which is the one the child's inputs
give until one of them changes, so a child keeps it only from then: the
setters of every input (margin, dock, visibility, placement,
`doNotIncludeInSize`, the requested position, `setPreferredSize`) call
`noteSizeInputsChanging()` before they change it. A child measured after its
parent counted it (`s_measureSeq` against the parent's count) may hold
another preferred size than the counted one, and taking the largest term from
its last holder leaves the largest of the rest unknown: either makes the next
count a full one. The content is the largest term, or the sums if larger,
with padding, as before.

A wrapped `Text` measures its lines the same way: with the rows as high as
they were made, only the changed lines are measured and given the row
height.

**The check.** Layout verification's full pass (`Canvas::verifyingLayout()`)
always takes the full walk, entering and placing every child, so a shortcut
that put any control elsewhere than the full walk does fails it. The
`controltest_verify_layout_*` tests run it over every demo and the scenarios
that change layout, and `tests/unit/layoutChangedChildren.cpp` checks each
case against it and against a tree built in the changed state. That full
pass also counts every parent's terms afresh and empties the lists, so with
verification on every frame starts from a full walk's state, and a mistake
in what the shortcut carries from frame to frame (the kept terms, the list,
`m_arrangeAll`) would only show in a frame after one without it. The last
case of `layoutChangedChildren.cpp` runs many frames of mixed edits (moves,
sizes, visibility, margins, deletes and adds, reorders, docks, alignment,
the parent's padding) with verification off and compares every frame with
a tree built fresh in that state.

**What it costs.** One Label changing among N (`gweni_bench --scene
set-text`) and a Window dragged over N Labels (`drag`) now lay out in the
same instructions at N=1000 and N=10000 (`tools/profile.sh`: set-text's
update phase 22.2 M and 158.8 M before, 7.32 M at both; drag's 15.3 M and
151.9 M before, 0.115 M at both). A pass that walks everything pays a few
instructions per control for the list (marking dirty, the measure stamp, the
decision in each arrange): 8 % of `resize --n 1000`'s and `scale --n 1000`'s
update-phase instructions, 1.4 % of `build --n 10000`'s, within the noise of
their timed runs.

**Its counter and the flagged subtrees.** `layout_children_scanned`
(`LayoutStats::childrenScanned`) counts what these loops go over, as
`layout_visited` counts the entries, and like it leaves out what a
`Base::setExcludedFromStats()` subtree (the inspector) costs: the loops of a
control whose entry is not counted (`layoutEntryCounted()`) count nothing,
and a flagged child is not one of the children its app parent's loops count
(`Base::countLayoutChildrenScanned`, `tests/unit/statsExclusion.cpp`). It is
still one of the parent's children for the walks' choices (`MinChangedWalk`,
the one in `ChangedShare`): a parent of 15 app children keeps a list once a
flagged panel is its 16th, and walks them all without one.

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
nothing invalidated or moved the control since the last pass. If the control is
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
* **Columns sized from their content, in the same pass.** A `layout::Table`
  sizes its auto columns in `layout()`, at the start of its X arrange, from
  its cells' preferred widths, which `calculateSize(X)` has just measured:
  each gets its widest cell and an even share of what is left, or, when the
  content is wider than the table, a share in proportion to it, and the
  integer remainder goes to the lowest-index ones, so the columns add up to
  the table's width (`Table::splitColumns`). Its rows place their column
  Labels at those widths in their own `arrangeHorizontal`, without writing
  them into the Labels' preferred sizes, so a Label's preferred width stays
  its text's and a column shrinks back when its widest cell does. A row
  handed a new width marks itself for the pass as placement (the Table
  arranges it next), so a cell's new text re-places the clean rows in that
  pass too (`tests/unit/tableColumns.cpp`).
* **Children the generic arrange places are handed their inputs, not their
  bounds.** The docks, margins, padding and visibility a control derives for its
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
  parent gives it (`Text::policyResize`). It measures that string once and
  keeps the size (`Text::measuredString`), as it keeps its font's line height
  and, wrapped, its longest word, until `setText` gives it a new string or
  the key changes: the font (the same object holding the same value: a Font
  can change size in place), the renderer, and the renderer's
  `textLayoutSerial()`, which a new text AA mode raises
  (`Text::measureKeyCurrent`; `setFont`, `setWrap` and `textChanged()` drop
  them all). The scale is not in the key: `measureText()` and `lineHeight()`
  are in canvas units at every scale, so a `Canvas::setScale` or a resize lays
  everything out again and measures nothing. So however many times a pass
  asks (`calculateSize` in X and Y, a ScrollControl measuring its children
  again), a Text costs one `measureText()` per change of its inputs
  (`tests/unit/textMeasure.cpp`); substring measures (caret places, wrap
  trials) are made each time. Wrapped, it asks for the width the
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
  known, and the helpers read the string unwrapped.
  An edit is wrapped again by the paragraph (`Text::wrapEdit`): `setText`
  finds the bytes that changed by comparing the old and new string from both
  ends (no measuring, no allocation; two edits with no wrap between are
  merged), and the next wrap re-wraps only the paragraphs they touch, from the
  start of the one holding the first changed byte to the first `'\n'` after
  the last, with the same code and measures as a whole-string wrap (a line
  restarts at each `'\n'` and no word crosses one), so it gives the same lines.
  Those lines are spliced in place of the old ones; the others hold the same
  characters and are kept. The X arrange then syncs only those lines'
  children (`Text::syncLines`): the edited lines' children are reused in
  order, the surplus deleted or the shortfall made there, so a line below the
  edit keeps its own child, and is moved a row only when the number of lines
  above it changed; a child already showing its string on its row is not
  touched. So a keystroke costs its paragraph, not the document
  (`tests/unit/textWrapEdits.cpp`, `tests/textWrap.cpp`), except that rows below
  an edit that changed its paragraph's line count all move (their glyphs are
  rewritten), and a string with no `'\n'` is one paragraph, wrapped again whole.
  A first wrap, a new width, font or wrap flag wraps the whole string, and the
  lines there are reused by index: each gets its new string and row, which
  writes nothing for a line that did not change, and only the extra lines are
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

An invalidation raised during a pass is not lost: the control it was raised
on stays dirty past its Y arrange (`Base::noteRedirty`), and a pass that
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
content's (inner panel plus its margin and the control's padding), not
counting the bars or the scroll position, so it does not depend on what an
earlier frame showed. The content is seen through the viewport
(`ScrollControl::getViewport()`): the control less its padding, the bars that
show and the inner panel's margin. The bars are decided against it, the inner
panel is placed at its origin less the scroll offset and is at least its size,
and the bars range over the inner panel through it, so at its preferred size a
ScrollControl shows its content exactly in the viewport, the same inset on
every side.

The inputs such a control writes into children it has not laid out yet count
as placement rather than as a re-dirty: their bounds through `placeChild` (the
bars and the inner panel are Owner), the rest (visibility, scroll range) under
a `Base::PlacementScope`. Keep the scope to those writes: anything a control
already laid out has read must still be reported.
