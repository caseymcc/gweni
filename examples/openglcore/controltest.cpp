/*
 *  Gweni - single-control test harness
 *
 *  Drops ONE control (via its demo builder) directly on the canvas, so each
 *  control can be verified in isolation without the demo's dock/page layout.
 *
 *  Usage: ControlTest [options]
 *    --control <Name>   control demo to show (default ButtonDemo)
 *    --size WxH         canvas size in logical units (default 640x480); the
 *                       framebuffer is that times --scale
 *    --scale <s>        render at s device pixels per logical unit (default
 *                       1, or the GWENI_SCALE environment variable; the flag
 *                       beats it). The framebuffer and the PNGs are
 *                       round(size*s) device pixels; --move/--click and the
 *                       other scripted coordinates, the scenarios' fixed
 *                       sizes and --dump-bounds stay in logical units, so a
 *                       listing is the same at any scale. The interactive
 *                       window draws at s times the OS content scale, and
 *                       --size is its canvas in logical units.
 *    --frames N         frames to render before capturing (default 3)
 *    --shot <path>      render offscreen and write one PNG, then exit
 *    --shot-all         write one PNG per control demo, in this one run
 *    --shot-dir <dir>   directory for shots, created if missing (default /tmp)
 *    --headless         force the surfaceless EGL context (no display needed)
 *    --stats            print one key=value line per frame; freezes the clock.
 *                       prim_ids= counts the live primitive ids (handed out
 *                       and not released) after the frame; the id bank is
 *                       process-wide, so it covers every canvas;
 *                       z_visited= the controls updateCanvas()'s z renumber
 *                       entered (0 unless the tree order changed)
 *    --require-one-pass fail the run (exit non-zero) if any frame's layout took
 *                       more than one update-phase pass or the render phase
 *                       laid out again; each such frame is named on stderr
 *                       with its control and the control that re-dirtied
 *                       layout. Works with any run that renders frames
 *                       (--control, --scenario, --check-teardown).
 *    --check-teardown   build, render and destroy every control (or only
 *                       --control's), then check that the canvas renders
 *                       byte-identical to the empty canvas it started as.
 *                       Exits non-zero on a mismatch and writes
 *                       teardown-reference.png and teardown-after.png to
 *                       --shot-dir, so the ghost can be looked at.
 *    --trace            log each scripted input on stderr with what it hit
 *    --list             print the control demo names and exit
 *    --help, -h         print the option list and exit
 *    --scenario <name>  per-control | resize | docked | scroll | interaction |
 *                       typing | clip | text-size | scale (see below)
 *    --dump-bounds [<path>]
 *                       print the laid-out control tree as text: one line per
 *                       control, depth-first in child order, indented by depth,
 *                       with its type, bounds, dock, size flags and hidden
 *                       state. Every child is listed, private ones included:
 *                       a ScrollControl shows its inner panel (its rows under
 *                       it) and both scrollbars. No path writes to stdout; a
 *                       path writes that file. Nothing pointer-, id- or
 *                       time-derived is printed, so two runs of the same
 *                       command give byte-identical listings.
 *
 *  Scripted input (repeatable, applied in the order written, and interleaved
 *  with --frames / --shot so one process run can capture a sequence):
 *    --move X,Y         move the mouse
 *    --press X,Y        move there and hold the left button down
 *    --release X,Y      move there and let the left button up
 *    --click X,Y        a whole left click (press then release)
 *    --double-click X,Y a left double click
 *    --scroll DX,DY     mouse wheel (DX is ignored; gweni only takes the Y axis)
 *    --key <name>       a key: return, kp-enter, tab, space, escape, up, down,
 *                       left, right, home, end, backspace, delete, shift,
 *                       control, alt, command, or a single printable
 *                       character. A named key is pressed and released as
 *                       synthesised GLFW key events and a character is typed
 *                       as a GLFW char event, both through the same
 *                       gweni::input::GLFW3 translator the examples' callbacks
 *                       use. A chord joins modifiers (ctrl, shift, alt,
 *                       command) and a key with '+': --key ctrl+a, --key
 *                       shift+left, --key ctrl+shift+end. The modifiers are
 *                       pressed in order, the key goes down and up with their
 *                       mods bits set, then the modifiers come up in reverse.
 *                       A chord's key is a named key, a letter or a digit, and
 *                       is sent as a key event only, as GLFW sends no char for
 *                       Ctrl+letter.
 *    --type <utf8>      type the text: it is decoded as UTF-8 and each
 *                       codepoint sent as a GLFW char event. Until UnicodeChar
 *                       is widened the translator drops all but printable
 *                       ASCII, so only that arrives. To type a space, --type " ".
 *    --expect-text <utf8>
 *                       the keyboard-focused control's text has to be exactly
 *                       this (a TextBox's getText()). A mismatch prints both on
 *                       stderr and the run exits non-zero at the end; the
 *                       script carries on, so every bad step is reported.
 *
 *  Any of those switches puts the run in script mode: --frames N then means
 *  "render N frames now" and --shot P means "capture the framebuffer now", both
 *  at the point they appear, e.g.
 *
 *    --control ButtonDemo --move 60,40 --frames 2 --shot hover.png \
 *                         --press 60,40 --frames 2 --shot press.png
 *
 *  Use --press (not --click) to photograph a held-down button: a --click is down
 *  and up again, so by the next frame the control is only hovered.
 *
 *  Typing into TextBoxDemo's first box (at 640x480 its centre is 320,28) and
 *  checking the result:
 *
 *    --control TextBoxDemo --frames 3 --click 320,28 --frames 2 \
 *        --type hello --expect-text hello --key backspace --expect-text hell \
 *        --key ctrl+a --key ctrl+c --key end --key ctrl+v --expect-text hellhell
 *
 *  Scenarios:
 *    per-control  the 28 single-control shots (same as --shot-all)
 *    resize       one control (default Demo) at 640x480, 1024x768 and
 *                 640x480 again, then once more on a new canvas built directly
 *                 at 640x480 (resize-00-fresh-640x480.png). The three 640x480
 *                 states are compared cheapest first - the listings, then the
 *                 renderer mirror id by id (shot 1 against shot 3), then the
 *                 pixels - and one line reports them:
 *                   resize control=X listing_settled=yes|no mirror_settled=yes|no
 *                          shrink_settled=yes|no fresh_matches=yes|no
 *                 listing_settled and shrink_settled compare shot 3 with shot 1
 *                 (listing, pixels), fresh_matches the fresh shot with both
 *                 (listing and pixels). Each mismatch is described on stderr:
 *                 the first differing listing line with its control path, the
 *                 differing primitive/text ids with their owning control, the
 *                 differing pixel count. Exits non-zero if any check is no.
 *    docked       the whole Demo (dock layout + status bar) at 1024x768
 *    scroll       the five overflowing demos, idle and after a scripted scroll
 *    interaction  hover, press, disabled and focus have to repaint, checked on
 *                 the pixels inside the control's own bounds, and one line
 *                 reports each check as yes/no:
 *                   ButtonDemo: idle, hover and press of the first Button
 *                   differ pairwise, moving off and setDisabled(false) give
 *                   the idle look back, setDisabled(true) changes it; the
 *                   hover frame writes 1..4 primitive slots and changes at
 *                   most 4 controls; a settled frame with no input changes
 *                   nothing. TextBoxDemo: clicking TextBox 1 changes it, and
 *                   once the mouse has moved on and clicked TextBox 2, TextBox
 *                   1 looks as it did before any click.
 *                   TreeControlDemo: collapsing a node repaints and moves the
 *                   node below it up. Exits non-zero if any check is no. The
 *                   clock is frozen as under --stats.
 *    typing       the keyboard on TextBoxDemo, every key a synthesised GLFW
 *                 event through gweni::input::GLFW3: click into the first box,
 *                 Ctrl+A and type "hello" (shot typing-hello.png), then
 *                 Backspace, Left, Delete, Home, End, Right, Shift+Left,
 *                 Ctrl+A/C/X/V, a Ctrl+letter with no shortcut, Escape, Ctrl
 *                 held while the window loses focus, Tab to the second
 *                 box; digits and letters into the TextBoxNumeric; Return and keypad Enter in the
 *                 TextBoxMultiline. Each step is followed by a frame and checks
 *                 the focused control's text; a failure is named on stderr with
 *                 the expected and actual text. Then one keystroke into a
 *                 settled box has to rewrite one control's text run and the
 *                 frame after it upload nothing; the line
 *                   typing keystroke controls_changed=N text=N
 *                          prim_slots_written=N text_slots_written=N upload_bytes=N
 *                 gives its counters, and
 *                   typing checks=N failed=N
 *                 the total. Exits non-zero if any check fails. The clock is
 *                 frozen, so key repeat (wall-time based) is not covered.
 *    clip         children cut at their clipping parent's edges, on the bare
 *                 canvas, each frame compared with the one with less in it:
 *                 a ScrollControl with no bars and content past its right and
 *                 bottom edges (a fill, a Button, a CheckBox, a Label) leaves
 *                 every pixel outside it as on the empty canvas, and once the
 *                 content is moved below it the frame is the empty canvas
 *                 again. A Window in a 400x300 parent, dragged by its title
 *                 bar past the parent's bottom-right and then top-left edges,
 *                 leaves everything outside the parent as on the empty
 *                 canvas; dragged right out of the parent the frame is the
 *                 parent alone, and dragged back it looks as it started. A
 *                 drag step changes controls in the Window's subtree only,
 *                 and a settled frame changes and uploads nothing. A Window
 *                 on the canvas itself, dragged past the canvas's own top-left
 *                 corner (to -28,-8), leaves everything outside it as on the
 *                 empty canvas, and its on-canvas part is the same Window
 *                 drawn whole at (60,56), shifted. One line
 *                   clip scroll_cut=yes|no ... canvas_back=yes|no
 *                 reports the checks. Exits non-zero if any check is no.
 *    text-size    a Label in OpenSans 24 px beside an 11 px one and a 48 px one,
 *                 then its font changed to 36 px at runtime through
 *                 Label::setFont(face, size, bold), then the Label deleted
 *                 (text-size-24.png, text-size-36.png, text-size-deleted.png).
 *                 All three sizes are drawn from the face's one SDF atlas, so the
 *                 change has to upload nothing to a font atlas and write no more
 *                 glyph slots in a frame than the Label's run holds; the Label
 *                 has to grow and repaint, and a frame after it write nothing.
 *                 The 48 px Label has to draw the same pixels after the change
 *                 and after the deletion, both of which free the Label's font,
 *                 and the atlas has to keep its GL name:
 *                   text-size from=24 to=36 sdf=yes atlas_upload_calls=N
 *                          text_slots_written=N run_glyphs=N ... same_atlas=yes
 *                 Exits non-zero if any of it fails, or if GWENI_SDF_TEXT_MIN puts
 *                 either size below the SDF threshold.
 *    scale        one control (default Demo) on a 640x480 canvas at scale 1,
 *                 then Canvas::setScale(2) with the framebuffer resized to
 *                 1280x960, then back to 1 (scale-01-1x.png, scale-02-2x.png,
 *                 scale-03-1x.png), then a new canvas built directly at scale
 *                 2 (scale-00-fresh-2x.png). It sets both scales itself, so
 *                 --scale is ignored. One line reports the checks:
 *                   scale control=X listing_settled=yes|no mirror_settled=yes|no
 *                          pixels_settled=yes|no fresh_matches=yes|no
 *                 listing_settled, mirror_settled and pixels_settled compare
 *                 shot 3 with shot 1 (listing, renderer mirror id by id,
 *                 pixels), fresh_matches the fresh shot with shot 2 (listing
 *                 and pixels). Mismatches are described as for resize. Exits
 *                 non-zero if any check is no.
 *
 *  Every scenario (and --shot-all) takes a listing per shot when
 *  --dump-bounds is on, named after that shot with a .txt extension, so the
 *  shot and its listing sit side by side in --shot-dir - resize yields four
 *  (the fresh one included) and per-control 28. The resize checks take their
 *  listings in memory whether or not --dump-bounds is on; it only writes them
 *  out. A scenario names its own listings, so --dump-bounds
 *  there takes no path.
 *
 *  All of them, in the build container, from the repo root (a non-zero exit
 *  from resize, interaction, typing, clip, text-size or scale is their
 *  assertion, so it is reported rather than aborting the loop):
 *
 *    for s in per-control resize docked scroll interaction typing clip text-size scale; do \
 *      ./build_linux_x64_debug/examples/openglcore/ControlTest \
 *          --headless --scenario $s --shot-dir /tmp/shots/$s; \
 *      echo "$s exit=$?"; \
 *    done
 *
 *  With no --shot/--shot-all/--stats/--scenario and no scripted input it opens
 *  the interactive window, as before, on the real platform clock.
 *
 *  A control type with no skin renderer fails the run, in every mode: when
 *  Base::setSkin() finds that a control's renderer lookup ended at the skin's
 *  Base entry (and the type does not opt out with drawsNothing()), it prints
 *    gweni: no skin renderer for <TypeName>
 *  on stderr, once per type, and before exiting ControlTest prints
 *    ControlTest: N control type(s) with no skin renderer: <TypeName> ...
 *  and exits non-zero (gweni::controls::typesWithoutSkinRenderer()).
 *
 *  Captures install a stepped clock (gweni::platform::setTimeSource), so a shot
 *  depends on the frame number and not on how fast the process ran. --stats
 *  freezes the per-frame step instead, so an idle frame is genuinely idle and
 *  the counters settle to zero.
 */

#include <GL/glew.h>
#include <gweni/platforms/renderers/openglCore.h>
#include <gweni/gweni.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/platforms/input/glfw3.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/label.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/windowControl.h>
#include <gweni/controls/checkBox.h>
#include <gweni/controls/label.h>
#include <GLFW/glfw3.h>
#include <gweni/utils/filesystem.h>
#include "capture.h"
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <demo/button.h>
#include <demo/demo.h>
#include <demo/checkBox.h>
#include <demo/collapsibleList.h>
#include <demo/colorPicker.h>
#include <demo/comboBox.h>
#include <demo/crossSplitter.h>
#include <demo/groupBox.h>
#include <demo/imagePanel.h>
#include <demo/label.h>
#include <demo/labelMultiline.h>
#include <demo/layoutDemo.h>
#include <demo/listBox.h>
#include <demo/menuStrip.h>
#include <demo/numeric.h>
#include <demo/pageControl.h>
#include <demo/progressBar.h>
#include <demo/properties.h>
#include <demo/radioButton.h>
#include <demo/scrollControl.h>
#include <demo/slider.h>
#include <demo/statusBar.h>
#include <demo/tabControl.h>
#include <demo/treeControl.h>
#include <demo/textBox.h>
#include <demo/textDemo.h>
#include <demo/textSizes.h>
#include <demo/unicodeText.h>
#include <demo/window.h>

// A capture's clock: one fixed step per rendered frame, so every run sees the
// same times. 1/60 s is the step, i.e. a nominal 60fps.
static const float g_frameStep=1.0f/60.0f;

// Every scripted press also steps the clock, by more than inputHandler's 0.5s
// double-click window (source/inputHandler.cpp), so that two --clicks in a row
// are two clicks however fast the process runs. --double-click is the only way
// to ask for a double click: it issues its second press with no step in
// between, i.e. 0s apart, which is inside any window.
static const float g_pressStep=1.0f;

static float g_clockSeconds=0.0f;

static float steppedClock()
{
    return g_clockSeconds;
}

static gweni::input::GLFW3 g_glfwInput;
static gweni::renderer::Base *g_renderer=nullptr;
static gweni::controls::Canvas *g_canvas=nullptr;
static float g_manualScale=1.0f;        // --scale or GWENI_SCALE

