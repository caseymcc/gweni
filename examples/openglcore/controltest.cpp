/*
 *  Gweni - single-control test harness
 *
 *  Drops ONE control (via its demo builder) directly on the canvas, so each
 *  control can be verified in isolation without the demo's dock/page layout.
 *
 *  Usage: ControlTest [options]
 *    --control <Name>   control demo to show (default ButtonDemo)
 *    --size WxH         canvas/framebuffer size (default 640x480)
 *    --frames N         frames to render before capturing (default 3)
 *    --shot <path>      render offscreen and write one PNG, then exit
 *    --shot-all         write one PNG per control demo, in this one run
 *    --shot-dir <dir>   directory for shots, created if missing (default /tmp)
 *    --headless         force the surfaceless EGL context (no display needed)
 *    --stats            print one key=value line per frame; freezes the clock
 *    --check-teardown   build, render and destroy every control (or only
 *                       --control's), then check that the canvas renders
 *                       byte-identical to the empty canvas it started as.
 *                       Exits non-zero on a mismatch and writes
 *                       teardown-reference.png and teardown-after.png to
 *                       --shot-dir, so the ghost can be looked at.
 *    --trace            log each scripted input on stderr with what it hit
 *    --list             print the control demo names and exit
 *    --help, -h         print the option list and exit
 *    --scenario <name>  per-control | resize | docked | scroll | interaction
 *                       (see below)
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
 *    --key <name>       a key: return, tab, space, escape, up, down, left,
 *                       right, home, end, backspace, delete, shift, control,
 *                       alt, command, or a single printable character
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
 *  Scenarios:
 *    per-control  the 25 single-control shots (same as --shot-all)
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
 *
 *  Every scenario (and --shot-all) takes a listing per shot when
 *  --dump-bounds is on, named after that shot with a .txt extension, so the
 *  shot and its listing sit side by side in --shot-dir - resize yields four
 *  (the fresh one included) and per-control 25. The resize checks take their
 *  listings in memory whether or not --dump-bounds is on; it only writes them
 *  out. A scenario names its own listings, so --dump-bounds
 *  there takes no path.
 *
 *  All of them, in the build container, from the repo root (a non-zero exit
 *  from resize or interaction is their assertion, so it is reported rather than
 *  aborting the loop):
 *
 *    for s in per-control resize docked scroll interaction; do \
 *      ./build_linux_x64_debug/examples/openglcore/ControlTest \
 *          --headless --scenario $s --shot-dir /tmp/shots/$s; \
 *      echo "$s exit=$?"; \
 *    done
 *
 *  With no --shot/--shot-all/--stats/--scenario and no scripted input it opens
 *  the interactive window, as before, on the real platform clock.
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
#include <gweni/controls/scrollControl.h>
#include <GLFW/glfw3.h>
#include <gweni/utils/filesystem.h>
#include "capture.h"
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

static void key_callback(GLFWwindow*, int key, int scancode, int action, int mods)
{   const gweni::input::GLFW3::KeyEvent e={key,scancode,action,mods}; g_glfwInput.processKeyEvent(e); }
static void cursor_pos_callback(GLFWwindow*, double x, double y)
{   g_glfwInput.processMouseMove(x,y); }
static void mouse_button_callback(GLFWwindow*, int b, int a, int m)
{   g_glfwInput.processMouseButtons(b,a,m); }
static void scroll_callback(GLFWwindow*, double x, double y)
{   g_glfwInput.processScroll(x, y); }
static void framebuffer_size_callback(GLFWwindow*, int w, int h)
{
    if(w<=0||h<=0) return;
    if(g_renderer) g_renderer->setViewRect(gweni::Rect(gweni::Point(0,0), gweni::Point(w,h)));
    if(g_canvas) g_canvas->setSize(w,h);
}

#define GWENI_CONTROL_DEMOS(X) \
    X(ButtonDemo) X(LabelDemo) X(LabelMultilineDemo) X(ProgressBarDemo) \
    X(GroupBoxDemo) X(ImagePanelDemo) X(StatusBarDemo) X(ComboBoxDemo) \
    X(TextBoxDemo) X(ListBoxDemo) X(CrossSplitterDemo) X(RadioButtonDemo) \
    X(CheckBoxDemo) X(NumericDemo) X(SliderDemo) X(MenuStripDemo) \
    X(WindowDemo) X(TreeControlDemo) X(PropertiesDemo) X(TabControlDemo) \
    X(ScrollControlDemo) X(PageControlDemo) X(CollapsibleListDemo) X(ColorPickerDemo) \
    X(UnicodeTextDemo)

static const char *const g_controlNames[]=
{
#define GWENI_NAME(N) #N,
    GWENI_CONTROL_DEMOS(GWENI_NAME)
#undef GWENI_NAME
};

// The whole demo application, not one of its pages: the dock layout, the demo
// list, the output ListBox and the status bar. Kept out of GWENI_CONTROL_DEMOS
// so --shot-all and --scenario per-control stay the 25 single-control shots.
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

// The scripted input switches are deliberately left out: they only make sense
// together, and the file header documents them as a group.
static void printUsage(std::ostream &out)
{
    out<<"Usage: ControlTest [options]\n"
         "  --control <Name>   control demo to show (default ButtonDemo)\n"
         "  --size WxH         canvas/framebuffer size (default 640x480)\n"
         "  --frames N         frames to render before capturing (default 3)\n"
         "  --shot <path>      render offscreen and write one PNG, then exit\n"
         "  --shot-all         write one PNG per control demo, in this one run\n"
         "  --shot-dir <dir>   directory for shots (default /tmp)\n"
         "  --headless         force the surfaceless EGL context\n"
         "  --stats            print one key=value line per frame\n"
         "  --check-teardown   check the canvas renders empty again once every\n"
         "                     control has been destroyed; --control narrows it\n"
         "  --trace            log each scripted input on stderr\n"
         "  --scenario <name>  per-control | resize | docked | scroll | interaction;\n"
         "                     resize prints listing_settled, mirror_settled,\n"
         "                     shrink_settled and fresh_matches, interaction one\n"
         "                     yes/no per repaint check; both fail on a no\n"
         "  --dump-bounds [<path>]  print the laid-out control tree as text,\n"
         "                     private children and hidden state included\n"
         "  --list             print the control demo names and exit\n"
         "Scripted input (--move/--press/--release/--click/--double-click/\n"
         "--scroll/--key) is documented in the header of controltest.cpp.\n";
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

// One grep-able line per frame, frame index first. The renderer numbers are read
// after renderCanvas(), so they describe the frame just flushed.
static void printFrameStats(int frame, const std::string &control,
    const gweni::controls::ChangeStats &changes,
    const gweni::renderer::OpenGLCore::FrameStats &gpu)
{
    std::printf("frame=%d control=%s controls_changed=%zu created=%zu visibility=%zu"
        " geometry=%zu background=%zu text=%zu prim_slots_written=%zu text_slots_written=%zu"
        " upload_calls=%zu upload_bytes=%zu mirror_bytes=%zu draw_calls=%zu"
        " atlas_upload_calls=%zu atlas_upload_bytes=%zu\n",
        frame, control.c_str(),
        changes.controls, changes.created, changes.visibility,
        changes.geometry, changes.background, changes.text,
        gpu.primitiveSlotsWritten, gpu.textSlotsWritten,
        gpu.uploadCalls, gpu.uploadBytes, gpu.mirrorBytes, gpu.drawCalls,
        gpu.atlasUploadCalls, gpu.atlasUploadBytes);
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
static gweni::controls::Canvas *createCanvas(gweni::skin::Base *skin, const gweni::Point &size)
{
    gweni::controls::Canvas *canvas=new gweni::controls::Canvas(skin);
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
    gweni::Point size;
    bool stats=false;
    bool freezeClock=false;             // as under --stats, without the lines
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
                s.renderer->getFrameStats());

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
    if(!writeFramebufferPng(s.size.x, s.size.y, path))
        return false;

    std::cout<<"wrote "<<path<<"\n";
    return true;
}

// The whole of framebuffer_size_callback, called directly: no window manager is
// involved, so a scripted resize is just these two plus a new offscreen target.
static bool resizeSession(Session &s, int width, int height)
{
    destroyFramebuffer(*s.fb);
    if(!createFramebuffer(*s.fb, width, height))
        return false;

    s.size=gweni::Point(width, height);
    s.renderer->setViewRect(gweni::Rect(gweni::Point(0, 0), s.size));
    s.canvas->setSize(width, height);
    return true;
}

//-----------------------------------------------------------------------------
// Scripted input. All of it goes in through Canvas's IInputEvent methods, which
// are the same gweni::input::onMouseMoved/onMouseClicked/onKeyEvent entry points
// the GLFW backend uses - there is no event loop behind this.
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

static const struct { const char *name; int key; } g_namedKeys[]=
{
    {"return", gweni::keys::Return},     {"backspace", gweni::keys::Backspace},
    {"delete", gweni::keys::Delete},     {"left", gweni::keys::Left},
    {"right", gweni::keys::Right},       {"shift", gweni::keys::Shift},
    {"tab", gweni::keys::Tab},           {"space", gweni::keys::Space},
    {"home", gweni::keys::Home},         {"end", gweni::keys::End},
    {"control", gweni::keys::Control},   {"up", gweni::keys::Up},
    {"down", gweni::keys::Down},         {"escape", gweni::keys::Escape},
    {"alt", gweni::keys::Alt},           {"command", gweni::keys::Command},
};

static bool scrollWheel(Session &s, int delta)
{
    const bool used=s.canvas->inputMouseWheel(delta);
    trace(s, std::string("wheel ")+std::to_string(delta)+(used?" consumed":" ignored"));
    return used;
}

static bool sendKey(Session &s, const std::string &name)
{
    if(name.size() == 1 && name[0]>=' ' && name[0]<'\x7f')
    {
        s.canvas->inputCharacter(static_cast<gweni::UnicodeChar>(name[0]));
        return true;
    }

    for(const auto &named : g_namedKeys)
    {
        if(name != named.name)
            continue;
        s.canvas->inputModifierKey(named.key, true);
        s.canvas->inputModifierKey(named.key, false);
        return true;
    }

    std::cerr<<"ControlTest: unknown key name: "<<name<<"\n";
    return false;
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
}

//-----------------------------------------------------------------------------
// Scripted runs
//-----------------------------------------------------------------------------
enum class StepKind { Move, Down, Up, Click, DoubleClick, Scroll, Key, Frames, Shot };

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
enum class Scenario { None, PerControl, Resize, Docked, Scroll, Interaction };

static bool parseScenario(const std::string &name, Scenario &scenario)
{
    if(name == "per-control") scenario=Scenario::PerControl;
    else if(name == "resize") scenario=Scenario::Resize;
    else if(name == "docked") scenario=Scenario::Docked;
    else if(name == "scroll") scenario=Scenario::Scroll;
    else if(name == "interaction") scenario=Scenario::Interaction;
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
    readFramebufferRgba(s.size.x, s.size.y, state.pixels);
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

static size_t reportMirrorKind(const char *kind, const std::map<size_t, std::vector<uint8_t>> &before,
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

        if(was == before.end()) how="live only after the shrink";
        else if(now == after.end()) how="live only before the grow";
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

// The renderer's CPU mirror, id by id, before the grow against after the
// shrink. Owners are named from the tree as it stood after the shrink.
static bool mirrorsMatch(const ResizeState &before, const ResizeState &after)
{
    if(before.mirror.primitives == after.mirror.primitives && before.mirror.texts == after.mirror.texts)
        return true;

    const IdOwners &owners=after.owners;
    std::cerr<<"ControlTest: renderer mirror after the shrink ("<<after.name
             <<") differs from before the grow ("<<before.name<<"):\n";
    const size_t primitives=reportMirrorKind("primitive", before.mirror.primitives,
        after.mirror.primitives, owners.primitives, false);
    const size_t texts=reportMirrorKind("text", before.mirror.texts,
        after.mirror.texts, owners.texts, true);
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
    s.canvas=createCanvas(skin, s.size);
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

// The demos that overflow their own bounds, idle and scrolled. Clipping is
// stubbed in OpenGLCore (startClip/endClip only toggle a flag), so the scrolled
// shots are expected to show content spilling outside the viewport - that is
// the state these captures are here to record.
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

// The RGBA pixels inside a control's bounds, clipped to the framebuffer.
static std::vector<uint8_t> regionPixels(const Session &s, gweni::controls::Base *control)
{
    std::vector<uint8_t> frame;
    readFramebufferRgba(s.size.x, s.size.y, frame);

    const gweni::Point origin=control->localPosToCanvas();
    const int left=std::max(0, origin.x);
    const int top=std::max(0, origin.y);
    const int right=std::min(s.size.x, origin.x+control->getWidth());
    const int bottom=std::min(s.size.y, origin.y+control->getHeight());

    std::vector<uint8_t> region;
    for(int y=top; y<bottom; ++y)
    {
        const auto row=frame.begin()+(static_cast<size_t>(y)*s.size.x+left)*4;
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
    std::vector<std::pair<std::string, bool>> results;

    void check(const std::string &name, bool passed, const std::string &why)
    {
        results.emplace_back(name, passed);
        if(!passed)
            std::cerr<<"ControlTest: interaction "<<name<<" failed: "<<why<<"\n";
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
    readFramebufferRgba(s.size.x, s.size.y, before);
    const gweni::Rect belowBefore=below->getBounds();

    const gweni::Point at=centreOf(toggle);
    clickMouse(s, at.x, at.y);
    renderFrames(s, s.frames);
    if(!shoot(s, shotDir+"/interaction-tree-collapsed.png"))
        ok=false;
    readFramebufferRgba(s.size.x, s.size.y, after);
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
    readFramebufferRgba(s.size.x, s.size.y, pixels);
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

    bool wrote=writeRgbaPng(reference, s.size.x, s.size.y, referencePath);
    wrote=writeRgbaPng(after, s.size.x, s.size.y, afterPath) && wrote;
    if(wrote)
        std::cout<<"wrote "<<referencePath<<"\nwrote "<<afterPath<<"\n";
    return false;
}

int main(int argc, char *argv[])
{
    gweni::capture::setToolName("ControlTest");

    std::string which="ButtonDemo";
    std::string shotPath;
    std::string shotDir="/tmp";
    gweni::Point screenSize(640, 480);
    int frames=3;
    bool shotAll=false;
    bool forceHeadless=false;
    bool stats=false;
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
        else if(arg == "--frames" && hasValue)
        {
            frames=std::atoi(argv[++i]);
            steps.push_back({StepKind::Frames, frames, 0, {}});
        }
        else if(arg == "--shot-all") shotAll=true;
        else if(arg == "--headless") forceHeadless=true;
        else if(arg == "--stats") stats=true;
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
                         <<"; try per-control, resize, docked, scroll or interaction\n";
                return EXIT_FAILURE;
            }
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

    // The resize check is about the dock layout re-settling, so it shoots the
    // whole demo unless a control was named.
    if(scenario == Scenario::Resize && !chosenControl)
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
    const bool capturing=writingPng || stats || scripted || dumpBounds
        || checkTeardownMode || scenario != Scenario::None;

    // The two fixed-size scenarios say what they render at, so start there
    // rather than resizing away from --size on the first frame.
    if(scenario == Scenario::Resize) screenSize=gweni::Point(640, 480);
    else if(scenario == Scenario::Docked) screenSize=gweni::Point(1024, 768);

    if(capturing && !ensureDirectory(shotDir))
        return EXIT_FAILURE;
    if(!shotPath.empty() && !ensureDirectory(fs::path(shotPathIn(shotDir, shotPath)).parent_path()))
        return EXIT_FAILURE;

    if(forceHeadless && !capturing)
    {
        std::cerr<<"ControlTest: --headless has no window to show;"
                   " pass --shot, --shot-all, --stats or --check-teardown\n";
        return EXIT_FAILURE;
    }

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
    else if(!initGlfwContext(ctx, screenSize, "Gweni ControlTest - "+which, !capturing))
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
    if(capturing && !createFramebuffer(fb, screenSize.x, screenSize.y))
    {
        destroyContext(ctx);
        return EXIT_FAILURE;
    }

    gweni::platform::CurrentPath paths("resources");
    gweni::renderer::OpenGLCore *renderer=
        new gweni::renderer::OpenGLCore(paths, gweni::Rect(gweni::Point(0,0), screenSize));
    renderer->init();

    gweni::skin::textured::TexturedSkin *skin=new gweni::skin::textured::TexturedSkin(renderer);
    skin->init("resources/DefaultSkin.json");
    skin->setDefaultFont("resources/OpenSans.ttf", 11);

    // The canvas fill comes from the theme, so a "theme" block in
    // resources/DefaultSkin.json moves it too. Canvas::renderCanvas does not
    // currently paint its own background (the draw call there is commented
    // out), so the clear color is what actually shows.
    const gweni::Color background=gweni::skin::textured::tokyonight::Canvas.top;
    glClearColor(background.r/255.0f, background.g/255.0f, background.b/255.0f, background.a/255.0f);

    gweni::controls::Canvas *canvas=createCanvas(skin, screenSize);

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
        session.stats=stats;
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

        if(!ok)
            result=EXIT_FAILURE;

        // The resize scenario builds a canvas of its own for its fresh reference.
        canvas=session.canvas;
    }
    else
    {
        gweni::controls::Base *control=createControl(canvas, which);
        control->setDock(gweni::DockPosition::Center);

        g_glfwInput.initialize(canvas);
        glfwSetKeyCallback(ctx.window, key_callback);
        glfwSetCursorPosCallback(ctx.window, cursor_pos_callback);
        glfwSetMouseButtonCallback(ctx.window, mouse_button_callback);
        glfwSetScrollCallback(ctx.window, scroll_callback);
        glfwSetFramebufferSizeCallback(ctx.window, framebuffer_size_callback);

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
    return result;
}
