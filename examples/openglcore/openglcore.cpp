/*
 *  Gweni

 *  See license in Gweni.h
 */

#include <GL/glew.h>
#ifdef USE_DEBUG_FONT
#   include <gweni/platforms/renderers/opengl_debugFont.h>
#else
#   include <gweni/platforms/renderers/openglCore.h>
#endif
#include <gweni/gweni.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/simple/simple.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>
#include <gweni/platforms/input/glfw3.h>
#include <gweni/controls/canvas.h>
#include <iostream>

#include <demo/demo.h>

#include <GLFW/glfw3.h>

//namespace gweni
//{
//namespace test
//{
//GWENI_DECLARE_TEST(Layout);
//}
//}

static gweni::input::GLFW3 g_glfwInput;
static gweni::renderer::Base *g_renderer=nullptr;
static gweni::controls::Canvas *g_canvas=nullptr;

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    const gweni::input::GLFW3::KeyEvent evt = { key, scancode, action, mods };
    g_glfwInput.processKeyEvent(evt);
}

static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
{
    g_glfwInput.processMouseMove(xpos, ypos);
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    g_glfwInput.processMouseButtons(button, action, mods);
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    g_glfwInput.processScroll(xoffset, yoffset);
}

static void framebuffer_size_callback(GLFWwindow* /*window*/, int width, int height)
{
    if(width <= 0 || height <= 0)
        return;

    // Resize the GL viewport/projection and the root canvas so docked controls
    // re-layout to the new window size.
    if(g_renderer)
        g_renderer->setViewRect(gweni::Rect(gweni::Point(0, 0), gweni::Point(width, height)));

    if(g_canvas)
        g_canvas->setSize(width, height);
}

void glfwErrorCallback(int error_code, const char *description)
{
    std::cout<<"GLFW error: "<<error_code<<" "<<description<<std::endl;
}


int main(int argc, char *argv[])
{
    const gweni::Point screenSize(1024,768);

    glfwSetErrorCallback(&glfwErrorCallback);

    if (!glfwInit())
    {
        glfwTerminate();
        return EXIT_FAILURE;
    }

//    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
//    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
//    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
//    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);%

    // Create a new window
    GLFWwindow *window=glfwCreateWindow(screenSize.x, screenSize.y,
        "Gweni OpenGL Core Profile Sample", NULL, NULL);

    if(!window)
    {
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
//    glfwSwapInterval(0); //removes fps limiter
    glewExperimental = GL_TRUE;
    GLuint error;

    if((error = glewInit()) != GLEW_OK)
    {
        std::cout<<"Glew init error: "<<glewGetErrorString(error)<<std::endl;
        glfwTerminate();
        return EXIT_FAILURE;
    }

//    gweni::platform::RelativeToExecutablePaths paths("../../resources/");
    gweni::platform::CurrentPath paths("resources");

    // Create a Gweni OpenGL Renderer
#ifdef USE_DEBUG_FONT
    gweni::renderer::OpenGL* renderer = new gweni::renderer::OpenGL_DebugFont();
#else
    gweni::renderer::OpenGLCore* renderer =
        new gweni::renderer::OpenGLCore(paths, gweni::Rect(gweni::Point(0,0), screenSize));
#endif
    renderer->init();

    // Create a Gweni skin
    gweni::skin::textured::TexturedSkin *skin = new gweni::skin::textured::TexturedSkin(renderer);

//    skin->init("DefaultSkin.png");
    skin->init("resources/DefaultSkin.json");
    skin->setDefaultFont("resources/OpenSans.ttf", 11);

    // Create a Canvas (it's root, on which all other Gweni panels are created)
    // The canvas fill comes from the theme, so a "theme" block in
    // resources/DefaultSkin.json moves it too. Canvas::renderCanvas does not
    // currently paint its own background (the draw call there is commented
    // out), so the clear color is what actually shows.
    const gweni::Color background=gweni::skin::textured::tokyonight::Canvas.top;
    glClearColor(background.r/255.0f, background.g/255.0f, background.b/255.0f, background.a/255.0f);

    gweni::controls::Canvas* canvas = new gweni::controls::Canvas(skin);
    canvas->setSize(screenSize.x, screenSize.y);
    canvas->setDrawBackground(true);
    canvas->setBackgroundColor(background);

    g_renderer=renderer;
    g_canvas=canvas;

//#define SINGLE_CONTROL

#ifndef SINGLE_CONTROL
    gweni::controls::Base *demo=canvas->newChild<gweni::Demo>();
#else//SINGLE_CONTROL
    gweni::controls::Button *button=canvas->newChild<gweni::controls::Button>();

    button->setText("ButtonTest");
#endif//SINGLE_CONTROL

    g_glfwInput.initialize(canvas);

    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // Apply the actual framebuffer size now (it can differ from the requested
    // window size, e.g. on high-DPI displays).
    {
        int fbWidth=0, fbHeight=0;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        framebuffer_size_callback(window, fbWidth, fbHeight);
    }

    // Begin the main game loop
    while (!glfwWindowShouldClose(window))
    {
        canvas->updateCanvas();
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        canvas->renderCanvas();
        glfwSwapBuffers(window);

        glfwPollEvents();

        gweni::platform::sleep(0);
    }

#ifndef SINGLE_CONTROL
    delete demo;
#else//SINGLE_CONTROL
    delete button;
#endif//SINGLE_CONTROL

    delete canvas;
    delete skin;
    delete renderer;

    // Clean up OpenGL
    glfwTerminate();

    return EXIT_SUCCESS;
}
