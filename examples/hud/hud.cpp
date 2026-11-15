/*
 *  Gweni
 *
 *  HudExample: gweni drawn as a HUD over an app's own GL scene
 *  (docs/architecture.md -> "gweni over an app's scene").
 *
 *  The scene is the app's: a spinning, depth-tested cube drawn with its own
 *  small shader (GLSL 3.30 core, or ES 3.00 in a GLES tree) into the target,
 *  with its own depth. Each frame the app clears and draws it, calls
 *  SlotRenderer::clearDepth() so its depth hides no control, and then
 *  renderCanvas(). The canvas draws no background (Canvas::setDrawBackground
 *  is off by default), so the scene shows wherever no control draws, and a
 *  translucent panel (the theme's Panel surface given alpha) blends over it.
 *  OpenGLCore::setPreserveAppState(true) gives the scene its GL state back
 *  after each frame.
 *
 *  Input the UI did not take (Canvas's input calls and input::GLFW3's
 *  process* return false) is the scene's: a left drag that starts on the bare
 *  canvas orbits the cube until its release, over the panel too (a press's
 *  moves and release get its answer), the wheel over the bare canvas zooms
 *  (pointerOverControl() keeps one over a Label from it), Space toggles the spin and
 *  R resets the view while no control has the keyboard. A press on the bare
 *  canvas drops the keyboard focus, so the keys come back to the scene.
 *
 *  Usage: HudExample [options]
 *    --screenshot <path>  render a fixed number of frames offscreen, write one
 *                         PNG and exit
 *    --headless           with --screenshot, go straight to the surfaceless
 *                         EGL context instead of trying a GLFW window first
 *    --stats-overlay [on|off]  open the stats overlay (F10 toggles it)
 *    --clear <r,g,b>      the scene's background colour (default 26,32,52)
 *    --scale <s>          draw at s device px per logical unit times the OS
 *                         content scale (default 1, or GWENI_SCALE)
 *    --resources <dir>    load the skin and fonts from <dir>
 *    --renderer gl        the only backend: a Vulkan HUD example is not there
 *                         yet, so vulkan prints one line and exits 1
 *
 *  A capture runs on the stepped clock (1/60 s a frame) and the fixed profile
 *  clock, so its PNG is the same in every run, and prints one line of where it
 *  drew, for its test (tests/hudExample.mjs):
 *
 *    HudExample: clear=r,g,b panel_color=r,g,b,a probe_scene=x,y
 *        probe_button=x,y probe_panel=x,y
 *
 *  (device px of the PNG, top row 0). Exit status: 0, 1 a failure, 2 written
 *  from incomplete resources.
 *
 *  See license in Gweni.h
 */

#include <gweni/platforms/renderers/glApi.h>
#include <gweni/platforms/renderers/openglCore.h>
#include <gweni/gweni.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>
#include <gweni/platforms/input/glfw3.h>
#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/label.h>
#include <gweni/tools/statsOverlay.h>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "capture.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

