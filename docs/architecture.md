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
`Canvas::doThink()` runs the delayed deletes, `anim::think()` (the animations, kept per
control: a `ProgressBar`'s cycle, a `TextBox`'s caret blink), the layout passes, which
stop at controls with nothing to lay out (`Base::recurseLayout`), and
`input::onCanvasThink`. The z indices are numbered depth-first, each control taking its
skin Control's `depthLayers()`, and that numbering only changes with the tree order, so
the canvas renumbers only on a frame after something called `Canvas::invalidateZOrder()`.
`Base` does for each order change: `addChild`/`addChildOfLayout`/`removeChild` (so a
reparent through `setParent` and a delete, delayed or not, too), `bringToFront`,
`sendToBack`, `bringNextToControl`, and a skin Control assigned or released (`setSkin`,
whose new Control may report other `depthLayers()`). A control off any canvas reports
nothing; attaching it is an `addChild` on the canvas's tree. A renumber is a full one and
marks `StateChange_Geometry` on exactly the controls whose z index changed.
`UpdateStats::zVisited` (`Canvas::getUpdateStats()`, `ControlTest --stats` prints it as
`z_visited=`) counts the controls it entered: Demo's first frame 2668, a settled one 0.
`tests/unit/updateWalk.cpp` pins it.

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
child: `Canvas::keepToolTipLast()` moves it back to the end of the child list just
before each z renumber. Every change of the child order (a `Menu` added or opened with
`bringToFront`, a `Modal`, any `addChild`) asks for a renumber anyway, so this costs
nothing on other frames, and the depth-first numbering then gives the box and its text
the highest z values in the tree. A reserved z range above the tree was the other
choice; it would need a ceiling that no tree reaches and a second numbering for the
overlays, while the last-child rule reuses the one numbering there is. A later
canvas-level overlay (a context menu, the drag-and-drop preview) can take the same
place.

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
before the offset and clip push and the copy of its children. So a settled frame
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

## The renderer abstraction

A renderer implements `gweni::renderer::Base` (`include/gweni/platforms/baseRender.h`).
The core never makes API calls directly — it only calls this interface. Key methods:

| Concern | Methods |
| --- | --- |
| Frame | `begin()`, `end()`, `setViewRect()` |
| Primitive lifetime | `generatePrimitive(id)`, `releasePrimitive(id)` |
| Geometry | `drawFilledRect(id, rect, z)`, `drawTexturedRect(tex, id, rect, z, uv…)`, `drawRoundedRect(…)`, `drawBilinearRect(…)`, `drawCheckerRect(…)`, `drawIcon(…)` |
| Visibility | `showPrimitive(style, id)`, `hidePrimitive(id)` |
| Text | `generateTextPrimitive(id)`, `renderText(id, font, pos, z, text)`, `showText/hideText`, `measureText()` |
| Color / clip | `setDrawColor()`, `startClip()/endClip()`, `setClipRegion()` |

The unit of retained state is the **primitive id**: the core asks the renderer to
allocate ids (one control owns N of them, declared by `skin::Control::requiredPrimitives()`),
then references those ids when emitting/updating/hiding geometry. How a primitive id
maps to actual GPU resources is entirely the renderer's business.

A backend can be **immediate** (just draw in each `drawFilledRect` call; ignore the
retained-update benefit) or **retained** (the primitive id indexes into a persistent
GPU buffer). The same control/skin code drives both.

## OpenGLCore renderer (retained model)

`source/platforms/renderers/OpenGLCore/openglCore.cpp` implements the retained model
on an OpenGL 3.3 core profile:

- One VAO + one vertex buffer (`m_vertexBuffer` / CPU mirror `m_vertexes`) + one index
  buffer (`m_indexBuffer` / `m_indexes`). Text has its own freetype-gl-backed buffer
  (`m_textVertexes`).
- **A primitive id is a fixed slot in those buffers**: vertices live at `id*4` (a quad),
  indices at `id*6`. `generatePrimitive()` pulls a free id from a bank
  (`getPrimitiveId`, `include/gweni/primitiveIds.h`) and grows the CPU buffers.
- Each `Vertex` is 16 16-bit fields, 32 bytes:
  `{ style, x, y, z, tx, ty, lx, ly, radius, border, cx, cy, bx, by, dx, dy }`.
  `x, y` are the corner's device-pixel position, **signed** (`int16_t`, read as an
  `ivec3` with `z`), so a rect that straddles the view's left or top edge keeps its
  negative corner and draws exactly its on-view part; `z` is 0..10000, the
  projection's depth range. A quad is drawn only if it fits in ±32 K device px:
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
  color (color0), `2` = image (`drawTexturedRect`, below), `3` = unused (it was a text glyph; text has its own
  program, below), `4` = SDF rounded rect
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
- `drawFilledRect()`/`drawTexturedRect()` write the 4 vertices + 6 indices for a quad
  into that id's slot in the CPU mirror.
