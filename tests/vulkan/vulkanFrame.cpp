/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  vulkan_frame: gweni draws a frame with renderer::Vulkan and nothing GL, as a
 *  Vulkan-only app would. It creates a validated device (createVulkanDevice,
 *  any validation warning or error aborts), a VulkanTarget and a renderer on
 *  it, loads the textured skin and its font from resources/ (run from the repo
 *  root), puts a Label, a Button, a CheckBoxWithLabel and a TextBox on a
 *  canvas, clears, updates and renders one frame, reads it back and fails
 *  unless it holds more than one colour. A skin or font that does not load, or
 *  a device error, fails it too, each with one line naming it.
 *
 *  It links gweni alone, so in the Vulkan-only tree (./generate.sh linux
 *  vulkan) it is the binary vulkanFrameLinks.cmake checks for GL libraries
 *  and symbols. Out of the glob of tests/ sources, as vulkan_smoke is.
 */
#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/checkBox.h>
#include <gweni/controls/label.h>
#include <gweni/controls/textBox.h>
#include <gweni/platforms/commonPlatform.h>
#include <gweni/platforms/renderers/vulkan.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace
{

using namespace gweni;

constexpr int Width=320;
constexpr int Height=200;

int fail(const char *message)
{
    std::fprintf(stderr, "vulkan_frame: %s\n", message);
    return EXIT_FAILURE;
}

int drawFrame(renderer::Vulkan &renderer)
{
    skin::textured::TexturedSkin skin(&renderer);

    if(!skin.init("DefaultSkin.json"))
        return fail("the textured skin did not load resources/DefaultSkin.json");
    if(renderer.loadFont(skin.getDefaultFont()) != Font::Status::Loaded)
        return fail("the skin's default font did not load from resources/");

    controls::Canvas *canvas=new controls::Canvas(&skin);
    canvas->setSize(Width, Height);

    controls::Label *label=canvas->newChild<controls::Label>();
    label->setText("gweni on Vulkan");
    label->setBounds(10, 10, 200, 20);
    controls::Button *button=canvas->newChild<controls::Button>();
    button->setText("Button");
    button->setBounds(10, 40, 100, 24);
    controls::CheckBoxWithLabel *check=canvas->newChild<controls::CheckBoxWithLabel>();
    check->label()->setText("Check");
    check->checkBox()->setChecked(true);
    check->setBounds(10, 74, 150, 20);
    controls::TextBox *text=canvas->newChild<controls::TextBox>();
    text->setText("text");
    text->setBounds(10, 104, 150, 24);

    renderer.clearTarget(Color(0, 0, 0, 255));
    canvas->updateCanvas();
    canvas->renderCanvas();

    std::vector<uint8_t> pixels;
    renderer.readPixels(Width, Height, pixels);
    delete canvas;

    if(renderer.deviceError())
        return fail("the device reported an error");
    if(pixels.size() != size_t(Width)*Height*4)
        return fail("readPixels returned the wrong size");

    std::set<uint32_t> colors;
    for(size_t i=0; i < pixels.size(); i+=4)
    {
        uint32_t color;
        std::memcpy(&color, &pixels[i], 4);
        colors.insert(color);
    }
    if(colors.size() < 2)
        return fail("the frame is one flat colour: nothing was drawn");

    std::printf("vulkan_frame: %dx%d frame, %zu colours\n", Width, Height, colors.size());
    return EXIT_SUCCESS;
}

}//namespace

int main()
{
    renderer::VulkanDevice device;
    renderer::VulkanDeviceOptions options;
    std::string error;

    options.applicationName="vulkan_frame";
    options.validation=true;
    if(!renderer::createVulkanDevice(device, options, error))
    {
        std::fprintf(stderr, "vulkan_frame: %s\n", error.c_str());
        return EXIT_FAILURE;
    }

    int result=EXIT_FAILURE;
    {
        renderer::VulkanTarget target(device);

        if(target.create(Width, Height))
        {
            platform::CurrentPath paths("resources");
            renderer::Vulkan renderer(paths, Rect(Point(0, 0), Point(Width, Height)), device, target);

            renderer.init();
            result=drawFrame(renderer);
        }
        else
            result=fail("could not make the Vulkan target");
    }
    renderer::destroyVulkanDevice(device);
    return result;
}
