/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  doctest main for gweni_unittests: sets up one GL context (the examples'
 *  surfaceless EGL path, gweni_capture) and one OpenGLCore renderer with
 *  OpenSans loaded, runs every test case against them, then tears them down.
 *  Run it from the repo root, as ctest does: the renderer loads resources/.
 */
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "capture.h"
#include "gpuRenderer.h"

#include <gweni/platforms/commonPlatform.h>

#include <cstdlib>
#include <iostream>

namespace gweni
{
namespace test
{

static renderer::OpenGLCore *g_renderer=nullptr;
static Font g_openSans;
static const Point g_viewSize(448, 160);

renderer::OpenGLCore &gpuRenderer()
{
    return *g_renderer;
}

const Font &openSans()
{
    return g_openSans;
}

Point viewSize()
{
    return g_viewSize;
}

}//namespace test
}//namespace gweni

int main(int argc, char **argv)
{
    using namespace gweni;

    doctest::Context context(argc, argv);

    // --help, --list-test-cases and the like need no GL context
    if(context.shouldExit())
        return context.run();

    capture::setToolName("gweni_unittests");

    capture::Context ctx;

#if GWENI_EXAMPLE_EGL
    if(!capture::initEglContext(ctx))
        return EXIT_FAILURE;
#else
    std::cerr<<"gweni_unittests: needs the surfaceless EGL path (Linux)\n";
    return EXIT_FAILURE;
#endif

    if(!capture::initGlew())
    {
        capture::destroyContext(ctx);
        return EXIT_FAILURE;
    }

    const Point viewSize=test::viewSize();
    capture::Framebuffer fb;

    if(!capture::createFramebuffer(fb, viewSize.x, viewSize.y))
    {
        capture::destroyContext(ctx);
        return EXIT_FAILURE;
    }

    platform::CurrentPath paths("resources");
    renderer::OpenGLCore *renderer=new renderer::OpenGLCore(paths, Rect(Point(0, 0), viewSize));
    renderer->init();

    test::g_openSans.facename="OpenSans.ttf";
    test::g_openSans.size=11;

    if(renderer->loadFont(test::g_openSans) != Font::Status::Loaded)
    {
        std::cerr<<"gweni_unittests: could not load resources/OpenSans.ttf"
                   " (run from the repo root)\n";
        delete renderer;
        capture::destroyFramebuffer(fb);
        capture::destroyContext(ctx);
        return EXIT_FAILURE;
    }

    test::g_renderer=renderer;

    const int result=context.run();

    test::g_renderer=nullptr;
    renderer->freeFont(test::g_openSans);
    delete renderer;
    capture::destroyFramebuffer(fb);
    capture::destroyContext(ctx);
    return result;
}