static void key_callback(GLFWwindow*, int key, int scancode, int action, int mods)
{   const gweni::input::GLFW3::KeyEvent e={key,scancode,action,mods}; g_glfwInput.processKeyEvent(e); }
static void char_callback(GLFWwindow*, unsigned int codepoint)
{   g_glfwInput.processCharEvent(codepoint); }
static void window_focus_callback(GLFWwindow*, int focused)
{   g_glfwInput.processFocus(focused == GLFW_TRUE); }
static void cursor_pos_callback(GLFWwindow*, double x, double y)
{   g_glfwInput.processMouseMove(x,y); }
static void mouse_button_callback(GLFWwindow*, int b, int a, int m)
{   g_glfwInput.processMouseButtons(b,a,m); }
static void scroll_callback(GLFWwindow*, double x, double y)
{   g_glfwInput.processScroll(x, y); }
// The framebuffer is in device pixels and the canvas in logical units, at the
// window's content scale times --scale; the cursor is mapped the same way.
static void fitCanvas(GLFWwindow *window)
{
    if(g_renderer && g_canvas)
        gweni::capture::fitCanvasToWindow(window, g_manualScale, *g_renderer, *g_canvas, g_glfwInput);
}
static void framebuffer_size_callback(GLFWwindow *window, int, int)
{   fitCanvas(window); }
static void content_scale_callback(GLFWwindow *window, float, float)
{   fitCanvas(window); }

#define GWENI_CONTROL_DEMOS(X) \
    X(ButtonDemo) X(LabelDemo) X(LabelMultilineDemo) X(ProgressBarDemo) \
    X(GroupBoxDemo) X(ImagePanelDemo) X(StatusBarDemo) X(ComboBoxDemo) \
    X(TextBoxDemo) X(ListBoxDemo) X(CrossSplitterDemo) X(RadioButtonDemo) \
    X(CheckBoxDemo) X(NumericDemo) X(SliderDemo) X(MenuStripDemo) \
    X(WindowDemo) X(TreeControlDemo) X(PropertiesDemo) X(TabControlDemo) \
    X(ScrollControlDemo) X(PageControlDemo) X(CollapsibleListDemo) X(ColorPickerDemo) \
    X(UnicodeTextDemo) X(TextSizesDemo) X(TextDemo) X(LayoutDemo)

static const char *const g_controlNames[]=
{
#define GWENI_NAME(N) #N,
    GWENI_CONTROL_DEMOS(GWENI_NAME)
#undef GWENI_NAME
};

// The whole demo application, not one of its pages: the dock layout, the demo
// list, the output ListBox and the status bar. Kept out of GWENI_CONTROL_DEMOS
// so --shot-all and --scenario per-control stay the 28 single-control shots.
static const char *const g_dockedControlName="Demo";

static gweni::controls::Base *createControl(gweni::controls::Canvas *canvas, const std::string &which)
{
    if(which == g_dockedControlName) return canvas->newChild<gweni::Demo>();
#define TRY(N) if(which==#N) return canvas->newChild<gweni::N>();
    GWENI_CONTROL_DEMOS(TRY)
#undef TRY
    return nullptr;
}

static bool isKnownControl(const std::string &which)
{
    if(which == g_dockedControlName) return true;
    for(const char *name : g_controlNames)
        if(which == name) return true;
    return false;
}

static void printControlNames(std::ostream &out)
{
    out<<"Available controls:\n";
    for(const char *name : g_controlNames)
        out<<"  "<<name<<"\n";
    out<<"  "<<g_dockedControlName<<" (the whole docked demo)\n";
}

// The scripted input switches are listed briefly: they only make sense
// together, and the file header documents them in full.
static void printUsage(std::ostream &out)
{
    out<<"Usage: ControlTest [options]\n"
         "  --control <Name>   control demo to show (default ButtonDemo)\n"
         "  --size WxH         canvas size in logical units (default 640x480)\n"
         "  --scale <s>        device pixels per logical unit (default 1, or\n"
         "                     GWENI_SCALE); the framebuffer and PNGs are\n"
         "                     round(size*s), coordinates and listings logical\n"
         "  --frames N         frames to render before capturing (default 3)\n"
         "  --shot <path>      render offscreen and write one PNG, then exit\n"
         "  --shot-all         write one PNG per control demo, in this one run\n"
         "  --shot-dir <dir>   directory for shots (default /tmp)\n"
         "  --headless         force the surfaceless EGL context\n"
         "  --stats            print one key=value line per frame\n"
         "  --require-one-pass fail if a frame's layout takes more than one pass\n"
         "  --check-teardown   check the canvas renders empty again once every\n"
         "                     control has been destroyed; --control narrows it\n"
         "  --trace            log each scripted input on stderr\n"
         "  --scenario <name>  per-control | resize | docked | scroll | interaction\n"
         "                     | typing | clip | text-size | scale; resize prints\n"
         "                     listing_settled, mirror_settled, shrink_settled and\n"
         "                     fresh_matches, interaction one yes/no per repaint\n"
         "                     check, typing its keyboard checks, clip one yes/no\n"
         "                     per clipping check, text-size what a 24 -> 36 px\n"
         "                     Label font change wrote, scale whether a 1 -> 2 -> 1\n"
         "                     setScale round trip came back; all six fail on a no\n"
         "  --dump-bounds [<path>]  print the laid-out control tree as text,\n"
         "                     private children and hidden state included\n"
         "  --resources <dir>  load the skin, fonts and textures from <dir> (relative\n"
         "                     to the working directory, or absolute) instead of\n"
         "                     the resources/ next to the executable\n"
         "  --list             print the control demo names and exit\n"
         "Scripted input, applied in order:\n"
         "  --move/--press/--release/--click/--double-click X,Y, --scroll DX,DY\n"
         "  --key <name>       a named key (backspace, left, home, ...), a single\n"
         "                     character, or a chord such as ctrl+a or shift+left,\n"
         "                     sent as GLFW events through the GLFW translator\n"
         "  --type <utf8>      type the text, one GLFW char event per codepoint\n"
         "  --expect-text <utf8>  fail the run unless the focused control's\n"
         "                     text is exactly this\n"
         "The header of controltest.cpp documents them in full.\n";
}

//-----------------------------------------------------------------------------

using gweni::capture::Context;
using gweni::capture::Framebuffer;
using gweni::capture::createFramebuffer;
using gweni::capture::destroyContext;
using gweni::capture::destroyFramebuffer;
using gweni::capture::ensureDirectory;
using gweni::capture::initGlfwContext;
#if GWENI_EXAMPLE_EGL
using gweni::capture::initEglContext;
#endif
using gweni::capture::readFramebufferRgba;
using gweni::capture::writeFramebufferPng;
using gweni::capture::writeRgbaPng;

// The control that first re-dirtied layout in a frame, as Type#id, or "none".
static std::string redirtiedBy(const gweni::controls::LayoutStats &layout)
{
    if(!layout.redirtiedType)
        return "none";
    return std::string(layout.redirtiedType)+"#"+std::to_string(layout.redirtiedId);
}

// One grep-able line per frame, frame index first. The renderer numbers are read
// after renderCanvas(), so they describe the frame just flushed.
static void printFrameStats(int frame, const std::string &control,
    const gweni::controls::ChangeStats &changes,
    const gweni::renderer::OpenGLCore::FrameStats &gpu,
    const gweni::controls::LayoutStats &layout,
    const gweni::controls::UpdateStats &update)
{
    std::printf("frame=%d control=%s controls_changed=%zu created=%zu visibility=%zu"
        " geometry=%zu background=%zu text=%zu prim_slots_written=%zu text_slots_written=%zu"
        " upload_calls=%zu upload_bytes=%zu mirror_bytes=%zu draw_calls=%zu"
        " atlas_upload_calls=%zu atlas_upload_bytes=%zu"
        " layout_passes=%d redirtied_by=%s render_layout=%d visited=%zu z_visited=%zu prim_ids=%zu\n",
        frame, control.c_str(),
        changes.controls, changes.created, changes.visibility,
        changes.geometry, changes.background, changes.text,
        gpu.primitiveSlotsWritten, gpu.textSlotsWritten,
        gpu.uploadCalls, gpu.uploadBytes, gpu.mirrorBytes, gpu.drawCalls,
        gpu.atlasUploadCalls, gpu.atlasUploadBytes,
        layout.passes, redirtiedBy(layout).c_str(), layout.renderLayout ? 1 : 0,
        changes.visited, update.zVisited, gweni::primitiveIdCount()-gweni::releasedPrimitiveIdCount());
    std::fflush(stdout);
}

static bool parseSize(const std::string &value, gweni::Point &size)
{
    const size_t sep=value.find_first_of("xX");
    if(sep == std::string::npos) return false;

    const int w=std::atoi(value.substr(0, sep).c_str());
    const int h=std::atoi(value.substr(sep+1).c_str());
    if(w<=0 || h<=0) return false;

    size=gweni::Point(w, h);
    return true;
}

// A logical point or size in device pixels, snapped as the renderer snaps an
// edge.
static gweni::Point devicePixels(const gweni::Point &size, float scale)
{
    return gweni::Point(gweni::renderer::Base::snap(float(size.x), scale),
        gweni::renderer::Base::snap(float(size.y), scale));
}

static bool parsePoint(const std::string &value, int &x, int &y)
{
    const size_t sep=value.find(',');
    if(sep == std::string::npos) return false;

    x=std::atoi(value.substr(0, sep).c_str());
    y=std::atoi(value.substr(sep+1).c_str());
    return true;
}

// The canvas every capture draws on. The fill comes from the theme, so a
// "theme" block in resources/DefaultSkin.json moves it too.
static gweni::controls::Canvas *createCanvas(gweni::skin::Base *skin, const gweni::Point &size,
    float scale)
{
    gweni::controls::Canvas *canvas=new gweni::controls::Canvas(skin);
    canvas->setScale(scale);
    canvas->setSize(size.x, size.y);
    canvas->setDrawBackground(true);
    canvas->setBackgroundColor(gweni::skin::textured::tokyonight::Canvas.top);
    return canvas;
}

//-----------------------------------------------------------------------------
// Capture session: everything a scenario or a scripted run drives.
//-----------------------------------------------------------------------------
struct Session
{
    gweni::renderer::OpenGLCore *renderer=nullptr;
    gweni::controls::Canvas *canvas=nullptr;
    Framebuffer *fb=nullptr;
    gweni::Point size;                  // the canvas, logical units
    float scale=1.0f;                   // --scale
    gweni::Point pixels;                // the framebuffer, device pixels
    bool stats=false;
    bool freezeClock=false;             // as under --stats, without the lines
    bool requireOnePass=false;          // --require-one-pass
    int multiPassFrames=0;              // frames that broke it
    int expectFailures=0;               // --expect-text mismatches
    bool trace=false;
    bool writePng=true;
    int frames=3;                       // --frames: the scenarios' settle count
    std::string label;                  // the control --stats lines name
    int frame=0;
    gweni::Point mouse=gweni::Point(-1, -1);
    bool dumpBounds=false;              // --dump-bounds
    bool dumpPerShot=false;             // one listing per shot, named after it
    std::string dumpPath;               // --dump-bounds <path>; empty is stdout
};

//-----------------------------------------------------------------------------
// --dump-bounds: the laid-out tree as text
//-----------------------------------------------------------------------------

// Walks the children actually parented to each control, not getChildren(),
// which skips past an inner panel to the children placed in it: so a
// ScrollControl lists its inner panel (with the rows under it) and both of its
// scrollbars, hidden or not. Those private parts are where a layout that does
// not come back usually shows.
static void writeControlLine(std::ostream &out, gweni::controls::Base *control, int depth)
{
    const gweni::Rect &bounds=control->getBounds();
    const gweni::SizeFlags flags=control->getSizeFlags();

    out<<std::string(static_cast<size_t>(depth)*2, ' ')
       <<control->getTypeName()
       <<" bounds=("<<bounds.x<<", "<<bounds.y<<", "<<bounds.w<<", "<<bounds.h<<")"
       <<" dock="<<gweni::getDockPositionName(control->getDock())
       <<" size="<<gweni::sizeFlagName(flags.horizontal)
       <<","<<gweni::sizeFlagName(flags.vertical)
       <<" hidden="<<(control->hidden() ? "yes" : "no")
       <<"\n";

    for(auto &&child : control->getActualChildren())
        writeControlLine(out, child, depth+1);
}

static void writeBoundsListing(std::ostream &out, const Session &s)
{
    out<<"# control="<<s.label<<" size="<<s.size.x<<"x"<<s.size.y<<"\n";
    writeControlLine(out, s.canvas, 0);
}

static std::string boundsListing(const Session &s)
{
    std::ostringstream out;
    writeBoundsListing(out, s);
    return out.str();
}

// A control's place in the tree, as listed: each step is its type and its index
// among its parent's children, e.g. Canvas/Demo[0]/DockBase[1]/Text[0]. That
// names one control even where its siblings share its type.
static std::string childPath(const std::string &parent, gweni::controls::Base *control, size_t index)
{
    return parent+"/"+control->getTypeName()+"["+std::to_string(index)+"]";
}

// The same path read back out of a listing, for its line `lineIndex` (0-based,
// header included), from the indentation alone.
static std::string listingPathAt(const std::string &listing, size_t lineIndex)
{
    std::vector<std::string> stack;     // path up to each depth
    std::vector<size_t> childCount;     // children seen so far at each depth
    std::istringstream in(listing);
    std::string line;

    for(size_t i=0; std::getline(in, line) && i <= lineIndex; ++i)
    {
        if(line.empty() || line[0] == '#')
            continue;

        const size_t indent=line.find_first_not_of(' ');
        const size_t depth=indent/2;
        const std::string type=line.substr(indent, line.find(' ', indent)-indent);

        stack.resize(depth);
        childCount.resize(depth+1, 0);

        if(depth == 0)
            stack.push_back(type);
        else
            stack.push_back(stack[depth-1]+"/"+type+"["+std::to_string(childCount[depth]++)+"]");
        childCount.resize(depth+2, 0);
        childCount[depth+1]=0;
    }
    return stack.empty() ? std::string("(header)") : stack.back();
}

// The listing beside a shot: the same path with a .txt extension.
static std::string boundsPathFor(const std::string &shotPath)
{
    const size_t dot=shotPath.rfind('.');
    const size_t slash=shotPath.find_last_of('/');
    if(dot != std::string::npos && (slash == std::string::npos || dot>slash))
        return shotPath.substr(0, dot)+".txt";
    return shotPath+".txt";
}

static bool dumpBoundsTo(Session &s, const std::string &path)
{
    if(path.empty())
    {
        writeBoundsListing(std::cout, s);
        std::cout.flush();
        return true;
    }

    if(!ensureDirectory(fs::path(path).parent_path()))
        return false;

    std::ofstream out(path, std::ios::binary|std::ios::trunc);
    if(!out)
    {
        std::cerr<<"ControlTest: cannot write "<<path<<"\n";
        return false;
    }
    writeBoundsListing(out, s);
    out.close();

    std::cout<<"wrote "<<path<<"\n";
    return true;
}

// --require-one-pass: a frame whose layout did not converge in its first pass
// is named on stderr and fails the run at exit.
static void checkOnePass(Session &s, const std::string &frame)
{
    const gweni::controls::LayoutStats &layout=s.canvas->getLayoutStats();
    if(!s.requireOnePass || (layout.passes <= 1 && !layout.renderLayout))
        return;

    ++s.multiPassFrames;
    std::cerr<<"ControlTest: layout took more than one pass: control="<<s.label
             <<" frame="<<frame<<" layout_passes="<<layout.passes
             <<" render_layout="<<(layout.renderLayout ? 1 : 0)
             <<" redirtied_by="<<redirtiedBy(layout)
             <<(layout.capped ? " (capped)" : "")<<"\n";
}

static void renderFrames(Session &s, int count)
{
    for(int i=0; i<count; ++i)
    {
        gweni::controls::resetChangeStats();

        s.canvas->updateCanvas();
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        s.canvas->renderCanvas();

        if(s.stats)
            printFrameStats(s.frame, s.label, gweni::controls::getChangeStats(),
                s.renderer->getFrameStats(), s.canvas->getLayoutStats(),
                s.canvas->getUpdateStats());
        checkOnePass(s, std::to_string(s.frame));

        ++s.frame;

        // --stats wants a frame with no input to be genuinely idle, so there the
        // clock only moves when a scripted press moves it.
        if(!s.stats && !s.freezeClock)
            g_clockSeconds+=g_frameStep;
    }
}

