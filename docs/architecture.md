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

## The renderer abstraction

A renderer implements `gweni::renderer::Base` (`include/gweni/platforms/baseRender.h`).
The core never makes API calls directly — it only calls this interface. Key methods:

| Concern | Methods |
| --- | --- |
| Frame | `begin()`, `end()`, `setViewRect()` |
| Primitive lifetime | `generatePrimitive(id)`, `releasePrimitive(id)` |
| Geometry | `drawFilledRect(id, rect, z)`, `drawTexturedRect(tex, id, rect, z, uv…)` |
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
- Each `Vertex` is `{ style, x, y, z, tx, ty }` packed as 16-bit ints. `style` selects
  the shader path: `0` = hidden (degenerate), `1` = solid color (color packed into
  `tx/ty`), `2` = textured, `3` = text glyph. `hidePrimitive()`/`showPrimitive()` just
  set `style`, leaving geometry in place.
- `drawFilledRect()`/`drawTexturedRect()` write the 4 vertices + 6 indices for a quad
  into that id's slot in the CPU mirror.
- Because the CPU mirror is persistent, a control that didn't change leaves its slot
  untouched between frames; its geometry is still in the buffer.
- A single shader pair (vertex + fragment) draws all non-text primitives; an ortho
  projection (`setViewRect()`) maps UI pixel coords to clip space; `z`/depth gives
  draw order.

## Current state vs. goals

What exists today (the foundation) and what's still aspirational:

**Implemented**
- Decoupled core ↔ renderer via `renderer::Base`; OpenGLCore, OpenGL and SDL2 backends.
- Two-phase `updateCanvas` / `renderCanvas` split.
- State-change tracking; unchanged controls emit no renderer calls.
- Persistent primitive-id-indexed CPU vertex/index buffers in OpenGLCore; per-control
  primitive ownership via `skin::Control`.
- **Incremental GPU upload.** `OpenGLCore::flush()` tracks which primitive slots were
  written and uploads only those, coalesced into contiguous `glBufferSubData` ranges;
  when nothing changed it skips the transfer and just draws the buffer already on the
  device. The buffers are sized with a grow-only `glBufferData`, which also re-sends
  everything for that one frame. A slot is marked dirty only when its bytes actually
  change, so a control that redraws itself identically (the skin re-emits unchanged
  controls every frame) costs nothing. `GWENI_UPLOAD_STATS=1` prints the per-frame
  upload call/byte counts.
- Window resize (`renderer->setViewRect()` + `canvas->setSize()`), see
  `examples/openglcore/openglcore.cpp`.

**Not yet done (the performance goal)**
- **Clipping.** Per-control scissor / per-primitive clip boxes are stubbed/commented
  (`shouldClip()`, `m_clippingBoxes`, `startClip/endClip`).
- **Cache-to-texture** (`ICacheToTexture`) and free-slot compaction/defragmentation of
  the primitive buffers are stubbed.

**Vulkan (planned)**
- Implements the same `renderer::Base` interface: own device-local vertex/index buffers
  indexed by primitive id, push dirty ranges through a staging buffer, and record/submit
  a command buffer that draws the index range. No control/skin/layout changes required.

See also [layout.md](layout.md) for the docking/layout system, and the skin/primitive
registration notes in `source/skins/textured/`.