- **Images (style 2) in the one draw.** `drawTexturedRect` takes a texture loaded on its
  own (`loadTexture`, e.g. an `ImagePanel`'s `gwen.png`), not a region of the skin atlas.
  Its UVs are 0..1 and stored in `tx/ty` as 0..65535; the draw colour is its tint (color1,
  alpha included), and `radius` holds the slot's **image unit**: one of 8 texture units
  (`GL_TEXTURE2` on, `OpenGLCore::ImageUnits`), all bound for the primitive draw, which
  the fragment shader picks with a constant-indexed switch over `images[8]`. A texture
  takes a unit on its first draw and keeps it until `freeTexture`, because a slot keeps
  its unit across frames. With all 8 in use, a further texture draws the missing image
  (`drawMissingImage`, a red fill) and logs once. A fully transparent texel is discarded,
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
  it draws in front of its whole subtree. A text run's glyph i is at
  `z+(i+1)/(glyphs+1)` (`OpenGLCore::renderText`, the move branch included), strictly
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
  the focus arriving) that raises `StateChange_Caret` alone, which shows or hides the
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
  runs from `setText` (`PasswordTextBox`'s too, which shows the hint unmasked),
  `setHidden` (showing a box shows every child) and `updateColors()`, which every focus
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
- **The indeterminate ProgressBar.** `ProgressBar::setIndeterminate(true)` draws a
  segment (`setSegmentLength`, 0.3 of the track) in the fill's slot instead of the fill,
  moving start to end (bottom to top when vertical) at `setIndeterminateSpeed` track
  lengths a second (0.75), sliding out past the end and in again at the start; it starts
  wholly on the track at its start. The bar's existing animation (`ProgressBarThink`,
  `platform::getTimeInSeconds()`, so the stepped clock in captures) advances its phase,
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
  where they are pixels (X11, Windows). `Canvas::setScale` reaches
  `Base::onScaleChanged` on every control, which invalidates its layout and marks it
  `StateChange_Geometry` once, so the next frame lays the tree out again and re-emits
  every primitive and clip box at the new scale; the text runs re-lay themselves,
  their `px` being in the run cache key. `ControlTest --scenario scale`
  (`controltest_scale_roundtrip`, every control) checks that 1 -> 2 -> 1 comes back
  byte for byte and that the 2x frame is a canvas built at scale 2.
- **Clip boxes.** Every primitive slot and every glyph slot carries the `clipRegion()`
  current when it was drawn, as a `Box` of four `uint16_t` corners (`m_clippingBoxes`,
  `m_textClipBoxes`), its edges snapped like the vertices' and clamped to `[0, 65535]` with
  `x1 >= x0`, `y1 >= y0`, so an empty region is an empty box. Each list is uploaded
  like the vertices (dirty slots only, compared by bytes) and read by the fragment
  shaders as a `GL_RGBA16UI` buffer texture, then `discard` outside
  `[x0,x1) x [y0,y1)`. The primitive shader fetches it at `gl_PrimitiveID >> 1`, which
  is the slot only because the primitive buffer is drawn with **one `glDrawElements`
  from index 0** and every slot is exactly 4 vertices / 6 indices (2 triangles), hidden
  and degenerate ones included; splitting or offsetting that draw breaks it. The text
  shader takes the slot from the vertex instead (`gl_VertexID >> 2`, flat), because the
  text is split into one draw per atlas (below). `startClip()`/`endClip()` do nothing in this
  renderer. `GWENI_CLIP_DEBUG=<inset px>` shrinks every box by the inset on each side,
  which shows the boxes on screen.
- **One text draw per font atlas.** Every loaded font has its own freetype-gl atlas
  texture: a coverage font per (face, px, bold) below the SDF threshold, one SDF font
  per (face, bold) at or above it (see "Hybrid text"). Each `TextRun` records the `GLFontData` it was laid out in,
  and each `GLFontData` keeps the text ids of its runs (`textRuns`). `flush()` binds
  each atlas in turn and draws its runs with one `glMultiDrawElements` over their
  existing index ranges (hidden and empty runs left out, adjacent ranges merged, in
  slot order), with the atlas's `GLTextMode` in the `textMode` uniform. The slots are
  never moved for this, so the dirty-slot upload is the same with one font or many.
  `freeFont` of a coverage font takes its runs off every list (they keep their slots
  and draw nothing); their next `renderText` lays them out again in the font loaded
  anew. An SDF atlas is shared by every size of the face, so `freeFont` of one size
  deletes it only when no run is laid out in it. Otherwise it marks it `freed`, every
  run keeps drawing, and it goes when its last run leaves it (`setTextRunFont`), unless
  an `ensureFont`/`loadFont` uses it again first.
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
  measured width (the final `pen.x`) is the drawn advance. The text shader outputs
  `vec4(color.rgb, color.a * coverage)` under the `SRC_ALPHA, ONE_MINUS_SRC_ALPHA`
  blend, so coverage is applied once; no gamma or contrast curve is applied
  (`markup.gamma` is 1). `tests/textGrayscale.cpp` pins all of this.