static bool shoot(Session &s, const std::string &path)
{
    if(s.dumpBounds && s.dumpPerShot && !dumpBoundsTo(s, boundsPathFor(path)))
        return false;
    if(!s.writePng)
        return true;
    if(!ensureDirectory(fs::path(path).parent_path()))
        return false;
    if(!writeFramebufferPng(s.pixels.x, s.pixels.y, path))
        return false;

    std::cout<<"wrote "<<path<<"\n";
    return true;
}

// The whole of framebuffer_size_callback, called directly: no window manager is
// involved, so a scripted resize is just these two plus a new offscreen target.
// The size is logical; the target is that at the session's scale.
static bool resizeSession(Session &s, int width, int height)
{
    s.size=gweni::Point(width, height);
    s.pixels=devicePixels(s.size, s.scale);

    destroyFramebuffer(*s.fb);
    if(!createFramebuffer(*s.fb, s.pixels.x, s.pixels.y))
        return false;

    s.renderer->setViewRect(gweni::Rect(gweni::Point(0, 0), s.pixels));
    s.canvas->setSize(width, height);
    return true;
}

// A runtime scale change, as the content-scale callback makes one: the canvas
// keeps its logical size, and the target is that at the new scale.
static bool rescaleSession(Session &s, float scale)
{
    s.scale=scale;
    s.pixels=devicePixels(s.size, s.scale);

    destroyFramebuffer(*s.fb);
    if(!createFramebuffer(*s.fb, s.pixels.x, s.pixels.y))
        return false;

    s.renderer->setViewRect(gweni::Rect(gweni::Point(0, 0), s.pixels));
    s.canvas->setScale(scale);
    return true;
}

//-----------------------------------------------------------------------------
// Scripted input. The mouse goes in through Canvas's IInputEvent methods, which
// are the same gweni::input::onMouseMoved/onMouseClicked entry points the GLFW
// backend uses. The keyboard goes one step further out: synthesised GLFW key
// and char events through gweni::input::GLFW3, so its translation is covered
// too. There is no event loop behind either.
//-----------------------------------------------------------------------------

// --trace, on stderr so stdout stays parseable. Scripted coordinates are blind
// without it: this is how you tell "my 40,6 missed the button" from "the button
// did not repaint".
static void trace(const Session &s, const std::string &what)
{
    if(!s.trace)
        return;
    std::cerr<<"trace "<<what
             <<" clock="<<g_clockSeconds
             <<" hovered="<<(gweni::HoveredControl
                 ? gweni::HoveredControl->getTypeName() : "(none)");
    for(gweni::controls::Base *p=gweni::HoveredControl?gweni::HoveredControl->getParent():nullptr; p; p=p->getParent())
        std::cerr<<" < "<<p->getTypeName();
    std::cerr<<" focus="<<(gweni::KeyboardFocus
        ? gweni::KeyboardFocus->getTypeName() : "(none)")<<"\n";
}

static void moveMouse(Session &s, int x, int y)
{
    const gweni::Point to(x, y);
    const gweni::Point delta=(s.mouse.x<0 && s.mouse.y<0)
        ? gweni::Point(0, 0)
        : gweni::Point(to.x-s.mouse.x, to.y-s.mouse.y);

    s.mouse=to;
    s.canvas->inputMouseMoved(to.x, to.y, delta.x, delta.y);
    trace(s, "move "+std::to_string(to.x)+","+std::to_string(to.y));
}

static void mouseDown(Session &s, int x, int y, bool stepClock)
{
    if(stepClock)
        g_clockSeconds+=g_pressStep;
    moveMouse(s, x, y);
    s.canvas->inputMouseButton(0, true);
    trace(s, "down");
}

static void mouseUp(Session &s, int x, int y)
{
    moveMouse(s, x, y);
    s.canvas->inputMouseButton(0, false);
    trace(s, "up");
}

static void clickMouse(Session &s, int x, int y)
{
    mouseDown(s, x, y, true);
    mouseUp(s, x, y);
}

static void doubleClickMouse(Session &s, int x, int y)
{
    clickMouse(s, x, y);
    mouseDown(s, x, y, false);  // 0s after the first: inside any double-click window
    mouseUp(s, x, y);
}

// Named keys as the GLFW key codes a keyboard sends, so --key goes through
// gweni::input::GLFW3's translator exactly as the examples' key callback does.
static const struct { const char *name; int key; } g_namedKeys[]=
{
    {"return", GLFW_KEY_ENTER},          {"backspace", GLFW_KEY_BACKSPACE},
    {"delete", GLFW_KEY_DELETE},         {"left", GLFW_KEY_LEFT},
    {"right", GLFW_KEY_RIGHT},           {"shift", GLFW_KEY_LEFT_SHIFT},
    {"tab", GLFW_KEY_TAB},               {"space", GLFW_KEY_SPACE},
    {"home", GLFW_KEY_HOME},             {"end", GLFW_KEY_END},
    {"control", GLFW_KEY_LEFT_CONTROL},  {"up", GLFW_KEY_UP},
    {"down", GLFW_KEY_DOWN},             {"escape", GLFW_KEY_ESCAPE},
    {"alt", GLFW_KEY_LEFT_ALT},          {"command", GLFW_KEY_LEFT_SUPER},
    {"kp-enter", GLFW_KEY_KP_ENTER},
};

// The modifiers a --key chord may hold, with the mods bit GLFW reports for them.
static const struct { const char *name; int key; int mod; } g_chordModifiers[]=
{
    {"ctrl", GLFW_KEY_LEFT_CONTROL, GLFW_MOD_CONTROL},
    {"control", GLFW_KEY_LEFT_CONTROL, GLFW_MOD_CONTROL},
    {"shift", GLFW_KEY_LEFT_SHIFT, GLFW_MOD_SHIFT},
    {"alt", GLFW_KEY_LEFT_ALT, GLFW_MOD_ALT},
    {"command", GLFW_KEY_LEFT_SUPER, GLFW_MOD_SUPER},
};

// The GLFW translator, pointed at this session's canvas. Looked up per event,
// since the resize scenario replaces the canvas part way through.
static gweni::input::GLFW3 &glfwInput(Session &s)
{
    g_glfwInput.initialize(s.canvas);
    return g_glfwInput;
}

