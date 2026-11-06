# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

gweni ("GUI Without Extravagant Nonsense") is a C++17 retained-mode GUI library, forked from Garry Newman's **GWEN** (MIT) and reworked to be OpenGL-aware. The core idea separating it from GWEN: instead of redrawing every widget each frame in immediate mode, controls allocate persistent renderer **primitives** and only regenerate/update them when their state actually changes. The long-term goal is good performance on modern OpenGL (3.0+ core profile) and eventually Vulkan.

The library builds as a static/object library plus example executables. Tests are run with CTest (see "Tests" under "Building"; a short version for humans is in [`docs/building.md`](docs/building.md)).

## Building

**All builds and dependency operations run inside the build container — never directly on a developer's host.** The toolchain, vcpkg, and all build tooling live in the container.

### Which situation are you in? Test `VCPKG_ROOT`

Everything below depends on whether you are *outside* the container (a developer on their own machine) or *already inside* it. Check before you run anything:

```bash
[ -n "$VCPKG_ROOT" ] && echo "inside the container" || echo "on a host — go through run_docker.sh"
```

`VCPKG_ROOT=/opt/vcpkg` is baked into the image (`Dockerfile`), so if it is set you are inside the build container and already have cmake, the compilers and vcpkg. If it is unset you are on a host; `generate.sh` detects this itself and refuses to run, pointing you at `run_docker.sh`.

### Inside the container (`VCPKG_ROOT` is set)

Run the same two steps directly — there is no docker inside the container, so `run_docker.sh` cannot work here:

```bash
./generate.sh linux                   # configure build_linux_x64_debug/
cmake --build build_linux_x64_debug -j # build
```

This is the case for an automated agent run: the agent image is built FROM this repo's own `Dockerfile`, so the container is already the environment described here. The repo is at the working directory, not at `run_docker.sh`'s `/home/developer/gweni`. Whether the vcpkg cache is available depends on how the container was started: check `VCPKG_DEFAULT_BINARY_CACHE`/`VCPKG_DOWNLOADS` — if they are unset, nothing mounted `/vcpkg-cache` and the dependency build starts cold (slow, and not reused by the next run).

### On a host machine (`VCPKG_ROOT` is unset)

`run_docker.sh` builds an Ubuntu 22.04 image (`Dockerfile`), creates a container that mounts the repo at `/home/developer/gweni`, and either attaches a shell or runs a command you pass it:

```bash
./run_docker.sh                       # interactive shell in the container
./run_docker.sh ./generate.sh linux   # run a single command in the container
./run_docker.sh cmake --build build_linux_x64_debug -j
```

Flags (before the command): `-r` rebuild the image, `-s` force-restart the container.

This path always gets a persistent vcpkg cache: `run_docker.sh` creates `.vcpkg-cache/` in the repo and mounts it at `/vcpkg-cache`, setting `VCPKG_DEFAULT_BINARY_CACHE` and `VCPKG_DOWNLOADS` into it, so the slow first dependency build is reused across container runs. `.vcpkg-cache/` and `vcpkg_installed/` are git-ignored.

### `generate.sh` is how this project is configured

**The build always goes through `generate.sh`.** It only runs `cmake` **configuration** (it does not build), and it is what points CMake at the vcpkg toolchain (`$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake`), chainloads `cmake/linux_toolchain.cmake`, pins the `x64-linux` triplet and selects the renderer options. A hand-written `cmake -B ...` configure line skips all of that and produces a differently-configured tree — it is not a valid way to build this project, even inside the container.

The commands in this section are the in-container form; from a host, prefix each with `./run_docker.sh`.

```bash
./generate.sh linux                  # configures build_linux_x64_debug/ (SDL2+legacy GL off, GL core on)
./generate.sh linux release          # release build dir
./generate.sh linux -DSOME_OPT=ON    # any -D... arg is passed through to cmake
./generate.sh linux asan             # AddressSanitizer tree, build_linux_x64_debug_asan/ (see "ASAN tree")
```

