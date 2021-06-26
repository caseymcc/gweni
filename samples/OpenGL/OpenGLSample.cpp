/*
 *  Gweni
 *  Copyright (c) 2012 Facepunch Studios
 
 *  See license in Gweni.h
 */

#include <gweni/Gweni.h>
#include <gweni/platforms/platform.h>
#include <gweni/Skins/Simple.h>
#include <gweni/Skins/TexturedBase.h>
#include <gweni/Test/Test.h>
#include <gweni/Input/GLFW3.h>
#ifdef USE_DEBUG_FONT
#   include <gweni/Renderers/OpenGL_DebugFont.h>
#else
#   include <gweni/Renderers/OpenGL.h>
#endif
#include <GLFW/glfw3.h>

static gweni::input::GLFW3 g_GweniInput;

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    const gweni::input::GLFW3::KeyEvent evt = { key, scancode, action, mods };
    g_GweniInput.ProcessKeyEvent(evt);
}

static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
{
    g_GweniInput.ProcessMouseMove(xpos, ypos);
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    g_GweniInput.ProcessMouseButtons(button, action, mods);
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    g_GweniInput.ProcessScroll(xoffset, yoffset);
}


int main()
{
    const gweni::Point screenSize(1024,768);

    if (!glfwinit())
        return -1;

    // Create a new window
    GLFWwindow* window = glfwCreateWindow(screenSize.x, screenSize.y,
                                          "Gweni OpenGL Sample", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    gweni::Platform::RelativeToExecutablePaths paths(GWENI_SAMPLE_RESOURCE_DIR);

    // Create a Gweni OpenGL Renderer
#ifdef USE_DEBUG_FONT
    gweni::renderer::OpenGL* renderer = new gweni::renderer::OpenGL_DebugFont();
#else
    gweni::renderer::OpenGL* renderer =
        new gweni::renderer::OpenGL(paths, gweni::Rect(gweni::Point(0,0), screenSize));
#endif
    renderer->init();

    // Create a Gweni skin
    gweni::skin::TexturedBase* skin = new gweni::skin::TexturedBase(renderer);
    skin->init("DefaultSkin.png");
    skin->SetDefaultFont("OpenSans.ttf", 11);

    // Create a Canvas (it's root, on which all other Gweni panels are created)
    gweni::Controls::Canvas* canvas = new gweni::Controls::Canvas(skin);
    canvas->SetSize(screenSize.x, screenSize.y);
    canvas->SetDrawBackground(true);
    canvas->SetBackgroundColor(gweni::Color(150, 170, 170, 255));

    // Create our unittest control (which is a Window with controls in it)
    auto unit = gweni::Test::CreateTests(canvas);
    g_GweniInput.Initialize(canvas);

    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // Begin the main game loop
    while (!glfwWindowShouldClose(window))
    {
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        canvas->RenderCanvas();
        glfwSwapBuffers(window);

        glfwPollEvents();

        gweni::Platform::sleep(0);
    }

    delete unit;
    delete canvas;
    delete skin;
    delete renderer;

    // Clean up OpenGL
    glfwTerminate();

    return EXIT_SUCCESS;
}