namespace
{

namespace tokyonight=gweni::skin::textured::tokyonight;

//-----------------------------------------------------------------------------
// The app's scene: a cube, its camera and its GL objects
//-----------------------------------------------------------------------------

struct SceneState
{
    float yaw=1.25f;        //!< the orbit, radians; a drag on the bare canvas turns it
    float pitch=0.45f;
    float distance=5.0f;    //!< the camera's distance; the wheel over the scene
    float speed=0.15f;      //!< turns a second
    bool spinning=true;
    double spun=0.0;        //!< turns spun so far
    double lastTime=0.0;
};

SceneState g_scene;

//! One body per stage behind the dialect's header, chosen as the tree's GL is.
#if defined(GWENI_GL_ES)
const char *const ShaderHeader="#version 300 es\nprecision highp float;\n";
#else
const char *const ShaderHeader="#version 330 core\n";
#endif

const char *const VertexBody=
    "uniform mat4 mvp;\n"
    "uniform mat4 model;\n"
    "layout(location=0) in vec3 position;\n"
    "layout(location=1) in vec3 normal;\n"
    "layout(location=2) in vec3 color;\n"
    "out vec3 shade;\n"
    "void main()\n"
    "{\n"
    "    vec3 n=normalize(mat3(model)*normal);\n"
    "    float light=0.35+0.65*max(dot(n, normalize(vec3(0.4, 0.8, 0.6))), 0.0);\n"
    "    shade=color*light;\n"
    "    gl_Position=mvp*vec4(position, 1.0);\n"
    "}\n";

const char *const FragmentBody=
    "in vec3 shade;\n"
    "out vec4 fragColor;\n"
    "void main()\n"
    "{\n"
    "    fragColor=vec4(shade, 1.0);\n"
    "}\n";

GLuint compileStage(GLenum stage, const char *body)
{
    const GLuint shader=glCreateShader(stage);
    const char *sources[2]={ShaderHeader, body};
    GLint ok=GL_FALSE;

    glShaderSource(shader, 2, sources, nullptr);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if(ok != GL_TRUE)
    {
        char log[1024]={};

        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr<<"HudExample: the cube's shader does not compile: "<<log<<"\n";
    }
    return shader;
}

//! The camera: the orbit round the origin, at \p size device px.
glm::mat4 viewProjection(const gweni::Point &size)
{
    const float aspect=float(size.x)/float(size.y > 0 ? size.y : 1);
    const glm::mat4 projection=glm::perspective(glm::radians(45.0f), aspect, 0.1f, 50.0f);
    const glm::vec3 eye(
        g_scene.distance*std::cos(g_scene.pitch)*std::sin(g_scene.yaw),
        g_scene.distance*std::sin(g_scene.pitch),
        g_scene.distance*std::cos(g_scene.pitch)*std::cos(g_scene.yaw));

    return projection*glm::lookAt(eye, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
}

//! The cube's spin
glm::mat4 modelMatrix()
{
    const float angle=float(std::fmod(g_scene.spun, 1.0)*2.0*3.14159265358979);

    return glm::rotate(glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 1.0f, 0.0f)),
        angle*0.5f, glm::vec3(1.0f, 0.0f, 0.0f));
}

//! Where the cube's centre is on the canvas, in logical units from its top left.
gweni::Point cubeCentre(const gweni::Point &size, float scale)
{
    const glm::vec4 clip=viewProjection(size)*glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    const float x=(clip.x/clip.w*0.5f+0.5f)*size.x;
    const float y=(0.5f-clip.y/clip.w*0.5f)*size.y;

    return gweni::Point(int(std::floor(x/scale+0.5f)), int(std::floor(y/scale+0.5f)));
}

class Cube
{
public:
    bool init()
    {
        // six faces, two triangles each, counter-clockwise from outside:
        // position, normal, colour
        static const float Faces[6][3][3]={
            // normal          u axis          v axis
            {{ 1, 0, 0}, { 0, 0,-1}, { 0, 1, 0}},
            {{-1, 0, 0}, { 0, 0, 1}, { 0, 1, 0}},
            {{ 0, 1, 0}, { 1, 0, 0}, { 0, 0,-1}},
            {{ 0,-1, 0}, { 1, 0, 0}, { 0, 0, 1}},
            {{ 0, 0, 1}, { 1, 0, 0}, { 0, 1, 0}},
            {{ 0, 0,-1}, {-1, 0, 0}, { 0, 1, 0}},
        };
        static const float Colors[6][3]={
            {0.97f, 0.46f, 0.56f}, {0.62f, 0.81f, 0.42f}, {0.48f, 0.64f, 0.97f},
            {0.88f, 0.69f, 0.41f}, {0.73f, 0.60f, 0.97f}, {0.49f, 0.81f, 1.00f},
        };
        static const float Corners[6][2]={{-1,-1}, {1,-1}, {1,1}, {-1,-1}, {1,1}, {-1,1}};
        float vertices[36*9];
        float *v=vertices;

        for(int f=0; f < 6; ++f)
        {
            for(int c=0; c < 6; ++c)
            {
                for(int k=0; k < 3; ++k)
                    *v++=Faces[f][0][k]+Corners[c][0]*Faces[f][1][k]+Corners[c][1]*Faces[f][2][k];
                for(int k=0; k < 3; ++k)
                    *v++=Faces[f][0][k];
                for(int k=0; k < 3; ++k)
                    *v++=Colors[f][k];
            }
        }

        m_program=glCreateProgram();

        const GLuint vertex=compileStage(GL_VERTEX_SHADER, VertexBody);
        const GLuint fragment=compileStage(GL_FRAGMENT_SHADER, FragmentBody);
        GLint ok=GL_FALSE;

        glAttachShader(m_program, vertex);
        glAttachShader(m_program, fragment);
        glLinkProgram(m_program);
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        glGetProgramiv(m_program, GL_LINK_STATUS, &ok);
        if(ok != GL_TRUE)
        {
            std::cerr<<"HudExample: the cube's shader does not link\n";
            return false;
        }
        m_mvp=glGetUniformLocation(m_program, "mvp");
        m_model=glGetUniformLocation(m_program, "model");

        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        for(GLuint attribute=0; attribute < 3; ++attribute)
        {
            glEnableVertexAttribArray(attribute);
            glVertexAttribPointer(attribute, 3, GL_FLOAT, GL_FALSE, 9*sizeof(float),
                reinterpret_cast<const void *>(attribute*3*sizeof(float)));
        }
        glBindVertexArray(0);
        return true;
    }

