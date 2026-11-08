/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  doctest main for gweni_unittests: sets up one device and one renderer with
 *  OpenSans loaded, runs every test case against them, then tears them down.
 *  GWENI_RENDERER picks the backend: gl (the default) is a GL context on the
 *  examples' surfaceless EGL path (gweni_capture) and an OpenGLCore drawing into
 *  an offscreen framebuffer; vulkan is a Vulkan device with the validation layer
 *  (any warning or error aborts) and a renderer::Vulkan drawing into a
 *  VulkanTarget, with no GL or EGL context at all. The tests see either as the
 *  SlotRenderer it is (gpuRenderer.h); this file is the one place that knows the
 *  backend. Run it from the repo root, as ctest does: the renderer loads
 *  resources/.
 */
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "capture.h"
#include "gpuRenderer.h"

#include <gweni/platforms/renderers/openglCore.h>
#ifdef GWENI_HAS_VULKAN
#include <gweni/platforms/renderers/vulkan.h>
#endif

#include <gweni/platforms/commonPlatform.h>

#include <cstdlib>
#include <iostream>
#include <string>

namespace gweni
{
namespace test
{

static renderer::SlotRenderer *g_renderer=nullptr;
static Font g_openSans;
static const Point g_viewSize(448, 160);
#ifdef GWENI_HAS_VULKAN
// set in a Vulkan run: the device and the target every renderer draws into
static renderer::VulkanDevice *g_vulkanDevice=nullptr;
static renderer::VulkanTarget *g_vulkanTarget=nullptr;
// g_vulkanDevice's handles, claiming no dualSrcBlend (newRendererWithoutDualSource)
static renderer::VulkanDevice g_noDualSourceDevice;
#endif

renderer::SlotRenderer &gpuRenderer()
{
    return *g_renderer;
}

renderer::SlotRenderer *newRenderer(ResourcePaths &paths, const Rect &viewRect)
{
#ifdef GWENI_HAS_VULKAN
    if(g_vulkanDevice)
        return new renderer::Vulkan(paths, viewRect, *g_vulkanDevice, *g_vulkanTarget);
#endif
    return new renderer::OpenGLCore(paths, viewRect);
}

renderer::SlotRenderer *newRendererWithoutDualSource(ResourcePaths &paths, const Rect &viewRect)
{
#ifdef GWENI_HAS_VULKAN
    if(g_vulkanDevice)
    {
        g_noDualSourceDevice=*g_vulkanDevice;
        g_noDualSourceDevice.dualSourceBlend=false;
        return new renderer::Vulkan(paths, viewRect, g_noDualSourceDevice, *g_vulkanTarget);
    }
#endif
    // read at construction only, so it is put back at once
    const char *previous=getenv("GWENI_GL_NO_DUAL_SOURCE");
    const std::string kept=previous?previous:"";

    setenv("GWENI_GL_NO_DUAL_SOURCE", "1", 1);

    renderer::SlotRenderer *made=new renderer::OpenGLCore(paths, viewRect);

    if(previous)
        setenv("GWENI_GL_NO_DUAL_SOURCE", kept.c_str(), 1);
    else
        unsetenv("GWENI_GL_NO_DUAL_SOURCE");
    return made;
}

bool deviceHasDualSource()
{
#ifdef GWENI_HAS_VULKAN
    if(g_vulkanDevice)
        return g_vulkanDevice->dualSourceBlend;
#endif
    return dynamic_cast<renderer::OpenGLCore &>(*g_renderer).glCapabilities().dualSourceBlend;
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

namespace
{

using namespace gweni;

//! Loads OpenSans into the shared renderer and runs every case, then frees it.
int runCases(doctest::Context &context, renderer::SlotRenderer *renderer)
{
    test::g_openSans.facename="OpenSans.ttf";
    test::g_openSans.size=11;

    if(renderer->loadFont(test::g_openSans) != Font::Status::Loaded)
    {
        std::cerr<<"gweni_unittests: could not load resources/OpenSans.ttf"
                   " (run from the repo root)\n";
        return EXIT_FAILURE;
    }

    test::g_renderer=renderer;

    const int result=context.run();

    test::g_renderer=nullptr;
    renderer->freeFont(test::g_openSans);
    return result;
}

int runGl(doctest::Context &context)
{
    capture::Context ctx;

#if GWENI_EXAMPLE_EGL
    if(!capture::initEglContext(ctx))
        return EXIT_FAILURE;
#else
    std::cerr<<"gweni_unittests: needs the surfaceless EGL path (Linux)\n";
    return EXIT_FAILURE;
#endif

    if(!capture::initGL(ctx))
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

    const int result=runCases(context, renderer);

    delete renderer;
    capture::destroyFramebuffer(fb);
    capture::destroyContext(ctx);
    return result;
}

int runVulkan(doctest::Context &context)
{
#ifdef GWENI_HAS_VULKAN
    renderer::VulkanDevice device;
    renderer::VulkanDeviceOptions options;
    std::string error;

    // every Vulkan test runs validated, and never without the layer
    options.applicationName="gweni_unittests";
    options.validation=true;
    if(!renderer::createVulkanDevice(device, options, error))
    {
        std::cerr<<"gweni_unittests: "<<error<<"\n";
        return EXIT_FAILURE;
    }

    const Point viewSize=test::viewSize();
    int result=EXIT_FAILURE;

    {
        renderer::VulkanTarget target(device);

        if(target.create(viewSize.x, viewSize.y))
        {
            platform::CurrentPath paths("resources");
            renderer::Vulkan *renderer=new renderer::Vulkan(paths, Rect(Point(0, 0), viewSize), device, target);

            renderer->init();
            test::g_vulkanDevice=&device;
            test::g_vulkanTarget=&target;
            result=runCases(context, renderer);
            test::g_vulkanDevice=nullptr;
            test::g_vulkanTarget=nullptr;
            delete renderer;
        }
        else
            std::cerr<<"gweni_unittests: could not make a "<<viewSize.x<<"x"<<viewSize.y<<" Vulkan target\n";
    }
    renderer::destroyVulkanDevice(device);
    return result;
#else
    (void)context;
    std::cerr<<"gweni_unittests: GWENI_RENDERER=vulkan, but this tree is built without Vulkan"
               " (GWENI_RENDER_VULKAN)\n";
    return EXIT_FAILURE;
#endif
}

}//namespace

int main(int argc, char **argv)
{
    using namespace gweni;

    doctest::Context context(argc, argv);

    // --help, --list-test-cases and the like need no device
    if(context.shouldExit())
        return context.run();

    capture::setToolName("gweni_unittests");

    const char *backend=getenv("GWENI_RENDERER");
    const std::string name=backend?backend:"gl";

    if(name == "vulkan")
        return runVulkan(context);
    if(name != "gl")
    {
        std::cerr<<"gweni_unittests: GWENI_RENDERER="<<name<<": not gl or vulkan\n";
        return EXIT_FAILURE;
    }
    return runGl(context);
}
