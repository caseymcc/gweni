# Building and testing gweni

Builds run inside the build container (`./run_docker.sh` from a host; see
`AGENTS.md` → "Building" for the full details) and are always configured through
`generate.sh`:

```bash
./generate.sh linux                       # build_linux_x64_debug/
./generate.sh linux release               # build_linux_x64_release/
./generate.sh linux asan                  # build_linux_x64_debug_asan/, AddressSanitizer
./generate.sh linux profile               # build_linux_x64_profile/, for profiling
./generate.sh linux gles                  # build_linux_x64_debug_gles/, GLES 3.0 (see "GLES tree")
cmake --build build_linux_x64_debug -j
```

Every tree `generate.sh` configures has the Vulkan build on
(`GWENI_RENDER_VULKAN=ON`, with the vcpkg manifest feature `vulkan`: the Vulkan
headers, the volk loader, glslang's `glslangValidator`, which compiles the
shaders to SPIR-V at build time, and the Khronos validation layer). CMake's own
default is off, so a plain CMake build needs none of it. For a GL-only tree:

```bash
./generate.sh linux -DGWENI_RENDER_VULKAN=OFF
```

A `-DVCPKG_MANIFEST_FEATURES=...` given to `generate.sh` (e.g.
`boost-filesystem`) is merged with `vulkan`. The build image has the Vulkan
loader, Mesa's lavapipe CPU driver (Vulkan 1.3), which the Vulkan tests run on,
and Xvfb; an older container lacks them (`ls /usr/share/vulkan/icd.d` tells,
and `vulkan_smoke` fails saying so).

## GLES tree

`./generate.sh linux gles` configures `build_linux_x64_debug_gles/` (with
`release`: `build_linux_x64_release_gles/`; with `asan`:
`build_linux_x64_debug_gles_asan/`; `profile` does not combine with it), the
GL renderer, the examples, the tests and the bench built against GLES 3.0
(`-DGWENI_GL_ES=ON`):

```bash
./generate.sh linux gles
cmake --build build_linux_x64_debug_gles -j
mm run --lock render -- ctest --test-dir build_linux_x64_debug_gles -L gpu
ctest --test-dir build_linux_x64_debug_gles -LE gpu
```

It compiles against `<GLES3/gl3.h>` and links `libGLESv2` (and `libEGL`), with
no glew, so a GL call newer than GLES 3.0 does not compile there, and the
examples and tests run on a GLES 3.0 context: the surfaceless EGL one is
`EGL_OPENGL_ES_API` 3.0, and a window asks GLFW for a GLES 3.0 context (there is
no display in the container, so only the EGL path is exercised there). It is
the browser's API without the browser: every test passes as in the desktop
trees, `docs_samples` byte for byte included, and `gweni_bench`'s counters are
the desktop tree's. Mesa's llvmpipe GLES context has
`GL_EXT_multi_draw_arrays` and `GL_EXT_blend_func_extended`, so the text draws
and LCD text work as on desktop GL. Without dual-source blending (WebGL 2, or
`GWENI_GL_NO_DUAL_SOURCE=1` in any tree) LCD text (`GWENI_TEXT_AA=rgb|bgr`)
falls back to grayscale with one warning.

## Web tree

`./generate.sh web` configures `build_web_wasm32_debug/` (`web release`:
`build_web_wasm32_release/`), a WebAssembly build made by Emscripten:

```bash
./generate.sh web
cmake --build build_web_wasm32_debug -j
ctest --test-dir build_web_wasm32_debug -L web
```

It builds the `gweni` library with `OpenGLCore` on WebGL 2, the textured skin,
the headless unit tests (`gweni_unit_tests.js`/`.wasm`, run under node) and
three pages, each an `.html` with its `.js`, `.wasm` and a `.data` holding
`resources/`:

- `examples/openglcore/ControlTest.html` and `OpenGLCore.html`, the examples;
- `tests/gweni_unittests.html`, the GPU unit tests.

A page runs what the native exe runs. A capture (`--headless` with `--shot`,
`--shot-all`, `--screenshot`, ...) renders into an offscreen framebuffer and
reads it back in one blocking call. Without one, the page is the interactive
window: ControlTest's control or the OpenGLCore demo, drawn once per browser
animation frame, with the mouse (both buttons and the wheel) and the keyboard
(keys, modifiers and typed text) as on a desktop. The canvas fills the page above
a 96 px output panel and follows its size, and it draws at the browser's
`devicePixelRatio` (a HiDPI display, or the page zoom) times `--scale`, as a
desktop window draws at its monitor's content scale.

What the browser does not have degrades, each said once on stderr (and in the
output panel):

