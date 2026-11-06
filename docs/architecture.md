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

`Canvas::updateCanvas()` runs `doThink()` then `updateRecursive()` over the tree
(`source/controls/canvas.cpp`, `source/controls/base.cpp`). This phase:

- processes input-driven state (hover/focus/press), animations and delayed deletes,
- runs layout/docking so every control has up-to-date bounds,
- assigns each control a draw order (`m_zIndex`),
- records *what changed* on each control as a **state-change bitmask**
  (`StateChange_Created | _Visibility | _Geometry | _Background | _Text`, in
  `include/gweni/controls/base.h`). A control with `StateChange_Nothing` needs no
  render work.

No rendering-API calls happen here.

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
  `tx/ty` is color0 (or the UV of style 2), `cx/cy` color1, `bx/by` the border color
  and `dx/dy` color3, each an RGBA color packed into two halves (`Color::rg()/ba()`).
  The shader reads color0 and color3 as smooth (interpolated) varyings and color1 and
  the border color as flat ones. `lx/ly` (the local position from the rect's centre),
  `radius`, `border`, color1, the border color and color3 are 0 except for the styles
  that use them. `style` selects the shader path: `0` = hidden (degenerate), `1` = solid
  color (color0), `2` = image (`drawTexturedRect`, below), `3` = unused (it was a text glyph; text has its own
  program, below), `4` = SDF rounded rect
  (`drawRoundedRect`: vertical gradient color0 → color1, border), `5` = SDF icon glyph
  (`drawIcon`), `6` = SDF bilinear fill (`drawBilinearRect`: each vertex carries its
  own top-edge color in color0 and bottom-edge color in color3; both are linear in x,
  so the blend along y is exact, with no crease on the quad's diagonal), `7` = SDF
  checker composite (`drawCheckerRect`: color0 with its alpha over 4 px cells of
  color1 (light) and color3 (dark), output opaque, so it does not depend on draw
  order). Styles 4, 6 and 7 share the rounded mask, anti-aliasing and border.
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
- **Scale and the device-pixel grid.** Controls lay out in logical units (integers);
  `renderer::Base::translate()` turns them into device pixels at `scale()` (set by
  `Canvas::setScale`). It snaps **edges**, not origin and size apart: a point is
  `snap(x+offset.x)`, a rect is `x0=snap(x+offset.x)`, `x1=snap(x+offset.x+w)`,
  `w=x1-x0`, and the same in y, where `Base::snap(v)` is `floor(v*scale+0.5)`. So
  two rects that meet in logical units meet on one device-pixel edge at any scale,
  with no seam or 1 px overlap. The SDF radius is `snap(radius)` and a border
  `max(1, snap(border))`, so a border stays a whole number of device pixels; icon
  strokes are relative to their rect and scale with it. At scale 1 all of this is
  the identity. Text glyphs are still rasterised at 1x; only their positions scale.
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
  horizontal position (`FT_LOAD_TARGET_LCD` stays for a depth-3 atlas). A glyph's
  advance comes from that same hinted load, in whole pixels, and kerning is rounded to
  whole pixels too, so the pen stays on whole pixels and identical glyph pairs get
  identical gaps; `text_buffer` places each quad at `round(pen.x) + offset_x`, as wide
  as its bitmap, so every pixel samples exactly one texel, and keeps the pen's
  fraction in the vertex's `shift` (zero while hinted) for a later subpixel mode.
  `measureText` and `renderText` both lay out through `text_buffer_add_text`, so the
  measured width (the final `pen.x`) is the drawn advance. The text shader outputs
  `vec4(color.rgb, color.a * coverage)` under the `SRC_ALPHA, ONE_MINUS_SRC_ALPHA`
  blend, so coverage is applied once; no gamma or contrast curve is applied
  (`markup.gamma` is 1). `tests/textGrayscale.cpp` pins all of this.
- **Hybrid text.** With `px = font.size*scale()`, text below `GWENI_SDF_TEXT_MIN`
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

**Not yet done (the performance goal)**
- **Clipping leftovers.** A fully clipped subtree with changes in it is still walked and
  its primitives still written (with an empty box) and drawn (every fragment discarded);
  nothing culls them on the CPU. Clipping is rectangular only: a rounded parent does not
  cut its children at its corners.
- **The update phase still walks the whole tree.** `Base::doThink()` calls `think()`
  on every control every frame, and `updateRecursive()` renumbers every z index.
- **Cache-to-texture** (`ICacheToTexture`) and free-slot compaction/defragmentation of
  the primitive buffers are stubbed.

**Vulkan (planned)**
- Implements the same `renderer::Base` interface: own device-local vertex/index buffers
  indexed by primitive id, push dirty ranges through a staging buffer, and record/submit
  a command buffer that draws the index range. No control/skin/layout changes required.

See also [layout.md](layout.md) for the docking/layout system, and the skin/primitive
registration notes in `source/skins/textured/`.