static std::string glfwKeyName(int key)
{
    for(const auto &named : g_namedKeys)
        if(named.key == key)
            return named.name;
    if(key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
        return std::string(1, static_cast<char>(key));
    if(key >= GLFW_KEY_0 && key <= GLFW_KEY_9)
        return std::string(1, static_cast<char>(key));
    return std::to_string(key);
}

// One synthesised GLFW key event, as glfwSetKeyCallback would deliver it.
static void glfwKey(Session &s, int key, int action, int mods)
{
    const gweni::input::GLFW3::KeyEvent event={key, 0, action, mods};
    glfwInput(s).processKeyEvent(event);
    trace(s, std::string("key ")+glfwKeyName(key)+(action == GLFW_PRESS ? " press" : " release")
        +" mods="+std::to_string(mods));
}

// One typed codepoint, as glfwSetCharCallback would deliver it.
static void glfwChar(Session &s, unsigned int codepoint)
{
    const bool used=glfwInput(s).processCharEvent(codepoint);
    char hex[16];
    std::snprintf(hex, sizeof(hex), "U+%04X", codepoint);
    trace(s, std::string("char ")+hex+(used ? " consumed" : " ignored"));
}

// A chord's last part: a named key or a single letter or digit. GLFW names a
// letter by its uppercase ASCII code, and a digit by its own.
static bool parseKeyPart(const std::string &part, int &key)
{
    for(const auto &named : g_namedKeys)
    {
        if(part == named.name)
        {
            key=named.key;
            return true;
        }
    }

    if(part.size() == 1 && std::isalpha(static_cast<unsigned char>(part[0])))
    {
        key=GLFW_KEY_A+(std::toupper(static_cast<unsigned char>(part[0]))-'A');
        return true;
    }
    if(part.size() == 1 && part[0] >= '0' && part[0] <= '9')
    {
        key=GLFW_KEY_0+(part[0]-'0');
        return true;
    }
    return false;
}

// --key: a single printable character is typed (the char callback); anything
// else is a key, or a chord of modifiers and a key joined by '+', sent as GLFW
// key events: each modifier pressed in order, the key pressed and released with
// every modifier's mods bit set, then the modifiers released in reverse. A
// chord's key goes only down the key path, as on a keyboard, where GLFW sends
// no char for Ctrl+letter.
static bool sendKey(Session &s, const std::string &name)
{
    if(name.size() == 1 && name[0]>=' ' && name[0]<'\x7f')
    {
        glfwChar(s, static_cast<unsigned char>(name[0]));
        return true;
    }

    std::vector<std::string> parts;
    for(size_t from=0;;)
    {
        const size_t plus=name.find('+', from);
        parts.push_back(name.substr(from, plus == std::string::npos ? std::string::npos : plus-from));
        if(plus == std::string::npos)
            break;
        from=plus+1;
    }

    std::vector<int> modifierKeys;
    std::vector<int> modifierBits;
    for(size_t i=0; i+1 < parts.size(); ++i)
    {
        bool found=false;
        for(const auto &modifier : g_chordModifiers)
        {
            if(parts[i] != modifier.name)
                continue;
            modifierKeys.push_back(modifier.key);
            modifierBits.push_back(modifier.mod);
            found=true;
            break;
        }
        if(!found)
        {
            std::cerr<<"ControlTest: --key "<<name<<": "<<parts[i]
                     <<" is not a modifier (ctrl, shift, alt, command)\n";
            return false;
        }
    }

    int key=0;
    if(!parseKeyPart(parts.back(), key))
    {
        std::cerr<<"ControlTest: unknown key name: "<<name<<"\n";
        return false;
    }

    int mods=0;
    for(size_t i=0; i < modifierKeys.size(); ++i)
    {
        glfwKey(s, modifierKeys[i], GLFW_PRESS, mods);
        mods|=modifierBits[i];
    }

    glfwKey(s, key, GLFW_PRESS, mods);
    glfwKey(s, key, GLFW_RELEASE, mods);

    for(size_t i=modifierKeys.size(); i-- > 0;)
    {
        glfwKey(s, modifierKeys[i], GLFW_RELEASE, mods);
        mods&=~modifierBits[i];
    }
    return true;
}

// UTF-8 to codepoints. A malformed sequence (a stray continuation byte, a
// truncated, overlong or surrogate encoding) decodes to U+FFFD and decoding
// resumes at the next byte.
static std::vector<unsigned int> decodeUtf8(const std::string &text)
{
    std::vector<unsigned int> codepoints;
    const size_t size=text.size();

    for(size_t i=0; i < size;)
    {
        const unsigned char lead=static_cast<unsigned char>(text[i]);
        size_t length=0;
        unsigned int codepoint=0;
        unsigned int minimum=0;

        if(lead < 0x80){ length=1; codepoint=lead; }
        else if((lead & 0xE0) == 0xC0){ length=2; codepoint=lead & 0x1F; minimum=0x80; }
        else if((lead & 0xF0) == 0xE0){ length=3; codepoint=lead & 0x0F; minimum=0x800; }
        else if((lead & 0xF8) == 0xF0){ length=4; codepoint=lead & 0x07; minimum=0x10000; }

        bool valid=length != 0 && i+length <= size;
        for(size_t j=1; valid && j < length; ++j)
        {
            const unsigned char next=static_cast<unsigned char>(text[i+j]);
            if((next & 0xC0) != 0x80)
                valid=false;
            else
                codepoint=(codepoint << 6) | (next & 0x3F);
        }
        if(valid && (codepoint < minimum || codepoint > 0x10FFFF
            || (codepoint >= 0xD800 && codepoint <= 0xDFFF)))
            valid=false;

        if(valid)
        {
            codepoints.push_back(codepoint);
            i+=length;
        }
        else
        {
            codepoints.push_back(0xFFFD);
            ++i;
        }
    }
    return codepoints;
}

// --type: the text one codepoint at a time through the char callback's path.
static void typeText(Session &s, const std::string &text)
{
    for(unsigned int codepoint : decodeUtf8(text))
        glfwChar(s, codepoint);
}

// The keyboard-focused control's text, if it has one (a Label, which every
// TextBox is).
static bool focusedText(std::string &text)
{
    const gweni::controls::Label *label=dynamic_cast<const gweni::controls::Label *>(gweni::KeyboardFocus);
    if(!label)
        return false;
    text=label->getText();
    return true;
}

// The text as a quoted C string, so a newline or an empty string shows.
static std::string quotedText(const std::string &text)
{
    std::string out="\"";
    for(char c : text)
    {
        if(c == '\n') out+="\\n";
        else if(c == '\t') out+="\\t";
        else if(c == '"' || c == '\\'){ out+='\\'; out+=c; }
        else out+=c;
    }
    return out+"\"";
}

// --expect-text: the focused control's text has to be exactly this. A mismatch
// names both on stderr and fails the run at exit, and the script carries on, so
// one run reports every step that went wrong.
static bool expectText(Session &s, const std::string &expected)
{
    std::string actual;
    const bool hasText=focusedText(actual);
    const bool match=hasText && actual == expected;
    trace(s, "expect-text "+quotedText(expected)+(match ? " ok" : " MISMATCH"));
    if(match)
        return true;

    ++s.expectFailures;
    if(!hasText)
        std::cerr<<"ControlTest: --expect-text "<<quotedText(expected)<<": the keyboard focus ("
                 <<(gweni::KeyboardFocus ? gweni::KeyboardFocus->getTypeName() : "none")
                 <<") has no text\n";
    else
        std::cerr<<"ControlTest: --expect-text mismatch: expected "<<quotedText(expected)
                 <<", the focused "<<gweni::KeyboardFocus->getTypeName()<<" has "<<quotedText(actual)<<"\n";
    return false;
}

static bool scrollWheel(Session &s, int delta)
{
    const bool used=s.canvas->inputMouseWheel(delta);
    trace(s, std::string("wheel ")+std::to_string(delta)+(used?" consumed":" ignored"));
    return used;
}

// Forgetting the hover between controls, so one capture entry cannot leave a
// control hovered for the next one.
static void forgetMouse(Session &s)
{
    if(s.mouse.x<0 && s.mouse.y<0)
        return;
    moveMouse(s, -1, -1);
    s.mouse=gweni::Point(-1, -1);
}

static gweni::controls::Base *addControl(Session &s, const std::string &name)
{
    forgetMouse(s);
    g_clockSeconds=0.0f;
    s.frame=0;
    s.label=name;

    gweni::controls::Base *control=createControl(s.canvas, name);
    if(control)
        control->setDock(gweni::DockPosition::Center);
    return control;
}

// Goes through the canvas so preDelete() releases the control's primitives; a
// bare delete leaves the renderer holding them.
static void dropControl(Session &s, gweni::controls::Base *control)
{
    control->delayedDelete();
    s.canvas->updateCanvas();
    checkOnePass(s, "drop");
}

//-----------------------------------------------------------------------------
// Scripted runs
//-----------------------------------------------------------------------------
enum class StepKind { Move, Down, Up, Click, DoubleClick, Scroll, Key, Type, ExpectText, Frames, Shot };

struct Step
{
    StepKind kind;
    int x=0, y=0;
    std::string text;
};

static std::string shotPathIn(const std::string &dir, const std::string &name)
{
    if(name.find('/') != std::string::npos)     // already a path, leave it alone
        return name;
    return dir.empty() ? name : dir+"/"+name;
}

static bool runSteps(Session &s, const std::vector<Step> &steps, const std::string &shotDir)
{
    for(const Step &step : steps)
    {
        switch(step.kind)
        {
        case StepKind::Move: moveMouse(s, step.x, step.y); break;
        case StepKind::Down: mouseDown(s, step.x, step.y, true); break;
        case StepKind::Up: mouseUp(s, step.x, step.y); break;
        case StepKind::Click: clickMouse(s, step.x, step.y); break;
        case StepKind::DoubleClick: doubleClickMouse(s, step.x, step.y); break;
        case StepKind::Scroll: (void)scrollWheel(s, step.y); break;
        case StepKind::Key: if(!sendKey(s, step.text)) return false; break;
        case StepKind::Type: typeText(s, step.text); break;
        case StepKind::ExpectText: (void)expectText(s, step.text); break;
        case StepKind::Frames: renderFrames(s, step.x); break;
        case StepKind::Shot:
            if(!shoot(s, shotPathIn(shotDir, step.text)))
                return false;
            break;
        }
    }
    return true;
}

//-----------------------------------------------------------------------------
// Scenarios
//-----------------------------------------------------------------------------
enum class Scenario { None, PerControl, Resize, Docked, Scroll, Interaction, Typing, Clip, TextSize, Scale };

static bool parseScenario(const std::string &name, Scenario &scenario)
{
    if(name == "per-control") scenario=Scenario::PerControl;
    else if(name == "resize") scenario=Scenario::Resize;
    else if(name == "docked") scenario=Scenario::Docked;
    else if(name == "scroll") scenario=Scenario::Scroll;
    else if(name == "interaction") scenario=Scenario::Interaction;
    else if(name == "typing") scenario=Scenario::Typing;
    else if(name == "clip") scenario=Scenario::Clip;
    else if(name == "text-size") scenario=Scenario::TextSize;
    else if(name == "scale") scenario=Scenario::Scale;
    else return false;
    return true;
}

static bool runPerControlScenario(Session &s, const std::string &shotDir)
{
    bool ok=true;
    for(const char *name : g_controlNames)
    {
        gweni::controls::Base *control=addControl(s, name);
        renderFrames(s, s.frames);
        if(!shoot(s, shotDir+"/"+name+".png"))
            ok=false;
        dropControl(s, control);
    }
    return ok;
}

// Which control owns each primitive and text id, by its listing path. Only the
// textured skin's Text renderer holds text ids; every other one holds primitive
// ids.
struct IdOwners
{
    std::map<size_t, std::string> primitives;
    std::map<size_t, std::string> texts;
};

static void collectIdOwners(gweni::controls::Base *control, const std::string &path, IdOwners &owners)
{
    if(const gweni::skin::Control *skinControl=control->getSkinControl())
    {
        const bool text=dynamic_cast<const gweni::skin::textured::Text *>(skinControl) != nullptr;
        for(size_t id : skinControl->getPrimitives())
            (text ? owners.texts : owners.primitives)[id]=path;
    }

    size_t index=0;
    for(auto &&child : control->getActualChildren())
        collectIdOwners(child, childPath(path, child, index++), owners);
}

// One 640x480 state of the resize scenario, as its three checks compare it.
struct ResizeState
{
    std::string name;                   // the shot's file name, for messages
    std::string listing;                // --dump-bounds' listing, always taken
    std::vector<uint8_t> pixels;
    gweni::renderer::OpenGLCore::MirrorSnapshot mirror;
    IdOwners owners;                    // who held each id, to name a mismatch
};

static void captureResizeState(Session &s, const std::string &path, ResizeState &state)
{
    state.name=fs::path(path).filename().string();
    state.listing=boundsListing(s);
    readFramebufferRgba(s.pixels.x, s.pixels.y, state.pixels);
    state.mirror=s.renderer->snapshotMirror();
    collectIdOwners(s.canvas, s.canvas->getTypeName(), state.owners);
}

// Prints the first line where two listings part, with the control it lists.
static bool listingsMatch(const ResizeState &a, const ResizeState &b)
{
    if(a.listing == b.listing)
        return true;

    std::istringstream left(a.listing), right(b.listing);
    std::string leftLine, rightLine;
    size_t line=0;

    for(;; ++line)
    {
        const bool haveLeft=static_cast<bool>(std::getline(left, leftLine));
        const bool haveRight=static_cast<bool>(std::getline(right, rightLine));
        if(!haveLeft) leftLine="(end of listing)";
        if(!haveRight) rightLine="(end of listing)";
        if(leftLine != rightLine || !haveLeft || !haveRight)
            break;
    }

    const size_t linesInA=static_cast<size_t>(std::count(a.listing.begin(), a.listing.end(), '\n'));
    const std::string &listing=(line < linesInA) ? a.listing : b.listing;
    std::cerr<<"ControlTest: listing of "<<b.name<<" differs from "<<a.name
             <<" first at line "<<line+1<<", control "<<listingPathAt(listing, line)<<"\n"
             <<"  "<<a.name<<": "<<leftLine<<"\n"
             <<"  "<<b.name<<": "<<rightLine<<"\n";
    return false;
}

// The largest x/y move of any glyph vertex of a text run that kept its glyph
// count: a run that was moved and moved back by `x += delta` (text defect B)
// differs only there, by float rounding.
static float largestGlyphMove(const std::vector<uint8_t> &before, const std::vector<uint8_t> &after)
{
    float largest=0.0f;
    const size_t count=(before.size()-1)/sizeof(ftgl::glyph_vertex_t);
    for(size_t i=0; i<count; ++i)
    {
        ftgl::glyph_vertex_t a, b;
        std::memcpy(&a, before.data()+1+i*sizeof(a), sizeof(a));
        std::memcpy(&b, after.data()+1+i*sizeof(b), sizeof(b));
        largest=std::max(largest, std::max(std::fabs(a.x-b.x), std::fabs(a.y-b.y)));
    }
    return largest;
}

static size_t reportMirrorKind(const char *kind, const ResizeState &beforeState,
    const std::map<size_t, std::vector<uint8_t>> &before, const ResizeState &afterState,
    const std::map<size_t, std::vector<uint8_t>> &after, const std::map<size_t, std::string> &owners,
    bool text)
{
    static const size_t maxReported=10;
    size_t reported=0;

    std::vector<size_t> ids;
    for(const auto &entry : before) ids.push_back(entry.first);
    for(const auto &entry : after)
        if(before.find(entry.first) == before.end()) ids.push_back(entry.first);
    std::sort(ids.begin(), ids.end());

    size_t differing=0;
    for(size_t id : ids)
    {
        const auto was=before.find(id);
        const auto now=after.find(id);
        std::string how;

        if(was == before.end()) how="live only in "+afterState.name;
        else if(now == after.end()) how="live only in "+beforeState.name;
        else if(was->second == now->second) continue;
        else if(was->second.size() != now->second.size()) how="contents differ (size)";
        else if(text && was->second[0] != now->second[0]) how="hidden state differs";
        else if(text)
            how="glyph vertexes differ, largest x/y move "+std::to_string(largestGlyphMove(was->second, now->second));
        else how="slot bytes differ";

        ++differing;
        if(reported++ == maxReported)
            std::cerr<<"  ... (only the first "<<maxReported<<" "<<kind<<" ids are shown)\n";
        if(reported > maxReported)
            continue;

        const auto owner=owners.find(id);
        std::cerr<<"  "<<kind<<" id="<<id<<" "<<how<<", owner "
                 <<(owner != owners.end() ? owner->second : std::string("(not found)"))<<"\n";
    }
    return differing;
}

// The renderer's CPU mirror, id by id, of an earlier state against a later one
// (before the grow against after the shrink). Owners are named from the tree
// as it stood in the later one.
static bool mirrorsMatch(const ResizeState &before, const ResizeState &after)
{
    if(before.mirror.primitives == after.mirror.primitives && before.mirror.texts == after.mirror.texts)
        return true;

    const IdOwners &owners=after.owners;
    std::cerr<<"ControlTest: renderer mirror of "<<after.name
             <<" differs from "<<before.name<<":\n";
    const size_t primitives=reportMirrorKind("primitive", before, before.mirror.primitives,
        after, after.mirror.primitives, owners.primitives, false);
    const size_t texts=reportMirrorKind("text", before, before.mirror.texts,
        after, after.mirror.texts, owners.texts, true);
    std::cerr<<"  "<<primitives<<" primitive ids and "<<texts<<" text ids differ\n";
    return false;
}

static bool pixelsMatch(const ResizeState &a, const ResizeState &b)
{
    if(a.pixels == b.pixels)
        return true;

    size_t differing=0;
    for(size_t i=0; i<a.pixels.size() && i<b.pixels.size(); i+=4)
        if(std::memcmp(&a.pixels[i], &b.pixels[i], 4) != 0)
            ++differing;
    std::cerr<<"ControlTest: "<<b.name<<" differs from "<<a.name<<" in "
             <<differing<<" pixels\n";
    return false;
}

// Replaces the session's canvas with a new one, built directly at the
// session's size. The old one goes first, so the new one gets the same
// (lowest-first) primitive ids a canvas built at start-up got.
static void replaceCanvas(Session &s)
{
    forgetMouse(s);
    gweni::skin::Base *skin=s.canvas->getSkin();
    delete s.canvas;
    s.canvas=createCanvas(skin, s.size, s.scale);
    g_canvas=s.canvas;
}

// Three shots around a grow-then-shrink, plus a reference from a canvas built
// directly at 640x480 once the cycle is over. All three 640x480 states have to
// be the same, checked cheapest first: the listings, then the renderer mirror
// id by id (before the grow against after the shrink), then the pixels.
// Anything that does not re-settle on shrink shows up here and nowhere else in
// the harness.
static bool runResizeScenario(Session &s, const std::string &controlName,
    const std::string &shotDir, bool &settled)
{
    const std::string fresh=shotDir+"/resize-00-fresh-640x480.png";
    const std::string first=shotDir+"/resize-01-640x480.png";
    const std::string grown=shotDir+"/resize-02-1024x768.png";
    const std::string shrunk=shotDir+"/resize-03-640x480.png";

    ResizeState freshState, firstState, shrunkState;

    if(!resizeSession(s, 640, 480))
        return false;

    gweni::controls::Base *control=addControl(s, controlName);

    renderFrames(s, s.frames);
    if(!shoot(s, first)) return false;
    captureResizeState(s, first, firstState);

    if(!resizeSession(s, 1024, 768)) return false;
    renderFrames(s, s.frames);
    if(!shoot(s, grown)) return false;

    if(!resizeSession(s, 640, 480)) return false;
    renderFrames(s, s.frames);
    if(!shoot(s, shrunk)) return false;
    captureResizeState(s, shrunk, shrunkState);

    dropControl(s, control);

    replaceCanvas(s);
    control=addControl(s, controlName);
    renderFrames(s, s.frames);
    if(!shoot(s, fresh)) return false;
    captureResizeState(s, fresh, freshState);
    dropControl(s, control);

    // Every comparison runs, so a failing run names all that differs; each
    // message says which pair it is about.
    const bool listingSettled=listingsMatch(firstState, shrunkState);
    bool freshMatches=listingsMatch(freshState, firstState);
    freshMatches=listingsMatch(freshState, shrunkState) && freshMatches;

    const bool mirrorSettled=mirrorsMatch(firstState, shrunkState);

    const bool shrinkSettled=pixelsMatch(firstState, shrunkState);
    freshMatches=pixelsMatch(freshState, firstState) && freshMatches;
    freshMatches=pixelsMatch(freshState, shrunkState) && freshMatches;

    settled=listingSettled && mirrorSettled && shrinkSettled && freshMatches;

    std::cout<<"resize control="<<controlName
             <<" listing_settled="<<(listingSettled?"yes":"no")
             <<" mirror_settled="<<(mirrorSettled?"yes":"no")
             <<" shrink_settled="<<(shrinkSettled?"yes":"no")
             <<" fresh_matches="<<(freshMatches?"yes":"no")<<"\n";
    if(!settled)
        std::cerr<<"ControlTest: the layout did not come back to its 640x480 state\n";
    return true;
}

// Three shots around a 1 -> 2 -> 1 Canvas::setScale, plus a reference from a
// canvas built directly at scale 2. A scale change lays the whole tree out
// again and re-emits every primitive, so the last 1x state has to be the first
// one byte for byte (listing, renderer mirror, pixels), and the 2x one what a
// start at --scale 2 draws.
static bool runScaleScenario(Session &s, const std::string &controlName,
    const std::string &shotDir, bool &settled)
{
    const std::string fresh=shotDir+"/scale-00-fresh-2x.png";
    const std::string first=shotDir+"/scale-01-1x.png";
    const std::string doubled=shotDir+"/scale-02-2x.png";
    const std::string back=shotDir+"/scale-03-1x.png";

    ResizeState freshState, firstState, doubledState, backState;

    if(!rescaleSession(s, 1.0f) || !resizeSession(s, 640, 480))
        return false;

    gweni::controls::Base *control=addControl(s, controlName);

    renderFrames(s, s.frames);
    if(!shoot(s, first)) return false;
    captureResizeState(s, first, firstState);

    if(!rescaleSession(s, 2.0f)) return false;
    renderFrames(s, s.frames);
    if(!shoot(s, doubled)) return false;
    captureResizeState(s, doubled, doubledState);

    if(!rescaleSession(s, 1.0f)) return false;
    renderFrames(s, s.frames);
    if(!shoot(s, back)) return false;
    captureResizeState(s, back, backState);

    dropControl(s, control);

    // built at scale 2 from the start, onto a 2x target
    if(!rescaleSession(s, 2.0f)) return false;
    replaceCanvas(s);
    control=addControl(s, controlName);
    renderFrames(s, s.frames);
    if(!shoot(s, fresh)) return false;
    captureResizeState(s, fresh, freshState);
    dropControl(s, control);

    const bool listingSettled=listingsMatch(firstState, backState);
    const bool mirrorSettled=mirrorsMatch(firstState, backState);
    const bool pixelsSettled=pixelsMatch(firstState, backState);
    bool freshMatches=listingsMatch(doubledState, freshState);
    freshMatches=pixelsMatch(doubledState, freshState) && freshMatches;

    settled=listingSettled && mirrorSettled && pixelsSettled && freshMatches;

    std::cout<<"scale control="<<controlName
             <<" listing_settled="<<(listingSettled?"yes":"no")
             <<" mirror_settled="<<(mirrorSettled?"yes":"no")
             <<" pixels_settled="<<(pixelsSettled?"yes":"no")
             <<" fresh_matches="<<(freshMatches?"yes":"no")<<"\n";
    if(!settled)
        std::cerr<<"ControlTest: the canvas did not come back from a 1 -> 2 -> 1 scale change\n";
    return true;
}

static bool runDockedScenario(Session &s, const std::string &shotDir)
{
    if(!resizeSession(s, 1024, 768))
        return false;

    gweni::controls::Base *control=addControl(s, g_dockedControlName);
    renderFrames(s, s.frames);
    const bool ok=shoot(s, shotDir+"/docked-1024x768.png");
    dropControl(s, control);
    return ok;
}

// The demos that overflow their own bounds, idle and scrolled. Content is cut
// at each clipping control's edges, so the scrolled shots show what is left
// inside the viewports.
static const char *const g_scrollDemos[]=
{
    "ScrollControlDemo", "PageControlDemo", "ListBoxDemo",
    "TreeControlDemo", "PropertiesDemo"
};

// ScrollControl::onMouseWheeled nudges by delta/60 of a nudge-per-content-size
// fraction, so one notch moves almost nothing; this is a deliberate shove.
static const int g_scrollNotch=-30;
static const int g_scrollNotches=10;

static void collectScrollControls(gweni::controls::Base *control,
    std::vector<gweni::controls::ScrollControl *> &found)
{
    if(control->hidden())
        return;
    if(auto *scroll=dynamic_cast<gweni::controls::ScrollControl *>(control))
        found.push_back(scroll);
    for(auto &&child : control->getChildren())
        collectScrollControls(child, found);
}

// Canvas::inputMouseWheel only ever reaches the hovered control, and a demo's
// scrollable regions are small and nowhere near the canvas centre, so aim at
// each one in turn rather than guessing a coordinate.
static int scrollEveryRegion(Session &s, gweni::controls::Base *control)
{
    std::vector<gweni::controls::ScrollControl *> regions;
    collectScrollControls(control, regions);

    int consumed=0;
    for(gweni::controls::ScrollControl *region : regions)
    {
        // Several demos lay their scroll regions out far wider than the canvas,
        // so the region's own centre can be off-screen and hover nothing: aim at
        // the middle of the part that is actually on the canvas.
        const gweni::Point origin=region->localPosToCanvas();
        const int left=std::max(0, origin.x);
        const int top=std::max(0, origin.y);
        const int right=std::min(s.size.x-1, origin.x+region->getSize().x);
        const int bottom=std::min(s.size.y-1, origin.y+region->getSize().y);
        if(left>right || top>bottom)
            continue;

        moveMouse(s, (left+right)/2, (top+bottom)/2);

        bool moved=false;
        for(int i=0; i<g_scrollNotches; ++i)
            moved=scrollWheel(s, g_scrollNotch) || moved;
        if(moved)
            ++consumed;
    }

    std::cout<<"scroll control="<<s.label<<" regions="<<regions.size()
             <<" scrolled="<<consumed<<"\n";
    return consumed;
}

static bool runScrollScenario(Session &s, const std::string &shotDir)
{
    bool ok=true;
    for(const char *name : g_scrollDemos)
    {
        gweni::controls::Base *control=addControl(s, name);

        renderFrames(s, s.frames);
        if(!shoot(s, shotDir+"/scroll-"+name+"-idle.png"))
            ok=false;

        scrollEveryRegion(s, control);

        renderFrames(s, s.frames);
        if(!shoot(s, shotDir+"/scroll-"+name+"-scrolled.png"))
            ok=false;

        dropControl(s, control);
    }
    return ok;
}

//-----------------------------------------------------------------------------
// --scenario text-size: a Label's font size changed at runtime
//
// Both sizes are at or above OpenGLCore's SDF threshold (by default), so both are
// drawn from the face's one SDF atlas: the change has to rasterise and upload no
// glyph, and rewrite only that Label's own glyph slots.
//-----------------------------------------------------------------------------

static const int g_textSizeFrom=24;
static const int g_textSizeTo=36;
static const int g_textSizeKept=48;

static std::vector<uint8_t> regionPixels(const Session &s, gweni::controls::Base *control);

static gweni::Font openSansAt(int size)
{
    gweni::Font font;
    font.facename="OpenSans";
    font.size=static_cast<float>(size);
    return font;
}

static bool runTextSizeScenario(Session &s, const std::string &shotDir, bool &passed)
{
    const char *const sample="Sphinx gyp 0123456789 &?!";

    forgetMouse(s);
    g_clockSeconds=0.0f;
    s.frame=0;
    s.label="text-size";

    // a Label in the skin's default font beside it, whose run must not be touched
    gweni::controls::Label *other=s.canvas->newChild<gweni::controls::Label>();
    other->setAlignment(gweni::Alignment::Absolute);
    other->setText("unchanged 11 px label");
    other->sizeToContents();
    other->setPos(20, 8);

    // Sizes are set through Label::setFont(face, size, bold), which frees the
    // Label's previous font: each size of the face shares the one SDF atlas, so
    // that must leave the other sizes drawing.
    gweni::controls::Label *label=s.canvas->newChild<gweni::controls::Label>();
    label->setAlignment(gweni::Alignment::Absolute);
    label->setFont("OpenSans", g_textSizeFrom, false);
    label->setText(sample);
    label->sizeToContents();
    label->setPos(20, 30);

    // another size of the same face below it, which neither the change nor the
    // changed Label's deletion may touch
    gweni::controls::Label *kept=s.canvas->newChild<gweni::controls::Label>();
    kept->setAlignment(gweni::Alignment::Absolute);
    kept->setFont("OpenSans", g_textSizeKept, false);
    kept->setText(sample);
    kept->sizeToContents();
    kept->setPos(20, 100);

    // the extra frame leaves every glyph the first frames needed on the GPU
    renderFrames(s, s.frames+1);

    const std::string fromShot=shotDir+"/text-size-"+std::to_string(g_textSizeFrom)+".png";
    const std::string toShot=shotDir+"/text-size-"+std::to_string(g_textSizeTo)+".png";
    const std::string deletedShot=shotDir+"/text-size-deleted.png";
    std::vector<uint8_t> fromPixels, toPixels;

    bool ok=shoot(s, fromShot);
    readFramebufferRgba(s.size.x, s.size.y, fromPixels);

    const gweni::Point fromSize=label->getSize();
    const std::vector<uint8_t> keptPixels=regionPixels(s, kept);
    const unsigned int atlas=s.renderer->getFontAtlasName(openSansAt(g_textSizeKept));

    label->setFont("OpenSans", g_textSizeTo, false);
    label->sizeToContents();

    // The change and however many frames it takes to settle: the most glyph slots
    // any one of them writes, and everything they upload to the font atlases.
    size_t atlasUploadCalls=0, atlasUploadBytes=0;
    size_t textSlotsWritten=0, primSlotsWritten=0, uploadBytes=0;

    for(int i=0; i<s.frames; ++i)
    {
        renderFrames(s, 1);

        const gweni::renderer::OpenGLCore::FrameStats &gpu=s.renderer->getFrameStats();
        atlasUploadCalls+=gpu.atlasUploadCalls;
        atlasUploadBytes+=gpu.atlasUploadBytes;
        textSlotsWritten=std::max(textSlotsWritten, gpu.textSlotsWritten);
        primSlotsWritten+=gpu.primitiveSlotsWritten;
        uploadBytes+=gpu.uploadBytes;
    }

    // and one more with nothing to do, which has to write nothing
    renderFrames(s, 1);
    const size_t idleSlots=s.renderer->getFrameStats().textSlotsWritten
        +s.renderer->getFrameStats().primitiveSlotsWritten;

    ok=shoot(s, toShot) && ok;
    readFramebufferRgba(s.size.x, s.size.y, toPixels);

    const gweni::Point toSize=label->getSize();
    const bool keptAfterChange=(regionPixels(s, kept) == keptPixels);

    // the changed Label goes, freeing its font a second time
    label->delayedDelete();
    label=nullptr;
    for(int i=0; i<s.frames; ++i)
    {
        renderFrames(s, 1);
        atlasUploadCalls+=s.renderer->getFrameStats().atlasUploadCalls;
        atlasUploadBytes+=s.renderer->getFrameStats().atlasUploadBytes;
    }
    ok=shoot(s, deletedShot) && ok;

    const bool keptAfterDelete=(regionPixels(s, kept) == keptPixels);
    const bool sameAtlas=(atlas != 0) && (s.renderer->getFontAtlasName(openSansAt(g_textSizeKept)) == atlas);

    // the sample is one glyph slot per codepoint, spaces included
    const size_t glyphs=std::strlen(sample);

    const bool sdf=(s.renderer->getTextMode(openSansAt(g_textSizeFrom)) == gweni::renderer::GLTextMode::Sdf)
        && (s.renderer->getTextMode(openSansAt(g_textSizeTo)) == gweni::renderer::GLTextMode::Sdf)
        && (s.renderer->getTextMode(openSansAt(g_textSizeKept)) == gweni::renderer::GLTextMode::Sdf);
    const bool noAtlasUpload=(atlasUploadCalls == 0);
    const bool onlyThatRun=(textSlotsWritten > 0) && (textSlotsWritten <= glyphs);
    const bool grew=(toSize.x > fromSize.x) && (toSize.y > fromSize.y);
    const bool repainted=(fromPixels != toPixels);
    const bool settled=(idleSlots == 0);
    const bool keptDrawing=keptAfterChange && keptAfterDelete && sameAtlas;

    passed=sdf && noAtlasUpload && onlyThatRun && grew && repainted && settled && keptDrawing;

    std::cout<<"text-size from="<<g_textSizeFrom<<" to="<<g_textSizeTo
             <<" sdf="<<(sdf?"yes":"no")
             <<" atlas_upload_calls="<<atlasUploadCalls<<" atlas_upload_bytes="<<atlasUploadBytes
             <<" text_slots_written="<<textSlotsWritten<<" run_glyphs="<<glyphs
             <<" prim_slots_written="<<primSlotsWritten<<" upload_bytes="<<uploadBytes
             <<" size="<<fromSize.x<<"x"<<fromSize.y<<"->"<<toSize.x<<"x"<<toSize.y
             <<" repainted="<<(repainted?"yes":"no")
             <<" idle_slots_written="<<idleSlots
             <<" kept="<<g_textSizeKept
             <<" kept_after_change="<<(keptAfterChange?"yes":"no")
             <<" kept_after_delete="<<(keptAfterDelete?"yes":"no")
             <<" same_atlas="<<(sameAtlas?"yes":"no")<<"\n";
    if(!passed)
        std::cerr<<"ControlTest: text-size: the "<<g_textSizeFrom<<" -> "<<g_textSizeTo
                 <<" px change "<<(!sdf ? "is not in the SDF range (GWENI_SDF_TEXT_MIN?)"
                     : !noAtlasUpload ? "uploaded to a font atlas"
                     : !onlyThatRun ? "wrote glyph slots outside the Label's run"
                     : !grew ? "did not grow the Label"
                     : !repainted ? "did not repaint"
                     : !settled ? "left a frame that still writes slots"
                     : !sameAtlas ? "or the Label's deletion replaced the face's SDF atlas"
                     : "or the Label's deletion changed the "+std::to_string(g_textSizeKept)+" px Label")<<"\n";

    other->delayedDelete();
    kept->delayedDelete();
    s.canvas->updateCanvas();
    checkOnePass(s, "drop");
    return ok;
}

//-----------------------------------------------------------------------------
// --scenario interaction: hover, press, disabled and focus repaint
//
// Every interaction state change ends in Base::redraw(), which has to mark the
// control dirty or the skin never re-picks its look. Each check compares the
// pixels inside one control's laid-out bounds, so an unrelated pixel elsewhere
// (a label colour, a caret) cannot make it pass.
//-----------------------------------------------------------------------------

// The first visible control of exactly this type, depth-first in child order,
// i.e. the first one --dump-bounds lists.
static gweni::controls::Base *findControl(gweni::controls::Base *control, const char *type, int skip=0)
{
    std::vector<gweni::controls::Base *> stack(1, control);
    while(!stack.empty())
    {
        gweni::controls::Base *at=stack.back();
        stack.pop_back();
        if(at->hidden())
            continue;
        if(std::strcmp(at->getTypeName(), type) == 0 && skip-- == 0)
            return at;

        const auto &children=at->getActualChildren();
        for(auto it=children.rbegin(); it != children.rend(); ++it)
            stack.push_back(*it);
    }
    return nullptr;
}

static gweni::Point centreOf(gweni::controls::Base *control)
{
    const gweni::Point origin=control->localPosToCanvas();
    return gweni::Point(origin.x+control->getWidth()/2, origin.y+control->getHeight()/2);
}

// The RGBA pixels inside a control's bounds (snapped to device pixels as the
// renderer snaps them), clipped to the framebuffer.
static std::vector<uint8_t> regionPixels(const Session &s, gweni::controls::Base *control)
{
    std::vector<uint8_t> frame;
    readFramebufferRgba(s.pixels.x, s.pixels.y, frame);

    const gweni::Point origin=control->localPosToCanvas();
    const gweni::Point topLeft=devicePixels(origin, s.scale);
    const gweni::Point bottomRight=devicePixels(origin+control->getSize(), s.scale);
    const int left=std::max(0, topLeft.x);
    const int top=std::max(0, topLeft.y);
    const int right=std::min(s.pixels.x, bottomRight.x);
    const int bottom=std::min(s.pixels.y, bottomRight.y);

    std::vector<uint8_t> region;
    for(int y=top; y<bottom; ++y)
    {
        const auto row=frame.begin()+(static_cast<size_t>(y)*s.pixels.x+left)*4;
        region.insert(region.end(), row, row+static_cast<size_t>(std::max(0, right-left))*4);
    }
    return region;
}

// Renders the frames a step needs to settle and returns the control's pixels;
// a shot is written for the record.
static std::vector<uint8_t> settledRegion(Session &s, gweni::controls::Base *control,
    const std::string &shotDir, const std::string &name, bool &ok)
{
    renderFrames(s, s.frames);
    if(!shoot(s, shotDir+"/interaction-"+name+".png"))
        ok=false;
    return regionPixels(s, control);
}

// One named assertion: a line on stderr when it fails, and a yes/no for the
// summary line.
struct InteractionChecks
{
    std::string scenario="interaction";     // names the failures on stderr
    std::vector<std::pair<std::string, bool>> results;

    void check(const std::string &name, bool passed, const std::string &why)
    {
        results.emplace_back(name, passed);
        if(!passed)
            std::cerr<<"ControlTest: "<<scenario<<" "<<name<<" failed: "<<why<<"\n";
    }

    bool allPassed() const
    {
        for(const auto &result : results)
            if(!result.second)
                return false;
        return true;
    }
};

// A hover transition dirties the control left (here ButtonDemo, which owns no
// primitive), the Button entered and the Button's label Text. Only the two
// Button-side ones can write a primitive slot, and a button's background is a
// single RoundedPrimitive slot. 4 leaves room for a left control that is a
// button too, plus an overlay primitive each, while anything that dirties
// ancestors or the whole tree (19 controls here) is far over it.
static const size_t g_hoverBound=4;

static void checkButton(Session &s, const std::string &shotDir, InteractionChecks &checks, bool &ok)
{
    gweni::controls::Base *demo=addControl(s, "ButtonDemo");
    renderFrames(s, s.frames);

    gweni::controls::Base *button=findControl(demo, "Button");
    if(!button)
    {
        checks.check("button_found", false, "ButtonDemo has no visible Button");
        dropControl(s, demo);
        return;
    }

    const gweni::Point on=centreOf(button);
    const gweni::Point off(s.size.x-1, s.size.y-1);    // ButtonDemo, no button

    moveMouse(s, off.x, off.y);
    const std::vector<uint8_t> idle=settledRegion(s, button, shotDir, "button-idle", ok);

    // The hover frame alone, so its counters are the transition's.
    moveMouse(s, on.x, on.y);
    renderFrames(s, 1);
    const gweni::controls::ChangeStats hoverChanges=gweni::controls::getChangeStats();
    const size_t hoverSlots=s.renderer->getFrameStats().primitiveSlotsWritten;
    const std::vector<uint8_t> hover=settledRegion(s, button, shotDir, "button-hover", ok);

    mouseDown(s, on.x, on.y, true);
    const std::vector<uint8_t> press=settledRegion(s, button, shotDir, "button-press", ok);

    // Let go off the button, so it is not clicked. The input handler keeps the
    // pressed control hovered until the mouse next moves, so move once more.
    mouseUp(s, off.x, off.y);
    moveMouse(s, off.x-1, off.y);
    const std::vector<uint8_t> released=settledRegion(s, button, shotDir, "button-released", ok);

    // A frame with no input once everything has settled must do nothing.
    renderFrames(s, 1);
    const gweni::controls::ChangeStats idleChanges=gweni::controls::getChangeStats();
    const gweni::renderer::OpenGLCore::FrameStats idleGpu=s.renderer->getFrameStats();

    button->setDisabled(true);
    const std::vector<uint8_t> disabled=settledRegion(s, button, shotDir, "button-disabled", ok);
    button->setDisabled(false);
    const std::vector<uint8_t> enabled=settledRegion(s, button, shotDir, "button-enabled", ok);

    dropControl(s, demo);

    checks.check("button_hover", hover != idle, "hovering the Button left its pixels as they were idle");
    checks.check("button_press", press != idle && press != hover,
        "pressing the Button did not change its pixels from idle and hover");
    checks.check("button_unhover", released == idle,
        "moving off and releasing did not bring the Button back to its idle look");
    checks.check("hover_bounded",
        hoverChanges.background >= 1 && hoverChanges.background <= g_hoverBound
        && hoverSlots >= 1 && hoverSlots <= g_hoverBound && hoverChanges.controls <= g_hoverBound,
        "the hover frame had controls_changed="+std::to_string(hoverChanges.controls)
        +" background="+std::to_string(hoverChanges.background)
        +" prim_slots_written="+std::to_string(hoverSlots)
        +"; want background and prim_slots_written in 1.."+std::to_string(g_hoverBound)
        +" and controls_changed at most "+std::to_string(g_hoverBound));
    checks.check("idle_zero",
        idleChanges.controls == 0 && idleGpu.primitiveSlotsWritten == 0 && idleGpu.textSlotsWritten == 0,
        "a settled frame with no input had controls_changed="+std::to_string(idleChanges.controls)
        +" prim_slots_written="+std::to_string(idleGpu.primitiveSlotsWritten)
        +" text_slots_written="+std::to_string(idleGpu.textSlotsWritten));
    checks.check("button_disabled", disabled != idle && enabled == idle,
        "setDisabled(true) did not change the Button, or setDisabled(false) did not restore it");
}

static void checkFocusHandOff(Session &s, const std::string &shotDir, InteractionChecks &checks, bool &ok)
{
    gweni::controls::Base *demo=addControl(s, "TextBoxDemo");
    renderFrames(s, s.frames);

    gweni::controls::Base *first=findControl(demo, "TextBox");
    gweni::controls::Base *second=findControl(demo, "TextBox", 1);
    if(!first || !second)
    {
        checks.check("textbox_found", false, "TextBoxDemo has fewer than two visible TextBoxes");
        dropControl(s, demo);
        return;
    }

    const std::vector<uint8_t> unfocused=settledRegion(s, first, shotDir, "textbox-unfocused", ok);

    // The mouse is moved onto TextBox 2 before the click, so TextBox 1 has its
    // mouse-leave repaint behind it and only the focus hand-off can change it.
    const gweni::Point firstAt=centreOf(first);
    const gweni::Point secondAt=centreOf(second);
    clickMouse(s, firstAt.x, firstAt.y);
    moveMouse(s, secondAt.x, secondAt.y);
    const std::vector<uint8_t> focused=settledRegion(s, first, shotDir, "textbox-focused", ok);

    clickMouse(s, secondAt.x, secondAt.y);
    const std::vector<uint8_t> handedOff=settledRegion(s, first, shotDir, "textbox-handoff", ok);

    dropControl(s, demo);

    checks.check("textbox_focus", focused != unfocused,
        "clicking TextBox 1 did not change its pixels");
    checks.check("focus_handoff", handedOff == unfocused,
        "after clicking TextBox 2, TextBox 1 does not look unfocused again");
}

// Walks the actual children, since a TreeNode's getParent() is the control it
// was added to and not the inner panel that holds it and its siblings.
static bool findOpenNode(gweni::controls::Base *control, gweni::controls::Base *&toggle,
    gweni::controls::Base *&below)
{
    const auto &children=control->getActualChildren();
    for(auto it=children.begin(); it != children.end(); ++it)
    {
        gweni::controls::Base *child=*it;
        if(child->hidden())
            continue;

        auto next=std::next(it);
        if(std::strcmp(child->getTypeName(), "TreeNode") == 0 && next != children.end())
        {
            for(auto &&part : child->getActualChildren())
            {
                if(!part->hidden() && std::strcmp(part->getTypeName(), "OpenToggleButton") == 0)
                {
                    toggle=part;
                    below=*next;
                    return true;
                }
            }
        }
        if(findOpenNode(child, toggle, below))
            return true;
    }
    return false;
}

// The layout path (invalidate / setBounds' StateChange_Geometry) is separate
// from redraw(), and is what moves the nodes below a collapsed one.
static void checkTreeCollapse(Session &s, const std::string &shotDir, InteractionChecks &checks, bool &ok)
{
    gweni::controls::Base *demo=addControl(s, "TreeControlDemo");
    renderFrames(s, s.frames);

    // The first node with a toggle on show and a sibling below it.
    gweni::controls::Base *toggle=nullptr;
    gweni::controls::Base *below=nullptr;
    findOpenNode(demo, toggle, below);
    if(!below)
    {
        checks.check("tree_found", false, "TreeControlDemo has no open node with a node below it");
        dropControl(s, demo);
        return;
    }

    std::vector<uint8_t> before, after;
    renderFrames(s, s.frames);
    if(!shoot(s, shotDir+"/interaction-tree-expanded.png"))
        ok=false;
    readFramebufferRgba(s.pixels.x, s.pixels.y, before);
    const gweni::Rect belowBefore=below->getBounds();

    const gweni::Point at=centreOf(toggle);
    clickMouse(s, at.x, at.y);
    renderFrames(s, s.frames);
    if(!shoot(s, shotDir+"/interaction-tree-collapsed.png"))
        ok=false;
    readFramebufferRgba(s.pixels.x, s.pixels.y, after);
    const gweni::Rect belowAfter=below->getBounds();

    dropControl(s, demo);

    checks.check("tree_repaint", before != after, "collapsing a tree node left the frame as it was");
    checks.check("tree_layout", belowAfter.y < belowBefore.y,
        "the node below the collapsed one stayed at y="+std::to_string(belowBefore.y)
        +" (now y="+std::to_string(belowAfter.y)+")");
}

static bool runInteractionScenario(Session &s, const std::string &shotDir, bool &passed)
{
    // Frozen like --stats, so a frame with no input is genuinely idle (no
    // caret blink); a scripted press still steps it.
    s.freezeClock=true;

    InteractionChecks checks;
    bool ok=true;
    checkButton(s, shotDir, checks, ok);
    checkFocusHandOff(s, shotDir, checks, ok);
    checkTreeCollapse(s, shotDir, checks, ok);

    std::cout<<"interaction";
    for(const auto &result : checks.results)
        std::cout<<" "<<result.first<<"="<<(result.second ? "yes" : "no");
    std::cout<<"\n";

    passed=checks.allPassed();
    return ok;
}

//-----------------------------------------------------------------------------
// --scenario clip: children are cut at their clipping parent's edges
//
// Two scenes on the bare canvas, each compared against the frame with less in
// it, so a pixel drawn past an edge (or left behind from an earlier frame)
// shows as a difference. The comparisons are whole frames, or every pixel
// outside one control's bounds.
//-----------------------------------------------------------------------------

static std::vector<uint8_t> framePixels(const Session &s)
{
    std::vector<uint8_t> frame;
    readFramebufferRgba(s.pixels.x, s.pixels.y, frame);
    return frame;
}

// The pixels of two frames that differ outside a control's bounds (snapped to
// device pixels as the renderer snaps them).
static size_t differencesOutside(const Session &s, const std::vector<uint8_t> &a,
    const std::vector<uint8_t> &b, gweni::controls::Base *control)
{
    const gweni::Point origin=control->localPosToCanvas();
    const gweni::Point topLeft=devicePixels(origin, s.scale);
    const gweni::Point bottomRight=devicePixels(origin+control->getSize(), s.scale);

    size_t count=0;
    for(int y=0; y<s.pixels.y; ++y)
    {
        for(int x=0; x<s.pixels.x; ++x)
        {
            if(x >= topLeft.x && x < bottomRight.x && y >= topLeft.y && y < bottomRight.y)
                continue;
            const size_t at=(static_cast<size_t>(y)*s.pixels.x+x)*4;
            if(std::memcmp(&a[at], &b[at], 4) != 0)
                ++count;
        }
    }
    return count;
}

static void countPending(gweni::controls::Base *control, size_t &count)
{
    if(control->getStateChange() != gweni::controls::StateChange_Nothing)
        ++count;
    for(auto &&child : control->getActualChildren())
        countPending(child, count);
}

// One frame whose update phase is inspected before it renders: how many
// controls have a change pending, in the whole tree and under `root`.
static void renderInspectedFrame(Session &s, gweni::controls::Base *root,
    size_t &pending, size_t &pendingUnderRoot)
{
    gweni::controls::resetChangeStats();
    s.canvas->updateCanvas();

    pending=0;
    pendingUnderRoot=0;
    countPending(s.canvas, pending);
    countPending(root, pendingUnderRoot);

    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    s.canvas->renderCanvas();

    if(s.stats)
        printFrameStats(s.frame, s.label, gweni::controls::getChangeStats(),
            s.renderer->getFrameStats(), s.canvas->getLayoutStats(),
            s.canvas->getUpdateStats());
    checkOnePass(s, std::to_string(s.frame));
    ++s.frame;
}

// A frame with no input once a step has settled: nothing changes, nothing is
// uploaded (the clip boxes included).
static void checkSteady(Session &s, InteractionChecks &checks, const std::string &name)
{
    renderFrames(s, 1);
    const gweni::controls::ChangeStats changes=gweni::controls::getChangeStats();
    const gweni::renderer::OpenGLCore::FrameStats gpu=s.renderer->getFrameStats();

    checks.check(name, changes.controls == 0 && gpu.uploadBytes == 0
        && gpu.primitiveSlotsWritten == 0 && gpu.textSlotsWritten == 0,
        "a settled frame had controls_changed="+std::to_string(changes.controls)
        +" upload_bytes="+std::to_string(gpu.uploadBytes)
        +" prim_slots_written="+std::to_string(gpu.primitiveSlotsWritten)
        +" text_slots_written="+std::to_string(gpu.textSlotsWritten));
}

// Where the scene puts a control, kept by layout: a dock-None child is otherwise
// aligned to its parent's top-left, and a size flag other than Fixed lets its
// preferred size win.
static void place(gweni::controls::Base *control, int x, int y, int w, int h)
{
    control->setAlignment(gweni::Alignment::Absolute);
    control->setSizeFlags({gweni::SizeFlag::Fixed, gweni::SizeFlag::Fixed});
    control->setBounds(x, y, w, h);
}

// The same at the control's preferred size.
static void place(gweni::controls::Base *control, int x, int y)
{
    control->setAlignment(gweni::Alignment::Absolute);
    control->setPos(x, y);
}

static std::vector<uint8_t> settledFrame(Session &s, const std::string &shotDir,
    const std::string &name, bool &ok)
{
    renderFrames(s, s.frames);
    if(!shoot(s, shotDir+"/clip-"+name+".png"))
        ok=false;
    return framePixels(s);
}

// (1) A ScrollControl with no scroll bars, holding content bigger than itself
// that runs past its right and bottom edges: a flat fill, a Button (rounded
// rect and text), a checked CheckBox (icon) and a Label. Then (3) the content
// is moved down below the ScrollControl, where it must leave nothing behind.
static void checkScrollContent(Session &s, const std::string &shotDir,
    const std::vector<uint8_t> &empty, InteractionChecks &checks, bool &ok)
{
    auto *scroll=s.canvas->newChild<gweni::controls::ScrollControl>();
    scroll->setScroll(false, false);
    place(scroll, 40, 40, 240, 160);

    std::vector<gweni::controls::Base *> content;

    auto *fill=scroll->newChild<gweni::TextDemoLightPanel>();
    place(fill, 150, 90, 200, 140);
    content.push_back(fill);

    auto *button=scroll->newChild<gweni::controls::Button>();
    button->setText("A Button wider than its ScrollControl");
    place(button, 10, 10, 300, 40);
    content.push_back(button);

    auto *check=scroll->newChild<gweni::controls::CheckBox>();
    check->setChecked(true);
    place(check, 228, 64);
    content.push_back(check);

    auto *label=scroll->newChild<gweni::controls::Label>();
    label->setText("A Label that runs on past the right edge");
    place(label, 120, 100);
    content.push_back(label);

    const std::vector<uint8_t> inside=settledFrame(s, shotDir, "scroll", ok);
    const size_t outside=differencesOutside(s, inside, empty, scroll);
    checks.check("scroll_cut", outside == 0,
        std::to_string(outside)+" pixels outside the ScrollControl differ from the empty canvas");
    checks.check("scroll_drawn", inside != empty, "the ScrollControl's content drew nothing");
    checkSteady(s, checks, "scroll_steady");

    for(gweni::controls::Base *child : content)
        child->setPos(child->getX(), child->getY()+200);

    const std::vector<uint8_t> out=settledFrame(s, shotDir, "scroll-out", ok);
    checks.check("scroll_out_gone", out == empty,
        "content moved out of the ScrollControl still draws (or left a stale copy)");
    checkSteady(s, checks, "scroll_out_steady");

    dropControl(s, scroll);
}

// Where the mouse holds a window's title bar, from the window's top-left.
static const gweni::Point g_titleGrip(40, 12);

static void dragTitleTo(Session &s, gweni::controls::Base *parent, const gweni::Point &local)
{
    const gweni::Point at=parent->localPosToCanvas()+local+g_titleGrip;
    moveMouse(s, at.x, at.y);
}

// (2) A Window in a parent smaller than the canvas, dragged by its title bar
// partly past the parent's right and bottom edges, then its left and top ones,
// then (3) right out of it, and back to where it started.
static void checkWindowDrag(Session &s, const std::string &shotDir,
    const std::vector<uint8_t> &empty, InteractionChecks &checks, bool &ok)
{
    auto *parent=s.canvas->newChild<gweni::TextDemoLightPanel>();
    place(parent, 120, 90, 400, 300);
    const std::vector<uint8_t> parentOnly=settledFrame(s, shotDir, "window-parent", ok);

    const gweni::Point start(100, 80);
    auto *window=parent->newChild<gweni::controls::WindowControl>();
    window->setTitle("Clipped Window");
    window->setClampMovement(false);
    place(window, start.x, start.y, 200, 140);
    // a Button fills the content area, so the window's subtree holds more than
    // its own surface
    auto *button=window->newChild<gweni::controls::Button>();
    button->setText("Inside the Window");
    button->setDock(gweni::DockPosition::Center);

    dragTitleTo(s, parent, start);
    const std::vector<uint8_t> inside=settledFrame(s, shotDir, "window-inside", ok);
    checks.check("window_drawn", inside != parentOnly, "the Window drew nothing");

    mouseDown(s, s.mouse.x, s.mouse.y, true);
    renderFrames(s, s.frames);

    // one drag step, inspected: only the window's subtree may have changed
    dragTitleTo(s, parent, start+gweni::Point(10, 10));
    size_t pending=0, pendingInWindow=0;
    renderInspectedFrame(s, window, pending, pendingInWindow);
    std::cout<<"clip drag controls_changed="<<pending<<" in_window="<<pendingInWindow<<"\n";
    checks.check("drag_only_window", pending > 0 && pending == pendingInWindow,
        std::to_string(pending-pendingInWindow)+" of the "+std::to_string(pending)
        +" controls changed by a drag step are outside the Window");

    const struct { const char *name; gweni::Point local; } edges[]=
    {
        {"window-bottomright", gweni::Point(290, 220)},
        {"window-topleft", gweni::Point(-70, -50)},
    };
    for(const auto &edge : edges)
    {
        dragTitleTo(s, parent, edge.local);
        const std::vector<uint8_t> frame=settledFrame(s, shotDir, edge.name, ok);
        const size_t outside=differencesOutside(s, frame, empty, parent);
        checks.check(std::string(edge.name)+"_cut", outside == 0,
            std::to_string(outside)+" pixels outside the parent differ from the empty canvas");
        checks.check(std::string(edge.name)+"_drawn", frame != parentOnly,
            "the Window drew nothing inside its parent");
    }

    dragTitleTo(s, parent, gweni::Point(420, 100));
    const std::vector<uint8_t> out=settledFrame(s, shotDir, "window-out", ok);
    checks.check("window_out_gone", out == parentOnly,
        "a Window dragged out of its parent still draws (or left a stale copy)");
    checkSteady(s, checks, "window_out_steady");

    dragTitleTo(s, parent, start);
    mouseUp(s, s.mouse.x, s.mouse.y);
    const std::vector<uint8_t> back=settledFrame(s, shotDir, "window-back", ok);
    checks.check("window_back", back == inside,
        "the Window dragged back to where it started does not look as it did");
    checkSteady(s, checks, "window_back_steady");

    forgetMouse(s);
    dropControl(s, parent);
}

// The pixels of `frame` inside `control`'s on-canvas device rect that differ
// from `reference` read `shift` device pixels further right and down, where the
// same control was drawn whole. A channel may be 1 off: the GPU's interpolation
// of a gradient is not exactly the same at every position (a Window moved
// between two positive positions already differs by that much).
static size_t differencesShifted(const Session &s, const std::vector<uint8_t> &frame,
    const std::vector<uint8_t> &reference, gweni::controls::Base *control,
    const gweni::Point &shift)
{
    const gweni::Point origin=control->localPosToCanvas();
    const gweni::Point topLeft=devicePixels(origin, s.scale);
    const gweni::Point bottomRight=devicePixels(origin+control->getSize(), s.scale);

    size_t count=0;
    for(int y=std::max(topLeft.y, 0); y<std::min(bottomRight.y, s.pixels.y); ++y)
    {
        for(int x=std::max(topLeft.x, 0); x<std::min(bottomRight.x, s.pixels.x); ++x)
        {
            const int rx=x+shift.x;
            const int ry=y+shift.y;
            if(rx < 0 || ry < 0 || rx >= s.pixels.x || ry >= s.pixels.y)
            {
                ++count;
                continue;
            }
            const size_t at=(static_cast<size_t>(y)*s.pixels.x+x)*4;
            const size_t refAt=(static_cast<size_t>(ry)*s.pixels.x+rx)*4;
            for(int c=0; c<4; ++c)
            {
                if(std::abs(int(frame[at+c])-int(reference[refAt+c])) > 1)
                {
                    ++count;
                    break;
                }
            }
        }
    }
    return count;
}

// (4) A Window on the canvas itself, dragged by its title bar past the canvas's
// own top-left corner, so its rects straddle device x=0 and y=0 (a primitive's
// corner at a negative device coordinate). Everything outside it must be the
// empty canvas, and its on-canvas part the matching part of the same Window
// drawn whole, shifted. The drag is a multiple of 8 logical units on each
// axis, so the shift is whole device pixels at scales 1.25, 1.5 and 2 too. The
// grip stays on the canvas.
static void checkWindowPastCanvasOrigin(Session &s, const std::string &shotDir,
    const std::vector<uint8_t> &empty, InteractionChecks &checks, bool &ok)
{
    const gweni::Point start(60, 56);
    const gweni::Point past(-28, -8);
    auto *window=s.canvas->newChild<gweni::controls::WindowControl>();
    window->setTitle("Clipped Window");
    window->setClampMovement(false);
    place(window, start.x, start.y, 200, 140);
    auto *button=window->newChild<gweni::controls::Button>();
    button->setText("Inside the Window");
    button->setDock(gweni::DockPosition::Center);

    // laid out before the mouse comes, and the reference taken with the title bar
    // held, as the shot past the corner is
    renderFrames(s, s.frames);
    dragTitleTo(s, s.canvas, start);
    mouseDown(s, s.mouse.x, s.mouse.y, true);
    const std::vector<uint8_t> whole=settledFrame(s, shotDir, "canvas-window", ok);
    checks.check("canvas_window_drawn", whole != empty, "the Window on the canvas drew nothing");

    dragTitleTo(s, s.canvas, past);
    const std::vector<uint8_t> frame=settledFrame(s, shotDir, "canvas-topleft", ok);
    const size_t outside=differencesOutside(s, frame, empty, window);
    checks.check("canvas_topleft_cut", outside == 0,
        std::to_string(outside)+" pixels outside the Window past the canvas's top-left corner"
        " differ from the empty canvas");
    const gweni::Point shift=devicePixels(start, s.scale)-devicePixels(past, s.scale);
    const size_t moved=differencesShifted(s, frame, whole, window, shift);
    checks.check("canvas_topleft_drawn", frame != empty && moved == 0,
        std::to_string(moved)+" pixels of the Window past the canvas's top-left corner"
        " differ from the same Window drawn whole");
    checkSteady(s, checks, "canvas_topleft_steady");

    dragTitleTo(s, s.canvas, start);
    const std::vector<uint8_t> back=settledFrame(s, shotDir, "canvas-back", ok);
    checks.check("canvas_back", back == whole,
        "the Window dragged back onto the canvas does not look as it did");
    mouseUp(s, s.mouse.x, s.mouse.y);

    forgetMouse(s);
    dropControl(s, window);
}

static bool runClipScenario(Session &s, const std::string &shotDir, bool &passed)
{
    s.freezeClock=true;
    s.label="clip";

    InteractionChecks checks;
    checks.scenario="clip";
    bool ok=true;

    const std::vector<uint8_t> empty=settledFrame(s, shotDir, "empty", ok);
    checkScrollContent(s, shotDir, empty, checks, ok);
    checkWindowDrag(s, shotDir, empty, checks, ok);
    checkWindowPastCanvasOrigin(s, shotDir, empty, checks, ok);

    std::cout<<"clip";
    for(const auto &result : checks.results)
        std::cout<<" "<<result.first<<"="<<(result.second ? "yes" : "no");
    std::cout<<"\n";

    passed=checks.allPassed();
    return ok;
}

//-----------------------------------------------------------------------------
// --scenario typing: the keyboard, through the GLFW translator
//
// Every keystroke is a synthesised GLFW event fed to gweni::input::GLFW3, the
// same object the examples' key and char callbacks feed, so this covers the
// translator as well as TextBox's editing. Each step is followed by a frame and
// checks the keyboard-focused control's text, so a step that went wrong is
// named on its own.
//-----------------------------------------------------------------------------

enum class TypingAction
{
    Click,          // click the centre of the first visible control of type arg
    Key,            // --key arg
    Type,           // --type arg
    HoldControl,    // press Left Ctrl and do not let it go
    LoseFocus,      // the window loses focus (glfwSetWindowFocusCallback)
};

struct TypingStep
{
    TypingAction action;
    const char *arg;
    const char *expect;     // the focused control's text afterwards
};

static const TypingStep g_typingSteps[]=
{
    {TypingAction::Click, "TextBox", ""},
    {TypingAction::Key, "ctrl+a", ""},              // a Ctrl+letter inserts nothing
    {TypingAction::Type, "hello", "hello"},
    {TypingAction::Key, "backspace", "hell"},
    {TypingAction::Key, "left", "hell"},
    {TypingAction::Key, "left", "hell"},            // caret between "he" and "ll"
    {TypingAction::Key, "backspace", "hll"},        // caret after "h"
    {TypingAction::Key, "delete", "hl"},
    {TypingAction::Key, "home", "hl"},
    {TypingAction::Type, "a", "ahl"},
    {TypingAction::Key, "end", "ahl"},
    {TypingAction::Type, "b", "ahlb"},
    {TypingAction::Key, "right", "ahlb"},           // already at the end
    {TypingAction::Key, "shift+left", "ahlb"},      // selects the "b"
    {TypingAction::Type, "c", "ahlc"},              // typing replaces the selection
    {TypingAction::Key, "shift+left", "ahlc"},
    {TypingAction::Key, "shift+left", "ahlc"},      // selects "lc"
    {TypingAction::Key, "delete", "ah"},
    {TypingAction::Key, "ctrl+a", "ah"},
    {TypingAction::Type, "x", "x"},
    {TypingAction::Key, "ctrl+a", "x"},
    {TypingAction::Key, "ctrl+c", "x"},
    {TypingAction::Key, "end", "x"},
    {TypingAction::Key, "ctrl+v", "xx"},
    {TypingAction::Key, "ctrl+a", "xx"},
    {TypingAction::Key, "ctrl+x", ""},
    {TypingAction::Key, "ctrl+v", "xx"},            // the cut text is on the clipboard
    {TypingAction::Key, "ctrl+b", "xx"},            // no accelerator: still not typed
    {TypingAction::Key, "escape", "xx"},
    // Ctrl held as the window loses focus, its release never seen: typing has
    // to work once the window has it back.
    {TypingAction::HoldControl, nullptr, "xx"},
    {TypingAction::LoseFocus, nullptr, nullptr},
    {TypingAction::Type, "y", "xxy"},
    // Tab moves the focus to the next tabable control in child order.
    {TypingAction::Key, "tab", "Normal Everyday Label"},
    // A numeric box takes the digits and drops the rest.
    {TypingAction::Click, "TextBoxNumeric", "2004"},
    {TypingAction::Key, "ctrl+a", "2004"},
    {TypingAction::Type, "a1b2", "12"},
    // Return and keypad Enter break the line in a multiline box.
    {TypingAction::Click, "TextBoxMultiline", nullptr},
    {TypingAction::Key, "ctrl+a", nullptr},
    {TypingAction::Type, "ab", "ab"},
    {TypingAction::Key, "return", "ab\n"},
    {TypingAction::Type, "cd", "ab\ncd"},
    {TypingAction::Key, "kp-enter", "ab\ncd\n"},
    {TypingAction::Type, "e", "ab\ncd\ne"},
};

static std::string typingStepName(size_t index, const TypingStep &step)
{
    static const char *const actions[]={"click", "key", "type", "hold-control", "lose-focus"};
    std::string name="step "+std::to_string(index)+" "+actions[static_cast<int>(step.action)];
    if(step.arg)
        name+=std::string(" ")+step.arg;
    return name;
}

static bool runTypingStep(Session &s, gweni::controls::Base *demo, const TypingStep &step)
{
    switch(step.action)
    {
    case TypingAction::Click:
    {
        gweni::controls::Base *target=findControl(demo, step.arg);
        if(!target)
        {
            std::cerr<<"ControlTest: typing: TextBoxDemo has no visible "<<step.arg<<"\n";
            return false;
        }
        const gweni::Point at=centreOf(target);
        clickMouse(s, at.x, at.y);
        return true;
    }
    case TypingAction::Key: return sendKey(s, step.arg);
    case TypingAction::Type: typeText(s, step.arg); return true;
    case TypingAction::HoldControl: glfwKey(s, GLFW_KEY_LEFT_CONTROL, GLFW_PRESS, 0); return true;
    case TypingAction::LoseFocus: glfwInput(s).processFocus(false); trace(s, "focus lost"); return true;
    }
    return false;
}

static bool runTypingScenario(Session &s, const std::string &shotDir, bool &passed)
{
    // Frozen like --stats: no caret blink, and no key repeat, which would need
    // the clock to pass c_KeyRepeatDelay with a key held.
    s.freezeClock=true;

    gweni::controls::Base *demo=addControl(s, "TextBoxDemo");
    renderFrames(s, s.frames);

    InteractionChecks checks;
    checks.scenario="typing";
    bool ok=true;
    size_t index=0;
    for(const TypingStep &step : g_typingSteps)
    {
        const std::string name=typingStepName(index++, step);
        if(!runTypingStep(s, demo, step))
        {
            checks.check(name, false, "the step could not be sent");
            continue;
        }
        renderFrames(s, 1);

        if(!step.expect)
            continue;

        std::string actual;
        const bool hasText=focusedText(actual);
        checks.check(name, hasText && actual == step.expect,
            "expected "+quotedText(step.expect)+", the keyboard focus "
            +(hasText ? std::string("has ")+quotedText(actual)
                : std::string("(")+(gweni::KeyboardFocus ? gweni::KeyboardFocus->getTypeName() : "none")
                  +") has no text"));

        // The picture: "hello" typed, the caret after it.
        if(step.action == TypingAction::Type && std::strcmp(step.arg, "hello") == 0)
        {
            renderFrames(s, s.frames);
            if(!shoot(s, shotDir+"/typing-hello.png"))
                ok=false;
        }
    }

    // One keystroke into a settled box: it rewrites that box's text run and
    // nothing else, and the frame after it is idle again.
    gweni::controls::Base *box=findControl(demo, "TextBox");
    if(box)
    {
        clickMouse(s, centreOf(box).x, centreOf(box).y);
        renderFrames(s, s.frames);

        typeText(s, "z");
        renderFrames(s, 1);
        const gweni::controls::ChangeStats keyChanges=gweni::controls::getChangeStats();
        const gweni::renderer::OpenGLCore::FrameStats keyGpu=s.renderer->getFrameStats();

        renderFrames(s, 1);
        const gweni::controls::ChangeStats idleChanges=gweni::controls::getChangeStats();
        const gweni::renderer::OpenGLCore::FrameStats idleGpu=s.renderer->getFrameStats();

        std::cout<<"typing keystroke controls_changed="<<keyChanges.controls
                 <<" text="<<keyChanges.text
                 <<" prim_slots_written="<<keyGpu.primitiveSlotsWritten
                 <<" text_slots_written="<<keyGpu.textSlotsWritten
                 <<" upload_bytes="<<keyGpu.uploadBytes<<"\n";

        checks.check("keystroke_one_text_run", keyChanges.text == 1 && keyGpu.textSlotsWritten >= 1,
            "typing one character changed text="+std::to_string(keyChanges.text)
            +" controls, text_slots_written="+std::to_string(keyGpu.textSlotsWritten)
            +"; want one control's text run rewritten");
        checks.check("keystroke_then_idle",
            idleChanges.controls == 0 && idleGpu.uploadBytes == 0,
            "the frame after the keystroke had controls_changed="+std::to_string(idleChanges.controls)
            +" upload_bytes="+std::to_string(idleGpu.uploadBytes));
    }

    dropControl(s, demo);

    size_t failed=0;
    for(const auto &result : checks.results)
        if(!result.second)
            ++failed;
    std::cout<<"typing checks="<<checks.results.size()<<" failed="<<failed<<"\n";

    passed=checks.allPassed();
    return ok;
}

//-----------------------------------------------------------------------------
// --check-teardown: teardown identity
//
// In a retained renderer a control's primitives are a persistent slot in the
// GPU buffer, so a destroyed control whose slots are never cleared keeps
// drawing forever. The sharpest statement that this has not happened is that
// the canvas, once every control is gone, renders byte for byte like the empty
// canvas it started as. That does not care which control leaked or what its
// leftover vertices drew, so it catches the whole class.
//-----------------------------------------------------------------------------

// The same starting point addControl() gives a control: no hover, clock and
// frame counter back at zero, so the two empty renders are comparable.
static void renderEmptyCanvas(Session &s, std::vector<uint8_t> &pixels)
{
    forgetMouse(s);
    g_clockSeconds=0.0f;
    s.frame=0;
    s.label="(empty)";

    renderFrames(s, s.frames);
    readFramebufferRgba(s.pixels.x, s.pixels.y, pixels);
}

static bool checkTeardown(Session &s, const std::vector<std::string> &controls,
    const std::string &shotDir)
{
    std::string set;
    for(const std::string &name : controls)
        set+=(set.empty() ? "" : ",")+name;

    std::vector<uint8_t> reference;
    renderEmptyCanvas(s, reference);

    for(const std::string &name : controls)
    {
        gweni::controls::Base *control=addControl(s, name);
        renderFrames(s, s.frames);
        dropControl(s, control);
    }

    std::vector<uint8_t> after;
    renderEmptyCanvas(s, after);

    size_t differing=0;
    for(size_t i=0; i<reference.size(); ++i)
        if(reference[i] != after[i])
            ++differing;

    if(differing == 0)
    {
        std::cout<<"teardown controls="<<set<<" identical=yes\n";
        return true;
    }

    const std::string referencePath=shotDir+"/teardown-reference.png";
    const std::string afterPath=shotDir+"/teardown-after.png";

    std::cout<<"teardown controls="<<set<<" identical=no"
             <<" differing_bytes="<<differing<<" of "<<reference.size()<<"\n";
    std::cerr<<"ControlTest: the canvas does not render empty after every control"
               " was destroyed: "<<differing<<" of "<<reference.size()
             <<" bytes differ, so something is still drawing. Compare "
             <<referencePath<<" with "<<afterPath<<"\n";

    bool wrote=writeRgbaPng(reference, s.pixels.x, s.pixels.y, referencePath);
    wrote=writeRgbaPng(after, s.pixels.x, s.pixels.y, afterPath) && wrote;
    if(wrote)
        std::cout<<"wrote "<<referencePath<<"\nwrote "<<afterPath<<"\n";
    return false;
}

int main(int argc, char *argv[])
{
    gweni::capture::setToolName("ControlTest");
    gweni::capture::installGlfwErrorHandler();

    std::string which="ButtonDemo";
    std::string shotPath;
    std::string shotDir="/tmp";
    std::string resourceDir;
    gweni::Point screenSize(640, 480);
    float scale=1.0f;
    bool scaleGiven=false;
    int frames=3;
    bool shotAll=false;
    bool forceHeadless=false;
    bool stats=false;
    bool requireOnePass=false;
    bool traceInput=false;
    bool chosenControl=false;
    bool dumpBounds=false;
    bool checkTeardownMode=false;
    std::string dumpPath;
    Scenario scenario=Scenario::None;
    std::vector<Step> steps;
    bool scripted=false;    // any scripted input switch turns --frames/--shot into steps

    for(int i=1; i<argc; ++i)
    {
        const std::string arg=argv[i];
        const bool hasValue=(i+1)<argc;

        const bool isMouseStep=(arg == "--move" || arg == "--press" || arg == "--release"
            || arg == "--click" || arg == "--double-click" || arg == "--scroll");

        if(arg == "--control" && hasValue){ which=argv[++i]; chosenControl=true; }
        else if(arg == "--shot" && hasValue)
        {
            shotPath=argv[++i];
            steps.push_back({StepKind::Shot, 0, 0, shotPath});
        }
        else if(arg == "--shot-dir" && hasValue) shotDir=argv[++i];
        else if(arg == "--resources" && hasValue) resourceDir=argv[++i];
        else if(arg == "--frames" && hasValue)
        {
            frames=std::atoi(argv[++i]);
            steps.push_back({StepKind::Frames, frames, 0, {}});
        }
        else if(arg == "--shot-all") shotAll=true;
        else if(arg == "--headless") forceHeadless=true;
        else if(arg == "--stats") stats=true;
        else if(arg == "--require-one-pass") requireOnePass=true;
        else if(arg == "--check-teardown") checkTeardownMode=true;
        else if(arg == "--trace") traceInput=true;
        else if(arg == "--list"){ printControlNames(std::cout); return EXIT_SUCCESS; }
        else if(arg == "--help" || arg == "-h"){ printUsage(std::cout); return EXIT_SUCCESS; }
        else if(arg == "--dump-bounds")
        {
            dumpBounds=true;
            // The path is optional, so only a following word that is not itself
            // a switch is one.
            if(hasValue && std::strncmp(argv[i+1], "--", 2) != 0)
                dumpPath=argv[++i];
        }
        else if(arg == "--key" && hasValue)
        {
            scripted=true;
            steps.push_back({StepKind::Key, 0, 0, argv[++i]});
        }
        else if(arg == "--type" && hasValue)
        {
            scripted=true;
            steps.push_back({StepKind::Type, 0, 0, argv[++i]});
        }
        else if(arg == "--expect-text" && hasValue)
        {
            scripted=true;
            steps.push_back({StepKind::ExpectText, 0, 0, argv[++i]});
        }
        else if(isMouseStep && hasValue)
        {
            int x=0, y=0;
            if(!parsePoint(argv[++i], x, y))
            {
                std::cerr<<"ControlTest: "<<arg<<" wants X,Y, e.g. 100,20\n";
                return EXIT_FAILURE;
            }
            StepKind kind=StepKind::Move;
            if(arg == "--press") kind=StepKind::Down;
            else if(arg == "--release") kind=StepKind::Up;
            else if(arg == "--click") kind=StepKind::Click;
            else if(arg == "--double-click") kind=StepKind::DoubleClick;
            else if(arg == "--scroll") kind=StepKind::Scroll;

            scripted=true;
            steps.push_back({kind, x, y, {}});
        }
        else if(arg == "--scenario" && hasValue)
        {
            const std::string name=argv[++i];
            if(!parseScenario(name, scenario))
            {
                std::cerr<<"ControlTest: unknown scenario: "<<name
                         <<"; try per-control, resize, docked, scroll, interaction, typing, clip,"
                           " text-size or scale\n";
                return EXIT_FAILURE;
            }
        }
        else if(arg == "--scale" && hasValue)
        {
            if(!gweni::capture::parseScale(argv[++i], scale))
            {
                std::cerr<<"ControlTest: --scale wants a number in (0, 16], e.g. 1.5\n";
                return EXIT_FAILURE;
            }
            scaleGiven=true;
        }
        else if(arg == "--size" && hasValue)
        {
            if(!parseSize(argv[++i], screenSize))
            {
                std::cerr<<"ControlTest: --size wants WxH, e.g. 640x480\n";
                return EXIT_FAILURE;
            }
        }
        else
        {
            std::cerr<<"ControlTest: unknown argument: "<<arg<<"\n";
            printUsage(std::cerr);
            return EXIT_FAILURE;
        }
    }

    if(frames<1) frames=1;

    // The flag beats the environment.
    if(!gweni::capture::manualScale(scaleGiven, scale))
        return EXIT_FAILURE;

    // The scale scenario sets both of its scales itself.
    if(scenario == Scenario::Scale)
        scale=1.0f;
    g_manualScale=scale;

    if(scripted && scenario != Scenario::None)
    {
        std::cerr<<"ControlTest: --scenario runs its own script;"
                   " drop the scripted input switches\n";
        return EXIT_FAILURE;
    }

    if(checkTeardownMode && (scenario != Scenario::None || scripted || shotAll
        || !shotPath.empty()))
    {
        std::cerr<<"ControlTest: --check-teardown runs its own build/render/destroy"
                   " cycle; drop --scenario, --shot, --shot-all and the scripted"
                   " input switches\n";
        return EXIT_FAILURE;
    }

    // per-control is --shot-all under a name, so it shares the machinery.
    if(scenario == Scenario::PerControl)
        shotAll=true;

    // The resize and scale checks are about the dock layout re-settling, so
    // they shoot the whole demo unless a control was named.
    if((scenario == Scenario::Resize || scenario == Scenario::Scale) && !chosenControl)
        which=g_dockedControlName;

    if(!isKnownControl(which))
    {
        std::cout<<"Unknown control: "<<which<<"\n";
        printControlNames(std::cout);
        return EXIT_FAILURE;
    }

    if(shotAll && !shotPath.empty())
    {
        std::cerr<<"ControlTest: --shot and --shot-all are exclusive;"
                   " --shot-all names its files after the controls\n";
        return EXIT_FAILURE;
    }

    // A run that shoots more than once names its listings after its shots, so
    // there is nowhere for one explicit path to go.
    const bool dumpPerShot=shotAll || scenario != Scenario::None;
    if(dumpBounds && dumpPerShot && !dumpPath.empty())
    {
        std::cerr<<"ControlTest: --dump-bounds takes no path here;"
                   " a scenario names its listings after its shots\n";
        return EXIT_FAILURE;
    }

    // --stats on its own still renders offscreen; it just keeps no PNG.
    const bool writingPng=shotAll || !shotPath.empty() || scenario != Scenario::None;
    const bool capturing=writingPng || stats || requireOnePass || scripted || dumpBounds
        || checkTeardownMode || scenario != Scenario::None;

    // The two fixed-size scenarios say what they render at, so start there
    // rather than resizing away from --size on the first frame.
    if(scenario == Scenario::Resize || scenario == Scenario::Scale) screenSize=gweni::Point(640, 480);
    else if(scenario == Scenario::Docked) screenSize=gweni::Point(1024, 768);

    // The canvas is screenSize logical units; the framebuffer, the window and
    // the view are that in device pixels.
    const gweni::Point screenPixels=devicePixels(screenSize, scale);

    if(capturing && !ensureDirectory(shotDir))
        return EXIT_FAILURE;
    if(!shotPath.empty() && !ensureDirectory(fs::path(shotPathIn(shotDir, shotPath)).parent_path()))
        return EXIT_FAILURE;

    if(forceHeadless && !capturing)
    {
        std::cerr<<"ControlTest: --headless has no window to show;"
                   " pass --shot, --shot-all, --stats, --require-one-pass or --check-teardown\n";
        return EXIT_FAILURE;
    }

    // Declared before the renderer, which keeps a reference to it.
    const std::unique_ptr<gweni::ResourcePaths> paths=gweni::capture::resourcePaths(resourceDir);
    if(!paths)
        return EXIT_FAILURE;

    Context ctx;
    if(forceHeadless)
    {
#if GWENI_EXAMPLE_EGL
        if(!initEglContext(ctx)) return EXIT_FAILURE;
#else
        std::cerr<<"ControlTest: --headless needs the EGL path, which this platform lacks\n";
        return EXIT_FAILURE;
#endif
    }
    else if(!initGlfwContext(ctx, screenPixels, "Gweni ControlTest - "+which, !capturing))
    {
        if(!capturing)
        {
            std::cerr<<"ControlTest: GLFW could not open a window;"
                       " interactive mode needs a display\n";
            return EXIT_FAILURE;
        }
#if GWENI_EXAMPLE_EGL
        std::cerr<<"ControlTest: GLFW could not open a window"
                   " (no display?); falling back to surfaceless EGL\n";
        if(!initEglContext(ctx)) return EXIT_FAILURE;
#else
        std::cerr<<"ControlTest: GLFW could not open a window\n";
        return EXIT_FAILURE;
#endif
    }

    if(!gweni::capture::initGlew())
    {
        destroyContext(ctx);
        return EXIT_FAILURE;
    }

    Framebuffer fb;
    if(capturing && !createFramebuffer(fb, screenPixels.x, screenPixels.y))
    {
        destroyContext(ctx);
        return EXIT_FAILURE;
    }

    gweni::renderer::OpenGLCore *renderer=
        new gweni::renderer::OpenGLCore(*paths, gweni::Rect(gweni::Point(0,0), screenPixels));
    renderer->init();

    gweni::skin::textured::TexturedSkin *skin=new gweni::skin::textured::TexturedSkin(renderer);
    if(!skin->init("DefaultSkin.json"))
        return EXIT_FAILURE;
    if(!gweni::capture::loadDefaultFont(*renderer, skin->getDefaultFont()))
        return EXIT_FAILURE;

    // The canvas fill comes from the theme, so a "theme" block in
    // resources/DefaultSkin.json moves it too. Canvas::renderCanvas does not
    // currently paint its own background (the draw call there is commented
    // out), so the clear color is what actually shows.
    const gweni::Color background=gweni::skin::textured::tokyonight::Canvas.top;
    glClearColor(background.r/255.0f, background.g/255.0f, background.b/255.0f, background.a/255.0f);

    gweni::controls::Canvas *canvas=createCanvas(skin, screenSize, scale);

    g_renderer=renderer;
    g_canvas=canvas;

    int result=EXIT_SUCCESS;

    if(capturing)
    {
        // Everything time-dependent in the library now reads this instead of the
        // platform clock; --stats leaves the per-frame step out so time only
        // moves when a scripted press moves it.
        gweni::platform::setTimeSource(&steppedClock);

        Session session;
        session.renderer=renderer;
        session.canvas=canvas;
        session.fb=&fb;
        session.size=screenSize;
        session.scale=scale;
        session.pixels=screenPixels;
        session.stats=stats;
        session.requireOnePass=requireOnePass;
        session.trace=traceInput;
        session.writePng=writingPng;
        session.frames=frames;
        session.dumpBounds=dumpBounds;
        session.dumpPerShot=dumpPerShot;
        session.dumpPath=dumpPath;

        bool ok=true;
        if(checkTeardownMode)
        {
            // The whole set by default, so one run covers every control; a named
            // --control narrows it, which is how a failure gets bisected.
            std::vector<std::string> set;
            if(chosenControl)
                set.push_back(which);
            else
                for(const char *name : g_controlNames)
                    set.push_back(name);

            ok=checkTeardown(session, set, shotDir);
        }
        else switch(scenario)
        {
        case Scenario::PerControl:
            ok=runPerControlScenario(session, shotDir);
            break;

        case Scenario::Resize:
        {
            bool settled=false;
            ok=runResizeScenario(session, which, shotDir, settled);
            if(ok && !settled)
                ok=false;   // the shrink did not re-settle: a failing assertion
            break;
        }

        case Scenario::Docked:
            ok=runDockedScenario(session, shotDir);
            break;

        case Scenario::Scale:
        {
            bool settled=false;
            ok=runScaleScenario(session, which, shotDir, settled);
            if(ok && !settled)
                ok=false;   // the round trip did not come back: a failing assertion
            break;
        }

        case Scenario::Scroll:
            ok=runScrollScenario(session, shotDir);
            break;

        case Scenario::Interaction:
        {
            bool passed=false;
            ok=runInteractionScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // something did not repaint: a failing assertion
            break;
        }

        case Scenario::Typing:
        {
            bool passed=false;
            ok=runTypingScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // a step left the wrong text: a failing assertion
            break;
        }

        case Scenario::Clip:
        {
            bool passed=false;
            ok=runClipScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // something drew past its parent: a failing assertion
            break;
        }

        case Scenario::TextSize:
        {
            bool passed=false;
            ok=runTextSizeScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // the size change cost more than its run: a failing assertion
            break;
        }

        case Scenario::None:
            if(shotAll)
            {
                ok=runPerControlScenario(session, shotDir);
                break;
            }

            {
                gweni::controls::Base *control=addControl(session, which);
                if(scripted)
                {
                    ok=runSteps(session, steps, shotDir);
                }
                else
                {
                    renderFrames(session, frames);
                    if(writingPng)
                        ok=shoot(session, shotPathIn(shotDir, shotPath));
                }
                if(ok && dumpBounds && !dumpBoundsTo(session, dumpPath))
                    ok=false;
                dropControl(session, control);
            }
            break;
        }

        if(session.expectFailures > 0)
        {
            std::cerr<<"ControlTest: "<<session.expectFailures
                     <<" --expect-text check(s) failed\n";
            ok=false;
        }

        if(session.multiPassFrames > 0)
        {
            std::cerr<<"ControlTest: "<<session.multiPassFrames
                     <<" frame(s) took more than one layout pass\n";
            ok=false;
        }

        if(!ok)
            result=EXIT_FAILURE;

        // The resize and scale scenarios build a canvas of their own for their
        // fresh reference.
        canvas=session.canvas;
    }
    else
    {
        gweni::controls::Base *control=createControl(canvas, which);
        control->setDock(gweni::DockPosition::Center);

        g_glfwInput.initialize(canvas);
        glfwSetKeyCallback(ctx.window, key_callback);
        glfwSetCharCallback(ctx.window, char_callback);
        glfwSetWindowFocusCallback(ctx.window, window_focus_callback);
        glfwSetCursorPosCallback(ctx.window, cursor_pos_callback);
        glfwSetMouseButtonCallback(ctx.window, mouse_button_callback);
        glfwSetScrollCallback(ctx.window, scroll_callback);
        glfwSetFramebufferSizeCallback(ctx.window, framebuffer_size_callback);
        glfwSetWindowContentScaleCallback(ctx.window, content_scale_callback);

        // The window's own content scale, and its real framebuffer size.
        fitCanvas(ctx.window);

        while(!glfwWindowShouldClose(ctx.window))
        {
            canvas->updateCanvas();
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            canvas->renderCanvas();
            glfwSwapBuffers(ctx.window);
            glfwPollEvents();
            gweni::platform::sleep(0);
        }

        delete control;
    }

    delete canvas;
    delete skin;
    delete renderer;
    if(capturing) destroyFramebuffer(fb);
    destroyContext(ctx);

    // setSkin() printed each of these when it first met it; the run fails on
    // them whatever the mode, so a control that draws nothing is not silent.
    const std::vector<std::string> unrendered=gweni::controls::typesWithoutSkinRenderer();
    if(!unrendered.empty())
    {
        std::cerr<<"ControlTest: "<<unrendered.size()<<" control type(s) with no skin renderer:";
        for(const std::string &type : unrendered)
            std::cerr<<" "<<type;
        std::cerr<<"\n";
        result=EXIT_FAILURE;
    }
    return result;
}
