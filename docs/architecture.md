# gweni architecture

## Goal

gweni is a retained-mode GUI whose **UI logic is fully decoupled from the rendering
API**. Layout, padding, docking, hover/highlight/focus state, hit-testing and the
control tree all live in the API-agnostic core (`gweni::controls`, `gweni::skin`,
`gweni::event`). Drawing is delegated to a pluggable renderer that implements one
abstract interface, so the same UI runs on OpenGL today and Vulkan (or any other
backend) later with no changes to control or skin code.

The design target is that the per-frame cost scales with **what changed**, not with
the size of the UI: a static UI that isn't being interacted with should cost almost
nothing to present.

## The two-phase frame

Once the canvas and a skin/renderer are initialized, the host app loop calls two
methods per frame (see `examples/openglcore/openglcore.cpp`):

```cpp
canvas->updateCanvas();   // 1. logic: layout + state tracking (no draw calls)
canvas->renderCanvas();   // 2. render: issue only the draw-state changes needed
```

### 1. `updateCanvas()` — logic / layout

`Canvas::updateCanvas()` runs `Canvas::doThink()`, then, if the tree order changed,
`renumberZRecursive()` over the tree (`source/controls/canvas.cpp`,
`source/controls/base.cpp`). This phase:

- processes input-driven state (hover/focus/press), animations and delayed deletes,
- runs layout/docking so every control has up-to-date bounds,
- assigns each control a draw order (`m_zIndex`),
- records *what changed* on each control as a **state-change bitmask**
  (`StateChange_Created | _Visibility | _Geometry | _Background | _Text`, in
  `include/gweni/controls/base.h`). A control with `StateChange_Nothing` needs no
  render work.

No rendering-API calls happen here.

Like the render walk, it enters only what changed. There is no per-control `think()`:
`Canvas::doThink()` runs the delayed deletes, `anim::think(this)` (the animations), the
layout passes and `input::onCanvasThink`. A keyboard focus that is hidden or takes no keyboard
input is unfocused (`input::dropLostKeyboardFocus`, through `Base::unfocus()`) before the
passes, so what its `onLostKeyboardFocus()` changes (a `DragNumber` ending its edit) is laid
out in them, and again after them, for a holder the layout hid, laying out once more if that
dirtied layout. A frame nothing invalidated runs no pass, and a
pass enters only the dirty paths: the controls invalidated or given new bounds, and the
way down to them. Every walk over children goes through `Base::doCalculateSize`/
`doArrange`, which skip a clean control, keeping its preferred size and bounds. A parent
of 16 children or more keeps the ones that changed since it last laid them out, and while
they are few and none is docked, it measures, places and enters those alone: the rest
cannot have moved (`docs/layout.md` → "Which children a pass enters", issue #430). A pure
move does not dirty the moved control's subtree (`docs/layout.md` → "Which controls a
pass enters", which also says what a `calculateSize()` may read for its cached result to
stay valid; issue #321).
`LayoutStats::visited` (`layout_visited=` in `ControlTest --stats` and `gweni_bench`)
counts the controls the frame's passes entered, one per `calculateSize()` and one per
`arrange()` in each axis, so four per dirty control and 0 on a settled frame;
`LayoutStats::childrenScanned` (`layout_children_scanned=`) the children the generic
layout loops (`Base::sizeOfChildren`, `arrangeHorizontal`/`arrangeVertical`, `arrange`,
`doArrange`'s dirty-child check, `LayoutItem`'s, a wrapped `Text`'s lines) went over,
entered or not;
`LayoutStats::measureCalls` (`measure_calls=`) counts the renderer `measureText()` and
`lineHeight()` calls the controls made, every one through `Base::measureText()` or
`Base::measureLineHeight()` (`tests/unit/layoutVisited.cpp`, `tests/unit/layoutWalk.cpp`).
An unwrapped `Text` with a string measures it once and keeps the size, its height
being that line box; only an empty one asks for `lineHeight(font)`, also kept
(`tests/unit/textLineHeight.cpp`). A `Text` measures again only when its string, its
font (pointer or value), its renderer or the renderer's `textLayoutSerial()` changed,
not for another `calculateSize()` and not for a new scale, `measureText()` being in
canvas units, so a resize or `Canvas::setScale` relayout measures nothing
(`docs/layout.md` → "Text", `tests/unit/textMeasure.cpp`). A wrapped `Text` wraps an edit again by the
paragraph, from the bytes `setText` found changed, and syncs only the line children of
the lines that changed, so typing into a long `TextBoxMultiline` measures and allocates
for its paragraph, not the document (`docs/layout.md` → "Text"; a string with no `'\n'`
is one paragraph). Those measures are made where the edit is, mostly in the input
phase (`TextBox::insertText` asks for the caret's place), so `gweni_bench` splits
`measure_calls` by phase (`measure_calls_input`, `_update`, `_render`), and
`ControlTest --stats`'s `measure_calls=` counts everything since the last frame line;
`LayoutStats::measureCalls` holds the update phase's and a render-phase layout's.
An invalidation (`Base::invalidate()`) marks the control and its logical parents up to
the canvas; a new scale invalidates a whole subtree in one walk that enters each
control once, with no walk up from each, and takes the way out of the subtree once
(`docs/layout.md` → "Which children a pass enters", **Invalidating a whole subtree**;
issue #519). A new size invalidates only the children the control holds (its inner
panel's included), and below them each control whose bounds its arrange changes
invalidates its own children in turn, so the canvas's resize invalidates the canvas's
children alone; inside a layout pass neither that nor the resized control walks up to
the canvas, since every ancestor is being arranged (**A new size invalidates the
children**, **No way up in a pass**; issue #578). `controls::invalidateCalls()` counts the calls (`invalidations=` in
`ControlTest --stats`, `invalidations` and its `_input`/`_update`/`_render` shares in
`gweni_bench`), none from a `Base::setExcludedFromStats()` subtree or a control with no
parent that is not a canvas (one being made).
`GWENI_LAYOUT_VERIFY=1` (ControlTest `--verify-layout`)
checks every frame's layout against a pass that enters every control.
The animation registry (`source/anim.cpp`) holds only
animations with work to do: a `TextBox`'s caret blink from the focus arriving until it
leaves, a `ProgressBar`'s while it cycles or is indeterminate at a non-zero speed, and a
timed animation (`anim_widthIn`, `anim::pos::X`, `anim::tools::Remove`, ...) until it
finishes. `anim::think(canvas)` runs, in the order they were added, those whose control
is on that canvas (`getCanvas()`, looked up when it runs, so a control moved to another
canvas animates there and one on no canvas, or on a hidden canvas, does not run), so
with several canvases each animation runs once a frame, in its own canvas's update
phase before that canvas's layout. An animation may add or cancel animations, or delete
controls, from inside its `think()`: removal waits until the loop ends.
`anim::cancel(control, animation)` removes one animation and leaves the control's
others. `UpdateStats::animsRun` (`anims_run=` in `ControlTest --stats` and
`gweni_bench`) counts the animations a canvas's update ran: 0 on a settled frame with no
focused TextBox and no moving bar (Demo's 1 is LogViewDemo's stream, which stays
registered on its hidden page and waits there; a hidden moving bar has none).
`tests/unit/animations.cpp` pins it. The z indices are numbered depth-first, each control taking its
skin Control's `depthLayers()`, and that numbering only changes with the tree order, so
the canvas renumbers only on a frame after the tree order changed
(`Canvas::zOrderDirty()`). `Base` reports each order change itself, for the control it
happens to: `addChild`/`addChildOfLayout`/`removeChild` (so a reparent through
`setParent` and a delete, delayed or not, too), `bringToFront`,
`sendToBack`, `bringNextToControl`, and a skin Control assigned or released (`setSkin`,
whose new Control may report other `depthLayers()`). A control off any canvas reports
nothing; attaching it is an `addChild` on the canvas's tree. A renumber enters only the
part of the tree the change can relabel and relabels only what it put out of order, and
marks `StateChange_Geometry` on exactly the controls whose z index changed (see "Gapped
z labels" and "Which controls a renumber enters" below).
`Canvas::renumberZOrder()` runs it at the end of `updateCanvas()` and again in
`renderCanvas()` right after `renderPhaseLayout()`, so a control that pass created (a
wrapped `Text`'s lines) is numbered before the walk draws it; it is not left at the
constructor's z 0, which is the depth-clear value and draws nothing. When the order did
not change both calls return at once.
`UpdateStats::zVisited` (`Canvas::getUpdateStats()`, `ControlTest --stats` prints it as
`z_visited=`) counts the controls it entered: Demo's first frame 2668, a settled one 0,
`gweni_bench --scene add-remove` 3 a frame at any N (the canvas, the new Label and its
text).
`UpdateStats::zChanged` (`z_changed=` in `gweni_bench`) counts those whose z or overlay
z it changed: the controls added or moved and the few after them up to a free gap.
`tests/unit/updateWalk.cpp`, `tests/unit/renderPhaseZ.cpp` and `tests/unit/zWalk.cpp`
pin them.

**Gapped z labels.** The numbering keeps tree order as draw order, but it hands z out
with gaps, so an order change rewrites the slots of what it moved and not of every
control after it (`Base::renumberZRecursive`, `Canvas::renumberZOrder()`). A cursor walks
the tree depth-first; each range (a control's `depthLayers()`, then its subtree, then
its `overlayLayers()`) keeps its z if that is still at or past the cursor, and otherwise
takes the cursor, which then moves past the range. So:
- the canvas's first numbering, and a compaction, give every range the cursor and leave
  `Canvas::ZGap` (7) free after it: stride 8 for a one-layer control;
- a control added between two numbered ones takes the next free z, packed; the ones
  after it keep theirs as soon as the cascade reaches a gap, so adding k controls in the
  middle changes about k z indices however large the tree (`tests/unit/zRelabel.cpp`);
- a removal leaves a gap and changes none;
- a moved subtree (`bringToFront`, `sendToBack`, `bringNextToControl`, a reparent through
  `addChild`, `keepOverlaysLast()` moving the popup or the tooltip box) gives up its
  labels (`Base::zOrderMoved`, a flag on its root) and takes new ones where it now is,
  since keeping them would put it out of order with what it passed;
- past the highest z the last numbering used no kept label can follow, so a range taken
  there leaves a gap at no cost but range: `ZGap` for a control numbered for the first
  time, `Canvas::MovedZGap` (1) for a moved one, which takes new z at the top of the
  range on every move (a Window brought to the front);
- a `depthLayers()` change (a skin swap) is absorbed by the cascade like an insert.

The labels therefore depend on the edits that led to a tree, and two trees that look
the same can draw at other z values in the same order; the determinism test still holds,
since the same edits give the same labels. ControlTest's mirror comparisons rank each
z among the drawn ones for that reason. When a pass runs past the renderer's
`maxZIndex()` (65535 in `OpenGLCore`, `renderer::Base::maxZIndex()`), one full pass
compacts the tree with the largest gap up to `ZGap` that spreads it over at most half
the range, leaving the other half for what is added or moved to the end before the
next compaction, packed when the tree barely fits, and clamped (with ties, one warning
on stderr) when even packed it does not. That pass costs a whole-tree rewrite once:
`gweni_bench --scene bring-to-front --n 2000` (4120 controls, a 515-control Window
moved to the end per frame) compacts once in about 32 frames.

**Which controls a renumber enters.** A walk that kept every label of a subtree would
leave the cursor at that subtree's end and change nothing in it, so the walk does not
enter such a subtree (`ZNumbering::skipClean` in `Base::renumberZRecursive`). Each
control caches that end, `m_zEnd` (the end of its last range: its overlay range, else
its last child's end, else its own range), set as the walk leaves it. A control whose
child list changed (an add, a removal, a move among its siblings, `keepOverlaysLast()`)
or whose skin Control was assigned or released marks itself and its `m_actualParent`
chain `m_zDirty` (`Base::zOrderChanged`), and a new control starts dirty. The walk then passes a child in O(1),
jumping the cursor to its `m_zEnd`, when the child is clean, not under a moved control
and its z is still at or past the cursor; it enters a dirty one, a moved subtree whole,
and a clean one the cascade from an insert reaches into (its z behind the cursor),
which is relabelled. So an insert enters the path to it, itself and its cascade
(`tests/unit/zWalk.cpp`: no more than depth + 1 + `zChanged`, the same count in a tree
of 61 controls as in one of 2401), and a move the path and the moved subtree. A
removal walks nothing: every label left is still in order, so `Base::zChildRemoved`
only moves the `m_zEnd` of the parent back, and of each ancestor whose last range it
was, and the canvas takes its new highest z from its own end. Three cases still enter
every control: the first numbering and a compaction (full passes), the pass after
`Canvas::invalidateZOrder()` (a skin Control's `depthLayers()` or `overlayLayers()`
changed on its own, in a control the canvas cannot name), and the pass after a
clamped one, whose ties are out of order. The walk still scans every child of each
control it enters, as the render walk does (#407): `add-remove` enters 3 controls
but passes each of the canvas's N children. `tests/unit/zWalk.cpp` runs 300 random
edits on two identical trees, one renumbered with `Canvas::invalidateZOrder()` before
each pass, and checks that every label is the same in both.

#### Hidden and visible

A control's `hidden()` is its own flag, what `setHidden`/`hide`/`show` last set on it,
and `visible()` is whether it is drawn: neither it nor any control on its
`m_actualParent` chain (inner panels included) hidden. Each control caches the second
half (`m_ancestorHidden`), so `visible()` is one test. Hiding or showing a control never
writes its descendants' own flags, so the children a control hid for its own reasons (a
`ComboBox`'s closed list, a `TabControl`'s other pages, a collapsed `TreeNode` or
`CollapsibleCategory`, a `ScrollControl`'s unneeded bar, a `setClosable(false)` close
button, `VirtualList`'s spare rows) stay hidden when a hidden page or window holding them
is shown. `setHidden` marks `StateChange_Visibility` on each control whose `visible()`
changed and stops at a descendant that hid itself, so showing a page with a collapsed
10 000-node tree enters none of the tree; under a hidden ancestor it changes the own
flag and marks nothing. Attaching a child (`addChild`, `addChildOfLayout`, so `setParent`
and `newChild` too) sets its cached flag from the new parent's `visible()`, and detaching
it clears it, so a child moved out of a hidden parent into a shown one is drawn.

Only the shown control is invalidated. The layout pass skips a hidden control's subtree
(`hidden()` children in `calculateSize`/`arrange`), and an `invalidate()` raised inside it
meanwhile stays pending on that control (`m_needsLayout`) and on the canvas, so the pass
of the frame it is shown lays it out. A `setHidden` override (`WindowControl`'s
`bringToFront`, `Menu`'s `onVisibilityChanged`) runs only for that control's own hide or
show; code that must follow an ancestor's, as the context menu, the popup and the tooltip
do, reads its owner's `visible()` in the update phase. `tests/unit/hiddenState.cpp` pins
it.

#### Hover and mouse capture

`HoveredControl` is the control under the mouse, kept by `input::onMouseMoved`
(`UpdateHoveredControl`, `source/inputHandler.cpp`): a move hit-tests the canvas
(`getControlAt`) and gives the old control `onMouseLeave` and the new one
`onMouseEnter`, each of which fires `onHoverLeave`/`onHoverEnter` and `redraw()`s that
control. **While a control on the canvas holds the mouse (`MouseFocus`, taken by a
press: Button, Dragger and its subclasses, the scroll bar tracks, TextBox selection, the
colour controls), it is the hovered control**: a move hit-tests nothing and sends no
leave or enter, so a drag repaints only what the captured control changes itself (a
slider thumb drag changes the thumb alone each frame) and the controls the cursor crosses
fire no hover events. A capture taken while another control is hovered gives that one a
single leave. When the capture ends, hover is hit-tested once: after a click whose
delivery cleared `MouseFocus` (`onMouseClicked`), and when `onCanvasThink` drops it
because its holder is hidden; the old holder is repainted unhovered in that frame. Since
the holder is hovered wherever the mouse is, `isHovered()` cannot tell a release on a
control from one off it: `Button` tests the release point against its own bounds
(`containsCanvasPoint`), so a press let go off the button is no click.
`tests/unit/mouseCapture.cpp` pins it.

The hit test itself is `Base::getControlAt`: the control's own hidden and bounds
checks, then every child in reverse child order (the last one holding the point wins),
then the control itself. That is one call per child of every control under the point,
and each call is one hit-test visit, counted by the running `input::hitTestVisits()`
(never reset; `gweni_bench` prints it per frame as `hit_tests=`, and `bench_scaling`
holds it flat in the local scenes). A long list would make a move cost one visit per
row, so `layout::Table` (a ListBox's rows) overrides it: while its children are all
docked Top and the shown, non-empty ones lie in child order by y without overlapping
the next, at most one row holds the point, and a binary search over a vector of those
rows finds it, so a move over a ListBox costs the same few dozen visits at 1 000 rows
as at 10 000. Anything else (before the first layout, a row moved over another, a child
not docked Top) walks every child as `Base` does. The vector is rebuilt on the first
hit test after a child is added, removed, moved, resized (`onChildBoundsChanged`),
hidden or shown (`onChildHiddenChanged`, which `Base::setHidden` calls on the parent),
so the answer is the walk's for the bounds as they are, whether the table is laid out
or not. `tests/unit/tableHitTest.cpp` compares the two at every point over a table in
each of those states.

#### The tooltip: a canvas-level overlay

A tooltip is a retained control like any other, decided in this phase. Each canvas owns
at most one `controls::ToolTip` box (`include/gweni/controls/toolTip.h`, a padded
`Label` painting the theme's `Tooltip` surface), created the first time a tooltip is
shown and reused for every control after that. `Base::setToolTipText` only stores the
text, and `setToolTip(control)` keeps that control as a hidden child of its owner, so
neither creates anything. `Canvas::updateToolTip`, run by `doThink()` before the layout
passes, compares the control whose tooltip the hovered control shows (its own, else its
parent's) with the one it has: a new one starts the delay (`Canvas::setToolTipDelay`,
0.5 s of `platform::getTimeInSeconds()`, so the stepped clock in captures), and once
the delay has passed the box is filled in (the text, or the owner's own control moved
into the box), sized from the measured text and placed by `ToolTip::placement`: below
the owner's bounds with the left edges aligned, above them when there is no room below,
pulled inside the canvas's right and left edges. It is anchored to the control and not
to the mouse, so moving the mouse inside the control writes nothing. Moving straight to
another control with a tooltip refills the same box at once. Leaving, a press (until the
mouse is on another control; `Canvas::inputMouseButton` takes the control under the mouse
at the press, so a move and a press in one frame count), the cursor leaving the window
(`input::GLFW3::processCursorEnter` → `Canvas::inputMouseLeave` → `input::onMouseLeft`,
which stops hovering unless a drag holds the mouse focus), and the owner being hidden,
disabled, deleted or losing its tooltip hide it with `setHidden`, so its slots are hidden,
not freed. The box is
placed before layout, so it is laid out in the frame's one pass; it is placed from the
owner's bounds of the frame before, so when the owner moves (a scroll, a re-layout) the
box follows one frame later. On a frame where
nothing about it changes the step walks up from the hovered control to its canvas and
marks no control.

**Its z: the canvas's last child.** The box has to draw above every control, an open
`Menu`, a dialog on the modal stack and a toast included, and be clipped to the canvas alone. It is a child of the
canvas, so its clip region is the canvas's, and it is kept as the canvas's **last**
child: `Canvas::keepOverlaysLast()` moves it back to the end of the child list just
before each z renumber. Every change of the child order (a `Menu` added or opened with
`bringToFront`, a modal pushed, any `addChild`) asks for a renumber anyway, so this costs
nothing on other frames, and the depth-first numbering then gives the box and its text
the highest z values in the tree. A reserved z range above the tree was the other
choice; it would need a ceiling that no tree reaches and a second numbering for the
overlays, while the last-child rule reuses the one numbering there is. The popup (below)
takes the place just before it, and the controls registered with `Canvas::addOverlay` (the
inspector's highlight box) go between the popup's Menus and the box, in the order they were
added. GWEN's drag-and-drop
preview, which re-rendered the dragged control after the walk at the mouse offset, was
deleted rather than moved here: the only drag that set it hid that control first, so it
never drew anything.

**A tool's tooltip is the tool's work.** While the box shows the tooltip of a control in
a `setExcludedFromStats()` subtree (a stats overlay's name cell), the box is flagged
too, from before the show changes it, through its hide, to the next show for a control
that is not, so a tool's tooltip counts in none of the frame's counters and an app's
counts as always; `Canvas::toolTipShownForId()` names the control the box last showed a
tooltip for, for the tool to take the box's slots out ("The stats overlay" → "Only its
names take the mouse").

#### The popup: a panel next to an anchor, one per canvas

`controls::Popup` (`include/gweni/controls/popup.h`) is imgui's `BeginPopup`: a panel of
controls (its children, docked or placed like any control's) that the canvas shows over
everything else next to an **anchor** control, a colour picker under its swatch or a list
of tabs under its button. `popup->open(anchor, PopupPlacement::Below|Right)` hands it to
`Canvas::showPopup`, which closes the popup shown before, moves it onto the canvas if it
was made elsewhere, shows it, brings it to the front and fires `onOpen`. The canvas shows
**one popup at a time**, and its context menu (below) is the same slot:
`Canvas::shownPopup()` / `popupAnchor()` are whichever is open, `closePopup()` closes
either, `shownContextMenu()` is the shown popup when it is a context menu. A popup's
textured renderer paints one `tokyonight::Menu` surface, a context menu's panel.

**It closes** on `close()` or `setHidden(true)` (both through `closePopup`, which fires
`onClose`), on a press of either button that is not on the popup, on its anchor or on an
`Menu` opened over it, a ComboBox's list in it (`Canvas::overPopup`: a floating `Menu`
after the popup among the canvas's children; a menu bar, a `MenuStrip`, is outside the
popup like any control, and it never closes) (`Canvas::popupPressed`, called by `input::onMouseClicked` before the press is
delivered, which it still is), on Escape (`Canvas::escapePopup`: a Menu open over it, a
ComboBox's list in it, closes first), when the update phase finds its anchor hidden or
disabled (`Canvas::updatePopup`, the step that also forgets a context menu closed as
menus close), when the anchor is deleted (`Canvas::popupControlDeleted` from `~Base`, a
delayed delete included), and when it is deleted itself (no `onClose` then). A press on
the anchor is the anchor's, so a button toggles its popup (`isOpen() ? close() :
open(this)`). Closing a popup closes the Menus open over it. **Ownership: a popup has an
owner.** It is a child of the canvas (opening one made elsewhere moves it there), so
closing only hides it and the canvas deletes it with its other children; it is not a
child of the control that made it, and that control owns it instead, as
`createContextMenu()`'s control owns its menu. `Base::createPopup()` makes a popup on the
control's canvas owned by the control (null with no canvas), and `Popup::setOwner(owner)`
makes any popup a control's (null: no owner's), `getOwner()` says whose. The owner keeps
one pointer, `Base::m_ownedPopups`, the head of a list threaded through the popups'
`m_nextOwned`, so the cost per control is that of `m_contextMenu`; `~Popup` takes a popup
off it. `~Base`, right after `dropContextMenu()`, empties the list
(`Base::dropOwnedPopups`) and goes by each popup's own canvas, not the owner's, so an
owner taken off the canvas (`setParent(nullptr)`) behaves the same: while that canvas is
deleting its children (`Canvas::releasingChildren()`) it does nothing, the canvas
deleting the popup, before or after the owner's window, whichever it came to first;
otherwise it closes the popup if the canvas shows it and `delayedDelete()`s it, delayed
because this can run from an event of one of the popup's own controls (an owner and its
popup in one delete batch, either first, are each deleted once: `PreDeleteCanvas` takes a
deleted control off the list); with no canvas it deletes it outright. So a control that
makes a popup has no cleanup of its own (ColorEdit, TabControl's tab list and `PopupDemo`,
which once `delayedDelete()`d their popups in their destructors: a canvas tearing down
had freed the popup first whenever a `bringToFront` had put the creator's window after it,
#490).

**It places itself, in its own arrange.** A popup is owner-placed
(`Placement::Owner`), so the canvas's arrange neither moves nor sizes it, and its own
`arrange(dim)` does both before its subtree is arranged: the size the canvas would give a
dock-None child (`policyResize`: its content, or its request by its size flags), then
the position `Popup::placement(anchorRect, size, canvas, side)` gives. `Below` puts it
`Popup::Gap` under the anchor with the left edges aligned, above it where it would run
past the bottom, with the right edges aligned where it would run past the right edge;
`Right` puts it beside the anchor with the top edges aligned, on its left, bottom edges
aligned, likewise; then `Menu::keepInside`. Each axis needs only that axis's size, final
when its arrange runs, and the anchor is arranged before it in the same pass (the popup
comes after the anchor's top-level ancestor among the canvas's children), so it is
right in the frame's one pass. **The canvas enters it in every pass the app runs while it
is open** (#574): `Canvas::enterFollowers`, at the top of each iteration of
`layoutUntilClean` (the update phase's passes and the render phase's), invalidates the
open popup and the open anchored menus (below) between passes, so not as a re-dirty. A
pass can move the anchor without entering it or the popup: a Top-docked sibling grown
moves the anchor's top-level panel, a pure move that dirties neither the panel nor
anything under it, and a popup decided on before the pass would sit at its old place for
a frame. Entering it costs each open follower's measure and arrange (the canvas is
already dirty) in each pass of a frame that lays out anyway; a frame with no pass pays
nothing. A pass the
canvas runs for `setExcludedFromStats()` subtrees alone (an Inspector's or a
StatsOverlay's refresh) enters none: those tools are placed after the app and hold no
anchor, so they move none, and entering a follower would make their pass the app's.
The test is the canvas's own `layoutEntryCounted()`, which only an invalidation from
outside every excluded subtree sets. Besides that, `updatePopup` lays it out again before
the pass when the anchor's canvas rect is not the one it was last placed by, for an
anchor that moves with no pass: a ZoomPanel's pan or zoom, which moves it through the
render transform. A zero-size anchor is a point: `Menu::placement(cursor, ...)`
is `Popup::placement` of the cursor with no gap. The position is computed, never asked
for, unlike `Menu::openAt`'s cursor: the canvas's generic arrange puts a dock-None child
at its requested position on every pass, which is why a popup is Owner rather than a
child placed in `layout()`.

**Its z.** `keepOverlaysLast` puts a shown popup after every other canvas child but the
Menus that were after it (a ComboBox list opened from inside it stays above it) and the
ToolTip box, which stays last: so the z renumber numbers the popup above the whole tree
and under a tooltip, and a control added to the canvas after it opened still goes under
it. Opening touches only the popup's subtree and the anchor (`controltest_popup`): a
popup made on the canvas moves nothing else in the child order, and one made inside a
page takes new z values when it is moved out, while the page's later siblings keep
theirs.

A popup holding a `ComboBox` shows it with its list closed: `Base::setHidden` sets the
popup's own flag only, and the list the box keeps as a child until it first opens keeps
its own (see "Hidden and visible").

#### The context menu: a right-click opens a Menu at the cursor

`Base::setContextMenu(menu)` gives a control a `Menu` that a right-button press on it,
or on a descendant with none of its own, opens (`Canvas::contextMenuOwnerFor` walks up
from the hovered control to the nearest one with a menu, and finds nothing for a
disabled or hidden one or for a part of an open menu). It is decided in input, not in a
think: `input::onMouseClicked` first closes the open menus, as for any press not on a
menu, then delivers the press to the control, and only then, for a right press, calls
`Canvas::showContextMenu`, which fires the owner's `onContextMenu` (so the application
can fill in or disable items), re-parents the menu to the canvas if it is elsewhere and
calls `Menu::openAt(cursor)`. A context menu is the canvas's **popup** (above): opening
one closes the popup shown, a Popup or another context menu, so a right-click while one
context menu is open closes it and opens the next, and the canvas keeps one open at a time
(`shownContextMenu()`, `contextMenuOwner()`, and `shownPopup()`/`popupAnchor()`). It closes
as every menu does (a press outside it, an item chosen), on Escape (`input::onKeyEvent`
takes the press before the focused control does, and only while a popup is open), when
the update phase finds its owner hidden or disabled (`Canvas::updatePopup`, one pointer
check in `doThink()`, before the tooltip step), when a Popup opens, and when the owner is
deleted.

**Placement waits for the size.** A menu that has been hidden has not been laid out, so
its size is not known when it opens. `openAt` only shows it, brings it to the front and
records the cursor; `Menu::layout` places it, in the layout pass of that frame, with the
pure `Menu::placement(cursor, size, canvas)`: at the cursor, on the cursor's left where
it would run past the right edge, above it where it would run past the bottom, then
`Menu::keepInside`. `layout()` runs at the start of the X arrange, with the height of the
pass before, and again in the Y arrange once a new height is set (see
`docs/layout.md`), so the second run puts it right, in the frame's one pass. Every other
menu the canvas places (a ComboBox list, and a position asked for with
`setPos`/`setBounds` after `openAt` or `openNextTo`) is only kept inside the canvas by
`keepInside`, never flipped. `Menu::open(pos)` opens at the mouse through `openAt`, so a popup opened
that way (the Properties colour selector) now flips up near the bottom instead of
running past it.

**A MenuItem's menu is placed from its item** (#517). `MenuItem::openMenu` opens a strip
item's menu below the item, 1 unit off it, and a submenu on its item's right, top to top,
through `Menu::openNextTo(anchor, side, gap)`, which records the anchor instead of a
point and makes the menu self-placed on the canvas (`Placement::Owner`, as a Popup is).
`Menu::arrange` places it with `Popup::placement` from the anchor's canvas rect, flipped
(above the item, on its left) and kept inside the canvas: in X in the X arrange and in both
axes in the Y arrange, since a ScrollControl, a `MenuStrip` or a `Menu` included, arranges
its items in X in its own Y arrange, so the item's x is final only then. The side is
carried down the chain (#573), as in Win32 and GTK: a menu is leftward
(`Menu::isLeftward()`) when its placement in this pass went left of where its side
starts it (`Popup::placement`'s `leftward`: a submenu on its item's left, a context menu
flipped to the cursor's left, a strip menu right-aligned with its item), and a submenu
whose item is in a leftward menu is placed as `PopupPlacement::Left`, on its item's left,
going right only where that would run past the canvas's left edge; it is worked out from
where the parent went in this pass, so a canvas grown wide enough brings the whole chain
back to the right in that pass, and nothing is kept from when the menu opened. A menu opened
before the first layout therefore opens where the item is laid out, and every opening at
the same place. While it is open the canvas follows it (`Canvas::anchoredMenus()`, the
open strip menu and its open submenus, in the order they opened, checked by
`Canvas::updateAnchoredMenus` after the popup step) as it follows a popup: every pass the
app runs enters it (`Canvas::enterFollowers`, above), so it is placed in that same pass,
after the item (it was brought to the front when it opened), wherever the pass moved the
item, a pure move of a top-level panel included; and the update phase invalidates it
when the anchor's canvas rect is not what it was last placed for, a move with no pass (a
ZoomPanel's pan or zoom). A submenu's item is in its parent menu, earlier in the list,
so a chain of submenus follows a moved strip in one frame. A frame with nothing laid
out looks at those few pointers and invalidates nothing. A menu whose anchor is no
longer `visible()` closes, with the submenus under it;
deleting the anchor closes it (`Canvas::menuAnchorDeleted`, from `~Base`); a closed menu,
or one placed some other way, leaves the list in the next update phase and a deleted one
at once (`~Menu`). A menu numbered before its strip (the strip brought to the front after
it opened) is placed from where the item was in that pass and put right on the next
frame, each frame in one pass.

**Its z** is the one every opened menu gets: `bringToFront` makes it the canvas's last
child but one, and the tooltip box stays last (`keepOverlaysLast`), so a tooltip draws
over an open context menu. Opening renumbers z, which marks the controls whose z changed;
the menus a demo creates come after its content, so those are hidden menus, which render
nothing, and the frame writes only the opened menu's subtree.

**Ownership.** A menu passed to `setContextMenu` stays its creator's: deleting the
control only closes it, one menu can be set on several controls, and deleting the menu
clears it from all of them (`Menu::~Menu` walks its `m_contextOwners`). A menu made by
`Base::createContextMenu()` (a hidden `Menu` on the canvas) belongs to the control: it is
deleted with it, or when another menu is set, through `delayedDelete`, since this can
run from one of the menu's own item events; a canvas tearing down deletes it with its
other children, and while it does (`Canvas::m_releasingChildren`) a deleted owner only
hides its open menu, because the submenus `closeAll()` would reach may already be gone.

#### Menu keyboard navigation

A `Menu` takes no keys unless the code opening it asks (#632). `openNextTo(anchor, side,
gap, focus)` and `openAt(cursor, focus)` take a `MenuFocus`: `None` (the default, every
menu as it was, so a `ComboBox`'s list, a context menu and a `MenuStrip`'s menus take no
focus and react to no key, and a list whose keys belong to a text field keeps them
there), `FirstItem` (the keyboard focus and the first enabled item highlighted: an open
from a key), or `Menu` (the focus and nothing highlighted until a key or the mouse moves
it: an open from a click, WinUI's pointer open). `Menu::startKeyboardNavigation()` does
the same for a menu already shown. While it navigates the menu turns its own keyboard
input on (`input::dropLostKeyboardFocus` drops a focus that takes none) and restores it
when it ends.

The keys are WinUI's and Win32's: Up and Down move the highlight over the shown, enabled
`MenuItem`s (`Menu::navigableItems()`: dividers, disabled and hidden items are skipped),
wrapping, from none to the first or the last; Home and End go to the first and the last;
Return and Space choose the highlighted item as a click does (`doAction()`: a checkable
item toggles, the canvas closes every menu, then `onMenuItemSelected` fires), or, for an
item with a submenu, open it as Right does, and do nothing for an item disabled or hidden
since it was highlighted; Right opens the highlighted item's submenu
navigating in turn (`MenuFocus::FirstItem`), the item staying highlighted in its parent
(its submenu is open); Left closes a submenu back to its item, and does nothing in the
first menu; Escape closes one level; Tab is taken, so the focus stays in the menu. A
key's release goes where its press went, so the Space that opened a menu from a button
does not choose in it. A move closes a submenu the mouse had opened on the item it
leaves. A submenu the mouse (or a click on its item) opens under a navigating menu
navigates too, with nothing highlighted (`MenuItem::openMenu` passes `MenuFocus::Menu`),
so it takes the focus from its parent menu: a press on one of its items keeps the focus
in the chain that gives it back, and after a press let go off its item its keys still
work, Escape closing one level at a time.

**One highlight.** The keyboard highlight is the hover's look (`MenuHover`, no new
surface). `MenuItem::isHighlighted()` is what the textured `MenuItem` renderer reads:
pressed, its submenu open, or hovered, except that while its menu navigates by keyboard
it is the menu's highlighted item (`Menu::getHighlightedItem()`, held as a control id, so
a deleted item is none) instead of the hovered one, and `MenuItem::updateColors` gives
that item the hover's text colour. The mouse moving onto an enabled item highlights it
(`Menu::onHoverItem`), so mouse and keys move one highlight; a disabled item takes none.
`setHighlightedItem` redraws the item it leaves and the one it reaches and nothing else,
so a highlight move writes the two items' slots. Started and ended, the item under a
resting mouse is redrawn, its look moving between the hover and the highlight.

**The focus goes back.** At the start the menu keeps the control that had the keyboard
focus (by id). When the menu is hidden, however it closes (Escape, a pick, a press
elsewhere, `Canvas::closeMenus`, its anchor hidden), navigation ends and, if the menu
still holds the focus, it goes back to that control. A submenu took it from its parent
menu, and both may close in one `closeMenus()` in either order, so a hidden menu passes
it on to the control it took it from, and a pick in a submenu gives the focus back to
the button that opened the first menu. A chosen item closes the menus before its
`onMenuItemSelected` runs, so the focus is back on the opener by then, and a dialog the
handler opens records the opener as the control to give it back to, not a menu hidden
by the time the dialog closes. A control that is gone, hidden or refused by a
modal gets nothing and the focus is dropped. Not yet: a highlight moved past the
viewport of a menu that scrolls does not scroll it into view.

#### The split and drop-down buttons

`controls::DropDownButton` is a `Button` whose press opens `getMenu()`, a `Menu` it makes
on the canvas on first use (hidden; held by id, so one the app deletes is none) and the
app fills: `Menu::openNextTo(menuAnchor(), Below, 1, focus)`, so the canvas places it
below the button, flips it above and keeps it inside the canvas, and follows the button
while it is open as every anchored menu does. A click opens it with `MenuFocus::Menu`,
Space, Return, Alt+Down and F4 (`keys::F4`, `Base::onKeyF4`, from `GLFW_KEY_F4`) with
`MenuFocus::FirstItem`; either way the menu gives the focus back to the button. A press
on the button while its menu is open closes it: the button is a menu component
(`isMenuComponent()`, as a `MenuStrip` item is), so `input::onMouseClicked` does not
close every menu before the press, and its own press decides (`openMenu` closes the
other open menus first). While the menu is open the button is pressed (`isDepressed()`),
redrawn from the menu's `onVisibilityChanged`. Disabling it closes the menu and, as
`Switch::setDisabled` does, turns its keyboard input and Tab stop off and drops the
focus, so a click on a disabled one focuses nothing and the menu's hand-back skips it
(enabling it gives them back; the arrow part of a SplitButton never takes either). A press on
the open button let go off it presses nothing, so the menu stays open with the focus
on the button; there Escape, Space, Return, Alt+Down and F4 close it. Its textured
renderer draws
`buttonSurface()` (the Button surface of a state, shared with every Button's renderer)
and the down chevron (icon kind 1, direction 0) at z+1, `ChevronSize` square
`ChevronInset` from its right edge, in the right text padding's `ChevronRoom`.

`controls::SplitButton` is two parts in one row, owner-placed in its arrange:
`internal::SplitButtonMain`, a `Button` as wide as its content, and
`internal::SplitButtonArrow`, a `DropDownButton` `ArrowWidth` wide, `Divider` unit apart.
Neither part takes the keyboard focus: a press finds the `SplitButton` as the nearest
ancestor taking keyboard input, so the control is one Tab stop, Space and Return press
the main part, and Alt+Down and F4 open the menu. The main part fires `onPress` (and
`onToggle` with `setIsToggle(true)`, WinUI's ToggleSplitButton), the arrow part opens and
closes the menu, and neither fires the other's. The arrow's menu opens below the whole
control, left-aligned with it (`SplitButtonArrow::menuAnchor()`). `setMainItem(item)`
makes a menu item the main action (the "last used" pattern): the main part shows its
text and a press fires its `onMenuItemSelected` after `onPress`. `setDisabled` disables
both parts, closes the menu and takes the control's keyboard input, Tab stop and focus
away; with the menu open and the focus on the control (a press on the open arrow let go
off it), Escape, Alt+Down and F4 close it, and Space and Return close it before pressing
the main part, as a click on the main part does.

Each part has its own renderer and slots, so a hover or press of one writes its slots
alone: the main part draws its `buttonSurface()` square on its right with no border there
(`SquareTopRight|SquareBottomRight|NoBorderRight`), the arrow part mirrored with its
chevron at z+1, and the `SplitButton` itself draws the `Divider`-unit column between them
in the `SplitButtonDivider` surface (the Button border colour), so the two read as one
outline. Both parts take the control's focus as their own (`buttonSurface(part,
focused)`), the focus change redrawing them. While the control is focused the divider is
`ButtonFocus`'s border colour and while it is disabled the disabled main part's, the
colour both parts' borders are then, so the outline has no notch at its top and bottom
rows; `Base::focus()`, `unfocus()` and `setDisabled()` redraw the control, and so the
divider. SplitButtonDemo shows both kinds, and
`--scenario split-button` holds them to their slots.

#### The modal stack: what a dialog blocks

A canvas keeps a stack of modals (`Canvas::pushModal`, `popModal`, `topModal`,
`modalCount`, `isModal`). A modal is a child of the canvas (`pushModal` moves one made
elsewhere there); while the stack is not empty, the top modal and what is above it are
all the mouse and keyboard reach. What is above it is decided by kind, not by where the
mouse went last (`Canvas::aboveModals`): the shown popup, every floating `Menu` (a
ComboBox's list, a context menu), closed `Popup`s, the overlays (`addOverlay`: the toast
host, the inspector's highlight, a StatsOverlay), the `ToolTip` box and every
`setExcludedFromStats()` subtree (a tool, an Inspector's window).
`Canvas::modalAllows(control)` is that rule for any control; the canvas itself is never
allowed, and a control on another canvas always is.

**Input.** The hit test (`Canvas::getControlAt`, an override of Base's) tests the
canvas's children from the front and stops after the top modal, skipping the app
children it passes on the way (a shown app child left above the modals for one frame).
So the hover (`input::onMouseMoved`), a press, the wheel, a context menu and a tooltip
(`toolTipOwnerFor`, `contextMenuOwnerFor`) never reach a control under a modal; the same
check guards each of them where they are delivered, for the frame between a push and
the next hit test (`pushModal` marks the hover stale, so the end of that update phase
hit-tests it again). A press made before the push is not cut off: the control holding
the mouse (`MouseFocus`) still gets its moves and its release. The keyboard focus cannot
leave the top modal: `Base::focus()` on a control it does not allow does nothing, the
push takes the focus from such a control, and a key with no allowed focus goes to the
top modal itself (`input::onKeyEvent`). Tab walks inside it (`Base::nextTabable` roots at
the top modal, or at the canvas child above it a control is in: a Popup opened from it).
The canvas's accelerators are looked up in the top modal's subtree, not the app's.
Escape goes to `Canvas::escapePopup()` first, as without a modal; then to the focused
control, whose `onKeyEscape` bubbles up to the modal's if it does not take it (a
DragNumber's edit box cancels its edit, a LogView clears its selection), so an Escape
that nothing inside takes closes the dialog. Closing on Escape before the focused control
saw it would make every in-place edit in a dialog close the dialog.

**Order and z.** The modals are kept, in stack order, right after the last shown child of
the app's (`Canvas::keepModalsInPlace`, `keepOverlaysLast`'s first step): above the app,
below the popups, the floating menus, the tools, the overlays and the ToolTip box. A
hidden child between them is skipped, so a closed ComboBox list or Popup does not make
the modals move; an app child shown again after the push (`Canvas::onChildHiddenChanged`)
or added to the canvas puts them after it again in the next renumber. A modal is moved
only when it is out of place, so opening one renumbers its own subtree and nothing else.
`WindowControl::touch()` does not bring a modal window to the front: it is above the app
already.

**Leaving.** `popModal` takes a control off the stack wherever it is in it; for the top
one it gives the keyboard focus back to the control that held it at the push (by control
id, so a deleted one is not followed; only if it is visible and allowed, and only if the
focus is free: none, inside the leaving modal, or not allowed any more), and marks the
hover stale. A modal below the top that leaves first hands the one above it its own
focus to give back when that one recorded a focus inside it, or none (`eraseModal`): a
large dialog closed by a timer under its confirm still gives the app its focus back when
the confirm goes. A modal deleted outright leaves the stack in `~Base` and gets its focus
back in the next update phase, after its subtree is gone; a modal pushed before that phase
records that focus as its own to give back (`pushModal` finds none to take). One hidden is popped by the next
update phase (`Canvas::updateModals`), and one taken off the canvas at once
(`Canvas::onChildRemoved`), so deleting it before that phase leaves nothing on the stack.
Neither step runs on a hidden canvas, whose update phase stops early, so a modal (or a
toast) gone while it is hidden is no work (`nextWorkTime()`) until it is shown. A canvas
deleting its children forgets the stack without restoring anything.

`WindowControl::makeModal()`/`destroyModal()` are thin calls onto the stack (the
`internal::Modal` control, which had no textured renderer, is gone with `modal.h`); a
modal window's Escape closes it as its close button does, and hiding it pops it. Its
constructor takes the focus as the window is made, so `makeModal` hands the stack the
control that held it before (`pushModal(control, focusReturn)`), also when the app has
focused a field in the window by then. The interaction
scenario presses WindowDemo's "Modal Window" (`window_modal_*`).

**Two input rules the dialogs needed.** A single-line `TextBox` passes the Return
*press* on to its parents (it returned true before), so a dialog's default button takes
Enter from a field; the release is still the box's (it moves to the next control and fires
`onEnter`). And a key's release goes only to the control its press went to
(`KeyData::PressTarget`): Enter closing a dialog gives the focus back to an app TextBox,
which would otherwise act on a release it never saw go down. A multiline TextBox, a
CodeEditor, a DragNumber and a Selectable keep Return; a Selectable takes only its own
(`KeyboardFocus` is the row), so a field in a row (a DataTableRow cell) does not press the
row on Enter, and the press goes on to a dialog above it.

**No backdrop.** Nothing is drawn between a modal and the app: what is under it stays as
it is, and only its input is blocked. A translucent scrim over the app would hide the
app's text rather than dim it: every primitive slot is drawn before every glyph, and the
scrim's slot, in front, writes the depth that rejects the glyphs behind it (the same
reason as for the stats overlay's panel, "The stats overlay"). A real scrim needs the
renderer to draw glyphs behind a translucent slot, which is its own change.

#### Dialogs: `dialogs::Dialog` and the standard dialogs

`dialogs::Dialog` (`controls/dialogs/dialog.h`) is a canvas child hidden until `open()`
shows it and pushes it on the modal stack: a title bar (`internal::DialogTitleBar`, a
`Dragger`) with its title and a `WindowCloseButton`, an optional wrapped message
(`setMessage`), a content area (`getContent()`, a `ScrollControl` that scrolls
vertically) and an optional footer row of buttons (`internal::DialogFooter`, hidden until
`addButton`, its buttons left to right in the order added, each taking the keyboard
focus, which the textured skin shows with the `ButtonFocus` surface).

**Placement.** It is owner-placed (`Placement::Owner`): its own arrange sizes and places
it, one axis per arrange, as a Popup does, so a canvas resize or `setScale` centres and
clamps it in the pass that lays the canvas out, with nothing of the app's laid out for it.
Its width and height are each an `Extent`: its content's preferred size (the default),
units (`Extent::units(1200)`) or a fraction of the canvas (`Extent::fraction(0.92f)`),
never more than the canvas. It is centred until the user drags it by its title bar
(`dragTo`, which clamps it inside the canvas and keeps the place across resizes); a drag
frame is a pure move, one pass entering the canvas and the dialog alone.

**Closing.** `close(result)` hides it, pops it (which gives the focus back), calls the
result handler (`setResultHandler`, `std::function<void(Result, const String &)>`) once,
then `onClose`, then `delayedDelete()`s it unless `setDeleteOnClose(false)` or the handler
(or `onClose`) opened it again, as a prompt that sends a bad value back does. The handler is
moved out before it runs, and `close` does nothing while closing or closed, so it fires
exactly once whatever it does; it may open another dialog or delete the control that
opened this one, and one that deletes this dialog outright stops `close` before
`onClose`. Escape, the close button, `close()` and `setHidden(true)` close it with its
cancel result (`setCancelResult`). Deleting it, or its canvas, calls no handler.
`onKeyReturn` presses the focused footer button, else the default button
(`setDefaultButton`); `open()` focuses `setInitialFocus`'s control, else the default
button.

**The message is not in the ScrollControl.** A wrapped text inside a ScrollControl is
measured at the width of the previous pass (the ScrollControl's Y arrange lays its content
out after every calculateSize has run; its comment says so), which left a confirm as tall
as a narrower wrap. `setMessage` is a wrapped Label docked Top under the title bar, the
dialog's own child, measured at this pass's width, in one pass.

**Its look** is four surfaces in `tokyonight::surfaceRegistry()`: `Dialog` (the panel, a
window's with a stronger border), `DialogTitle` and `DialogFooter` (strips inside the
border, their outer corners rounded) and `DialogShadow`. The shadow is **opaque**,
`Dialog::ShadowSize` (4) units below and right of the panel and inside the dialog's
bounds: a translucent one would hide the app's text under it (as the backdrop would), and
inside the bounds the canvas clamps it with the panel. The panel is drawn at `z+1` over
the shadow (`depthLayers()` 2).

**The standard dialogs** (`controls/dialogs/query.h`): `message(canvas, title, text,
onResult)` (OK; its cancel result Ok), `confirm(canvas, title, text, Buttons::YesNo |
OkCancel, onResult)` (No and Yes, or Cancel and OK; the cancel result No or Cancel) and
`prompt(canvas, title, text, initial, onResult)` (a `PromptDialog`: a single-line field
and Cancel and OK; the handler gets the field's text with Ok and "" with Cancel). Each
opens at once and returns the dialog, a handle to `close(result)` or `close()` until its
handler has run. The text wraps at `dialogs::TextWidth` (360) units, the default button
is the rightmost (OK, Yes) and Enter chooses it, a prompt opens with its field focused
and its text selected, and the content is hidden. A dialog opened over another (a confirm
from a large dialog) is modal to it, and closing it hands the input and the focus back.

DialogDemo opens each; LargeDialogDemo opens a 1200 x 0.92 dialog holding a form and a
LogView over a page whose counter a canvas timer ticks while it is open (and only then,
so an idle window draws nothing), with a confirm to stack on it. `--scenario dialog`
holds each open frame to the dialog's ids, each settled frame to nothing, a change behind
the large dialog to that Label's ids and no pixel inside the dialog, and each close to
the frame before the open.

#### Toasts: `Canvas::notify`

`Canvas::notify(kind, text)` and `notify(kind, text, ToastOptions{timeout, title})` show a
`controls::Toast` (`controls/toast.h`) in the canvas's one toast host
(`internal::ToastHost`, made the first time): a stack owner-placed `Toast::Gap` (8) units
in from the canvas's top-right corner (at the left edge of a canvas narrower than that),
`Toast::Width` (320) wide and as high as its toasts. The toasts are docked Bottom in the order they came, so the newest is at the top
and removing one closes its gap towards the corner. The host is a canvas overlay
(`addOverlay`): above every modal, popup and menu, below the ToolTip box, so a tooltip
still draws over a toast. It draws nothing and takes no mouse, so the mouse between and
around the toasts reaches the app; a toast takes the mouse but no keyboard focus, and a
press on one leaves the app's focus where it was (`Canvas::inToastHost`, read by
`input::onMouseClicked`).

A toast is an optional title in its kind's colour (`Colors.Toast`, the theme's
`ToastInfo`, `ToastPositive`, `ToastWarning` and `ToastNegative` text colours; the text is
`ToastText`), a text wrapped at the toast's width, and a close button, on the opaque
`Toast` surface with an accent strip of its kind's surface down its left edge (`z+1`).

**Lifetime.** Each kind has a default timeout (Info and Positive 4 s, Warning 6 s,
Negative 8 s; `setToastTimeout`), a call may give its own, and 0 keeps it until it is
dismissed. The deadlines are on the **toast clock** (`toastClock()`): the canvas clock
less the time the stack was paused. While the canvas's hovered control is in the host
(a toast, its text, its close button) the stack is paused: the update phase records when
the pause began, and when the hover leaves it adds the time paused to the total, so every
toast resumes with the time it had left; that is moving every deadline by the paused
time, without touching each toast. The update phase's toast step (`Canvas::updateToasts`,
after the tooltip step, before layout) compares the clock with the earliest deadline,
once a frame, and dismisses what is due: expiry is no animation (`anims_run=0` while
toasts show), and `nextWorkTime()` returns the first time the comparison holds, so an idle
window wakes for it and not before. More than `setToastLimit()` (5) shown drops the
oldest, hovered or not.

**Removal.** `Toast::dismiss()` (its close button's press, the expiry, the limit) hides
it at once, so the others close the gap in that frame's layout, and `delayedDelete()`s
it, so the next frame gives back every id it took; the canvas marks the hover stale, so
what is under a resting mouse after a removal is hovered. A click on a toast's body does
nothing. The host stays once made; it holds its own control's one id.

ToastDemo has a button per kind, a multi-line toast and a sticky one; `--scenario toast`
holds the stack, the pause (its entry, held and exit frames write nothing), the expiry
(the frame before them back, and the ids) and the sticky toast's close.

### 2. `renderCanvas()` — render

`Canvas::renderCanvas()` brackets the frame with `renderer->begin()` / `end()` and,
after `renderPhaseLayout()` and the z renumber, renders the controls waiting in the
**pending-render queue** (`Canvas::renderPendingControls()` → `Base::drainRenderQueue()`
→ each one's hooks → `Base::render()`). For each control, `Base::render()`:

- returns immediately if `getStateChange() == StateChange_Nothing` (nothing to do),
- on `StateChange_Created`, calls the skin renderer's `generate()` to **allocate
  primitives** for that control,
- calls the skin renderer's `update()` to **re-emit only the changed draw state**,

and the drain clears the control's state-change mask once its hooks have returned.
So an unchanged control issues no renderer calls at all; a changed control only
rewrites its own primitives.

**The queue.** `addStateChange()` (not virtual, so no subclass can set bits without
it) puts the control at the end of one process-global `std::vector<Base *>`, unless it
is queued already: each control keeps its index, `m_renderQueueIndex` (-1 when it is
not queued), so the push is O(1) and a control is in the queue once. `setSkin()`,
which sets `StateChange_Created` itself, queues it the same way. `~Base`, as the last
thing it does, nulls its slot through that index, and a drain compacts the vector,
so the queue never hands out a deleted control. Nothing happens on attach: a control
created or changed off any canvas keeps its entry until it is on one that renders.
One UI thread uses it, as the id banks in `primitiveIds.h` (`controls::queuedRenderCount()`
for tests; another thread hands work over through `Canvas::post`, "Threads, posts and
the idle loop"). A canvas renders the queued controls on its own tree and nothing else, so a
settled frame renders no control and a hover renders the controls it repaints, not
their ancestors. `ChangeStats::visited` counts the controls rendered (`visited=` in
`ControlTest --stats` and gweni_bench): Demo's first frame renders 4150 and a settled
one 0, and one changed row of a 10 000-row ListBox renders that row, with no
sibling scanned. `tests/unit/renderWalk.cpp` pins it.

**The drain** (`Base::drainRenderQueue(skin, zTied, treeSize)`, shared by `Canvas`
and `WindowCanvas`):

1. *Root state.* It reads the render offset and clip region the canvas has just set on
   the renderer, rather than assuming them: (0, 0) and `getBounds()` for a `Canvas`,
   (-x, -y) and `getRenderBounds()` for a `WindowCanvas`. It leaves the renderer at
   that state when it is done, as the recursive walk did, for whatever the canvas
   draws after.
2. *A dense frame walks the tree.* When the queue holds a quarter of the tree or more
   (the first frame, a resize, a scale; `treeSize` is the canvas's estimate,
   `m_zHigh / (1 + m_zGap)`, one control per gapped range of its last numbering), the
   drain walks the tree once in preorder, as the recursive walk did but with no skip,
   rendering the queued controls it meets with the state steps 4 and 5 describe: that
   costs a few instructions per clean control where a sort and a chain per dirty one
   would cost more (`tools/profile.sh resize --n 1000`: 30.29 M render instructions,
   against 30.31 M for the recursive walk, where sorting every entry cost 34.3 M). A
   tree change from a hook stops the walk at once, without stepping on through a list
   the hook may have changed, and what it had not reached goes through steps 3 and 4.
3. *Order.* Otherwise it sorts the entries by `getZIndex()`, ties by queue order: not
   at all if they are in z order already, else a stable radix sort on the bytes z uses
   (`sortRenderOrder`), O(entries). z is the tree's
   preorder (a parent before its children, each child's subtree before the next
   child's) and strictly increasing, because the renumber that has just run
   (`renumberZOrder()`, after `renderPhaseLayout()`) gives every control a range after
   the one before it in preorder, gaps or not, and leaves a clean subtree's labels only
   when they are still in that order ("Which controls a renumber enters"). So `generate()`
   allocates ids, and the skin writes slots, in the same order as a walk of the tree,
   and the pixels are byte for byte the same. When the renumber clamped
   (`Canvas::m_zClamped`: a tree too large for the depth range, whose last ranges tie)
   the drain walks the tree, as in step 2, instead of sorting.
4. *State.* For each entry it follows `m_actualParent` up to the nearest control on its
   **ancestor stack**: the levels of the previous entry's chain, each holding the
   offset and region that control's children render in. The root is always the
   stack's first level. It pops back to that ancestor and pushes the levels below it,
   each as the recursive walk computed them: the control's bounds' origin added to the
   offset, the region narrowed to its bounds if it `shouldClip()`, and a child's region
   narrowed further by its parent's `getChildClipRect()` (a ScrollControl's viewport).
   Since the entries come in preorder, a parent is usually on the stack, and the entry
   rendered last is pushed when the next one is its child (a leaf costs no level): a
   change over many controls costs O(entries), not O(entries × depth). A control's level is found from its `m_renderStackLevel`,
   trusted only if that level still holds it, so the stack is never cleared control by
   control and holds pointers it compares but never dereferences.
5. *Hooks.* `renderUnder()` and `render()` run at the parent's offset and region,
   `renderOver()` and `renderFocus()` with the control's own offset added, in the
   parent's region, as in the recursive walk. They run one after the other, before the
   control's children: the walk ran `renderOver()` and `renderFocus()` after the
   subtree, which no override depends on (each draws only its own control, or nothing).
   A hook runs only for a control that is queued, never for a clean ancestor of one,
   so a look that depends on descendants' state cannot be picked in a hook: a
   `PropertyRow` and its label, whose look (Normal, Hover, Editing) follows the hover
   anywhere in the row and its property's keyboard focus, are redrawn from
   `onDescendantInteraction()`, which `Base` calls on every ancestor of a control whose
   hover or keyboard focus changed, in the update phase (the input handler too, where it
   moves either without an enter, a leave or a `focus()`), and only when the look
   changed. The hover moves on before the old control hears its leave, as the focus
   does in `focus()`, so an ancestor told of the leave already sees where the hover
   went, and the mouse moving between a row's label and its value marks nothing.
6. *Clearing.* After the hooks have returned, the control's state and index are
   cleared, so a change it raised on itself from its own hooks is cleared with it, as
   before.
7. *Carried entries.* An entry whose chain ends somewhere else (detached, or on another
   canvas) stays queued for a later drain, the one of the canvas it is on then. An
   entry queued during the drain (a hook raising a change on another control) is
   rendered after the sorted ones, in the order they came, up to a cap (the number of
   entries the drain began with, at least 256), so hooks that keep queueing each other
   end the frame; the rest waits for the next one. `ChangeStats::renderCarried`
   (`render_carried=`) counts the entries left: 0 in every Demo frame, and constant at
   any N in every gweni_bench scene (`bench_scaling` checks both counters).

The drain allocates nothing once its vectors (the queue, the order and its radix
scratch, the stack and the chain, kept from frame to frame) have grown.

**The render walk only draws.** Adding, removing, reordering or deleting a control
belongs to the update phase or to layout, never to a `render()`, `renderUnder()`,
`renderOver()` or `renderFocus()`. The rule is checked in every build.
`Canvas::renderCanvas()` and `WindowCanvas`'s render hold a `RenderWalkScope` around
the drain only, after `renderPhaseLayout()` (layout may create controls before the
drain: a wrapped `Text` builds its line children there), and the drain holds one of
its own. The drain keeps the control whose hooks are running in a global. While a
scope is open, `Base::addChild()`, `addChildOfLayout()`, `removeChild()` (and so
`setParent()`), `bringToFront()`, `sendToBack()`, `bringNextToControl()` and `~Base`
each print one line on stderr the first time an (operation, type) pair is seen:

```
gweni: <Type>#<id> <operation> during the render walk (rendering <Type>#<id>)
```

They also record it in `controls::renderWalkMutations()` (`resetRenderWalkMutations()`
forgets the record, for tests). `~Base` sees the type as `Base`, because the derived
destructors have run. Outside a scope the check is one branch. ControlTest exits
non-zero when the record is not empty, as it does for `typesWithoutSkinRenderer()`,
so `controltest_teardown` and `controltest_one_pass_each` fail on an offence in any
demo.

**The guard reports a tree change; the queue survives it.** Outside the dense walk
(which stops at the change, step 2) the drain follows no list a hook could change
under it: it holds queue slots, which a deleted control nulls,
and checks its entry's slot after each hook, so a hook that deletes the control being
rendered (with its parent, say) ends that control's turn without another call on it.
Every reported change also bumps a counter the drain reads before each entry: if it
moved, the stack is rebuilt from the root, since it may name a control that moved or
was deleted. A control detached by a hook is carried, and rendered once it is on a
canvas again. What that frame draws for the controls the hook moved is still not
defined (they may render at their old place, or not until the next frame), which is
why it is reported; but nothing is touched after it is freed, and the drain ends
(`tests/unit/renderWalk.cpp` deletes and detaches queued controls from a `render()`;
the ASAN tree checks the memory).

## Threads, posts and the idle loop

gweni is one-UI-thread: the controls, the render queue, the id banks and the renderer
are used by the thread that calls `updateCanvas()` and `renderCanvas()`, and by no
other. The one thing in the library another thread may call is a canvas's post queue.
With it, timers and `Canvas::nextWorkTime()`, a window can sleep while its canvas is
settled and wake for a post, a timer, an input or a deadline (#584). This is for an
app left open for days and fed from other threads, such as a dashboard whose HTTP
callbacks arrive on a worker.

**The clock is a double.** `platform::getTimeInSeconds()` is a `double` of seconds
since start, and so is `setTimeSource(std::function<double()>)`. So are the stored
times: a `TimedAnimation`'s start and end, the caret's next blink, the tooltip's start,
the key repeats, the last click, the ProgressBar's last frame, the stats overlay's
refresh, an undo step's time and the CodeEditor's auto-scroll. A `float` of seconds
steps by 7.8 ms after a day and by 0.25 s after a month, and that is not enough for a
caret, a tooltip delay or a timer. Durations (`setToolTipDelay`, the repeat delays)
and per-frame deltas (`Animation::run(float)`) stay `float`, and a time source
returning a `float` still converts. The stepped clocks of the captures, ControlTest,
OpenGLCore and the bench count whole steps and read them as `steps*(1.0/60)`, so a
capture's frame k has the same time in every run and every tree. No sample moved.
`tests/unit/longClock.cpp` runs the caret, the tooltip, a key repeat, a
`TimedAnimation` and a timer 30 days in, on 1/60 s steps.

**Posts.** `Canvas::post(fn)` hands a function to the UI thread from any thread. It
goes to the canvas's `gweni::PostQueue` (`include/gweni/postQueue.h`), a mutex and a
vector. `updateCanvas()` runs the queue first thing, before `doThink()`, so a post runs
after the input the loop has just delivered. `run()` swaps the vector out under the
lock and calls the functions outside it, in the order they were queued:
- FIFO per thread, and across threads the order in which they took the lock;
- a function posted while the posts run waits for the next update, so one that posts
  itself again cannot keep a frame from ending.

`postHandle()` is the queue as a `std::shared_ptr`, for a worker that may outlive the
canvas. `~Canvas` closes the queue first: it destroys what is still queued (on the
UI thread, outside the lock, since a capture's destructor may post) and drops the wake
handler, and after that every `post()` through the handle returns `false` and does
nothing. Without pthreads (the web tree) the mutex compiles to nothing and the same
code builds. `tests/unit/postQueue.cpp` covers it: 8 threads × 10 000 posts each run
once on the updating thread, in each thread's order and in lock order (a sequence
number taken under the lock), a post from a post runs in the next update, and a post
after the canvas is deleted returns false (clean in the ASAN tree; helgrind is clean
on the queue, and TSAN cannot run on the container's kernel).

**The wake handler.** `Canvas::setWakeHandler(fn)` is called when a `post()` finds the
queue empty, from the posting thread and outside the lock, so it must be safe on any
thread. On the UI thread it is also called when a timer is added with an earlier
deadline than any pending one. The core never wakes anything itself: the loop helper
installs `glfwPostEmptyEvent()` (natively) or sets a frame-wanted flag (on the web).

**Timers.** `Canvas::after(seconds, fn, owner)` fires once and `every(seconds, fn,
owner)` repeats. Both return a `TimerHandle`, which `cancel()` takes, and
`timerPending()` and `timerCount()` read them. They run in `updateCanvas()` after the
posts and before `doThink()`, so before the animations, off `getTimeInSeconds()`,
which `setTimeSource()` drives:
- A timer fires on the first update at or past its deadline. Timers due in the same
  update fire by deadline, then in the order they were added. One that a callback
  adds or reschedules waits for the next update.
- A repeating timer's deadlines are `origin + k*period`, so it does not drift. After a
  gap of several periods it fires once and goes on from the next multiple past the
  update's time. A period of 0 or less fires once each update.
- `cancel()` is safe from inside any callback, its own or another timer's. A cancelled
  entry stays in the heap until it comes up (lazy removal), and its function is
  destroyed once nothing is running it.
- With an owner, `~Base` cancels the owner's timers on every live canvas
  (`Base::m_ownsTimers` keeps that walk off controls that own none), as it calls
  `anim::cancel(control)`.

They are a min-heap of (deadline, serial, id) over a map of the pending ones, and not
`anim::Animation`s: the registry runs and counts every animation every frame, while a
waiting timer costs an update one look at the heap's top and adds nothing to
`anims_run` (`tests/unit/timers.cpp`).

**`Canvas::nextWorkTime()`** is when the canvas next needs an update and a render, on
`getTimeInSeconds()`. It returns `Canvas::WorkNow` (-infinity), a time, or
`Canvas::NoWork` (+infinity).

It is WorkNow while any of these holds:
- posts are pending, or `needsRedraw()` is set;
- a control on this canvas's tree is queued for render (`queuedRenderCount(this)`, a
  scan of the process-global queue; a settled queue holds only entries off every
  canvas);
- layout or the z order is dirty, or a delayed delete is queued;
- the hover is stale on a shown canvas with no input capture set, or the renderer's
  `textLayoutSerial()` moved (a text-AA switch). `input::onCanvasThink` takes a stale
  hover only then, so under a capture (the depth view) or on a hidden canvas it waits,
  and is work again once the capture is cleared or the canvas shown;
- a hovered control's tooltip owner is new.

Otherwise it is the earliest of:
- the next timer;
- `anim::nextThinkTime(canvas)`, the earliest `anim::Animation::nextThinkTime()` of the
  canvas's animations. The default is now, today's every-frame behaviour. A
  `TimedAnimation` returns its start until it has started. The caret blink returns
  its next edge (`Base::nextCaretBlinkTime()`, `CaretBlink::nextEdge`). An
  indeterminate or cycling ProgressBar, and a spinning Spinner, keep the default
  while they show and have no animation registered while they are hidden
  (`Base::onVisibleChanged()` cancels it): the demo shell's hidden pages hold several,
  which kept an idle window drawing every frame. LogViewDemo's stream returns never
  from `nextThinkTime()` while it is hidden, and leaves out of its stream the time
  slept on its first run once shown (`window_idle_demo` runs the
  idle check on the whole shell). The stats overlay's refresh and the Inspector's
  (`tools::RefreshAnimation`) keep the default while they are open;
- the tooltip's deadline;
- the next key repeat (`input::nextKeyRepeatTime(canvas)`, from the process-global key
  state).

The tooltip and key deadlines are nudged up with `nextafter` to the first time for
which the update's own comparisons hold, so a frame drawn at the deadline does the
work it waited for. A hidden canvas runs no layout, animation, tooltip or key repeat,
so it waits for its timers alone. `tests/unit/nextWorkTime.cpp` covers each case.

A change that touches pixels but no control has to mark the canvas, or an idle loop
shows it late. The depth view's camera calls `redraw()`, the theme editor's apply
marks the whole tree, and a text-AA switch moves `textLayoutSerial()`. The verify mode
(below) exists to find any others.

**The loop.** `platform::IdleLoop` (`include/gweni/platforms/idleLoop.h`) is the
scheduling with no window system in it, so `tests/unit/idleLoop.cpp` drives it with a
fake clock and a fake wait. Its hooks are `now`, `nextWorkTime`, `poll`, `wait`,
`drawFrame`, `canDraw`, `frameWasIdle` and `verifyFailed`.
- `iterate()` (native) polls, then draws a frame if the work is due (`nextWorkTime() <=
  now()`). Otherwise it blocks in `wait(due - now)`, +infinity when there is no work,
  and draws nothing. An event that changed nothing therefore makes the next
  `iterate()` wait again. `canDraw` false (a Vulkan window iconified, or a failed
  begin) waits for an event instead, in either mode.
- `tick()` (one browser animation frame) polls, and draws only if work is due or
  `wantFrame()` was called; otherwise it returns at once.
- `LoopMode::Continuous` polls and draws on every iteration it can draw.

Over a simulated idle minute the loop draws 0 frames, after a post exactly 1, and with
a focused TextBox 2 a second (its caret edges). A post from another thread ends a
blocked wait and is drawn in the next frame.

`input::GlfwLoop` (`include/gweni/platforms/input/glfwLoop.h`, header-only, beside
`input::GLFW3`; the core never links GLFW) is that loop on a GLFW window:
- Natively, `wait` is `glfwWaitEventsTimeout(seconds)`, or `glfwWaitEvents()` with no
  deadline. The wake handler is `glfwPostEmptyEvent()`, GLFW's one call that is safe
  from any thread. A window refresh callback (its contents lost) calls `redraw()`.
  A frame that is not drawn is not swapped or presented either. That is safe because
  every frame a `SlotRenderer` draws, it draws its whole mirror.
- In the browser, `run()` is `emscripten_set_main_loop_arg` at the browser's rate, each
  callback a `tick()`. The wake handler sets the frame-wanted flag. The main loop is
  never paused (`emscripten_pause_main_loop`): a missed wake there would leave the
  page frozen.

`capture::runWindowLoop` and `Device::runWindowLoop(canvas, renderer, options, frame,
afterIteration)` put both apps' windows on it, GL and Vulkan, native and web.
`drawFrame` is the update, clear, render and swap on GL, and `beginWindowFrame`, the
frame and `presentWindowFrame` on Vulkan. A Vulkan frame that was not shown (an
out-of-date acquire) calls `canvas->redraw()`, so the next iteration draws it again.
The windows are idle by default: `--loop idle|continuous` in both apps
(`capture::loopOptions`), and on the web too. Captures, `--window-test` and the bench
step their own frames and draw every one.

Measured under Xvfb (debug tree), the process's CPU time (`getrusage` of the child,
every thread) from start to a SIGINT:

| window | wall | idle | `--loop continuous` |
|---|---|---|---|
| ControlTest ButtonDemo, GL | 4 s | 0.43 s | 10.4 s |
| OpenGLCore demo, GL | 6 s | 1.17 s (1.14 s over 14 s) | 68.9 s |
| OpenGLCore demo, Vulkan | 6 s | 1.70 s | 64.3 s |

The idle figures are start-up and the first frames; they do not grow with the time
left open. Settled, `--idle-test` measures 0.2 to 0.5 ms of CPU over 2 s.

**Verify mode.** With `LoopOptions::verify` (`GWENI_IDLE_VERIFY=1`, ControlTest
`--verify-idle`), whenever the loop would skip a frame it draws one anyway. That frame
fails, naming its number on stderr, if the renderer's `frameStats()` show a slot
written or an upload (`GlfwLoop::frameWrites`). ControlTest's `--verify-idle` does the
same around every scripted and scenario frame whose `nextWorkTime()` is past the clock
before its update. It also compares the pixels with the frame before, which catches a
change that writes no slot (the depth view's camera). It found one miss: the
interaction scenario hid a primitive directly, and it now calls `redraw()`.
`controltest_verify_idle_<scenario>` runs it over the interaction, tooltip,
context-menu, depth-view and stats-overlay scenarios.

**The window, measured.** `window_idle` and `window_idle_vulkan`
(`tests/windowIdle.cmake`) run `ControlTest --idle-test` on ButtonDemo under
`xvfb-run -a`:
1. A worker thread waits for the canvas to settle, then lets the window idle 2 s.
2. It posts. The posted function checks that no frame was drawn and nothing was swapped
   or presented meanwhile, and that the process took under 0.1 s of CPU. Then it sets
   a Button's text.
3. The frame the post ran in must write glyph slots (the new text). A canvas timer 1 s
   later checks that no frame was drawn between that one and its own, and that one
   swap or present was made from the post's frame on (on GL a swap is a drawn frame;
   on Vulkan the device counts its presents), and closes the window.

On the web, `pre.js` publishes `gweniRun.ticks` (callbacks) beside `gweniRun.frames`
(frames drawn), and ControlTest exports `gweni_web_post()`, a `post()` of a `redraw()`.
`web_input`'s idle check requires a settled ButtonDemo to draw no frame over 30 ticks,
exactly one for a post, and none after it. The page scripts wait for a page to settle
(`tests/web/frames.mjs`: the frames asked for drawn, or a tick passed with none),
never for "some more frames", which a settled page never draws.

Not done here: an O(1) skip of the animations that are not due, a coalescing
`post(key, fn)` and `emscripten_pause_main_loop`. `WindowCanvas`, which never runs
`Canvas::updateCanvas()`, has no posts or timers.

## The renderer abstraction

A renderer implements `gweni::renderer::Base` (`include/gweni/platforms/baseRender.h`).
The core never makes API calls directly — it only calls this interface. Key methods:

| Concern | Methods |
| --- | --- |
| Frame | `begin()`, `end()`, `setViewRect()` |
| Primitive lifetime | `generatePrimitive(id)`, `releasePrimitive(id)` |
| Geometry | `drawFilledRect(id, rect, z)`, `drawTexturedRect(tex, id, rect, z, uv…)`, `drawRoundedRect(…)`, `drawBilinearRect(…)`, `drawCheckerRect(…)`, `drawIcon(…)` |
| Visibility | `showPrimitive(style, id)`, `hidePrimitive(id)` |
| Text | `generateTextPrimitive(id)`, `renderText(id, font, pos, z, text)`, `showText/hideText`, `measureText()`, `lineHeight(font)` |
| Color / clip | `setDrawColor()`, `startClip()/endClip()`, `setClipRegion()` |

The unit of retained state is the **primitive id**: the core asks the renderer to
allocate ids (one control owns N of them, declared by `skin::Control::requiredPrimitives()`),
then references those ids when emitting/updating/hiding geometry. How a primitive id
maps to actual GPU resources is entirely the renderer's business.

A backend can be **immediate** (just draw in each `drawFilledRect` call; ignore the
retained-update benefit) or **retained** (the primitive id indexes into a persistent
GPU buffer). The same control/skin code drives both.

## The shared core: `SlotRenderer` and its backend hooks

A retained renderer is mostly API-independent, so that part is one class,
`renderer::SlotRenderer` (`include/gweni/platforms/renderers/slotRenderer.h`,
`source/platforms/renderers/SlotRenderer/slotRenderer.cpp`), deriving from
`renderer::Base`, and a backend derives from it and implements a narrow set of
pure-virtual hooks. `OpenGLCore` is the GL backend and `renderer::Vulkan` the Vulkan
one (#368, "renderer::Vulkan (headless)" below); a WebGL one (#338) is meant to be
another. The core holds, with no API call and no GL header of its
own:

- the CPU mirrors: the primitive `Vertex` slots and their clip `Box`es, the glyph
  slots (`ftgl::glyph_vertex_t`) and theirs, and the per-slot byte diff that marks only
  a slot whose bytes changed (`markPrimitiveChanges`, the `DirtySlots` sets,
  `snapshotMirror`);
- the vertex packing of every style: `setVertex`, `writeRoundedQuad`, `drawIcon`,
  `drawLine`'s stroke placement, `drawTexturedRect`'s page, layer and size and
  `cullOutOfRangeQuad`;
- the image pages' layers: which class an image takes, its layer from the page's
  free list, a full page doubled (`allocateImageLayer`, `placeImage`);
- the text runs and their slot allocator (`allocateTextRun`), and each atlas's draw list
  (`buildTextDrawList`, `setTextRunFont`), as `ChunkDraw`s of index ranges;
- font resolution and layout: `resolveFont` (coverage, LCD or SDF by size and
  `TextAA`), `loadFont`/`freeFont`/`ensureFont` with their owners, `layoutSdfText`,
  `measuredPenShifts`, `measureText`, over freetype-gl's CPU-side atlases and their
  dirty row band;
- the loaded images (`SlotTextureData`, their pixels while readable);
- the upload policy: which ranges are sent (coalesced dirty runs, the slots the mirror
  gained), when a buffer or clip-box table regrows (1.5 times, a power of two of rows),
  how a run of clip boxes is cut into row pieces;
- `FrameStats`, every count of it made here from what it asks the hooks to do, so two
  backends count alike by construction.

The hooks (`SlotRenderer`'s protected section, in the order `flush()` calls them) take
sizes, ranges and the core's own data, never an API object:

| Hook | What the backend does |
| --- | --- |
| `beginFrame()` | the frame's state: blending, the depth test `LESS` (from `begin()`) |
| `allocateVertexBuffer(path, bytes)`, `uploadVertexRange(path, offset, bytes, data)` | (re)allocate a path's vertex buffer, copy a byte range of its mirror to it |
| `maxClipBoxRows()`, `allocateClipBoxes(path, rows)`, `uploadClipBoxRows(path, x, y, w, h, boxes)` | the device's row limit; (re)allocate a path's clip-box table, `kClipColumns` boxes a row; copy a block of boxes to it |
| `createAtlasTexture(atlas)`, `uploadAtlasRows(texture, atlas, firstRow, rows)`, `destroyAtlasTexture(texture)` | a font atlas's texture (R8, or RGB8 for depth 3), its dirty rows, its end |
| `createImageTexture(image, rgba)`, `destroyImageTexture(texture)`, `readTextureRgba(...)` | an image's texture with its mip levels, each uploaded from `imageMipChain`, its end, a read back (`pixelColor`, `readFontAtlas`) |
| `beginPrimitiveDraws()`, `drawPrimitiveChunk(draw)`, `endPrimitiveDraws()` | one draw per chunk with a slot that can show, in slot order, of the chunk's `ChunkDraw`: its runs of such slots ("Only the slots that can show are drawn") |
| `beginTextDraws()`, `beginTextAtlas(font)`, `drawTextChunk(draw)`, `endTextDraws()` | per atlas with runs to draw: its texture and `TextMode` (an LCD one blended per channel), then one draw per `ChunkDraw` |
| `submitFrame()` | the end of `flush()`: the frame's commands go to the device |
| `indexChunkSlotsChanged()` | `setIndexChunkSlots()` changed the chunk size |

`path` is `SlotPath::Primitives` or `SlotPath::Text`. A `TextureHandle` is the
backend's name for a texture (OpenGLCore's GL texture name; 0 is none): an atlas's is
kept in freetype-gl's `texture_atlas_t::id`, an image's in `SlotTextureData::m_handle`.
A `ChunkDraw` is the ranges of one chunk's draw, a primitive chunk's or an atlas's
text in a chunk: an index count and the first index's byte offset, from the chunk's
first slot, per range (the form `glMultiDrawElements` takes; a first index is the
offset over `sizeof(uint16_t)`). `OpenGLCore` draws one as a `glMultiDrawElements`
where the context has it (`useMultiDraw()`) and as a loop of `glDrawElements`
otherwise, `renderer::Vulkan` as one `vkCmdDrawIndexed` per range; either way it is
one of `FrameStats::drawCalls`.
Five public pure-virtual methods serve the tests and the apps on any backend:
`clearTarget(color)` and `readPixels(width, height, pixels)` (top row first),
`textureExists`, `textureChannelSizes` and `deviceError`.

`init()` and `setViewRect()` are the core's too (it forgets what the device holds and
keeps the view); a backend's own call the core's and then make or resize its objects.
A backend's destructor calls `releaseResources()` first, while its device is still
there: it destroys every atlas and image texture through the hooks and frees the CPU
side (`eraseFont`, `deleteTextureData`). The core's `end()` is `flush()`, so a
wrapper such as `gweni_bench`'s `TimedFlush<Renderer>` times the flush of any backend
by overriding `end()`.

The vendored freetype-gl's headers include its `opengl.h` (`freetype-gl.h` does
directly, and `text-buffer.h`, which the core lays coverage text out with,
`text_buffer_add_text` into a CPU vector, through `vertex_buffer_t`'s GL names), so
the GL headers are still on the core's include path, and on that of everything
including `slotRenderer.h`, through them; the core calls only freetype-gl's CPU side.

The GPU unit tests (`tests/`) take the shared renderer as a `SlotRenderer`
(`test::gpuRenderer()`, `test::newRenderer()`, `test::clearView()`,
`test::readView()`), so they are written once for every backend; `tests/main.cpp` is
the one place that picks it. What checks GL itself (`glslDialects.cpp`,
`textDrawLoop.cpp` and `fontRelease.cpp`'s GL-object case) is the doctest test suite
`gl_only` (`tests/glRenderer.h`), which a run on another backend leaves out
(`-tse=gl_only`); each file's header says why.
`GWENI_RENDERER=vulkan` makes `main.cpp` create a Vulkan device and a
`renderer::Vulkan` instead, with no GL or EGL context, and `unittests_vulkan` is that
run ("renderer::Vulkan" below).

## renderer::Vulkan (headless)

`renderer::Vulkan` (`include/gweni/platforms/renderers/vulkan.h`,
`source/platforms/renderers/Vulkan/`) is the Vulkan 1.3 backend of `SlotRenderer`,
built with `GWENI_RENDER_VULKAN` (which, for now, needs `GWENI_RENDER_OPENGL_CORE`:
the core, freetype-gl and the shader bodies are built with it). It draws every
primitive style, with the clip boxes and the image pages, and text in every mode
(coverage, LCD with dual-source blending, SDF) from the core's atlases and glyph
slots, so `unittests_vulkan` runs every GPU unit case but the `gl_only` ones.

The application owns the Vulkan objects, as it owns a GL context:

- **`VulkanDevice`**: the instance, physical device, device and one graphics queue.
  `createVulkanDevice(device, options, error)` makes one (volk loads
  `libvulkan.so.1`, nothing links it): Vulkan 1.3, the first device with
  `dynamicRendering`, `shaderDemoteToHelperInvocation` (glslang compiles a Vulkan 1.3
  shader's `discard` to a demote) and a graphics queue, and `dualSrcBlend` where the
  device has it (`VulkanDevice::dualSourceBlend`; without it `lcdTextUnavailable()`
  says so and LCD text is grayscale, as on a GLES context without
  `GL_EXT_blend_func_extended`). With `VulkanDeviceOptions::validation` it enables
  `VK_LAYER_KHRONOS_validation` and a debug-utils messenger that prints every
  validation warning and error on stderr (`gweni: Vulkan validation error: ...`) and
  aborts, and fails, saying so, without the layer. The options also take the
  instance and device extensions a window adds (#375). No GLFW in the library.
- **`VulkanTarget`**: a colour image (`R8G8B8A8_UNORM`) and its depth buffer
  (`X8_D24_UNORM_PACK32` where the device renders to it, as GL's
  `GL_DEPTH_COMPONENT24`, else `D24_UNORM_S8_UINT` or `D32_SFLOAT`), of one size,
  what an EGL context's framebuffer is to `OpenGLCore`. Any renderer on the device
  draws into it and reads it back; between frames its images are in
  `COLOR_ATTACHMENT_OPTIMAL` and `DEPTH_STENCIL_ATTACHMENT_OPTIMAL`, as every
  renderer leaves them. `renderer::Vulkan(paths, viewRect, device, target)` draws
  into one, `setTarget()` into another of the same formats (another format makes its
  pipeline again).

How each GL feature maps:

| OpenGLCore | renderer::Vulkan |
| --- | --- |
| GL context, offscreen framebuffer | `VulkanDevice`, `VulkanTarget` |
| `glBufferData` (a regrow) | a new device-local buffer, after `vkQueueWaitIdle` (a frame in flight may draw from the old one) |
| `glBufferSubData` per dirty range | the range copied into the frame's host-visible staging memory, one `vkCmdCopyBuffer` per range |
| per-chunk vertex array, `glMultiDrawElements` of the chunk's ranges | `vkCmdBindVertexBuffers` at the chunk's first slot, `slotBase` pushed, one `vkCmdDrawIndexed` per range (`offsets/2` the first index); `draw_calls` counts the chunk once, as on GL |
| the static 16-bit quad index buffers | the same patterns, device-local, uploaded once by `init()` |
| clip boxes: 2D `GL_RGBA16UI` texture, `glTexSubImage2D` per row piece | 2D `R16G16B16A16_UINT` image, `kClipColumns` texels a row, one `vkCmdCopyBufferToImage` per row piece, sampled `NEAREST` |
| 5 image pages, `uniform sampler2DArray imagePages[5]` | one array of 5 combined image samplers in set 0, each a `2D_ARRAY` view (a class with no page samples a 1x1 one-layer array of opaque black, as GL samples texture 0) |
| `projection`, `slotBase` uniforms | push constants (`PrimitivePush`) |
| image page: a `GL_TEXTURE_2D_ARRAY` of `GL_RGBA8` layers, every level a `glTexImage3D`; an image's levels (`imageMipChain`'s bytes, RGB with alpha 255) one `glTexSubImage3D` each at its layer's origin, filtered nearest | `R8G8B8A8_UNORM` array image, the image's levels in one staging buffer, one `vkCmdCopyBufferToImage` of a region per level into its layer, sampled `NEAREST` |
| a full page grown: a new array of twice the layers, each kept layer's every level copied by a `glBlitFramebuffer` (`glFramebufferTextureLayer`, `NEAREST` at the same size: no `glCopyImageSubData` in the subset), the old one deleted | the same new image, one `vkCmdCopyImage` of a region per level over the kept layers, the old image retired until the frames in flight that may sample it are done |
| atlas: `GL_R8`, `GL_RGB8` for LCD, `GL_LINEAR`, clamped | `R8_UNORM` for coverage and SDF; `R8G8B8A8_UNORM` for LCD (no RGB8 format is guaranteed to be sampled), each texel expanded with alpha 255 on upload (the shader reads `.rgb`), `textureChannelSizes` still 8,8,8,0; sampled `LINEAR`, clamped |
| an atlas's dirty row band: one `glTexSubImage2D` | the band's rows staged (expanded for LCD), one `vkCmdCopyBufferToImage` into them, the image `TRANSFER_DST_OPTIMAL` for the frame's uploads; `atlas_upload_*` count the same, being the core's |
| text: per-chunk vertex array, `glMultiDrawElements` of an atlas's ranges | `vkCmdBindVertexBuffers` at the chunk's first glyph slot, one `vkCmdDrawIndexed` per range (`offsets/2` the first index) through the glyph quad order (`0, 1, 2, 0, 2, 3`); `text_draws` counts the chunk once, as on GL |
| `texture1` bound per atlas, `textMode`, `textContrast` uniforms | a descriptor set per atlas drawn in the frame (the atlas, the glyph clip-box table), from the frame's pools, reset as it begins; `textMode`, `textContrast` with `projection`, `slotBase` in one push-constant block for both stages (`TextPush`, `Vulkan/textPush.glsl`) |
| `glBlendFunc(SRC1_COLOR, ONE_MINUS_SRC1_COLOR)` for an LCD atlas | a second text pipeline with those factors (colour and alpha), made only with `dualSrcBlend` |
| `glDeleteTextures` of a freed atlas or image | retired: destroyed once the newest frame that may sample it has signalled its fence, its handle naming nothing from then on |
| `glBlendFunc(SRC_ALPHA, ONE_MINUS_SRC_ALPHA)`, `GL_LESS` with depth writes | the pipeline's blend and depth state |
| `glClear` | a dynamic rendering with `LOAD_OP_CLEAR`, submitted at once |
| `glReadPixels`, rows turned over | `vkCmdCopyImageToBuffer` into a host-visible buffer, rows turned over |
| `glFlush` | the frame's two command buffers submitted with its fence |

**One frame.** A frame records two command buffers, submitted together: the uploads
(staging copies, the atlas and clip-box images' layout changes) and then the draws
(one `vkCmdBeginRendering` with `LOAD_OP_LOAD`). `flush()` uploads the glyph slots
after drawing the primitives, and a copy cannot go inside a rendering, so every upload
goes into the first buffer, which runs before every draw. Each frame in flight
(`FramesInFlight`, 2) has its own command buffers, staging memory, descriptor set and
fence, and a frame waits for the one that last used them. The uploads buffer starts
with a barrier after every earlier submission's vertex, index and shader reads, so
a copy never overwrites a slot a frame still in flight draws from, and ends with one
before this frame's reads; the draws buffer starts with a barrier after every earlier
use of the target, another renderer's frame or a clear included. A texture written in
the frame is `TRANSFER_DST_OPTIMAL` from its first upload and `SHADER_READ_ONLY_OPTIMAL`
again at the end of the uploads. Replacing something the frames in flight might use
(a regrown vertex buffer or clip-box table) waits for the queue first; a freed atlas
or image is **retired** instead (`Vulkan::retireHandle`): the queue runs the frames in
order, so it is destroyed when the newest frame that may sample it is done (the one
being recorded, else the one submitted last), as that frame's slot is begun again
after its fence, or at once if no frame is in flight. A font freed and loaded again,
or one Label of many changing size, then costs no wait. `clearTarget`, `readPixels` and `readTextureRgba` submit what the frame
recorded, then run their own command buffer and wait for it. The counts are the
core's: one upload per range it hands over, one draw per chunk, and a settled frame
uploads nothing, on Vulkan as on GL.

**The shaders** are GL's bodies: `source/platforms/renderers/Vulkan/primitive.vert`
and `primitive.frag` are `#version 450` prologues that `#include` them
(`GL_GOOGLE_include_directive`), compiled to SPIR-V at build time by
`gweni_add_spirv` with `--auto-map-locations` (the bodies' `in`/`out` variables have
no locations: GL links them by name, and both stages declare them in the same order).
The prologue makes `projection` and `slotBase` the push constants, declares the
samplers in set 0, and makes `VERTEX_ID` `gl_VertexIndex`, which counts from the
chunk's first vertex as `gl_VertexID` does because each chunk binds its vertex buffer
at its first slot (with a `vertexOffset` it would count from the buffer's start).
`primitive.frag` hard-codes `CLIP_COLUMNS` (2048), which `vulkan.cpp` asserts.
The text shaders are the same: `Vulkan/text.vert` before `shaders/text.vert.glsl`
(whose `CLIP_SPACE_TO_API` GL defines as nothing), and `Vulkan/textFragment.glsl`,
which declares the atlas and the clip-box table in set 0 and hard-codes `SDF_SPREAD`
(6, asserted too), before `shaders/text.frag.glsl`, in two variants: `text.frag` with
the second output dual-source blending reads (`GWENI_DUAL_SOURCE`, `index = 1`), which
both text pipelines use on a device with `dualSrcBlend`, as OpenGLCore compiles its
one text program with it on a context that has it, and `textSingle.frag` without, for
a device that has not (whose LCD modes fall back to grayscale, as on GLES without
`GL_EXT_blend_func_extended`: `tests/noDualSource.cpp` checks that fallback on Vulkan
too, with a renderer on the device claiming no `dualSrcBlend`).

**GL's pixels.** The projection is OpenGLCore's matrix, into GL's clip space, and
`CLIP_SPACE_TO_API` maps its z from [-w, w] to [0, w] in the vertex shader as GL's
viewport transform does (`z*0.5 + w*0.5`; w is 1 in the front view, anything under
a view transform's camera, `SlotRenderer::setViewTransform`), so every depth is
computed as on GL. y is not flipped: the target holds its rows bottom-up, as a GL framebuffer does,
and `readPixels` turns them over as `OpenGLCore::readPixels` does. Every triangle then
lands on GL's window coordinates, so its coverage, its interpolated values and its
2x2 derivatives are GL's. A window will show the target through a flipping blit
(#375), not by drawing it the other way up. Flipping y in the projection (the first
way this was written) gave 405 of the 422 frames the primitive cases read back
byte-identical to GL's; not flipping gave 415. Of the 7 that differed, 4 differed by
1 in 1 or 2 pixels (a 50 % blend rounding to 127 or 128), and 3 were the polyline
frames of `lines.cpp`: a cap or joint pixel's coverage 1.0 on GL and 254/255 on
Vulkan, and so (the line style's half-layer depth deciding which segment of a
polyline blends over a joint) 1 or 2 joint pixels blended twice on one and once on
the other, up to 64 per channel. That was the float code GL's GLSL compiler and the
SPIR-V path made of `mix()` (#427). With the shared bodies on `lerp()` (#374) it is
gone: measured before #483 changed anything, all 18 polyline frames of `lines.cpp`
(scales 1, 1.5 and 2, thicknesses 1, 2 and 3, opaque and at 50 %) read back byte for
byte the same on both backends, joint pixels included. Since #483 a polyline drawn
with its neighbours no longer leaves a joint to the depth test at all: the segment
nearest a pixel draws it and its neighbour leaves it ("Line segments (style 3)"), so
neither the depth format (Vulkan's D24 or D32F) nor its rounding reaches a joint, and
`lines.cpp` checks that model on both backends, on a 24-bit and a 32-bit float depth
buffer, every frame of it byte-identical between the backends and between the two
depth buffers. The pixel test of the apps,
`backend_pixels`, has its own bound, measured on the ControlTest shots ("Vulkan's
counters and pixels against GL's" below).

Text is as near. Three runs of 12 to 40 px OpenSans (white, blue and black, on
Tokyo Night's dark ground) and a skinned canvas (a Label, a Button and a TextBox in
the skin's 11 px font, a 26 px Label) at scales 1 and 1.5, 448x160 each, read back
from both backends: grayscale 12 px, LCD rgb 12 px, LCD bgr 14 px and the canvas at
1.5 are byte-identical to GL's; SDF at 28 and 40 px differ in 2 and 1 pixels, by 1;
the canvas at scale 1 in 62 of 71 680 pixels (0.09 %), by 1, at the Button's and the
SDF Label's edges. The coverage and LCD paths do nothing but sample, multiply and
blend, which both rasterisers do alike; the SDF path's `smoothstep` over a filtered
field and its screen-space gradient is where the two shader compilers' float code can
round a value the other way.

**The apps on either backend.** ControlTest, the OpenGLCore example and `gweni_bench`
take `--renderer gl|vulkan` (or `GWENI_RENDERER`). What a run draws with is a
`capture::Device` (`examples/openglcore/capture.h`), made by `capture::openDevice`:
for GL the context the examples always made (a window, a hidden one for a capture,
or surfaceless EGL) with the offscreen `Framebuffer` a capture draws into; for Vulkan
a `VulkanDevice` from `createVulkanDevice` (validated with
`GWENI_VULKAN_VALIDATION=1`) and a `VulkanTarget`, with no GL or EGL context and no
window for a capture, so every capture is headless. The apps see only the `SlotRenderer` it makes
(`Device::newRenderer`): a frame starts with `clearTarget` (on GL the `glClear` it
replaces), a capture reads back with `readPixels` (`glReadPixels`), a resize remakes
the target (`Device::resizeTarget`) and the bench waits for the device with
`Device::finish()` (`glFinish`, `vkQueueWaitIdle`). So the whole-app checks run
unchanged on Vulkan: each is registered again as `<name>_vulkan`
(`gweni_whole_app_test` in `tests/CMakeLists.txt`), with `GWENI_RENDERER=vulkan`
in its environment, which the exes and every child a script starts take as their
`--renderer`. A run on Vulkan without a capture is the window, below.

### The Vulkan window

An interactive run on Vulkan (`OpenGLCore --renderer vulkan`, ControlTest without a
capture) gets its `capture::Device` from `openVulkanWindowDevice`
(`examples/openglcore/vulkanWindow.cpp`): a GLFW window with no client API
(`GLFW_NO_API`, `GLFW_SCALE_TO_MONITOR` as a GL window), a `VulkanDevice` made with
the instance extensions `glfwGetRequiredInstanceExtensions` names and
`VK_KHR_swapchain`, the window's surface (`glfwCreateWindowSurface`; GLFW opens the
same `libvulkan.so.1` volk does), and a FIFO swapchain of `B8G8R8A8_UNORM` (or
`R8G8B8A8_UNORM`) images, UNORM as a GL window's default framebuffer is, with
`TRANSFER_DST` and `TRANSFER_SRC` usage. The library has no swapchain in it:
renderer::Vulkan draws into the device's `VulkanTarget` as in a capture, the target
kept at the framebuffer's size, and showing a frame is the application's.

**One loop, three steps.** `Device::runWindowLoop(canvas, renderer, options, frame)` is
the loop both apps run on either backend (GL: `capture::runWindowLoop`, the browser's
included), on `input::GlfwLoop`, idle by default ("Threads, posts and the idle loop").
On Vulkan a frame is `beginWindowFrame()`, then `frame()` (update, clear, render,
which submits the renderer's frame) and `presentWindowFrame()`; when
`beginWindowFrame()` says the window is minimised the loop draws nothing and waits in
`glfwWaitEvents()`, and a frame not shown (an out-of-date acquire) marks the canvas
(`redraw()`), so the next iteration draws it.
`presentWindowFrame` waits for the frame in flight two frames back (its command
buffer, fence and acquire semaphore), acquires an image, and records: the target to
`TRANSFER_SRC` after the renderer's draws, the image to `TRANSFER_DST`, one
`vkCmdBlitImage` of the target turned over (the target's rows are bottom-up, GL's;
the image's top-down, as the window shows it), `NEAREST` and one to one, so every
pixel is copied as it is (RGBA8 into the 8-bit image, swizzled), the image to
`PRESENT_SRC` and the target back to `COLOR_ATTACHMENT_OPTIMAL` for the next frame's
draws. The submit waits for the acquire's semaphore at the transfer and signals the
image's own semaphore, which the present waits for; one per swapchain image, so it is
free again by the time the image is acquired again. On one queue every barrier is
ordered after the renderer's submits, so the blit reads the finished frame and the
next frame draws after the blit. With a `pixels` argument the presented image is
also copied into a host-visible buffer after the blit, which is how ControlTest's
`--window-test` reads back what was presented.

**Recreate.** The swapchain is made again in `beginWindowFrame`, after
`vkDeviceWaitIdle` (nothing in flight uses an old image, view or semaphore), when the
framebuffer is another size than the target (GLFW's framebuffer-size callback has
already run `fitCanvasToWindow`: the renderer's view and the canvas are the new size,
the content scale applied), and when an acquire or a present returned
`VK_ERROR_OUT_OF_DATE_KHR` (nothing acquired: the frame is not shown) or
`VK_SUBOPTIMAL_KHR` (shown, then made again). The target is made again first if the
size changed; the new swapchain is made with the old one as `oldSwapchain`, at the
surface's `currentExtent`. A frame between the surface changing and the recreate
blits the part of the target both share and clears the rest of the image to black.
Under Xvfb, lavapipe's X11 WSI answers each resize with one `VK_SUBOPTIMAL_KHR`.

**Minimise.** While the window is iconified (`GLFW_ICONIFIED`: on X11 the framebuffer
keeps its size) or its framebuffer is 0x0 (Windows), `beginWindowFrame` returns false:
no frame is drawn, nothing is acquired or presented, no swapchain is made at 0x0, and
the loop sleeps in `glfwWaitEvents` until the restore's events come. A frame begun
just before the window was iconified is dropped by `presentWindowFrame`
(`WindowStats::dropped`) rather than presented to the unmapped window.

**`window_vulkan`.** ControlTest's `--window-test <dir>` drives that loop one iteration
at a time (with `glfwWaitEventsTimeout` while minimised, so the script goes on), on the
stepped clock and with no input callbacks, so what it shows depends on the window's
sizes alone: it presents at `--size`, resizes itself (`glfwSetWindowSize`) to 5/4 and
4/5 of it and back, iconifies and restores (`glfwIconifyWindow`, `glfwRestoreWindow`),
reads back the image presented at each size and after the restore, and prints the
swapchain's counts (`Device::WindowStats`). `tests/windowVulkan.cmake` runs it under
`xvfb-run -a` and `gweni_test_wm` (`tests/vulkan/testWindowManager.cpp`): Xvfb has no
window manager, and without one an iconify (an ICCCM `WM_CHANGE_STATE` message to the
root) does nothing, so the test window manager unmaps the window and sets its
`WM_STATE` to iconic, which GLFW reads as iconified. The five images must be byte for
byte `--headless` captures at their sizes, every resize must have recreated the
swapchain, and the iconified half second must draw and present nothing. Under Xvfb the
content scale is 1; the scale path in a window is `fitCanvasToWindow`'s, the same as
GL's, and a real HiDPI display is not tested.

### Vulkan's counters and pixels against GL's

Two tests hold a Vulkan run of the apps to the GL run of the same tree
(`tests/backends.mjs`, run on node, labels `gpu` and `vulkan`):

- **`backend_counters`**: every `gweni_bench` scene at N=50 for 3 frames, every
  `ControlTest --list` control with `--headless --stats` (one process each, Demo
  included) and `OpenGLCore --headless --frames 3 --stats`, on both backends: every
  counter of every frame and summary line must be the same, slots written, uploads
  and their bytes, atlas uploads, draws, text draws, regrows, the change, render,
  layout, measure, z and hit-test counts and the live ids. They are counted by the
  core, so a difference is the core taking another path on one backend (a hook
  reporting another limit, a frame the backend made the core redo). Left out, with
  the reason in the script's `EXCLUDED`: the times, the bench's `allocs*`/`alloc_bytes*`
  (C++ allocations in a frame, which count the backend's own: Vulkan's command
  recording and the validation layer's in the flush, its target resized in the input
  phase of `resize` and `scale`, the atlas textures it makes for text laid out at a
  new scale in their render phase) and the summary's `renderer=`/`device=`. It also
  checks, on Vulkan, that every settled frame (a ControlTest frame after the first
  that changed and rendered no control, every measured frame of the `idle` scene)
  writes and uploads nothing.
- **`backend_pixels`** and **`backend_pixels_scale200`**: `ControlTest --headless
  --shot-all` and the hero (`OpenGLCore --headless --demo ShowcaseContainersDemo
  --screenshot`, as `tools/capture_samples.sh` takes it), at scale 1 and 2, on both
  backends, every PNG compared by `tests/web/pngdiff.mjs` (the browser's own bound
  is that script's too): no channel of any pixel more than 1 from GL's, and at most
  0.01 % of a PNG's pixels differing at all. On a failure it keeps its `mktemp -d`
  directory, with `diff/<name>.diff.png` (GL, Vulkan and the difference side by side)
  for each PNG over the bound.

**The bound is measured.** The feature's ceiling is 2 and 0.5 %; the bound is the
measurement rounded up (2026-11-09, lavapipe against llvmpipe, Mesa 23.2.1, the 51
`--shot-all` PNGs and the hero): at scale 1 two PNGs differ, in 9 pixels (the worst
TextSizesDemo, 0.003 %), at 1.5 five, at most 0.002 %, at 2 36 of the 52 in 1 to 82
pixels, at most 0.007 % (LabelMultilineDemo); every one by 1. They are edge pixels
of SDF glyphs (at scale 2 up to 8 pixels of a glyph's horizontal stroke on one row),
where a value lands on an 8-bit tie and the two drivers round it apart; no gradient
row differs. Some
are the blend's: one pixel is `109,114,142` on GL and `109,115,141` on Vulkan, half
coverage of `192,202,245` over `26,27,38`, so G and B are exactly 114.5 and 141.5 and
come out rounded in opposite directions, which no alpha a level off would give.
Others move every channel by 1, as a coverage a level apart would. Spelling
`smoothstep` out, as `mix()` is below, changes none of them, so no shader change
removes them. There are more at scale 2, where the skin's 11 px text is in the SDF
range.

The first measurement was not within the ceiling: up to 0.875 % of a page differed
(ShowcaseContainersDemo, TreeControlDemo 0.732 %, ShowcaseButtonsDemo 0.572 %), all
by 1, on two rows of every vertical gradient (style 4, a TreeControl node's, a
Button's) and the text over them. Reading the operands back as bits showed the
gradient's `t` the same on both and `mix(VertexColor, vColor1, t)` not: Mesa's GLSL
compiler and lavapipe (from glslang's `FMix`) lower `mix()` to different arithmetic.
Of ButtonDemo's 1 510 gradient pixels where the result differed, GL's was
`a + (b - a)*t` in 1 315 and Vulkan's `a*(1 - t) + b*t` in 1 268, and on a row where
the value falls on an 8-bit tie the two round apart. Written out in either form, each
was bit-identical on both backends. The shared fragment bodies now spell it out as
`a + (b - a)*t` (`lerp()` in `shaders/primitive.frag.glsl` and `text.frag.glsl`), so
every backend computes the same, and GL's own pixels did not change (every
`--shot-all` PNG and the hero, and `docs_samples`, byte for byte). A `mix()` in a
shader body is a tie waiting to happen; use `lerp()`.

**Style 4's gradient is an exact integer level per row.** `lerp()` made both backends
compute the same expression, not the same operands. Row r of an H-device-px rect
(counted from the top colour's edge) has its centre at `t = (2r+1)/(2H)`, so its exact
value is a tie, x.5 of a level, wherever `(bottom-top)*(2r+1)/(2H)` is a half-integer:
an odd difference over an odd height on the middle row, a difference of 3 at H 3 or
16 at H 16 on every row. A float gradient handed to the framebuffer's UNORM conversion
lands exactly on such a tie, and there one ulp decides the level. #289's first
ThemeEditorDemo showed it: ButtonNormal `#3D59A1 -> #2C3D6B` over 15-unit headers puts
their middle row's red on 52.5, and 1 950 pixels of it (0.635 %, `backend_pixels`
failed) were 53 on GL and 52 on Vulkan, not even whole rows, since the error moved
along x. The ulp was the interpolated `vLocal`'s, which llvmpipe and lavapipe
interpolate a little differently: the top colour made exact changed nothing, `t`
made exact from the row took the difference to 1 pixel. So `primitive.frag.glsl`
(`gradientRow()`, `gradientLevel()`) works the level out in ints: the row is
`floor(vLocal.y + vHalf.y)` (r + 0.5 at a pixel centre, so an interpolation error
under half a pixel cannot move it), H is `int(2*vHalf.y + 0.5)`, the colours' bytes
are `ivec4(c*255 + 0.5)` (exact: the vertex shader unpacks byte/255), and each channel,
alpha included, is

    (top*2H + (bottom-top)*(2r+1) + H) / (2H)

the row centre's value rounded half up, the dividend non-negative and under 2^24. The
division is a float quotient of the two exact operands, at most a level off, put right
with two integer compares: an integer division of each channel took llvmpipe 6.4 %
more instructions in a frame of `gweni_bench`'s `idle` scene, this takes 2.9 %. The
level then goes through `roundedRect()` (border, edge coverage) and the blend as
before, so every backend, GL, GLES, Vulkan and WebGL, writes the same level on every
interior pixel of a gradient whatever its float arithmetic (`tests/gradientRows.cpp`
holds every row of heights 1 to 100 to the formula, at scales 1 and 1.5). GL's own
pixels moved by 1 where its float had rounded a tie down (about half of them), where
it had left a row uneven along x, and where a pixel blends the fill (a corner arc, a
translucent surface), whose fill is now its exact level rather than a fraction off
it; `docs/images/` was regenerated with it. Style 6 (`drawBilinearRect`) takes the
row's exact `t` (`gradientT()`) for its y direction; its x direction is still
interpolated, since an exact one needs all four corner colours passed flat.

## OpenGLCore renderer (retained model)

`source/platforms/renderers/OpenGLCore/openglCore.cpp` implements the retained model
on an OpenGL 3.3 core profile, as `SlotRenderer`'s GL backend: the core above packs
and diffs the slots, and OpenGLCore holds the buffers, textures, vertex arrays, the
two programs and the draws. What follows describes both together:

- **One GL subset.** Every GL call and every line of GLSL keeps to what GL 3.3 core,
  GLES 3.0 and WebGL 2 all have, so one code path serves a desktop context, a GLES
  one and the browser. That rules out what llvmpipe's 4.5 used to let through behind
  the 3.3 request: no direct state access (`glCreate*`, `glProgramUniform*`,
  `glVertexArray*`; objects are `glGen*`'d and bound to be edited, and a uniform is set
  with `glUniform*` in the program in use), no buffer textures, no `gl_PrimitiveID`,
  no `texture2D`, no `glGetTexImage` or `glGetTexLevelParameteriv` (a texture is read
  back through a framebuffer it is attached to, `OpenGLCore::readTexture`, and its
  format seen there, `textureChannelBits`), no `glClearDepth` (the tests leave the
  clear depth at its default 1), `glReadPixels` only as `GL_RGBA`/`GL_UNSIGNED_BYTE`,
  textures with a sized internal format that matches their data (`GL_RGB8` for RGB
  images), and a buffer is only ever bound to one target (the vertex buffers to
  `GL_ARRAY_BUFFER`, the quad index buffers to `GL_ELEMENT_ARRAY_BUFFER`), as WebGL
  requires. Two things beyond it are used where the context has them, found by
  `init()` (`OpenGLCore::glCapabilities()`, from `GL_VERSION` and the extension list):
  `glMultiDrawElements` for a chunk's ranges, the text's and the primitives' (core in GL 3.3; on GLES the
  `GL_EXT_multi_draw_arrays` one, `glMultiDrawElementsEXT`, whose entry point the GLES
  tree gets from `eglGetProcAddress`, since `libGLESv2` exports only the core ones;
  without it, each range of a draw is a `glDrawElements` of its own, which draws the
  same pixels and counts the same `FrameStats`; `GWENI_NO_MULTI_DRAW=1` at
  construction, or `setUseMultiDraw(false)`, takes the loop where the context has it
  too, `tests/textDrawLoop.cpp` and `tests/primitiveDrawLoop.cpp`), and dual-source blending for LCD text (core in GL
  3.3, `GL_EXT_blend_func_extended` on GLES). Without dual-source blending the text
  program has no second output and the LCD modes are grayscale: `OpenGLCore` answers
  `SlotRenderer::lcdTextUnavailable()` with why, `setTextAA(LcdRGB/LcdBGR)` (and a
  `GWENI_TEXT_AA=rgb|bgr` read at construction, which `init()` re-applies through
  `applyLcdTextSupport()`) leaves the mode `Grayscale`, and the renderer logs one
  warning the first time an LCD mode is asked for (`LCD text needs dual-source
  blending (GL_EXT_blend_func_extended), which this context does not have, using
  grayscale`), never again from that renderer. `GWENI_GL_NO_DUAL_SOURCE=1` at
  construction makes `init()` take any context to have none, so the fallback is
  checked in every tree (`tests/noDualSource.cpp`).
- **The GLES tree.** `./generate.sh linux gles` (`GWENI_GL_ES=ON`) builds `OpenGLCore`,
  the examples, the tests and the bench against `<GLES3/gl3.h>` (with
  `<GLES2/gl2ext.h>` for the extensions' enums) and `libGLESv2`, with no glew, so a
  call newer than GLES 3.0 does not compile. The choice is made in one header,
  `include/gweni/platforms/renderers/glApi.h`, which every file calling GL includes
  (`GWENI_GL_ES` is a public compile definition of `gweni`, and Emscripten defines it
  by itself there); freetype-gl gets the same switch from a gweni patch
  (`freetype-gl_WITH_GLES3`, `FREETYPE_GL_GLES3` in its `opengl.h`), and
  `GLFW_INCLUDE_NONE` keeps `<GLFW/glfw3.h>` from bringing the desktop `<GL/gl.h>`
  back. The examples' context (`examples/openglcore/capture.cpp`) is GLES 3.0 there: the
  surfaceless EGL one binds `EGL_OPENGL_ES_API` with an `EGL_OPENGL_ES3_BIT` config,
  and a window asks GLFW for `GLFW_OPENGL_ES_API` 3.0; `capture::initGL` loads
  nothing (glewInit on desktop). In every tree `initGL` checks a window's framebuffer,
  and `createFramebuffer` the offscreen one, for a 24-bit depth buffer, failing with one
  line if it has less: `OpenGLCore` spreads its 65536 z values over the depth range. On
  Mesa's llvmpipe the GLES 3.0 context has both `GL_EXT_multi_draw_arrays` and
  `GL_EXT_blend_func_extended`, so it draws through the same calls as the desktop
  trees and its tests, `docs_samples` byte for byte, pass as theirs do;
  `tests/glslDialects.cpp` compiles the ES bodies there natively and skips the
  `#version 330 core` ones a GLES context cannot compile.
- **In the browser.** The web tree (`./generate.sh web`) compiles the same `OpenGLCore`
  against Emscripten's `<GLES3/gl3.h>` (`glApi.h` defines `GWENI_GL_ES` under
  `__EMSCRIPTEN__`) and links Emscripten's WebGL 2 library. A context there is WebGL 2
  on the page's `#canvas`, made by `emscripten_webgl_create_context`
  (`capture::initEglContext`, `GWENI_EXAMPLE_WEBGL`). Its `GL_VERSION` reads `OpenGL ES
  3.0 (WebGL 2.0 ...)`, so `init()` takes the ES dialect as on the GLES tree. There is no
  multi-draw (`OpenGLCore` resolves `glMultiDrawElementsEXT` only through EGL, in the GLES
  tree) and no `GL_EXT_blend_func_extended` (WebGL's is `WEBGL_blend_func_extended`, which
  `OpenGLCore` does not use yet), so the primitive and text draws loop and LCD text is grayscale. In the
  build container the context is ANGLE on Chromium's SwiftShader, a CPU rasteriser
  like llvmpipe but another one, so its pixels are close to the native GLES tree's, not
  the same (see "The browser against native" below).
  A capture is one blocking `main`: it renders into the offscreen framebuffer and
  reads it back with `glReadPixels`, which WebGL does synchronously, so it needs no
  main loop. The interactive window is emscripten-glfw's (Emscripten's
  `contrib.glfw3` port) on the same `#canvas`, with a WebGL 2 context of its own
  (`capture::initGlfwContext`: `GLFW_CONTEXT_VERSION_MAJOR` is the WebGL version there;
  24 depth bits, no alpha, so nothing behind the page shows through a translucent
  blend, no stencil, no multisampling). A browser cannot run a blocking loop and still
  draw or deliver input, so `capture::runWindowLoop`, the one loop both apps' windows
  run, is `emscripten_set_main_loop_arg` there: one callback per animation frame, each a
  `glfwPollEvents()` (where emscripten-glfw applies a resize; input arrives between
  callbacks) and, when the canvas has work or a post asked for a frame, the frame
  (`updateCanvas`, `glClear`, `renderCanvas`, as natively, where the loop adds the swap
  and the poll); a callback with nothing to do returns at once (`input::GlfwLoop`,
  "Threads, posts and the idle loop"). It never returns, and simulating the
  infinite loop unwinds `main`'s stack before the first frame, so the frame holds only
  heap objects (the canvas) and statics (the GLFW translator). The browser presents
  the canvas after each callback; emscripten-glfw's `glfwSwapBuffers` only logs that it
  is not implemented, so the web frame does not call it.
- **What the browser lacks, and what it maps to.** Each is said once on stderr (the
  page's output panel), never silently, or maps to the browser's own equivalent
  (`source/platforms/crossPlatform.cpp`):
  - the cursor: `platform::setCursor` sets the CSS `cursor` of the page's canvas
    (`Module.canvas`), `default`, `text`, `ns-resize`, `ew-resize`, `nwse-resize`,
    `nesw-resize`, `move`, `not-allowed`, `wait` or `pointer` for each `cursor::` kind;
  - the file dialogs: one line saying the web build has none, and `fileOpen`,
    `fileSave` and `folderOpen` return cancelled;
  - the clipboard: the browser's in the interactive page (see "The clipboard" below);
    in a headless page capture an in-process buffer, said once when it is first used;
  - LCD text: no dual-source blending, so grayscale, with the one warning of "GLES
    tree" above.

  Touch, pointer events and an IME are not handled: a page loads on a phone, but only
  the mouse and the keyboard drive it. Building for wasm32 also showed two struct layouts that only agreed on
  x86-64 by luck, both fixed: freetype-gl's `vertex_buffer_t` with and without
  `FREETYPE_GL_USE_VAO` (now a PUBLIC definition of the `freetype-gl` target), and
  `json.h`'s `returnValue<uint64_t>`, which on x86-64 was the `size_t` specialisation.
- **The browser against native.** The web tree's `web_*_native` tests
  (`tests/web/native.cmake`, run by `tests/web/native.mjs`) hold the browser to the
  native GLES tree of the same build type, `GWENI_NATIVE_REFERENCE_DIR` (which
  `./generate.sh web` sets): they run the native exes themselves, each under the
  `render` lock through `mm` when it is on `PATH`, and fail, naming the path and the
  commands, when the tree is not built. Layout and change tracking are the same code on
  every backend, so they are held to identity: every `--dump-bounds` listing at scales
  1, 1.5 and 2 (`web_bounds_native*`), and every `--stats` counter of every control and
  of OpenGLCore, times left out (`web_stats_native`), the draws' loops included,
  since a loop of `glDrawElements` counts as the one multi-draw it replaces (and both
  paths build the same ranges, with one gap and cap). Inside the
  browser two page loads give the same bytes (`web_controltest_determinism`, through
  `tests/determinism.cmake`). Pixels are held to a tolerance (`web_pixels_native` at
  scales 1 and 2, `tests/web/pngdiff.mjs`): per PNG, no channel more than 2 off and at
  most 0.1 % of the pixels more than 1 off. Pixels 1 off are not counted: two
  rasterisers round a blend or a float-to-8-bit conversion differently, on a text or
  gradient pixel near a rounding boundary, up to 10.5 % of a text-heavy page
  (LabelMultilineDemo), and no shader can remove that. What is counted measured 0.021 %
  at worst (`native.cmake` has the table), against a ceiling of 2 and 0.5 %: a PNG
  over it is a bug to find, not a bound to widen. Leaving the 1-off pixels out of the
  count (and ImagePanelDemo at scale 1 out until #418) was the user's decision on
  #382, since no shader can meet the first bar, which counted every differing pixel.
  Getting there took five shader
  changes, each something GL leaves to the implementation, which had put 14 demos up
  to 82/255 apart at icon edges and 78/255 at SDF-text ones:
  - the checkmark and chevron (icon kinds 0 and 1) take their anti-aliasing width as
    one device pixel in the stroke's units, not `fwidth(d)`: a derivative of a value
    that is not linear is computed fine or coarse, per 2x2 block, as the GL likes. Their
    `vLocal` is rounded to the multiple of 0.5 it is at every pixel centre (the quad's
    corners are whole px), so its interpolation error, which depends on the rasteriser
    and on where the quad is, does not move a level at the stroke's edge either;
  - SDF text's edge width is the field's change per texel (`1/(2*kSdfSpread)`) times
    `texCoord`'s screen gradient in texels, not the sampled field's own gradient, for
    the same reason; `texCoord` is linear over the quad;
  - the SDF field is filtered in the shader, bilinearly in float from four
    `texelFetch`es, not by the texture unit, whose weights keep a few bits of the
    implementation's choosing, which the narrow edge multiplied by up to ten;
  - text quads are put on a 1/16 px grid in the vertex shader, the coarsest sub-pixel
    grid GL guarantees (`GL_SUBPIXEL_BITS` is at least 4), so no rasteriser snaps them
    further: an SDF glyph's pen is fractional, and each rasteriser's own grid moved the
    field by up to 1/32 px, 10/255 at an edge.

  - an image (style 2) is filtered in the shader, from mip levels made on the CPU
    ("Images: mip levels on the CPU, filtered in the shader" below), not through
    `glGenerateMipmap`'s levels and `textureGrad`, whose downsampling filter, LOD and
    trilinear weights are the implementation's: ImagePanelDemo, minified 8.96 times,
    was 4 off on one pixel at scale 1 and left out of the comparison until #418; it is
    now byte-identical at scales 1, 1.5 and 2.

  No PNG is left out. The comparator is `pngdiff.mjs`, which
  needs node alone, so another backend held to the first within a tolerance (the Vulkan
  feature) can share it; a PNG over the bound gets an expected | actual | difference
  image beside it.
- **The shaders' dialect is chosen at run time.** Each stage is one body shared by
  every dialect, and by the Vulkan backend ("renderer::Vulkan" below): a file of its
  own in `source/platforms/renderers/shaders/` (`primitive.vert.glsl`,
  `primitive.frag.glsl`, `text.vert.glsl`, `text.frag.glsl`), embedded in the library
  at build time (`cmake/embedFile.cmake`). `OpenGLCore::shaderSource` puts a header in
  front: `#version 330 core`, or for an ES context `#version 300 es` with `precision highp`
  for `float`, `int`, `sampler2D` and `usampler2D` (ES's fragment defaults, mediump
  int and lowp samplers, would not give the desktop bytes), plus the clip-box
  texture's width (`CLIP_COLUMNS`) and, for the text program with dual-source
  blending, `GWENI_DUAL_SOURCE` (and on ES `#extension GL_EXT_blend_func_extended`),
  then GL's prologue: what a body leaves to its backend, each file's header listing
  it. That is the uniforms (`projection`, `slotBase`, the samplers, `textMode`,
  `textContrast`, found by name), `VERTEX_ID` (`gl_VertexID`) and
  `CLIP_SPACE_TO_API(p)` (nothing on GL).
  The bodies use no implicit conversion and nothing else GLSL ES 3.00 lacks:
  `tests/glslDialects.cpp` compiles and links every program as ES 3.00 in the desktop
  context (`GL_ARB_ES3_compatibility`), and without its dual-source output.

- One vertex buffer (`m_vertexBuffer` / CPU mirror `m_vertexes`), drawn over one
  **static 16-bit quad index buffer** (`m_quadIndexBuffer`): `kSlotsPerChunk` (16383)
  quads of `v, v+1, v+2, v+1, v+3, v+2` with `v = i*4`, filled once by `init()`. A
  slot's indices never change, so there is no index mirror and no index upload. Text
  has its own freetype-gl-backed vertex buffer (`m_textVertexes`), drawn the same way
  over a second static buffer (`m_textQuadIndexBuffer`) of freetype-gl's glyph quads,
  `v, v+1, v+2, v, v+2, v+3`: its quads go round their corners, so each path keeps
  the diagonal its quads were always split along.
- **A primitive id is a fixed slot in the vertex buffer**: its vertices live at `id*4`
  (a quad). `generatePrimitive()` pulls a free id from a bank
  (`getPrimitiveId`, `include/gweni/primitiveIds.h`) and grows the CPU buffers.
- **Chunked draws.** A 16-bit index of `id*4` cannot reach past slot 16383, so the
  slots are drawn in chunks of at most `kSlotsPerChunk`: chunk `c` has a vertex array
  of its own (`primitiveVertexArray(c)`, made on first use) whose attributes start at
  slot `c*kSlotsPerChunk`'s first vertex, and is one `glDrawElements` of its slots'
  `count*6` indices from the start of the static buffer, in ascending slot order, so
  blending and depth are those of one draw over the whole buffer. Today's scenes are
  one chunk, one draw. The glyph slots are chunked alike (`textVertexArray(c)`; see
  "One text draw per font atlas" below). 16383 rather than 16384 keeps the largest index at 65531:
  WebGL 2 always restarts the primitive at `0xFFFF`. No `glDrawElementsBaseVertex` and
  no 32-bit indices, so this holds on GLES and WebGL. `GWENI_INDEX_CHUNK_SLOTS=<n>`
  (read at construction; `setIndexChunkSlots()` in tests) makes the chunks `n` slots,
  which must draw the same pixels; `controltest_index_chunks` checks every
  `--shot-all` PNG and the hero `--screenshot` byte for byte at 64, and
  `tests/indexChunks.cpp` slots past 16383, chunks of 1 to 5 slots, and text runs
  split between chunks, hidden and shrunk.
- **Only the slots that can show are drawn** (#431, #508). A slot can show when its
  style is not 0; `hidePrimitive` and `releasePrimitive` set it to 0, and a slot the
  mirror grew by is 0 until drawn. The mirror never shrinks, so before, every frame
  drew every slot up to its peak: a hidden page's slots and the ids a deleted tree gave
  back each went through the vertex shader as 4 vertexes that made nothing.
  `SlotRenderer` keeps a bit per slot (`m_visibleSlots`) and, per chunk
  (`m_primitiveChunks`, a `PrimitiveChunkSlots`), how many of its slots can show and
  the `ChunkDraw` it is drawn with: its **runs** of slots that can show, a gap of
  fewer than `primitiveRangeGap()` (`kPrimitiveRangeGap`, 2) hidden slots drawn with
  the runs around it, and at most `maxPrimitiveRanges()` (`kMaxPrimitiveRanges`, 16)
  ranges, the smallest gaps (the leftmost of equal ones) merged past that.
  `flush()` skips a chunk with none, and the pixels are those of the whole chunk (a
  style-0 slot makes no fragment, the ranges are in slot order, and each index is
  still `slot*4+k` from the chunk's first slot, so `gl_VertexID >> 2` and `slotBase`
  are unchanged). The bits are brought up to date from the dirty slots alone, before
  the upload clears them (`updateSlotVisibility`): every write that changes a slot's
  bytes (a `draw*`, a hide, a show, a release, a quad collapsed out of range, a scale
  or font change redrawing it) marks it for the upload anyway, so this costs O(slots
  changed), with no scan of the mirror. A bit that changes marks its chunk's ranges
  stale, and a stale chunk's are built again (`buildPrimitiveRanges`, a word at a
  time over its at most 256 words) only when it is drawn; `setIndexChunkSlots()`
  recounts the chunks from the bits and `setPrimitiveRangeLimits()` (or
  `GWENI_PRIMITIVE_RANGES=<gap>,<cap>` at construction: two unsigned decimal counts
  that fit a `size_t`, the cap at least 1, anything else reported on stderr and left
  out; `gweni_bench --range-gap/--range-cap` override it one limit at a time) marks
  them all stale. So a
  frame that shows or hides nothing builds no range list
  (`FrameStats::primitiveRangeBuilds`), whatever else it writes.
  `FrameStats::primitiveSlotsDrawn` is the slots the ranges cover,
  `primitiveRanges` the ranges, `drawCalls` the chunks drawn (one each, multi-draw or
  loop), and `GWENI_UPLOAD_STATS=1` prints `N of M primitive slots drawn in R ranges
  (B chunks' ranges built)`; `gweni_bench` reports them as `primitive_slots_drawn`
  and `primitive_ranges`. Slots are not moved (see "free-slot compaction" under "Not
  yet done") and ids are handed out lowest first, so before the runs, when a chunk's
  draw ended at its last shown slot, a control that took a slot after the rest (a
  hover surface, a newly shown page) held every hidden slot below it in the draw.
  At 1024x768 (`GWENI_UPLOAD_STATS=1`; the slots that can show counted with
  `GWENI_PRIMITIVE_RANGES=1,100000`, which merges and caps nothing):

  | | up to the last shown slot (#431) | the runs (#508) | slots that can show |
  | --- | --- | --- | --- |
  | `ControlTest --control TabControlDemo` | 46 of 52 | 30 of 52, 5 ranges | 23, in 12 runs |
  | `--control Demo` | 3726 of 4158 | 311 of 4158, 16 ranges | 240, in 67 runs |
  | `Demo`, after `--click 72,85` (Buttons & toggles) and `--click 70,66` (back to Containers & data) | 4174 of 4174 | 337 of 4174, 16 ranges | 260, in 71 runs |

  The limits were measured, not guessed, in the release tree under `render` and
  `quiet` with `gweni_bench --scene hidden-gaps` (N Buttons, all hidden but every
  (K+1)-th, so its slots are runs of 1 between gaps of K), N=16000, each gap left a
  range of its own (`--range-gap 0`) or every gap merged into one range, with
  multi-draw and with the loop (`GWENI_NO_MULTI_DRAW=1`); `flush_us` medians of the
  quietest paired run:

  | gap K | ranges kept: loop / multi-draw | gaps merged, one range: loop / multi-draw |
  | --- | --- | --- |
  | 1 (8000 ranges) | 7035 / 4714 | 6519 / 4730 |
  | 2 (5334 ranges) | 4674 / 3566 | 5951 / 5093 |
  | 3 (4000 ranges) | 2913 / 1776 | 3780 / 3369 |

  A hidden slot drawn costs about 0.15 µs on llvmpipe, a range about 0.02-0.07 µs
  with multi-draw and 0.1-0.3 µs more in the loop, so merging pays only for a gap of
  1, and only in the loop: `kPrimitiveRangeGap` is 2. The cap of 16 bounds a chunk's
  loop of `glDrawElements` (the web's draw, which has no multi-draw: one G and R for
  every path, so `web_stats_native`'s counters match the GLES tree's) and costs
  little where it bites: Demo draws 337 slots in 16 ranges against 300 in 31
  uncapped. With K=15 the cap of 16 drew 15760 of 16000 slots in about 2670 µs,
  against 1000 slots in 1000 ranges in about 490 µs uncapped (multi-draw), a case no
  UI makes. `gweni_bench --scene hidden-pages --n 10000` (K=10 hidden pages of N
  Buttons between a header and a footer row built before and after them, 40 shown)
  is the case the runs are for: in the release tree, 6 runs each (pooled medians,
  `render` and `quiet`), the whole mirror (`--draw-slots every`) drew 10050 slots in
  `flush_us` 593 + `gpu_us` 242, the runs 40 slots in 2 ranges in 104 + 244, 57 %
  less; the draw up to the last shown slot drew the same 10050 as the whole mirror
  and, in #508's go/no-go measurement on a busier host, took about as long (`flush_us`
  + `gpu_us` 1833 µs, the whole mirror 2042, the runs 1030). The saving is all in `flush`: llvmpipe runs the vertex work of
  the draw on the calling thread. `bench_scaling` pins both counters for
  `hidden-pages`, whose shown part is fixed (they follow the shown slots, so in the
  other scenes they grow with N). `tests/indexChunks.cpp` checks the counts against a
  model of its own and that every frame draws the pixels of the whole mirror
  (`setDrawEveryPrimitiveSlot`): a hidden middle chunk, a hidden tail, a hidden run
  inside a chunk whose last slot shows (800 of 1000 slots in 2 ranges), a 1-slot gap
  merged, 22 runs capped to 16, released top ids, a regrow, a new chunk size, and no
  range list built by a settled frame or one that only recolours a slot;
  `tests/primitiveDrawLoop.cpp` (`gl_only`) that the loop draws what multi-draw draws.
- Each `Vertex` is 16 16-bit fields, 32 bytes:
  `{ style, x, y, z, tx, ty, lx, ly, radius, border, cx, cy, bx, by, dx, dy }`.
  `x, y` are the corner's device-pixel position, **signed** (`int16_t`, read as an
  `ivec3` with `z`), so a rect that straddles the view's left or top edge keeps its
  negative corner and draws exactly its on-view part; `z` is `uint16_t`, 0..65535
  (`SlotRenderer::kMaxZIndex`, what `maxZIndex()` reports; the shader takes its low 16
  bits, `inPosition.z & 0xffff`, since `GL_SHORT` sign-extends it), the projection's
  depth range: z=0 is depth 1.0, the clear value, and 65535 is depth 0. In a 24-bit
  depth buffer (the capture framebuffer's `GL_DEPTH_COMPONENT24`, and the window's
  through an explicit `GLFW_DEPTH_BITS` hint) that is 256 depth steps per z, so the
  glyphs of a run longer than about 254 glyphs (glyph i at `z+(i+1)/(glyphs+1)`)
  share depths: the text shader discards every pixel a glyph does not draw, in every
  mode, so a glyph's box writes no depth over the next glyph's ink. A quad is drawn only if it fits in ±32 K device px:
  `cullOutOfRangeQuad`, called by every quad writer (`drawFilledRect`,
  `writeRoundedQuad`, `drawIcon`, `drawTexturedRect`), puts all four corners of a
  quad with a corner outside `[-32768, 32767]` at 0,0, a zero-area quad that draws
  nothing and keeps its `style` and other fields (so `hide/showPrimitive` work and an
  unchanged one writes the same bytes). It never clamps a corner: that would move it,
  and the interpolated UV, colour and `vLocal` with it, and the flat `vHalf` (the SDF
  half-size, taken from the provoking corner). The limit matches the SDF's `lx/ly`,
  which already hold no more than 32767. With unsigned positions a corner at -1 was
  stored as 65535, and a rect straddling the edge drew as a bar out to the clip box's
  edge (#170, #252); a long list's rows past 32 K device px are now culled instead of
  wrapping.
  `tx/ty` is color0 (or the UV of style 2), `cx/cy` color1, `bx/by` the border color
  and `dx/dy` color3, each an RGBA color packed into two halves (`Color::rg()/ba()`).
  The shader reads color0 and color3 as smooth (interpolated) varyings and color1 and
  the border color as flat ones. `lx/ly` (the local position from the rect's centre,
  in half-pixel units: `±w`/`±h` at the corners, halved by the vertex shader, so an
  odd size's half is exact and the quad is exactly the rect), `radius`, `border`, color1, the border color and color3 are 0 except for the styles
  that use them. `style` selects the shader path: `0` = hidden (degenerate), `1` = solid
  color (color0), `2` = image (`drawTexturedRect`, below), `3` = line segment (`drawLine`, below; it
  was a text glyph once, and text has its own program), `4` = SDF rounded rect
  (`drawRoundedRect`: vertical gradient color0 → color1, border), `5` = SDF icon glyph
  (`drawIcon`; `radius` = kind, `border` = direction: kinds 0 checkmark and 1 chevron
  in coords normalized to the rect, with a `smoothstep` edge; kinds 2 cross, 3 dash,
  4 square outline and 5 restore, the window buttons' glyphs, in device px over the
  square centred in the quad, with the stroke `max(1, snap(1))` packed in the high
  byte of `bx` and the box-filter coverage below at a constant `aa`, so the cross is
  mirror-symmetric pixel for pixel and every edge of 3-5 is a whole px from the
  quad's corner: their strokes are whole rows and columns, fully covered; kind 6 is
  a separator's rule, the stroke the quad's whole length through the floor of its
  middle, horizontal for direction 0 and vertical for 1, so a rule is
  `max(1, snap(1))` px thick wherever it lands, where a 1-unit rect snapped by
  `translate()` is 1 or 2 px by position at 1.25 and 1.5; a style 5 pixel with less
  than half an 8-bit step of coverage is discarded, writing no depth, since the
  textured Separator's quad reaches one unit past it over its neighbours; kinds 7 and 8
  are a filled triangle and its outline, the stroke thick, in the chevron's normalized
  frame and pointing its way, a node graph's pin markers, the largest of the three sides'
  distances; kind 9 is `drawArc`'s arc, below), `6` = SDF bilinear fill (`drawBilinearRect`: each vertex carries its
  own top-edge color in color0 and bottom-edge color in color3; both are linear in x,
  so the blend along y is exact, with no crease on the quad's diagonal), `7` = SDF
  checker composite (`drawCheckerRect`: color0 with its alpha over 4 px cells of
  color1 (light) and color3 (dark), output opaque, so it does not depend on draw
  order), `8` = grid lines (`drawGrid`: a line every spacing px from a phase on each
  axis, a whole-px stroke where `drawLine` would put one across that coordinate, every
  `majorEvery`-th in color3 and the rest in color1, a pixel on no line discarded; the
  spacing a float's bits in tx/ty and the phases 1/16 px in bx/by, unpacked by the
  vertex shader into the line style's `vSegment`/`vJoints`; one slot for a NodeGraph's
  whole grid, `tests/grid.cpp`), `9` = trapezoid (`drawTrapezoid`, a chart area's
  segment: "Trapezoids (style 9)" below). Styles 4, 6 and 7 share the rounded mask, anti-aliasing and border
  (`roundedRect()` in the fragment shader). The edge and the border are a 1 px box
  filter: coverage `clamp(0.5 - d/aa, 0, 1)` and border mix
  `clamp(0.5 + (border + d)/aa, 0, 1)`, with `aa` the pixel's size from `vLocal`'s
  derivatives (not `fwidth(d)`, which is √2 too wide on a corner diagonal and at a
  square corner's kink). A pixel-aligned straight edge is therefore 1 inside and 0
  outside: the outermost pixel is fully covered, two rects that abut meet with no
  seam, and an N px border is exactly N rows; only a corner arc is partly covered.
  A pixel the shape leaves uncovered (outside a corner arc, less than half an 8-bit
  step of alpha) is discarded and writes no depth, so a slot behind it drawn after
  it still shows there (a node graph's link made after the pin whose circle it ends
  in, `tests/uncoveredDepth.cpp`); a partly covered arc pixel still writes it, so
  there the result depends on slot order, as for an image's partly transparent texel.
  **The edge mask**: `drawRoundedRect`'s overload with an `edges` argument (a
  combination of `renderer::RoundedEdge`) makes each corner it names square and keeps
  the border off each side it names, as a tab joined to its page is square and
  unbordered on the page's side. `writeRoundedQuad` packs it in `Vertex::radius`'s high
  byte, so the vertex stays 32 bytes and the radius is its low byte, at most 255 device
  px (the shader cuts a radius to the rect's half-size anyway, so a larger one draws the
  same unless the rect is over 510 px on its shorter side); the vertex shader splits
  the field for styles 4, 6 and 7 only (`vEdges`, 0 for every other style, which keep
  the whole field). Bits 0-3 are the top-left, top-right, bottom-left and bottom-right
  corners square, bits 4-7 the top, right, bottom and left sides without a border.
  `roundedRect()` takes the radius of the pixel's quadrant (`cornerRadius()`, by the
  signs of `vLocal`, 0 where the corner is square), and in a quadrant bounded by a
  borderless side measures the border to the nearer of the shape and the straight box
  with that side pushed out by `border + aa`, so no pixel inside is within the border
  of that side and the borders of the sides next to it run straight to its edge; a
  quadrant with no such side keeps the shape's distance, its arc included (so a
  borderless side next to a corner that stays rounded loses the border along that arc
  too, a combination nothing draws). That is
  selects, not a branch: a branch around a second `sdRoundBox()` cost llvmpipe 4.6 % of
  `gweni_bench idle`'s flush instructions with no rect masked, and the selects 2.6 %
  fewer than before the mask (5,180 M before the mask, 5,417 M with the branch,
  5,046 M with the selects, at N=1000). With no bit set every step gives the value it
  gave before the mask, so an unmasked rect draws the same bytes. **The reach**: the
  masked overload's `reach` (0 by default) widens the quad, after `translate()`, by
  `max(1, snap(reach))` device px on each borderless side, the width a border of
  `reach` is drawn, so a rect drawn up to a neighbour's outer edge covers exactly that
  neighbour's border rows at any scale: a logical unit of overlap snaps to 1 or 2 px by
  its position at 1.25, 1.5 or 1.75, while a border is the same whole px everywhere.
  `renderer::Base`'s default drops it with the mask.
  `renderer::Base`'s default for the masked call draws the unmasked rect, the mask
  dropped, so a backend without it still draws a rounded rect in the right place;
  `drawBilinearRect`/`drawCheckerRect` take no mask (yet). `tests/roundedEdges.cpp`
  checks the pixels on every backend, `tests/unit/roundedEdges.cpp` the packing.
  `renderer::Base`'s defaults for `drawBilinearRect`/`drawCheckerRect` approximate
  them with `drawRoundedRect`. `hidePrimitive()`/`showPrimitive()` just set `style`,
  leaving geometry in place.
- **Line segments (style 3).** `drawLine(id, p0, p1, thickness, z, prev, next)` draws a segment
  with round caps (a capsule) in the draw colour, its endpoints in canvas units as
  `PointF`s, so a plot's samples can fall between whole units. The thickness is
  snapped like a border, `max(1, snap(thickness))` whole device px, and each endpoint
  coordinate is placed where its stroke across starts on a whole px,
  `floor(v*scale - T/2 + 0.5)` (for an even `T` that is `snap(v) - T/2`), so a
  horizontal or vertical segment is `T` whole fully covered rows (columns), like a
  rule. The quad is both endpoints' stroke squares and 1 px around them. `lx/ly` run
  over it in half-pixel units as for style 4, the first endpoint is stored the same way
  relative to the quad's centre in color1 (`cx/cy`) as `int16_t` bits (the vertex
  shader sign-extends it into the flat `vSegment`, whose second endpoint is its
  negation: the quad is centred on the segment), and `border` is `T`. The neighbour
  points a caller gives (`drawLine`'s `prev` and `next`, the polyline's points before
  `p0` and after `p1`) are placed as endpoints are, at this segment's `T`, and stored
  the same way in `bx/by` (previous) and `dx/dy` (next; without one, `dx/dy` holds the
  second endpoint, as before neighbours existed), with `radius` saying which it holds
  (`LineHasPrev`, `LineHasNext`); a neighbour 16383.5 px or more from the quad's centre
  on either axis, which `int16_t` half px cannot hold, is left out, and that joint is
  left to the depth rule below, as one without neighbours is. The fragment shader
  covers a pixel by the part of it between the stroke's two edges, each a straight edge across the pixel normal to the direction from the
  segment's nearest point (`pixelArea()`: the exact area of a unit square on one side
  of a line, linear in the middle of its `|nx|+|ny|` wide projection and quadratic at
  its corners). That is the pixel's exact area along the body of the stroke, so a thin
  slanted line is equally bright on every row (a 1 px box filter of the distance, style
  4's, is up to 12 % off per row at 45°), and for an axis-aligned edge it is style 4's
  box filter. A pixel with less than half an 8-bit step of coverage is discarded, as
  for style 5. **Depth at a joint:** the segments of a polyline share a z, and under
  `GL_LESS` the first segment's anti-aliased cap rim would keep the next one off
  pixels it covers fully, a lighter arc inside the stroke at every joint. So a partly
  covered style 3 pixel writes its depth up to half a layer behind its z,
  `mix(depth(z - 0.5), depth(z), coverage)` (`vBehindDepth`, the shader writing
  `gl_FragDepth`, which for every other style is the fixed function's
  `gl_FragCoord.z`): a later segment at the same z draws over the pixels an earlier
  one covers less, and nothing at a lower z does. A fully covered pixel keeps its z,
  so a translucent polyline is blended once there however many of its segments cover
  it, and only the rim of a cap's disc can be blended twice: where the first segment's
  coverage is just under full, by an amount the depth buffer's format and rounding
  decide (about 0.996 with D24). **Joints drawn with neighbours:** a segment given its neighbours decides
  each pixel they reach with them, so it is blended once whatever the depth buffer,
  compiler or backend. The segment nearest the pixel's centre draws it, ties (the disc
  both segments end at) going to the earlier one, and the others discard it. Every slot
  must agree on that to the bit, so the centre is `vLocal` rounded to its half-px grid
  (exact at a pixel centre) and each squared distance is a function of the centre less
  a segment's start and its end less its start, whole half px whichever slot's centre
  they are taken from, all three through the one `lineOffset()` call in a loop (GLSL
  3.30 and ES 3.00 have no `precise`). The owner draws its stroke and its nearer
  neighbour's together: where both nearest points are the joint, the larger of the
  two coverages (the same disc); elsewhere `a + ak - overlap`, the overlap mixed by
  the cosine `c` between the two edge normals from the product `a*ak` (edges crossing
  at right angles) to the smaller (parallel, covering the same side) or to
  `max(a + ak - 1, 0)` (parallel, opposite sides, a sharp bend's tip). That is the
  union of the two strips over the pixel within a few levels (against a 16x16
  supersampled union at joint pixels: rms 0.6 to 2.2, max 3.5 to 10.5 levels, the
  single segment's own model error; the plain product `1 - (1-a)(1-ak)` was rms
  about 3 with a max of 22 to 31 on a joint's outer rim, where one segment's disc lies
  inside the other's capsule), and it is continuous where one nearest point leaves
  the joint, the two normals being one there. A pixel no neighbour reaches takes its
  coverage at `vLocal` as interpolated, so a segment without neighbours draws byte for
  byte as before. Ownership is by distance rather than by coverage because llvmpipe
  runs every branch of the shader for every pixel, masked: the three distances and
  the neighbour's coverage add about 10 % to its rasterising of every frame (an idle
  scene's `flush`), where three coverages and a clipped strip union added 250 %. The
  rule covers adjacent segments only: segments two or more apart along a polyline (one
  shorter than the stroke between them, or a zigzag crossing itself) both draw a pixel
  they reach and keep the depth rule above, and a caller that hides a segment draws
  its neighbours again without it (`PlotLines` does, at a NaN). A quad wider or higher
  than `lx/ly` hold (32767 half px) is collapsed like one past the vertex range.
  `renderer::Base`'s default draws a filled rect along the segment's longer axis, its
  extent there and `round(thickness)` across, centred on its middle: exact only for an
  axis-aligned segment without its caps, and ignores the neighbours. `tests/lines.cpp`
  checks style 3 pixel by pixel against a model of the placement, the coverage and a
  joint's owner and union.
- **Arcs (style 5, kind 9).** `drawArc(id, rect, startTurns, sweepTurns, thickness,
  color, z)` draws a stroked circular arc with round caps in one slot: on the largest
  circle centred in `rect` (its diameter the shorter side, in device px after
  `translate()`), the stroke `max(1, snap(thickness))` whole px wide (at most the
  circle's radius) inside the circle, its outer edge on it. The angles are in turns
  from the top (12 o'clock) clockwise, y being down: `startTurns` any value, only its
  fraction counting, `sweepTurns` clamped to 0..1 (a negative one is the same arc from
  its other end, the start moved back by it). It is an icon kind rather than a style
  of its own because style 5 already has all it needs: the device-px box filter and
  constant `aa` of kinds 2-6, the stroke in `bx`'s high byte, the half-step discard
  that writes no depth for an uncovered pixel, and a vertex shader that unpacks
  color1 into a flat varying, so the arc took no new attribute, varying or vertex
  code. **The `Vertex` fields:** `style` 5, `radius` 9 (the kind), `lx/ly` the quad's
  half-pixel local position as for every SDF style, `tx/ty` the colour, `bx`'s high
  byte the stroke px, `cx` the start in 1/65536 of a turn and `cy` the sweep in 1/65535
  (`renderer::arcStartBits`, the fraction rounded, and `arcSweepBits`, rounded with
  65535 the whole ring), and the unit directions from the centre to the start
  (`border`, `by`) and to the end (`dx`, `dy`), worked out from those bits on the CPU
  and stored as `int16_t` in 1/32767, which the fragment shader reads back exactly
  from the flat `vBorder`, the border colour's and color3's bytes. Two calls with the
  same bits draw the same bytes, so a caller animating an arc compares the bits to tell
  a step that draws nothing new (the Spinner does). The shader takes the pixel centre
  rounded to the half px it is at (`vLocal`, as kinds 0 and 1) and its distance to the
  stroke's middle circle less half the stroke (the ring); a pixel outside the sweep
  (by the signs of its cross products with the two directions, and for a sweep under
  half a turn its side of either direction: not of their sum, which just under half a
  turn nearly cancels after the `int16` rounding and points anywhere, cutting a gap
  into the start cap) takes the distance to the nearer cap's centre less
  half the stroke instead, which is the arc's exact distance; the coverage is the box
  filter `clamp(0.5 - d/aa, 0, 1)`. A sweep of 65535 skips the caps, so the whole ring
  has no seam. The shader computes no `sin`, `cos` or `atan`: with them (the first
  version), SwiftShader's precision put a few cap pixels of SpinnerDemo at 2x 3 levels
  from llvmpipe's, past `web_pixels_native_scale200`'s bound; with the directions from
  the CPU every backend does the same arithmetic. `renderer::Base`'s default draws
  nothing, as `drawIcon`'s does;
  `StubRenderer` records each call in `arcs`. `tests/arc.cpp` reads back every pixel of
  arcs at sweeps of 0.25 to 1 turn from a start in each quadrant, and of 32756 to
  32767/65535 from 16 starts, against the same
  distance worked out on the CPU (within 2 levels), the start's direction, the stroke
  at scales 1 and 2 and the depth of an uncovered pixel, on every backend.
- **Trapezoids (style 9).** `drawTrapezoid(id, x0, x1, top0, top1, bottom0, bottom1,
  colour, z)` draws a region with vertical sides at `x0` and `x1` and straight top and
  bottom edges between them, in canvas units (a `Chart` area's segment, from its line
  down to its base). The sides are snapped with `RenderTransform::deviceX`'s rounding,
  `translate()`'s, so a whole-unit side lands where a rect's edge does and two trapezoids
  sharing a side share its device px: the left one covers the columns before it, the
  right one those after, with no seam and no column in both. The edges are at `y*scale`
  unsnapped (the centre a `drawLine` stroke along them has). The quad is the sides'
  columns over every row an edge crosses; the four heights, device px from the quad's
  top, are floats' bits in `cx/cy` (top at `x0`), `bx/by` (top at `x1`), `dx/dy` (bottom
  at `x0`) and `radius/border` (bottom at `x1`), the colour in `tx/ty`. The vertex shader
  turns each edge, once a vertex, into what the fragment's coverage needs
  (`trapezoidEdge()`: the line's distance terms and `pixelArea()`'s `h1`, `b`, `1/a` and
  `1/(2ab)`), in the flat varyings the other styles' fragments read (`vSegment`,
  `vJoints`, `vColor1`, `vBorderColor`). The fragment finds its pixel as the one `vLocal`
  is in (half a px from its edges at the centre, farther than an interpolation error
  moves it), so its coverage is a function of the flat terms and its column and row
  alone, and takes each edge's part of the pixel by `pixelArea()` of the centre's
  distance to it, written without a branch or a division (its linear part `e/a` with
  `|e|` held to `h1`, plus the quadratic end `t(2b - t)/(2ab)`, `t = clamp(|e| - h1, 0,
  b)`): the pixel's exact area on that side of a straight edge crossing its column. The
  coverage is the two parts less the 1 their union is (above the bottom and below the
  top every point is inside one of them), and where the edges have crossed (an area
  segment whose line crosses its base) the same sum negated, so it is the sum's
  magnitude: exact on either side of the crossing, and in the column it is in a pixel
  holding both sides is covered by their difference. So a flat trapezoid on whole device
  px is `drawFilledRect`'s rect byte for byte, and a nearly flat edge is computed from
  the same flat floats and integer pixel by every backend (GL and Vulkan draw
  `tests/trapezoids.cpp`'s frames byte for byte alike). An uncovered pixel (alpha under
  half an 8-bit step) is discarded and writes no depth, as for styles 3-7. Cost: llvmpipe
  runs the branch, masked, for every primitive's pixels; the first version (the slope,
  normal and `pixelArea()`'s branches in the fragment, and a coverage flag for the
  discard) cost `gweni_bench idle`'s flush +3.0 % instructions, this +1.7 % (4,623.8 M
  without the style, 4,762.7 M first, 4,703.3 M now at N=1000; the branch with a constant
  coverage 4,627.1 M). A quad wider or higher than `lx/ly` hold is collapsed like one
  past the vertex range. `renderer::Base`'s default draws one `drawFilledRect` from `x0`
  to `x1` between the middles of the two edges, in the colour. `tests/trapezoids.cpp`
  checks a row of trapezoids (each at a z of its own, so a column drawn twice would be
  blended twice), opaque and at 50 %, against a CPU model at scales 1, 1.5 and 2, the
  outer sides' whole columns, a flat one against `drawFilledRect`, a crossed one, the
  uncovered pixels' depth and the slot's writes.
- `drawFilledRect()`/`drawTexturedRect()` write the 4 vertices + 6 indices for a quad
  into that id's slot in the CPU mirror.
- **Images (style 2) in the one draw.** `drawTexturedRect` takes a texture loaded on its
  own (`loadTexture`, e.g. an `ImagePanel`'s `gwen.png`), not a region of the skin atlas.
  Its UVs are 0..1 and stored in `tx/ty` as 0..65535; the draw colour is its tint (color1,
  alpha included). The image is in a layer of an **image page**: one 2D texture array per
  size class, `SlotRenderer::ImageClasses` (5) of them, 128, 256, 512, 1024 and 2048
  texels square (`imageClassSide`), RGBA8 with every mip level, all bound for the
  primitive draw (`GL_TEXTURE2` on), which the fragment shader picks with a
  constant-indexed switch over `imagePages[5]`. `radius` holds the class, `border` the
  layer and `bx/by` the image's size in texels. An image takes, as it is loaded, a layer
  of the smallest class that holds it, at the layer's origin with its own mip chain, and
  keeps it until `freeTexture`, which puts it on its page's free list for the next load.
  A page is made when its class is first needed, with 8 layers for the smallest class
  down to 1 for the two largest, and a page with no layer free is made again with twice
  as many, its layers copied (`FrameStats::imagePageRegrows`, `image_regrows=` in
  `ControlTest --stats` and `gweni_bench`), up to the device's most array layers (256 at
  least on GLES 3.0 and WebGL 2), past which a load fails. So any number of images draw in
  the one draw, in slot order (the 8 image units it replaced drew the missing image for a
  ninth). An image larger than the largest page loads only with a max size, as the first
  level of its mip chain that fits it and the page (a 4000 px side as 2000 at a max size
  of 2048 or more); with none, as every file load, it fails. The 5 pages and the clip
  boxes take 6 of the program's 16 texture units. A fully transparent texel is discarded,
  so it writes no depth; a partly transparent one still depends on slot order, which is
  why `ImagePanel` takes its primitive id on its first render, after its parent's skin
  took its own.
- **Images: mip levels on the CPU, filtered in the shader.** An image's levels are
  made by `imageMipChain` (`slotRenderer.h`) when it is loaded, the same bytes for
  every backend: level n is `max(1, size >> n)` on each axis down to 1x1, GL's and
  Vulkan's sizes, and each texel the 2x2 box of the level above, every channel
  `(a+b+c+d+2)/4`, rounded to nearest. Alpha is averaged straight, not premultiplied,
  as `glGenerateMipmap` averaged the stored texels and as the image is blended
  (`SRC_ALPHA, ONE_MINUS_SRC_ALPHA`). Where the level above is 1 texel wide (high) the
  box takes that column (row) twice, and an odd side's last column (row) is in no box.
  `OpenGLCore` uploads each level with a `glTexSubImage3D` into the image's layer of its
  page, `renderer::Vulkan` copies them all from one staging buffer. The image is at the
  origin of each level of its layer, the rest of the layer unused, and the taps are
  clamped to the image's own size at that level (`bx/by` shifted down by the level), so
  nothing of the layer outside it is read.
  The primitive shader's `imageTexel` (`shaders/primitive.frag.glsl`) filters them as
  `GL_LINEAR_MIPMAP_LINEAR` with `GL_CLAMP_TO_EDGE` would, with the GL spec's LOD
  and weights computed in float, so that llvmpipe and the browser's SwiftShader draw
  the same pixels (they did not: "The browser against native" above):
  - the LOD is `log2` of the longer of `uv`'s two screen gradients (`dFdx`, `dFdy` of
    the UV, linear over the quad, so the same anywhere and on any GL up to a float
    error) in level 0's texels, rounded to 1/256;
  - at or below 0 (magnification) it is level 0 bilinear, the 4 texels around the UV
    clamped to the level's edge, weighted by the UV's place between their centres to
    1/256 (as a texture unit keeps its filter's weights, so that the float error of
    the UV, which a moved quad or another GL interpolates differently, moves no level:
    `tests/clipBoxes.cpp` draws an image straddling the view's edge and the same one
    moved inside it, and wants the same pixels); above 0 the 4 of each of the two
    levels around the LOD, mixed by its fraction; past the last level (1x1), its 4.
  Each tap reads one texel exactly, a `textureLod` at its centre and its level's whole
  LOD with the image's filter nearest. Not a `texelFetch`, which reads the same texel:
  llvmpipe runs every branch of a shader for every pixel, masked, and skips a sample
  whose pixels are all masked off but not a `texelFetch`. The 5 image pages need a
  switch around each sample (GLSL 3.30 and ES 3.00 index a sampler array only with a
  constant), so the taps are one loop whose count (4 or 8) is not a constant, around
  one switch of 5 samples, which no compiler unrolls into 8 switches. A tap's centre
  is in the page's level, a power of two texels square, so it is exact, and it reads
  the texel it read in a texture of the image's own size: the pages draw the 8 units'
  pixels byte for byte.

  **What it costs.** Per image pixel, the shader went from one `textureGrad` to one
  `textureSize` and 8 `textureLod`s minified (4 magnified), about 375 SPIR-V
  instructions minified (glslang, unoptimised; 205 magnified). Under llvmpipe
  (callgrind, `LP_NUM_THREADS=0`, Mesa 23.2.1) a frame of ImagePanelDemo, whose image
  is 100x100 px, went from 14.29 M to 21.63 M instructions, about 700 more a pixel of
  the image, and a frame of ButtonDemo, which has none, from 15.17 M to 15.53 M
  (2.4 %: every pixel runs the image branch masked; with `texelFetch`es it was
  5.7 %, and with the taps unrolled, 52 %). The primitive program compiles in fewer
  instructions than with the 8 `textureGrad`s (ButtonDemo's whole run 2.74 G against
  3.06 G). `gweni_bench`'s counters are unchanged in every scene; no scene draws an
  image. The image pages (#606) took the `textureSize` out (the image's size is in the
  vertex) and the switches down to 5 cases: `gweni_bench`'s flush, which every pixel's
  masked image branch is part of, went down 2.0 % in `idle` (4,702.2 M to 4,607.1 M
  instructions), 2.4 % in `hover-sweep` and 2.0 % in `build` at N=1000, `set-text`
  unchanged, and no other counter moved but the new `image_regrows`, 0 in every scene.
- Because the CPU mirror is persistent, a control that didn't change leaves its slot
  untouched between frames; its geometry is still in the buffer.
- A single shader pair (vertex + fragment) draws all non-text primitives; an ortho
  projection (`setViewRect()`) maps UI pixel coords to clip space; `z`/depth gives
  draw order.
- **Depth.** A larger z is in front (`GL_LESS`). The canvas numbers the controls
  depth-first (`Base::renumberZRecursive`): a control takes its skin Control's
  `depthLayers()` z values from `getZIndex()` on, then its children are numbered, then
  it takes `overlayLayers()` more (0 by default) from `getOverlayZIndex()` on, for what
  it draws in front of its whole subtree. The ranges are increasing in tree order but
  not packed: gaps between them let an insert relabel only its neighbours ("Gapped z
  labels"). A text run's glyph i is at
  `z+(i+1)/(glyphs+1)` (`SlotRenderer::renderText`, the move branch included), strictly
  between the Text control's z and the next one, so a rect drawn at the Text's z is
  behind every glyph and in front of everything numbered before it.
- **The TextBox caret and selection.** The textured skin's `TextBox` renderer (shared by
  `TextBoxNumeric`, `TextBoxMultiline` and `PasswordTextBox`) keeps three slots: the box
  at its z, the selection at the text's z (`TextBox::getTextZIndex()`) and the caret, 1
  unit wide and line-high in the theme's `LabelDefault` colour, at its one overlay z, in
  front of a multiline box's line children too. Both are drawn only while the box has
  the focus. `TextBox::updateCursorRects` takes them in box coordinates from where the
  text is after layout (`postLayout`) or after an edit or cursor move scrolled it
  (`makeCaretVisible`), and marks `StateChange_Background` only when one changed; the
  renderer adds the box's render position and cuts them to the box. The blink is an
  animation (`TextBox::updateCaretBlink`, 0.5 s, lit for 1.5 s after an edit, a move or
  the focus arriving; registered only while the box has the focus) that raises `StateChange_Caret` alone, which shows or hides the
  caret slot and rewrites nothing else: one 128-byte upload per edge
  (`GWENI_UPLOAD_STATS=1`), nothing in between, and nothing at all while unfocused. The
  selection colour is the `TextSelection` surface (`tokyoNight.h`, overridable in
  `DefaultSkin.json`'s `"theme"`). A wrapped (multiline) box draws no selection yet: it
  needs one rect per line.
- **The TextBox placeholder.** `TextBox::setPlaceholder` adds a second `internal::Text`
  child, made on the first non-empty hint, with `m_text`'s font, text padding and text
  alignment (kept in step by `setFont`, `setTextPadding`, `setTextAlignment` and
  `setWrap`), so its first glyph is where the first typed one goes; a multiline box's
  hint is one unwrapped line at its first line. It is drawn in the skin's
  `Colors.PlaceholderText` (the theme's `Muted`, `"PlaceholderText"` in `"textColors"`),
  is `doNotIncludeInSize()`, so it never widens the box, and a hint wider than the box
  is clipped by it like the text. `TextBox::updatePlaceholder` shows it while the box is
  visible and empty and does not have the focus, and hides it otherwise, through
  `setHidden` only: its text id is hidden, not freed, and nothing is laid out again. It
  runs from `setText` (`PasswordTextBox`'s too, which shows the hint unmasked) and
  `updateColors()`, which the box's own `setHidden` (through `redraw()`) and every focus
  change reaches through `redraw()` once `KeyboardFocus` has moved; `onLostKeyboardFocus`
  runs before it has (`Base::focus`), and after it has in `Base::unfocus`, which is how the
  canvas drops the focus of a hidden or input-disabled box (`input::dropLostKeyboardFocus`).
- **The field validation state.** A `TextBox` (and so `PasswordTextBox`, `TextBoxNumeric`
  and `NumericUpDown`) has an error (`setError`, `clearError`, `hasError`, `getError`,
  `onErrorChanged` once per change of the text), a helper text (`setHelperText`) and a
  validator (`setValidator(fn, ValidateOn)`, `validate()`), all shown on one message line
  under the field. Its state is a `TextBox::MessageLine` on the heap, made by the first
  use, so a box with none holds a null pointer and lays out, draws and counts exactly as
  before.
  - *The line.* An `internal::Text` child, `Placement::Owner` and `doNotIncludeInSize()`,
    in the box's font one size smaller (a font of its own, replaced on every change, as a
    Text tells a new font by its pointer), placed by `TextBox::arrange` in each axis before
    the children are: x at the field padding's left, as wide as the box from there, so a
    longer message is cut by the box's clip (a Label clips its children to its bounds),
    and y at the field part's bottom plus a 2-unit gap. It is not content: not
    `getText()`, never masked (a `PasswordTextBox` masks `m_text` alone), and a press or
    double-click on it (`TextBox::onMessageLine`) moves no caret and takes no selection.
    Its colour follows the skin (`internal::SkinTextColor::ErrorText`, else `HelperText`),
    so a theme edit reaches it.
  - *Its room.* `getMessageRoom()`, the line's font's line height plus the gap, measured in
    the box's Y measure (`TextBox::updateMessageRoom`, one counted measure), goes into two
    places: the bottom padding (`m_padding`, which every docked or aligned child is laid
    out in, so the text, the caret and a NumericUpDown's Right-docked buttons stay in the
    field part; `getPadding()` and `setPadding()` are the app's field padding), and the
    request, through `Base::requestedSizeExtra(dim)` (0 by default): every rule that reads
    the request (`preferredFromContent`, `dockNoneSize`, `layoutCellSize`, the Fixed
    minimum and maximum) reads `Base::requestedSize(dim)`, the request plus it, so a box
    asked to be 200x20 is 20 plus the room high, its field part (`getFieldHeight()`) 20.
    The room is reserved from the first `setHelperText`, `setValidator`,
    `setReserveMessageLine(true)` or `setError`, and kept: the first one lays out the box's
    parent's path (its size changed), and from then on a message set, changed or cleared is
    the Text's new string and a `redraw()` of the box, one layout pass entering the way
    down to the box and its subtree, its frame's slot and the message's glyphs written.
    `setReserveMessageLine(false)` gives the room back only while nothing is shown and no
    validator is set.
  - *The validator* returns the error text (empty for valid), and its result goes through
    `setError`, the app's path. `ValidateOn::Change` runs it in `onTextChanged` (after a
    `TextBoxNumeric`'s `isTextAllowed` filtering, before `onTextChangedCaller`),
    `ValidateOn::FocusLoss` (the default) in `onLostKeyboardFocus` (before
    `onLostFocusCaller`), both with `Change|FocusLoss`; Return in a single-line box runs it
    in either mode, in `TextBox::onEnter` (after a NumericUpDown's clamp, before
    `onReturnPressedCaller`). Setting one runs nothing; `setValidator(nullptr)` removes it
    and keeps the error.
  - *The look.* The textured `TextBox` renderer draws the frame over the field part alone
    and, while `hasError()`, picks `TextBoxError` (the theme's `Red` border),
    `TextBoxErrorFocus` (the same red, 2 units thick) and `TextBoxErrorDisabled` (the red
    mixed halfway with the disabled fill in `tokyoNight.h`: a border's alpha does not blend
    with the fill under it); the message colours are `Colors.ErrorText` (`Red`) and
    `Colors.HelperText` (`FgDark`), `"ErrorText"`/`"HelperText"` in `"textColors"`.
  `tests/unit/fieldValidation.cpp` holds it over the four field types, and
  `controltest_field_validation` (FieldValidationDemo) holds the costs on the real
  renderer.
- **The document and undo model.** `text::Document` (`include/gweni/text/document.h`) is
  a text as a `std::vector<String>` of lines, split on `'\n'` (`"\r\n"` is read as
  `"\n"`, always at least one line). A position is `(line, byte)`, and line i is O(1)
  to reach. Every change is a `text::Edit`: the `range` it replaced (in the positions
  before it) and the `text` put in, plus `removed` and `textEnd` (where the text ends,
  in the positions after it), which whoever applies the edit fills in.
  `Document::apply()` rewrites only the lines the range and the text touch. One that
  adds or removes lines inserts or erases vector entries, which moves the later
  strings' handles but none of their bytes. It then tells every observer
  (`addObserver`) the `LineChange{first, removed, inserted}` it made, which is what a
  view's rows and a tokenizer cache need. Because a record carries `removed` and
  `textEnd`, `Edit::inverse()` (`{range.begin, textEnd}` replaced by `removed`, ending
  at `range.end`) needs no knowledge of how positions are counted. `text::UndoStack`
  (`undoStack.h`) therefore stores records without a document, and `TextBox` keeps its
  own `String`, counting positions as `(0, byte offset)`.

  A step (`UndoStep`) is the records it applied, plus the `Selection` (anchor, caret)
  before the first and after the last. `record()` joins an Insert, DeleteBackward or
  DeleteForward edit to the step on top unless one of these holds:
  - the step was sealed (`seal()`, or an undo or redo since);
  - the kind changed (Paste, Cut and Other never join);
  - the selection before the edit is not the one the step left (the caret moved);
  - the edit inserts a `'\n'` (each typed line is a step);
  - more than `interval()` (1 s) of `platform::getTimeInSeconds()` passed since the
    step's last edit, so a capture's stepped clock gives the same steps every run.

  A new step drops the steps that could be redone, and `setLimit()` drops the oldest
  past the cap. A group (`UndoGroup`) puts several records into one step: its first
  record joins or starts a step by the rules above, as an edit of the group's kind,
  and the rest join that step. `TextBox::insertText` groups the selection's erase with
  the insert, so a letter typed over a selection starts a step that the next letters
  join, and a paste over one is a single Paste step. `undo()` and `redo()` hand the
  step back; the caller applies the inverses from the last edit to the first, or the
  edits in order, and restores the selection.

  `TextBox` records every edit of its text: typing (`onChar`, a multiline box's Return),
  Backspace, Delete, Ctrl+V, Ctrl+X, and the app's `insertText`/`deleteText`. Each
  handler sets the kind with an `EditKindScope`, and an app's call records as Other.
  Ctrl+Z, Ctrl+Y and Ctrl+Shift+Z call `TextBox::undo()` and `redo()`. They apply the
  step to one copy of the string and set it once through `setText`, so there is one
  `onTextChanged` per step, and `m_text` re-wraps only the paragraphs that differ, as
  after any edit. Then the step's caret and selection are restored.
  `setText()` from the app (and `PasswordTextBox`'s) is not an edit: one that changes
  the text clears the history (the text the box already has keeps it, as
  `ColorEdit` re-sets a field when it loses the focus). The box's own edits set the
  text through `setEditedText`, which keeps the history: it raises `m_applyingEdit`,
  and that `setText` takes it down before its `onTextChanged` fires, so a `setText`
  from a handler (`HSVColorPicker` clamping what is typed into its number boxes) is
  someone else's and clears the history. `setText` counts the changes it makes
  (`m_textSets`), so an edit whose own set had another nested in it (a handler's
  `setText` or edit) leaves a text the edit did not make: the history is cleared and
  the edit is not recorded. An undo or redo first checks that each edit's text is
  where the step says (`replaceEdited`), and a step that does not fit clears the
  history instead of being applied; it copies the step's selection before setting
  the text, since a handler's edit drops the steps that could be redone, that step
  among them. Losing the focus seals the step, so typing after the focus comes back
  is a step of its own. `TextBoxNumeric`, `PasswordTextBox` and DragNumber's edit box
  inherit all of this; the edit box is given its text when it opens and its history
  cleared, so each edit starts with none.

  Accelerators are looked up by `Base::acceleratorKey()`: upper case, no spaces, and
  Ctrl before Shift, the order `input::handleAccelerator` builds them in, so
  `"Shift + Ctrl + Z"` is `"CTRL+SHIFT+Z"`.

  What it costs: an empty stack allocates nothing (its steps are a `std::vector`;
  libstdc++'s empty `std::deque` allocates a map and a 512-byte block), so a box
  costs only its three new accelerators at construction. That is about 15
  allocations and 780 bytes per box, measured in `gweni_bench`'s `build` tree, which
  holds 3 TextBoxes in every 20 items (+2 250 allocations at N=1000). Typing adds about one allocation a frame (`type`: median 35 to
  36), for the records and for a new step's edit vector.

  One trap: the `gweni::text` namespace comes with every `textBox.h`. A file that
  says `using namespace gweni;` and defines a function named `text` gets ambiguous
  calls, as three unit tests' `text(Rect)` helpers did; they are `rectText` now.

  `tests/unit/textDocument.cpp`, `textUndoStack.cpp` and `textBoxUndo.cpp` pin this
  model, and `controltest_textbox_undo` checks it through ControlTest's keys.
- **Toggle buttons.** The textured `Button` renderer (`ButtonTemplate<>`, which also
  draws `LabelClickable` and the input buttons) paints one slot and picks its surface
  in this order: disabled (`ButtonOnDisabled` for a toggle that is on, else
  `ButtonDisabled`), pressed (`ButtonDown`, whatever the toggle state, so a press reads
  as a press), on (`ButtonOnHover` while hovered, else `ButtonOn`), hovered
  (`ButtonHover`), `ButtonNormal`. "On" is `isToggle() && getToggleState()`, so a
  Button that is no toggle never shows a toggle state. The on surfaces are an
  accent-tinted fill with a `Blue` border, unlike `ButtonDown`'s grey, and the disabled
  one stays a dim blue, unlike `ButtonDisabled`. A flip (`setToggleState`, or the
  release of a click) goes through `redraw()`, so it marks that button alone and
  rewrites its slot; the text keeps `Colors.Button.Down`, which an on toggle already
  had. `tests/unit/buttonToggle.cpp` pins the choice and the marks, and
  `--scenario interaction` the pixels of ButtonDemo's "Toggle Me".
- **Selectable rows.** `controls::Selectable` (`include/gweni/controls/selectable.h`) is a
  `Button` (for its press handling: the click lands on release over the row, Space
  presses it, Return too) that draws through its own textured renderer, docked Top with
  `{Expand, Shrink}`, so it is its parent's (or its layout cell's) width and its text's
  height. Its one slot is **hidden** while the row is idle, or disabled and unselected:
  a transparent rect would still write depth over what lies behind it. Otherwise it
  paints `SelectableHover`, `SelectableDown`, `SelectableSelected`,
  `SelectableSelectedHover` or `SelectableSelectedDisabled` (`tokyoNight.h`, the list
  row's looks), and the text takes `Colors.Selectable` (`"SelectableNormal"`,
  `"SelectableHover"`, `"SelectableSelected"`, `"SelectableDisabled"` in
  `"textColors"`; the hover colour is the normal one, so a hover rewrites no glyph).
  `setSelected` changes the state through `redraw()`, so only that row is marked.
  `SelectableGroup` is a plain object, not a container control, so its rows can sit in
  any parents (a layout, later a table's cells): selecting a member deselects the
  previous one through its own `setSelected`, members are untoggleable, and a member
  that is deleted, or the group itself, lets the other go without an event. A selection
  change in a group therefore marks the two rows (and their labels) alone.
- **Tree labels.** A `TreeNode`'s label (`TreeNodeText`, a private `Button` subclass in
  `source/controls/treeNode.cpp`) draws as a flat list row, not a button: the textured
  `TreeNodeText` renderer, found by its type name before `Button`'s, has one slot,
  **hidden** at rest and while disabled (selected or not), painting ListBox's
  `ListItemHover` while hovered and `ListItemSel` while selected (the title's toggle
  state, which `TreeNode::setSelected` sets), hovered or not. A press selects at once
  (`onDownCaller`), so there is no pressed look. The fill is the label's bounds, from
  its 16-unit margin past the toggle to the tree's right edge; the connector lines and
  the chevron are the `TreeNode`'s and the toggle's, and the text colours are
  `TreeNodeText::updateColors`' (`Colors.Tree`). Hover and selection go through
  `redraw()`, so moving the selection marks the two labels and the newly selected
  `TreeNode` (its own `redraw()` in `setSelected`, whose lines are rewritten with the
  same bytes), and writes the two labels' slots alone. A `PropertyTreeNode`'s
  title takes the same renderer and is not selectable (`setSelectable(false)`): at rest
  the category's `Properties.Border` band shows through, and a hover paints
  `ListItemHover`, a little darker than the band.
- **Menu rows.** A `Menu` is one rounded backdrop (the textured `Menu` renderer, the
  `Menu` surface) with flat rows on it; a `MenuStrip` has its own renderer, found
  before `Menu`'s by the type walk, painting the flat, square `MenuStrip` surface. A
  `MenuItem` has its own renderer too, found before `Button`'s: like a `Selectable` row
  its first slot is **hidden** while idle or disabled, and paints `MenuHover` over the
  item while it is hovered, pressed or has its submenu open (`isMenuOpen()`). Its
  second slot is the check of a checkable, checked item (`isCheckable()`,
  `isChecked()`; a click toggles only a checkable item): a checkmark (style 5, kind
  0) at the item's z+1 (`depthLayers()` 2), in its text colour, so it follows the
  hover and dims when disabled, centred in the icon column (the item's left text
  padding), where it hides the item's image; otherwise that slot is hidden.
  `setChecked` and `setCheckable` redraw the item on a real change. An item on a
  `MenuStrip` itself or in a menu with `setDisableIconMargin(true)` has no column and
  draws no check. The
  submenu opening and closing marks the item: `Menu::setHidden` fires
  `onVisibilityChanged` on a real change, and the item's handler calls `redraw()`, so
  the highlight follows the submenu however it closes (`Menu::closeAll`, the canvas
  closing every menu, a hover on another item). The menu's 2-unit padding is its
  inner panel's, with no inner margin and none of its own: `ScrollControl::placeContents`
  places the inner panel at the viewport (`getViewport()`: the control less its padding,
  the bars that show and the inner panel's margin, 5 by default), which for a menu is
  the whole control, so the rows are 2 units in from the backdrop's border on every
  side. A `MenuStrip` has no padding, no inner margin and no bars (`setScroll(false,
  false)`); its items' `StripItemInset` (7) above and below their caption makes it 29
  high, and the inner panel's 5-unit left padding puts the first one 5 in. Labels are
  `Left|CenterV` after the 24-unit icon column (their left text padding), clear of the
  accelerator docked on the right; a `MenuDivider` has 3 units of backdrop above and
  below its 1-unit rule.
- **The indeterminate ProgressBar.** `ProgressBar::setIndeterminate(true)` draws a
  segment (`setSegmentLength`, 0.3 of the track) in the fill's slot instead of the fill,
  moving start to end (bottom to top when vertical) at `setIndeterminateSpeed` track
  lengths a second (0.75), sliding out past the end and in again at the start; it starts
  wholly on the track at its start. The bar's animation (`ProgressBarThink`,
  `platform::getTimeInSeconds()`, so the stepped clock in captures; registered only
  while the bar moves, its first step the time since it started) advances its phase,
  and `ProgressBar::indeterminateSegment` turns the phase into the segment's rect in
  whole units, cut to the track (the bar's own `render()` is clipped by its parent's
  region, not its own). The control and the renderer both use that function, and the
  control marks `StateChange_Background` only on a frame the rect changes, so a running
  bar writes its one fill slot per frame at most and a frame it moves less than a unit
  writes nothing. Its auto label is empty while it runs; `setProgress` still stores the
  value, and `setIndeterminate(false)` brings back that fill and its label. The auto
  label is set only when the whole percentage changes. An `Elastic` bar is as high as
  its label, so a bar whose label comes and goes is given a `Fixed` height if that must
  not move its neighbours (ProgressBarDemo does).
  The animation is registered only while the bar moves **and** is `visible()`: a
  cycling or indeterminate bar on a page switched away from, under a hidden ancestor
  or hidden itself has none, so it costs its canvas's update phase nothing
  (`anims_run=0`), and shown again it gets a new one. That is
  `Base::onVisibleChanged()`, which `ProgressBar` overrides to call
  `updateAnimation()` (below, "The visibility hook"). A bar built into a page hidden
  already (every shell page from index 1 on) registers nothing until the page shows;
  its first step after the show is the time since it was shown, and its pixels and
  counters while shown are as they were (ProgressBarDemo's, byte for byte, in
  ControlTest and in the shell).
- **The visibility hook.** `Base::onVisibleChanged()` (virtual, empty) is called on
  each control whose `visible()` changed, wherever `StateChange_Visibility` is marked
  for that reason: from its own `setHidden()` (unless under a hidden ancestor, where
  nothing changes), from `setAncestorHidden()` as an ancestor's hide or show reaches
  it (stopping at a descendant that hid itself, whose subtree does not change), and
  so from being attached to or detached from a hidden parent. It is called once its
  whole subtree has its new flags, so `visible()` is the new value on it and below it.
  A control that does not override it pays one empty virtual call per change, nothing
  more. `setParent()` moving a control straight from one parent to another leaves its
  ancestor flag for `addChild()` to set (`g_movingChild` in `source/controls/base.cpp`;
  `removeChild()` would otherwise set it shown on its own in between), so a move
  between two hidden parents marks nothing and calls nothing. Work kept only while a
  control shows (a ProgressBar's or a Spinner's animation) is registered and
  cancelled from it. The Switch does not use it: its slide ends at once when it finds
  itself hidden. `tests/unit/visibleChanged.cpp` pins the calls.
- **The scroll position: a whole-unit offset per axis.** A `ScrollControl` keeps how
  far its content is scrolled as an offset in units per axis (`m_scrollOffset`,
  `get/setHorizontalScrollOffset`, `get/setVerticalScrollOffset`, 0 to
  `getMax*ScrollOffset()`, the content less the viewport), not as its bars' fraction:
  `placeContents` places the inner panel at the viewport's origin less the offset, clamps
  it to the content and viewport that pass gives and sets each bar to show it as a
  fraction (inside its `PlacementScope`, so that does not re-dirty the pass). So content
  that grows keeps the offset, the inner panel stays where it is and the bar's amount and
  thumb follow the new size; content that shrinks keeps it while it fits and clamps it to
  the new end otherwise; a bar no longer required takes it back to 0. There is no
  sticking to the end: scrolled to the bottom, a grown content leaves the new end below
  the viewport. A bar not where the control last put it (`m_shownAmount`) was moved by
  the user (its thumb, its buttons, the wheel, a ListBox's arrow keys), and its amount is
  converted at the range of the last pass (`m_scrollRange`), the content it was moved
  against, rounded to a whole unit; a bar moved to between two units keeps its amount
  while it rounds to that offset at the same content, so nudges and wheel notches of
  under half a unit (content overflowing by a few units) add up instead of being put
  back. `scrollToTop/Bottom/Left/Right` are a pending edge per axis, resolved to 0 or
  the range by the next pass, so an `addItem` then `scrollToBottom` before any layout
  (`Demo::printText`) ends on the new last row; any `set*ScrollOffset` on the axis
  cancels it, even one that keeps the offset, while a new virtual content height
  (`VirtualList::setItemCount`) clamps the offset without the setter and keeps it. Before this (issue #512) the panel was placed at `-(content -
  viewport)*amount`, so a DataTable column drag while scrolled sideways slid the body
  under the cursor and a ListBox append moved, and re-stamped, every row
  (`tests/unit/scrollOffset.cpp`, `dataTableScrolledResize.cpp`).
- **Width-dependent content in a ScrollControl.** A ScrollControl leaves its subtree out
  of the X pass and lays it out in its Y arrange (`docs/layout.md` -> "Children whose X
  layout needs the parent's height"), so content whose height follows its width (wrapped
  text, a `layout::Flow` of cards) is measured in the pass's Y measure at the width it had
  before: the bars and the inner panel are decided (`placeContents`) from that height. Once
  the content is laid out at its new width it is measured again, and when that gives
  another height `ScrollControl::arrange` decides the bars again for it (`decideBars`, the
  arithmetic `placeContents` uses):
  - **They show as placed:** always with a *stable gutter* (vertical scrolling with
    `setAutoHideBars(false)`, the vertical bar shown, disabled while the content fits, so
    the viewport's width does not depend on the content; or no vertical scrolling at
    all), and with auto-hide when the new height overflows the viewport exactly when the
    old one did. Every width the content was laid out at is then final, and
    `placeContents` runs again for the new height before the Y arrange, inside its
    `PlacementScope`: the bars' ranges, the offset kept or clamped, the inner panel's
    height, and a `scrollTo*()` of the frame resolved at the new content (the pending
    edges are put back before it). One pass.
  - **They would show otherwise** (auto-hide, the bar to come or go): the content was laid
    out at the wrong width. The inner panel takes the new height and the ScrollControl
    re-dirties itself (`noteRedirty`), so the canvas's next pass decides the bars from the
    height measured here: two passes, and a narrower viewport never makes the content
    shorter, so that second pass keeps its bars. Before #611 this path resized the inner
    panel before its Y arrange, which re-dirtied nothing: the pass converged with the
    bars ranged over the height at the old width (in a first frame, the inner panel's
    default 10-unit width), for good.

  So a Flow in a scroll view needs a stable gutter to lay out in one pass. Its
  ScrollControl's own preferred height is still its content's as the Y measure saw it,
  at the old width (the ancestors above it are measured from it before its arrange), so
  one that fills its parent is given a requested height its dock does not read
  (`setHeight`), which it then asks for instead (`sizedByRequest`): otherwise
  `--verify-layout` and a resize round trip show the ancestors' preferred heights from an
  earlier frame. FlowDemo does both. The Flow's own Y measure before its X arrange in a
  pass (this stale one) keeps its last height and enters no card
  (`layout::Flow::calculateSize`), so a card's wrapped text is never measured at the
  width it had before the Flow placed it. `tests/unit/scrollWidthDependent.cpp`,
  `tests/unit/flow.cpp` and `ControlTest --scenario flow` pin it.
- **The horizontal wheel.** The wheel has two axes from the input translator to the
  control. `IInputEvent::inputMouseWheelH(int)` is a non-pure hook beside
  `inputMouseWheel` (60 a notch, the same sign convention: a negative delta scrolls the
  content towards its end), returning `false` by default, so a translator or a test
  implementer written before it still compiles and reports the event unused.
  `input::GLFW3::processScroll` passes GLFW's x offset to it (times 60, rounded, as the y
  offset), each non-zero axis on its own call; in a page emscripten-glfw gives the wheel
  event's `deltaX` as that offset (`web_input`'s `board` check). `Canvas` routes both to
  the hovered control, `Base::onMouseWheeledH` for the horizontal one, which bubbles to
  `m_actualParent` as `onMouseWheeled` does, and turns a vertical wheel with Shift held
  into a horizontal one, there and not in GLFW3, so every translator gets Shift+wheel.
  `ScrollControl::onMouseWheeledH` moves its horizontal bar when it scrolls horizontally,
  the bar is shown and enabled and the notch moves it (not at an end), else bubbles: the
  nearest ScrollControl that can move sideways takes it. An `InputCapture` gets the axis
  as `captureMouseWheel(delta, Dim)`, as it arrived, before the Shift mapping (the
  DepthView takes either axis as its one wheel; the ZoomPanel and NodeGraph captures drop
  `Dim::X`, so a sideways swipe zooms nothing mid-gesture as outside one)
  (`tests/unit/horizontalWheel.cpp`, `inputCapture.cpp`).
- **The board: columns that scroll on their own.** `controls::Board` is a ScrollControl
  scrolling horizontally only, whose inner panel is the viewport's height because its
  `contentSize()` is 0 high, so no column's content asks the board for height. Its `BoardColumn`s are Owner-placed
  (`addChild` sets it, `removeChild` gives `Placement::Parent` back): `contentSize()` is
  the shown columns' `getColumnWidth()` (240) each with `getGap()` (12) between them, and
  `contentsPlaced()` puts each at its x, `columnWidth` wide and the viewport's height,
  whatever its cards. A `BoardColumn` is a header Label docked Top (`setHeaderHeight`, 36)
  and a body, a vertical ScrollControl docked Center that its `newChild()` children go
  into (dock cards Top, or put a `layout::Flow` there), with a requested height of 0 so
  it never asks the column for its cards' height. The body's bar is a **stable gutter**
  (`setScroll(false, true)`, `setAutoHideBars(false)`): shown, disabled while the cards
  fit, so a card added to a fitting column changes no card's width and the wrapped cards
  lay out in one pass ("Width-dependent content in a ScrollControl"). The wheel, as
  decided for master-mind's board: the body's `onMouseWheeled` tries its bar and returns
  `true` whatever happened, scrolled, at an end or fitting, so a vertical wheel over a
  column never moves the board; the header, the gaps (the board's inner panel) and the
  board's inset are outside every body, so a vertical wheel there bubbles to the Board,
  which as a horizontal-only ScrollControl scrolls sideways; and the body does not take
  the horizontal wheel, so a horizontal wheel or Shift+wheel anywhere on the board, over
  a body too, scrolls the board. Each keeps the offsets of the others: content that
  grows keeps every offset ("The scroll position"), so a card added or a card's text
  grown moves no scroll position. Both draw nothing themselves (`GWENI_DRAWS_NOTHING`:
  the header, the cards and the bars draw). BoardDemo has five columns of wrapped cards,
  an overflowing Backlog and a fitting Ready; `tests/unit/board.cpp` and
  `ControlTest --scenario board` pin it (`docs/layout.md` -> "Board").
- **The virtual list: a pool of the rows on screen.** `controls::VirtualList`
  (`include/gweni/controls/virtualList.h`) shows `setItemCount(n)` fixed-height rows
  (`setRowHeight`, 20 by default) through a pool of row controls: `ceil(viewport/rowHeight)+1`,
  made by the row factory (`setRowFactory`; by default `internal::VirtualListRow`, a
  `Selectable` that never stays selected, since the row shows another item once the list
  scrolls, and fires `onItemPressed` with its item) and filled for an item by the row
  binder (`setRowBinder(row, index)`). The pool is created or deleted only when the
  viewport's height changes, so 100 000 items have the controls, primitive ids and text
  ids of the dozen or so rows on screen, and a frame costs what those rows cost.
  **A resized pool gets fresh rows:** in the frame its size changes, `contentsPlaced`
  first works out which rows keep their item, then deletes the rows the items in view
  do not reach, wherever they are (each giving up its press and focus first), and
  appends fresh rows up to the new size; it does not keep the back ones. So a list
  grown and shrunk back with no scroll between holds the pool a list built at that
  size does: the same rows in the same child order, and a spare that was never bound,
  measured, placed or drawn, not one shown at the larger size with its bounds, text,
  slots and run (issue #479). A released primitive slot's vertexes are zeroed
  (`SlotRenderer::releasePrimitive`), so a fresh row given a deleted row's ids reads as
  a never-written slot too, and `ControlTest --scenario resize` holds on
  VirtualListDemo, whose list fills the canvas's height. The cost, only in a frame whose
  pool size changes (one that already makes or deletes rows and renumbers z), is one
  row made and one deleted more than needed while the items fill the viewport; when
  they end inside it (a short list, an empty one), every hidden row past the last item
  is deleted and made again, on a grow as on a shrink.
  **Positions stay small:** nothing is `n*rowHeight` high. The list is a `ScrollControl`
  with *virtual content* (`ScrollControl::setVirtualContentHeight`): the vertical bar is
  decided and ranged from that height, but the inner panel is the viewport, not scrolled
  vertically, and the scroll position is the control's vertical offset
  (`getVerticalScrollOffset`, 0 to `getMaxVerticalScrollOffset()`, see "The scroll
  position" above), which the bar shows as a fraction and which a bar the user moves
  (thumb, buttons, wheel) sets. Each row is placed in the viewport at `index*rowHeight-offset`, so every
  rect drawn is on screen: a list of millions of rows never comes near the int16 device
  range (`cullOutOfRangeQuad`) or the slot ceiling. The content height is capped at
  `INT_MAX` units. **Binding** happens in the Y arrange, in `contentsPlaced` (called by
  `ScrollControl::arrange` right after `placeContents`, under a `PlacementScope`): the
  rows still showing an item in view keep it and are not bound again; the others take
  the items that came into view and are bound, then measured in X, since the X
  measuring of the pass is over; a row showing nothing (the spare one while the offset
  is a whole number of rows, the rows past the last item) is hidden. So a one-row scroll
  moves the rows on screen (their glyph slots; an idle row's slot is hidden and costs
  nothing) and binds one, a jump binds the rows whose item changed, and a count that
  changes binds only the rows that came into view, keeping the offset inside the new
  content. Rows made during the pass are made under a `Base::PartScope`, so their
  constructors' defaults are not counted as layout requests. The binder must only set a
  row's content. **A row's interaction state is its item's:** a row given another item
  gives up the mouse it holds (a press on its old item is cancelled, its depressed look
  dropped) and the keyboard focus, so neither acts on the item it shows now; and when a
  shown row moved, was bound again or was shown or hidden, the list calls
  `Canvas::hoverMayHaveMoved()`, and `input::onCanvasThink`, which runs after the
  frame's layout, hit-tests the hover again at the resting mouse (on the canvas the
  mouse last moved on, unless a capture pins it). So after a wheel the highlight is on
  the row under the mouse, and a click with no move in between presses that item.
  `scrollToItem` is resolved by the next layout, with the viewport it gives, so it works
  before the first one and in the frame of a resize. The latest scroll request in a
  frame decides the view: a `scrollByRows`, a `setVerticalScrollOffset` (virtual on
  `ScrollControl` for it), a wheel notch the list takes and a move of its bar by the
  user each drop a `scrollToItem` still pending (#661). The bar's arrows and track tell
  the list nothing, so every request first takes a bar moved since the last layout
  (`takeBarScroll()`, below), as does the list's own Y arrange: a move found there came
  after every request made so far. `scrollRequestSerial()` counts the requests (an
  offset only when it moves the offset or drops a pending item; not an edit's keeping
  of the rows on screen, nor a layout). `scrollByRows`,
  `firstVisibleItem`, `rowForItem`/`itemForRow` and `refresh`/`refreshItem` are the
  rest; a wheel notch scrolls `setWheelRows` rows (3). VirtualListDemo holds 100 000
  items.
- **The virtual list's variable mode: a height index and an anchor.** A list given an
  item measurer (`VirtualList::setItemMeasurer((index, width) -> height)`) gives each
  item its own height, the measurer's at the rows' width, and a null one gives fixed
  mode back, which is the code path above unchanged (`bench.sh compare` flags nothing
  in its scenes, and `controltest_virtual_list` and `controltest_data_table_virtual`
  hold). **Heights come from a measurer, not from laying a row out:** rows are bound in
  the Y arrange, and a row's height must be known before the rows below it are placed,
  in the same pass. For wrapped text the measurer is the wrap `internal::Text` does:
  its line breaking is `text::wrapLines`/`text::splitWords` (`include/gweni/text/wrap.h`),
  a pure function of the string, the width and a width measure, which a wrapped `Text`
  calls with `measureText()` in its font and `Base::wrappedTextLines()`/
  `wrappedTextHeight(font, string, width)` call for anyone else, so a row measured with
  them is laid out at that height (VirtualListVariableDemo's rows: a wrapped
  `VirtualListRow`, its height the wrap at its width less its padding, plus the
  padding). **The height index** is `controls::HeightIndex`
  (`include/gweni/controls/heightIndex.h`), a B+-tree of sums: leaves of up to 256
  items, each a height and the generation it was measured at, inner nodes of up to 64
  children holding their subtree's count, height sum and lowest generation. Append,
  insert, erase (a pop from the front included) and set cost O(log N) plus the items
  added or removed, `offsetOf` (item to offset), `itemAt` (offset to item) and
  `firstStale` (the next item measured before a generation) O(log N), the size, the
  total and the lowest generation O(1); a Fenwick tree has no middle insert or erase
  and no pop from the front, which a capped log and an editor's line edits need
  (`tests/unit/heightIndex.cpp` holds it to a vector under random edits). The content
  height is the index's total, capped at `INT_MAX`; an item not measured counts at the
  estimator's height (`setItemEstimator`, for items added after it is set), else the
  last measured height, else the row height. **The scroll position is an anchor:** the
  item at the viewport's top and how far it is scrolled out (`getAnchorItem`,
  `getAnchorOffset`). Each layout measures the items on screen from the anchor down
  until they fill the viewport (and up from the last one when the end shows, or after a
  scroll to the bottom), places them one below the other from it at the index's
  heights, and only then sets the content height, the bar and the offset from the
  anchor through the index. `m_scrollOffset[1]` stays the bar's view of it: the list
  keeps the offset the anchor last gave, and any other at the next layout (the bar, the
  wheel, `setVerticalScrollOffset`, `scrollToBottom`) was a scroll, anchored at its
  offset; `scrollToItem` and `scrollByRows` (by whole items) move the anchor itself.
  An edit between two layouts anchors such a scroll first, a bar the user moved (its
  amount no longer the one the list last showed) included, so the edit keeps it. A bar
  moved to between two units keeps its amount while it rounds to the offset at the same
  range, as a ScrollControl's does (#512), so nudges of under half a unit add up.
  `scrollToItem` measures the items from the anchor down to its item, or down to the
  viewport's bottom when it is further, before it decides whether the item is in view:
  estimates would say an item shows that does not. An item at least as tall as the
  viewport (a long wrapped line) goes to its top, never its bottom, so a second call
  on it changes nothing; `scrollToItem(index, partTop, partHeight)` asks for a part of
  it instead (a wrapped line's visual row), and such an item scrolls only as far as
  puts that part in view. So an item above the top changing height, items inserted or removed above it, and an
  estimate refined into a measure move the offset and the thumb, never a row on
  screen. **Edits** (`insertItems(at, n)`, `removeItems(at, n)`, `setItemCount`,
  `itemChanged(i)`, which measures that item again at once) cost O(log N) in the
  item count: the pooled rows' items are shifted by index arithmetic, so a row of a
  moved item keeps it and is not bound again, and one of a removed item gives up its
  press and focus and is free for the next layout (which tests the hover again); the
  item at the top stays there, or, removed, the one after the removed ones takes its
  place (a `setItemCount` cutting it off leaves the anchor past the end, which an
  insert above it keeps there, so the next layout shows the end). Fixed mode gets the same edits by whole rows. In both modes the top is where the bar is when the edit is made: a bar the user moved since the last layout is taken first, at the content it was moved against, as `placeContents` would take it. **Stuck to the end**
  (`setStickToEnd(true)`), every layout puts the last item's bottom at the viewport's
  bottom, through appends, removals and new heights, in their frame; only the app
  sticks or unsticks it, a scroll does not. **The pool** is the viewport at the
  smallest height measured, `ceil(viewport/minHeight)+1`, the same at 1 000 and 100 000
  items. **The vertical bar always shows** in variable mode (disabled while everything
  fits, `setAutoHideBars(false)`, given back on leaving it): otherwise a height measured
  in the pass could show or hide the bar, narrow the rows' width and call for a second
  pass. **A new width is one counter:** the measure generation goes up by one, so every
  item is stale without a walk (`refresh()`, the items changed, is one too, and so is a new
  text layout, the renderer's `textLayoutSerial()` moving as a runtime `setTextAA` moves it,
  which `placeVariable` reads with the width, as `Text` keys its measures; a canvas scale
  is none, since `measureText` gives the same units at every scale); the layout of the resize measures the items on screen
  at once and keeps the anchor, and the rest are measured by the **idle re-measure**,
  an animation registered only while some item is not measured at the generation (a
  new width, items added or never shown), up to `setRemeasureBudget` items a frame (64;
  0 measures only what shows), from the item at the top down and then from the first,
  through `firstStale`. Its new heights move the offset and the thumb through the
  anchor; once every item is measured it finishes, so a settled frame runs, measures
  and lays out nothing. Meanwhile the thumb creeps; nothing on screen moves. A shown
  row whose bounds or preferred height is not its item's measure (a measurer that
  disagrees with the row) is reported by layout verification, through
  `Base::verifyOwnLayout` (`docs/layout.md`). `ControlTest --scenario
  virtual-list-variable` holds all of it on VirtualListVariableDemo at 1 000 and 100 000
  items, `tests/unit/virtualListVariable.cpp` on the headless fixture, and the
  `vlist-variable-scroll` bench scene is local. Wrapped rows given an item with another
  line count gain or lose line `Text`s, so a scroll may renumber z inside the list.
- **The code editor: the lines on screen, in cells.** `controls::CodeEditor`
  (`include/gweni/controls/codeEditor.h`) is a `VirtualList` in fixed mode (in its
  variable mode once wrapped, below) over a `text::Document` it owns (`getDocument()`; "The document and undo model"): one
  `CodeLine` row per line in view plus one, each holding its line as **one text run**
  (an `internal::CodeLineText` in the editor's font, coloured by its line's tokens
  with `setLanguage`: "Syntax highlighting") and two primitives of its own, a line
  background (`CodeCurrentLine` on the caret's line) at the row's z and the selection
  rect at the Text's z, behind every glyph (`CodeSelection`). The row height is the
  font's `lineHeight()`, so a document of 100 000 lines has the controls, ids and slots
  of the forty or so on screen, and a frame costs what they cost
  (`tests/unit/codeEditor.cpp`: the pool and the live ids the same at 1 000 and
  100 000 lines). The document's observer maps each `LineChange` onto the list: lines
  replaced in place are `refreshItem`ed (their rows bound again), lines added or
  removed are `insertItems`/`removeItems` (the rows below shift to their new lines by
  index arithmetic, unbound, and a line that came into view gets a row in the next
  layout), so a keystroke rewrites its line's run and a line break mid-screen moves
  the rows below it, nothing past the screen. The content's width is the longest
  line's, in cells, kept as a count of lines per width (`m_widthLines`), the new
  lines' widths added before the old ones are taken away, so an edit never scans every
  line; its vector of per-line widths grows to powers of two, as the document's own
  vector of lines does, so the first line break after a new text copies nothing. The
  `code-edit` and `code-scroll` bench scenes hold every per-frame counter at
  N=10 000 to N=1 000 (`bench_scaling`).

  **Cells.** The font is the skin's mono face (`getDefaultMonoFont()`, DejaVu Sans
  Mono), a fixed-width face whose runs lay every codepoint in whole terminal cells
  ("Font fallback"), so every column is `text::cellCount()` cells: 2 for a wide
  glyph, 0 for a combining mark, 1 otherwise (`text::stringCells`). The cell is the
  advance of `'0'`, read as `measureText` of 32 zeros over 32 (an SDF run's
  fractional advance reads as its average), with the line height, in the editor's X
  `calculateSize` under a `PlacementScope`, once per font, renderer, text layout
  (`textLayoutSerial()`) and canvas scale; a column's x is
  `TextInset + floor(column*cellWidth)` on every line, so a line of CJK keeps the
  lines below on the same columns. The caret, a click (`text::byteNearestCell`), the
  selection, Up and Down's goal column (`text::byteAtCell`: before a wide glyph the
  column falls inside) and the tab stops are all in cells. A caret stop is a
  codepoint boundary not followed by a combining mark (`text::nextCaretStop`,
  `prevCaretStop`; `caretStopAt` snaps a position inside a codepoint or before a
  mark), so a mark moves with its base. Malformed UTF-8 (a Latin-1 file, a cut
  sequence) is read as `utf8CodepointAt` reads it, each stray byte one U+FFFD and
  one stop, walking forward and back alike (`text::utf8PrevCodepoint`), so the
  caret crosses it a byte at a time and Backspace removes it a byte at a time. A cell width that changes after the first read (a font of another
  advance) marks the rows holding the selection and the caret again, since their
  rects are in cells even where the rows keep their bounds.

  **Caret and keys.** The caret and the selection's other end are document positions
  (`getCaret()`, `getAnchor()`), so a selection whose ends are off screen survives
  scrolling and edits of other lines. The caret is drawn by the editor's own renderer
  at `getOverlayZIndex()` (`overlayLayers()` 1), in front of every row, one unit wide
  and a line high, cut to the viewport (`getCaretBounds()`); it blinks through
  `internal::CaretBlink` (`include/gweni/controls/caretBlink.h`), the animation
  `TextBox`'s caret uses too, registered while the editor has the focus, each edge a
  `StateChange_Caret` that rewrites the caret's slot alone (the animation calls the
  control's `Base::updateCaretBlink()`, so it has no member and costs a TextBox no
  more than its own did). Arrows, Home (the first non-blank, and from there column
  0), End, Ctrl+Home/End, PageUp/PageDown (the view and the caret by the viewport's
  whole lines; `keys::PageUp`/`PageDown` reach a control as
  `Base::onKeyPageUp`/`onKeyPageDown`), Ctrl+Left/Right (blanks, then a word of
  letters, digits, `'_'` and non-ASCII, or a run of punctuation, and line ends),
  typing (UTF-8), Backspace, Delete, Return (keeping the line's leading blanks up to
  the caret) and Ctrl+C/X/V through `platform::setClipboardText` and
  `requestClipboardText` (see "The clipboard").
  A move puts the caret in view: its line through `scrollToItem` when the last
  layout left it out of view (a move inside the view lays nothing out), its column at
  once with a cell to spare.

  **Selection.** Each row holds the cells of its line the selection covers (its end
  one cell past the last glyph when the line's break is selected too), set by the
  editor over the pool when the selection changes, a row is bound or lines shift, and
  draws it as one rect, `columnX(begin)` to `columnX(end)`, a line high, at any scale
  (`tests/unit/codeEditorSelection.cpp`). Shift with the keys extends it from the
  anchor; a press and a drag select to the mouse, and with the mouse past the
  viewport's edge an animation scrolls the view on the canvas clock (`AutoScrollBase`
  plus `AutoScrollRate` units a second per unit past the edge, the fraction carried),
  the caret with it; Shift+press extends; a double-click selects the word, the
  punctuation run or the blanks under the mouse; Ctrl+A selects everything;
  `getSelectedText()` is the document's `textIn(getSelection())`.

  **Undo, Tab, read-only.** Every edit the editor makes goes through one path, which
  records it on a `text::UndoStack` with `TextBox`'s coalescing (typing, the two
  deletes, Return, paste and cut each their kind); Ctrl+Z, Ctrl+Y and Ctrl+Shift+Z
  apply a step's inverses or records and give back its selection, the caret in view.
  An edit the app applies to the document (or `setText`) clears the history. Tab
  inserts spaces to the next multiple of `setTabWidth` (4) cells, in place of a
  selection inside one line; over several lines it indents each by the tab width,
  and Shift+Tab outdents the caret's line or the selected lines to the stop before
  their indent (a leading tab character is one stop), one undo step. Tab never moves
  the focus out of the editor; Escape arms the next Tab or Shift+Tab to (any other key
  disarms it), and `setTabInsertsSpaces(false)` gives Tab back to the focus.
  `setReadOnly(true)` draws no caret and no current line and makes no edit, the
  moves, the selection, Ctrl+A and Ctrl+C left working, and Tab moving the focus
  (`tests/unit/codeEditorUndo.cpp`). In a page emscripten-glfw prevents the default
  of the keys it hands the canvas, so Tab moves no browser focus and Ctrl+Z runs no
  page undo (`web_input`'s `codeeditor` check).

  **Line numbers.** `setLineNumbers(true)` adds `internal::CodeGutter`, the editor's
  own child (made with the inner panel set aside, so it is not in the scrolled
  content), in a left padding the editor keeps for it: `2*GutterInset` plus the line
  count's digits in cells. One right-aligned `internal::CodeLineNumber` per pooled
  row, paired with its row by control id (a deleted row's address can come back as a
  fresh row's, which would hand the fresh row a used number), so a row the pool
  drops takes its number along and a resized gutter holds what a fresh one does; the
  numbers are bound with the rows in `contentsPlaced`. The gutter does not scroll
  sideways, takes no mouse (a press on it, or anywhere left of the viewport, is at
  column 0 of the line beside it, however far the view is scrolled), and a new digit count (999 to 1000 lines) is a new padding of the
  editor's, set in its X `calculateSize` under a `PlacementScope`, which lays out the
  editor alone (`tests/unit/codeEditorGutter.cpp`: the same layout entries with and
  without a panel of Labels beside it). The theme's `CodeEditor`, `CodeEditorFocus`,
  `CodeGutter`, `CodeCurrentLine` and `CodeSelection` surfaces and the
  `CodeLineNumber` and `CodeLineNumberCurrent` text colours
  (`Colors.CodeEditor.LineNumber`, `LineNumberCurrent`, read again by the numbers'
  `updateColors()`) are in the ThemeEditor and strict mode. CodeEditorDemo (the
  shell's "Code editor", and in the README gallery) holds 5000 generated lines of YAML,
  highlighted; `controltest_code_editor` holds a typed character, an undone burst, a
  block scalar opened and undone, Return and a scroll to the part of the editor each may
  touch.

  **Wrap.** `setWrap(true)` wraps every line at the rows' width, counted in cells
  with no measuring: `text::wrapCells` (`include/gweni/text/cellWrap.h`) gives a
  row the caret stops that fit in `wrapCellsFor(width)` cells (the width less
  `TextInset` on both sides), a word that does not fit going to the next row whole
  and one wider than the row cut between stops; blanks (`' '`, `'\t'`) always fit,
  so a run of them past the edge hangs on the row it ends, as a textarea's spaces
  do, and a row never starts with the blanks that ended a word; a stop wider than
  the row (a wide glyph at 1 cell) is a row of its own (`tests/unit/codeEditorWrap.cpp`).
  The horizontal bar goes, and the list goes to its variable mode ("The virtual
  list's variable mode"): a line is one item, its height its visual rows times the
  line height. The measurer is `wrapRowCount` at the rows' width (O(line)); the
  estimator a line's cells over the wrap cells (O(1), from the widths the editor
  keeps anyway), and every line is estimated again whenever the wrap width or the
  line height changes (`applyWrapMeasurer`, from `contentsPlaced` before the list
  places its rows): O(N) arithmetic once per width, the lines on screen measured by
  that layout, the rest by the idle re-measure. An edit measures again only the
  lines it replaced (`itemChanged`), so a keystroke costs its line's wrap whatever
  the document's length (the `code-edit-wrap` bench scene, local: it has no line
  numbers, since a gutter as wide as the line count's digits makes the wrap width
  depend on the count). A `CodeLine` shows each visual row as a `Text` of its own,
  a line height below the one before, made as its line needs them and deleted as it
  needs fewer (a spare row keeps none), so a row holds what a row bound to its line
  at that width holds; highlighted, each takes the line's colour spans over its own
  bytes, counted from its start. Toggling wrap, and a new size that wraps again or a new
  viewport height, keep the caret's line on screen (`scrollToItem` of it when it was
  shown), and of a line taller than the viewport (one paragraph in a short field) the
  caret's visual row: `revealCaretRow()` asks the next layout for it, and that
  layout's `contentsPlaced`, once it knows the wrap width, gives the row's place in
  the line to `scrollToItem(line, partTop, partHeight)`, while the caret's request is
  still the list's last.

  **The latest scroll request in a frame decides the view** (#661), wrapped or not,
  as it does in the list ("The virtual list"). The editor records
  `VirtualList::scrollRequestSerial()` after the caret's own request (a
  `scrollToItem()`, or wrapped an offset), so it knows whether the last request is
  the caret's rather than guessing from the pending item's index. A move judges the
  caret in the view a bar the user moved before it gives (`takeBarScroll()` first). A
  caret move that needs no scroll re-targets a `scrollToItem()` still pending (an
  earlier move's in the frame, the app's) at its own line, which the layout finds in
  view, so of two moves in one frame (Ctrl+End, Ctrl+Home: a scripted `--key` run, a
  browser's input between frames) the later decides; a `scrollToItem()`,
  `scrollByRows()`, offset, wheel notch or bar move after the move is where the view
  ends (the list drops the caret's pending item), and so is one in a resize frame,
  over the resize's keeping of the caret's line; a caret request of the frame, by
  offset too, puts its row in view at the new width even when no row showed its
  line; an app's edit that merges the caret's line into the one above (the list's
  request moved to the line after it) moves the caret's request to the caret's new
  line (`tests/unit/codeEditorScrollRequests.cpp`).

  **Visual rows.** Wrapped, Up and Down move a visual row at a time, inside a line
  and across lines, the goal column a column in cells on the row; Home and End go
  to the visual row's start and end first, then to the line's (Home's first
  non-blank and column 0, End its end); a click lands on the visual row under it
  (`HeightIndex::itemAt` of the point's offset in the content, the row the line
  height's step into the line); PageUp and PageDown move the view and the caret by
  the viewport's rows. A byte at a wrap point is both the end of the upper row and
  the start of the lower one: the caret is drawn at the upper row's end after End,
  a click past a row's end or Up/Down landing past it (`isCaretAtRowEnd()`), at the
  lower row's start after any other move or an edit of its line (an edit of another
  line keeps the look), and `getCaretRow()`/`getCaretBounds()` follow. A move puts the caret's visual row in view through the scroll offset (the
  row's top is the line's `offsetOf` plus its rows above), or through
  `revealCaretRow()` for a line not measured at this width or a row past the last
  layout's content (an auto-height field grown past its max by the keystroke)
  (`tests/unit/codeEditorWrapEditing.cpp`). Wrapped, the selection draws one rect
  per visual row it covers (`CodeLine::getSelectionRects`), between its cells on
  that row from the row's start: a row it goes on past is covered to its last cell,
  at most the wrap cells (hanging blanks are cut at the edge), the line's last row
  one cell past its last glyph when the break is selected, and a selection reaching
  a wrap point covers nothing of the row beyond it; the rects are in units, the
  same at any scale, and the selection stays in document positions. The textured
  `CodeLine` renderer takes a slot per rect after the line background, two at least,
  and gives the rest back, so an unwrapped row keeps its two
  (`tests/unit/codeEditorWrapSelection.cpp`).

  **Auto-height.** `setAutoHeight(minLines, maxLines)` (`maxLines` 0 turns it off)
  makes the editor's preferred height its visual rows (its lines, unwrapped)
  clamped to `[minLines, maxLines]`, times the line height, plus its padding. They
  are counted in the editor's Y `calculateSize`, which runs once every width is
  final (`docs/layout.md`), at the width with no vertical bar, and the count stops
  past `maxLines`, so a layout of the editor costs the wrap of the lines up to that
  many rows whatever the document's length. Vertical scrolling turns on only past
  `maxLines` (`setScroll`'s vertical, under a `PlacementScope`), so a form field
  shows no bar until it needs one; since a narrower width only adds rows, the bar
  past the most rows changes nothing of that answer. A row more or fewer is a new
  preferred height of the editor's, which its parent lays out in the same pass: the
  editor's subtree and its ancestors are entered, and each field a dock moves is
  entered once (its own arrange), none of their subtrees
  (`tests/unit/codeEditorAutoHeight.cpp`: a form of 20 fields below the editor
  costs the growth 18 entries more than one of 2).

  **Plain mode.** `setPlain(true)` is the form-field preset, a textarea: wrapped, no
  gutter, no current-line background, the theme's text-field look (the `TextBox`,
  `TextBoxFocus` and `TextBoxDisabled` surfaces) and Tab moving the focus
  (`setTabInsertsSpaces(false)`), as in a browser; code mode keeps capturing Tab.
  It is still monospace, every column a cell of the mono face, as everywhere in the
  editor; a form field sets no language (`setLanguage`, "Syntax highlighting").
  `setPlain(false)` gives back the code look, the current line and Tab's editing,
  and leaves the wrap and the gutter as they are. CodeEditorDemo's "Wrap lines"
  toggle wraps its document (each agent's prompt line wraps); CodeEditorFormDemo
  (ControlTest's `--list`) is a form of plain auto-height fields, a description of
  3 to 8 rows and a comment of 2 to 6. `controltest_code_editor` goes on wrapped
  through the toggle (a typed character keeping its line's rows writes its row, the
  gutter and the caret alone, one adding a row moves the rows below inside the
  editor, the toggle and a smaller canvas keep the caret's line on screen, a wrapped
  line's selection draws its fill on each visual row) and ends on the form's
  comment field growing a row in one pass.
- **The log view: an append-only log the view owns.** `controls::LogView`
  (`include/gweni/controls/logView.h`) is a variable-mode `VirtualList` (see "The
  virtual list's variable mode") over a row store it owns: a `std::deque` of
  `{RowId, LogRow}`, a `LogRow` being a kind, a text (a block's title), a block's lines,
  an optional colour and a block's `collapsed`. `append()` numbers rows up from 0,
  `prepend()` (older rows before the first, a dashboard's "Show all") down from below
  the lowest id ever given, so no id is given twice, not even one the cap dropped. Ids
  rise with the rows' positions, so `rowPosition(id)` is the id's distance from the
  last row (any appended id) or the first one, O(1), and a binary search only for an
  id in a prepended run whose oldest rows were dropped before a later prepend.
  `replace(id, row)` is by id, since positions move, and re-measures that row alone
  (`itemChanged`); an id the view no longer holds returns false. `setMaxRows(n)` is
  applied by the next append, `removeItems(0, k)` on the list, O(log N); the anchor
  keeps what is on screen (`tests/unit/logViewRows.cpp`).

  **Rows.** The pool's rows are `internal::LogViewRow`s, wrapped `Label`s in the view's
  font (held by pointer, so `setFont` changes it in place and tells the rows), padded
  `RowPadX`/`RowPadY`, measured as `Base::wrappedTextHeight` of their text at the row
  width less the padding, the wrap the row's Text does. A block row is the same row
  with a left padding `ChevronColumn` wider and a `LogViewChevron` in it (icon kind 1,
  right or down, in the row's text colour), made the first time the row shows a
  block; its text is `blockText()`: `"title (4 of 120 lines)"` and the first
  `getBlockPreviewLines()` lines while collapsed, `"title (120 lines)"` and every line
  expanded. A press on the title line toggles it (`setBlockExpanded`), one
  `itemChanged`, so only that row's height changes and the rows above it stay where
  they are. A new `setBlockPreviewLines()` is one measure generation (`refresh()`),
  as a new width: the next layout measures the rows on screen, the idle re-measure
  the rest. An app's kind (`setRowKind(kind, {factory, binder, measurer})`, from
  `LogRow::FirstAppKind`) hides the row's text and shows the kind's content, made by
  its factory once per pooled row the first time the row shows the kind, placed inside
  the padding at the kind's measure (the row's preferred height is the list's height
  for its item, the kind's contract being that the content lays out at it); a kind
  registered again deletes the pooled rows' content of the old factory, so the new
  binder only fills what the new factory made. The binder
  sets content only, padding and which children show included, as `VirtualList`
  requires (`tests/unit/logViewBlocks.cpp`). An expanded block is one row, so it makes
  a line `Text` per wrapped line of all its lines while on screen.

  **Following.** Following is the list stuck to its end (`setStickToEnd`), so an append
  puts the last row's bottom at the viewport's bottom in its own frame. It is on by
  default and every scroll up detaches it: a wheel notch up (before the list scrolls,
  which stuck would put back), Up, PageUp and Home (the view takes the keyboard),
  `scrollToRow()` of a row above the last, the thumb dragged (`onVeritcalBarMoved`) and
  the bar's arrows or track, which tell nobody. A bar the user moved since the last
  layout (its amount against `m_shownAmount`) is taken as the offset by whatever needs
  the offset first, `VirtualList::takeBarScroll()`: an edit (`append`, `replace`,
  `prepend`, the cap, a block toggled), a scroll request, the idle re-measure in the
  update phase, or the list's own Y arrange, which takes it under a `PlacementScope`
  before `placeContents()` would. Each tells the one hook `barScrollTaken(amount)`,
  which the view overrides: at the end it follows, above it it detaches, so a press
  on the arrows during a re-measure after a resize detaches as any other does. Detached,
  appends leave every row on screen where it is, and an `internal::LogViewJumpButton`
  (the view's own child, made with the inner panel set aside the first time the view
  detaches, never taking the keyboard focus) shows centred `JumpGap` above the
  viewport's bottom with the count appended since (`jumpText()`), relabelled once a
  frame by the layout after the appends. Its press, End, `scrollToRow()` of the last
  row and a scroll down (wheel, Down, the bar) to the end follow again. A log whose
  rows all fit (`fitsInView()`, the last layout's maximum offset 0) has nothing to jump
  to: a scroll up there detaches nothing, `clear()` follows again, and a detached view
  whose rows come to fit (capped, replaced shorter, made taller) follows again in the
  layout that finds it (`contentsPlaced`, under a `PlacementScope`).
  `onFollowingChanged` fires on every change, possibly from the layout (the arrows'
  path), so a handler only reads (`tests/unit/logViewFollow.cpp`).

  **Selection.** The selection is ranges of `Position{row id, character}` ends, the
  character a byte of the row's `rowText()` (a block's title and every line, collapsed
  or not), a whole row from 0 to its length. A click selects the row whole, as a
  row-based log does (#594). A press-drag selects characters once the mouse leaves the
  pressed one, from it to the character under the mouse (`positionAt()`: the row under
  the point, then `Text::getClosestCharacter` on whichever wrapped line of the row's
  Text is under it), across wrapped lines and rows; back on the pressed character the
  row is whole again. Past the viewport's top or bottom the drag scrolls on the canvas
  clock (CodeEditor's speeds), the focus the edge row's start or end (the rows there
  are placed only by the next layout), a scroll up detaching the view, and a
  selection alone never does. Shift+click (and a Shift+press drag) moves the last
  range's focus to the character clicked, its **anchor unit** kept whole: what the
  range started from, the row a click selected, the character a drag was pressed on
  or the word a double-click selected (`m_anchorLow`/`m_anchorHigh`), the range
  running from the unit's far end when the focus is before it, so a Shift+click above
  a clicked row still takes that row. A double-click selects the word under it
  (`selectWordAt`: a run of letters, digits, `_` and non-ASCII codepoints, of blanks or
  of punctuation, by caret stops, a `'\n'` part of none, the run before it at a line's
  end) and a drag from it extends with the word kept; with Shift or Ctrl, on a
  block's title line or on a row of an app's kind (whose content has no characters
  to pick a word of), it is a press. Ctrl+click toggles a row whole (splitting a range
  round it), Ctrl+A selects every row and Escape clears. A block row shows
  `blockText()`, its rows' text being `rowText()`: `shownToRowCharacter()` maps a byte
  of the shown text to it (the count after the title is the title's end, the lines
  after it their own bytes) and `rowToShownCharacter()` back (a collapsed block's
  hidden lines at its shown text's end), so a range starting or ending inside a
  collapsed block copies the characters it covers, one running past its end the
  hidden lines too, and one over all of it the block's whole text. Ranges keep their
  ids, so scrolling, binding, appends and prepends change nothing; the cap drops a
  range with no row left and moves a dropped low end to the first row held; `replace`
  clamps the characters on its row (and the anchor unit's), an end at the row's end
  staying at its end. Ctrl+C and the context menu's Copy (the view's, made on its
  first right press, a right press on an unselected row selecting it; its items carry
  no accelerator, which the canvas-wide accelerator search would find) put
  `getSelectedText()` on the clipboard: each row's covered bytes, the union of the
  ranges' parts of it, joined by `'\n'` (`tests/unit/logViewSelection.cpp`,
  `tests/unit/logViewCharSelection.cpp`).

  **The highlight.** Only the pooled rows draw it, so its cost is the rows on screen
  however many rows the selection holds. Each is told what of its row is selected
  (`LogViewRow::Selection`: whether a range touches it, its covered bytes in the shown
  text, whether a range runs on from the row above or past its end) when bound and
  when the selection changes, and marks `StateChange_Background` only when that
  changes (or, bound while selected in part, always: its rects follow the text the
  bind set). `getSelectionRects()` gives the rects in the row's coordinates: a row
  selected whole, alone or with the selection running on past its end (from the row
  above or from its own first character), is one rect over its bounds, #594's band,
  so its left edge is the rows' below it; a row of an app's kind is that rect while
  the selection covers any of it, and nothing otherwise; any other a rect per wrapped
  line the selection covers, from
  the caret at its first covered byte to the caret at its last (`getCharacterPosition`
  of the row's wrapped Text), the row's left edge where the selection comes from the
  line before, its right edge where it runs on past the line's last caret (its wrap
  point, its `'\n'`, or into the next row), each line's row the view's line height,
  the row's top and bottom padding included where the selection runs into the row
  above or below, so a selection over several rows is one shape. The textured
  `LogViewRow` draws `LogViewSelection` in a slot per rect at its Text's z, behind
  every glyph (the slots past the rects given back, the first hidden when there are
  none), so a drag inside one row writes that row's slots alone and a settled frame
  nothing.

  **Cost.** A frame costs the rows on screen. Following, an append frame moves every
  row on screen, and a moved run is a fresh layout ("A moved run is a fresh layout"),
  so it rewrites a screen of glyph slots: in the `log-append` bench scene (600x300, 17
  rows a frame) 39 controls changed, 655 glyph slots and 1 341 text measures a frame
  at N=1 000 and at N=100 000 alike. Detached (`log-append-detached`), a frame writes
  the thumb's slot and the jump button's glyphs alone; capped (`log-append-capped`),
  each append drops one row through the height index. A small excerpt is a `LogView`
  `heightForRows(5)` high with a small cap: its own pool, nothing per row of its log.
  The theme's `LogView`, `LogViewFocus` and `LogViewSelection` surfaces are the field
  and the highlight. LogViewDemo (the shell's "Log view", the README gallery) streams
  600 events from an animation on the canvas clock (paused while the demo is hidden)
  beside three five-line excerpts; `controltest_log_view` holds appends, a detach, the
  jump button, a replace above the viewport, a resize, an expand, a copied selection,
  a character drag (a frame inside one row writing that row's highlight slots alone),
  its copy, scrolled off and back, a double-clicked word, a Shift+click and the cap to
  what each may touch at 1 000 and 100 000 rows.
- **The slider's value text.** `Slider::setValueFormat("%.2f")` shows the value as text
  (imgui's SliderFloat), formatted with `snprintf` from `getFloatValue()`. The format is
  checked first (`Slider::validValueFormat`): exactly one conversion of a double (`f F e
  E g G a A`, with the flags `-+ #0`, a width and a precision of at most 2 digits each,
  no `*`, no length modifier), plus any text and `%%`; anything else is refused with one
  stderr line and the old format kept, so an app string is never passed to `snprintf`
  unchecked. The text is an `internal::Text` child, created by the first non-empty
  format and deleted by an empty one, so a slider with no format takes no text id. It is
  owner-placed (`placeValueText()`, from the subclasses' `layout()`) and added after the
  thumb, so it is numbered, and drawn, above it. **Horizontal:** the textured `Slider`
  renderer draws the track as a field the thumb's height (`ThumbThickness`, at most the
  slider's) and the slider's length, in `SliderTrack`, with the thumb moving inside it and
  the text centred over both in `Colors.Label.Default` (`Colors.Button.Disabled` while
  disabled); the notch ticks go below the field, which shrinks to leave them room, so a
  notched slider with a value text wants about 10 units more height than the field. The
  slider asks for at least the text's line plus 2 high. **Vertical:** the text sits in a
  column right of the track, `ValueTextGap` (4) from it and vertically centred; the column
  is the wider of the formatted min and max (`getValueTextColumn()`, measured in the X
  pass from the range, never the value), and the slider asks for its request plus the gap
  and the column, so it keeps its width while it is dragged. `getTrackBounds()` is the
  part of the slider the track, thumb and ticks are drawn in: all of it but that column.
  **Only a new string costs anything:** `setValueInternal` sets the text when the value
  changes, and `Text::setText` returns at once for the same string, so a drag, a key step
  or a `setFloatValue` that keeps the shown string marks nothing text-related and writes
  no glyph slot (the thumb's one slot moves); a new string rewrites that run, at most its
  glyphs, and lays the slider out again to re-centre it. `setRange` re-formats the text
  and re-measures the column. With no format every path is as before: the same track,
  thumb and ticks, byte for byte. SliderTextDemo shows `%.0f`, `%.2f`, a percentage, a
  notched and a disabled slider and a vertical one; ShowcaseInputDemo has one.
- **Notebook tabs.** In the textured skin a TabControl's tabs show which page is open by
  their shape (#540). The page frame is the `TabControlInner` panel, the `TabControl`
  surface in one flat fill; the `TabControl` itself has no primitive and draws nothing, so
  the strip and the row past its last tab show the parent's background. The strip reaches
  `TabStrip::PageOverlap` (1) unit into the page on the side facing it (a negative margin
  there, set by `setTabPosition`), so each tab's last row (a Left or Right strip: column)
  is the frame's border row, and its row starts `TabStrip::LeadingPadding` (6, the frame's
  compiled corner radius) units in, so a first tab meets the frame's straight border and
  not its corner arc (at 1.25, where `snap(6)` is 8 px against 7.5, its corner is on the
  arc's last px, a twentieth of a px off the straight edge; a theme with a larger
  `TabControl` radius puts it on the arc). Every tab is drawn with the edge mask (`paintSurface`'s overload taking a
  rect and a `RoundedEdge` mask): square on the two corners facing the page, no border on
  that side, and drawn `PageOverlap` short of the page, up to the frame's outer edge.
  The selected tab is the rest of its bounds in `TabActive`, the frame's fill and border,
  drawn over the frame (the frame is sent to the back) and reaching over the frame's
  border from there by that border's width (`drawRoundedRect`'s reach, the `TabControl`
  surface's `borderWidth`), so it covers exactly the frame's border rows and tab and page
  are one outline with no accent at any scale: its bounds alone would cover 1 or 2 device
  px of a 2 px border at 1.5 by their position, leaving a border line across its opening
  or its side borders a px into the page (`tests/tabJoin.cpp` checks the pixels at 1,
  1.25, 1.5, 1.75 and 2 on the four sides). An unselected one is `TabInactive` (darker
  than the page), `TabHover` while hovered, with no reach, so the frame's border runs on
  under it, drawn `TabButton::UnselectedOuterInset` (2) shorter on the outer side and
  `UnselectedEndInset` (1) in at each end along the row. That last inset is what keeps
  a selection to the two tabs: tabs overlap their neighbours by a unit and a later tab is
  in front, so one drawn whole would cover the selected tab's end border, and drawing it
  to suit its neighbour would make a selection rewrite the neighbours too. What a tab draws
  follows only its own state (selected, hovered, disabled, which looks unselected and does
  not hover) and its strip's side, so a selection writes the old and new tabs' slots and
  their glyphs, never the frame's, and a hover one slot; `setTabPosition` marks the tabs
  `StateChange_Background` when the side changes, since a tab's bounds can stay as they
  were (Top and Bottom place it alike). The text and the bounds are the same in every
  state. `tests/unit/tabStripPlacement.cpp` checks the placement on the four sides,
  `tests/unit/tabLook.cpp` the rects, masks, surfaces and slots.
- **Closable tabs.** `TabButton::setClosable(true)` gives a tab a `TabCloseButton` child
  (imgui's `BeginTabItem(..., &open)`), a `WindowCloseButton` subclass, so it draws with
  the window close button's renderer, found up its parent types: the cross (icon kind 2)
  in `FgDark`, a red `WindowCloseHover`/`WindowCloseDown` surface on hover and press,
  nothing at rest. It is owner-placed in a `TabButton::CloseButtonSize` (14) square at
  the start of the tab's right padding: `calculateSize(X)` adds the square to the padding
  it sets for the strip's side, so the text is laid out clear of it on every side (on a
  Left/Right strip, where a tab fills the strip, the square is at the tab's right end),
  and the padding is set once, final, so a measure that keeps it changes nothing.
  `arrange` sizes and places it, one axis at a time, centred between the top and bottom
  padding, once that axis's size is final and before the tab's children are arranged
  (placed in `postLayout`, after the tab's own arrange, its first placement left the way
  down to it dirty after the pass, for whatever next pass to enter and count; the canvas
  now lays such a leftover out in a second pass of the same frame, `docs/layout.md`). The square is reserved whether or not the button shows, so a hover never
  changes a tab's width or moves its neighbours. **Shown like imgui's:** on the selected
  tab and on the hovered one, the tab or the button itself under the mouse
  (`isTabHovered()`, which the tab's text colour follows too), and on no disabled tab.
  `updateColors()` decides it, so every `redraw()` (hover, press, a selection change;
  `TabControl::onTabPressed` redraws the tab it selects, also from a key or a close) and
  the button's own enter and leave (`onDescendantInteraction`) settle it with a
  `setHidden`: a hover writes the button's glyph slot and nothing outside that tab. The
  mouse moving from the tab onto its button leaves the tab before it enters the button,
  so for that one frame the button is hidden and shown again, which writes nothing.
  **Closing:** the press goes to the button, which has no drag package, so it neither
  selects the tab nor starts a tab drag; its click is `TabControl::requestClose(tab)`,
  which fires `onClose` (`controlCaller` the TabControl, `control` the TabButton). A
  handler that calls `vetoClose()` keeps the tab (an "unsaved changes" prompt); otherwise
  `closeTab(tab)` runs: if the tab was selected, the next tab is selected, or the previous
  one when it was the last; it leaves through `removePage` (so `onLoseTabCaller` fires),
  and the tab and its page are hidden and `delayedDelete()`d, so a close from the
  button's own click handler is safe and their ids are given back at the start of the
  next frame. `closeTab` closes without asking. TabCloseDemo has a pinned tab among
  closable ones and a Left strip; ShowcaseContainersDemo's TabControl has two closable
  tabs.
- **Tab overflow and the tab list.** A Top or Bottom strip whose tabs are wider than the
  `TabControl` **overflows**. Like any control a TabControl is as wide as its content, the
  whole row of tabs, so it overflows only where something makes it narrower: its size
  flags (Fixed, Shrink), a dock or a layout cell. `TabControl::arrange(X)` decides it,
  before the children are placed, from the control's width (final then) and the strip's
  preferred width: the left and right `internal::ScrollBarButton`s (`TabControl::ButtonSize`,
  14, square, chevrons of icon kind 1) show in a column at the strip's trailing end,
  `ColumnInset` (2) in from the right edge and `ButtonSpacing` (2) apart, and the visible
  tabs end `ColumnGap` (4) before it (`getTabsRight()`). The strip, whose tabs do not clip
  themselves, is clipped there and to its left padding through `getChildClipRect`, so no
  tab draws or takes the mouse under the column, and a clip that changes without the
  strip's bounds marks the strip's subtree `StateChange_Geometry`. `arrange(Y)` centres the
  buttons on the row once its height is final. The buttons are owner-placed, measured even
  while hidden (`calculateSize`), since the X arrange may show one. **Scrolling:** the
  offset is `TabStrip::setScrollOffset`, which moves the row's origin in the strip's own
  `arrangeHorizontal` and nothing else: the strip's preferred size, and so the
  TabControl's, is the whole row's at any offset, so the offset can be clamped in the X
  arrange without making any preferred size depend on it. A press on the right button
  scrolls by one tab, to the next tab's start along the row (from the tabs' preferred
  sizes and margins), so that tab sits where the first one starts; the last step stops at
  `getMaxScrollOffset()`, where the last tab's right edge is at the column, and the left
  button goes back a tab at a time to 0. A press only invalidates the TabControl and the
  strip: the tabs move (their slots and glyphs are rewritten), the page is not entered.
  Selecting a tab (a click, a key, a close, the list, code) asks for it to be scrolled
  fully into view by the least offset (`scrollToTab`), resolved in the next X arrange. A
  control that grows until the tabs fit, or closes tabs until they do, hides the buttons
  and the offset goes back to 0; one that only changes width keeps the offset clamped, so
  scrolled to the end it keeps the last tab at the column. A Left or Right strip never
  overflows. **The tab list:** `setTabListButton(true)` (off by default) puts a down
  chevron, a third `ScrollBarButton`, at the column's right end on a Top or Bottom strip,
  shown whether or not the tabs overflow (the tabs overflow when they run past the column
  of that button alone). Its press opens the canvas's `Popup` below it (`toggleTabList`,
  so a second press closes it) with one `Selectable` row per shown tab in strip order,
  captioned as the tab, the selected tab's row selected; the rows are made the first time
  and reused, the surplus deleted, each time it opens, never per frame. A row's click
  selects its tab, if it is still in the strip, scrolls it into view and closes the
  popup; Escape, a press outside and the button hidden close it as they close any popup.
  The popup is the TabControl's (`createPopup()`), so it is deleted with it, with no
  cleanup in `TabControl` (see "The popup"). TabOverflowDemo has twelve tabs in a Top
  strip with the list and in a Bottom one opened on its eighth tab;
  ShowcaseContainersDemo's TabControl has six tabs in a strip too short for them, with
  the list.
- **The colour field: ColorButton and ColorEdit.** `controls::ColorButton`
  (`colorButton.h`, imgui's ColorButton) is a Button with an empty caption drawn as a
  swatch: the textured `ColorButton` renderer paints its colour in one slot, through the
  same `ColorDisplay::paint` as the HSV picker's swatches (style 4 while opaque, style 7
  over `ColorChecker` cells while translucent), in `ColorSwatch`'s radius and border at
  rest and in `ColorButtonHover`, `ColorButtonDown` or `ColorButtonDisabled`'s border
  otherwise, so a hover, a press or a new colour rewrites that one slot and nothing is
  drawn at `z+1`. `setColor` marks `StateChange_Background` and sets the tooltip to the
  hex value (`toHex`: `#RRGGBB`, `#RRGGBBAA` while translucent) only for a new colour.
  It replaced `internal::ColorButton`, which had the same type name and drew as a plain
  button (its swatch draw was commented out), so `property::ColorSelector`'s swatch is
  drawn now, from its "r g b" or "r,g,b" value. `controls::ColorEdit` (`colorEdit.h`,
  imgui's ColorEdit4) is a row docked Left: the swatch, then four `TextBoxNumeric` fields
  R, G, B, A with a label each (`ColorEditMode::Rgba`), or one hex `TextBox` (`Hex`); the
  fields are Fixed (a TextBox is), 34 and 84 units wide. **Applied as typed:** a field
  that holds a whole value (a number 0-255, six hex digits or eight with alpha, the '#'
  optional, `ColorButton::fromHex`) applies it on its `onTextChangedCaller`; when it
  loses the focus (`TextBox::onLostFocusCaller`, so Return, Tab or a click elsewhere) a
  number is clamped into 0-255 and applied, and the field is written as the colour it
  holds, so text that is no value is reverted. Applying sets the swatch's colour and the
  other fields' text (`Label::setText` returns for the same string), so an RGBA field
  edit marks the swatch's Background and that field alone, and `onColorChanged` fires
  once per change of the colour, never for the same colour or number again.
  `setAlphaEnabled(false)` hides the A field, takes the hex field to six digits and makes
  the colour opaque. **The picker:** a click on the swatch makes (the first time) a
  `Popup` on the canvas holding an `HSVColorPicker`, gives it the colour and opens it
  below the swatch; a picker change applies its red, green and blue, keeping the alpha,
  and a colour applied from a field is given to the open picker under a guard
  (`m_settingPicker`) that drops the change events `HSVColorPicker::setColor` raises, so
  there is no picker, fields, picker loop. A second click, Escape or a click outside
  closes it, as for any popup. The popup is the ColorEdit's (`createPopup()`): deleting
  the ColorEdit deletes it, and a canvas tearing down deletes it once, whichever of the
  two comes first among its children (an open makes the popup a later child than the
  ColorEdit's window, and a `bringToFront` of the window can put it earlier again, #436),
  with no cleanup in `ColorEdit` (see "The popup", #490).
  ColorEditDemo has the two buttons and both modes; ShowcaseInputDemo has a
  translucent RGBA field and a hex one.
- **The data table: a frozen header over scrolled rows.** `controls::DataTable`
  (`include/gweni/controls/dataTable.h`, imgui's BeginTable) is a header
  (`internal::DataTableHeader`) above a body (`internal::DataTableBody`, a
  `ScrollControl` with no inset) of `DataTableRow`s, both owner-placed. It is not called
  `Table`: `controls::layout::Table` has that type name, and renderers are looked up by
  bare type name. **Columns** are table state: a caption, a width (`setColumnWidth`, at
  least `setColumnMinWidth`, 40 by default, room for a short caption and the chevron) and,
  with `setLastColumnStretch` (on by default), the last column grown to fill the body's
  width (never shrunk below its own). The stretch is the last column's so that a column's
  new width moves only it and the columns after it, while the columns fit the body: once
  they overflow it, every row is as wide as the columns, so a new width changes every
  row's width (and the body's inner panel's), but, the body keeping its horizontal
  scroll offset wherever it is scrolled (so the dragged grip moves by exactly the width
  change, issue #512), at the same origin and with the same region within the viewport, so `Base::onBoundsChanged` marks the row alone and the cells
  before the column are not marked (the header's cells are placed one by one and stay
  bounded; issue #511, `tests/unit/dataTable.cpp`, `dataTableVirtual.cpp`). `columnSpans(width)` gives every
  column's x and width across a width, and the header and every row place their cells
  (owner-placed) with it across the same width, the body's content width (its inner
  panel's), so a row's cells sit exactly under the header's. A row's preferred width is
  the columns' sum (`contentWidth()`), so the body scrolls sideways when they do not fit;
  a width change invalidates the header and every row, and a cell whose bounds do not
  change is not marked. **Layout:** the header's cells follow the body's scrolled
  content, which the body only knows once laid out, and a `ScrollControl` lays its subtree
  out in its Y arrange. So the table does as a `ScrollControl` does: it leaves its subtree
  alone in X, and in its Y arrange places the header and the body, lays the body out,
  then hands the header the content's origin and width (`setTrack`, as placement) and
  lays it out X then Y. The header clips its cells to the body's viewport
  (`getChildClipRect`), so they never draw over the vertical bar. **Resize and sort:**
  each header cell (a `Button`) has a `DataTableColumnGrip` along its right edge, 5 units
  wide with the resize cursor: a drag there sets the column's width from the width it was
  laid out at when pressed and the mouse's travel, and does not reach the cell, so it
  never sorts. Starting from the laid-out width is what makes the stretched last
  column's edge follow the mouse from the first unit (its asked width is less); dragged
  left, it asks for less and still fills the body. A click
  elsewhere on the cell sorts by its column, ascending first, then alternating
  (`setSortColumn(-1, SortDirection::None)` clears it), fires `onSortChanged` once per
  change (`point` is the column and 1, -1 or 0) and marks only the two cells whose
  chevron changed (`StateChange_Background`); the chevron's column is always kept in the cell's right text padding,
  so sorting changes no size. The table never sorts: the app orders its records and
  either sets the rows' cells again or puts the rows in the new order (`setRowOrder`,
  each row brought to the inner panel's end in turn; a `bringToFront` re-lays the
  control holding the row, here the inner panel, #486). Either is
  laid out in one pass. **Rows** are `Selectable`s in the table's `SelectableGroup`: one
  selected at a time (`getSelectedRow`, `onSelectionChange`), hover and selection drawn
  as a Selectable's. `setStriped` (on by default) fills every odd row of the row order
  (`DataTableStripe`); the stripes are set again on `addRow`, `removeRow` and
  `setRowOrder`, and a row whose stripe does not change is not marked. `setBorders`
  draws a kind-6 rule along each row's bottom (`Rows`), at each column's right edge but
  the last (`Columns`) or both, at the row's z+1; the header always has its rule along
  the bottom and its column rules (the grips), lit while hovered or dragged. The row's
  renderer resizes its slot list to its rules, and a row whose cells moved marks itself
  for its column rules. `removeRow` takes a row out of the group (firing the change if
  it was selected), hides it and deletes it with the canvas; a row deleted outright
  leaves the table too. `setCellContents` puts a control in a cell; the cell takes no
  mouse, the control as its own flag says, so a Button there takes its click and
  selects nothing, while a click on a Label or elsewhere on the cell selects the row.
  **Source mode** is the virtualised body, for tables of any length: `setItemCount(n)`
  and `setRowBinder((row, item))` (the first of either turns it on for good) drop the
  row-mode body and the rows `addRow` made (their selection with its event), and the
  body becomes a `DataTableVirtualBody`, a `VirtualList` (see "The virtual list") of
  pooled `DataTableRow`s of the table's columns, `setRowHeight` high (21, a row-mode
  row's at the default skin's text). So 100 000 items cost the controls, ids and slots of
  the rows on screen plus one, and a frame's counters are the same at any count. The
  binder fills a row for an item (its cells' text or contents) as the list binds it; a
  row still showing its item is not bound again. The body scrolls sideways too: its rows
  are owner-placed, which do not count in the inner panel's preferred size, so it gives
  the columns' width as its content through `ScrollControl::contentSize()`, the hook the
  bars and the inner panel are sized from, and the header follows it exactly as in row
  mode. Everything else goes by **item index**: a bound row is striped when its item is
  odd and selected when its item is the selection (`bindRow`, before the app's binder),
  so a stripe never moves with a pooled row; the selection is an item
  (`getSelectedItem`, `setSelectedItem`; `onSelectionChange`'s `integer` is the item, its
  `control` the row showing it or null), a press on a row selects the item it shows, and
  it is drawn on whichever row holds the item, none while it is scrolled out. A sort
  still only fires `onSortChanged`: the app reorders its own data and calls `rebind()`,
  which binds the rows on screen again in the next layout and only those
  (`rebindItem(i)` one), in one pass. The column API, resize included, reaches every
  pooled row (`bodyRows()`) and moves only the changed column's cells and the following
  columns'; a new item count keeps the rows still showing their item, the view inside
  the count, and drops a selection past its end. The row API (`addRow`, `removeRow`,
  `setRowOrder`) does nothing in source mode. **Row keys** (source mode only): the table
  takes the keyboard focus and is one Tab stop, and its pooled rows take neither (a
  pooled row gives the focus up when it is bound to another item), so a press on a row
  focuses the table, its first ancestor taking keys. Up and Down move the selection a
  row, Home and End to the first and the last item, PageUp and PageDown by `pageRows()`
  (the rows the viewport shows whole), clamped at the ends; each fires
  `onSelectionChange` and scrolls the item fully into view (`moveSelectionTo`, through
  `scrollToItem`), and with nothing selected every one selects the first item, End the
  last. A row-mode or empty table leaves them to its parents, and so does the table while
  another control has the focus: a key from a TextBox in a cell (`setCellContents`)
  bubbles up through the table and goes on past it, as a `Selectable` row passes on a
  Return that is not its own, so a dialog's default button gets it. Nothing draws the focus,
  so a table at rest looks as it did. The selection's change goes through a protected
  hook, `selectedItemChanged(row, item, doEvents)`, which fires the event by default.
  DataTableDemo has the first 40 elements, sortable by any column, and
  DataTableVirtualDemo 100 000 generated records in source mode, made into text only as
  a row binds them, sorted by any column on a vector of record numbers;
  ShowcaseContainersDemo has a two-column file table.
- **The tree table: a tree over DataTable's source mode.** `controls::TreeTable`
  (`include/gweni/controls/treeTable.h`; QTreeView with columns, GTK's TreeView) derives
  from `DataTable` and is always in source mode, so its columns, header, grips, sort
  chevrons and `onSortChanged`, stripes and borders are DataTable's, unchanged. **The
  hierarchy is data:** a node store of `Node`s (the app's id, which is also the handle,
  its parent or `Root`, its children in order, its depth, whether it is expanded and
  whether it has children, which `addNode(parent, id, hasChildren)` may say before any is
  given, for lazy loading), a `std::vector<Node>` with a hash map from id to index.
  `addNode` builds it; `setExpanded`, `isExpanded`, `expandAll`, `collapseAll` and
  `expandSubtree` change it, firing `onExpanded`/`onCollapsed` once per node that
  changed (`integer` the id, a hidden node's too); changing the hierarchy once shown,
  and sorting across it, are the next task's (#638). **The visible rows** are a
  flattened index, `std::vector<VisibleNode>` of (id, depth) in display order, the
  expanded tree in preorder (built iteratively, so any depth works): it is the
  source-mode item index, `setItemCount(visible.size())` and the row binder mapping a
  pooled row's item to its node and calling the app's node binder `(row, id, depth)`.
  Expanding a shown node inserts its visible subtree (its descendants under expanded
  nodes) after it and collapsing erases them, so each is a `memmove` of the entries
  after it; the bulk operations build the index once and compare it with the old one for
  the first entry that changed. Either way the pooled rows still showing an index at or
  past the first changed one are bound again (`rebindItem`), and only those: rows above
  keep their content, and no control or id is made or freed. Finding a node's index
  (`itemForNode`) is a scan of the index. **What that costs at 100 000:**
  `gweni_bench --scene tree-table-toggle` (a 50-row node collapsed and expanded under
  about N shown roots) counts under callgrind, over 20 frames, 0.62 M instructions in
  its input phase (the toggle: the scan and the memmove) at N=1000 and 3.45 M at
  100 000, about 0.17 M a frame, against 10.6 M in update and 32.6 M in render at both
  (the rows bound again and drawn). Timed (the profile tree, one run each under `render`
  and `quiet`), the input phase's median frame is 4.9 µs at 1000 and 54.7 µs at 100 000
  (about 1.6 MB moved and scanned), in a frame of about 2.5 ms, most of it llvmpipe's
  flush and finish. So the vector is not the cost and stays; at a million shown nodes an
  index that inserts in O(log N) (and finds a node's row without the scan) would be. **The
  selection is a node:** `getSelectedNode`, `setSelectedNode` (no event; a hidden or
  unknown node is refused), kept on DataTable's selected item through its
  `selectedItemChanged` hook. An expand or collapse shifts that item by the rows it
  added or removed above it, in O(1), with no event; a collapse that hides the selected
  node moves the selection to the collapsed node (`collapseAll`: to the topmost collapsed
  ancestor) and fires `onSelectionChange` once, its `integer` the id. The index's new
  count and the selection's new item are set in an order that never lets
  `setItemCount` find the item past the end (which would drop it with an event).
  **The tree column** (`setTreeColumn`, 0 by default): every pooled row is a
  `TreeTableRow` (`DataTable::newSourceRow`, a protected factory hook), made with its
  `internal::TreeTableExpander`, a Button owner-placed in the tree cell and clipped to it
  (`getChildClipRect`; the other children keep Label's rects, and the row marks the
  expander itself when its rect changes rather than reporting `hasChildClipRects()`, so
  a new row width, an overflowing column drag's, marks no cell left of the column), so
  binding a parent to a row later makes nothing. A row bound again in place by an
  expand or collapse can show or hide its expander under a resting mouse, so the table
  then calls `Canvas::hoverMayHaveMoved()`, as `TreeNode` does. The cell's left
  padding is `CellPaddingX + depth*Indent + Indent` (Indent 16, the expander 15 square),
  a leaf's too, so siblings line up and a child's expander sits under its parent's text;
  a row bound to a node at another depth changes that padding and moves the expander,
  laying out that row alone. The expander is hidden for a node with no children and
  shows the node's expansion: a change marks the expander alone (`StateChange_Background`;
  the toggled row is above the changed rows and is not bound again, `syncExpanders`). A
  click on it toggles the node without selecting the row or taking the focus; a
  double-click on a row toggles a parent and activates a leaf (`onNodeActivated`,
  `integer` the id). Its renderer draws `TreeTableExpanderHover`/`Down` while hovered or
  pressed (a hidden slot at rest) and a kind-1 chevron at z+1, right collapsed and down
  expanded, in the `TreeTableExpander`, `-Hover` and `-Disabled` text colours. **The
  keys** add the tree's to DataTable's row keys: Right expands a collapsed parent or else
  moves to its first child, Left collapses an expanded node or else moves to its parent
  (the nearest row above at a lesser depth), `*` (a character) expands the selected
  node's whole subtree, Return activates it; with nothing selected, or while a control
  in a cell has the focus, none of them is taken. TreeTableDemo has a generated file-system-like tree of 100 000 nodes, 8 folders
  with 10 files and folders each, numbered breadth first so the rows near the top are
  the same at any node count, Name, Size and Modified columns, and a label following the
  selection; `controltest_tree_table` holds its counts to the same at 1 000 and 100 000
  nodes.
- **The plot histogram: one slot per bar, redrawn where a bar changed.**
  `controls::PlotHistogram` (`include/gweni/controls/plotHistogram.h`, imgui's
  PlotHistogram) draws a series (`setValues`, `setValue(i, v)`) as bars from the bottom of
  a frame. The plot is the control less its padding (2 units all round) and, with a
  caption (`setCaption`, imgui's overlay_text), less the caption's line: the caption is an
  owner-placed `internal::Text` at the top centre, created by the first non-empty caption,
  and its line height, measured in the pass, is taken off the plot's top in `layout()`, so
  no bar runs under its text (a new height marks `StateChange_Background`). **Bars:** bar
  i takes the columns `[i*W/N, (i+1)*W/N)` of the plot's W units, rounded down, so the
  remainder is spread a unit at a time and no bar is more than a unit wider than another
  (a resize moves each edge by at most the change, never re-deals the widths); a column 2
  units wide or more leaves its last unit as the gap to the next bar. A bar's height is
  its value's share of the range times the plot's height, rounded and clamped: at or
  below the minimum (or NaN) it draws nothing, past the maximum it fills the column.
  `setScaleRange` fixes the range; the automatic one (`setAutoScale`, the default) is
  `min(0, lowest)` to the highest value, worked out again on every change, so a value
  past it, or the highest lowered, rescales every bar. **Change tracking:** `setValue`
  marks `StateChange_Background` only when its bar's height or the automatic range
  changes, so a sample that keeps its bar's height costs nothing. The textured
  `PlotHistogram` renderer draws the frame (`PlotFrame`) at the control's z and one slot
  per bar (`PlotBar`, `PlotBarHover` for the hovered one) at z+1 (`depthLayers()` 2),
  resizes its slot list to the bar count in `update()` (as the Slider's notches do) and
  keeps what it last drew in each slot: a frame marking the control's geometry,
  visibility or creation (a move, a resize, a renumber, an ancestor's bounds, which also
  re-stamp the clip box) draws every slot, any other only the slots whose rect, surface
  or z is not the one drawn, and a bar with no rect is hidden. So a changed sample
  rewrites its bar's slot alone, and a rescale the bars that moved. **Hover:** the hovered
  bar is the one whose column holds the mouse inside the plot (`getBarAt`); moving to
  another bar redraws the two, and sets the plot's tooltip text to `"<index>: <value>"`
  (empty outside the plot), which the canvas's ToolTip box shows after its delay and
  changes at once while shown; a move inside the same column changes nothing.
  PlotHistogramDemo has a 32-bar series in the automatic range and frame times in a fixed
  range with a caption; ShowcaseDisplayDemo has one.
- **The plot lines: one slot per segment, redrawn where a segment's endpoints moved.**
  `controls::PlotLines` (`include/gweni/controls/plotLines.h`, imgui's PlotLines) draws a
  series as a polyline in the same frame. The series, its range (fixed, or the automatic
  `min(0, lowest)` to the highest value), its caption, the plot's bounds and the hovered
  sample's tooltip are `controls::PlotBase`'s (`plotBase.h`), which `PlotHistogram`
  derives from too; a subclass says what a value is drawn as (`drawnKey`: a bar's height,
  a point's y), which sample a point is over (`sampleAt`) and what a new hovered sample
  changes (`hoveredChanged`: the bars' looks, nothing for a line). **Points:** the N
  samples are joined by N-1 segments, each one `drawLine` (style 3) of the line's
  thickness T (`setLineThickness`, 1 unit by default); sample i is at
  `T/2 + i*(W-T)/(N-1)` across the plot and `T/2 + (1-share)*(H-T)` down it, fractional
  units, the share clamped to 0..1, so the stroke stays inside the plot and a value past
  the range lies on its edge. With T = 1 that is `i*(W-1)/(N-1)` plus the half unit that
  centres the stroke on its column, where `drawLine` puts it. A NaN value leaves a gap
  (the segments touching it are hidden); a single sample draws nothing. **Change
  tracking:** `setValue` marks `StateChange_Background` only when its point's y or the
  automatic range changes. The textured `PlotLines` renderer draws the frame
  (`PlotFrame`) at the control's z and one slot per segment at z+1 (`depthLayers()` 2)
  in `PlotLine`'s colour, resizes its slot list with the sample count, and, as
  `PlotHistogram`'s, keeps what it last drew in each slot: a frame marking geometry,
  visibility or creation draws every slot, any other only the segments whose endpoints,
  neighbours, thickness, colour or z changed. Each segment is drawn with its neighbours
  (`drawLine`'s `prev` and `next`: samples i-1 and i+2, none past the ends or across a
  gap), so its joints are blended once at any alpha ("Line segments (style 3)"). So a
  changed sample rewrites the four segments around it, i-2..i+1 (the two it ends and the
  two holding it as a neighbour; fewer at the ends and next to a gap), a sample set to
  NaN hides the two it ends and draws the two beyond them again without it, and a
  rescale or a new thickness rewrites every segment. `push(v)`, for
  a scrolling series such as a frame-time graph, drops the oldest value and appends `v`:
  every value moves one sample left, so a series of distinct values redraws every
  segment, with no layout. A hover has no look: it sets the tooltip to the nearest
  sample's `"<index>: <value>"` and writes nothing. PlotLinesDemo has a 64-sample sine
  in the automatic range and 40 frame times in a fixed range with a caption and a
  1.5-unit line; ShowcaseDisplayDemo has the frame times.
- **The chart: series of slots over time, value or category axes.** `controls::Chart`
  (`include/gweni/controls/chart.h`, with the axis helper `controls::axis` in
  `chartAxis.h`) draws line, step-line, bar, stacked-bar and (stacked, step) area series
  against a value axis on
  the left and one on the right, over a time (epoch seconds), value or category x axis. It
  does not derive from `PlotBase`: its points are `(x, y)` doubles, since a float resolves
  about 2 minutes at 1.7e9 s. **Axes:** `controls::axis` has no Chart in it, so the Timeline
  takes it as it is: value ticks at 1, 2 or 5 times a power of ten (`niceStep`,
  `niceRange` widening an automatic range to whole steps, ticks as `k*step` so none
  drifts, `formatValue` with the step's decimals or the app's `ValueFormatter`); time
  ticks at the smallest of 1 s, 5 s, 15 s, 1 min, 5 min, 15 min, 1 h, 6 h, 1 d, 1 week,
  1 month and 1 year whose labels leave a gap at the axis's width (`chooseTimeTicks`, Year
  with thinned labels when none does), the steps under a day on the zone's clock and the
  calendar ones at 00:00 in it (a month tick is the 1st, a week tick a Monday), through a
  UTC-offset hook `std::function<int(double)>` (seconds east of UTC at an instant,
  `localUtcOffset()`, `localtime_r`'s `tm_gmtoff`, by default; `fixedUtcOffset`). Across a
  DST change an hour the clock skips has no tick, one it passes twice one, and a calendar
  tick the clock skips (a midnight jumped over) is where its day begins. Labels are
  "HH:MM:SS", "HH:MM" or "Nov 14", the date at a midnight and the year on January 1st.
  Category ticks are one per category (none past the last, however far a point or the
  range reaches), and `thinLabels` gives the smallest stride at which
  every stride-th label leaves the gap. `tests/unit/chartAxis.cpp` pins them, DST and a
  leap year under a hook with DST included. **Data:** a series (`addSeries(name, type,
  side)`) keeps its points sorted (`setSeriesPoints`, `appendPoint` after every x not
  above its own, `setPointValue`, `removeFirstPoints`); what is drawn from them is worked
  out lazily (`computeData`): the bar groups side by side at one x (each unstacked bar
  series one, every stacked series one together, in the order of their first series),
  the bar interval (`setBarInterval`, or the smallest step between two x of a visible bar
  series), the stacks (by equal x in series order, positive values up from 0 and negative
  ones down, a series with no point at an x adding nothing there) and the extents. The x
  range is `setXRange`'s, or the visible series' extent widened by half a bar interval
  with bars; a value axis's is `setYRange`'s, or its series' extent (with 0, and always
  with a bar's 0) widened by `niceRange` for a tick every `YTickSpacing` units of the
  plot. A data change marks `StateChange_Background`, and invalidates only when the x
  range or that series' value axis is automatic, so with both fixed a new point lays
  nothing out. **Layout:** the plot's width needs the value labels' widths, which need
  the plot's height, so the chart lays its children out in its Y arrange, as a
  ScrollControl does ("Children whose X layout needs the parent's height"): the legend's
  column at the right (its ScrollControl's measured preferred size, a third of the
  chart's width and its inner height at most), the value labels' columns, the x labels'
  row and the titles' lines come off the control, the ticks are computed for what is
  left, and the tick labels, titles and marker captions are pooled owner-placed
  `internal::Text`s, set only when their string changes and measured once per string
  (a width cache keyed on the font, the renderer and its `textLayoutSerial()`).
  **Rendering:** the textured `Chart` renderer keeps a slot list per part, each slot
  holding what it last drew (a rect or a segment with its neighbours, its colour, z and
  clip box): the frame (`ChartFrame`, taken at generate), the grid (a `drawLine` per tick
  of each shown value axis with its grid on, and with `setXGrid` per x tick, then the
  line along the plot's bottom), the spans (`drawRoundedRect`, `ChartSpan`, translucent),
  one list per series (a bar or stacked segment per point, `getBarRect` cut to the plot,
  or a segment per vertex pair of a line or step line, drawn with its neighbours), the
  markers (a vertical `drawLine` each) and the crosshair's one slot; an area adds the
  list of its line (below). Their z values, back to front, are the frame's z, the grid's
  z+1, the spans' z+2, series i's z+3+i, the k-th area's line's z+3+S+k, the markers'
  z+3+S+A and the crosshair's z+4+S+A (S series, A of them areas), so the renderer's
  layer count follows the series: `skin::Control::depthLayersOf(control)` (`depthLayers()` by default) is what
  the canvas asks, and the chart calls `Base::zOrderChanged()` when a series comes or
  goes. The lists are taken back to front, and only once the chart is shown with a plot,
  so a translucent span is blended over the grid behind it and a line's anti-aliased rim
  over both whether the chart was built shown or hidden (a slot drawn earlier at a higher
  z writes depth a slot behind it, drawn later, cannot pass). Slots draw in id order, so
  that order has to hold after the first draw too: each list is taken with
  `Chart::capacity(n)` slots for the n it draws (half as many again, at least 4 more),
  the spare ones hidden, and when one needs more than it has (an appended point, a new
  series, more grid lines after a resize, a span or a marker added) or fewer than it
  was taken for (a series gone, a resize back), every list is given back and taken again
  back to front, the bank's lowest-first ids rising from list to list (any at or below
  the frame's put aside while taking), and that frame draws them all. So an append is
  amortised to its own slots, a sliding window is taken again once and then holds, a
  chart grown point by point draws what one built with those points draws
  (`tests/chartSlotOrder.cpp`), and a resize there and back gives the same slots back
  (`controltest_resize_each`). All but
  the frame are drawn in the plot's clip region (the control's region cut to
  `getPlotBounds()`), and each segment is first cut on the CPU, in doubles and from its
  end nearer the plot, to a box a few units wider than the plot, so a point far out of
  range is clipped, never clamped, and no quad reaches past what a vertex holds; marker
  captions are clipped to the plot through `getChildClipRect`. So a changed point
  rewrites its bar and the bars stacked on it at its x, or a line's four segments around
  it (a step line's five, around its two vertices); an appended point its own slot (and
  a line's joint before it); a moved marker its slot and its caption's glyphs. A rescale,
  a new x range, a sliding window or a series toggled redraws every slot of the series
  it moves. **Areas:** `Area`, `StepArea`, `StackedArea` and `StackedStepArea` are a line
  (a step line for the step kinds) with the region between it and its base filled. An
  unstacked area's base is 0 held to its value axis's range (`getAreaBaseValue`), its
  NaN a gap of the two segments beside it. The stacked areas form a stack of their own,
  apart from the bars' and drawn on the first stacked area's axis: it is taken at every x
  of a visible stacked area (`getAreaStackXs`), each one's value there its point's y, or
  0 where it has none or a NaN (the feature's "a missing x counts as 0"), positive values
  up from 0 and negative ones down, so every stacked area has a vertex at each of those x
  and its band's bottom edge is exactly the top edge of the band below it
  (`getAreaStackBase`, `getAreaStackTop`). `getAreaSegment` gives each segment's sides,
  line and base in the control's coordinates, unrounded (a step's flat at its first x's
  value), and `getVertex` an area's line. The renderer draws the fill in the series' own
  list and layer, one slot per segment: a `drawTrapezoid` in `ChartArea<i>`'s top colour
  (the series' colour, translucent, a theme surface of its own), cut on the CPU to a box
  a little wider than the plot along its edges (exact: they are straight) and its heights
  held `Chart::AreaReach` units past the plot so the quad fits the vertex's range; a step
  area a `drawRoundedRect` of the step's rect in whole units, cut to the plot. The line is
  drawn as a line series' is, with its neighbours, in a list and a layer of its own in
  front of every series' fill (a lower band's line is not covered by the band on it),
  that list taken after every series' list. So a changed point rewrites the two
  trapezoids beside it, those of every band stacked on it at its x, and the lines'
  segments around it; a hidden stacked area leaves the stack (its x and its band) to the
  others. `pointAt` picks an area's point near its line's vertex as a line's, else, in
  the area whose band holds the mouse (the last one, in front, first; a binary search of
  its segments' x), its point nearest in x. `tests/unit/chartArea.cpp` pins the stack,
  the fills, the layers, the writes and the click. **Legend:** a `ScrollControl` of `internal::ChartLegendRow`s, Selectables with
  their series' swatch (`ChartSeries<i>`, faded while hidden) in a column before the name;
  the rows follow the series outside the layout, a click toggles its series (the axes
  rescale and the stacks are worked out again) and fires `onSeriesToggled`, and the row
  is never selected. A row past the series, after one is removed (even by the
  `onSeriesToggled` handler of that row's own click), is hidden and `delayedDelete()`d,
  and a row keeps its index, so one that now shows another series is redrawn whenever its
  look (`ChartLegendRow::lookStale()`: shown or hidden, colour index) no longer matches
  its series. **Hover and click:** both are arithmetic. `hoverAt` is the visible
  point x in the x range nearest the mouse (a point past the range's edge never takes the
  hover from one inside it), a binary search of each series' points, with each series'
  index there (`ChartHover`); while the mouse is over the plot the crosshair is drawn at
  it and the tooltip set, the chart's own text (the x's label, on a time axis the point's date and time of day
  through `axis::formatDateTime`, never the tick's year alone, and each value), the app's
  (`setHoverText`) or an app control filled per hover (`setHoverControl`), refilled when
  the data under the mouse changes; a move within one x marks nothing. The canvas's one
  ToolTip box goes under `Base::getToolTipAnchor()`, the control's canvas bounds by
  default and the chart's beside the crosshair at the plot's top, so the box follows the
  hovered x. `pointAt` is a visible bar or stacked segment holding the point (the bars
  either side of its x, the front series first), else the line point nearest it within
  `PointHitRadius`; a press and release on the same one fire `onPointClicked`. Markers
  (`addMarker(x, caption)`) and spans (`addSpan(x0, x1)`) count in no range. Theme:
  `ChartFrame`, `ChartGrid`, `ChartAxis`, `ChartSpan`, `ChartMarker`, `ChartCrosshair`,
  `ChartSeries0`..`7` and the text colours `ChartAxisText` and `ChartMarkerText`.
  ChartTimeBarsDemo has four stacked bar series over two weeks of days with a legend,
  ChartLinesDemo a day's memory (line, left) and GPU use (step line, right) with two
  markers and a span, both pinned to UTC. `controltest_chart` (`--scenario chart`) holds
  the slot counts above on both demos, and `gweni_bench`'s `chart-append` appends a point
  a frame to a line of N in a fixed range at the same cost at any N.
- **The timeline: lanes of time-range bars, each a slot.** `controls::Timeline`
  (`include/gweni/controls/timeline.h`) is a Gantt-style view: lanes, one row each, with
  their captions in a fixed column at the left and a time axis above, and in each lane
  bars of a start and an end (epoch seconds, doubles), a theme surface or a colour of
  their own and an app id (`TimelineBar`), a thousand of them costing one control and a
  thousand slots. **Axis and range:** the x range is `setXRange`'s, or the bars' extent;
  the ticks and labels are the Chart's `controls::axis` as it is (`chooseTimeTicks` in the
  zone of `setUtcOffset`, then `thinLabels`), pooled `internal::Text`s in a row at the
  top. A bar changed inside an automatic range that did not hold one of its ends lays
  nothing out, and with a fixed range no bar change does. **Scroll and captions:** the
  lanes scroll in `internal::TimelineBody`, a ScrollControl with virtual content as tall
  as the lanes and a vertical bar alone; neither it nor its inner panel takes the mouse,
  so the Timeline itself is hovered over its lanes and the wheel scrolls `WheelLanes` (3)
  a notch. Its viewport less the caption column is the bar area; the Timeline lays the
  body out first in its Y arrange and the axis after, from that viewport. The captions
  are a pool of Texts in `internal::TimelineCaptionColumn`, which clips them, as many as
  the most lanes it can show any of (VirtualList's rule): a row whose lane stays in view
  keeps it, a lane coming into view takes a free row, and a new pool size deletes the
  free rows before topping the pool up, so a timeline resized back has the rows one built
  at that size has (without that, `controltest_resize_each` failed on the hidden captions'
  old bounds and glyphs). **Overlaps:** the app packs its bars, but where two of a lane
  overlap in time the later one is in front: each bar has an overlap level, 0 when no
  earlier bar of its lane overlaps it and otherwise one more than the highest of theirs,
  `MaxOverlapLevel` (3) at most, and the renderer draws a level in a depth layer of its
  own. A lane's bar slots follow the bars' indices, so a later bar's slot is drawn after
  the earlier one's, and at one z `GL_LESS` would keep the earlier one's pixels: the
  layer, not the slot order, puts it in front (`tests/timelineOverlap.cpp`, GL and
  Vulkan pixels). Past the cap (five bars overlapping at once) or where two packed bars'
  rounded rects both reach one column, two bars share a layer, and there the slot order
  decides: the earlier bar is drawn first and kept, so it is the one on top, drawn and
  hit (the same test pins both). The levels are worked out lane by lane on the first use
  after a change, each bar against the earlier bars whose ends reach its start: O(n log n)
  in a packed lane, O(n^2) at worst (every bar under one long one). **Hit test:** arithmetic. The lane from y (`laneAt`: the fixed lane
  height and the scroll offset), then the lane's bars sorted by start with a running
  maximum of their ends (`ensureLane`, worked out again only for a lane whose bars
  changed): a binary search for the bars starting before the time at x+1, walked back
  while that maximum reaches the time at x-1, each candidate held to its rect exactly
  (`getBarRect`: whole units, a unit wide at least, held `OffAreaMargin` outside the area
  but not cut to it), the highest level and then, in a shared layer, the earlier bar
  winning. In a packed lane that is O(log n) plus a bar or two; an earlier long bar under
  short later ones is still found between them, at the cost of walking back over every
  bar that starts before the point while it reaches it. `tests/unit/timeline.cpp` holds it to a brute force over random lanes. **Hover and
  click:** the bar under the mouse gets the hover look and the tooltip (the lane's
  caption and the bar's times, `axis::formatDateTime`; the app's `setHoverText` or a
  control it fills, `setHoverControl`), placed below the bar (`getToolTipAnchor`) and set
  only when the hovered bar changes (and set again under a resting mouse when the bar, its
  lane's caption or the zone changes); a press and release on the same bar with the same
  id fire `onBarClicked` with the lane, the index and the id. **Rendering:** the textured
  `Timeline` renderer keeps a slot list per part, each slot holding what it last drew:
  the frame (`TimelineFrame`, taken at generate), a stripe per odd lane across the
  caption column and the bars (`TimelineLane`) at z+1, a grid line per tick and the two
  axis lines (`TimelineGrid`, `TimelineAxis`) at z+2, a list per lane of its bars at z+3
  plus their level (`TimelineBar`, the surface a bar names, or its colour with
  `TimelineBar`'s border), and the hover look's one slot at z+4+`MaxOverlapLevel`, so
  `depthLayers()` is 8. The lists are taken back to front once the timeline is shown with
  a bar area, each with Chart's `capacity(n)` (spare slots hidden, so an added bar takes
  one of its lane's), and all again when a lane outgrows its list or drops below half of
  what it was taken for, and when the lane count changes: an `addLane` or `removeLane`
  rewrites every slot, since a list taken alone could get ids below the stripes' and the
  grid's, and a translucent bar drawn before them would hide them behind it (the depth
  test), while lanes come and go rarely. The lanes, grid and bars are drawn again only when
  `Timeline::getBarsSerial()` moved (every change of the bars, lanes, range, layout or
  scroll bumps it; the hover does not), the bar area's clip changed or the frame marks
  the timeline's text, which the timeline itself never does but a theme edit does
  (`ThemeEditor::applyToCanvas()`, which marks the background and text of every control
  and, for an editor row's edit, lays nothing out: without that, `controltest_theme`'s
  live TimelineDemo kept its old bar and lane colours in 135299 pixels), and then a slot
  only when its rect, surface, z or clip differs; a lane out of view hides its bars. The
  bars and the hover look are square (`TimelineBar`'s radius is 0) at whole units, so
  their device pixels are fully covered and no slot's order against another's changes a
  pixel of an opaque bar. The hover look is the hovered bar's own fill with
  `TimelineBarHover`'s translucent fill blended over it on the CPU, and that surface's
  border: a border over a transparent fill draws nothing, since style 4's alpha is the
  fill's times the shape's coverage (`roundedRect()`), so a look that leaves the bar's
  pixels showing through is not a style 4 slot. So a hover frame writes the look's one
  slot, a changed or added bar its own, a settled frame nothing. **A lane scroll moves
  the bars:** `controltest_timeline` measures a wheel notch over TimelineDemo (3 lanes at
  640x480) writing 268 primitive slots, 255 of them bars (`bars_moved=yes`), all of them
  stripes, bars and captions of the 20 lanes in view before or after it, the hover look,
  the scroll bar and the ToolTip box: the bars' vertices are in device pixels, so a bar
  that stays in view is drawn again where its lane moved, and one that leaves it is
  hidden; none is merely clipped. The cost is the lanes on screen, not the bars.
  TimelineDemo is a day's 1000 runs over 96 lanes (12 hosts of 8 agent slots) in UTC;
  `controltest_timeline` holds the counts above, a click and an overlap, and
  `gweni_bench`'s `timeline-hover` moves the mouse onto another bar a frame among N bars
  over 100 lanes at the same cost (one slot) at any N.
- **The drag number: scrubbed by the mouse, typed into in a box made once.**
  `controls::DragNumber` (`include/gweni/controls/dragNumber.h`, imgui's DragFloat and
  DragInt) is a `Label` whose text is its value, centred in a field of the size asked for
  (Fixed, as a TextBox). The value is a double, `setNumber`/`getNumber`: `Base` already
  has a virtual `getValue()` returning a String, which here is the edit text. It is
  clamped to `setRange` (none by default) and, `setInteger(true)`, rounded to whole
  numbers; `onValueChanged` fires once per real change, never for a value that stays.
  **Scrub:** a press takes the mouse (`MouseFocus`, so the capture keeps the field
  hovered and nothing the mouse crosses repaints) and a horizontal move changes the value
  by the motion times `getSpeed()` (`setSpeed`; 0.1, or 1 for an integer field),
  `ShiftFactor` (10) times as fast with Shift held and `AltFactor` (0.1) with Alt. The
  motion is summed from the press in units of the mouse times those factors and the
  value is `start + sum*speed`, so steps worth less than a whole unit of an integer field
  add up, a drag back to its start gives the value at the press, and the sum is held at
  the range's ends, so turning back moves the value at once. A press and release without
  motion changes nothing. The cursor is `SizeWE`. **Text:** the value is shown through
  `setFormat`, which takes one conversion of a double as the slider's value text does
  (`Slider::validValueFormat`; "%.3f", or "%.0f" for an integer field, by default), and
  `Label::setText` marks the Text, and lays it out again, only for a new string, so a drag
  frame that keeps the string writes no slot at all. **Edit:** a double-click, a
  Ctrl+click or Return on the focused field shows an `internal::DragNumberTextBox` (a
  `TextBoxNumeric`, drawn by the TextBox renderer up its parent types) owner-placed over
  the field, focused, holding the value at the format's precision with trailing zeros
  dropped, all selected; the field's Text is hidden and the field's slot too. The hidden
  Text would count for nothing in `sizeOfChildren`, and the owner-placed box neither, so a
  field with any flag but Fixed would shrink to its padding under the box (#521): while
  it is open, `DragNumber::calculateSize` measures the Text afresh (`doCalculateSize`, so a
  value set during the edit still resizes the field) and counts it as a shown Text, so
  opening or closing the edit changes no size and its layout stops at the field. Return
  commits it and gives the focus back to the field (a TextBox's Return would tab to the
  next control), a lost focus commits it and leaves the focus where it went, Escape
  cancels it; text that is no number (`parseNumber`: a whole finite `strtod`) leaves the
  value as it was. The box is created by the first edit and only shown or hidden after
  that, so it keeps its ids. While the field has the focus, Up and Right step the value
  up by the speed and Down and Left down (the modifiers as for a drag, a whole unit at
  least for an integer field); the box keeps Up and Down while editing. The textured
  `DragNumber` renderer draws the field in one slot, `DragNumber` at rest,
  `DragNumberHover`, `DragNumberFocus`, `DragNumberDown` while pressed and
  `DragNumberDisabled`, so a hover, a press or the focus rewrites that slot and nothing
  else. DragNumberDemo has an integer, a ranged float, a formatted one and a disabled
  one; ShowcaseInputDemo has two.
- **The vector field: a row of DragNumbers, a change marking one component.**
  `controls::VectorField` (`include/gweni/controls/vectorField.h`, imgui's DragFloat2/3/4
  and DragInt2/3/4) holds 2 to 4 `DragNumber`s (`setComponentCount`, 3 by default),
  `setValue(i, v)`/`getValue(i)` and `setValues`/`getValues` (a `std::array<double, 4>`).
  The range, speed, format and rounding are set for every component at once
  (`setRange`, `setSpeed`, `setFormat`, `setInteger`) and kept for components added
  later; `setRange(i, ...)` and `getField(i)` reach one. The components split the row's
  width in whole units with `Gap` (4) between them: each is `(width-Gap*(count-1))/count`
  wide and the last takes the remainder (`componentSpan`), so the others keep their width
  as the row grows by less than a unit per component. With `setAxisTags(true)` each
  component starts with a `TagWidth` (14) unit `internal::VectorFieldTag`, a Label
  showing X, Y, Z or W in the skin's `AxisTagText` colour on a block the textured
  `VectorFieldTag` renderer fills with `VectorTagX`..`VectorTagW` (red, green, blue,
  grey; `DragNumberDisabled` while disabled), its field taking the rest. The tag is a
  control of its own before the field, not an overlay drawn over it, so it reserves no
  depth layer and the field's text stays centred in its own rect. Tags and fields are
  owner-placed (`VectorField::layout`), the row the size it was asked for (300x20 by
  default, `sizedByRequest`), and they are its children in component order, tag before
  field (`setAxisTags` moves each field after its new tag), so Tab and Shift+Tab step
  through the components; the tags are not tabable. `onChanged` fires once per real
  change of a component, from a drag, a typed edit, a key step or `setValue`, with the
  component's index in `info.integer` and its field in `info.control` (an
  `event::Packet` is the handler's own, given when it was added, so the index travels in
  `event::Info` as a DataTable's column does). A change goes through that component's
  `DragNumber` alone, so it marks that field and nothing else in the row. Adding a
  component or the tags takes their ids and removing them gives them back.
  VectorFieldDemo has a float2, a tagged float3, a tagged integer vector of four and a
  disabled one; ShowcaseInputDemo has a tagged float3.
- **Attached children: a badge on its anchor's corner.** `Base::attachTo(anchor, corner,
  offset)` makes a control a child of the anchor's own list (never an inner panel's or a
  LayoutItem's), `Placement::Owner`, centred on that corner of the anchor's bounds and
  moved by the offset; `controls::Badge` (`include/gweni/controls/badge.h`) is one, and
  any control can be. Nothing in the anchor's type knows about it:
  - *Placement.* `Base::doArrange()`, the non-virtual wrapper every arrange goes through,
    places the anchor's attached children along the axis at their preferred size
    (`dockNoneSize`, so a request still counts) before calling the anchor's `arrange()`,
    and arranges any its `arrange()` did not enter (a LayoutItem's, an override that
    places its children itself) after it; `doCalculateSize()` measures one the anchor's
    own `calculateSize()` did not. Owner-placed, it is not in the anchor's preferred
    size, so 9 to 10 resizes no button, and its bounds are in the anchor's coordinates,
    so a pure move of the anchor enters neither it nor the anchor's other children.
  - *The clip.* An attached child renders in the region its anchor renders in, not in
    the anchor's bounds, whether or not the anchor clips and over any
    `getChildClipRect()` rect the anchor gives its other children: the overhang shows
    wherever the anchor could draw, and is cut wherever an ancestor cuts the anchor (a
    ScrollControl's viewport, or a plain container, which clips at its bounds, so a row
    of badged buttons keeps padding for the overhang). That region is the render
    drain's own state for the anchor: its tree walk has it at hand, and its ancestor
    stack keeps the region each level renders in (`RenderLevel::clip`, one more `Rect`
    a level), so no rect is cached or worked out per call, and none changes when the
    anchor resizes. `clipRectRegionChanged()` follows the same rule, and a Label does
    not mark an attached child when its own rects change. An anchor that
    `transformsChildren()` (a ZoomPanel) is the exception: its attached children
    render in its children's space.
  - *The depth.* z is the preorder, so a child of the anchor is numbered under the
    anchor's later siblings. The anchor's parent (its `m_actualParent`) reserves one z
    per attached child of its children in its overlay range, after its skin Control's
    `overlayLayers()`: `renumberZRecursive` counts them over the children it walks or
    passes (`countAttachedOverlay`) and hands them out in child order
    (`numberAttachedChildren`), marking a child whose z changed `StateChange_Geometry`.
    The parent's count is `attachedOverlayLayers()`, the child's z
    `getAttachedZIndex()`, which its skin Control draws at: in front of the anchor's
    later siblings, their glyphs included, and behind whatever comes later in the tree
    (a Popup, a Menu, the ToolTip box, the canvas's overlays). A draws-nothing row
    reserves them as well as any control: the count is `Base`'s, not a skin Control's.
    An anchor whose parent is the root (a canvas child) reserves its own, after its
    subtree, since its parent's range ends after the Popups and the ToolTip box.
    Attaching is an add, which marks the way to the parent for the renumber;
    detaching, or taking an anchor off its parent, marks the parent with
    `zOrderChanged()`; a new count renumbers nothing. Only what the attached control's
    own skin Control draws is lifted: its children keep their preorder z, so the
    textured `Badge` draws its number in a text run of its own at
    `getAttachedZIndex()` with its bubble, and has no `internal::Text` child.
  - *Input.* `getControlAt()` passes an attached child over: a point on its part
    inside the anchor finds the anchor (or the anchor's child there), and one on the
    overhang is outside the anchor's bounds, so the walk goes on to what is under it.
  - The corner, offset and z of an attached child, and a parent's reserved count, are
    kept in side tables, with three bools in `Base`'s padding saying which controls
    have one, so `Base` stays 752 bytes and a control that is no part of an attachment
    costs nothing. Deleting the anchor deletes its attached children with it.

  A `Badge` shows `setCount()`'s number, `"<cap>+"` past `setCap()` (99), a plain
  `DotSize` dot with `setDot()`, and hides itself (its own flag) at 0 unless
  `setShowZero()`. A digit is a circle of its line's height and a longer number a pill
  (`PaddingX` each side), drawn with the `Badge` surface, as round as its height allows,
  the number in `BadgeText`. A new count marks `StateChange_Text`; `refresh()` measures
  the new string and calls `invalidate()` only when the size it takes changes, so a
  count of the same width lays nothing out and a wider one the way down to the badge
  alone. On a VirtualList's or DataTable's pooled row a badge follows the row, not the
  item: the row binder sets its count. `tests/unit/attachedChild.cpp` pins the
  mechanism on plain controls, `tests/unit/badge.cpp` the Badge,
  `tests/badgeOverlap.cpp` its draw order over the next Button and its cut at a
  viewport on both backends, and ControlTest's `--scenario chip-badge` the counts,
  an anchor's move, a chip's hover and a removal on ChipBadgeDemo.
- **The chip: a toned label, clickable or removable.** `controls::Chip`
  (`include/gweni/controls/chip.h`) is a `Button` drawn as a pill: one primitive slot,
  its Label's Text and, with `setRemovable(true)`, a `ChipRemoveButton` (a
  `WindowCloseButton` subclass, drawn by that renderer's cross, as a closable tab's
  close button is) in a `RemoveButtonSize` square kept in its right padding and placed
  in its `arrange()`, so its width never follows the button. Its colour is a tone
  (`setTone`: Neutral, Accent, Success, Warning, Danger) or a palette entry
  (`setPaletteIndex(i)`, i mod `PaletteSize` (8), a negative i from the end), each a
  theme surface of its own (`ChipNeutral` .. `ChipDanger`, `ChipPalette0` ..
  `ChipPalette7`: the colour at about a fifth over the background as the fill, nearly
  half as the border) with its text colour (`ChipNeutralText` .. `ChipPalette7Text`
  in `textColorRegistry()`, the full colour; `Colors.Chip` in the skin), so the theme
  editor and a theme file set them by name. The hover and press looks are derived,
  not named: the textured `Chip` renderer moves the entry's fill 35 % (hover) or 60 %
  (press) toward its border, so a theme sets one surface per entry, and a disabled chip
  draws `ButtonDisabled`'s colours in its own radius (a pill, not a button's corners).
  A disabled chip hides its remove button (the square stays in the padding, so the
  width holds) and fires no `onRemove`, as a disabled closable tab shows no close
  button. A chip that is not clickable (the default) takes no press:
  its clicks, Space and accelerator do nothing and it draws no hover look, though it
  keeps the mouse for its tooltip; `setClickable(true)` gives it the looks and fires
  `onPressCaller`. The remove button fires `onRemove`; the chip never deletes itself,
  the app does (`delayedDelete()` from the handler, as ChipBadgeDemo does; a `delete`
  or a `setRemovable(false)` there would free the button inside its own click). A tone or
  palette change is a `redraw()` of the chip alone, and its text colour is set in
  `updateColors()`. A new text of the same size lays nothing out: the chip turns on
  `internal::Text::setSameSizeKeepsLayout(true)`, with which a Text measures a new
  unwrapped string in `setText()` (once its old one was measured, under the current
  measure key) and calls `invalidate()` only when the size changed, the glyphs alone
  redrawn otherwise. It is off for every other Text: a TextBox places its caret in the
  layout. A chip asks for no size of its own (a Button asks for 100x20, which a layout
  cell would take as its least), so in a row or a layout cell it is its text and
  padding wide. `tests/unit/chip.cpp` pins it; ChipBadgeDemo shows the tones, the
  palette, clickable and removable chips.
- **The switch.** `controls::Switch` (`include/gweni/controls/switch.h`) is an on/off
  toggle, a pill track with a round thumb (left while off, right while on), built on
  `Button`'s toggle: `setIsToggle(true)`, the state is `getToggleState()` (`setOn`,
  `isOn`, `toggle`), and a change fires Button's toggle callers, then `onToggled` and
  `onToggledOn` or `onToggledOff`, once per real change, from a click, a key or `setOn`
  alike; `setOn` with the value it has fires and marks nothing. If a handler turns
  it back from Button's callers or `onToggled`, the outer call fires none of the
  Switch's own notifications left, so it never announces a stale On or Off
  (Button's `onToggleOnCaller` or `onToggleOffCaller`, turned back from
  `onToggleCaller`, fires the state it ends in a second time, as for any toggle
  Button). Its label is the
  switch's own Text (`setText`), not a control beside it as a `CheckBoxWithLabel`'s
  is: the track sits in the switch's padding on the label's other side
  (`setLabelOnLeft`, `TrackWidth` and `Gap` units wide), so a hover or a click
  anywhere on it, label included, is the switch's, and a hover writes the track's
  slot alone, with no `setLabelHovered` relay between two controls. Its colour stays
  `Colors.Label.Default` (followed, so a theme edit reaches it) whatever the state,
  hover and press, `Colors.Button.Disabled` while disabled, so none of them writes a
  glyph. Its size is the track's (`TrackWidth` x `TrackHeight`, constants in the
  header) and the label's beside it, so a toggle lays nothing out. It takes the
  keyboard focus and Tab; Space and Return toggle it on their release, since a held
  key repeats its press. Disabled, it takes no press, no keys, no focus and no Tab,
  and gives up the focus it holds (`setDisabled`: a click would otherwise focus it,
  as it does any control taking keys). Its cursor is Normal while disabled and
  Finger while enabled.

  The textured `Switch` renderer has two slots: the track (style 4, its surface by
  state: `SwitchTrackOff`, `SwitchTrackOffHover`, `SwitchTrackOn`,
  `SwitchTrackOnHover`, `SwitchTrackDisabled`, `SwitchTrackOnDisabled`; focused, its
  border is `SwitchFocus`'s colour and width, as a focused TextBox shows
  `TextBoxFocus`'s border) and the thumb (style 4, `SwitchThumbOff`, `SwitchThumbOn`,
  `SwitchThumbDisabled`, at `getZIndex()+1`, `depthLayers()` 2), each as round as its
  height allows (the surfaces' radius is clamped to half of it). The thumb's look
  follows the state alone, so a hover rewrites the track's slot and the thumb's keeps
  its bytes.

  A toggle slides the thumb over `SlideSeconds` (0.15 s for the whole travel, a half
  cosine ease), on the canvas clock, by an animation registered with `anim::add` when
  the slide starts and cancelled by the step that arrives, as the indeterminate
  ProgressBar's is registered only while it moves. A toggle mid-slide starts again
  from where the thumb is, over the part of the travel it has left. Each step places
  the thumb (`getThumbPosition()`, 0..1; `getThumbRect()` rounds it to whole units)
  and marks `StateChange_Background` only when that rect changed, as the
  ProgressBar's `m_segmentMarked` does; the track's slot is redrawn with the same
  bytes, so a slide frame writes the thumb's slot alone. The thumb jumps to its end,
  with no animation, when the switch is not `visible()`, has not been rendered yet
  (`Switch::render` notes the first time: a switch set on as its demo is built would
  otherwise slide in its first frames; `StateChange_Created` is not a reliable sign of
  that there) or `GWENI_ANIMATE` is off; a step that finds the switch not `visible()`
  (a page hidden mid-slide) puts the thumb at its end and cancels itself, so the
  frame after the hide runs no animation. The step runs once for that (a slide lasts
  0.15 s, so it does not use the visibility hook, "The visibility hook").
  `tests/unit/switch.cpp` pins the control;
  `--scenario switch` (`controltest_switch`) its slots, pixels and animations on
  SwitchDemo.

- **The spinner.** `controls::Spinner` (`include/gweni/controls/spinner.h`) is a busy
  indicator (Material's CircularProgressIndicator, WinUI's ProgressRing, GTK's
  Spinner): an arc turning round a ring while something runs, drawn by the textured
  `Spinner` renderer as one `drawArc` slot at `getZIndex()+1` in `SpinnerArc`'s top
  colour, with an optional track ring behind it (`setShowTrack`: a whole ring at
  `getZIndex()` in `SpinnerTrack`'s while it spins, a slot hidden otherwise: a
  stopped one's still ring on the same circle would show the track's edge round it;
  `depthLayers()` 2).
  **Its motion is Material's**, not a fixed sweep: the base turns at `setSpeed()` turns
  a second (1), and over each cycle of `CycleTurns` (1.5) base turns the head runs
  `MaxSweep - MinSweep` (0.64) turns ahead, eased, then the tail catches it up, so the
  sweep grows from 0.08 to 0.72 and back, each cycle starting where the last one's tail
  ended. `Spinner::arcAt(seconds, speed)` gives the arc as a function of the canvas
  clock alone (`platform::getTimeInSeconds()`), not of when the spinner started: two
  spinners at one speed are drawn alike, one shown again is where the clock says, and
  a capture's stepped clock gives the same frames in every run
  (`controltest_determinism`). **Its animation** is registered only while it spins at a
  speed other than 0 and is `visible()`: `start()`, `stop()`, `setSpeed()` and
  `onVisibleChanged()` ("The visibility hook") register or cancel it, so a stopped or
  hidden spinner, or one on a hidden page, runs nothing. A step works the arc out and
  keeps it as its slot keeps it (`renderer::arcStartBits`, `arcSweepBits`), marking
  `StateChange_Background` only when those bits change, so a step that would draw the
  same arc (a frozen clock, a speed too slow to move a bit) marks and writes nothing,
  and a spinning frame writes the arc's slot alone. **Stopped**, it hides itself
  (`setHideWhenStopped(true)`, the default), and `start()` shows it again only if
  `stop()` hid it, not if the app did (`setHidden` clears that); with
  `setHideWhenStopped(false)` it shows a whole still ring in `SpinnerStopped`'s colour,
  dimmed. **Its size** is its diameter, the preferred size both ways: `Small` (the
  default) is the skin font's line height (`measureLineHeight`), so it fits in a
  Button's row beside its text; `Medium` and `Large` are 32 and 48 units, for a card;
  `setDiameter()` sets one of its own. The stroke is a tenth of the shorter side of
  its bounds, at least 1.5 units (`drawArc` snaps it). It takes no keyboard input, no
  focus and no Tab, and no mouse input unless it has a tooltip (`setToolTipText`,
  `setToolTip`), which needs its hover; with one, the hover and a press over it are
  its own, doing nothing, as on any control taking the mouse, so a spinner inside a
  Button leaves the button a dead spot there (no hover look, a press that presses
  nothing). A spinner in a host that takes the mouse has no tooltip of its own: the
  host's goes on the host, as SpinnerDemo's "Saving…" button has it. The three surfaces read only their top
  colour, as a stroke has no fill or border. `tests/unit/spinner.cpp` pins the
  control and its renderer's draws; `--scenario spinner` (`controltest_spinner`, its
  `_vulkan` twin and `controltest_verify_layout_spinner`) its slots, pixels and
  animations on SpinnerDemo.

- **The navigation rail.** `controls::NavigationRail`
  (`include/gweni/controls/navigationRail.h`) is a side rail of pages (Material's
  NavigationRail, WinUI's NavigationView) put together from what exists: docked Left,
  filling its parent's height, it holds a header slot docked Top (`setHeader`, which
  takes the control over and hides the one it replaces, `delayedDelete()`ing it as a
  TabControl does a closed tab, so a header may replace itself from its own press
  handler; `getHeader()` skips the one waiting to go), a footer docked Bottom
  (`getFooter()`, any controls) and the items between them (`internal::NavigationRailItems`,
  the rail's inner panel, so `newChild()` on the rail makes a main item). Both item
  containers enrol a `NavigationItem` in the rail's one `SelectableGroup` as it is
  added (`onChildAdded`) and take it out as it leaves, so a footer item made with
  `getFooter()->newChild<NavigationItem>()` is in the same group as `addItem`'s.
  - *Items.* `NavigationItem` is a `Selectable` that is not toggleable (a click on the
    selected row keeps it selected), with an `ImagePanel` icon (`setImage`, loaded by
    name as `Button::setImage` does; an empty name hides it, but keeps it, since it is
    the badge's anchor) and a `Label` child of its own, both owner-placed in its
    `arrange()`: the Selectable's own Text stays empty and hidden, so the label is a
    control the rail can hide without hiding the row. `setSelected(item)` (only an
    item of this rail; null clears it) and `getSelected()` go through the group, and
    `onSelectionChange` fires once per real change, `control` the new item. Moving the
    selected item between the main items and the footer (`setParent`) takes it out of
    the group and enrols it again, so that fires twice, null and then the item, which
    is selected again after.
  - *Collapse.* The app decides (`setCollapsed`, `isCollapsed`, `onCollapsedChanged`);
    the rail watches no width. Collapsed, each label is hidden (`setHidden`, never
    deleted) and its text is the item's tooltip (`setToolTipText`, kept in step by
    `setText`), and the rail's preferred width is `IconSize` plus the row padding;
    expanded, the tooltips are cleared, the labels shown, and the width is the widest
    shown item's preferred width, or `setExpandedWidth`'s. A collapse or expand is
    one `invalidate()` of the rail with the labels' visibility, so it lays out in one
    pass entering the rail's subtree and its parent's path, and marks only the labels'
    visibility and the items' new bounds. NavigationRailDemo collapses its rail below
    560 units from its own X arrange, before it docks the rail (inside a
    `PlacementScope`, the rail measured again there), so a resize across the width is
    one pass too.
  - *Badges.* `NavigationItem::setBadgeCount(n)` and `setBadgeDot(bool)` make a
    `Badge` (#609) on first use and `attach()` it to the icon's top-right corner, so it
    shows collapsed and expanded alike and a count change is the Badge's own: its slots
    alone, and a layout of the way down to it only when its size changes.
  - *Keyboard.* The rail is one Tab stop (the items take no keyboard input). Its focus
    enters at the selected item (the first enabled one if that is disabled or hidden),
    kept as a logical focus by control id (`getFocused()`, `isNavigationFocused()`);
    Up and Down move it through the main and the footer items, wrapping, Home and End
    to the ends, skipping disabled and hidden items; Return and Space select the focused
    item, Material's behaviour (WinUI selects on focus): moving the focus selects no
    page, so an app does not build pages the user only passes over.
  - *The look.* The textured `NavigationRail` renderer paints the `NavigationRail`
    surface in one slot, and `NavigationItem`'s renderer one slot per row: hidden when
    the row is idle (`Selectable`'s rule), else `NavItemHover`, `NavItemDown`,
    `NavItemFocus`/`NavItemFocusHover`, `NavItemSelected` and its `Hover`, `Focus`,
    `FocusHover` and `Disabled` variants. The selection's indicator is a bar on the
    leading edge drawn in the same slot: a selected surface's 3-unit border, with the
    edge mask keeping it off the top, right and bottom and squaring the left corners.
    The label's colour is `NavItemText`, `NavItemSelectedText` or `NavItemDisabledText`
    (`Colors.NavigationItem`), so a hover writes the row's slot alone and a selection
    the two rows' slots and their labels' glyphs, laying nothing out.
  `tests/unit/navigationRail.cpp` pins it headless, and ControlTest's
  `--scenario navigation-rail` the costs and pixels of a hover, a click, Down and
  Return, a badge count, a collapse, its tooltip and the expand on NavigationRailDemo.
- **Segmented buttons.** `controls::SegmentedButtons`
  (`include/gweni/controls/segmentedButtons.h`) is one row of joined toggle segments
  (Material's segmented button, WinUI's RadioButtons, a QButtonGroup of tool buttons).
  `addSegment(text[, image, imageOnly], name)` returns a `SegmentedButton`, a
  `Selectable` (so a Button) with text, `Button::setImage`'s image or both;
  `setImageOnly(true)` leaves the text out of the segment and makes it the tooltip.
  - *The selection.* Exclusive (the default) or Multi (`setMode`). The exclusive
    selection is a `SelectableGroup`, reused, not copied (RadioButtonController's
    radios keep their own checked state and the controller unchecks its children
    itself, so there was nothing to share): every segment is a member while the row
    is Exclusive, none while it is Multi, and `setAllowNone(true)` gives the members
    their toggling back, so a click on the selected one clears it. An exclusive row
    that does not allow none always has one selected once a segment exists: the first
    added, and after a removal the next enabled one, else the one before; the segment
    itself refuses that selection's deselect (`SegmentedButton::setSelected`, so
    `getSegment(i)->setSelected(false)` cannot clear it either), while the group moving
    the selection on deselects the old one after it holds the new one.
    `removeSegment` takes the segment out of the row at once (hidden, out of the
    group) and `delayedDelete()`s it, since it may be called from that segment's own
    click: an `onSelectionChange` handler runs inside `Button::onPress`, which goes on
    with the segment once it returns. A removed segment keeps the row's handlers until
    it is deleted (an `event::Caller` must not lose a handler while it calls it), and
    the row ignores them once the segment's row is no longer it. A segment the app
    deletes itself leaves the row from its own destructor, while its selection can
    still be read (Base's destructor, which tells the row, runs after the
    `Selectable` part is gone), and one moved to another parent leaves it in
    `onChildRemoved`, with its group, its owner placement and the check's room, its
    handlers kept and ignored as a removed segment's are (an `onSelectionChange`
    handler may move the clicked segment while its `onSelect` calls them). A hidden
    segment is not supported (it keeps its room and its place in the joints and the
    ends): remove it instead.
    `onSelectionChange` fires once per real change with the changed segment: the one
    the selection reached (the one it left fires nothing: its deselect comes while the
    group's selection is already the new one), the cleared one, or the toggled one.
  - *The row.* Owner-placed in its `arrange()`, before `Base::arrange` enters the
    segments (from `MinChangedWalk` (16) children on, that walks only the changed ones
    and calls neither `arrangeHorizontal` nor `arrangeVertical`, `Flow`'s case): each segment
    its content width (or with `setEqualWidths(true)` the widest one's), all the
    tallest one's height (a row its dock or a layout makes taller keeps them at it, at
    its top padding), and each one after the first starting `Joint` (1) unit left
    of the previous one's right edge (`segmentPositions`). A segment places its own
    Owner Text and image too, as one group centred in it: the check's room
    (`CheckSize` and `CheckGap`, in every segment while the row `setShowCheck`s, so a
    selection never changes a width), the image, the text, `SidePadding` in from its
    sides. So a hover, a selection and a toggle lay nothing out. A new text that moves
    the check inside bounds that stay (a narrower segment in a row of equal widths)
    marks the segment, so the check is drawn again where `getCheckRect()` says.
  - *The look.* The textured `SegmentedButton` renderer draws one rounded rect per
    segment, rounded only at the row's ends through the edge mask: the first is square
    on its right, the last on its left, the inner ones on both. A joint is its left
    segment's border column alone: a segment after the first is drawn from `Joint`
    units into its bounds with `NoBorderLeft`, so its fill starts where its left
    neighbour's bounds end. Both snap that one logical x, so the joint is
    `max(1, snap(border))` device px at any scale, as wide as the outer border, and a
    segment's state never repaints a neighbour; for the same reason the border colour
    is `Line` in every state (`Segment`, `SegmentHover`, `SegmentDown`, `SegmentOn`,
    `SegmentOnHover`, `SegmentDisabled`, `SegmentOnDisabled`), the state shown by the
    fill. A selected segment of a row that shows checks draws the check icon (kind 0)
    at `getZIndex()+1` in `getCheckRect()` (`depthLayers()` 2). The segment holding
    the keyboard focus is drawn as it is otherwise, with `SegmentFocus`'s border, and
    after the first a third slot paints its left joint in that colour: a rect of no
    width at its fill's left edge, `NoBorderLeft` with the border's width as its
    `reach`, which the renderer widens over exactly the device pixels the neighbour's
    border has. Its own left border could not: a 1-unit joint is 1 or 2 px at 1.5 by
    where it lands and the border always 2, so drawn over its bounds it left a column
    of the neighbour's border beside it on every other joint (3 px in all). The slot is
    hidden while the segment is not focused (`requiredPrimitives()` 3), and the
    scenario's `focus_joints` scans each joint with each segment focused. It is not an
    inset ring: style 4's alpha is the fill's times the shape's, so a rect with a
    transparent fill shows no border either. The text is `Colors.SegmentedButton`'s
    (`SegmentNormal`, `SegmentHover` (the normal one, so a hover writes no glyph),
    `SegmentSelected`, `SegmentDisabled`; `SegmentCheck` the check's).
  - *The keyboard.* The row is one Tab stop (`SegmentedButton::isTabable()` asks
    `getTabStop()`: the focused segment, else the exclusive selection, else the first
    enabled one), so Tab enters at the selection and Shift+Tab leaves from wherever
    the focus moved. Left/Up and Right/Down step to the previous and next enabled
    segment, Home/End to the ends, without wrapping; an Exclusive row's selection
    moves with the focus, a Multi row's focus moves alone and Space/Return
    (Selectable's) toggle the focused segment. An Exclusive row's segment does not take
    Return, which has nothing to toggle there, so Enter goes on up to a dialog's
    default button. The keys reach the row because a
    segment does not take them and `Base::onKeyPress` passes an unhandled key up. A
    disabled row disables every segment (`SegmentedButton::isDisabled()` asks the row
    too, so each gets its own state back) and takes no key and no Tab.
  - *The cost.* A hover writes the hovered segment's slot; an exclusive change the two
    segments' slots and their Texts' glyphs (the colour); a multi toggle one
    segment's fill and check; none lays out. `--scenario segmented` holds it on the
    GPU pixels and the frame's counts, at scale 1 and 1.5, and measures every inner
    joint as one run of the border colour as wide as the outer border.
    `tests/unit/segmentedButtons.cpp` pins the selection, the events, the widths and
    joints, the removal and the keyboard, `tests/unit/segmentedLook.cpp` the
    renderer; SegmentedButtonsDemo shows each kind of row.
- **Scale and the device-pixel grid.** Controls lay out in logical units (integers);
  `renderer::Base::translate()` turns them into device pixels at `scale()` (set by
  `Canvas::setScale`). It snaps **edges**, not origin and size apart: a point is
  `snap(x+offset.x)`, a rect is `x0=snap(x+offset.x)`, `x1=snap(x+offset.x+w)`,
  `w=x1-x0`, and the same in y, where `Base::snap(v)` is `floor(v*scale+0.5)`. So
  two rects that meet in logical units meet on one device-pixel edge at any scale,
  with no seam or 1 px overlap. The SDF radius is `snap(radius)` and a border
  `max(1, snap(border))`, so a border stays a whole number of device pixels; icon
  strokes are relative to their rect and scale with it. At scale 1 all of this is
  the identity. Text is drawn at `font.size*scale()` px from its snapped origin
  (see "Hybrid text"), while `measureText` stays in canvas units. `scale()` is the
  scale of the space being drawn in: the canvas's, times every ZoomPanel's zoom the
  control is in (see "The zoom panel").
- **Where the scale comes from, and a change of it.** The GLFW examples size the
  canvas in logical units: `capture::fitCanvasToWindow` (`examples/openglcore/capture.cpp`,
  run once when the window is made and from the framebuffer-size and content-scale
  callbacks) gives the renderer the framebuffer in device pixels as its view, and the
  canvas `round(framebuffer/scale)` at `scale = glfwGetWindowContentScale x` the manual
  factor (`--scale`, else `GWENI_SCALE`, else 1; it multiplies, so it is also an app
  zoom). An offscreen capture and the EGL context have a content scale of 1. A visible
  window is created with `GLFW_SCALE_TO_MONITOR`, so on X11 and Windows it is its
  requested size in logical units too. The cursor arrives in window coordinates;
  `input::GLFW3` maps it with `input::windowToLogical` (`x*(fbW/winW)/scale`, before
  truncating, so the deltas are logical as well) through the sizes and scale the same
  call sets with `setWindowMetrics`, and the canvas only sees logical units. That one
  formula is right where window coordinates are points (macOS, scaled Wayland) and
  where they are pixels (X11, Windows). In the browser the window is
  emscripten-glfw's with `GLFW_SCALE_FRAMEBUFFER`: its window coordinates are CSS
  pixels, its framebuffer the canvas's CSS size times `devicePixelRatio` (floored) and
  its content scale `devicePixelRatio`, and the canvas element follows the size of the
  page's `#canvas-container`; a page resize and a change of `devicePixelRatio` (a
  zoom, or a move to another display) come as the same two callbacks, so the same
  `fitCanvasToWindow` and `windowToLogical` serve it (`web_input` checks a 640x480 CSS
  container at `devicePixelRatio` 2: a 1280x960 framebuffer and a 640x480 canvas laid
  out as a capture's at `--scale 2`). `Canvas::setScale` reaches
  `Base::onScaleChanged` on every control, in one walk (`Base::scaleChangedSubtree`,
  each control invalidated once without walking up), which invalidates its layout and marks it
  `StateChange_Geometry` once, so the next frame lays the tree out again and re-emits
  every primitive and clip box at the new scale; the text runs re-lay themselves,
  their `px` being in the run cache key. `ControlTest --scenario scale`
  (`controltest_scale_roundtrip`, every control) checks that 1 -> 2 -> 1 comes back
  byte for byte and that the 2x frame is a canvas built at scale 2.
- **Clip boxes.** Every primitive slot and every glyph slot carries the `clipRegion()`
  current when it was drawn, as a `Box` of four `uint16_t` corners (`m_clippingBoxes`,
  `m_textClipBoxes`), its edges snapped like the vertices' and clamped to `[0, 65535]` with
  `x1 >= x0`, `y1 >= y0`, so an empty region is an empty box. Each list lives in a 2D
  `GL_RGBA16UI` texture (`m_clipTexture`, `m_textClipTexture`, on texture unit 1),
  one texel per slot, `SlotRenderer::kClipColumns` (2048) slots a row: slot `s` is
  texel `(s % 2048, s / 2048)`, read with `texelFetch` (nearest filtering, as an
  integer texture needs). The fragment shaders `discard` outside `[x0,x1) x [y0,y1)`.
  Both programs take the slot from the vertex shader as a `flat uint`,
  `slotBase + (gl_VertexID >> 2)`: each chunk is drawn from index 0 of its own slots
  with its attributes pointed at its first slot (the `slotBase` uniform, set per
  chunk), every slot exactly 4 vertices, and the four vertexes of a slot carry the
  same value, so whichever is the provoking vertex gives it. (The primitive shader
  read `gl_PrimitiveID >> 1` in its fragment stage before, which GLSL ES 3.00 lacks and
  a Vulkan fragment shader has only with geometry shaders; the text shader always took
  the slot from the vertex, since each of its draws covers only some of its chunk's
  slots, below.) The boxes are uploaded like the vertices, dirty slots only, compared
  by bytes (`uploadDirtyClipBoxes`): each dirty run is one `glTexSubImage2D` per row
  piece it covers (its part of its first row, the whole rows between, its part of its
  last), straight from the mirror, whose rows are consecutive. The boxes the mirror
  gained since the last upload go up with the dirty runs. Only when the texture's rows
  no longer hold the mirror is it re-allocated, with the power of two rows at or above
  1.5 times what the mirror needs, and every box sent again, as a vertex buffer's
  regrow does (`FrameStats::bufferRegrows`; see "Incremental GPU upload" below). Its
  height is a power of two (grow-only): llvmpipe compiles a shader variant per sampler shape,
  power-of-two or not among it, so any other height compiled one more mid-frame (with
  LLVM's C++ allocations in `gweni_bench`'s `allocs_flush`); 2048 wide and up to 2048
  rows (GLES 3.0's least `GL_MAX_TEXTURE_SIZE`) holds 4 M slots. The bytes sent are
  those the buffer textures were sent, and only `FrameStats::uploadCalls` changed: a
  run that crosses a row boundary is two or three calls where it was one. Over
  `gweni_bench`'s scenes at their default N (`upload_calls_total` before -> after, every
  other counter, `upload_bytes` included, the same): `build` 4 -> 5, `drag` 140 -> 160,
  `scroll` 60 -> 99, `type` 51 -> 62, `add-remove` 40 -> 50, `resize` 1630 -> 1640,
  `scale` 7416 -> 7425, `tree-toggle` 38640 -> 38680; `idle`, `hover-sweep`,
  `set-text`, `animate` and `bring-to-front` unchanged. `startClip()`/`endClip()` do
  nothing in this renderer. `GWENI_CLIP_DEBUG=<inset px>` shrinks every box by the
  inset on each side, which shows the boxes on screen.
- **One text draw per font atlas.** Every loaded font has its own freetype-gl atlas
  texture: a coverage font per (face, `coverageSize(px)`, bold) below the SDF threshold, one SDF font
  per (face, bold) at or above it (see "Hybrid text"). Each `TextRun` records the `FontData` it was laid out in,
  and each `FontData` keeps the text ids of its runs (`textRuns`). `flush()` binds
  each atlas in turn and draws its runs with one `glMultiDrawElements` per index chunk
  they reach (or, without multi-draw, that draw's ranges as a loop of `glDrawElements`,
  counted as one draw; "One GL subset" above), one per atlas until the glyph slots outgrow a chunk
  (`FontData::drawChunks`, built by `buildTextDrawList`): the glyphs in use of each
  run that is not hidden, adjacent ones merged, split at the chunks' edges, in slot
  order, as index ranges of the static glyph-quad buffer from the chunk's first slot,
  with the atlas's `TextMode` in the `textMode` uniform. That list is what keeps a
  hidden run, a freed one and a shrunk run's tail off the screen: no index is ever
  written for them, so hiding, showing or shrinking a run uploads no index (there are
  none to upload) and marks only its font's list for rebuilding (`markTextDrawList`).
  A run marks it only when what it puts in the list changes (`drawnRange`: its first
  slot and glyph count, or nothing while hidden or empty) or it moves to another font
  (`setTextRunFont` marks both), so a string relaid in the same slots and length, a
  Label given a new string as long as the old one, rebuilds nothing. A rebuild is
  still O(runs in the font) and a sort, but refills the entries already in
  `drawChunks` and a scratch vector of ranges, so in steady state it allocates
  nothing; a debug build checks every list it did not rebuild against a rebuild
  (`verifyTextDrawList`) and asserts that they match.
  The slots are never moved for this, so the dirty-slot upload is the same with one
  font or many.
- **A font is held by its owners and by the runs laid out in it.** Every caller
  holding an equal `Font` shares one entry (two Labels in the same font, a skin's
  default font and the Texts drawn in it), so `loadFont`/`freeFont` are a counted
  pair: each successful load adds one to `FontData::owners`, each `freeFont` takes
  one back, and a font is freed only when none is left. Even then it is deleted only
  if no run is laid out in it; otherwise it is marked `freed`, every run keeps
  drawing from the same atlas, and it goes when its last run leaves it
  (`setTextRunFont`), unless a `loadFont` gives it an owner again first. That
  matters most for an SDF atlas, which every size of the face shares, but holds in
  every mode. A font loaded only by a use (`ensureFont`, from `renderText` or
  `measureText`) has no owner. Once its last run has left it, it is kept for
  `SlotRenderer::kIdleFontFlushes` (60) flushes, about a second, and then erased by
  the next `flush()` (`FontData::idleSince`, `eraseIdleFonts`) unless a run is laid
  out in it or a `loadFont` owns it before then: so the sizes a zoom or a canvas
  scale passes through do not stay, while a size a zoom wiggles back across, a
  scale cycled through, or a font a dialog closed and opened again draws in is not
  rasterised again. (Erased at once, as a freed one, it cost the bench's `build`
  scene its primer's bold font, rasterised again in the measured frame, 19 times the
  update phase's instructions, and its `scale` scene a re-rasterised atlas at every
  scale change, three times the render phase's.) A freed font used again is a use's
  from then on. One no run was ever laid out in (a measure-only font) stays cached
  until a `freeFont` asks for it.
  `setTextAA` is the one place a font goes with runs in it: it takes the coverage
  fonts' runs off every list (they keep their slots and draw nothing until their next
  `renderText` lays them out in the new mode) and loads each again with the owners it
  had. A font loaded at one `scale()` and freed at another gives back its owner on
  the drawn key if it has one there, else on the measured one (`font.size`).
- **A missing face is drawn in a built-in font.** The library embeds
  `resources/OpenSans.ttf` (Apache-2.0, `resources/OpenSans-LICENSE.txt`): the build
  turns it into a byte array (`cmake/embedFile.cmake`, `generated/fallbackFont.cpp`
  in the build tree). When a face's file is missing or freetype-gl cannot read it,
  `loadFont` logs that error, then loads this font from memory
  (`texture_font_new_from_memory`) into that key's own manager and atlas, set up for
  its mode like any face, logs `using built-in fallback for <face>` and returns
  `Font::Status::LoadedFallback` (`FontData::fallback`, so a second `loadFont`
  says so again). It is in `m_fonts` like a loaded face, so it is loaded and
  reported once per key, and `ensureFont` counts it as loaded. Only if that fails
  too, or with `setFallbackFont(false)` (a test hook), does the key go into
  `m_failedFonts`, its text drawing nothing and measuring `(0,0)`. This is a
  *file* fallback for a whole face; a codepoint a face has no glyph for is the glyph
  fallback chain's ("Font fallback" below).
- **Grayscale text on the pixel grid.** Each font's atlas is depth 1 (`LCD_FILTERING_OFF`):
  one coverage byte per texel, a `GL_R8` texture uploaded as `GL_RED` with
  `GL_UNPACK_ALIGNMENT` 1, a third of the bytes an RGB atlas takes. The vendored
  freetype-gl rasterises a hinted depth-1 atlas with `FT_LOAD_FORCE_AUTOHINT |
  FT_LOAD_TARGET_LIGHT`, which snaps to the grid vertically only, so stems keep their
  horizontal position. A glyph's
  advance comes from that same hinted load, in whole pixels, and kerning is rounded to
  whole pixels too, so the pen stays on whole pixels and identical glyph pairs get
  identical gaps; `text_buffer` places each quad at `round(pen.x) + offset_x`, as wide
  as its bitmap, so every pixel samples exactly one texel, and keeps the pen's
  fraction in the vertex's `shift` (zero while hinted).
  `renderText` lays out through `text_buffer_add_text`, and `measureText` walks the same
  pens without building a `text_buffer_t`: `SlotRenderer::walkPens` is the one pen walk
  that `textPens` (so `measuredPenShifts`), a coverage `measureText` and `measureCarets`
  read, stepping through the codepoints as `text_buffer_add_text` does and adding each
  glyph's kerning against the codepoint before it and its advance, so the measured width
  (the widest line's final pen) is the drawn advance. It finds each glyph in its `FontData`'s table
  (`FontData::glyph`: an array for ASCII, a map above, filled on first lookup and freed
  with the font, which `layoutSdfText` looks its glyphs up in too) instead of
  freetype-gl's linear search over the font's glyphs; `tests/measureTextPens.cpp` holds it to a `text_buffer_t`'s pen
  (`measureTextWithTextBuffer`, line by line) in every coverage size, mode and scale. The
  measured height of one line is the line box, `ceil(font->height)` (ascender-descender, no
  line gap) whatever the string; in an SDF font `ceil(font->height*px/48)`. A string with
  `'\n'`s, which `renderText` draws on as many lines (an unwrapped `Text`, a tooltip), is
  measured as an SDF one is: the widest line, and `ceil(lineBox + (lines-1)*lineStep)`, a
  trailing `'\n'` an empty last line. `SlotRenderer::lineStep` is the distance freetype-gl
  draws lines apart, `ascender - (int)descender` (`text_buffer_finish_line` moves the pen
  down by the truncated descender, the next glyph by the ascender), less than the line box
  (14.76 against 14.98 at 11 px); in an SDF font `round((ascender-descender)*px/48)`.
  `lineHeight(font)` reads that
  same metric (`SlotRenderer::lineBoxHeight`) without laying a string out, so it is
  `measureText(font, " ").y` exactly (`tests/lineHeight.cpp`). The text shader outputs
  `vec4(color.rgb, color.a * coverage)` under the `SRC_ALPHA, ONE_MINUS_SRC_ALPHA`
  blend, so coverage is applied once; no gamma or contrast curve is applied
  (`markup.gamma` is 1). `tests/textGrayscale.cpp` pins all of this.
- **LCD subpixel text (opt-in).** `SlotRenderer::setTextAA(TextAA::{Grayscale, LcdRGB,
  LcdBGR})`, or `GWENI_TEXT_AA=gray|rgb|bgr` at construction; Grayscale by default, and
  then every output is the grayscale path's byte for byte. In an LCD mode a coverage-range
  font resolves to `TextMode::LcdRGB`/`LcdBGR` (the mode is part of the `FontKey`, so
  grayscale and LCD fonts never share an atlas) and loads a depth-3 atlas
  (`LCD_FILTERING_ON`): freetype-gl rasterises it with FreeType's LCD filter
  (`FT_RENDER_MODE_LCD`), one coverage byte per subpixel, uploaded
  as `GL_RGB8`/`GL_RGB` initially and in the dirty bands. The text fragment shader has a
  dual-source output (`layout(location=0, index=1) Coverage`): for an LCD atlas it is the
  texel's `.rgb` (`.bgr` for BGR) times the colour's alpha, and `flush()` draws that
  atlas's runs under `glBlendFunc(GL_SRC1_COLOR, GL_ONE_MINUS_SRC1_COLOR)`, putting the
  previous blend back after, so each channel blends by its own subpixel's coverage. The
  other modes leave `FragColor` as it was and their blend does not read `Coverage`.
  **Glyphs sit at 1/3 device px.** An LCD font sets freetype-gl's
  `texture_font_t::subpixel_positions` to 3 (a gweni patch, before its cache loads):
  glyphs load with `FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_LIGHT` (vertical-only
  hinting, so the outline's x is the design's), advances are the unhinted
  `linearHoriAdvance` and kerning is `FT_KERNING_UNFITTED` unrounded, so the pen is the
  exact fractional sum. `text_buffer_add_char` rounds the pen to the nearest third,
  puts the quad on the whole pixel below it and draws the image of that phase
  (`texture_font_get_glyph_phase`): the outline moved right by `phase*64/3` 26.6 units
  (`FT_Outline_Translate`) before `FT_Render_Glyph(FT_RENDER_MODE_LCD)`, each phase a
  glyph of its own in the atlas, keyed by `texture_glyph_t::phase`. The cache is
  rasterised at phase 0 and the other phases as runs first need them; three phases of
  8-16 px ASCII take 1-3 % of the 1024x1024 atlas. The vertex's `shift` is `phase/3`.
  `measureText` walks the same pens (`walkPens`; the phase moves the quad, not the pen),
  so its width (the final pen, rounded up) is the drawn one, and it rasterises no phase
  variant: `renderText` does on the run's first draw; at a canvas scale an LCD run gets no
  `measuredPenShifts`, since unhinted pens scale linearly. **Contrast:** blending sRGB
  values, light text over a dark background comes out thinner than dark over light, so
  the LCD branch of the text shader raises each subpixel's coverage through
  DirectWrite's enhanced-contrast curve `c*(k+1)/(c*k+1)`, `k = contrast * luminance`
  of the text colour: white text gets the most and black none. `contrast` is
  `setTextContrast()`, `GWENI_TEXT_CONTRAST=<f>` at construction, default 1 (mid
  coverage 0.5 becomes 0.67 for white); 0 turns it off. Grayscale and SDF text do not
  use it. The SDF range stays
  grayscale in every mode. **Without dual-source blending** (a GLES context without
  `GL_EXT_blend_func_extended`, WebGL 2, `GWENI_GL_NO_DUAL_SOURCE=1`) the LCD modes fall
  back to grayscale with one warning per renderer ("One GL subset" above). `setTextAA` at runtime replaces every coverage-range font with
  one in the new mode (its runs drop out of the draw lists and draw nothing until laid out
  again) and raises `renderer::Base::textLayoutSerial()`; `Canvas::updateCanvas` sees the
  new serial and calls `onScaleChanged()`, the whole-tree re-layout a scale change uses, so
  the next frame is the one a canvas started in that mode draws. `tests/textAA.cpp` pins
  it: stem edges blue-left/red-right in rgb, mirrored in bgr, R == B in gray, and runtime
  switches matching fresh renderers pixel for pixel; `tests/textSubpixel.cpp` the
  positions (each glyph within 1/3 px of FreeType's own unhinted pens, at scale 1 and
  1.5), the atlas fill and the contrast curve. `renderer::Vulkan` blends it with the
  `dualSrcBlend` device feature and falls back to grayscale without it.
- **Hybrid text.** With the effective size `px = textPx(font) = font.size*scale()`,
  text below `GWENI_SDF_TEXT_MIN`
  (default 18 px; `0` = SDF everywhere, a large value = coverage everywhere) keeps the
  hinted grayscale coverage atlas (above) of its (face, `coverageSize(px)`, bold). At or
  above it, a font resolves to its face's one R8 signed-distance-field atlas (1024²,
  rasterised by FreeType's `FT_RENDER_MODE_SDF` at 48 px with a 6 px spread, unhinted),
  shared by every size. `layoutSdfText` scales that atlas's metrics and unhinted
  advances by `px/48` and puts the baseline on a whole pixel, for `measureText` and
  `renderText` alike. The text shader's SDF mode anti-aliases the 0.5 edge over about a
  screen pixel (`smoothstep` with `w` from the field's gradient) and discards where it
  draws nothing, so a glyph's padded quad writes no depth. A size change inside the SDF
  range therefore re-lays only that run: its vertex slots are uploaded and nothing else
  (the glyph indices are static).
- **Coverage sizes are whole device px.** A grayscale coverage atlas is rasterised at
  `SlotRenderer::coverageSize(px)`, `px` rounded half up to a whole device px (at
  least 1), in every tree and at every canvas scale: `resolveFont` keys it so, while
  the run keeps its exact `px` (`TextRun::px`) and `measuredPenShifts` puts each glyph
  at its measured pen times the scale, rounded to a whole pixel, so the run still
  ends where the layout sized it. So 11 px text at 1.5 is drawn from the 17 px atlas,
  not a 16.5 px one, and a continuous zoom (a ZoomPanel) on coverage text passes
  through whole sizes only, at most one atlas per size below the SDF threshold per
  face, each erased `kIdleFontFlushes` flushes after its last run left it (above). An LCD atlas keeps the exact
  `px`: its glyph images are rasterised at the 1/3 px phases of the unhinted pens at
  that size, which a whole-px shift would not keep. Positions, rects, lines and SDF
  text stay continuous. At a whole canvas scale nothing changes; at 1.25 and 1.5 the
  coverage text's pixels moved (no checked-in sample is at a fractional scale).
  `tests/zoomTextAtlases.cpp` sweeps a ZoomPanel 0.25 -> 4 -> 0.25 in 200 steps (16
  whole sizes and the SDF atlas, never more than one font more per size passed
  through, none left once 60 frames have passed, and a size come back to a few frames
  later uploading nothing).
- **Text at a canvas scale.** `renderText` resolves and lays a run out at
  `px = font.size*scale()` (11 px UI text at scale 2 is 22 px, in the SDF range), from
  its origin snapped to a whole device pixel by `translate()`; a `TextRun` keeps the
  `px` it was laid out at, so a run drawn again after a scale change is re-laid even in
  the SDF atlas both sizes share. `measureText` measures at `font.size`, in the font that
  size resolves to at scale 1, so it returns **canvas units** and a layout is the same
  at every scale (`controltest_scale_bounds`). This deliberately differs from the legacy
  OpenGL and Software renderers, which return scaled pixels. Where a run is drawn as
  grayscale coverage, its drawn font's hinted whole-pixel advances are not the measured
  ones scaled, whether it was measured as coverage or as SDF (an SDF `font.size` drawn
  below the threshold: 20 px at 0.75 is drawn at 15 px, 24 px at 0.5 at 12), so
  `measuredPenShifts` moves each drawn glyph to its measured pen position times the
  scale (rounded to a whole pixel for a coverage draw): the run ends where the layout
  sized it, and a caret placed from `measureText` falls between the glyphs it means
  (without it, "AVATAR Toyota WAVE fly" at 24 px times 0.5 had a glyph 2.9 px off). Nor
  is the drawn font's line step the measured one scaled, each truncating its descender to
  a whole pixel: at 10 px times 1.5 it is a pixel longer per line, so the lines of a run
  with `'\n'`s would end below its box. `measuredPenShifts` also moves each of its glyphs
  down by whole pixels, to within half a pixel of the measured line's baseline scaled
  (relative to the first line, so a one-line run moves nothing), in grayscale and LCD alike.
  Only a run whose layout is linear in px needs no x shift: one drawn in an LCD font
  (unhinted, to within its 1/3 px) and an SDF `font.size` drawn as SDF. The SDF one's line step
  is rounded to a whole pixel at each px (20 px at 1.5 steps 41 px against 27 times
  1.5), so the line correction applies whatever the measured mode, drawn as SDF or as
  coverage (20 px at 0.75 is drawn at 15), at a canvas scale and in a `ZoomPanel` alike.
  At scale 1 nothing of this changes a byte. `tests/scaledText.cpp`, `tests/scaledSdfPens.cpp`
  and `tests/multiLineText.cpp` pin it.
- **Ink past the pens: the Label's clip margin.** `measureText` measures pens (whole-pixel
  advances and kerning, `ceil`ed), never ink, and a Label is sized to that box, so a
  glyph's ink can reach past it: a leading 'J' a pixel left of it at scale 1, a trailing
  'g' at 10 px times 1.25 two device columns right of it (the glyph is rasterised at
  `size*scale`, and `measuredPenShifts` places its pen, not its ink), LCD text's filter
  fringe a pixel left of the first glyph, SDF's anti-aliased edge a pixel out. Rather
  than measure ink, which would change layout at scale 1 and could not bound ink drawn
  at `size*scale` without making layout depend on the scale, a Label lets the ink show:
  it does not clip as one (`shouldClip()` is false) but gives each child a clip rect of
  its own (`getChildClipRect()`), its bounds `{0, 0, w, h}` for every child but its
  Text (the region clipping at the bounds gave before) and, while the Text lies wholly
  inside the Label, the bounds widened by `Label::inkMargin(font.size)` =
  `ceil(size/12)+1` canvas units on the left and the right. The margin is measured by
  `tests/labelInkOverhang.cpp`, which sweeps every printable ASCII glyph as a run's
  last and first at 8 to 24 px, scales 1 to 2, grayscale, LCD and SDF, on GL and
  Vulkan, against the run drawn alone: the ink reached up to 4 device px past the box,
  and the fewest whole units that showed all of it, the edges snapped, were 2 up to 18
  px and at 23 and 3 at 19 to 22 and 24; the margin is about a twelfth of the size plus
  a unit for the fringe and the snapping (one unit more than was needed at 13 to 18 and
  23 px), and the test fails if any run needs more than `inkMargin`. Ink may so reach up
  to the margin into a neighbour's area, as text does in any toolkit, and the rect is
  still cut by every clipping ancestor's region (a Label at a ScrollControl's edge draws
  nothing outside its viewport). A Text that is not wholly inside (a Label narrower than
  its text, whose unwrapped Text keeps its measured width, `Text::policyResize`; a
  TextBox's scrolled text) is clipped at the bounds as before, and so are the lines of a
  wrapped Text, which are its children, except that a long word overflowing its line is
  not seen (the Text's bounds fit) and shows up to the margin. The rects are members
  (`m_childClip`, `m_textClip`) worked out again when the Label's size, its Text's
  bounds or its font change, and a change marks the subtree of each child whose region
  it changed, through `Base::clipRectRegionChanged()`, the ancestor walk
  `childRegionChanged()` makes, on two rects; so `hasChildClipRects()` stays false and
  a Label's new size marks no more than it did. `TabButton`, which never clipped, gives
  no rects, and a subclass that overrides `getChildClipRect()` gives Label's for the
  children it does not clip itself. The Inspector says `clips per child` for such a
  control. Layout and every `--dump-bounds` listing are unchanged; the pixels change
  only in the ink edge columns that were cut off. `tests/labelInkOverhang.cpp` also pins
  the narrow Label, the scrolled TextBox, the ScrollControl edges and a fit toggled by a
  resize and by a text change against fresh builds, and `tests/unit/labelClipRects.cpp`
  the rects and their marks.
- **A moved run is a fresh layout.** A same-string run that only moved is not laid
  out again: `renderText` adds the whole-device-pixel delta to its glyph vertexes.
  That equals a fresh layout at the new place only if each sum is exact, and an SDF
  quad's corners (pen and offset times `px/48`) and a coverage run's
  `measuredPenShifts` are fractions a float holds only rounded. So the rebuild rounds
  each corner's offset in the run to 1/256 device px before it adds the run's
  position: a dyadic offset plus a whole pixel is exact while |x| < 65536, and so is
  every later move. Without it a Window dragged around and back at `--scale 2`, its
  11 px text drawn as 22 px SDF, came back a few ulps off and changed SDF edge
  pixels (`controltest_clip_scale200`). `tests/textRunCache.cpp` moves SDF, coverage
  and LCD runs away and back and in steps and compares each with a fresh layout.
- **Colour spans in a text run.** A run's colour is baked into its glyph vertexes, so
  highlighting a line one text id per token would multiply the ids, runs and draws.
  `renderer::Base::renderText(textId, font, pos, z, text, spans)` takes
  `renderer::ColorSpan`s instead: sorted byte ranges of the string, each a colour; a
  glyph takes the colour of the span holding its codepoint's first byte, one in no
  span the draw colour, all opaque. The run is laid out as the plain call lays it
  out, so it measures as `measureText` says. `renderer::Base`'s default sets the draw
  colour to the first span's and makes the plain call. `SlotRenderer` keeps a run's
  spans and its glyphs' byte offsets (`glyphByteOffsets`, the layout's walk: no glyph
  for a `'\n'`, an ignorable or a codepoint the face has none for) in
  `m_textRunSpans`, by text id, and only a flag in the `TextRun` (in `hidden`'s
  padding): the first version kept two vectors in every run and `build`'s
  `alloc_bytes` grew by 184 KB at N=1000, and its span comparisons cost the plain
  call +1.7 % of `render` in `scroll` and `tree-toggle`. A run with spans, or one
  drawn plain after it had some, given the same string, font and size is not laid out
  again for a new colour or new spans: `colorTextRun` writes each glyph's colour and
  marks for the upload only the glyph slots whose colour changed, with the arithmetic
  the markup's foreground colour is made with, so a glyph in the draw colour has the
  plain call's bytes. The plain call on a run that never had spans rebuilds it for a
  new colour, as it always did, so its counters and pixels are unchanged
  (`docs_samples`, `backend_pixels` and `tools/bench.sh compare` with no flags).
  `tests/colorSpans.cpp` (GL and Vulkan) draws three spans in their colours, writes
  only the changed glyphs' slots for a span change with no atlas upload, and holds
  the plain call to its old bytes; `tests/unit/colorSpans.cpp` the default and
  `StubRenderer`'s record (`textSpans`). A `CodeEditor` line's run, an
  `internal::CodeLineText`, has a skin renderer of its own (the textured skin's
  `CodeLineText`) that draws through the overload; every other Text's renderer is the
  plain call it was (a virtual check of every Text for spans cost `resize` +1.3 % of
  `render`).
- **Every caret from one layout: `measureCarets`.** `renderer::Base::measureCarets(font,
  text, xs)` gives the x of each caret position of `text`, one per codepoint boundary
  of `utf8Next`'s walk with 0 and `text.size()` included, so the codepoint count + 1
  entries: `xs[k]` is by definition `measureText(font, prefix to boundary k).x`, 0 for
  the empty prefix. The base class's default is that definition, one `measureText` per
  prefix, so a backend that does not override it answers as before. `SlotRenderer`'s
  override lays the string out once and reads the pen before each codepoint: in a
  coverage font (grayscale or LCD) `walkPens`, the walk `measureText` makes; in an SDF
  font `layoutSdfText`; both `ceil(max(width, pen))` before each codepoint, the widest
  line so far. A `measureText` of a prefix is the same walk stopped there, so each x equals it
  by construction. Where freetype-gl's codepoint steps (`utf8_surrogate_len`) are not
  `utf8Next`'s (malformed UTF-8) or the string holds a NUL, it falls back to the
  default. `Text::closestCharacterIn`, under every TextBox click and drag-select move,
  makes one such call per probe instead of a `measureText` per caret, so it costs
  O(L) glyph layouts for a line of L codepoints, not O(L²); `controls::Base::measureCarets`
  counts it as one measure (`measureTextCalls()`). `tests/measureCarets.cpp` pins the
  equality and `getClosestCharacter`'s answers against the old per-prefix walk in every
  mode and at scale 1.5; `tests/unit/closestCharacter.cpp` the one call per probe.

## Syntax highlighting

`text::Tokenizer` (`include/gweni/text/tokenizer.h`) is one line at a time: `tokenize(line,
start, tokens)` appends the line's `Token`s (a byte range and a `TokenKind`, ascending, not
overlapping; a byte in no token is plain) and returns the state the next line starts in.
The kinds are a fixed set every language maps onto, so a theme gives each one colour:
plain, keyword, string, number, comment, key, punctuation, heading, emphasis, strong,
code, link, template, added, removed, hunk and meta. The state, `TokenState`, is six
`uint32_t` words a language uses as it likes (all zero its usual start) and that are
compared to tell whether an edit changed what the next line starts in; a tokenizer keeps
nothing between calls, so a cache can redo any line from its start state
(`tests/unit/highlightLanguages.cpp` tokenizes every line twice and fails on a
difference). A language nested in another keeps its state in the words after the outer
one's first (`TokenState::nested`, `inner`), so Jinja over Markdown over a fence's
language (up to three words, bash's heredoc) fits.

**The languages** are hand-written state machines, one file each in
`source/text/highlight/` with the shared byte classes in `scan.h`, registered by name
(`findLanguage`, `registerLanguage`, `languageNames`; `languageForFileName` maps
`*.yaml`/`*.yml`, `*.json`, `*.md`, `*.md.j2`, `*.diff`/`*.patch` and `*.sh`). YAML: document
markers and directives, keys (a scalar before `: `, flow collections included), plain
scalars that are numbers or the literals, quoted ones, comments, sequence dashes, anchors,
aliases and tags, and block scalars, whose lines are strings while blank or indented past
the node they belong to (the key's column, or an entry's `-`), the state `{1, column}`.
JSON, with JSONC's comments. diff: the file headers, a hunk header, and in a hunk (its old
and new line counts in the state) `-` and `+` lines whatever they look like. bash:
comments at a word's start, keywords as whole words, assignments, `'...'` and `"..."` over
lines with the `$variables` cut out of the latter, and heredocs (the delimiter's FNV hash
and length in the state; a `<<` inside a `$(( ))` or `(( ))` on its line is a shift and
opens none, nor does a here-string `<<<`). Markdown: ATX headings, thematic breaks, quote and list markers,
HTML comments over lines, code spans, strong, emphasis, links and escapes, and fences
whose body is tokenized by the fence's language (yaml, json, bash or sh, diff), its state
nested after the fence's word (bits for the mode, the language, `~` and the length), Code
for any other.

**Jinja** is an overlay, `text::makeJinjaOverlay(host)`: the tags `{{ }}`, `{% %}` and
`{# #}` are cut out of each line first (a quoted string in a tag closes nothing), each of
which may run over lines, the open one in the state's first word and the host's state
after it. The host tokenizes the line with each tag's bytes made `x`s, so the Markdown
around a tag reads as if the tag were a word (a heading or a `**...**` holding one is
still one, cut at the tag); a line wholly inside tags is none of the host's and leaves its
state as it was, so a fence or a YAML block scalar inside one goes on past a comment line.
A `{% %}`'s statement name is a keyword, a quoted string a string, the rest template,
`{# #}` a comment. The registry's `markdown+jinja` is Jinja over its own `markdown`.

**The cache**, `text::HighlightCache` (`include/gweni/text/highlightCache.h`), keeps each
line's tokens, the state it was tokenized from and its end state, following a `Document`
through its own observer (`attach`) or a caller's `linesChanged()` (the editor's, first in
its own observer, so a row bound there reads the edited lines' tokens). Lines `[0,
frontier())` are known fresh. A line past the frontier is checked when asked for: walked
in order, a line whose text is unchanged and whose start state is the previous line's end
is fresh as it is, at the cost of one comparison, and any other is tokenized again. So an
edit tokenizes its lines; when its last line's end state is what it was the lines after
are compared, not tokenized, and an edit that changes no state tokenizes one line. One
that does (an opened fence, block scalar or `{# ...`) tokenizes through the last line on
screen (`setVisibleLines`, the pooled rows' lines, moved with their lines by an edit above
them, as the list's anchor moves, so lines inserted above the screen take the pass to the
lines on it after the shift, not those at the old numbers) and no further: the lines below keep
what they had, and are compared or tokenized when a row is bound to them or they are
scrolled to (the laziness has a price only once: a jump to the end after such an edit
walks the comparisons down to it). Lines inserted or removed shift the entries after them
(a vector of entries, as the `Document`'s own vector of lines), which are not tokenized
again. The change handler gets the lines whose tokens an edit's pass changed, for their
rows to be coloured again (`tests/unit/highlightCache.cpp`, with a toy language; each
language's file holds seeded random edits through the cache equal to a fresh tokenize,
and the pass stopping at the screen's last line).

**In the editor.** `CodeEditor::setLanguage(name)` (or `setTokenizer`) highlights its
lines: each `CodeLine`'s run is an `internal::CodeLineText` holding the colour spans of its
line's tokens in the skin's `Colors.CodeEditor.Tokens` (one per `TokenKind`, in
`textColorRegistry()` as `CodePlain` to `CodeMeta`, Tokyo Night's in `setThemeColors()`;
the line's own colour follows `CodePlain`), set when the row is bound, when an edit's pass
changed its line's tokens and in `updateColors()` (a theme edit). New spans mark
`StateChange_Text` alone, so the run is re-coloured and not laid out ("Colour spans in a
text run"). `tests/unit/codeEditorHighlight.cpp` holds the rows to their lines' spans,
and `controltest_code_editor` an opened block scalar to the rows on screen below it, with
no line past the screen tokenized; the `code-edit` bench scene, highlighted, opens and
closes a block scalar every ten frames, and `bench_scaling` holds its counters at
N=10 000 to N=1 000.

## Font fallback

Two mechanisms are both called fallback; they answer different questions.

- **The file fallback** (`SlotRenderer::setFallbackFont`, `m_fallbackFont`,
  "A missing face is drawn in a built-in font" above) is about a *face*: a font whose file is
  missing or unreadable is loaded from the embedded OpenSans instead, as a font of its own
  (`Font::Status::LoadedFallback`). It changes which face a whole run is in.
- **The glyph fallback chain** (`SlotRenderer::setGlyphFallback(face, chain)`, the skin
  config's `"fontFallback"`, freetype-gl's `texture_font_t::fallback`) is about a
  *codepoint*: a run stays in its own face, and a codepoint that face has no glyph for is
  drawn from the first face of its chain that has one, in the same run, atlas and draw.

**The chain.** `setGlyphFallback(face, chain)` sets a face's chain, `""` the default one
that every face without its own takes; `glyphFallback(face)` is the chain a face's fonts
use, less the face itself and repeats (so a face's own chain may be empty, and its own
missing glyph shows). The textured skin sets it from `DefaultSkin.json`:
`"fontFallback": {"default": ["DejaVuSansMono"], "<face>": [...]}`, read by
`TexturedSkin::init` before it loads the default font (`IResourceLoader::setGlyphFallback`,
a no-op on renderers without one). The shipped config's default chain is DejaVu Sans Mono
alone, so the arrows, ✓ ✗ ⚠, ★ and box drawing OpenSans lacks are mono glyphs in
proportional text; DejaVu Sans Mono's own chain is empty. The renderer sets no chain by
itself. A chain changed after fonts are loaded loads the fonts whose chain it changed again
with their owners (`reloadFonts`, which `setTextAA` uses too) and raises
`textLayoutSerial()`, so their runs are laid out again on the next frame.

**One atlas, the run's.** Each chain face becomes, for a font that needs it, one more
`texture_font_t` on the **primary** font's atlas (the same `font_manager_t`'s), at the
primary's size and mode: the coverage size and hinting, LCD's `subpixel_positions`, SDF's
`sdf_spread` and base size. A run is still one `TextRun` with one `FontData`, so
`buildTextDrawList`, the backends' text programs and `text_draws` are what they were.
freetype-gl asks the chain through a callback, `texture_font_t::fallback(fallback_data,
index)` (`SlotRenderer::glyphChainFont`, the font's `GlyphChain`), when
`FT_Get_Char_Index` gives 0 for a codepoint that is not 0: chain fonts are tried in order
and each is made only when the walk reaches it, so a font whose codepoints are all its own
opens none. That is why it is a callback and not a list of fonts: making a `texture_font_t`
loads its null glyph into the atlas, which would move a Latin run's atlas bytes. A glyph
taken from a chain font is kept in the primary's `fallback_glyphs` (and so in
`FontData::glyph()`'s table), `texture_glyph_t::font` naming the font it came from. Only
when no chain face has the codepoint does the primary's own missing glyph show. A chain
font starts with an empty cache, no ASCII warm-up, and is deleted with its primary's
`FontData` (`GlyphChain`'s destructor); #552's idle-flush wait for a font no load owns
applies to the primary as before. Each chain face's file is read into memory once and
shared by every font using it (`glyphFallbackBlob`, kept by `weak_ptr` in
`m_glyphFallbackBlobs`, so it goes when the last font holding it does, and is read again
when needed), and loaded from memory (`TEXTURE_FONT_MEMORY`), since a file-backed
freetype-gl font opens its file again on every glyph load. A config that names a chain
face whose file is missing reports it (`Glyph fallback font file not found: <path>`), drops
it and fails `init` ("Missing resources").

**Metrics.** The line height and baseline are the run's face's. In proportional text a
chain glyph moves the pen by its own face's advance, and two glyphs of different faces are
not kerned. That needs no check in the walks: `text_buffer_add_char`, `walkPens` and
`layoutSdfText` look a pair up by the previous codepoint in the glyph's own table
(`texture_glyph_get_kerning`), a font's table holds the pairs of its own glyphs only
(`texture_font_generate_kerning`), and a chain font, its primary's alone, is asked only for
codepoints the faces before it lack, so every codepoint a table names is drawn by the font
that holds it (`texture_glyph_get_kerning_glyph`, which compares the glyphs' fonts, says
so outright for two glyphs). A walk over text with chain glyphs costs what it did. Since
the three walks take the same glyphs, `measureText` and `measureCarets` give the drawn
width with chain glyphs as with any other.

**Default-ignorables draw nothing.** U+200B..200D (zero-width space, non-joiner, joiner),
U+2060 (word joiner), U+FE00..FE0F (variation selectors, as in "⚠️") and U+FEFF have no
advance and no glyph slot in every face and mode: `ftgl::utf8_is_ignorable` is the one
predicate, and `text_buffer_add_char` (coverage and LCD), `walkPens` (their measures and
carets) and `layoutSdfText` (SDF) skip such a codepoint before any glyph lookup. It still
takes a caret position (a codepoint boundary), at the same x as the one before it, and it
is the `previous` of the next codepoint, so no kerning pair spans it.

**Terminal cells in a fixed-width face.** A run whose face is `FT_IS_FIXED_WIDTH`
(`texture_font_t::fixed_width`, DejaVu Sans Mono) lays every codepoint in whole cells, as a
terminal's `wcwidth` does, so tables CLI tools pad line up: `gweni::text::cellCount(cp)`
(`include/gweni/text/cells.h`) is 2 for East_Asian_Width Wide and Fullwidth (with the
blocks the UCD defaults to W), 0 for combining marks (Mn, Me) and the ignorables above, 1
for everything else, Ambiguous included. It goes by codepoint whichever face draws the
glyph: ⚡ U+26A1 and ☔ U+2614 take 2 cells though DejaVu Sans Mono draws them one cell
wide. Its range tables (`source/text/cellWidthTable.inc`) are generated by
`tools/gen_cell_widths.py` from the UCD's `EastAsianWidth.txt` and
`DerivedGeneralCategory.txt`, at the Unicode version `cellWidthUnicodeVersion()` names
(18.0.0); the script downloads them or reads `--ucd DIR`, and regenerating an unchanged
version writes the same bytes. `loadFont` sets a fixed-width font's
`texture_font_t::cell_width` to its advance for `0` as that font loads it (a whole pixel
when hinted, the base size's unhinted one in an SDF font, scaled with the run) and
`cells` to `cellCount`. Then `texture_font_cell_place` places each glyph: the pen moves by
`cells*cell_width`, and the glyph's advance box is centred in its cells (a zero-cell
glyph with an advance over the cell before the pen, one without at the pen, so a
combining mark is drawn over its letter), the offset a whole pixel in a hinted font
without subpixel positions, and quantised with the pen to 1/3 px in LCD. A font with cells
is not kerned. `text_buffer_add_char`, `walkPens` and `layoutSdfText` all go through it, so
every caret is a whole number of cells. A font that is not fixed-width has `cell_width` 0
and is laid out by its own advances.

`tests/glyphFallbackChain.cpp` pins the freetype-gl patch (the callback, the borrowed
glyphs' cache and owner, no kerning across fonts, every table's pairs of one font),
`tests/glyphFallbackText.cpp` the chain in `SlotRenderer` in grayscale, LCD and SDF
(Latin text reading no chain file and
drawing as with no chain, a chain glyph being the chain face's image in one run and one
draw, the chain's order, the measures, the shared file, a chain set late),
`tests/skinFonts.cpp` the skin's `"monoFont"` and `"fontFallback"`,
`tests/ignorableText.cpp` the ignorables in every mode, and `tests/monoCells.cpp`
`cellCount`'s table and a box-drawing table in DejaVu Sans Mono at 8 to 24 px (coverage
and SDF) and scale 1, 1.5 and 2, its carets whole cells and its three rules unbroken and
in the same device px in every row, on GL and Vulkan. UnicodeTextDemo shows a symbols line
through the chain, a log table in the mono face and the CJK line, which no shipped face
has, still as missing-glyph boxes.

## The zoom panel

`controls::ZoomPanel` (`include/gweni/controls/zoomPanel.h`) shows its subtree at a
continuous zoom and a pan: a content point `p` is drawn at the panel's origin plus
`pan() + p*zoom()` canvas units. Its children go into an inner panel
(`getInnerPanel()`, owner-placed at the panel's top left, `contentSize()` units large:
`setContentSize`, or the panel's own size on an axis given a negative one), which lays
out in its own units as always: a zoom or a pan only moves the content on screen. The
zoom is clamped to `setZoomRange` (0.25..4 by default). Everything inside draws,
clips, hit-tests and sizes its text as it would on a canvas at
`Canvas::setScale(canvasScale*zoom)`: ButtonDemo in a panel filling a scale-1 canvas
at 2, 1.5 and 1.37 draws the very pixels ButtonDemo draws on a canvas at that scale,
and every device px hits the same control (`controltest_zoom_panel`).

- **The transform stack.** `renderer::Base` draws through a `renderer::RenderTransform`
  (`baseRender.h`): the device px the space's origin is at, a whole px
  (`originX/Y`) and a fraction in [0, 1) kept apart (`fractionX/Y`), its scale in
  device px per unit (the canvas's times every zoom the space is in), and an optional
  device-px clip (`bounded`, `deviceClip`). `setScale()` resets it to the identity at
  that scale and `scale()` is the transform's, so `translate()` (a rect's edges
  snapped as `deviceX(v) = originX + floor(fractionX + v*scale + 0.5)`), the clip
  boxes (`makeClipBox`, cut to `deviceClip` when bounded), the SDF radius and border,
  `drawLine`'s thickness and stroke start, the icon strokes and `SlotRenderer::textPx`
  all work at the effective scale, and with no fraction a space at a whole px draws
  as one at 0 does, shifted. The render drain (`Base::drainRenderQueue`) carries a
  `RenderState` (offset, clip region, transform) on both of its paths, the whole-tree
  walk and the sorted queue that rebuilds each entry's state down its `m_actualParent`
  chain: at a control that `transformsChildren()` (`innerRenderState` →
  `enterChildSpace`) the children's state becomes offset 0, a clip region that is as
  good as none (`SpaceClipReach`), and the transform the control makes
  (`childRenderTransform`: for a ZoomPanel its origin on the whole device px
  `translate()` put it at, plus the pan in device px, at the outer scale times the
  zoom) with, as its device clip, the region it was entered from in device px (cut
  to any outer one). After the drain the renderer is back at the canvas's own
  transform. `childRegionChanged()` answers yes above a transforming parent, so a
  resized control under a panel re-stamps its children's clip boxes.
- **The input mapping.** Two hooks on `Base`, `toChildSpace`/`fromChildSpace`, map a
  transforming control's own units to the space its children are placed in and back
  (identity by default; a ZoomPanel's `(p - pan)/zoom` and `pan + c*zoom`).
  `getControlAtWalk` maps the point through them before it searches the children,
  taking the unit the point's unit starts in (floor), as the input layer turns a
  device px into the canvas unit `floor(px/scale)`, so a hit lands where it does on a
  `setScale` canvas. `localPosToCanvasF`/`canvasPosToLocalF` walk the actual-parent
  chain through them (nested panels multiply); `localPosToCanvas` rounds to the
  nearest unit and `canvasPosToLocal` floors. A new zoom or pan calls
  `Canvas::hoverMayHaveMoved`, so the control under a resting mouse is hovered.
  A `Dragger` (a scroll bar or slider thumb, a splitter, a window's title bar) and a
  `Resizer` move their target by the mouse taken into the units of the control the
  target is placed in (its actual parent: the inner panel for a control added to
  the ZoomPanel itself, as a window is; `canvasPosToLocal` of the mouse, less the
  hold in the target's own units), so
  the grabbed point stays under the cursor at any zoom (at zoom 1 the same integer
  arithmetic as before). A DragNumber's scrub stays in canvas units (a value step per
  screen unit).
- **On-screen rects.** `Base::getCanvasBounds()` is a control's rect as it is drawn,
  its size zoomed too, each edge rounded to a canvas unit. The tooltip box
  (`ToolTip::setContentFor`/`placeFor`), `Popup::anchorRect`, a Menu's anchor
  (`Menu::canvasPosition`, `followAnchor`), a ComboBox's list (below the box, as
  wide) and the inspector's highlight are placed from it. They are canvas children,
  so they draw at canvas scale, not zoomed. A popup or menu anchored in a panel
  follows its anchor, so with one open a zoom or pan frame lays it out again.
- **The wheel and the pan.** The wheel bubbles up from the hovered control
  (`Base::onMouseWheeled`), so a ScrollControl under the cursor that can scroll takes
  it; otherwise `ZoomPanel::onMouseWheeled` zooms by `WheelStep` (1.1) a notch about
  the cursor (`zoomAt(zoom, at)`: the content point under `at` stays there), and takes
  the wheel at the range's ends too. A press goes first to `Base::pressGesture` of the
  hovered control and its ancestors (`Canvas::inputMouseButton`, on a press with no
  button down and no mouse capture): a ZoomPanel takes a middle or right press, or a
  left one with Space held, with its `PanGesture`, an `InputCapture` that has the
  canvas's input until the release, so the hover and the focus stay. Once the mouse
  is `PanThreshold` (3) units from the press the content follows it; let go before,
  the press and the release are given to the canvas as they would have been, so a
  right click opens the control's context menu and a click presses. The press that
  starts a pan closes the open menus and the popup as any press there does
  (`closeMenus`, `popupPressed`). The horizontal wheel, and so Shift+wheel (which the
  canvas makes horizontal), zooms nothing: the panel takes it (`onMouseWheeledH`) so
  nothing above it scrolls sideways instead, and a pan's capture drops the horizontal
  wheel. A capture gets the wheel's axis before the Shift mapping, so a vertical
  notch with Shift held zooms mid-pan as one without it does. A wheel notch during
  a pan zooms about the cursor
  and the drag goes on from there (the gesture takes the mouse and the pan anew),
  so the content point under the cursor stays under it. The release moves the
  canvas to where the mouse is; keys during a pan go to the capture. A disabled
  panel neither zooms nor pans, and `setZoomRange` keeps its low end at
  `MinZoomLimit` (1/1024) or above.
- **Known limits.** Space is a key like any other before it is a pan's modifier: held
  over a focused TextBox it types a space, over a focused Button it presses it. A
  ComboBox's list and a context menu are placed once, when they open, so a wheel
  zoom over the panel with one open leaves it where it was (scrolling a
  ScrollControl under an open list does the same); a pan's press closes them.
- **Cost.** A zoom marks the inner panel's subtree `StateChange_Geometry |
  StateChange_Text`, a pan `StateChange_Geometry`, and neither invalidates: the frame
  runs no layout pass, calls `invalidate()` nowhere and renders the subtree alone,
  every control re-emitting its slots at the new transform (its clip boxes re-stamped)
  and every text run re-laid at the new px on a zoom (`renderText`'s run cache keys
  on px), so it writes the subtree's slots and nothing outside it, and the next frame
  writes nothing. A zoom inside the SDF range uploads nothing to an atlas; one that
  crosses coverage sizes rasterises the whole-px size it reaches (unless it was
  drawn in the last second), and the size it left goes a second later (see
  "Coverage sizes are whole device px"). So a frame is O(the panel's
  subtree), not O(the change): a pan moves every slot on the CPU, which a backend
  per-region transform would make free (not done). `tests/unit/zoomPanelRender.cpp`,
  `zoomPanelInput.cpp`, `zoomPanelGestures.cpp`, `tests/zoomTextAtlases.cpp` and
  `--scenario zoom-panel` pin it; ZoomPanelDemo (in ControlTest's `--list`, not in the
  shell) holds a Button with a tooltip, a ComboBox, a DragNumber, a TextBox and a
  ScrollControl at 1.25x.

## The node graph

`controls::NodeGraph` (`include/gweni/controls/nodeGraph.h`) is a node editor's view:
a ZoomPanel holding `Node`s at positions in graph units, `Pin`s on their sides and
`Link`s between pins, over a grid. The app owns the graph's model (types, values,
evaluation, cycles); the widget holds view objects only, each with the app's id and
pointer (`setUserId`, `setUserData`), and nothing in it knows what a pin carries.

- **The pieces.** A graph point is the ZoomPanel's content point: it is drawn at the
  panel's origin plus `pan() + p*zoom()`. The inner panel is placed `GraphExtent`
  (65536) units up and left of the content's origin and is twice that each way, so
  nodes may sit at negative points (`Node::setGraphPosition`, a request on the node,
  `GraphExtent` added). It holds three owner-placed layers, each its whole size, in
  this order of z: the **link layer** (every `Link`, and the marquee's fill first),
  the **node layer** (every `Node`) and the **overlay layer** (the preview of a link
  being dragged out, and the marquee's outline). A `Node` is a title bar
  (`NodeTitleBar`, its title Label, the theme's `NodeTitle` or `setTitleColor`), a
  pin row (two `NodePinColumn`s, inputs down the left and outputs down the right)
  and a body (`getBody()`, which `newChild()` fills with any controls, which work at
  any zoom: the ZoomPanel's job). Its frame, the box its renderer draws
  (`NodeBody`, `NodeSelected` while selected, the title joined to it with the edge
  mask), is its bounds less `PinOverhang` each side, so a pin's marker centred on the
  frame's edge is inside the node: drawn and hit with no clip cutting it. A `Pin` is
  a marker (`PinShape::Circle`, `Square` or `Triangle`, drawIcon kinds 7 and 8,
  filled while connected, a ring otherwise, in `PinIn`/`PinOut` or the app's
  colour) and a name; `getAnchor()` is the marker's centre in graph units. The grid
  is the graph's own drawing, under everything: a backdrop slot (`GraphGrid`) and one
  `drawGrid` slot (style 8), a line every `gridStep()` graph units
  (`GridSpacing` times the least power of `GridMajorEvery` that keeps the lines
  `MinGridGap` apart on screen), every `GridMajorEvery`-th in `GraphGridMajor`, each
  one unit thick on screen at any zoom. One slot, taken when the graph is made,
  however many lines there are: a slot per line was the first way, and lines whose
  slots a zoom took after the nodes' drew after them, losing to the depth the
  nodes' anti-aliased corners wrote (`--scenario hide-show` found it).
- **The link segment model.** A `Link` joins two pins as a polyline
  (`Link::polyline()`, kept until an end, the style or the zoom moves): `Straight`,
  one segment; `Bezier`, a cubic leaving and entering each pin along its side's way
  out, `LinkCurveMin`..`LinkCurveMax` (16..24) segments, one per `LinkCurveStep` (16)
  canvas units of its control polygon on screen; `Orthogonal`, 3 runs between facing
  pins with room, 5 otherwise. It covers its layer and is never laid out: a node
  that moves (its bounds, its pin row's or a pin's, `onBoundsChanged`) marks its
  links, which draw again. The textured renderer follows PlotLines': one `drawLine`
  slot per segment at the link's z, each drawn with its neighbours, so its joints
  are blended once, and only a segment whose endpoints, neighbours, thickness,
  colour or z changed is written. A link takes the most slots its style can draw
  when it is first drawn (24, 5 or 1) and hides the ones it does not use, so a zoom
  that picks another segment count takes no slot after the nodes'.
- **CPU clipping.** A quad with a corner past ±32 K device px is culled whole
  (`cullOutOfRangeQuad`), so a link to a node far off screen would vanish. Before
  `drawLine`, each segment is cut (Liang-Barsky) to the graph's view in graph units,
  a few units wider than the stroke, and a segment wholly outside is hidden; a cut
  joint is drawn without that neighbour. A link to a node 40 000 device px away
  draws its visible part (`tests/unit/nodeGraph.cpp`).
- **Input.** A left press on a title bar drags the selection: an unselected node
  becomes the selection's one node (Shift adds it, Ctrl toggles it), the pressed node
  is brought to the front (its subtree alone takes new z), and every selected node
  moves by the mouse's motion in whole graph units (`setGraphPosition`); a plain click
  on a node of a selection makes it the selection. A left press on a pin is a link
  drag (`Pin::pressGesture`, an `InputCapture`): its preview, a `Link` with a free end
  in the overlay layer, follows the mouse, the wheel still zooms and Escape ends it. A
  left press on empty space (no node, link or pin under it, Space not held) is the
  marquee (`NodeGraph::pressGesture`, another `InputCapture`): from `PanThreshold`
  units on, every move selects the nodes whose frames and the links whose curves
  (`Link::touches`) the rect touches, with the earlier selection kept under Ctrl or
  Shift; a release that never got that far is a click, which clears the selection
  (not under Ctrl or Shift); Escape gives the earlier selection back. Its outline
  (four `drawLine`s, a canvas unit thick) is in the overlay layer and its
  translucent fill in the link layer, under the nodes: text is drawn after every
  slot, and a translucent box over it would hide it (as the stats overlay does). A
  link is hovered within `Link::HoverDistance` (4) canvas units on screen of its
  curve (`Link::getControlAt`, `distanceTo`) and drawn in `LinkHover`, measured
  from the exact graph point of the tested canvas unit's centre, which
  `NodeGraph::getControlAt` keeps while the hit-test walks (the walk hands its
  children whole graph units, at zoom 4 four canvas units each); a hovered
  pin's marker is a unit larger and lighter. A press focuses the graph (unless a
  control in a body takes the keys), and Delete removes the selection. A right click
  opens the context menu (`setContextMenu`) of the link, node or graph under it, the
  ZoomPanel giving an unmoved right press back as a click; every right press sets
  `getContextMenuPosition()`, and `openContextMenuAt()` opens the graph's at a point.
- **Events and vetoes.** A user's edit is made unless a request handler vetoes it,
  and a done event fires after; what the app does from code fires nothing.
  `onNodesMoved` fires once, on a drag's release, with `getMovedNodes()`.
  A link dropped on a pin it may end on (`canLink`: an Input and an Output of two
  nodes, or `setLinkFilter(from, to)`, the Output first) fires `onLinkRequested`
  (`getRequestedFrom/To`, `vetoLink()`), then `onLinkCreated` with the link; one
  dropped where no node is fires `onLinkDroppedOnEmpty` (`info.point`,
  `getDropPosition`, `getDropPin`) and makes nothing. Delete (`removeSelection`)
  fires `onNodeRemoveRequested` per selected node and `onLinkRemoveRequested` per
  selected link left (`vetoRemove()`), then `onNodeRemoved`, and `onLinkRemoved`
  for each link that went, a node's with it. `onSelectionChanged` fires once per
  user action that changed the selection. Removing a node or pin takes its links,
  and every primitive id comes back.
- **The z layers and their budget.** Links are numbered before every node and the
  overlay after them, so a node is always over every link. Each control takes its
  `depthLayers()` z values and the canvas spreads them with `Canvas::ZGap` gaps, then
  compacts the gaps as the tree grows: a node of a title, two pins and a Label body
  with its link is 17 controls, about 18 z values, so about 3 600 such nodes fill the
  0..65535 range. Past that the labels clamp and tie, and the graph no longer draws
  in order: hundreds of nodes, the target, fit easily, and `bench_scaling` checks the
  graph scenes at 3 000 rather than 10 000 (`LARGE_N_FOR` in `bench/scaling.cmake`).
- **Cost.** A drag frame of a two-node selection in NodeGraphDemo writes 35
  primitive slots and its 15 runs' glyphs, all the moved nodes' and their links',
  and lays out in one pass entering 28 controls; a hover writes the hovered item's
  slots and the left one's (1 for a pin, 17 for a Bezier link); a marquee frame its
  5 slots and those of the items it (de)selects (`--scenario node-graph`, which
  checks each by id). `graph-drag` and `graph-marquee` are `scaling=local` and do
  the same work at any N; `graph-pan` and `graph-zoom` write every node's slots
  with no layout pass, as a ZoomPanel's pan does. Not local, by the v1 decision of no
  spatial index: a hit-test where no node is under the mouse (a hover or a release
  over empty space) visits every node and link, a marquee move tests every node's
  frame and link's curve on the CPU (with nothing allocated), and every node's
  slots are drawn every frame. The style 8 and icon kind 7/8 branches cost llvmpipe
  about 3 % of a primitive-heavy frame's rasterising, for pixels that draw neither
  (it runs short branches as selects). `tests/unit/nodeGraph.cpp` and `tests/grid.cpp`
  pin the pieces; NodeGraphDemo (ControlTest's `--list` and the README gallery) is
  a small shader-like graph with every link style, pin shape and context menu.

## The inspector

`gweni::tools::Inspector` (`include/gweni/tools/inspector.h`, `source/tools/inspector.cpp`)
is the library's live view of the change tracking: a `WindowControl` an app opens on its
canvas (`Inspector::open`/`close`/`toggle`; the library binds no key, the examples bind
F12). It shows the control tree, the selected control's state and the counters of the
last frame, and over a settled UI it shows 0 slots written, which is true: the frames
after it write nothing, its own slots included (`controltest_inspector`).

**What it shows.** The tree is a `TreeControl` of `InspectorNode`s, one per control
(type, name, id), each node's children made only when it is first opened (a `...`
placeholder shows the toggle until then), so a tree of thousands of controls costs the
rows opened. It is brought in line with the canvas when `Canvas::treeSerial()` moves,
which every tree-order change Base reports raises (what makes the z renumber run) unless
it happened inside an excluded subtree, so its own nodes do not move it: a level whose
targets differ is made again, the nodes that were open opened again. The selected
control's lines are its type, bounds (logical and on the canvas), z and `depthLayers()`,
overlay z and `overlayLayers()`, its primitive and text ids (`skin::Control::appendIds`,
which a Control taking text ids overrides: the textured `Text`), its pending
`StateChange_*` flags by name, and hidden/visible/disabled/clips. Selecting a control
whose node was never made opens the nodes down to it. The counters are
`Canvas::previousFrameStats()`, the last whole frame's `LayoutStats`, `UpdateStats` and
the `ChangeStats` its render phase counted, kept as the next `updateCanvas()` starts,
and the renderer's slots written and uploaded (`renderer::Base::frameStats()`, "not
counted" by default). An `InspectorHighlight` frames the hovered node's control, else
the selected one: four edge slots (a style 4 rect with a transparent fill would write
depth over what it frames), an overlay on the canvas (`Canvas::addOverlay`), moved, and
so drawn, only when its target's rect or visibility changes.

**Its own work is not the app's.** It refreshes once a frame, at the start of the update
phase, from an animation of its own, reading the frame before, and sets a label only
when its string changes. Two things keep what it shows the app's alone:
- Its window and its highlight box are `Base::setExcludedFromStats()`. Nothing under a
  flagged root counts in `ChangeStats`, the canvas's `LayoutStats` and `UpdateStats`,
  `measureTextCalls()` or `animsRun`. Neither do the entries of app controls a pass or a
  renumber enters only on the way to a flagged subtree's change: an invalidation or a z
  mark started inside one sets `m_updateLayout`/`m_zDirty` up the chain without
  `m_layoutDirtyCounted`/`m_zDirtyCounted` (`Base::layoutEntryCounted()`), and a pass
  with no counted entry is not counted at all. While no control is flagged none of this
  is looked up (`statsExcluded()` is O(1)), and every count is as it was.
  `tests/unit/statsExclusion.cpp` runs two canvases with the same app tree, one with a
  changing flagged panel on the canvas or inside an app panel, and requires every count
  to be the same on both, frame by frame.
- The slots are counted by the renderer, in `SlotRenderer::flush()`, by comparing slot
  bytes, so no snapshot around its subtree in the walk could separate them. The
  renderer reports which ids it wrote instead (`writtenPrimitiveIds()`, and
  `writtenTextIds()` per text id with its glyph slots, a glyph slot under the text id
  whose run last held it), and the inspector takes out every tool's ids through the
  canvas's one record of them, `Canvas::excludedSlots()` (below). Uploads cannot be told
  apart by id: a frame in which only the tools wrote shows 0 uploads, one in which the
  app wrote shows the frame's whole uploads.

**One record of the tools' slots, per canvas.** With the Inspector and a `StatsOverlay`
both open, a tool that left out only its own ids would count the other's as the app's,
and neither would ever read 0. So the canvas keeps the record (`controls::ExcludedSlots`,
`include/gweni/controls/canvas.h`) and both tools read it through
`tools::appFrameWrites()` (`include/gweni/tools/toolSupport.h`). It is a **snapshot of
who wrote the last flush**, taken as `renderCanvas()` ends, after the renderer's `end()`
(`Canvas::snapshotExcludedSlots()`, `WindowCanvas`'s render too):
- **held**: the ids every `setExcludedFromStats()` subtree on the canvas holds at that
  flush (`Base::statsExcludedRoots()`, each flagged root's subtree walked through
  `skin::Control::appendIds`), sorted, without repeats;
- **gone**: of the held ids of the flush before, those no longer live
  (`isPrimitiveIdLive`, `isTextIdLive`): the tools' slots this flush cleared, however the
  id was given back (a control of a flagged subtree deleted, its skin Control replaced, or
  a skin Control shrinking its own slot list, as a plot's does);
- the live id counts at the flush, so `appPrimitives()`/`appTexts()` are the app's live
  ids then. These two counter reads are made at every flush, a tool on the canvas or not,
  so an overlay opened over a settled app shows the app's ids on its first refresh, which
  runs before any flush has held the overlay.

A slot written in frame N is a tool's iff its id is in held or gone of flush N; every
other slot is the app's. An id a tool gave back that the app took again before the flush
(the id bank hands a freed id out again at once, lowest first, and the Inspector deletes
nodes exactly when the app's tree changes) is live and not held, so it is the app's; a
control a tool owns but has not flagged (a popup it made) is the app's, since only flagged
subtrees are held. The record is not touched between flushes: a flag or tree change counts
from the next flush, the record reads the same however often and in whatever order the
tools read it, and the frame-N writes of a control moved into a tool (or the box flagged)
in update N+1 stay the app's. A tool closed whole still owns the slots the next flush
clears: the snapshot is taken while a flagged root is on the canvas or the last snapshot
held or cleared anything, and with no flagged control on any canvas nothing is walked,
no id's liveness is checked and every counter is as it was. One edge counts the other way: an id a tool gave
back that the app took and gave back again before the same flush is in gone, so that
slot's clear is the tool's.

The overlay's `prim ids` and `text ids` rows are the snapshot's app counts (the live ids
less the held ones, both at the last flush), so they read the same with the Inspector
open as without, and follow a deletion one frame later, which a 250 ms refresh hides. The
canvas's ToolTip box is one of the flagged subtrees while it shows, or last showed, a
tool's tooltip ("The stats overlay"), so a name's tooltip is left out by every tool.
`tests/unit/excludedSlots.cpp` pins the record: a released id kept to the frame after the
flush that cleared it, an id the app takes again, a skin Control's own release, an app
control deleted in a tool's teardown, a tool closed whole, a flag or tree change taking
effect at the next flush, two tools reading one frame and the box as every tool's; and
`--scenario stats-overlay` holds both tools open over the settled Demo ("The stats
overlay").

**Open, close, toggle.** `open()` makes the canvas's inspector and its tree at once, so
nodes can be opened and selected straight away; its counter lines are first set by its
first refresh, in the next update phase. `open()` is usually called from a key callback,
between two frames, where `previousFrameStats()` is still frame N-2 while the renderer's
writes are frame N-1's, so a refresh there would show two frames as one. `close()` and
the window's close button hide it and `delayedDelete()` it; from then on `find()` no
longer finds it (`Canvas::isPendingDelete()`, a lookup in the canvas's delayed-delete
set), and `toggle()` is `close()` when `find()` finds one, else `open()`, so a second
toggle in the same frame, or one after the close button, makes a new, shown inspector
instead of showing the doomed one again (#502). The ThemeEditor and the StatsOverlay
`find()` and `toggle()` the same way (`tests/unit/toolToggle.cpp`).

**On a `WindowCanvas` it shows nothing.** The counters are `previousFrameStats()` and
the times `CanvasFrameStats::times`, which `Canvas::updateCanvas()` and
`Canvas::renderCanvas()` record. A `WindowCanvas` runs `Canvas::doThink()` and its own
render instead, so neither is ever filled: there the Inspector's counter lines and the
overlay read `-` and 0. Nothing in the repository uses a `WindowCanvas`.

After an app change it converges in two frames: frame k writes the app's slots; frame
k+1 shows them and writes its own labels' (taken out); frame k+2 shows 0 again and
writes those; frame k+3 changes no string and writes nothing.
`tests/unit/inspector.cpp` pins the tree, the details, the settling and the ids given
back on close; `--scenario inspector` does the same over the whole Demo.

## The theme file and strict mode

The textured skin's colours are compiled in (Tokyo Night): the SDF surface fills are
mutable globals in `tokyoNight.h`, named in `tokyonight::surfaceRegistry()`, and the
text colours are set by `TexturedSkin::setThemeColors()`. An optional `"theme"` object
in `DefaultSkin.json` overrides them without recompiling: `"surfaces"` maps a surface
name to any of `top`, `bottom`, `border` (`[r,g,b]` or `[r,g,b,a]`), `radius` and
`borderWidth`, and `"textColors"` maps a name in `TexturedSkin::textColorRegistry()`
(`LabelDefault`, `ButtonNormal`, ... 15 of them; the map points into that skin's
`Colors`) to a colour. `"metrics"` maps a name in `skin::Base::Metrics` to an
integer in logical units: `ScrollBarThickness`, 6-64 (Tokyo Night's compiled value is
12, any other skin's 15), a scroll bar's thickness and the side of its square arrow
buttons, which the bar reads from its skin each time it lays out. A value that is not
valid is reported on one stderr line starting `theme ` and left as it was; the rest
still applies. Anything the file omits keeps its compiled default.

**Strict mode** is for writing a theme: with `"strict": true` in the `"theme"` object,
`loadTheme` first paints every registered surface missing
(`tokyonight::setAllSurfacesMissing()`: `top`/`bottom` `colors::Missing`, `border`
`colors::MissingDark`) and sets every `textColorRegistry()` colour to `colors::Missing`,
then applies the file as usual. So everything the theme has not said yet is magenta,
field by field: a surface entry that gives only `top` and `bottom` keeps a magenta
border. `radius`, `borderWidth` and the `"metrics"` sizes are not colours: strict mode
leaves them at their compiled values or the file's. After
applying it prints one line per kind, names sorted, a surface with none of its colours
set by its bare name and one with some of them as `Name.field` per missing field (here
the shipped theme with `"strict": true` added):

```
theme strict: 191 of 195 surfaces not set: Badge, ButtonDisabled, ButtonDown, ...
theme strict: 68 of 70 text colors not set: AxisTagText, BadgeText, ButtonDisabled, ...
```

(`theme strict: all 195 surfaces set` for a kind with no gaps, and a single
`theme strict: all 195 surfaces and 70 text colors set` when there are none at all). A
rejected value counts as not set. A surface that is transparent at rest on purpose
(`WindowButton`, `SelectableHover`: a transparent fill hides the slot) is magenta too
until the theme sets it to `[0,0,0,0]`; the theme has not said what it looks like. The
gaps are deliberate, so they are not a failure: `init` stays `true` and a capture exits
0. A `"strict"` that is not a boolean is reported and taken as `false`. The shipped
`DefaultSkin.json` does not set it, and without it nothing changes. Like every theme
value, it paints the process-global surfaces, which a later `init()` does not reset.

## The theme editor

`gweni::tools::ThemeEditor` (`include/gweni/tools/themeEditor.h`,
`source/tools/themeEditor.cpp`) edits the textured skin's theme live, as imgui's style
editor does: a `WindowControl` an app opens on its canvas (`ThemeEditor::open`/`close`/
`toggle`; the library binds no key, the examples bind F11 and save to and load from
`gweni-theme.json` in the current directory). Its body is a `PropertyTree` of
`ThemeEditorNode`s, one per `tokyonight::surfaceRegistry()` surface sorted by name, then
one for the skin's text colours (`TexturedSkin::textColorRegistry()`). A node's rows are
made when it is first opened, so the closed tree is one row per node: a surface's `top`,
`bottom` and `border` (`ThemeColorProperty`, a `ColorEdit` in hex mode with alpha) and
its `radius` and `borderWidth` (`ThemeNumberProperty`, a `DragNumber`, 0-32 in tenths),
and one `ThemeColorProperty` per text colour. Above them are Revert, Load and Save, and
under them a status line saying what the last of those did. With any other skin the
tree is empty and the status line says there is nothing to edit.

**An edit applies at once, and once.** A row's change writes the value (the surface's
field, a process global; the text colour, in the skin's `Colors`) and calls
`ThemeEditor::applyToCanvas()` on the editor's canvas, which:
- gives every `internal::Text` that follows a skin colour its new value
  (`Text::refreshSkinTextColor()`);
- calls every control's `updateColors()`, so a control that picks its text colour for
  its state (a Button, a Selectable, a TextBox's placeholder, the ToolTip box, a
  Slider's value text) reads the skin again;
- marks the whole tree `StateChange_Background|StateChange_Text` once, so every
  renderer paints its surface again on the next frame, and the frame after that writes
  nothing. With a changed metric (a Load or Revert of `ScrollBarThickness`) it lays the
  whole tree out again too.

It runs once per edit, never on a frame without one: a drag on a radius edits once a
frame it moves the value, and a settled editor writes nothing. An edit that sets the
value a field already has redraws nothing. The surfaces are shared by every textured
skin in the process, but only the editor's canvas is redrawn: another canvas shows the
new values as its controls redraw, or at once if the app calls `applyToCanvas()` on it.

**Which Text follows which colour.** A text colour is copied into the control that shows
it, so a Label cannot tell the skin's default colour from one the app gave it. Each
`internal::Text` records it (`internal::SkinTextColor`): a new one follows
`Colors.Label.Default`, `Label::makeColorNormal/Bright/Dark/Highlight` and
`Label::setSkinTextColor` make it follow that colour, a TextBox's text the label colour
and its placeholder `PlaceholderText`, and any `setTextColor()` gives it a colour of its
own, which it keeps. A control that picks its colour in `updateColors()` needs no
record. So a theme change recolours what the skin coloured and nothing the app did.

**Revert, Save and Load.** The editor keeps the values it was opened with
(`TexturedSkin::captureTheme()`, a `ThemeValues` of every surface, text colour and
metric by name), and Revert gives them all back (`applyTheme()`), shows them in the made
rows, and redraws. Save is `TexturedSkin::saveThemeFile(getThemePath())`: it writes the
**whole** theme (`themeJson()`), not what differs from the compiled defaults, so loading
it sets every value whatever was set before. Surfaces, text colours and metrics each
sorted by name, one a line, every value as rapidjson's `Writer` writes it (a colour
`[r,g,b]`, `[r,g,b,a]` if not opaque, a whole number as an integer, any other float as
the shortest double that reads back as the same float), so the same theme always gives
the same bytes. Where it goes depends on what is at the path, found with rapidjson's
SAX `Reader` and the stream's offsets:
- a skin config with a `"theme"` object: that object is replaced, its lines indented as
  its `{` is, and every other byte of the file is kept (CRLF files stay CRLF);
- a skin config without one: a `"theme"` member is added after its last member;
- nothing, or a standalone theme file (an object of only `surfaces`, `textColors`,
  `metrics`, `strict` and `_` comments): the theme is the whole file.

The theme's `"_comment"` is kept. A file that does not parse, or whose `"theme"` is not
an object, is left as it was and the save fails with one stderr line. Load is
`TexturedSkin::loadThemeFile()`: a skin config's `"theme"` or a standalone theme, applied
by `loadTheme()`, so what the file does not set keeps its current value; it shows the
loaded values in the rows and redraws if anything changed.

`tests/unit/themeEditor.cpp` pins the nodes and rows, an edit redrawing once, Revert,
Save and Load round trips and the file cases. `--scenario theme` (`controltest_theme`)
changes several surfaces, text colours and the scroll bar thickness under the settled
Demo and applies them live: the frame after the one drawing them writes nothing, and
the pixels are a Demo built under that theme on a new canvas, byte for byte. Then it
saves the theme, gives the defaults back live (the first Demo's pixels again) and loads
the file (the changed values and pixels again). ThemeEditorDemo shows it beside a few
controls, ButtonNormal's top and bottom edited; the first ThemeEditorDemo alive keeps the
theme it found and the last one to go gives it back, so the demos after it draw as
before.

## The stats overlay

`gweni::tools::StatsOverlay` (`include/gweni/tools/statsOverlay.h`,
`source/tools/statsOverlay.cpp`) is the third library tool: a compact, read-only panel
in the canvas's bottom-right corner that shows, live, what the frames cost, so the claim
that a frame costs what changed can be watched holding: hover a row and `rendered` and
`prim slots` blip and drop back to 0 while the frame time stays flat. An app opens it
(`StatsOverlay::open`/`close`/`toggle`/`find`, the Inspector's shape; the library binds
no key, the examples bind F10 and take `--stats-overlay [on|off]`), and the hosted
`demo.html` opens it by default.

**What it shows.** Rows of Labels, each value in a fixed-width right-aligned cell, each
row and column name with a tooltip saying what it shows:
- a time table under a `time (ms)` corner (drawn in the normal text colour, the column
  and row names dim), min, avg and max in ms to one decimal, of `interval` (one
  `updateCanvas()` start to the next), `cpu` (update + render, the flush included),
  `update`, `render` without the flush and `flush`;
- `fps`, the frames of the last second of the canvas's clock that it saw move on, and
  `controls`, the app's controls on the canvas (the tools' excluded subtrees left out);
- `prim ids` and `text ids`, the live primitive and text ids less those every tool on the
  canvas holds, both as the last flush ended (`Canvas::excludedSlots()`, an open
  Inspector's too);
- under "most in one frame", the largest value one frame of the window had of `rendered`
  (`ChangeStats::visited`) and `changed`, layout `passes` and `entered`
  (`LayoutStats::visited`), `z visited`, `anims` run, the app's `prim slots` and
  `glyphs` written, `upload` and `draws` (`renderer::FrameCounts::drawCalls`); the
  upload value carries its unit (`StatsOverlay::bytes()`: bytes below 1000, then kB, MB,
  ... of 1000 with one decimal below 10, truncated as the counts are, so `512B`, `1.2kB`,
  `12kB`, `1.0MB`, never more than 5 characters); the slot and upload rows read `-` with
  a renderer that does not count them;
- two `PlotLines` of the last 60 refreshes (15 s): `cpu ms`, the most cpu time one frame
  of each refresh window took, and `slots / frame`, the most primitive and glyph slots
  one frame of it wrote. A point is its window's **max**, so a one-frame spike shows.

**The windows.** An animation of its own (`tools::RefreshAnimation`, shared with the
Inspector through `include/gweni/tools/toolSupport.h`) runs at the start of every update
phase and gathers the frame before (`Canvas::previousFrameStats()`, whose `times` are
that frame's, and the renderer's `frameStats()`) into the current window. Every
`RefreshSeconds` (250 ms) of the canvas's clock (`platform::getTimeInSeconds()`, wall
time in a window: `steady_clock` from the first read on Linux and Windows (where GWEN's
clock let each read advance at most 0.1 s, so a loop that slept to a deadline woke short
of it), and on Linux it used to be
`clock()`, the process's CPU time over all its threads, and a window rasterising on
llvmpipe's threads read fps 6 at 130 frames a second; `tests/unit/platformClock.cpp`) it
shows that window and starts the next: the time table and the "most in one frame" rows
cover the frames since the last refresh, the plots the last 60 refreshes, fps the last
second. A clock that goes back (a capture installing its stepped clock after the overlay
opened on the platform's) starts the second and the window again; one that stands still
(paused, or a capture's frozen clock) saw no new frame, so a frame keeps a time for fps
only when the clock moved on since the last one kept, at most `MaxFrameClock` (1024) of
them, and once the clock moves again fps comes from the frames it saw. The control count is a
walk of the tree, made on a refresh only when `Canvas::treeSerial()` has moved since the
last one, so it costs O(N) at most four times a second and only after the tree changed;
nothing is kept per control. Hidden (on a hidden page, or closed until its delayed
delete), it neither gathers nor shows; a closed one is not `find()`'s any more
(`Canvas::isPendingDelete()`), so a second `toggle()` in the same frame opens a new one
("The inspector" → "Open, close, toggle"). `open()` gathers nothing: called between two
frames (from a key callback), the canvas's previous frame there is N-2 while the
renderer's writes are N-1's, and the next update phase would gather N-1 again. Its first
gather and refresh are its animation's, in the next update phase, before the layout that
first places it, so the first window shows one frame, N-1, counted once
(`tests/unit/statsOverlay.cpp`). Like the Inspector, it reads `-` and 0 on a
`WindowCanvas`, which never runs `Canvas::updateCanvas()`.

**It settles.** A label is set only when its string changes (`tools::setLabelText`), and
every row and cell has a fixed size, so a refresh changes text and never re-lays the
panel: over a settled app the frames between refreshes write nothing, and a refresh
frame writes only the overlay's own slots, those of the labels whose strings changed and
the plots' moved segments (the plots hold their 60 points from the start, so a push
moves segments and creates none).

**What is excluded and what is not.** As with the Inspector, what it shows is the app's
work: the panel is `Base::setExcludedFromStats()`, so none of its controls, layout
entries, z entries, measures or its animation count in the canvas's counters
(`animsRun` included), and every tool's slots (its own, an open Inspector's) are taken
out of the renderer's `writtenPrimitiveIds()`/`writtenTextIds()` through the canvas's
one record of them (`Canvas::excludedSlots()`, read by `tools::appFrameWrites()`, also
the Inspector's), so `prim slots` and `glyphs` are the app's; uploads cannot be split by
id, so `upload` is the frame's whole uploads in a frame where the app wrote and 0 in
one where only the tools did. Three things cannot be split per control and include the overlay: the
**times** (its gathering every frame and its refreshes every 250 ms are in `update`,
`render` and `flush`), `draws` (its own primitive and text draws), and the `--stats`
keys that are whole-frame renderer counts (`prim_slots_written`, `upload_bytes`,
`draw_calls`, ...). `controltest_stats_overlay` holds Demo's `--stats` lines with
`--stats-overlay` to those without it, all but those renderer keys and
`excluded_changed`.

**Only its names take the mouse.** Each of the 24 name cells (the corner, `min`/`avg`/
`max`, the five time rows, "most in one frame" and the 14 counter names;
`StatsOverlay::nameCells()`) has a tooltip and takes the mouse, for it; the value cells,
the plots, the gaps and the panel take none. `StatsOverlay::getControlAt()` finds the
visible name cell under the point over the cells' fixed rects and returns null anywhere
else, counting no hit-test visit either way, so a hit test there passes through the
panel as if it were not there: the app under a value or a plot keeps its hover and
clicks (and the plots' own hover tooltip never fires). A click on a name does nothing
but what a click on empty canvas does (it closes the open menus and the shown popup,
and takes the keyboard focus away).

A name's tooltip is the canvas's, shown in its one `ToolTip` box like any other, and it
is the overlay's work, not the app's: the rule is the canvas's and general. While the
control the box shows a tooltip for is `statsExcluded()`, the box is
`setExcludedFromStats()` too: `Canvas::showToolTipFor` sets the flag from the owner
before it fills the box in, the flag stays through the hide, and the next show for an
app control clears it, so the frames that show, change and hide a tool's tooltip count
nothing of it and an app's tooltip after it counts as always (straight from a name to
the shell's Stats button, the box is the app's in that frame). A box made for a tool's
tooltip (the first tooltip of the canvas) is flagged before it is attached, and the
canvas's own share of attaching it (`onChildAdded`'s invalidate and the z mark of the
canvas's changed children) is made inside a `controls::ExcludedFromStatsScope`, which
counts the invalidations and z marks made while it is open as a flagged subtree's;
without it the first show counted one layout pass entering the canvas and one z entry.
`Canvas::toolTipShownForId()` keeps the control id of the control the box last showed a
tooltip for, through the hide. While the box is flagged it is one of the canvas's
flagged subtrees, so the canvas's record of the tools' ids (`Canvas::excludedSlots()`,
"The inspector") holds its ids for every tool: the overlay's `prim slots`, `glyphs`,
`prim ids` and `text ids` leave the box out, and so does an open Inspector's `slots
written`. A `setExcludedFromStats()` change on a control on a canvas raises
`Canvas::treeSerial()`, so the overlay's `controls` count follows the box out of and
back into the app; the record takes the flip at the next flush, so the box's writes of
a frame are counted by whose it was as that frame's flush ended. Over a settled app the refresh after
a name's tooltip shows reads 0 rendered, changed, slots, passes and z. The box is the
canvas's last child, after its overlays, so it is numbered and drawn above the panel; it
goes below the name, over the panel, where there is room, and flips above it near the
canvas's bottom (`ToolTip::placement`). It stays the canvas's after the overlay closes:
an overlay opened later is kept before it, which moves its z, so that frame writes the
hidden box's glyphs again.

The panel is owner-placed (`Placement::Owner`) and places itself `Inset` (8) units in from its
parent's bottom-right corner in its own arrange, one axis at a time as a `Popup` does
next to its anchor, so it follows a resize and a scale change. `open()` makes it on the
canvas as a canvas overlay (`Canvas::addOverlay`), numbered above the app; made as any
other control's child (StatsOverlayDemo's page) it is that control's. The panel is the
textured skin's `StatsOverlay` renderer painting the theme surface `StatsOverlay`
(`tokyonight::surfaceRegistry()`, so the ThemeEditor and a theme file's `"surfaces"`
reach it, and strict mode counts it), a dark fill at alpha 0xe0: the app's fills show
dimly through it, but the app's text under it does not, since every primitive slot is
drawn before every glyph and the panel's depth rejects the glyphs behind it.

**Open and closed: the notification.** `StatsOverlay::onOpenChangedCaller`, a static
`event::Caller`, is called when `open()` makes a canvas's overlay and when `close()`
closes it (so on every real `toggle()`), after `find()` already gives the new answer,
never for an overlay made as another control's child or for an `open()`/`close()` with
nothing to do. The info's `control` is the canvas (Canvas has no `gweni_cast`
identifier: compare the pointer, or `info.control->getCanvas()`), `integer` 1 for open
and 0 for closed, `controlCaller` the overlay. A handler sees every canvas's, so it
compares the canvas; a deleted handler is dropped (`event::Handler`'s destructor takes
itself off every caller). Nothing polls `find()`: a settled frame does no work for it.
The demo shell's **Stats** button is the user: a toggle `Button` docked Right in its
status bar, tooltip "Stats overlay (F10)", whose `onToggleOn`/`onToggleOff` open or
close the overlay only when it is not already so, and which, told of a change (F10,
`--stats-overlay`) or built while one is open (ControlTest opens it before the Demo),
sets its toggle state from `find()`. A click on it then a sync is a no-op, so the
overlay is never opened or closed twice. A click writes the button's slots and the
overlay's only; its layout pass enters the canvas alone, as an F10 open does (the
overlay is a new child of the canvas).

**Keeping clear of a bar: the bottom reserve.** `StatsOverlay::setBottomReserve(canvas,
units)` keeps `units` clear below the canvas's overlay, above its `Inset`: its bottom is
`Inset+units` up from the canvas's. The value is the canvas's, kept in a map keyed by the
canvas pointer together with its control id (ids are never reused), so it survives a
`close()` and the next `open()`, which makes a new overlay, and a canvas made later at a
deleted one's address reads 0; setting 0 erases the entry, and the shell does that when
it goes. Only the canvas's own overlay reads it (StatsOverlayDemo's, on a page, keeps to
its parent's corner), and a change invalidates that overlay alone, owner-placed, so
nothing else re-lays. The shell sets it from its own Y arrange, once its status bar is
placed, to the distance from the canvas's bottom to the bar's top (20 units: the bar
lays out 19 high, its preferred size, not the 22 it requests, inside the shell's 1-unit
padding), so the overlay's bottom is `Inset` above the bar and never covers the Stats
button; the distance is the same at every canvas size and scale, so only the shell's
first pass changes it. For that to take effect in the same pass the overlay must be
arranged after the shell: `Canvas::doThink` keeps the overlays last before the layout as
well as at the z renumber after it (`keepOverlaysLast()`, when the order is dirty), since
a control added after an overlay (ControlTest's Demo, built after `--stats-overlay` opened
it) otherwise comes before it in frame 0's walk. Set from a layout pass, the reserve
invalidates the overlay under a `PlacementScope` (the pass reaches it later and arranges
it with the new value), unless the pass has already arranged it in Y, when it is an
ordinary invalidation and the next pass places it. On the web OpenGLCore exports
`gweni_web_stats_overlay()` (`"overlay=open button=on rect=x,y,w,h"`) for `web_input`'s
`stats` check, which clicks the button in the browser.

**Phase timing in the library.** The times come from `platform::profileTimeMicros()`
(`include/gweni/platforms/platform.h`), steady_clock by default, which
`platform::setProfileClock()` replaces. `Canvas::updateCanvas()` and `renderCanvas()`
record their durations and the frame's start in `CanvasFrameStats::times`
(`controls::FrameTimes`), and `SlotRenderer::flush()` its own in
`FrameStats::flushMicros`, reported as `renderer::FrameCounts::flushMicros`; the render
phase without the flush is `renderMicros - flushMicros`, since `renderCanvas()` calls the
renderer's `end()` itself. The reads change no counter (`tools/bench.sh compare` against
the base shows every counter of every scene unchanged) and cost a fixed amount a frame,
whatever N: under callgrind 108 instructions in `update` and 129 in `render` (`idle` and
`hover-sweep` at N=1000 alike), about 55 per read. `ControlTest --stats` keeps
printing its own `std::chrono` times and `gweni_bench` its `TimedFlush`.

**Captures read a fixed clock.** A ControlTest or OpenGLCore capture (and
`--window-test`) installs `capture::installFixedProfileClock()`, which advances
`FixedProfileStepMicros` (100 µs) per read. The reads a frame makes are fixed, so the
times shown are fixed and nonzero, and a shot of the overlay is the same in every run,
on every backend and in the browser: StatsOverlayDemo is in `--shot-all`,
`controltest_determinism` and the backend and browser-against-native comparisons. Its
`draws` row is the one count that depends on the index chunk size, so
`controltest_index_chunks` leaves StatsOverlayDemo.png out. A capture never shows the
overlay unless `--stats-overlay` or a scenario opens it, so no sample changes. A window
keeps steady_clock.

**In the browser** the profile clock is Emscripten's steady_clock, `performance.now()`,
which Chromium coarsens to about 100 µs on a page that is not cross-origin isolated
(the site sends no COOP/COEP headers), so a settled frame's phases read 0.0 to 0.1 ms
there and only their averages over many frames mean much.

`tests/unit/profileClock.cpp` pins the clock and the times under a fixed one,
`tests/unit/statsOverlay.cpp` the rows, the windows, the settling, the pass-through and
the ids given back on close. `--scenario stats-overlay` (`controltest_stats_overlay`,
`_vulkan`, and `controltest_verify_layout_stats_overlay`) opens it with F10 over the
settled Demo, the clock stepping: frames between refreshes write nothing, a refresh only
the overlay's ids, each refresh shows what the harness counted for the frames of its
window, a hover over the demo list shows on the next refresh and 0 on the one after, a
click through the panel toggles the Button under it, and F10 again gives back every id
it took; then the shell's Stats button follows F10, the overlay sits `Inset` above the
status bar, right-aligned and clear of the button, and a click, F10, F10 and a click
keep the button on exactly while it is open, each telling the handlers once, each click
frame writing only the button's and the overlay's slots and laying out the canvas alone,
and the overlay giving its ids back; and with the Inspector open too (F12), each tool
shows exactly the app's slots (a frame only a tool wrote in, the other opening, closing
or giving ids back, reads 0), the overlay's ids row is the same as without the
Inspector, a hover shows in both and then 0, and closing both gives every id back.

## The depth view

`gweni::tools::DepthView` (`include/gweni/tools/depthView.h`,
`source/tools/depthView.cpp`) draws a canvas's UI in 3D, each slot at its own
`Vertex::z` as depth, through a camera that orbits, pans and dollies round it. The depth
buffer is gweni's draw order (gapped z labels, glyph *i* of a run at
`z+(i+1)/(glyphs+1)`, overlays at `getZIndex()+1`, a caret at `getOverlayZIndex()`), so
a z that is wrong (a child sharing its parent's depth, a layer a skin Control did not
report through `depthLayers()`, a popup numbered under something) shows as a stack out
of order instead of only as its symptom. It is also the retained model at work: the
camera spins while the renderer writes and uploads nothing. The examples bind F9 and
take `--depth-view [on|off]` (so `?depth-view` on a web page); the library binds no key.

**The renderer hook.** `SlotRenderer::setViewTransform(const float[16])` (column-major,
glm's layout, so glm stays out of `slotRenderer.h`) and `clearViewTransform()`. Both
backends' projection becomes `ortho * T` (`viewProjection()` in `openglCore.cpp` and
`vulkan.cpp`): `T` maps the view's own space (device px, y down, z a z index) into
itself, projectively, so a camera is built in pixels and the existing ortho turns its
output into clip space. The backend rebuilds the matrix in its `viewTransformChanged()`
hook and at `setViewRect`, never per frame, and nothing of the mirrors changes, so a
frame after a camera move writes no slot and uploads no byte. Unset, the projection is
the ortho alone, as before. Neither backend culls faces (GL never enables
`GL_CULL_FACE`; Vulkan's pipelines are `VK_CULL_MODE_NONE`), so a turned view shows the
UI's back.

**`w` is not 1 any more.** Two places assumed it, and both now divide by it, the same
arithmetic exactly when it is 1 (so `docs_samples` and `backend_pixels` are
byte-identical):
- the line style's half-layer-behind depth (`primitive.vert.glsl`):
  `vBehindDepth = behind.z/behind.w*0.5 + 0.5`;
- Vulkan's `CLIP_SPACE_TO_API` (`Vulkan/primitive.vert`, `text.vert`), which maps GL's
  clip-space z to Vulkan's: `z*0.5 + w*0.5`, [-w, w] to [0, w]. With `z*0.5 + 0.5` a
  nearer layer's depth after the divide passes 1 and it is clipped away
  (`tests/viewTransform.cpp` fails on it).

Nothing else depends on the orthographic projection: the SDF anti-aliasing is
`dFdx/dFdy` of `vLocal`, text's of the atlas coordinates, depth is `gl_FragCoord.z`, and
clipping compares the interpolated pre-transform pixel position (`vPixel`) with the
slot's clip box, so a clipped slot is clipped in its own plane however it is seen. One
thing is a little off when the view is turned: `vBehindDepth` is a `flat` varying, so a
line's partly covered pixel lerps its own depth towards the half-layer-behind depth of
the segment's provoking vertex, not of its own point, and along a segment that runs in
depth that rim depth is off by up to the segment's depth change. Only the order of a
polyline's anti-aliased rims against what lies at nearly their depth can change, which
a debug view can live with (a line sits at z+1 over its frame).

**The input: a canvas capture, not an overlay control.** A covering control would change
the UI it shows: adding it moves `treeSerial` and renumbers z, taking the mouse un-hovers
the control under it, and taking the keyboard focus ends a caret's blink and commits a
half-typed `ColorEdit` or `DragNumber`. Instead `Canvas::setInputCapture(InputCapture *)`:
while one is set, each of the canvas's `input*` calls (move, button, wheel,
`inputModifierKey`'s keys, characters, the cursor leaving) hands the event to it
(`controls::InputCapture`, `canvas.h`) and returns, so nothing is hit-tested and
`input::`'s state (the hover, the focus, the mouse position, the keys held) stays as it
was. One pointer test per event, none per frame. The UI keeps running: animations, the
caret blink, a tooltip's delay. When the capture goes the canvas has the mouse position
it had, and the next real move hit-tests from there, and a hover made stale by content
moving under the mouse (`Canvas::takeHoverStale`) waits for that too. A key or a
mouse button the canvas has down when the capture is set does not stay down there: no
key repeats while a capture is set (`input::onCanvasThink`), and the release of a key
`input::` has down, or of a left or right button it has down, goes up on the canvas as
well as to the capture, so a held Backspace does not go on deleting once the view is
closed, and a held press (a Button, a slider's thumb, a Window's title bar) ends where
the canvas has the mouse, its mouse focus let go: a Button held under it is clicked, as
a release there would click it. A key or button pressed while the capture is set was
never down on the canvas, so its release after the capture is gone goes nowhere. A canvas deleted with a capture set tells it (`captureCanvasDeleted`). The apps'
F9-F12 are handled in their key callbacks before the canvas, so the other tools still
toggle while the view is open. The web reaches the canvas through `input::GLFW3` as
native does, so it needs nothing of its own.

**The tool.** A plain object, not a control: `DepthView::open(canvas, renderer)` (the
canvas skin's renderer by default; a `dynamic_cast` to `SlotRenderer`, and on any other
one line on stderr and nothing opened), `close`, `toggle`, `find`, and
`onOpenChangedCaller`, called after each open and close with the canvas and 1 or 0 (for
a toggle button kept in step; not for a view whose canvas is deleted under it). One
renderer draws one canvas's view at a time. It sets the canvas's `needsRedraw()` on each
camera move, for a loop that waits for one. The camera is built from the renderer's view
rect, so an app calls `DepthView::viewChanged(canvas)` after a resize
(`capture::fitCanvasToWindow` does).
The demo shell's **Depth** toggle (`Demo::depthButton()`, left of Stats in its status
bar) is such a button: a click opens the view on the skin's renderer, and it follows
F9, Escape and `--depth-view` through `onOpenChangedCaller` (and `find()` when the
shell is built with the view open). The view takes the mouse, so a click on the button
while it is open does nothing; Escape or F9 leaves.

**The camera** (`DepthView::Camera`), in the view's device px with x right, y down and z
towards the camera:
- the model: the canvas centre on z 0 at the origin, each z label `zSpacing` px towards
  the camera (raw z, its gaps as they are); by default (`defaultZSpacing()`) the
  canvas's highest z (`Canvas::highestZIndex()`, the last renumber's) half the view's
  width in front, clamped to 0.01..64 px a label;
- turned by `yaw` (about the vertical axis) then `pitch` (clamped to ±90°) round that
  centre, and panned across the view;
- perspective: a camera `distance` focal lengths from z 0, the focal length
  `FocalViews` (4) times the view's longer side. A point at depth d from it is drawn at
  `c + (x, y)*focal/d`, so the plane `distance - 1` focal lengths in front of z 0 is
  drawn exactly as in the front view, a nearer layer larger and a farther one smaller;
  orthographic: every layer at the perspective's z 0 size, `1/distance` times the front
  view's;
- at reset (`resetCamera()`) the camera is straight on, in perspective, with the default
  spacing, at `distance = 1 + zSpacing*high/focal` (`high` the highest z, at least 1,
  both from the helpers `apply()` takes them from, so they cannot drift apart): the
  highest z, the frontmost layer, is drawn exactly as in the front view, and every layer
  behind it smaller about the view's centre, z 0 at `focal/(focal + zSpacing*high)` of
  its size (0.889 on a landscape view, where the highest z is half a width in front and
  the focal length four widths; nearer 1 on a portrait one). Each layer is inside the
  front view's rect scaled about its centre, so the whole canvas is on the view whatever
  it holds and wherever (`tests/unit/depthView.cpp`, on a landscape and a portrait
  view). Orthographic draws every layer at that z 0 size, so P at reset does not move
  the back plane. A spacing widened with + after a reset can push the front layer past
  the edges again; R fits it again;
- the z index out is fitted to the scene: everything is within the radius of the view's
  half-diagonal and its highest z from the centre, so perspective maps the nearest and
  farthest distances that radius allows to `kMaxZIndex` and 0 (`a + b/d`, which as
  `Z = w*(a + b/d)` is linear in the point), and orthographic maps the radius linearly;
  a 24-bit depth buffer has 256 steps a z index out, so two labels of the Demo shell at
  its default spacing (0.01 px) are still about a hundred steps apart, and ButtonDemo's
  (2.6 px) tens of thousands.

The mouse and the keys: left-drag orbits (0.4° a unit), right- or middle-drag pans by
the mouse's device px, the wheel and W/S dolly (0.85 a notch, away from the user is in),
the arrows and A/D move the camera 24 units, P switches perspective and orthographic, +
and - space the labels 1.25 times further or closer, R resets (straight on, perspective,
the default spacing, the highest z at its front-view size) and Escape closes it, as F9
does in the examples. The view opens at reset.

**`--depth-view`** opens it once the first frame has numbered the z labels
(`capture::openDepthViewOnce`, after each `updateCanvas()` in the apps' loops), so the
default spacing fits them: before the first renumber `highestZIndex()` is 0. Raw z
spacing shows what the numbering does: the demo shell numbers every page, the hidden
ones too, so its highest z is about 42700 and the page shown sits near the back
while the demo list and the status bar, numbered after the pages, stand near the camera.

**Its cost.** Off, nothing: no shader change in the hot path and no per-frame CPU work
(`tools/bench.sh compare` moves no counter). On, a camera move is a matrix built on the
CPU and handed to the backend; the frame after it, like the frames opening and closing
the view, enters no control, writes and uploads nothing, lays out nothing and renumbers
no z.

**Backends.** GL and Vulkan draw the same picture for the same camera, held to
`backend_pixels`' bound (max |Δ| 1, 0.001 % of the pixels of a turned perspective shot);
desktop GL and GLES byte for byte. WebGL 2 (ANGLE's SwiftShader) differs from native
along the edges of turned quads and glyphs (up to 64 levels on 0.17 % of ButtonDemo's
pixels; interiors equal): the two rasterisers place a sub-pixel edge differently, which
the axis-aligned front view never shows, so the web's comparisons are not run on it.

`tests/viewTransform.cpp` pins the hook on every backend (the identity and none draw the
front view uploading nothing, a whole-pixel translation moves every pixel by exactly
that much, a view from behind is mirrored with the far layer in front, a perspective
camera draws its reference layer as the front view and a nearer one larger and in
front), `tests/unit/inputCapture.cpp` the capture and `tests/unit/depthView.cpp` the tool
on a `MirrorOnlyRenderer`. `--scenario depth-view` (`controltest_depth_view`, `_vulkan`,
through `tests/depthView.cmake`, which runs it twice and requires byte-identical PNGs,
and `controltest_verify_layout_depth_view`) opens it with F9 on ButtonDemo, the clock
frozen and the mouse resting on "Toggle Me", sets a turned perspective camera, moves it
with a left-drag, the wheel, Left, P and +, clicks on the toggle and closes it with F9:
every one of those frames enters no control, changes, writes and uploads nothing, lays
out nothing, renumbers no z and hit-tests nothing, the click presses nothing, the
hover, focus, tree serial and live ids are kept, and the frame after the close is the
one before the open, byte for byte. `backend_pixels` holds its shots on Vulkan to GL's.

## The URL, history, links and storage (`platform.h`)

An app reached through deep links (`/app/tasks/42?view=backlog`) keeps its state in
the URL, opens external links in a new tab, keeps a draft across a reload and shows
times in the user's zone. `gweni::platform` has a small set of functions for that, the
same on every target: the browser's own behaviour in a page, a native equivalent or an
in-process stand-in elsewhere, so the app is written once. Fetch and WebSocket stay the
app's (Emscripten's own APIs), and nothing here needs ASYNCIFY or another main loop.

- **The URL helpers** (`source/platforms/url.cpp`, plain string functions): `Url {path,
  query, hash}` with `parseUrl`/`formatUrl` (split at the first `?` and `#`, nothing
  decoded), `parseQuery`/`buildQuery` (`+` a space, `%XX` a byte, so UTF-8 comes back as
  it went; a key with an empty value is written alone, so `?stats-overlay` stays a flag),
  `splitPath`/`joinPath` (each segment decoded or encoded on its own, so `%2F` is a `/`
  inside its segment; an empty segment and a trailing `/` are kept). There is no router:
  the app matches `splitPath(parseUrl(currentUrl()).path)` itself.
- **Three forms, no resolver.** `pushUrl`/`replaceUrl` take `/path[?query][#hash]` (all
  three parts replaced), `?query[#hash]` (the path kept) and `#hash` (the path and query
  kept), and refuse anything else with `false` and one stderr line: a relative path
  (`tasks/42`), a scheme or a host (`https://...`, `//host/x`), a `.` or `..` segment
  (encoded or not), and a character a browser would percent-encode itself in that part
  (a space, a control or non-ASCII byte, `"`, `<` and `>` anywhere; `\`, `^`, `{`, `|`,
  `}` in the path; a backquote in the path or hash; a `'` in the query: Chromium's sets,
  measured byte by byte through `location` by `tests/webPlatform.cpp`). So what
  `currentUrl()` gives is never refused in the part it came from: the demo copies the
  hash it keeps as it is, and `#a|b` in the address bar stays pushable. One function, `detail::resolveUrlTarget`
  (`source/platforms/urlTarget.h`), turns the argument into the whole new path, query and
  hash on every target, and the web hands that to `history.pushState`/`replaceState`
  made absolute against `location.href`. With no relative resolution anywhere, the
  in-process history and the browser cannot disagree, a page's `<base href>` cannot
  change what a push means, and `currentUrl()` gives back exactly what was pushed (no
  character in it is one the browser rewrites).
- **Back and forward.** `historyBack()`/`historyForward()` move the history and
  `setUrlChangedHandler(fn)` is told the new URL; the app's own push or replace is never
  delivered. On the web they are `history.back()`/`forward()` and a `popstate` listener
  (installed with the first handler, removed with an empty one and when the runtime
  exits) that calls the exported `gweni_platform_url_changed`. A DOM event is a task of
  its own, outside the main loop's frame, as input is, so the handler runs between frames
  and never inside the render walk; `history.back()` is asynchronous, so it runs on a
  later task, and it also runs for the browser's own buttons and a hash edited in the
  address bar. Elsewhere the history is an **in-process stack** starting at `/`: a push
  drops the entries forward of the current one, and back and forward call the handler
  inside the call. A desktop app given a deep link on its command line passes it to
  `replaceUrl` at start-up; there is no other way to set it.
- **Opening a link.** `openUrl` takes `http` and `https` with a host and `mailto` with an
  address, and refuses every other scheme (`javascript:`, `file:`, `data:`, ...), a space
  or a control character, with one stderr line. On the web it is `window.open(url,
  '_blank', 'noopener,noreferrer')`; with `noopener` it returns null whether a popup
  blocker stopped it or not, so a blocked tab cannot be told apart and `openUrl` does not
  claim to. A link opened from a click or a key works: emscripten-glfw delivers input
  inside the DOM event, which carries the user's activation. One opened from a timer or a
  fetch callback may be blocked. On Linux it is `xdg-open` (`open` on macOS,
  `ShellExecute` on Windows), started through fork and exec with the URL as its one
  argument, never through a shell, in a grandchild the app never reaps; a failed exec
  comes back through a close-on-exec pipe, so `openUrl` returns false when there is no
  opener. `setOpenUrlHandler` replaces the opener (after the checks), for tests.
- **Storage.** `storageGet`/`storageSet`/`storageRemove`/`storageKeys` keep UTF-8 strings,
  every key under `setStoragePrefix`'s prefix (on every target), so apps sharing a store
  do not collide. On the web the store is `localStorage` (a write over the quota returns
  false with the old value kept). Elsewhere it is the JSON object in the file
  `setStorageFile` names, loaded whole when it is set and written whole on every change:
  into a temporary file beside it, flushed, then renamed over it, so the file is the old
  one or the new one and never part of either; a `storageSet` of the value already
  there, or a `storageRemove` of a key not there, writes nothing. It is not a database: two instances on one
  file each write what they hold, and the last one wins. With no file the values are in
  memory, and the first use says so on stderr once per process.
- **Names.** `timeZoneName()` is the IANA zone (`Intl.DateTimeFormat().resolvedOptions()
  .timeZone` on the web; elsewhere `TZ`, or the zoneinfo file `/etc/localtime` links to,
  or `/etc/timezone` where `/etc/localtime` is a copy and not a link, `UTC` with no
  `/etc/localtime`, empty where none of them names it), `localeName()` the BCP 47 locale
  (`navigator.language`; elsewhere `LC_ALL` or `LANG` without its codeset, so
  `en_US.UTF-8` is `en-US`, and `en-US` for `C`). Under
  Emscripten `localtime_r` already uses the browser's offset, so a name is all an app
  needs to label its times; there is no zone database or date formatting here.
- **Under node** (the web tree's `unit:` tests) there is no `window`: every function keeps
  to the in-process version, so `tests/unit/browserPlatform.cpp` runs there unchanged,
  and `tests/webPlatform.cpp` pins the browser's in `gweni_unittests.html`.
- **No window title.** The app owns the GLFW window on both targets and
  `glfwSetWindowTitle` sets `document.title` in emscripten-glfw, so the library, which
  owns no window and links no GLFW, has no title call.

**A page that keeps its state in the query** sets `Module.gweniQueryArgs = false` beside
`Module.gweniDefaultArgs` (`examples/openglcore/web/pre.js`): main then gets the default
arguments alone, instead of every query key as `--key value`, which ControlTest and
OpenGLCore would refuse, and the app reads `currentUrl()` itself. `pre.js` never reads
the path.

**A page served at every path** (the orchestrator serves its frontend at every
`/app/...`) must load its script absolutely (`src="/app/OpenGLCore.js"`) or set `<base
href>`: a relative `src` at `/app/tasks/42` loads `/app/tasks/OpenGLCore.js`, and
Emscripten finds the `.wasm` and `.data` next to the script (`scriptDirectory`), so the
script tag is the one URL that matters. gweni's own pages (`shell.html`,
`tools/site/*.html`) are served from a directory and stay relative; `web_navigation`
serves `OpenGLCore.html` at every `/app/` path with `<base href="/">` put in its head.

**The demo shell** uses the query alone, since GitHub Pages cannot serve every path.
`Demo::setPageShownHandler` reports each page shown and whether the user picked it from
the list or the app showed it (`showDemo`). OpenGLCore's window (never a capture, so
what a capture draws never depends on an earlier run) pushes `?demo=<Name>` for a pick,
keeping the query's other keys, and replaces the URL for a page it showed itself
(`--demo`, the stored page, back or forward) or one picked while it is already shown, so
restoring a page never grows the history and Back always shows another page; the hash
is kept as `currentUrl()` gave it; its URL-changed handler shows the page the URL names (the start page if none).
It stores the page last shown (`localStorage` under `gweni-demo/` on the web,
`$XDG_STATE_HOME/gweni/OpenGLCore.json`, or `~/.local/state/gweni/...`, elsewhere, so a
desktop run prints nothing) and shows it at start-up when `--demo` names none, and titles
the window `gweni — <Name>`. With neither, the page the shell is built showing (its
first, `Demo::shownDemo()`) is the page shown: titled and stored at once, so a pick of it
replaces, with the URL left bare, which already names the start page. The status bar's "gweni on GitHub" link opens
`Demo::LinkUrl` with `openUrl`. `web_navigation` (`tests/web/navigation.mjs`) checks all
of it in Chromium on `OpenGLCore.html` and `demo.html`.

## The clipboard (`platform.h`)

`platform::getClipboardText()` and `setClipboardText()` are an in-process buffer
unless the app installs a backend for the system's clipboard. A window's app does; a
capture, a test and the bench do not, so what they paste never depends on the host.

- **The backend.** `platform::setClipboardBackend(get, set, request)` replaces the
  buffer: `get` and `set` answer `getClipboardText()` and `setClipboardText()`, and
  `request`, if given, answers `requestClipboardText(reply)`, holding the reply and
  calling it whenever the text arrives. An empty function keeps the buffer for that
  call, and all three empty give the in-process clipboard back (a test that installs
  a fake one does that on its way out: the state is process-global). With no backend,
  or one without `request`, `requestClipboardText` answers inside the call, so a
  scripted Ctrl+V still pastes in its keystroke.
- **A paste is a request.** `TextBox::onPaste` and `CodeEditor::onPaste` ask
  `requestClipboardText` and insert the text when it comes, as one paste step. The
  reply holds the control's id, not a pointer: `controls::Base::getControlById(id)`
  finds the live control, or null once it is deleted (`~Base` clears its entry, and
  ids are never handed out again), so an answer for a box deleted meanwhile does
  nothing. Copy and cut call `setClipboardText`, which is also what a Copy button calls
  (TextBoxDemo has one): on the web from an input handler, whose user activation the
  browser's write needs.
- **Who installs it.** `input::GLFW3::useSystemClipboard(window)` (a header compiled
  into the app; the library does not link GLFW) installs GLFW's
  (`glfwGetClipboardString`, `glfwSetClipboardString`; an empty or non-text clipboard
  pastes nothing, without a GLFW error line), and null gives the buffer back, before
  the window is destroyed. The examples call it for the interactive window only:
  `GlDevice::open` for a shown GL window and the Vulkan window
  (`capture::DeviceOptions::systemClipboard`, which ControlTest's `--window-test`
  turns off), undone by `destroyContext`. Another `input::GLFW3` app calls it once with
  its window. On X11 the text is the `CLIPBOARD` selection: after a copy the window owns
  it and serves other clients from `glfwPollEvents()`, which the window loop runs, and
  loses it when it closes (the X11 rule). `clipboard_x11` (`tests/clipboardX11.cmake`)
  holds both directions to another X client, `gweni_test_clipboard`
  (`tests/x11/testClipboard.cpp`), with ControlTest's `--clipboard-test`, a script in a
  shown GL window under Xvfb, and `--wait-clipboard-change`, which polls the window
  until the other client has taken the selection.
- **In a browser page** `useSystemClipboard` calls `platform::useBrowserClipboard(true)`
  (`crossPlatform.cpp`'s `EM_JS` glue), so the interactive page has the browser's
  clipboard and a headless page capture keeps the buffer. emscripten-glfw's own would
  not do: its `glfwGetClipboardString` gives the text of the last paste event, and
  gweni's Ctrl+V runs on keydown, before the browser fires that keystroke's paste, so it
  would paste the clipboard as it was before; and its copy listener, finding no DOM
  selection, writes `' '`. So gweni listens to `copy`, `cut` and `paste` itself, on
  `window` in the capture phase, before emscripten-glfw's listeners on `document`, which
  never see an event gweni takes (`stopImmediatePropagation`). emscripten-glfw leaves
  Ctrl+C, X and V to the browser (no `preventDefault`), so the event follows gweni's
  keydown in the same task:
  - a copy or cut: `setClipboardText` records the text, and the event writes it
    (`clipboardData.setData`, `preventDefault`). It also calls
    `navigator.clipboard.writeText`, which is the write for a copy no event follows (a
    button's click); in a keystroke both write the same text. A copy with nothing
    recorded (no selection in the box) leaves the clipboard as it is;
  - a paste: `requestClipboardText` holds the reply, and the paste event answers it with
    the clipboard's text at that moment, before the next frame. With no paste event (a
    paste from a click), a task later it reads `navigator.clipboard.readText()` if the
    page may, and otherwise pastes nothing;
  - a copy or paste for another element of the page (a text field, a selection of the
    page's own text with the focus off the canvas) is left to the browser.

  `getClipboardText()` there is the text last copied or pasted in the page.
  `Module.gweniClipboard` keeps `writes` (the copy and cut events gweni wrote in) and
  `answers` (how each paste was answered: `paste`, `readText` or `none`), which
  `web_input`'s clipboard check reads: headless Chromium fires a native paste event for
  Playwright's Ctrl+V. Cmd+C/V on a Mac reach no accelerator yet (gweni's are
  Ctrl-only, #658).
- **Ctrl+letter runs once.** emscripten-glfw also sends Ctrl+letter's letter as a char
  event, right after its key event (native GLFW sends none), and
  `Canvas::inputCharacter` ran the accelerator a second time, so on the web every
  Ctrl+letter accelerator ran twice (a paste pasted twice, Ctrl+Z undid two steps).
  `input::GLFW3::processCharEvent` drops a char that follows a Ctrl+letter press
  (`tests/unit/clipboardBackend.cpp`).

## Images from memory: the loaders, ImagePanel's modes, the image grid

An app whose images come over the network (the dashboard's attachments and
galleries, fetched as full-size bytes) has no file for them, so `IResourceLoader`
loads them from memory too (#606). `SlotRenderer` is the loader that implements it;
the base class fails these loads (`ErrorBadData`), so another loader still builds.

- `loadTexture(texture, bytes, size, maxSize)`: a PNG or JPEG, the format found from
  its first bytes (`imglib::detectFormat`: the PNG signature, or `FF D8 FF`), never
  from a name. `loadTexturePixels(texture, width, height, stride, rgba, maxSize)`:
  rows of RGBA8 texels `stride` bytes apart (the padding not copied). A file load
  (`loadTexture(texture)`, by name) reads the file and takes the bytes path with no
  max size, so a file and its bytes load the same.
- **The limits, from the header.** `imglib::probeImage` reads the width and height
  from a PNG's IHDR or a JPEG's first frame header, decoding and allocating nothing.
  An image over `SlotRenderer::kMaxImagePixels` (64 megapixels), or with no max size
  and a side over `kMaxImageSide` (2048, the largest image page), fails before any
  pixel buffer is allocated. The decoders also take the limit (`maxPixels`) against
  the image as they would decode it, so a header the probe misread cannot allocate
  past it either.
- **Kept at a max size.** `maxSize` is the longest side kept, in image px (0: as it
  is). The image is reduced through its own mip chain (`imageMipLevel`, the level
  `imageMipChain` makes) to the largest level that fits, each level freeing the one
  above it, and only that level and the ones below it are uploaded: 4000x3000 at
  512 keeps 500x375. A JPEG decodes straight at 1/2, 1/4 or 1/8 (libjpeg's
  `scale_denom`) while that still holds the side kept, so its full size is never
  allocated (4000x3000 at 512 decodes at 1000x750, `lastDecodedSize()`); a PNG
  decodes whole and its full buffer is gone before the load returns. The full bytes
  are not kept, so a new canvas scale does not decode again. Grey and grey-with-alpha
  images become RGBA.
- **Failures.** Bad, truncated or unknown bytes, a corrupt header and bad pixels are
  `ErrorBadData` with one log line (`Texture <what>: <source>`), the texture unloaded,
  so it draws the missing image. imglib's buffer loaders print nothing of their own
  (libpng's errors and warnings and libjpeg's messages are dropped) and fail on
  libjpeg's premature-end warning rather than decoding grey rows;
  `tests/unit/imageBuffer.cpp` changes every byte of a PNG and a JPEG in turn, and
  every prefix of them, and none aborts or reads past its buffer (the ASAN tree).
- `loadedTextureCount()` is the loads not freed yet, the leak check of every image
  test; a texture is its `dataIndex` (the old name-keyed map is gone, since an image
  from memory has no name).

**ImagePanel** draws one image from a file (`setImage`), bytes (`setImageData`) or
pixels (`setImagePixels`), each with the loaders' `maxSize`; `clearImage()` frees it.
Every set frees the image before it, and setting the same name, bytes or pixels and
max size again (an FNV-1a hash of them) marks nothing. A panel with no image (nothing
set, or cleared, `isEmpty()`) hides its slot and draws nothing; only an image that
failed to load (`failedToLoad()`) draws the missing image over its bounds. A new image
on a shown panel writes its one slot at most: the image takes the layer the old one
gave back, so a same-size replacement's slot can keep its bytes, only the page's
texels changing. It still takes its primitive id on its first render, empty or not,
so an image set later keeps its slot after its parent's.

`setScaleMode` places the image in the render bounds (`ImagePanel::placeImage`, a
rect in whole units and the UVs): `Stretch` fills them, `Fit` is the largest rect of
the image's aspect ratio inside them, centred, `Fill` covers them, centred, and `None`
is the image at one image px a logical unit (it grows with the canvas scale), centred.
`Fill` and `None` are cut to the bounds through the UVs, not through a clip, so the
slot never draws outside its control. `translate()` snaps the rect's edges as any
rect's, so a mode looks the same at scale 1, 1.5 and 2 (`tests/imagePanel.cpp`).
`setStretch(true)` is `Stretch` and `false` is `None` (which used to draw from the
top-left at the natural size, past small bounds).

**layout::Tile** sizes itself now: its preferred height is the shown children's rows
at the width its X arrange gave it (`columns()`, the wrap `arrange` does) times the
tile height, and its padding, so a `ScrollControl` around it (docked Top, a stable
gutter, as for `layout::Flow`) scrolls to its last row, and a cell added or a resize
lays out in one pass (`tests/unit/tileLayout.cpp`).

**ImageGridDemo** (ControlTest's `--list`, not the shell: its bad-data cell logs a
line, and the shell must not) is 12 images from PNG and JPEG bytes in the binary
(`examples/demo/imageGridImages.inc`, drawn by `tools/gen_image_grid_images.py`),
fitted in captioned cells of a Tile in a ScrollControl and kept at twice the cell's
width, an empty cell and one given bytes that are no image; Replace swaps the first
cell's image and back, Free clears the second's and loads it again.
`controltest_image_grid` (`--scenario image-grid`) holds each click to its button and
cell (slots and pixels) and the live image count to what it should be. On the web,
ControlTest exports `gweni_web_set_image(cell, ptr, len)`, which gives a cell of the
window's grid the bytes a page `fetch()`ed and copied into a `Module._malloc` buffer
(the pages export `_malloc`, `_free` and `HEAPU8`); `web_input`'s image check fetches
a PNG served beside the page that way and finds it in the cell.

## Missing resources: degraded, not fatal

The library reports a resource it cannot load and goes on drawing; it never exits.

- **Skin config** (`DefaultSkin.json` missing, a directory, unreadable, not parsing,
  or not a JSON object): `TexturedSkin::init` prints one line naming it and returns
  `false` with the skin usable: every registered surface `colors::Missing` (magenta,
  `colors::MissingDark` borders, `tokyonight::setAllSurfacesMissing()`), the text
  colours at their compiled defaults so labels stay readable. The canvas clear colour
  the examples take from `tokyonight::Canvas.top` is magenta too.
- **Atlas texture** (`textureFileName`): reported and `init` returns `false`, but the
  rest of the config, the theme included, applies. A texture that cannot be drawn is
  `drawMissingImage`, the same magenta.
- **Font**: `SlotRenderer::loadFont` draws a missing or unreadable face in the built-in
  OpenSans and returns `Font::Status::LoadedFallback` (see "A missing face is drawn in a
  built-in font"); only if that fails too is the text drawn as nothing.
- **Mono and chain faces** (the config's `"monoFont"` and `"fontFallback"` faces): a
  file the config names that is not there prints one line naming the path, once per
  file however many roles name it (`Mono font
  file not found: <path> (using OpenSans)`, `Glyph fallback font file not found:
  <path>`) and `init` returns `false`. A missing mono face is replaced by the default
  font's face (`getDefaultMonoFont()` keeps its size), a missing chain face is left out
  of the chain ("Font fallback").

What to do about it is the application's choice. The examples (`OpenGLCore`,
`ControlTest`) keep going: `capture::resourcePaths()` reports a missing resource
directory and returns the paths anyway, and a `false` from `init` or a default font
that is not `Loaded` (`capture::loadDefaultFont` returns the status) only marks the run
degraded. A window keeps running and exits as usual. A capture writes what it drew and
`capture::exitStatus` turns its exit status into `capture::ExitDegraded` (2), after one
`<exe>: resources incomplete, exiting 2` line, so a script notices; a failure (1) wins
over it. `gweni_bench` stops with 1 instead, since a degraded frame measures nothing
worth having.

## Current state vs. goals

What exists today (the foundation) and what's still aspirational:

**Implemented**
- Decoupled core ↔ renderer via `renderer::Base`; OpenGLCore is the only backend, the
  GL implementation of `SlotRenderer`'s hooks.
  `StubRenderer` (`tests/unit/headlessFixture.h`) is a second implementation, compiled
  in every build with tests on (the default), so the interface stays implementable
  outside OpenGL.
- Two-phase `updateCanvas` / `renderCanvas` split.
- State-change tracking; unchanged controls emit no renderer calls, and the render
  walk skips every subtree with nothing pending in it, so a settled frame visits no
  control (`ChangeStats::visited`).
- Persistent primitive-id-indexed CPU vertex buffers in OpenGLCore, drawn in chunks
  over one static 16-bit quad index buffer; per-control
  primitive ownership via `skin::Control`.
- **Incremental GPU upload.** `SlotRenderer::flush()` tracks which primitive slots were
  written and uploads only those, coalesced into contiguous `glBufferSubData` ranges;
  when nothing changed it skips the transfer and just draws the buffer already on the
  device. The buffers grow geometrically (`uploadDirty`): when the mirror outgrows a
  vertex buffer it is re-allocated (`glBufferData` with no data, so its name and the
  chunks' vertex arrays stay) at 1.5 times the mirror in whole slots, and the mirror
  sent once with `glBufferSubData`; the clip-box textures likewise, by rows, above.
  That one frame is a regrow (`FrameStats::bufferRegrows`, `buffer_regrows=` in
  `gweni_bench` and `ControlTest --stats`), so a mirror growing by a slot a frame
  regrows O(log n) times over n frames. Every other frame sends the dirty slots and
  the slots the mirror gained since the last upload, never the slack past the mirror,
  which nothing draws. The draws are each chunk's runs of slots that can show, and skip
  a chunk with none ("Only the slots that can show are drawn" above,
  `FrameStats::primitiveSlotsDrawn`, `primitiveRanges`). Before (#323) a buffer grew to exactly the mirror, and growing
  by one slot sent all of it: typing a character into `gweni_bench`'s `type`
  TextBoxMultiline sent 1.29 MB a frame at N=1000 and 12.7 MB at 10 000; now 15.9 KB
  at both, with no regrow in its measured frames.
  The glyph mirror grows by slots a run takes past its end. `allocateTextRun` gives a
  run its slots best fit from the free blocks, split to what it needs, or past the end
  of the in-use ones when none fits. A first layout gets exactly its glyphs, since most
  strings never change length; a run that outgrew its slots (a line typed into) gets
  slack, a quarter more rounded up to `kTextRunSlackGranule` (8) slots, kept when a
  free block is split, so the next characters fit where it lands and it moves O(log n)
  times growing to n glyphs. The tail past its glyphs is in no draw list. Without
  the slack the typed line freed its slots and found nothing big enough (its own hole
  was a slot short), so it moved to the end and the mirror grew by a glyph per
  keystroke. A slot is marked dirty only when its bytes actually
  change, so a control that redraws itself identically (the skin re-emits unchanged
  controls every frame) costs nothing. `GWENI_UPLOAD_STATS=1` prints the per-frame
  upload call/byte counts (the clip-box textures' row uploads included).
- **Font atlas upload follows lazy glyph rasterisation.** `loadFont()` allocates the
  atlas texture and marks the whole atlas, with the printable-ASCII cache in it, as the
  dirty band, so the next `flush()` uploads it once and counts it like any other atlas
  upload; any other glyph is rasterised into
  the CPU atlas later, by `measureText` (logic phase) or `renderText`. The vendored
  freetype-gl carries a gweni patch for this (and another, `texture_font_t::sdf_spread`,
  for the SDF glyphs): `texture_atlas_t` has a dirty row band
  (`dirty_y0`/`dirty_y1`, half-open) that `texture_atlas_set_region` widens and
  `texture_atlas_clear` sets to the whole atlas. `flush()` uploads each font's band as
  full-width rows with one `glTexSubImage2D` and empties it. Glyphs are packed in
  shelves, so a few new glyphs make a band of a few rows, and many scattered ones grow it
  to the whole atlas. Counted separately from the buffer uploads, as
  `FrameStats::atlasUploadCalls`/`atlasUploadBytes`.
- **Text-path diagnostics.** `GWENI_TEXT_DEBUG=<substring>` (empty matches everything)
  makes `measureText` print each glyph's pen, the resolved
  `texture_glyph_t` metrics and the size it returns, and `renderText` dump the
  `m_textVertexes` slots it wrote for each matching string, with each slot's index
  chunk and whether its font's draw list draws it. Off unless
  the variable is set; it is verbose (tens of lines per string).
- **Clipping.** Each control with `shouldClip()` (the default) narrows the clip region
  for its children: the render drain (`Base::drainRenderQueue`, "renderCanvas()") calls
  `addClipRegion` with its bounds at its absolute origin, which intersects them with the
  parent's region, as it rebuilds a queued control's state down its ancestor chain. A control's own
  `render()` runs before that, so it is clipped by its parent's region and its children by
  its own; `renderOver()`/`renderFocus()` get the parent's region back. A control that
  returns false (`TabStrip`, `TabButton`, `Text`, whose children are its own wrapped
  lines, and `Label`, which gives every child a rect instead) hands its children its
  parent's region. A control can also narrow one child's
  region to a rect of its own (`Base::getChildClipRect(child)`, none by default): a
  Label gives its bounds, and its Text the bounds widened by its ink margin while the
  Text fits ("Ink past the pens: the Label's clip margin"); a
  ScrollControl gives its viewport for its inner panel, so content scrolled partly out of
  view is cut at the viewport and never draws in the padding and margin band around it,
  while its bars keep their own bounds. Hit-testing (`getControlAt`) skips a child outside
  that rect the same way, so a click in the band over a row scrolled out of view does not
  reach the row. OpenGLCore stamps the region into every
  slot drawn under it and the shaders discard outside it (see "Clip boxes" above). A
  region only changes when an ancestor's bounds or a `getChildClipRect` rect do (its owner
  marks what is under a rect it changed), and `onBoundsChanged` marks the whole
  subtree (`StateChange_Geometry`) when the children's region can have changed, so a moved
  ScrollControl panel or a dragged Window re-stamps exactly its own subtree and an
  unchanged box costs no upload. A move always marks it (every absolute position changed).
  A new size at the same origin marks it only when the control has child clip rects
  (`Base::hasChildClipRects()`: ScrollControl, TabControl, DataTableHeader, whose rects
  follow their size), or clips and its region, its bounds within its parent's region, is
  not the one it had (`Base::childRegionChanged`, which narrows the region up the chain by
  the drain's rule, `shouldClip` and `getChildClipRect`, an axis at a time, stopping once
  both sizes give the same region);
  otherwise only the control is marked, and whatever its arrange moves is marked by its
  own `setBounds`. So an overflowing ScrollControl or ListBox, wherever it is scrolled
  (its inner panel keeps its origin, issue #512), given one more item marks that item, the inner panel and the thumb (4 and 6 controls, 64 and 96 before), and an
  overflowing DataTable's column drag leaves the cells before the column unmarked; a
  Window resized inside the canvas, or a CrossSplitter's clipping panel resized, still
  marks its subtree, since its edge moves inside its parent's region and every slot under
  it gets a new box. Nothing may read an ancestor's size at render time outside layout,
  or it would miss that mark (`base.h`, `childRegionChanged`; `tests/unit/sizeChangeMarks.cpp`,
  issue #511). A new viewport
  that does not move the inner panel is marked by `ScrollControl::placeContents` itself: a
  larger bottom padding over taller content, which changes neither bounds, and a bar
  showing or hiding, where the panel resized at the same origin measures its region against
  the new viewport alone and so does not mark its content. An intersection
  with no overlap is an empty rect (never a negative size) and the walk goes on into it:
  dirty children are drawn again with the empty box, so nothing stale stays on screen.
  Clean, fully clipped subtrees are skipped like any clean subtree, not culled, and a
  dirty one is still walked. `ControlTest --scenario clip` (ctest `controltest_clip`)
  checks it on the pixels: content overflowing a ScrollControl and a Window dragged past
  each edge of its parent leave everything outside unchanged, and moved right out they
  leave nothing behind. `GWENI_CLIP_DEBUG=<inset px>` insets every box, to check it on
  screen.
- Window resize (`renderer->setViewRect()` + `canvas->setSize()`), see
  `examples/openglcore/openglcore.cpp`.
- **A resize round trip comes back to the same frame, laid out in one pass.**
  `ControlTest --scenario resize` shoots a control at 640x480, 1024x768 and 640x480
  again, then on a canvas built directly at 640x480, and requires the listings, the
  renderer mirror (per primitive and text id) and the pixels of the 640x480 shots to
  match. `--require-one-pass` fails any frame whose layout took more than one pass
  or that laid out again in the render phase ([layout.md](layout.md)). ctest runs
  both for every demo: `controltest_resize`, `controltest_resize_each`,
  `controltest_one_pass_each`, and `controltest_teardown` with `--require-one-pass`.
- **The update phase visits only what changed.** No control `think()`s, the z indices
  are renumbered only on a frame after the tree order changed
  (`Canvas::zOrderDirty()`), and the animations are only the active ones, each run
  by its own control's canvas (`anim::think(canvas)`), so a settled frame enters no
  control in `updateCanvas()` and runs no animation either (`UpdateStats::zVisited`,
  `UpdateStats::animsRun`, see "updateCanvas()").
- **A settled window sleeps.** `Canvas::nextWorkTime()` says when a canvas next has
  work, and the example windows' loop (`input::GlfwLoop`) draws only then, waiting for
  events, posts from other threads and timers in between, natively and in the browser
  (`window_idle`, `web_input`; see "Threads, posts and the idle loop").

**Not yet done (the performance goal)**
- **Clipping leftovers.** A fully clipped subtree with changes in it is still walked and
  its primitives still written (with an empty box) and drawn (every fragment discarded);
  nothing culls them on the CPU. Clipping is rectangular only: a rounded parent does not
  cut its children at its corners.
- **Cache-to-texture** (`ICacheToTexture`) and free-slot compaction/defragmentation of
  the primitive buffers are stubbed. Without compaction the mirror and buffers keep
  their peak size, and a hidden or released slot is still drawn where its gap is merged
  into a chunk's ranges: a 1-slot gap, or the smallest gaps of a chunk with more than
  16 runs ("Only the slots that can show are drawn"). The web draws a chunk's ranges
  as a loop of `glDrawElements` (no `WEBGL_multi_draw`); if a measurement there shows
  the loop costs something, multi-draw or a tighter cap for the web is the next step.
  Ids are not contiguous per subtree, so a page's slots interleave with others' and
  make more runs than the page has controls.

**Vulkan (#368)**
- `renderer::Vulkan` ("renderer::Vulkan (headless)" above) implements the hooks with
  device-local vertex buffers indexed by primitive id, the dirty ranges pushed through a
  staging buffer per frame in flight, and command buffers recording one draw per chunk.
  It draws every primitive style and every text mode headless, and runs every GPU unit
  case but the `gl_only` ones; the apps' `--renderer` (#373), the cross-backend checks
  (#374) and a window (#375) are still to come.

See also [layout.md](layout.md) for the docking/layout system, and the skin/primitive
registration notes in `source/skins/textured/`.