- the file dialogs say they are unavailable and return cancelled;
- the clipboard is in-process: copy and paste work inside the page, but not with
  other programs;
- LCD text needs dual-source blending, which WebGL 2 lacks, so text is grayscale.

The mouse cursor is the canvas's CSS cursor (a text beam, a pointer, a resize
arrow, ...), as the control under the mouse asks for it.

The dependencies come from vcpkg's `wasm32-emscripten` triplet, with emsdk's own
`Emscripten.cmake` chainloaded. `glew` and `glfw3` are left out on that
platform; GLFW comes from Emscripten's `contrib.glfw3` port instead. Everything
is built with `-fexceptions`, so C++ exceptions are caught as natively.
`gweni_bench` is not built. `asan` and `profile` do not combine with `web`. The
vcpkg binary cache starts cold for this triplet: the first configure builds its
dependencies, about 30 s on a 32-core host.

### Running a page by hand

`tests/web/run.mjs` runs one page in the image's headless Chromium, as you would
run the executable natively:

```bash
node tests/web/run.mjs --out /tmp/shots build_web_wasm32_debug/examples/openglcore/ControlTest.html \
    -- --headless --control ButtonDemo --shot /out/ButtonDemo.png
```

Everything after `--` is the program's argv. The page writes to an in-memory
file system, and the runner copies whatever the run wrote under `/out` to the
`--out` directory. `--mount <dir>:<path>` copies a host directory into the page's
file system before `main` runs, e.g. a `resources/` to load with
`--resources <path>`. The runner prints the run's stdout and stderr and exits
with its exit status. It fails with exit 1 and one `run.mjs: FAIL:` line if:

- the page logs a console error or throws;
- the run aborts or times out (`--timeout <s>`, default 600);
- the run made no WebGL 2 context on ANGLE's SwiftShader, or one with fewer than
  24 depth bits.

In a browser of your own, serve the build directory over HTTP (a page cannot
load its `.wasm` and `.data` from `file://`). The build tree is in the repo, so
on a host:

```bash
python3 -m http.server -d build_web_wasm32_debug/examples/openglcore 8000
```

and open `http://localhost:8000/ControlTest.html?control=ButtonDemo` or
`http://localhost:8000/OpenGLCore.html`. The arguments are the URL query, in
order and URL-decoded: `key=value` is `--key value`, a bare `key` is `--key`,
and `argv=<text>` is one argument as it is. So
`ControlTest.html?control=ButtonDemo&scale=2` runs
`ControlTest --control ButtonDemo --scale 2`, the window at twice the display's
scale, and `ControlTest.html?headless&control=ButtonDemo&shot=/out/x.png` a
capture. An argument the program does not take ends the run as it does natively,
with its usage in the output panel under the canvas, where the page shows
everything the program prints.

The build image has what it needs, pinned in the `Dockerfile`, about 2.1 GB
more image in all:

- emsdk 6.0.13 at `/opt/emsdk` (`EMSDK`), with its bundled node 24, which runs
  the wasm tests and Playwright;
- Playwright 1.65.0 at `/opt/playwright` (`NODE_PATH`), with the Chromium 158
  headless shell it pins (`PLAYWRIGHT_BROWSERS_PATH`) and that shell's apt
  packages.

Without emsdk (an older container: `echo $EMSDK` is empty), `./generate.sh web`
prints one line saying so and exits 1. From a host, `./run_docker.sh -r`
rebuilds the image.

Every test in the web tree has the label `web` and needs no GPU lock:

- each `unit:` case, run by node;
- `web_probe`, which opens a blank page in the headless Chromium
  (`tests/web/probe.mjs`) and fails unless it gets a WebGL 2 context on
  ANGLE's SwiftShader with at least 24 depth bits;
- `web_probe_fails_without_webgl2` and `web_run_fails_without_webgl2`, which
  check that the probe and the runner fail when the browser has no WebGL 2;
- `web_unittests`, the GPU unit tests in the browser. It leaves out the LCD
  text cases (WebGL 2 has no dual-source blending, so LCD text is grayscale
  there) and the one that compares the text draws' `glDrawElements` loop with a
  multi-draw the web does not have. The list and its reasons are in
  `tests/CMakeLists.txt`. Doctest counts the cases left out as skipped;
- `web_controltest_shot` and `web_openglcore_screenshot`: a capture writes a
  PNG of the canvas's size that is not one flat colour (`tests/web/shot.mjs`);
- `web_controltest_missing_resources*` and `web_openglcore_missing_resources*`:
  `tests/missingResources.cmake`'s cases, with the incomplete `resources/` copy
  mounted into the page;
