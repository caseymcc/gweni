/*
 *  Gweni - single-control test harness
 *
 *  Drops ONE control (via its demo builder) directly on the canvas, so each
 *  control can be verified in isolation without the demo's dock/page layout.
 *
 *  Usage: ControlTest [DemoName]   (default: ButtonDemo)
 *  e.g.   ControlTest CheckBoxDemo
 */

#include <GL/glew.h>
#include <gweni/platforms/renderers/openglCore.h>
#include <gweni/gweni.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/platforms/input/glfw3.h>
#include <gweni/controls/canvas.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <string>

#include <demo/button.h>
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
#include <demo/window.h>

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

static gweni::controls::Base *createControl(gweni::controls::Canvas *canvas, const std::string &which)
{
#define TRY(N) if(which==#N) return canvas->newChild<gweni::N>();
    TRY(ButtonDemo) TRY(LabelDemo) TRY(LabelMultilineDemo) TRY(ProgressBarDemo)
    TRY(GroupBoxDemo) TRY(ImagePanelDemo) TRY(StatusBarDemo) TRY(ComboBoxDemo)
    TRY(TextBoxDemo) TRY(ListBoxDemo) TRY(CrossSplitterDemo) TRY(RadioButtonDemo)
    TRY(CheckBoxDemo) TRY(NumericDemo) TRY(SliderDemo) TRY(MenuStripDemo)
    TRY(WindowDemo) TRY(TreeControlDemo) TRY(PropertiesDemo) TRY(TabControlDemo)
    TRY(ScrollControlDemo) TRY(PageControlDemo) TRY(CollapsibleListDemo) TRY(ColorPickerDemo)
#undef TRY
    return nullptr;
}

int main(int argc, char *argv[])
{
    const std::string which = argc>1 ? argv[1] : "ButtonDemo";
    const gweni::Point screenSize(640, 480);

    if(!glfwInit()) return EXIT_FAILURE;
    GLFWwindow *window=glfwCreateWindow(screenSize.x, screenSize.y,
        ("Gweni ControlTest - "+which).c_str(), NULL, NULL);
    if(!window){ glfwTerminate(); return EXIT_FAILURE; }
    glfwMakeContextCurrent(window);
    glewExperimental=GL_TRUE;
    if(glewInit()!=GLEW_OK){ glfwTerminate(); return EXIT_FAILURE; }

    gweni::platform::CurrentPath paths("resources");
    gweni::renderer::OpenGLCore *renderer=
        new gweni::renderer::OpenGLCore(paths, gweni::Rect(gweni::Point(0,0), screenSize));
    renderer->init();

    gweni::skin::textured::TexturedSkin *skin=new gweni::skin::textured::TexturedSkin(renderer);
    skin->init("resources/DefaultSkin.json");
    skin->setDefaultFont("resources/OpenSans.ttf", 11);

    gweni::controls::Canvas *canvas=new gweni::controls::Canvas(skin);
    canvas->setSize(screenSize.x, screenSize.y);
    canvas->setDrawBackground(true);
    canvas->setBackgroundColor(gweni::Color(200, 200, 200, 255));

    g_renderer=renderer;
    g_canvas=canvas;

    gweni::controls::Base *control=createControl(canvas, which);
    if(!control)
    {
        std::cout<<"Unknown control: "<<which<<"\n";
        return EXIT_FAILURE;
    }
    control->setDock(gweni::DockPosition::Center);

    g_glfwInput.initialize(canvas);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    while(!glfwWindowShouldClose(window))
    {
        canvas->updateCanvas();
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        canvas->renderCanvas();
        glfwSwapBuffers(window);
        glfwPollEvents();
        gweni::platform::sleep(0);
    }

    delete control;
    delete canvas;
    delete skin;
    delete renderer;
    glfwTerminate();
    return EXIT_SUCCESS;
}
