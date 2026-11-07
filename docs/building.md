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

## Choosing the renderer

`ControlTest`, `OpenGLCore` and `gweni_bench` draw with `OpenGLCore` by default.
`--renderer vulkan` (or `GWENI_RENDERER=vulkan`; the flag wins) draws with
`renderer::Vulkan` on a Vulkan device instead, with no GL or EGL context at
all; a capture is always headless:

```bash
mm run --lock render -- build_linux_x64_debug/examples/openglcore/ControlTest \
    --renderer vulkan --headless --control ButtonDemo --shot /tmp/button.png
mm run --lock render -- build_linux_x64_debug/examples/openglcore/OpenGLCore \
    --renderer vulkan --headless --screenshot /tmp/demo.png
mm run --lock render -- build_linux_x64_release/bench/gweni_bench \
    --renderer vulkan --scene hover-sweep --n 1000
```

On Vulkan a capture (`--shot`, `--shot-all`, `--screenshot`, `--frames`,
`--stats`, the scenarios, `--check-teardown`) is the same headless run with or
without `--headless`. Without one, both examples open their window as on GL,
with a Vulkan swapchain (see "Vulkan in a window" below). `--renderer vulkan`
in a tree built without Vulkan prints one line and exits 1.
`GWENI_VULKAN_VALIDATION=1` turns the validation layer on (any warning or error
aborts the run); the Vulkan tests set it, with the layer's path
(`VK_LAYER_PATH=<tree>/vulkan/explicit_layer.d`) and lavapipe's ICD
(`VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json`).

### Vulkan in a window