Dependencies are managed by **vcpkg** (manifest `vcpkg.json` at the repo root), bootstrapped into the image at `/opt/vcpkg`; `generate.sh` triggers the manifest install at configure time.

Then build into the generated directory (named `build_<platform>_<arch>_<buildtype>`):

```bash
cmake --build build_linux_x64_debug -j
```

The Linux example binary lands in `build_linux_x64_debug/examples/openglcore/OpenGLCore` and must be run from the **repo root** (it loads `resources/DefaultSkin.json`, `resources/OpenSans.ttf` via a `CurrentPath("resources")` loader).

### Regenerating the UI samples

The sample PNGs under `docs/images/` (embedded by `README.md`) are checked in and regenerated by one script, run from the repo root inside the container after a normal build:

```bash
./tools/capture_samples.sh
```

It captures the hero demo (`OpenGLCore --demo`) and the gallery controls (`ControlTest --control`) offscreen, so no display is needed: it uses `xvfb-run` when the image has it and otherwise the examples' own surfaceless-EGL path. The sample list at the top of the script is the interface — edit it to change the set, and update the gallery table in `README.md` to match, since it embeds each PNG by path. Re-running on an unchanged tree produces byte-identical PNGs.

### Tests

The test suite is CTest, under `tests/`. After `cmake --build`, from the repo root:

```bash
ctest --test-dir build_linux_x64_debug            # everything
ctest --test-dir build_linux_x64_debug -L gpu     # only the tests that need a GL context
ctest --test-dir build_linux_x64_debug -LE gpu    # only the ones that don't (may be "no tests")
```

Tests labelled `gpu` run the examples headless (`--headless`, the surfaceless-EGL path), so an agent holds the GPU lock around `ctest` (`mm run --lock gpu -- ctest ...`), as for any ControlTest capture. `controltest_determinism` (`tests/determinism.cmake`) runs `ControlTest --shot-all --dump-bounds` in two separate processes and fails, naming each file, if any PNG or `.txt` listing differs between them. A demo that is known to differ is marked out in that script's `EXCLUDED_DEMOS` with its issue number. `controltest_teardown` runs `--check-teardown`. The tests are built only when `GWENI_BUILD_TESTS` is on (the default), and the ControlTest tests also need `GWENI_BUILD_EXAMPLES`.

C++ unit tests use **doctest** and all build into one executable, `gweni_unittests` (registered as the CTest test `unittests`, label `gpu`). Its sources are in `tests/`: `main.cpp` creates a surfaceless EGL context through the examples' `gweni_capture` library (`examples/openglcore/capture.{h,cpp}`), plus one init()'d `OpenGLCore` with OpenSans loaded, and then runs every case. A test reaches them through `test::gpuRenderer()` / `test::openSans()` (`tests/gpuRenderer.h`). `textLength.cpp` pins the UTF-8 code-point counts `OpenGLCore` passes to freetype-gl (the `cca344a` bug). It counts drawn glyphs with `getFrameStats().textSlotsWritten`. To add a test, write a `.cpp` in `tests/` with `#include <doctest/doctest.h>` and `TEST_CASE`s, and add it to `gweni_unittests`'s source list in `tests/CMakeLists.txt`. Do not define a second `main`. The executable is only built when `gweni_capture` exists (`GWENI_BUILD_EXAMPLES`). It must run from the repo root, because it loads `resources/`. Run one case with `build_linux_x64_debug/tests/gweni_unittests -tc="<name>"`, holding the GPU lock as for any other GPU job.

### ASAN tree

`./generate.sh linux asan` configures a second tree, `build_linux_x64_debug_asan/` (`asan` combines with `release` too: `build_linux_x64_release_asan/`), with `-DGWENI_SANITIZE=address`. It builds and tests like the plain one:

```bash
./generate.sh linux asan
cmake --build build_linux_x64_debug_asan -j
mm run --lock gpu -- ctest --test-dir build_linux_x64_debug_asan --output-on-failure
```

The sanitizer flags are set before `deps/` is added, so gweni, the examples, the tests and the vendored freetype-gl and imglib are instrumented. The vcpkg-installed dependencies are not. In that tree every test runs with `ASAN_OPTIONS=detect_leaks=0:abort_on_error=1` (`tests/CMakeLists.txt`): Mesa/EGL leak at exit and that is not ours, so leak checking is off and only memory errors fail a test. An ASAN report aborts the process, and that fails the test, including a ControlTest child of `determinism.cmake`. Run one binary by hand with the same `ASAN_OPTIONS`.

The heap-string case in `tests/textLength.cpp` is a real check only in this tree. If `OpenGLCore` passes freetype-gl a byte count again (the `cca344a` bug), ASAN reports `heap-buffer-overflow` in `deps/freetype-gl/text-buffer.c`. ControlTest's UnicodeTextDemo also triggers that report.

**ASAN does not catch an over-read that stays inside one object.** ListBoxDemo's original instance of this bug read past a short string held in SSO storage inside `controls::Text`, into the neighbouring `m_font` field. That memory is addressable, so ASAN and valgrind both stay silent. The deterministic trailing-bytes test in `textLength.cpp` exists for that case, and so does the two-process determinism test.

### Dependencies / vcpkg notes
- The active OpenGL-core build pulls `rapidjson`, `glm`, `freetype`, `glew`, `glfw3`, `libpng`, `libjpeg-turbo` (last two via the `deps/imglib` subproject). RapidJSON is header-only — CMake consumes `RAPIDJSON_INCLUDE_DIRS`, not an imported target.
- `doctest` (header-only, `find_package(doctest CONFIG)` → `doctest::doctest`) is used only by the unit tests in `tests/`, never by the `gweni` library.
- The SDL2 backend and the Android Boost.Filesystem backend are optional vcpkg **features** (`sdl2`, `boost-filesystem`); enable with `-DVCPKG_MANIFEST_FEATURES=sdl2`.
- The vendored `deps/freetype-gl` and `deps/imglib` are added via `add_subdirectory`; `deps/CMakeLists.txt` sets `freetype-gl_USE_HUNTER OFF` so they resolve deps through the vcpkg toolchain's standard find modules.

### Build options (CMakeLists.txt)
`GWENI_RENDER_SDL2`, `GWENI_RENDER_OPENGL` (legacy fixed-function), `GWENI_RENDER_OPENGL_CORE` (the active modern renderer), `GWENI_BUILD_EXAMPLES`, `GWENI_BUILD_TESTS` (CTest suite under `tests/`, default on), `GWENI_SANITIZE` (sanitizers to build everything with, passed as `-fsanitize=<value>`; empty by default, `address` in the ASAN tree that `./generate.sh linux asan` sets up). The Linux toolchain turns SDL2 and legacy GL **off** and uses only the GL core renderer. `GWENI_RENDER_OPENGL_CORE` is what pulls in glm + freetype + the `deps/freetype-gl` and `deps/imglib` subprojects.

## Architecture

> Full write-up in [`docs/architecture.md`](docs/architecture.md); summary below.

### Core goal: API-agnostic, change-tracked UI
The whole point of gweni is that **UI logic (layout, padding, docking, hover/highlight/focus, hit-testing) is decoupled from the rendering API**. The same control/skin code is meant to drive OpenGL today and Vulkan (or any backend) later, by implementing one abstract interface — `gweni::renderer::Base` (`include/gweni/platforms/baseRender.h`). The core never makes API calls directly; it only calls that interface.

