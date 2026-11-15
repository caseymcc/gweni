/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  renderer::Vulkan's clearTarget() submits without waiting for the queue: in a
 *  loop of clearTarget(), updateCanvas() and renderCanvas() over a settled canvas
 *  the renderer calls vkQueueWaitIdle zero times (queueWaitIdleCount()), and a
 *  readPixels() after the loop reads the last clear and what the last frame drew
 *  over it. Its command buffers are reused once their fences have signalled, so
 *  their count stays at the clears on the device at once
 *  (clearCommandBufferCount()): a few in that loop, and none more after a wait.
 *  Clears with no frame between them, and a renderer destroyed right after
 *  clears, leave the validation layer of a Vulkan run (unittests_vulkan) with
 *  nothing to report: no command buffer or fence is reset or freed in flight.
 *  On GL there is no such renderer and the cases say so.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#ifdef GWENI_HAS_VULKAN
#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/label.h>
#include <gweni/platforms/commonPlatform.h>
#include <gweni/platforms/renderers/vulkan.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <memory>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Point;
using gweni::Rect;
using gweni::renderer::Vulkan;

//! The shared renderer as a renderer::Vulkan, or null in a GL run.
Vulkan *vulkanRenderer()
{
    return dynamic_cast<Vulkan *>(&gweni::test::gpuRenderer());
}

//! A pixel of a top-down RGBA8 read-back.
Color pixelAt(const std::vector<uint8_t> &pixels, int x, int y)
{
    const size_t i=(static_cast<size_t>(y)*gweni::test::viewSize().x+x)*4;

    return Color(pixels[i], pixels[i+1], pixels[i+2], pixels[i+3]);
}

bool same(const Color &a, const Color &b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

}//namespace

TEST_CASE("Vulkan clear: a loop of clearTarget and frames over a settled canvas waits for the queue zero times")
{
    Vulkan *renderer=vulkanRenderer();

    if(!renderer)
    {
        MESSAGE("skipped: the renderer is not renderer::Vulkan");
        return;
    }

    const Point viewSize=gweni::test::viewSize();
    gweni::skin::textured::TexturedSkin *skin=new gweni::skin::textured::TexturedSkin(renderer);

    REQUIRE(skin->init("DefaultSkin.json"));
    skin->setDefaultFont("OpenSans.ttf", 11);

    gweni::controls::Canvas *canvas=new gweni::controls::Canvas(skin);

    canvas->setSize(viewSize.x, viewSize.y);

    gweni::controls::Button *button=canvas->newChild<gweni::controls::Button>();

    button->setText("Button");
    button->setPos(20, 20);
    button->setSize(120, 30);

    gweni::controls::Label *label=canvas->newChild<gweni::controls::Label>();

    label->setText("A label");
    label->setPos(20, 70);

    // a clear colour per frame, so a read-back tells the last clear from an earlier one
    auto clearColour=[](int frame)
    {
        return Color((uint8_t)(frame*37), (uint8_t)(255-frame*11), (uint8_t)(frame*5), 255);
    };
    auto frame=[&](int i)
    {
        renderer->clearTarget(clearColour(i));
        canvas->updateCanvas();
        canvas->renderCanvas();
    };

    // settle: the fonts, the slots and their buffers are made in these frames
    for(int i=0; i<5; ++i)
        frame(i);

    const uint64_t waits=renderer->queueWaitIdleCount();
    const int frames=60;

    for(int i=0; i<frames; ++i)
        frame(5+i);

    CHECK(renderer->queueWaitIdleCount() == waits);
    // the clears on the device at once: never every one of the loop's
    INFO("clear command buffers: ", renderer->clearCommandBufferCount());
    CHECK(renderer->clearCommandBufferCount() <= Vulkan::FramesInFlight+2);

    // the last clear and the last frame's draws over it
    std::vector<uint8_t> pixels;

    renderer->readPixels(viewSize.x, viewSize.y, pixels);

    const Color last=clearColour(5+frames-1);
    const Color corner=pixelAt(pixels, viewSize.x-2, viewSize.y-2);

    INFO("corner (", int(corner.r), ",", int(corner.g), ",", int(corner.b), ",", int(corner.a),
        "), the last clear (", int(last.r), ",", int(last.g), ",", int(last.b), ",", int(last.a), ")");
    CHECK(same(corner, last));

    // the button's fill covers its bounds but for its rounded corners
    const Point origin=button->localPosToCanvas(Point(0, 0));
    const int w=button->getWidth();
    const int h=button->getHeight();
    int drawn=0;

    for(int y=origin.y; y<origin.y+h; ++y)
        for(int x=origin.x; x<origin.x+w; ++x)
            drawn+=!same(pixelAt(pixels, x, y), last);
    INFO(drawn, " of the button's ", w*h, " pixels differ from the clear");
    CHECK(w*h > 0);
    CHECK(drawn > w*h*9/10);

    // readPixels() waited, so every clear is done: the next ones reuse them
    const size_t made=renderer->clearCommandBufferCount();

    for(int i=0; i<10; ++i)
        frame(i);
    CHECK(renderer->clearCommandBufferCount() <= made);

    delete canvas;
    delete skin;
}

TEST_CASE("Vulkan clear: clears with no frame between them, then the renderer destroyed")
{
    if(!vulkanRenderer())
    {
        MESSAGE("skipped: the renderer is not renderer::Vulkan");
        return;
    }

    gweni::platform::CurrentPath paths("resources");
    const Point viewSize=gweni::test::viewSize();
    std::unique_ptr<gweni::renderer::SlotRenderer> own(
        gweni::test::newRenderer(paths, Rect(Point(0, 0), viewSize)));
    Vulkan *renderer=dynamic_cast<Vulkan *>(own.get());

    const bool isVulkan=(renderer != nullptr);

    REQUIRE(isVulkan);
    renderer->init();
    REQUIRE_FALSE(renderer->deviceError());

    const uint64_t waits=renderer->queueWaitIdleCount();

    // ten clears in a row, every one still on the device for all the renderer knows
    for(int i=0; i<10; ++i)
        renderer->clearTarget(Color((uint8_t)(i*20), 40, 60, 255));
    CHECK(renderer->queueWaitIdleCount() == waits);
    CHECK(renderer->clearCommandBufferCount() >= 1);
    CHECK(renderer->clearCommandBufferCount() <= 10);

    std::vector<uint8_t> pixels;

    renderer->readPixels(viewSize.x, viewSize.y, pixels);
    CHECK(same(pixelAt(pixels, 5, 5), Color(180, 40, 60, 255)));

    // and ten more, then destroyed with them submitted and not waited for
    for(int i=0; i<10; ++i)
        renderer->clearTarget(Color(10, (uint8_t)(i*20), 60, 255));
    CHECK(renderer->queueWaitIdleCount() == waits+1);
    own.reset();

    // the shared renderer still draws into the target the destroyed one cleared
    gweni::test::clearView(Color(1, 2, 3, 255));
    gweni::test::readView(pixels);
    CHECK(same(pixelAt(pixels, 5, 5), Color(1, 2, 3, 255)));
}

#else

TEST_CASE("Vulkan clear: a loop of clearTarget and frames over a settled canvas waits for the queue zero times")
{
    MESSAGE("skipped: built without Vulkan");
}

#endif