    void destroy()
    {
        glDeleteBuffers(1, &m_vbo);
        glDeleteVertexArrays(1, &m_vao);
        glDeleteProgram(m_program);
    }

    //! The scene, with its own clear and depth, into the bound framebuffer.
    void draw(const gweni::Point &size, const gweni::Color &clear) const
    {
        const glm::mat4 model=modelMatrix();
        const glm::mat4 mvp=viewProjection(size)*model;

        glViewport(0, 0, size.x, size.y);
        glClearColor(clear.r/255.0f, clear.g/255.0f, clear.b/255.0f, 1.0f);
#if defined(GWENI_GL_ES)
        glClearDepthf(1.0f);
#else
        glClearDepth(1.0);
#endif
        glDepthMask(GL_TRUE);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glDisable(GL_BLEND);

        glUseProgram(m_program);
        glUniformMatrix4fv(m_mvp, 1, GL_FALSE, glm::value_ptr(mvp));
        glUniformMatrix4fv(m_model, 1, GL_FALSE, glm::value_ptr(model));
        glBindVertexArray(m_vao);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

private:
    GLuint m_program=0;
    GLuint m_vao=0;
    GLuint m_vbo=0;
    GLint m_mvp=-1;
    GLint m_model=-1;
};

void resetView()
{
    const SceneState fresh;

    g_scene.yaw=fresh.yaw;
    g_scene.pitch=fresh.pitch;
    g_scene.distance=fresh.distance;
}

//! The spin moved on to \p now (seconds on the library's clock).
void advance(double now)
{
    if(g_scene.spinning)
        g_scene.spun+=(now-g_scene.lastTime)*g_scene.speed;
    g_scene.lastTime=now;
}

//-----------------------------------------------------------------------------
// The HUD
//-----------------------------------------------------------------------------

//! The HUD's panel, holding the buttons and labels. Named Panel because the
//! skin finds a renderer by type name: the textured skin's Panel renderer
//! draws it in the Panel surface, which main() makes translucent.
class Panel: public gweni::controls::Base
{
public:
    GWENI_CONTROL_INLINE(Panel, gweni::controls::Base)
    {
        using namespace gweni;
        using namespace gweni::controls;

        setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        setBounds(16, 16, 250, 196);
        setPadding(Padding(10, 10, 10, 10));

        Label *title=newChild<Label>();
        title->setText("gweni over a GL scene");
        title->setDock(DockPosition::Top);

        Label *hint=newChild<Label>();
        hint->setText("Drag the scene to orbit, wheel to zoom");
        hint->setDock(DockPosition::Top);
        hint->setMargin(Margin(0, 2, 0, 6));

        m_spin=newChild<Button>();
        m_spin->setText("Spin");
        m_spin->setIsToggle(true);
        m_spin->setToggleState(true);
        m_spin->setDock(DockPosition::Top);
        m_spin->setMargin(Margin(0, 0, 0, 4));
        m_spin->onToggleCaller.add(this, &Panel::onSpin);

        m_faster=newChild<Button>();
        m_faster->setText("Faster");
        m_faster->setDock(DockPosition::Top);
        m_faster->setMargin(Margin(0, 0, 0, 4));
        m_faster->onPressCaller.add(this, &Panel::onFaster);

        Button *reset=newChild<Button>();
        reset->setText("Reset view");
        reset->setDock(DockPosition::Top);
        reset->onPressCaller.add(this, &Panel::onReset);

        m_speed=newChild<Label>();
        m_speed->setDock(DockPosition::Bottom);
        showSpeed();
    }