- `web_input`: the interactive windows of ControlTest and the OpenGLCore demo,
  driven with real mouse and keyboard input (`tests/web/interactive.mjs`, whose
  header lists the checks): a button press and release, typing "abcé", a wheel
  notch, the CSS cursor, the canvas's size at `devicePixelRatio` 2, after a
  resize and with `?scale=1.5` (its layout the same as a capture's at that size
  and scale), an unknown argument, and a click in the demo list.

The browser tests take no lock because the runner keeps Chromium on its own
SwiftShader. The image pins the Vulkan loader to Mesa's lavapipe
(`VK_DRIVER_FILES`) for gweni's Vulkan tests, and with that pin ANGLE would
render on lavapipe instead. `probe.mjs` launches Chromium without the pin, and
both scripts fail if the renderer is not SwiftShader.

## Tests

The suite is CTest, under `tests/`, run from the repo root:

```bash
ctest --test-dir build_linux_x64_debug
ctest --test-dir build_linux_x64_debug_asan
```

The tests labelled `gpu` open a GL context through the examples' headless,
surfaceless-EGL path, or a Vulkan device: Mesa's CPU rasterisers, llvmpipe and
lavapipe, not a hardware GPU. On a shared host run them under the `render` lock,
and the headless rest on their own with no lock; never hold a lock around a
whole `ctest`, and never around a build (`./generate.sh`, `cmake --build`):

```bash
mm run --lock render -- ctest --test-dir build_linux_x64_debug -L gpu
ctest --test-dir build_linux_x64_debug -LE gpu
```

The Vulkan tests are also labelled `vulkan`, so `ctest -LE vulkan` runs the
GL-only suite. The tests:

- `controltest_determinism`: two separate ControlTest processes must write
  byte-identical `--shot-all` PNGs and `--dump-bounds` listings.
  `controltest_determinism_scale150` is the same at `--scale 1.5`.
- `controltest_scale_bounds`: `--shot-all --dump-bounds` at `--scale 1` and 2.
  The listings (logical units) must be identical and every PNG exactly twice
  as wide and high (`tests/scaleBounds.cmake`).
- `controltest_scale_roundtrip`: `ControlTest --scenario scale --require-one-pass`
  for every control in `--list`, Demo included. `Canvas::setScale` 1 -> 2 -> 1
  comes back byte for byte (listing, renderer mirror, pixels), and the 2x frame
  matches a canvas built directly at scale 2 (`tests/eachControl.cmake`).
- `controltest_teardown`: every demo builds, renders and tears down cleanly,
  and every frame lays out in one pass (`--require-one-pass`).
- `controltest_resize`: `ControlTest --scenario resize --require-one-pass`.
  Demo at 640x480, 1024x768 and 640x480 again comes back byte for byte, and
  matches a canvas built directly at 640x480.
- `controltest_resize_each`, `controltest_one_pass_each`: the same round trip
  for every other control in `ControlTest --list`, and the plain run of every
  control in one layout pass per frame (`tests/eachControl.cmake`).
- `docs_samples`: `tools/capture_samples.sh --check`. The README samples in
  `docs/images/` must match a fresh capture byte for byte, so a change that
  alters one regenerates them (`./tools/capture_samples.sh`) in the same
  commit. Only meaningful inside the build container.
- `controltest_interaction`: `ControlTest --scenario interaction`. Hover, press,
  disabled and a focus hand-off repaint the control, a hover touches only a few
  slots, an idle frame changes nothing, and collapsing a tree node moves the
  nodes below it.
- `controltest_text_size`: `ControlTest --scenario text-size`. A Label's font
  changed from 24 to 36 px at runtime, both drawn from the face's SDF atlas,
  uploads nothing to a font atlas and writes only that Label's glyph slots.
- `controltest_script_first_frame`: a ControlTest script that starts with
  input (`--move 30,5 --shot`) is settled first and renders a frame before the
  shot, so its PNG matches `--frames 3 --move 30,5 --frames 1 --shot`, and
  `--frames 0 --shot` exits 1 and writes nothing (`tests/scriptFirstFrame.cmake`).
- `unittests`: doctest cases (`gweni_unittests`), e.g. the UTF-8 text-length
  guards in `tests/textLength.cpp`.
- `vulkan_smoke` (labels `gpu`, `vulkan`): the Vulkan plumbing. It loads the
  loader through volk, creates an instance with `VK_LAYER_KHRONOS_validation`
  (any validation warning or error aborts it), finds a Vulkan 1.3 device with
  `dynamicRendering` and `dualSrcBlend`, and builds a pipeline from SPIR-V
  compiled at build time. Like every Vulkan test it runs with only the vcpkg
  validation layer on the loader's layer path and the device pinned to
  lavapipe (`gweni_vulkan_test_environment` in `cmake/Vulkan.cmake`).