The frame is two phases (`examples/openglcore/openglcore.cpp`):
1. **`canvas->updateCanvas()`** — logic only: think/input, layout/docking, z-order, and recording *what changed* per control as a state-change bitmask. No draw calls.
2. **`canvas->renderCanvas()`** — render: `renderer->begin()`, walk the tree, and for each control emit **only the draw-state changes needed**, then `renderer->end()`. A control with `StateChange_Nothing` issues no renderer calls.

A backend may be **immediate** (redraw everything each `drawFilledRect`) or **retained** (primitive ids index a persistent GPU buffer); the same UI code drives both. The performance target is that per-frame cost scales with what changed, not UI size.

### Namespaces & layout
- `gweni::controls` — all widgets, rooted at `controls::Base`. Headers in `include/gweni/controls/`, impls in `source/controls/`.
- `gweni::skin` — draws controls; `simple` (vector) and `textured` (atlas-based, the one the examples use) skins under `skins/`.
- `gweni::renderer` — `renderer::Base` abstract API; concrete renderers under `source/platforms/renderers/` (`OpenGLCore/`, `OpenGL/`).
- `gweni::platform` — OS abstractions (clipboard, cursor, file dialogs, timing, window creation) in `platforms/`.
- `gweni::event` — signal/slot system in `events.h`.

Public headers live under `include/gweni/...`; the directory tree there mirrors the `source/...` tree. Note the header path `include/gweni/controls/dialogs/` maps to source `source/controls/dialog/` (singular).

### The retained-primitive model (the important part)
This is what makes gweni different from upstream GWEN and is easy to get wrong:

1. Every `controls::Base` tracks a dirty bitmask via `addStateChange(...)` with flags `StateChange_Created / Visibility / Geometry / Background / Text` (defined in `controls/base.h`). `StateChange_Nothing` means nothing to regenerate.
2. Controls own persistent renderer **primitive IDs** (`primitiveIds.h`: `getPrimitiveId`/`getTextId`/`releasePrimitiveId`). The skin's `Control` objects (`skin.h`) declare `requiredPrimitives()` and have `generate()/update()/remove()` keyed off the control's `State` (`Generate`/`Update`/`Remove`).
3. In `OpenGLCore` a **primitive id is a fixed slot in one persistent VBO/EBO**: vertices at `id*4`, indices at `id*6` (`source/platforms/renderers/OpenGLCore/openglCore.cpp`). `update()` rewrites only that slot in the CPU mirror; unchanged controls leave their slots intact. A `Vertex` carries `{style,x,y,z,tx,ty}` plus SDF fields `{lx,ly,radius,border,cx,cy,bx,by}` (16-bit); `style` selects the shader path (0 hidden / 1 fill / 2 texture / 3 text / **4 SDF rounded-rect**); `show/hidePrimitive` just toggle `style`. Text uses a separate freetype-gl buffer keyed by text id.

### SDF rounded-rect path (the active visual style)
Style 4 is an analytic **signed-distance-field** rounded rectangle drawn in the fragment shader (`drawRoundedRect` on `renderer::Base`): vertical gradient fill + optional border, anti-aliased via `fwidth()`. Style 5 is an **icon glyph** (`drawIcon`): a stroked SDF checkmark or directional chevron (used for checkbox ticks, combo/scrollbar arrows). The SDF fields are zero for all other styles, so the fill/texture/text paths are unchanged. Overlay primitives (checkmark, radio dot, arrows) must be drawn at `getZIndex()+1` so the `GL_LESS` depth test doesn't reject them behind their own background. The textured skin drives it through `tokyonight::Surface` + `paintSurface()` (`include/gweni/skins/textured/tokyoNight.h`) and the `RoundedPrimitive` helper (`texturedPrimitives.h`); each control renderer's `update()` just picks a `Surface` for its state. The default theme is **Tokyo Night** (dark, blue-toned): surface fills are mutable globals in `tokyoNight.h`, text/line colors are set in code by `TexturedSkin::setThemeColors()`, and `renderText` bakes the control's draw color into the glyphs so per-control text colors apply. **The theme is swappable without recompiling**: an optional `"theme"` object in `resources/DefaultSkin.json` (`surfaces` + `textColors`) is loaded by `TexturedSkin::loadTheme()` and overrides any surface (by name, via `tokyonight::surfaceRegistry()`) or text color; omitted entries keep the compiled defaults. The legacy 9-slice atlas path (`BorderedPrimitive`) still exists but is no longer used by the converted controls.