`OpenGLCore --renderer vulkan` and `ControlTest --renderer vulkan` with no
capture open a GLFW window with no GL context and draw into it through a FIFO
swapchain. They resize, follow the content scale and stop drawing while
minimised as the GL window does (`docs/architecture.md` -> "The Vulkan
window"). The build container has no display, so there it is tried under
Xvfb (`window_vulkan` does that for you, see "Tests"):

```bash
xvfb-run -a -s "-screen 0 1280x1024x24" \
    build_linux_x64_debug/examples/openglcore/ControlTest --renderer vulkan --control ButtonDemo
```

To try it on a host with a real Vulkan driver (not part of any check; on a
HiDPI display the window is drawn at its content scale):

```bash
./run.sh opengl --renderer vulkan
./run.sh controltest --renderer vulkan --control ButtonDemo
GWENI_VULKAN_VALIDATION=1 VK_LAYER_PATH=$PWD/build_linux_x64_debug/vulkan/explicit_layer.d \
    ./run.sh opengl --renderer vulkan      # the same, every validation message aborting
```

The host needs, besides what the GL exes need (see "Running the examples"
in `AGENTS.md`), the Vulkan loader (`libvulkan.so.1`, e.g. Ubuntu's
`libvulkan1`) and a Vulkan 1.3 driver (its ICD manifest under
`/usr/share/vulkan/icd.d/`: Mesa's `mesa-vulkan-drivers` for AMD and Intel,
NVIDIA's own driver), on X11 or XWayland. `vulkaninfo --summary`
(`vulkan-tools`) lists what the loader finds. The exes take the first device
with Vulkan 1.3, dynamic rendering and a graphics queue; one that cannot present
to the window prints one line and exits 1.

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
cmake --build build_linux_x64_debug_gles -j   # the native reference (./generate.sh linux gles)
ctest --test-dir build_web_wasm32_debug -L web
```

It builds the `gweni` library with `OpenGLCore` on WebGL 2, the textured skin,
the headless unit tests (`gweni_unit_tests.js`/`.wasm`, run under node) and
three pages, each an `.html` with its `.js`, `.wasm` and a `.data` holding
`resources/` (all of it but the fonts' license texts, which nothing loads):

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
The release web tree is built with `-Oz` instead of CMake's `-O3`, because it is
what the hosted site sends to every visitor (see "The hosted site").
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
  and scale), an unknown argument, and a click in the demo list;
- `web_controltest_determinism`: two page loads of `ControlTest --shot-all
  --dump-bounds` write the same bytes;
- `web_bounds_native*`, `web_stats_native` and `web_pixels_native*`, which hold
  the browser to the native GLES tree: every `--dump-bounds` listing and every
  `--stats` counter identical, every PNG within 2 per channel with at most 0.1 %
  of its pixels more than 1 off (`tests/web/native.cmake` has the measured
  values). They run the native tree's exes themselves, so build it first:

  ```bash
  ./generate.sh linux gles && cmake --build build_linux_x64_debug_gles -j
  ```

  Without it they fail and say so. `./generate.sh web` points them at the GLES
  tree of the same build type (`GWENI_NATIVE_REFERENCE_DIR`). A PNG over the
  bound is written beside its native one and the difference, as
  `<test dir>/diff/<name>.diff.png`.

- `web_site_smoke`, in the release web tree only: the hosted site, staged and
  loaded page by page (see "The hosted site").

The browser tests take no lock because the runner keeps Chromium on its own
SwiftShader. The image pins the Vulkan loader to Mesa's lavapipe
(`VK_DRIVER_FILES`) for gweni's Vulkan tests, and with that pin ANGLE would
render on lavapipe instead. `probe.mjs` launches Chromium without the pin, and
both scripts fail if the renderer is not SwiftShader.

### The hosted site

`tools/build_site.sh` builds the release web tree (configuring it with
`./generate.sh web release` if it is not yet) and stages the live demo as one
static directory, `site/` in the repo (git-ignored), that GitHub Pages or any
static server serves as it is:

```bash
./tools/build_site.sh                    # stage site/
./tools/build_site.sh --out /tmp/site    # stage it elsewhere
./tools/build_site.sh --build-dir DIR    # take the pages from another web tree
python3 -m http.server -d site 8000      # then http://localhost:8000/
```

It holds three pages, their files and nothing else:

- `index.html`, the landing page: what gweni is, links to the demo, the gallery
  and the GitHub repo, and the hero sample (`docs/images/demo.png`) as a preview;
- `demo.html`, the OpenGLCore demo's window;
- `controltest.html`, ControlTest's window with a control picker. Its entries
  are `ControlTest --list`, written in when the site is staged. The URL query is
  ControlTest's argv, so `controltest.html?control=ButtonDemo&scale=2` is a link
  to one control at 2x, and choosing a control loads its page with every other
  parameter kept.

They are written in `tools/site/` (the pages, `site.css`), dark like the Tokyo
Night skin, with no framework and nothing loaded from elsewhere. Every URL is
relative, so the site works under `https://<user>.github.io/gweni/` as from a
local server. It needs no COOP/COEP headers, since nothing in the build uses
threads or SharedArrayBuffer. A non-empty `--out` directory is cleared only if
it holds a site staged by the script (its `.gweni-site` marker).

The script then prints what each page sends, every file raw, gzip and brotli,
with nothing taken to be cached (`tools/site/sizes.mjs`). It exits 1 if the
gallery page's gzip total is over its `BUDGET_BYTES`, 8 MB. Measured at this
change:

| page | raw | gzip | brotli |
|---|---|---|---|
| `controltest.html` (`ControlTest.wasm` 2566 kB, `.data` 715 kB, `.js` 146 kB) | 3.43 MB | 1.41 MB | 1.07 MB |
| `demo.html` (`OpenGLCore.wasm` 2360 kB, `.data` 715 kB, `.js` 146 kB) | 3.22 MB | 1.35 MB | 1.03 MB |
| `index.html` (with the 152 kB preview) | 0.16 MB | 0.15 MB | 0.15 MB |

The release web tree is `-Oz` for this: at CMake's `-O3`, `ControlTest.wasm` is
3041 kB (1080 kB gzip), against `-Oz`'s 2566 kB (975 kB gzip), and a
browser run (`--check-teardown`, Demo's resize scenario) is no slower within
3 %. `-Os` lies between them (2854 kB).

`web_site_smoke` (`tests/web/site.cmake`, release web tree) runs the script into
the build tree and then `tests/web/site.mjs`, which serves the staged site and
loads every page in the headless Chromium: the landing page with its image, the
demo, and the gallery at ButtonDemo, TextBoxDemo and ShowcaseButtonsDemo at
`?scale=2`, where it also picks SliderDemo and must get
`controltest.html?control=SliderDemo&scale=2`. It fails on a console error, an
uncaught exception, a failed request or an HTTP error (a file missing from the
site), a page without a WebGL 2 context on SwiftShader with 24 depth bits, a
window that draws no frames or whose canvas is one colour after 5 of them, a
picker not showing its control, a link into the site that is not there, and,
through the script, a gallery page over the budget. `node tests/web/site.mjs
--url http://localhost:8000/` checks a site some other server serves.

### Publishing the site

The site is live at <https://caseymcc.github.io/gweni/>. The GitHub Actions
workflow `.github/workflows/pages.yml` (the repo's only CI) publishes it on
every push to `main`, and on demand from *Actions → pages → Run workflow*. Its
`build` job:

1. builds the build image from the repo's `Dockerfile`, with the runner's
   UID/GID as `run_docker.sh` passes the host's;
2. runs `tools/ci_site.sh` in a container from it, with the checkout mounted at
   `/home/developer/gweni` and the vcpkg cache at `/vcpkg-cache`, as
   `run_docker.sh` mounts them;
3. uploads the staged `site/` as the Pages artifact.

Then its `deploy` job deploys that artifact to the `github-pages` environment.
If any step fails, a web test included, nothing is deployed and the site stays
as the last good run left it.

`tools/ci_site.sh` is every build step, so the YAML holds none and a run can be
repeated by hand in the container:

```bash
./tools/ci_site.sh                 # in the container; ./run_docker.sh ./tools/ci_site.sh from a host
./tools/ci_site.sh --out /tmp/site # stage the site elsewhere
```

It stops at the first step that fails:

1. the native GLES release tree, `build_linux_x64_release_gles/`, with Vulkan
   off (`-DGWENI_RENDER_VULKAN=OFF`): only its `ControlTest` and `OpenGLCore`,
   which the `web_*_native` tests compare with. Nothing here uses Vulkan, and
   its vcpkg ports (glslang, the validation layers) would double a cold
   dependency build. A tree of that name configured with Vulkan on is
   reconfigured without it;
2. the whole release web tree, `build_web_wasm32_release/`;
3. `ctest -L web` in it, `web_site_smoke` and the comparisons with native
   included. On a runner, without `mm`, their native runs take no lock and say
   so;
4. `tools/build_site.sh`, the site and its size against the budget;
5. `tools/check_site_links.py README.md site/`, which fails unless:
   - every link in `README.md` to `https://caseymcc.github.io/gweni/` is a page
     of the staged site, and any `control=`/`demo=` in it is one of the
     gallery's picker entries;
   - the README links to the site itself;
   - every `docs/images/` image is a link to its live page: the hero to
     `demo.html`, a gallery image `<Name>.png` to
     `controltest.html?control=<Name>`;
   - the gallery images are `tools/capture_samples.sh`'s `SAMPLES`, in order.

   So a sample added to `SAMPLES` needs its linked image in the README, and a
   page renamed in `tools/site/` needs the README's links changed with it.

Two caches keep later runs short. Both live in GitHub's Actions cache (10 GB
per repository; the oldest entries are evicted beyond that):

- the image's layers, through buildx's `gha` cache, so a run rebuilds from the
  first `Dockerfile` layer that changed;
- vcpkg's binary cache and downloads, keyed on the image's vcpkg commit and on
  `vcpkg.json`. The `Dockerfile` clones vcpkg unpinned, so the commit is the one
  in the cached layer. A new key starts from the newest older entry. The cache
  is saved even when a later step fails, so a failed run still keeps the
  dependencies it built.

Every action is pinned to a full commit SHA, its version beside it. Each job
gets only the permissions it needs: `contents: read` to build, and `pages:
write` and `id-token: write` to deploy. Runs take turns (the `pages`
concurrency group), and a run in progress is never cancelled half-deployed.

Done once, by the repository's owner (no agent changes repository settings or
pushes):

- *Settings → Pages → Build and deployment → Source: **GitHub Actions***.
- Expect the first run to be slow: a cold image build (emsdk, Playwright and
  Chromium) and cold vcpkg builds for both triplets, `x64-linux` and
  `wasm32-emscripten`, on a 4-core runner. Later runs reuse both caches.

`node tests/web/site.mjs --url https://caseymcc.github.io/gweni/`, in the
container, runs the `web_site_smoke` checks against the deployed site.

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
GL-only suite. Each whole-app check below (the `controltest_*` and
`openglcore_missing_resources*` tests, but `controltest_verify_layout_*`,
`controltest_demo_labels*` and `controltest_any_cwd`, which check layout and
paths, not drawing) and `bench_smoke` is registered twice: on GL, and as
`<name>_vulkan` (labels `gpu`, `vulkan`), the same command with
`GWENI_RENDERER=vulkan`, validated. `docs_samples` stays GL only. The tests:

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
  nodes below it. In CheckBoxDemo a box lights when it or its label is hovered,
  a click checks it and a second unchecks it, and the disabled box looks
  muted and ignores hover and clicks.
- `controltest_text_size`: `ControlTest --scenario text-size`. A Label's font
  changed from 24 to 36 px at runtime, both drawn from the face's SDF atlas,
  uploads nothing to a font atlas and writes only that Label's glyph slots.
- `controltest_script_first_frame`: a ControlTest script that starts with
  input (`--move 30,5 --shot`) is settled first and renders a frame before the
  shot, so its PNG matches `--frames 3 --move 30,5 --frames 1 --shot`, and
  `--frames 0 --shot` exits 1 and writes nothing (`tests/scriptFirstFrame.cmake`).
- `unittests`: doctest cases (`gweni_unittests`), e.g. the UTF-8 text-length
  guards in `tests/textLength.cpp`.
- `unittests_vulkan` (labels `gpu`, `vulkan`): the same `gweni_unittests` on
  the Vulkan backend (`GWENI_RENDERER=vulkan`: a validated Vulkan device and
  `renderer::Vulkan`, no GL context), so its cases check the same analytic
  models on both: every case but the `gl_only` suite (what only GL has: the
  GLSL dialects, `glMultiDrawElements`, the GL objects a renderer deletes).
- `vulkan_smoke` (labels `gpu`, `vulkan`): the Vulkan plumbing. It loads the
  loader through volk, creates an instance with `VK_LAYER_KHRONOS_validation`
  (any validation warning or error aborts it), finds a Vulkan 1.3 device with
  `dynamicRendering` and `dualSrcBlend`, and builds a pipeline from SPIR-V
  compiled at build time. Like every Vulkan test it runs with only the vcpkg
  validation layer on the loader's layer path and the device pinned to
  lavapipe (`gweni_vulkan_test_environment` in `cmake/Vulkan.cmake`).
- `window_vulkan` (labels `gpu`, `vulkan`): the Vulkan window, under an Xvfb
  it starts itself and a minimal test window manager (`gweni_test_wm`):
  `ControlTest --renderer vulkan --window-test` presents ButtonDemo at
  640x480, resizes the window to 800x600, 512x384 and back, iconifies and
  restores it, and reads back each image it presented; every one must be byte
  for byte a `--headless` capture at its size, every resize must recreate the
  swapchain and nothing may be drawn or presented while iconified
  (`tests/windowVulkan.cmake`). It fails, not skips, without Xvfb.
- `backend_counters` (labels `gpu`, `vulkan`): every `gweni_bench` scene, every
  `ControlTest --stats` control and `OpenGLCore --stats` run on `--renderer gl`
  and `--renderer vulkan`; every counter of every frame must be the same (the
  times and the bench's allocation counts left out), and a settled Vulkan frame
  must write and upload nothing (`tests/backends.mjs`, run on emsdk's node).
- `backend_pixels` and `backend_pixels_scale200` (labels `gpu`, `vulkan`):
  `ControlTest --shot-all` and the hero on both backends, at scale 1 and 2; each
  Vulkan PNG within 1 level per channel of GL's, with at most 0.01 % of its pixels
  differing. A failure keeps its scratch dir under `/tmp` with a
  `diff/<name>.diff.png` (GL, Vulkan, the difference) per PNG over the bound
  (`docs/architecture.md` -> "Vulkan's counters and pixels against GL's").
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
`OpenGLCore` on surfaceless EGL (or `renderer::Vulkan` with `--renderer vulkan`),
and prints per measured frame the wall time of
each phase (`input`, `update`, `render`, `flush`, `gpu`) and the work counters:
controls rendered and changed (`visited`, the queued controls the render
phase rendered, 0 on a settled frame, and `render_carried`, the entries it
left queued for a later frame), slots written, uploads and buffer regrows
(`buffer_regrows`), draw calls, layout passes
and the controls they entered (`layout_visited`), text measures
(`measure_calls`, and `measure_calls_<phase>` for the input, update and
render phases), z renumbering, live ids and C++ allocations. The same
arguments give the same counters every run. Times are only meaningful from a release tree, and `gpu` is
Mesa llvmpipe (or lavapipe) on the CPU inside the container; the summary's
`renderer=` and `device=` say which:

```bash
build_linux_x64_release/bench/gweni_bench --list
build_linux_x64_release/bench/gweni_bench --scene hover-sweep --n 10000
```

The scenes cover a cold first frame, idle frames, hover, typing, scrolling, a
dragged window, a tree node toggled, controls added, removed and brought to the
front, a canvas resized and rescaled, and animations. `--list` marks each
`scaling=local` (its per-frame counters must not grow with N) or `scaling=linear`
(it touches every control by design). `--help` lists the options and every
output key. `bench_smoke` (label `gpu`) only checks that every scene runs, and
`bench_smoke_vulkan` that it runs on Vulkan.
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

## Comparing two revisions

`tools/bench.sh compare` builds two revisions in their profile trees, runs every
scene on both and prints a markdown report (also written to
`/tmp/gweni-bench-<base>-<rev>.md`) of how the work counters, the instructions
of each phase under callgrind and, with `--time`, the wall time moved. It flags
regressions and never fails because of one: it is a report to accept or act on,
not a gate, and the baseline is always the base revision.

```bash
tools/bench.sh compare origin/main            # origin/main vs the working tree
tools/bench.sh compare origin/main --keep     # keep origin/main's build for next time
tools/bench.sh compare origin/main --time     # wall time too, alternating runs
```

Before finishing a change to `source/` or `include/`, run it against
`origin/main` without `--time` and look at the flags. The flags: any counter
that changed; a phase more than 1 % more instructions; a counter of a local
scene that now grows from N=1000 to N=10000 (a new O(N)); with `--time`, a phase
more than 5 % and more than twice the spread slower. A base commit is exported
with `git archive` to `/tmp` and built there, so the first compare against it
configures a new tree (slow without the vcpkg binary cache); `--keep` keeps it.
A default compare takes about 40 minutes, nearly all of it callgrind;
`--no-ir` about 2, and `--scenes` limits it to the scenes a change touches.
Each timed run holds the `render` and `quiet` locks (no other locked job runs
alongside it), but builds take no lock, so on a shared host a wall-time flag can
still be noise (a compare of a commit with itself flagged 2 of 78 phases): run
it again before acting on it. Callgrind runs take no lock. See `AGENTS.md` → "Comparing two
revisions".