    gweni::controls::Button *spinButton() const { return m_spin; }
    gweni::controls::Button *fasterButton() const { return m_faster; }

    //! The cube's tag, a press on it toggling the spin as Spin does.
    void onTag(gweni::event::Info) { toggleSpin(); }

    //! The scene's keys follow the panel's buttons.
    void toggleSpin() { m_spin->setToggleState(!m_spin->getToggleState()); }

private:
    void onSpin(gweni::event::Info)
    {
        g_scene.spinning=m_spin->getToggleState();
    }

    void onFaster(gweni::event::Info)
    {
        g_scene.speed=(g_scene.speed >= 1.0f) ? 0.15f : g_scene.speed*2.0f;
        showSpeed();
    }

    void onReset(gweni::event::Info)
    {
        resetView();
    }

    void showSpeed()
    {
        char text[64];

        std::snprintf(text, sizeof(text), "Speed: %.2f turns/s", g_scene.speed);
        m_speed->setText(text);
    }

    gweni::controls::Button *m_spin=nullptr;
    gweni::controls::Button *m_faster=nullptr;
    gweni::controls::Label *m_speed=nullptr;
};

//-----------------------------------------------------------------------------
// The window's input: the UI first, the scene what it did not take
//-----------------------------------------------------------------------------

gweni::input::GLFW3 g_glfwInput;
gweni::renderer::Base *g_renderer=nullptr;
gweni::controls::Canvas *g_canvas=nullptr;
Panel *g_panel=nullptr;
float g_manualScale=1.0f;
gweni::Point g_framebuffer;

//! A left drag the canvas did not take: the orbit, until its release.
bool g_orbiting=false;
double g_lastX=0.0;
double g_lastY=0.0;

void keyCallback(GLFWwindow *, int key, int scancode, int action, int mods)
{
    if(key == GLFW_KEY_F10)
    {
        if((action == GLFW_PRESS) && g_canvas)
            gweni::tools::StatsOverlay::toggle(g_canvas);
        return;
    }

    const gweni::input::GLFW3::KeyEvent event={key, scancode, action, mods};

    if(g_glfwInput.processKeyEvent(event) || (action != GLFW_PRESS))
        return;
    // the UI does not have the keyboard: the key is the scene's
    if(key == GLFW_KEY_SPACE && g_panel)
        g_panel->toggleSpin();
    else if(key == GLFW_KEY_R)
        resetView();
}

void charCallback(GLFWwindow *, unsigned int codepoint)
{
    g_glfwInput.processCharEvent(codepoint);
}

void focusCallback(GLFWwindow *, int focused)
{
    g_glfwInput.processFocus(focused == GLFW_TRUE);
    if(focused != GLFW_TRUE)
        g_orbiting=false;
}

void cursorCallback(GLFWwindow *, double x, double y)
{
    // the canvas hovers what is under the cursor, an orbit going on or not;
    // the moves of a press the scene got are the scene's (false) all the way
    const bool taken=g_glfwInput.processMouseMove(x, y);

    if(g_orbiting && !taken)
    {
        g_scene.yaw-=float(x-g_lastX)*0.01f;
        g_scene.pitch=std::fmax(-1.5f, std::fmin(1.5f, g_scene.pitch+float(y-g_lastY)*0.01f));
    }
    g_lastX=x;
    g_lastY=y;
}

void enterCallback(GLFWwindow *, int entered)
{
    g_glfwInput.processCursorEnter(entered == GLFW_TRUE);
}

void buttonCallback(GLFWwindow *window, int button, int action, int mods)
{
    const bool taken=g_glfwInput.processMouseButtons(button, action, mods);

    if(button != GLFW_MOUSE_BUTTON_LEFT)
        return;
    if(taken)
        return;
    // a release gets its press's answer, so the orbit's comes here wherever
    // the cursor is, over the panel too
    if(action == GLFW_PRESS)
    {
        glfwGetCursorPos(window, &g_lastX, &g_lastY);
        g_orbiting=true;
    }
    else if(action == GLFW_RELEASE)
        g_orbiting=false;
}

void scrollCallback(GLFWwindow *, double xoffset, double yoffset)
{
    // a wheel no control scrolled is not the scene's while it is over the HUD
    // (a Label, the panel's bare part)
    if(!g_glfwInput.processScroll(xoffset, yoffset) && g_canvas && !g_canvas->pointerOverControl())
        g_scene.distance=std::fmax(2.0f, std::fmin(12.0f, g_scene.distance*float(std::pow(0.9, yoffset))));
}

void fitCanvas(GLFWwindow *window)
{
    glfwGetFramebufferSize(window, &g_framebuffer.x, &g_framebuffer.y);
    if(g_renderer && g_canvas)
        gweni::capture::fitCanvasToWindow(window, g_manualScale, *g_renderer, *g_canvas, g_glfwInput);
}

void framebufferCallback(GLFWwindow *window, int, int)
{
    fitCanvas(window);
}

void contentScaleCallback(GLFWwindow *window, float, float)
{
    fitCanvas(window);
}

//-----------------------------------------------------------------------------
// A capture's clock and the line it prints
//-----------------------------------------------------------------------------

int64_t g_clockSteps=0;

double steppedClock()
{
    return double(g_clockSteps)*(1.0/60.0);
}

//! Frames a --screenshot renders: the dock layout settled and the cube turned.
const int CaptureFrames=30;

//! A logical point of the canvas in device px, rounded as the renderer snaps.
gweni::Point toDevice(float x, float y, float scale)
{
    return gweni::Point(int(std::floor(x*scale+0.5f)), int(std::floor(y*scale+0.5f)));
}

bool parseColor(const char *text, gweni::Color &color)
{
    int r=0, g=0, b=0;
    char end=0;

    if(std::sscanf(text, "%d,%d,%d%c", &r, &g, &b, &end) != 3)
        return false;
    if(r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255)
        return false;
    color=gweni::Color(uint8_t(r), uint8_t(g), uint8_t(b), 255);
    return true;
}

void printUsage(std::ostream &out)
{
    out<<"Usage: HudExample [--screenshot <out.png>] [--headless]\n"
         "                  [--stats-overlay [on|off]] [--clear <r,g,b>]\n"
         "                  [--scale <s>] [--resources <dir>] [--renderer gl]\n"
         "\n"
         "  --screenshot <path>  render offscreen on the stepped clock, write one\n"
         "                       PNG, print where the scene, a Button and the\n"
         "                       panel are, and exit\n"
         "  --headless           force the surfaceless EGL context; needs\n"
         "                       --screenshot\n"
         "  --stats-overlay [on|off]  open the stats overlay (F10 toggles it)\n"
         "  --clear <r,g,b>      the scene's background colour (default 26,32,52)\n"
         "  --scale <s>          draw at s device px per logical unit times the OS\n"
         "                       content scale (default 1, or GWENI_SCALE)\n"
         "  --resources <dir>    load the skin, fonts and textures from <dir>\n"
         "  --renderer <gl>      the backend (default gl, or GWENI_RENDERER); a\n"
         "                       Vulkan HUD example is not there yet\n"
         "\n"
         "In the window the input the UI did not take is the scene's: a left drag\n"
         "from the bare canvas orbits the cube, the wheel zooms, Space toggles the\n"
         "spin and R resets the view while no control has the keyboard; F10\n"
         "toggles the stats overlay.\n";
}

}//namespace