When adding or modifying a control, the rule is: mutating anything visual must call the appropriate `addStateChange(...)` (or `addStateChangeRecursive`) so the primitive gets regenerated — otherwise the change won't appear. Conversely, don't redraw unconditionally; that defeats the whole design.

**Current state vs goal:** incremental upload is in place — `OpenGLCore::flush()` records which primitive slots were written, uploads only those as coalesced `glBufferSubData` ranges, and skips the transfer entirely when nothing changed (the draw still happens every frame). A slot counts as changed only if its bytes differ, because the skin re-emits unchanged controls every frame. Buffers are sized by a grow-only `glBufferData`, which re-sends everything for that frame. Glyphs outside the ASCII cache are rasterised lazily into the CPU font atlas, and `flush()` uploads the rows they touched (a dirty row band added to the vendored freetype-gl's `texture_atlas_t`) before drawing. `GWENI_UPLOAD_STATS=1` prints per-frame upload calls/bytes (atlas uploads separately); `GWENI_TEXT_DEBUG=<substring>` dumps `measureText`'s glyph metrics and the text mirror slots for matching strings. A Vulkan backend would do the same with device-local buffers + a staging upload. Still open: clipping, cache-to-texture, and free-slot compaction — see `docs/architecture.md` → "Current state vs. goals".

### Controls
- Construct via the canvas: `canvas->newChild<SomeControl>()` or the `newControl<T>(name)` / `T::newInstance(name)` factory pattern. Controls form a parent/child tree (`Base::List`), set up by `setParent`.
- Each control type exposes static/virtual `getTypeName()` and `getControlName()` for runtime identification and the reflection-ish `getControlType(name)` lookup.
- Layout helpers (`align.h`, `controls/layout/`) and docking (`DockPosition` in `uiTypes.h`) handle positioning.

### Skins
A skin is constructed around a renderer and `init()`'d from a resource: the textured skin loads `DefaultSkin.json` (atlas description) + a TTF font. `texturedControls.cpp` / `texturedPrimitives.cpp` map each control type to its primitive layout in the atlas.

### Events
`event::Handler` is a base of `controls::Base`. Hooks carry an optional `event::Packet` (a `std::variant<Base*, String, int, float, size_t>`) delivered via `event::Info`. Use `Packet::isControl()/isString()/...` + `get<T>()` to unpack.

## Conventions
- C++17, `lowerCamelCase` for methods/functions, `PascalCase` for types, member fields prefixed `m_`.
- Public symbols crossing the library boundary are tagged `GWENI_EXPORT` (`export.h`).
- `gweni::String` / `gweni::Point` / `gweni::Rect` / `gweni::Color` / `BoundingBox` (aliased to `Margin`/`Padding`) come from `platforms/platformTypes.h` and `uiTypes.h` — use these rather than raw types.
- Filesystem backend is selected at configure time via `GWENI_USE_FILESYSTEM` (std / experimental / boost) based on compiler; code must go through that macro, not include `<filesystem>` directly.
- Verbose per-control debug output (`logUpdates` → `printf`, plus `DEBUG_ID` and `DEBUG_LAYOUT` in `base.h`) is off by default and enabled by configuring with `-DGWENI_DEBUG_LOGGING=ON`; expect verbose stdout when running such a build. `DEBUG_ID` also appends each control's id to label text, so that build looks different too.

## Origin / license
MIT. Copyright headers credit Facepunch Studios (original GWEN). Keep the license header on new files.
