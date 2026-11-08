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
layout passes and `input::onCanvasThink`. A frame nothing invalidated runs no pass, and a
pass enters only the dirty paths: the controls invalidated or given new bounds, and the
way down to them. Every walk over children goes through `Base::doCalculateSize`/
`doArrange`, which skip a clean control, keeping its preferred size and bounds; a dirty
parent still loops over all its children, which is what such a pass costs beyond the
dirty controls themselves. A pure move does not dirty the moved control's subtree
(`docs/layout.md` → "Which controls a pass enters", which also says what a
`calculateSize()` may read for its cached result to stay valid; issue #321).
`LayoutStats::visited` (`layout_visited=` in `ControlTest --stats` and `gweni_bench`)
counts the controls the frame's passes entered, one per `calculateSize()` and one per
`arrange()` in each axis, so four per dirty control and 0 on a settled frame;
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
focused TextBox and no moving bar (Demo's 2 are ProgressBarDemo's indeterminate bars).
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
chain `m_zDirty` (`Base::zOrderChanged`), as `m_childStateChanged` is marked for the
render walk, and a new control starts dirty. The walk then passes a child in O(1),
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
`Menu` or `Modal` included, and be clipped to the canvas alone. It is a child of the
canvas, so its clip region is the canvas's, and it is kept as the canvas's **last**
child: `Canvas::keepOverlaysLast()` moves it back to the end of the child list just
before each z renumber. Every change of the child order (a `Menu` added or opened with
`bringToFront`, a `Modal`, any `addChild`) asks for a renumber anyway, so this costs
nothing on other frames, and the depth-first numbering then gives the box and its text
the highest z values in the tree. A reserved z range above the tree was the other
choice; it would need a ceiling that no tree reaches and a second numbering for the
overlays, while the last-child rule reuses the one numbering there is. The popup (below)
takes the place just before it. GWEN's drag-and-drop
preview, which re-rendered the dragged control after the walk at the mouse offset, was
deleted rather than moved here: the only drag that set it hid that control first, so it
never drew anything.

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
open(this)`). Closing a popup closes the Menus open over it. **Ownership:** a popup is
its creator's: closing only hides it, and the canvas deletes it with its other children.
Once opened it is a child of the canvas, not of the control that made it, so it is not
deleted with that control: a control that makes a popup makes it on the canvas and
`delayedDelete()`s it in its destructor (`PopupDemo`), delayed so that a canvas tearing
down, which deletes it as one of its children, deletes it once.

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
right in the frame's one pass, and placed again on every pass: when the canvas shrinks or
grows back and when the anchor moves. `updatePopup` lays it out again when the anchor's
canvas rect is not the one it was last placed by (an ancestor scrolled or moved without a
pass reaching the popup). A zero-size anchor is a point: `Menu::placement(cursor, ...)`
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
menu on the canvas (a ComboBox list, a strip or submenu placed by `setPos`/`setBounds`,
and a position asked for after `openAt`) is only kept inside the canvas by `keepInside`,
never flipped. `Menu::open(pos)` opens at the mouse through `openAt`, so a popup opened
that way (the Properties colour selector) now flips up near the bottom instead of
running past it.

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

### 2. `renderCanvas()` — render

`Canvas::renderCanvas()` brackets the frame with `renderer->begin()` / `end()` and
walks the tree via `doRender()` → `renderRecursive()` → `Base::render()`. For each
control, `Base::render()`:

- returns immediately if `getStateChange() == StateChange_Nothing` (nothing to do),
- on `StateChange_Created`, calls the skin renderer's `generate()` to **allocate
  primitives** for that control,
- calls the skin renderer's `update()` to **re-emit only the changed draw state**,
- clears the control's state-change mask.

So an unchanged control issues no renderer calls at all; a changed control only
rewrites its own primitives.

The walk itself only goes where something changed. `addStateChange()` sets the
control's bits and then `m_childStateChanged` on each ancestor, up to the first one
that has it already (the chain is `m_actualParent`, the owners of the `m_children`
lists the walk follows). Attaching a control that has something pending, in
`addChild()` or `addChildOfLayout()`, marks its new parent's chain the same way.
`doRender()` returns at once for a control with neither its own change nor the bit,
before the offset and clip push and the loop over its children. So a settled frame
enters no control at all, and a hover enters the controls it repaints and their
ancestors.
`renderRecursive()` clears the bit before it visits the children. A change raised on
one of them later in the walk (from a sibling's `render()`, say) marks the path
again: it is rendered later in the frame if the walk has not reached that control
yet, and otherwise on the next frame. `ChangeStats::visited` counts the controls
entered (`ControlTest --stats` prints it as `visited=`). Demo's first frame enters
2662 and a settled one 0. `tests/unit/renderWalk.cpp` pins the skip. A
control's `render()`, `renderOver()` and `renderFocus()` therefore run only on a
frame something in or below it changed. Anything that changes what one of them draws
must raise a state change: `ImagePanel`'s setters do, for example.

`renderRecursive()` loops over the live `m_children` list, with no copy, so the walk
allocates nothing per child. It still hands every child of an entered control to
`doRender()`, so it costs O(siblings) of each control it enters in time, which
`ChangeStats::childrenScanned` counts (`children_scanned=` in `ControlTest --stats`
and gweni_bench). One changed row of a 10 000-row ListBox scans 10 000 children.
Visiting only the dirty ones would need an index of them kept in `m_children` order
(#407).

**The render walk only draws.** Adding, removing, reordering or deleting a control
belongs to the update phase or to layout, never to a `render()`, `renderUnder()`,
`renderOver()` or `renderFocus()`. That is what lets the loop follow the live list:
`sendToBack()` removes the control's list node and pushes a new one, so a render
that called it on the child being visited would leave the loop on a freed node. The
rule is checked in every build. `Canvas::renderCanvas()` and `WindowCanvas`'s
render hold a `RenderWalkScope` around `doRender()`, after `renderPhaseLayout()`
(layout may create controls before the walk: a wrapped `Text` builds its line
children there). `renderRecursive()` keeps the control it is rendering in a global
(saved, set and restored, one store per visit). While a scope is open,
`Base::addChild()`, `addChildOfLayout()`, `removeChild()` (and so `setParent()`),
`bringToFront()`, `sendToBack()`, `bringNextToControl()` and `~Base` each print one
line on stderr the first time an (operation, type) pair is seen:

```
gweni: <Type>#<id> <operation> during the render walk (rendering <Type>#<id>)
```

They also record it in `controls::renderWalkMutations()` (`resetRenderWalkMutations()`
forgets the record, for tests). `~Base` sees the type as `Base`, because the derived
destructors have run. Outside a scope the check is one branch. The report does not
stop the walk: what that frame does next is undefined, as with any use-after-free,
and the line is how the bug is found. ControlTest exits non-zero when the record is
not empty, as it does for `typesWithoutSkinRenderer()`, so `controltest_teardown` and
`controltest_one_pass_each` fail on an offence in any demo.

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
pure-virtual hooks. `OpenGLCore` is the GL backend; a Vulkan one (#368) and a WebGL one
(#338) are meant to be others. The core holds, with no API call and no GL header of its
own:

- the CPU mirrors: the primitive `Vertex` slots and their clip `Box`es, the glyph
  slots (`ftgl::glyph_vertex_t`) and theirs, and the per-slot byte diff that marks only
  a slot whose bytes changed (`markPrimitiveChanges`, the `DirtySlots` sets,
  `snapshotMirror`);
- the vertex packing of every style: `setVertex`, `writeRoundedQuad`, `drawIcon`,
  `drawLine`'s stroke placement, `drawTexturedRect`'s image units (`imageUnit`) and
  `cullOutOfRangeQuad`;
- the text runs and their slot allocator (`allocateTextRun`), and each atlas's draw list
  (`buildTextDrawList`, `setTextRunFont`), as `TextChunkDraw`s of index ranges;
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
| `createImageTexture(image, rgba)`, `destroyImageTexture(texture)`, `readTextureRgba(...)` | an image's mipmapped texture, its end, a read back (`pixelColor`, `readFontAtlas`) |
| `beginPrimitiveDraws()`, `drawPrimitiveChunk(chunk, firstSlot, slots)`, `endPrimitiveDraws()` | one draw per chunk of slots, in slot order |
| `beginTextDraws()`, `beginTextAtlas(font)`, `drawTextChunk(draw)`, `endTextDraws()` | per atlas with runs to draw: its texture and `TextMode` (an LCD one blended per channel), then one draw per `TextChunkDraw` |
| `submitFrame()` | the end of `flush()`: the frame's commands go to the device |
| `indexChunkSlotsChanged()` | `setIndexChunkSlots()` changed the chunk size |

`path` is `SlotPath::Primitives` or `SlotPath::Text`. A `TextureHandle` is the
backend's name for a texture (OpenGLCore's GL texture name; 0 is none): an atlas's is
kept in freetype-gl's `texture_atlas_t::id`, an image's in `SlotTextureData::m_handle`.
A `TextChunkDraw` is the ranges of one chunk's draw: an index count and the first
index's byte offset, from the chunk's first slot, per range (the form
`glMultiDrawElements` takes; a first index is the offset over `sizeof(uint16_t)`).
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
the one place that picks it. The three that check GL itself (`fontRelease.cpp`,
`glslDialects.cpp`, `textDrawLoop.cpp`) are the doctest test suite `gl_only`
(`tests/glRenderer.h`), which a run on another backend leaves out (`-tse=gl_only`).

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
  `glMultiDrawElements` for the text (core in GL 3.3; on GLES the
  `GL_EXT_multi_draw_arrays` one, `glMultiDrawElementsEXT`, whose entry point the GLES
  tree gets from `eglGetProcAddress`, since `libGLESv2` exports only the core ones;
  without it, each range of a draw is a `glDrawElements` of its own, which draws the
  same pixels and counts the same `FrameStats`; `GWENI_NO_MULTI_DRAW=1` at
  construction, or `setMultiDrawText(false)`, takes the loop where the context has it
  too, `tests/textDrawLoop.cpp`), and dual-source blending for LCD text (core in GL
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
  `OpenGLCore` does not use yet), so the text draws loop and LCD text is grayscale. In the
  build container the context is ANGLE on Chromium's SwiftShader, a CPU rasteriser
  like llvmpipe but another one, so its pixels are close to the native GLES tree's, not
  the same: ButtonDemo differs by at most 2 per channel, on 0.56 % of its pixels.
  A capture is one blocking `main`: it renders into the offscreen framebuffer and
  reads it back with `glReadPixels`, which WebGL does synchronously, so it needs no
  main loop. The interactive window is emscripten-glfw's (Emscripten's
  `contrib.glfw3` port) on the same `#canvas`, with a WebGL 2 context of its own
  (`capture::initGlfwContext`: `GLFW_CONTEXT_VERSION_MAJOR` is the WebGL version there;
  24 depth bits, no alpha, so nothing behind the page shows through a translucent
  blend, no stencil, no multisampling). A browser cannot run a blocking loop and still
  draw or deliver input, so `capture::runWindowLoop`, the one loop both apps' windows
  run, is `emscripten_set_main_loop_arg` there: one frame per animation frame, each a
  `glfwPollEvents()` (where emscripten-glfw applies a resize; input arrives between
  callbacks) and the frame (`updateCanvas`, `glClear`, `renderCanvas`, as natively,
  where the loop adds the swap and the poll). It never returns, and simulating the
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
  - the clipboard: an in-process buffer, so copy and paste work inside the page but
    not with other programs, said once when it is first used;
  - LCD text: no dual-source blending, so grayscale, with the one warning of "GLES
    tree" above.

  Touch, pointer events and an IME are not handled: a page loads on a phone, but only
  the mouse and the keyboard drive it. Building for wasm32 also showed two struct layouts that only agreed on
  x86-64 by luck, both fixed: freetype-gl's `vertex_buffer_t` with and without
  `FREETYPE_GL_USE_VAO` (now a PUBLIC definition of the `freetype-gl` target), and
  `json.h`'s `returnValue<uint64_t>`, which on x86-64 was the `size_t` specialisation.
- **The shaders' dialect is chosen at run time.** Each stage is one body shared by
  every dialect, behind a header `OpenGLCore::shaderSource` puts in front:
  `#version 330 core`, or for an ES context `#version 300 es` with `precision highp`
  for `float`, `int`, `sampler2D` and `usampler2D` (ES's fragment defaults, mediump
  int and lowp samplers, would not give the desktop bytes), plus the clip-box
  texture's width (`CLIP_COLUMNS`) and, for the text program with dual-source
  blending, `GWENI_DUAL_SOURCE` (and on ES `#extension GL_EXT_blend_func_extended`).
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
  textured Separator's quad reaches one unit past it over its neighbours), `6` = SDF bilinear fill (`drawBilinearRect`: each vertex carries its
  own top-edge color in color0 and bottom-edge color in color3; both are linear in x,
  so the blend along y is exact, with no crease on the quad's diagonal), `7` = SDF
  checker composite (`drawCheckerRect`: color0 with its alpha over 4 px cells of
  color1 (light) and color3 (dark), output opaque, so it does not depend on draw
  order). Styles 4, 6 and 7 share the rounded mask, anti-aliasing and border
  (`roundedRect()` in the fragment shader). The edge and the border are a 1 px box
  filter: coverage `clamp(0.5 - d/aa, 0, 1)` and border mix
  `clamp(0.5 + (border + d)/aa, 0, 1)`, with `aa` the pixel's size from `vLocal`'s
  derivatives (not `fwidth(d)`, which is √2 too wide on a corner diagonal and at a
  square corner's kink). A pixel-aligned straight edge is therefore 1 inside and 0
  outside: the outermost pixel is fully covered, two rects that abut meet with no
  seam, and an N px border is exactly N rows; only a corner arc is partly covered.
  `renderer::Base`'s defaults for `drawBilinearRect`/`drawCheckerRect` approximate
  them with `drawRoundedRect`. `hidePrimitive()`/`showPrimitive()` just set `style`,
  leaving geometry in place.
- **Line segments (style 3).** `drawLine(id, p0, p1, thickness, z)` draws a segment
  with round caps (a capsule) in the draw colour, its endpoints in canvas units as
  `PointF`s, so a plot's samples can fall between whole units. The thickness is
  snapped like a border, `max(1, snap(thickness))` whole device px, and each endpoint
  coordinate is placed where its stroke across starts on a whole px,
  `floor(v*scale - T/2 + 0.5)` (for an even `T` that is `snap(v) - T/2`), so a
  horizontal or vertical segment is `T` whole fully covered rows (columns), like a
  rule. The quad is both endpoints' stroke squares and 1 px around them. `lx/ly` run
  over it in half-pixel units as for style 4, the two endpoints are stored the same way
  relative to the quad's centre in color1 (`cx/cy`) and color3 (`dx/dy`) as `int16_t`
  bits (the vertex shader sign-extends them into the flat `vSegment`), and `border` is
  `T`. The fragment shader covers a pixel by the part of it between the stroke's two
  edges, each a straight edge across the pixel normal to the direction from the
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
  it, and only the rim of a cap's disc can be blended twice. A quad wider or higher
  than `lx/ly` hold (32767 half px) is collapsed like one past the vertex range.
  `renderer::Base`'s default draws a filled rect along the segment's longer axis, its
  extent there and `round(thickness)` across, centred on its middle: exact only for an
  axis-aligned segment without its caps. `tests/lines.cpp` checks style 3 pixel by
  pixel against a model of the placement and the coverage.
- `drawFilledRect()`/`drawTexturedRect()` write the 4 vertices + 6 indices for a quad
  into that id's slot in the CPU mirror.
- **Images (style 2) in the one draw.** `drawTexturedRect` takes a texture loaded on its
  own (`loadTexture`, e.g. an `ImagePanel`'s `gwen.png`), not a region of the skin atlas.
  Its UVs are 0..1 and stored in `tx/ty` as 0..65535; the draw colour is its tint (color1,
  alpha included), and `radius` holds the slot's **image unit**: one of 8 texture units
  (`GL_TEXTURE2` on, `SlotRenderer::ImageUnits`), all bound for the primitive draw, which
  the fragment shader picks with a constant-indexed switch over `images[8]`. A texture
  takes a unit on its first draw and keeps it until `freeTexture`, because a slot keeps
  its unit across frames. With all 8 in use, a further texture draws the missing image
  (`drawMissingImage`, a magenta `colors::Missing` fill) and logs once. A fully transparent texel is discarded,
  so it writes no depth; a partly transparent one still depends on slot order, which is
  why `ImagePanel` takes its primitive id on its first render, after its parent's skin
  took its own. Textures are mipmapped and clamped to edge.
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
  runs before it has (`Base::focus`), and a hidden box loses the focus with no call at
  all (`input::onCanvasThink`).
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
- **Menu rows.** A `Menu` is one rounded backdrop (the textured `Menu` renderer, the
  `Menu` surface) with flat rows on it; a `MenuStrip` has its own renderer, found
  before `Menu`'s by the type walk, painting the flat, square `MenuStrip` surface. A
  `MenuItem` has its own renderer too, found before `Button`'s: like a `Selectable` row
  its one slot is **hidden** while idle or disabled, and paints `MenuHover` over the
  item while it is hovered, pressed or has its submenu open (`isMenuOpen()`). The
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
- **The virtual list: a pool of the rows on screen.** `controls::VirtualList`
  (`include/gweni/controls/virtualList.h`) shows `setItemCount(n)` fixed-height rows
  (`setRowHeight`, 20 by default) through a pool of row controls: `ceil(viewport/rowHeight)+1`,
  made by the row factory (`setRowFactory`; by default `internal::VirtualListRow`, a
  `Selectable` that never stays selected, since the row shows another item once the list
  scrolls, and fires `onItemPressed` with its item) and filled for an item by the row
  binder (`setRowBinder(row, index)`). The pool is created or deleted only when the
  viewport's height changes, so 100 000 items have the controls, primitive ids and text
  ids of the dozen or so rows on screen, and a frame costs what those rows cost.
  **Positions stay small:** nothing is `n*rowHeight` high. The list is a `ScrollControl`
  with *virtual content* (`ScrollControl::setVirtualContentHeight`): the vertical bar is
  decided and ranged from that height, but the inner panel is the viewport, not scrolled,
  and the scroll position is a whole-unit offset (`getVerticalScrollOffset`, 0 to
  `getMaxVerticalScrollOffset()`), which the bar shows as a fraction and which a bar the
  user moves (thumb, buttons, wheel) sets, as `placeContents` finds the bar where it did
  not put it. Each row is placed in the viewport at `index*rowHeight-offset`, so every
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
  before the first one and in the frame of a resize. `scrollByRows`,
  `firstVisibleItem`, `rowForItem`/`itemForRow` and `refresh`/`refreshItem` are the
  rest; a wheel notch scrolls `setWheelRows` rows (3). VirtualListDemo holds 100 000
  items.
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
  (see "Hybrid text"), while `measureText` stays in canvas units.
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
  `Base::onScaleChanged` on every control, which invalidates its layout and marks it
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
  texture: a coverage font per (face, px, bold) below the SDF threshold, one SDF font
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
  (`setTextRunFont`), unless an `ensureFont`/`loadFont` uses it again first. That
  matters most for an SDF atlas, which every size of the face shares, but holds in
  every mode. A font loaded only by a use (`ensureFont`, from `renderText` or
  `measureText`) has no owner and stays cached until a `freeFont` asks for it.
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
  `m_failedFonts`, its text drawing nothing and measuring `(0,0)`.
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
  `measureText` and `renderText` both lay out through `text_buffer_add_text`, so the
  measured width (the final `pen.x`) is the drawn advance. The measured height is the
  line box, `ceil(font->height)` (ascender-descender, no line gap) whatever the string;
  in an SDF font `ceil(font->height*px/48)` for one line. `lineHeight(font)` reads that
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
  `measureText` lays out through the same `text_buffer_add_text`, so its width (the
  final pen, rounded up) is the drawn one; at a canvas scale an LCD run gets no
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
  1.5), the atlas fill and the contrast curve. A Vulkan backend would need the
  `dualSrcBlend` device feature and fall back to grayscale without it.
- **Hybrid text.** With the effective size `px = textPx(font) = font.size*scale()`,
  text below `GWENI_SDF_TEXT_MIN`
  (default 18 px; `0` = SDF everywhere, a large value = coverage everywhere) keeps the
  hinted grayscale coverage atlas (above) of its (face, px, bold), byte for byte as before. At or
  above it, a font resolves to its face's one R8 signed-distance-field atlas (1024²,
  rasterised by FreeType's `FT_RENDER_MODE_SDF` at 48 px with a 6 px spread, unhinted),
  shared by every size. `layoutSdfText` scales that atlas's metrics and unhinted
  advances by `px/48` and puts the baseline on a whole pixel, for `measureText` and
  `renderText` alike. The text shader's SDF mode anti-aliases the 0.5 edge over about a
  screen pixel (`smoothstep` with `w` from the field's gradient) and discards where it
  draws nothing, so a glyph's padded quad writes no depth. A size change inside the SDF
  range therefore re-lays only that run: its vertex slots are uploaded and nothing else
  (the glyph indices are static).
- **Text at a canvas scale.** `renderText` resolves and lays a run out at
  `px = font.size*scale()` (11 px UI text at scale 2 is 22 px, in the SDF range), from
  its origin snapped to a whole device pixel by `translate()`; a `TextRun` keeps the
  `px` it was laid out at, so a run drawn again after a scale change is re-laid even in
  the SDF atlas both sizes share. `measureText` measures at `font.size`, in the font that
  size resolves to at scale 1, so it returns **canvas units** and a layout is the same
  at every scale (`controltest_scale_bounds`). This deliberately differs from the legacy
  OpenGL and Software renderers, which return scaled pixels. Where `font.size` is a
  coverage size, its hinted whole-pixel advances are not the drawn font's scaled, so
  `measuredPenShifts` moves each drawn glyph to its measured pen position times the
  scale (rounded to a whole pixel for a coverage draw): the run ends where the layout
  sized it, and a caret placed from `measureText` falls between the glyphs it means. An
  SDF `font.size` needs no shift, its layout being linear in px. At scale 1 nothing
  of this changes a byte. `tests/scaledText.cpp` pins it.
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

## The theme file and strict mode

The textured skin's colours are compiled in (Tokyo Night): the SDF surface fills are
mutable globals in `tokyoNight.h`, named in `tokyonight::surfaceRegistry()`, and the
text colours are set by `TexturedSkin::setThemeColors()`. An optional `"theme"` object
in `DefaultSkin.json` overrides them without recompiling: `"surfaces"` maps a surface
name to any of `top`, `bottom`, `border` (`[r,g,b]` or `[r,g,b,a]`), `radius` and
`borderWidth`, and `"textColors"` maps a name in `TexturedSkin::textColorRegistry()`
(`LabelDefault`, `ButtonNormal`, ... 14 of them; the map points into that skin's
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
set by its bare name and one with some of them as `Name.field` per missing field:

```
theme strict: 57 of 59 surfaces not set: ButtonDisabled, ButtonDown, ButtonHover.border, ...
theme strict: 12 of 14 text colors not set: ButtonDisabled, ButtonDown, ...
```

(`theme strict: all 59 surfaces set` for a kind with no gaps, and a single
`theme strict: all 59 surfaces and 14 text colors set` when there are none at all). A
rejected value counts as not set. A surface that is transparent at rest on purpose
(`WindowButton`, `SelectableHover`: a transparent fill hides the slot) is magenta too
until the theme sets it to `[0,0,0,0]`; the theme has not said what it looks like. The
gaps are deliberate, so they are not a failure: `init` stays `true` and a capture exits
0. A `"strict"` that is not a boolean is reported and taken as `false`. The shipped
`DefaultSkin.json` does not set it, and without it nothing changes. Like every theme
value, it paints the process-global surfaces, which a later `init()` does not reset.

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
  which nothing draws. Before (#323) a buffer grew to exactly the mirror, and growing
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
  makes `measureText` print its freetype-gl vertex run, the resolved
  `texture_glyph_t` metrics and the size it returns, and `renderText` dump the
  `m_textVertexes` slots it wrote for each matching string, with each slot's index
  chunk and whether its font's draw list draws it. Off unless
  the variable is set; it is verbose (tens of lines per string).
- **Clipping.** Each control with `shouldClip()` (the default) narrows the clip region
  for its children: `Base::renderRecursive` calls `addClipRegion` with its bounds at its
  absolute origin, which intersects them with the parent's region. A control's own
  `render()` runs before that, so it is clipped by its parent's region and its children by
  its own; `renderOver()`/`renderFocus()` get the parent's region back. A control that
  returns false (`TabStrip`, `TabButton`, and `Text`, whose children are its own wrapped
  lines) hands its children its parent's region. A control can also narrow one child's
  region to a rect of its own (`Base::getChildClipRect(child)`, none by default): a
  ScrollControl gives its viewport for its inner panel, so content scrolled partly out of
  view is cut at the viewport and never draws in the padding and margin band around it,
  while its bars keep their own bounds. Hit-testing (`getControlAt`) skips a child outside
  that rect the same way, so a click in the band over a row scrolled out of view does not
  reach the row. OpenGLCore stamps the region into every
  slot drawn under it and the shaders discard outside it (see "Clip boxes" above). A
  region only changes when an ancestor's bounds do, and `onBoundsChanged` marks the whole
  subtree (`StateChange_Geometry`), so a moved ScrollControl panel or a dragged Window
  re-stamps exactly its own subtree and an unchanged box costs no upload. A new viewport
  that moves neither the ScrollControl nor its inner panel (a larger bottom padding over
  taller content) is marked by `placeContents` itself. An intersection
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

**Not yet done (the performance goal)**
- **Clipping leftovers.** A fully clipped subtree with changes in it is still walked and
  its primitives still written (with an empty box) and drawn (every fragment discarded);
  nothing culls them on the CPU. Clipping is rectangular only: a rounded parent does not
  cut its children at its corners.
- **Cache-to-texture** (`ICacheToTexture`) and free-slot compaction/defragmentation of
  the primitive buffers are stubbed.

**Vulkan (planned, #368)**
- A second `SlotRenderer` backend: it implements the hooks ("The shared core") with
  device-local vertex buffers indexed by primitive id, the dirty ranges pushed through a
  staging buffer, and a command buffer recording one draw per chunk. The mirrors, the
  packing, the text and the counts are the core's, so no control/skin/layout change is
  required and `FrameStats` counts the same.

See also [layout.md](layout.md) for the docking/layout system, and the skin/primitive
registration notes in `source/skins/textured/`.