int main(int argc, char *argv[])
{
    const gweni::Point screenSize(960, 600);

    gweni::capture::setToolName("HudExample");

    std::string shotPath;
    std::string resourceDir;
    std::string rendererName;
    bool scaleGiven=false;
    bool forceHeadless=false;
    bool statsOverlay=false;
    gweni::Color clear(26, 32, 52, 255);

    for(int i=1; i < argc; ++i)
    {
        const std::string arg=argv[i];
        const bool hasValue=(i+1) < argc;

        if(arg == "--screenshot" && hasValue)
            shotPath=argv[++i];
        else if(arg == "--headless")
            forceHeadless=true;
        else if(arg == "--stats-overlay")
            statsOverlay=gweni::capture::onOffArgument(argc, argv, i);
        else if(arg == "--clear" && hasValue)
        {
            if(!parseColor(argv[++i], clear))
            {
                std::cerr<<"HudExample: --clear wants r,g,b, each 0..255\n";
                return EXIT_FAILURE;
            }
        }
        else if(arg == "--scale" && hasValue)
        {
            if(!gweni::capture::parseScale(argv[++i], g_manualScale))
            {
                std::cerr<<"HudExample: --scale wants a number in (0, 16], e.g. 1.5\n";
                return EXIT_FAILURE;
            }
            scaleGiven=true;
        }
        else if(arg == "--resources" && hasValue)
            resourceDir=argv[++i];
        else if(arg == "--renderer" && hasValue)
            rendererName=argv[++i];
        else if(arg == "--help" || arg == "-h")
        {
            printUsage(std::cout);
            return EXIT_SUCCESS;
        }
        else
        {
            std::cerr<<"HudExample: unexpected argument "<<arg<<"\n";
            printUsage(std::cerr);
            return EXIT_FAILURE;
        }
    }

    if(!gweni::capture::manualScale(scaleGiven, g_manualScale))
        return EXIT_FAILURE;

    gweni::capture::Backend backend;

    if(!gweni::capture::chooseBackend(rendererName, backend))
        return EXIT_FAILURE;
    if(backend != gweni::capture::Backend::GL)
    {
        std::cerr<<"HudExample: --renderer vulkan: the HUD example draws its scene with GL;"
                   " a Vulkan one is not there yet\n";
        return EXIT_FAILURE;
    }

    const bool capturing=!shotPath.empty();

    if(forceHeadless && !capturing)
    {
        std::cerr<<"HudExample: --headless has no window to show; pass --screenshot\n";
        return EXIT_FAILURE;
    }
    if(capturing && !gweni::capture::ensureDirectory(fs::path(shotPath).parent_path()))
        return EXIT_FAILURE;

    bool resourcesFound=true;
    const std::unique_ptr<gweni::ResourcePaths> paths=gweni::capture::resourcePaths(resourceDir, resourcesFound);

    gweni::capture::installGlfwErrorHandler();

    gweni::capture::DeviceOptions deviceOptions;

    deviceOptions.size=screenSize;
    deviceOptions.title="gweni HUD over a GL scene";
    deviceOptions.capturing=capturing;
    deviceOptions.headless=forceHeadless;

    std::unique_ptr<gweni::capture::Device> device=gweni::capture::openDevice(backend, deviceOptions);

    if(!device)
        return EXIT_FAILURE;

    const gweni::capture::Context &ctx=device->context();

    Cube cube;

    if(!cube.init())
        return EXIT_FAILURE;

    gweni::renderer::SlotRenderer *renderer=
        device->newRenderer(*paths, gweni::Rect(gweni::Point(0, 0), screenSize));
    // the scene's GL state comes back after each of gweni's frames
    if(auto *gl=dynamic_cast<gweni::renderer::OpenGLCore *>(renderer))
        gl->setPreserveAppState(true);
    renderer->init();

    gweni::skin::textured::TexturedSkin *skin=new gweni::skin::textured::TexturedSkin(renderer);
    bool degraded=!resourcesFound;

    if(!skin->init("DefaultSkin.json"))
        degraded=true;
    if(gweni::capture::loadDefaultFont(*renderer, skin->getDefaultFont()) != gweni::Font::Status::Loaded)
        degraded=true;

    // The HUD's panel is the theme's Panel surface, flat and translucent, so
    // the scene shows through it; this app has no other control drawn with it.
    tokyonight::Panel.top=gweni::Color(0x20, 0x23, 0x36, 0xb0);
    tokyonight::Panel.bottom=tokyonight::Panel.top;

    // The canvas draws no background (Canvas::setDrawBackground is off by
    // default): the scene shows wherever no control draws.
    gweni::controls::Canvas *canvas=new gweni::controls::Canvas(skin);
    const gweni::Point canvasSize=gweni::capture::logicalSize(screenSize, g_manualScale);

    canvas->setScale(g_manualScale);
    canvas->setSize(canvasSize.x, canvasSize.y);

    Panel *panel=canvas->newChild<Panel>();

    // A tag on the cube, a control placed each frame at the cube's centre on
    // screen: drawn over the cube only because clearDepth() cleared the
    // cube's depth first.
    gweni::controls::Button *tag=canvas->newChild<gweni::controls::Button>();

    tag->setText("Cube");
    tag->setSizeFlags({gweni::SizeFlag::Fixed, gweni::SizeFlag::Fixed});
    tag->setToolTipText("Toggles the spin");
    tag->onPressCaller.add(panel, &Panel::onTag);

    g_renderer=renderer;
    g_canvas=canvas;
    g_panel=panel;
    g_framebuffer=screenSize;

    if(statsOverlay)
        gweni::tools::StatsOverlay::open(canvas);

    int result=EXIT_SUCCESS;

    // The app's frame: its scene with its own depth, then the scene's depth
    // cleared so it hides no control, then the UI over it.
    auto frame=[canvas, renderer, tag, &cube, clear]()
    {
        advance(gweni::platform::getTimeInSeconds());

        const gweni::Point centre=cubeCentre(g_framebuffer, canvas->scale());

        // as high as the panel's buttons, so it is drawn as they are
        tag->setSize(64, g_panel->fasterButton()->getHeight());
        tag->setPos(centre.x-tag->getWidth()/2, centre.y-tag->getHeight()/2);
        canvas->updateCanvas();
        cube.draw(g_framebuffer, clear);
        renderer->clearDepth();
        canvas->renderCanvas();
    };

    if(capturing)
    {
        gweni::platform::setTimeSource(&steppedClock);
        gweni::capture::installFixedProfileClock();

        for(int i=0; i < CaptureFrames; ++i)
        {
            frame();
            ++g_clockSteps;
        }

        if(gweni::capture::writeRendererPng(*renderer, screenSize.x, screenSize.y, shotPath))
        {
            // where the test reads its pixels: the scene clear of the cube and
            // the panel, a Button inside its border left of its text, and the
            // panel's bottom padding, which no child covers
            const float scale=canvas->scale();
            const gweni::Point button=panel->spinButton()->localPosToCanvas();
            const gweni::Point scene=toDevice(4.0f, 4.0f, scale);
            const gweni::Point buttonProbe=toDevice(button.x+6.0f,
                button.y+panel->spinButton()->getHeight()*0.5f, scale);
            const gweni::Point faster=panel->fasterButton()->localPosToCanvas();
            const gweni::Point fasterProbe=toDevice(faster.x+6.0f,
                faster.y+panel->fasterButton()->getHeight()*0.5f, scale);
            const gweni::Point tagProbe=toDevice(tag->getX()+6.0f,
                tag->getY()+tag->getHeight()*0.5f, scale);
            const gweni::Point panelProbe=toDevice(panel->getX()+12.0f,
                panel->getY()+panel->getHeight()-4.0f, scale);
            const gweni::Color &tint=tokyonight::Panel.top;

            std::printf("HudExample: clear=%d,%d,%d panel_color=%d,%d,%d,%d"
                " probe_scene=%d,%d probe_button=%d,%d probe_panel=%d,%d"
                " probe_faster=%d,%d probe_tag=%d,%d\n",
                clear.r, clear.g, clear.b, tint.r, tint.g, tint.b, tint.a,
                scene.x, scene.y, buttonProbe.x, buttonProbe.y, panelProbe.x, panelProbe.y,
                fasterProbe.x, fasterProbe.y, tagProbe.x, tagProbe.y);
            std::cout<<"wrote "<<shotPath<<"\n";
        }
        else
            result=EXIT_FAILURE;
    }
    else
    {
        g_glfwInput.initialize(canvas);

        glfwSetKeyCallback(ctx.window, keyCallback);
        glfwSetCharCallback(ctx.window, charCallback);
        glfwSetWindowFocusCallback(ctx.window, focusCallback);
        glfwSetCursorPosCallback(ctx.window, cursorCallback);
        glfwSetCursorEnterCallback(ctx.window, enterCallback);
        glfwSetMouseButtonCallback(ctx.window, buttonCallback);
        glfwSetScrollCallback(ctx.window, scrollCallback);
        glfwSetFramebufferSizeCallback(ctx.window, framebufferCallback);
        glfwSetWindowContentScaleCallback(ctx.window, contentScaleCallback);
        fitCanvas(ctx.window);

        // the scene moves every frame, so the loop draws every frame
        gweni::platform::LoopOptions options=gweni::capture::loopOptions();

        options.mode=gweni::platform::LoopMode::Continuous;
        g_scene.lastTime=gweni::platform::getTimeInSeconds();
        device->runWindowLoop(canvas, renderer, options, frame);
    }

    g_panel=nullptr;
    g_canvas=nullptr;
    delete canvas;
    delete skin;
    delete renderer;
    cube.destroy();
    device.reset();

    return capturing ? gweni::capture::exitStatus(result, degraded) : result;
}