- **LCD subpixel text (opt-in).** `OpenGLCore::setTextAA(TextAA::{Grayscale, LcdRGB,
  LcdBGR})`, or `GWENI_TEXT_AA=gray|rgb|bgr` at construction; Grayscale by default, and
  then every output is the grayscale path's byte for byte. In an LCD mode a coverage-range
  font resolves to `GLTextMode::LcdRGB`/`LcdBGR` (the mode is part of the `GLFontKey`, so
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
  grayscale in every mode. `setTextAA` at runtime replaces every coverage-range font with
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
  (`writeTextIndexes` marks only index slots whose bytes change).
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

## Current state vs. goals

What exists today (the foundation) and what's still aspirational:

**Implemented**
- Decoupled core ↔ renderer via `renderer::Base`; OpenGLCore is the only backend.
  `StubRenderer` (`tests/unit/headlessFixture.h`) is a second implementation, compiled
  in every build with tests on (the default), so the interface stays implementable
  outside OpenGL.
- Two-phase `updateCanvas` / `renderCanvas` split.
- State-change tracking; unchanged controls emit no renderer calls, and the render
  walk skips every subtree with nothing pending in it, so a settled frame visits no
  control (`ChangeStats::visited`).
- Persistent primitive-id-indexed CPU vertex/index buffers in OpenGLCore; per-control
  primitive ownership via `skin::Control`.
- **Incremental GPU upload.** `OpenGLCore::flush()` tracks which primitive slots were
  written and uploads only those, coalesced into contiguous `glBufferSubData` ranges;
  when nothing changed it skips the transfer and just draws the buffer already on the
  device. The buffers are sized with a grow-only `glBufferData`, which also re-sends
  everything for that one frame. A slot is marked dirty only when its bytes actually
  change, so a control that redraws itself identically (the skin re-emits unchanged
  controls every frame) costs nothing. `GWENI_UPLOAD_STATS=1` prints the per-frame
  upload call/byte counts (the clip-box buffers included).
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
  `m_textVertexes`/`m_textIndexes` slots it wrote for each matching string. Off unless
  the variable is set; it is verbose (tens of lines per string).
- **Clipping.** Each control with `shouldClip()` (the default) narrows the clip region
  for its children: `Base::renderRecursive` calls `addClipRegion` with its bounds at its
  absolute origin, which intersects them with the parent's region. A control's own
  `render()` runs before that, so it is clipped by its parent's region and its children by
  its own; `renderOver()`/`renderFocus()` get the parent's region back. A control that
  returns false (`TabStrip`, `TabButton`, and `Text`, whose children are its own wrapped
  lines) hands its children its parent's region. OpenGLCore stamps the region into every
  slot drawn under it and the shaders discard outside it (see "Clip boxes" above). A
  region only changes when an ancestor's bounds do, and `onBoundsChanged` marks the whole
  subtree (`StateChange_Geometry`), so a moved ScrollControl panel or a dragged Window
  re-stamps exactly its own subtree and an unchanged box costs no upload. An intersection
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
- **The update phase visits only what changed.** No control `think()`s (the animations
  run from `anim::think()`), and the z indices are renumbered only on a frame after the
  tree order changed (`Canvas::invalidateZOrder()`), so a settled frame enters no
  control in `updateCanvas()` either (`UpdateStats::zVisited`, see "updateCanvas()").

**Not yet done (the performance goal)**
- **Clipping leftovers.** A fully clipped subtree with changes in it is still walked and
  its primitives still written (with an empty box) and drawn (every fragment discarded);
  nothing culls them on the CPU. Clipping is rectangular only: a rounded parent does not
  cut its children at its corners.
- **Cache-to-texture** (`ICacheToTexture`) and free-slot compaction/defragmentation of
  the primitive buffers are stubbed.

**Vulkan (planned)**
- Implements the same `renderer::Base` interface: own device-local vertex/index buffers
  indexed by primitive id, push dirty ranges through a staging buffer, and record/submit
  a command buffer that draws the index range. No control/skin/layout changes required.

See also [layout.md](layout.md) for the docking/layout system, and the skin/primitive
registration notes in `source/skins/textured/`.
