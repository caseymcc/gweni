/*
 *  Gweni
 
 *  See license in Gweni.h
 */

#include <GL/glew.h>
#ifdef USE_DEBUG_FONT
#   include <gweni/platforms/renderers/OpenGL_DebugFont.h>
#else
#   include <gweni/platforms/renderers/OpenGLCore.h>
#endif
#include <gweni/Gweni.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/simple.h>
#include <gweni/skins/texturedSkin.h>
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

int main(int argc, char *argv[])
{
    const gweni::Point screenSize(1024,768);

    if (!glfwInit())
        return EXIT_FAILURE;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

    // Create a new window
    GLFWwindow* window = glfwCreateWindow(screenSize.x, screenSize.y,
                                          "Gweni OpenGL Core Profile Sample", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE;
    GLuint error;
    if ((error = glewInit()) != GLEW_OK)
    {
        std::cout << "Glew init error: " << glewGetErrorString(error) << std::endl;
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
    gweni::skin::TexturedSkin *skin = new gweni::skin::TexturedSkin(renderer);

//    skin->init("DefaultSkin.png");
    skin->setDefaultFont("OpenSans.ttf", 11);

    // Create a Canvas (it's root, on which all other Gweni panels are created)
    gweni::controls::Canvas* canvas = new gweni::controls::Canvas(skin);
    canvas->setSize(screenSize.x, screenSize.y);
    canvas->setDrawBackground(true);
    canvas->setBackgroundColor(gweni::Color(150, 170, 170, 255));

    gweni::controls::Base *demo=new gweni::Demo(canvas);

    g_glfwInput.initialize(canvas);

    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // Begin the main game loop
    while (!glfwWindowShouldClose(window))
    {
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        canvas->renderCanvas();
        glfwSwapBuffers(window);

        glfwPollEvents();

        gweni::platform::sleep(0);
    }

    delete demo;
    delete canvas;
    delete skin;
    delete renderer;

    // Clean up OpenGL
    glfwTerminate();

    return EXIT_SUCCESS;
}