- `unit:<case name>`: the headless doctest cases (`gweni_unit_tests`, sources
  in `tests/unit/`), one CTest test per case, e.g. the id-bank checks across
  skin swaps in `tests/unit/skinSwap.cpp`. No GL context, no `gpu` label.

## ASAN tree

`./generate.sh linux asan` sets `GWENI_SANITIZE=address`. That instruments gweni,
the examples, the tests and the vendored `deps/freetype-gl` and `deps/imglib`,
but not the vcpkg dependencies. Its tests run with
`ASAN_OPTIONS=detect_leaks=0:abort_on_error=1`, so memory errors fail them and
leaks (Mesa/EGL's at exit) do not.

ASAN only sees accesses outside an allocation. An over-read that stays inside one
object goes unreported. ListBoxDemo's original UTF-8 over-read ran from a short
(SSO) string into the neighbouring `m_font` field of `controls::Text`, and neither
ASAN nor valgrind reported it. The deterministic trailing-bytes unit test and the
two-process determinism test cover that case.

## Profile tree

`./generate.sh linux profile` configures `build_linux_x64_profile/`: an
optimised build with debug info (`RelWithDebInfo`) and frame pointers kept
(`GWENI_FRAME_POINTERS=ON`, `-fno-omit-frame-pointer` on everything, `deps/`
included), for profiling and for comparing revisions. It does not combine with
`release` or `asan`. Its tests pass like every other tree's. The build image has
the profiling tools, `valgrind` (callgrind), `graphviz` (`dot`) and `gprof2dot`;
an older container lacks them (`which valgrind` tells).

## Benchmarks

`gweni_bench` (`bench/`) runs a fixed scene on the full path, gweni plus
`OpenGLCore` on surfaceless EGL, and prints per measured frame the wall time of
each phase (`input`, `update`, `render`, `flush`, `gpu`) and the work counters:
controls visited and changed, slots written, uploads and buffer regrows
(`buffer_regrows`), draw calls, layout passes
and the controls they entered (`layout_visited`), text measures
(`measure_calls`, and `measure_calls_<phase>` for the input, update and
render phases), z renumbering, live ids and C++ allocations. The same
arguments give the same counters every run. Times are only meaningful from a release tree, and `gpu` is
Mesa llvmpipe on the CPU inside the container:

```bash
build_linux_x64_release/bench/gweni_bench --list
build_linux_x64_release/bench/gweni_bench --scene hover-sweep --n 10000
```

The scenes cover a cold first frame, idle frames, hover, typing, scrolling, a
dragged window, a tree node toggled, controls added, removed and brought to the
front, a canvas resized and rescaled, and animations. `--list` marks each
`scaling=local` (its per-frame counters must not grow with N) or `scaling=linear`
(it touches every control by design). `--help` lists the options and every
output key. `bench_smoke` (label `gpu`) only checks that every scene runs.
`bench_scaling` (label `gpu`) is the one test on the bench's numbers: it fails
if a `scaling=local` scene's per-frame work counters (never its times) are
larger at N=10000 than at N=1000. See `AGENTS.md` → "Benchmarks and profiling".

## Profiling

`tools/profile.sh` runs one scene from the profile tree under callgrind,
collecting only the measured frames (and the `build` scene's construction), not
setup, EGL init, resource loading, warm-up or teardown:

```bash
tools/profile.sh hover-sweep --n 1000     # into /tmp/gweni-profile-hover-sweep-1000/
tools/profile.sh build --n 10000 --out /tmp/build10k
```

It writes `callgrind.out` (open it with kcachegrind or qcachegrind on a host),
`phases.txt` (the instructions of each phase, `phase=<name> ir=<n>`, and their
`total`), `annotate.txt` (the functions by inclusive cost and every line of
gweni's sources with its cost) and `callgraph.svg` (the call graph, hot paths
red). It runs a copy of the bench from a fixed directory in a fixed
environment, so the same binary gives the same counts every run, to the
instruction; two revisions lay the heap out differently, which alone can move
`update` by up to 0.4 % and the other phases by under 0.1 %
(`--layout-pad K` samples that). Inside the container nearly all of a
small scene's instructions are Mesa llvmpipe rasterising, in the `flush` phase
under callgrind; Mesa's functions and its JIT code show as anonymous
`???:0x...` functions. See
`AGENTS.md` → "Profiling a scene".
