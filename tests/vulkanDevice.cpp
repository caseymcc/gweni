/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  What an app that makes its own Vulkan device and composites gweni's target
 *  relies on, in a Vulkan run (unittests_vulkan, validated; on GL the cases say
 *  they are skipped):
 *
 *  - The VulkanTarget's colour image is sampled: its view is written into a
 *    combined-image-sampler descriptor (vkUpdateDescriptorSets), and taken to
 *    SHADER_READ_ONLY_OPTIMAL and back to COLOR_ATTACHMENT_OPTIMAL with the
 *    barriers the app makes after gweni's submit and before its next frame;
 *    without VK_IMAGE_USAGE_SAMPLED_BIT the validation layer aborts both. The
 *    target still reads back as it was drawn after the round trip.
 *  - Vulkan::init() refuses a device for which volk is not loaded: one stderr
 *    line naming the instance, or with volk loaded for the instance but another
 *    device, the device; deviceError() is true, and a frame, a clear and a
 *    read-back on it make no Vulkan call (volk's pointers are the other
 *    device's) and leave the target as it was. A second device made with
 *    createVulkanDevice loads volk for itself; volk is loaded for the suite's
 *    device again before the case ends, or every case after it would break.
 *    A VulkanTarget made while volk is another device's is refused the same way
 *    (create() false, one line, no image), before any Vulkan call.
 *  - An instance made at apiVersion 1.2 or 1.0 (vk-bootstrap's default) on
 *    lavapipe, a 1.3 driver: the loader gives volk no 1.3 entry point, and at 1.0
 *    the instance's vkGetPhysicalDeviceFeatures2 dispatches through null, so
 *    VulkanTarget::create() and Vulkan::init() each refuse it with one line,
 *    "the device is not Vulkan 1.3", before querying the physical device; a
 *    frame, a clear and a read-back on the refused renderer crash nothing.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#ifdef GWENI_HAS_VULKAN
#include <gweni/platforms/commonPlatform.h>
#include <gweni/platforms/renderers/vulkan.h>

#include <cstdio>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Point;
using gweni::Rect;
using gweni::renderer::Vulkan;
using gweni::renderer::VulkanDevice;

Vulkan *vulkanRenderer()
{
    return dynamic_cast<Vulkan *>(&gweni::test::gpuRenderer());
}

//! Loads volk for \a device's instance and device, as an app does.
void loadVolkFor(const VulkanDevice &device)
{
    volkLoadInstanceOnly(device.instance);
    volkLoadDevice(device.device);
}

//! Records one command buffer, submits it and waits, on the suite's device.
template<typename Record>
void submitAndWait(const VulkanDevice &device, Record &&record)
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags=VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex=device.queueFamily;
    VkCommandPool pool=VK_NULL_HANDLE;
    REQUIRE(vkCreateCommandPool(device.device, &poolInfo, nullptr, &pool) == VK_SUCCESS);

    VkCommandBufferAllocateInfo allocate{};
    allocate.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate.commandPool=pool;
    allocate.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate.commandBufferCount=1;
    VkCommandBuffer commands=VK_NULL_HANDLE;
    REQUIRE(vkAllocateCommandBuffers(device.device, &allocate, &commands) == VK_SUCCESS);

    VkCommandBufferBeginInfo begin{};
    begin.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commands, &begin);
    record(commands);
    vkEndCommandBuffer(commands);

    VkSubmitInfo submit{};
    submit.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount=1;
    submit.pCommandBuffers=&commands;
    CHECK(vkQueueSubmit(device.queue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS);
    CHECK(vkQueueWaitIdle(device.queue) == VK_SUCCESS);
    vkDestroyCommandPool(device.device, pool, nullptr);
}

void colorBarrier(VkCommandBuffer commands, VkImage image, VkImageLayout from, VkImageLayout to,
    VkPipelineStageFlags srcStage, VkAccessFlags srcAccess, VkPipelineStageFlags dstStage, VkAccessFlags dstAccess)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask=srcAccess;
    barrier.dstAccessMask=dstAccess;
    barrier.oldLayout=from;
    barrier.newLayout=to;
    barrier.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    barrier.image=image;
    barrier.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount=1;
    barrier.subresourceRange.layerCount=1;
    vkCmdPipelineBarrier(commands, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

//! Captures what is written to stderr (fd 2) while it lives.
struct StderrCapture
{
    StderrCapture()
    {
        std::fflush(stderr);
        file=std::tmpfile();
        saved=dup(2);
        dup2(fileno(file), 2);
    }

    //! Puts stderr back and returns what was written.
    std::string stop()
    {
        std::fflush(stderr);
        dup2(saved, 2);
        close(saved);

        std::string text;
        char buffer[512];

        std::rewind(file);
        for(size_t read; (read=std::fread(buffer, 1, sizeof(buffer), file)) > 0;)
            text.append(buffer, read);
        std::fclose(file);
        return text;
    }

    std::FILE *file=nullptr;
    int saved=-1;
};

size_t lineCount(const std::string &text)
{
    size_t lines=0;

    for(char c:text)
        lines+=(c == '\n');
    return lines;
}

//! A renderer of its own on `device`, init()'d, drawing a frame, a clear and a
//! read-back: what init() printed, and whether deviceError() was set.
struct Attempt
{
    std::string printed;
    bool error=false;
};

Attempt attemptOn(const VulkanDevice &device)
{
    gweni::platform::CurrentPath paths("resources");
    const Point viewSize=gweni::test::viewSize();
    Attempt attempt;
    StderrCapture capture;

    {
        Vulkan renderer(paths, Rect(Point(0, 0), viewSize), device,
            vulkanRenderer()->target());

        renderer.init();
        attempt.printed=capture.stop();
        attempt.error=renderer.deviceError();

        // nothing of this reaches the device: volk's pointers are another one's
        size_t id=0;
        std::vector<uint8_t> pixels;

        renderer.generatePrimitive(id);
        renderer.begin();
        renderer.setDrawColor(Color(255, 0, 0, 255));
        renderer.drawFilledRect(id, Rect(0, 0, viewSize.x, viewSize.y), 1);
        renderer.end();
        renderer.clearTarget(Color(0, 255, 0, 255));
        renderer.readPixels(viewSize.x, viewSize.y, pixels);
        renderer.releasePrimitive(id);
        renderer.begin();
        renderer.end();
        CHECK(renderer.deviceError());
    }
    return attempt;
}

}//namespace

TEST_CASE("Vulkan target: its colour view is sampled, into a combined-image-sampler descriptor and through SHADER_READ_ONLY_OPTIMAL")
{
    Vulkan *renderer=vulkanRenderer();
    const VulkanDevice *device=gweni::test::vulkanDevice();

    if(!renderer || !device)
    {
        MESSAGE("skipped: the renderer is not renderer::Vulkan");
        return;
    }

    const gweni::renderer::VulkanTarget &target=renderer->target();
    const Point viewSize=gweni::test::viewSize();

    gweni::test::clearView(Color(10, 20, 30, 255));

    VkDescriptorSetLayoutBinding binding{};
    binding.binding=0;
    binding.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount=1;
    binding.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount=1;
    layoutInfo.pBindings=&binding;
    VkDescriptorSetLayout setLayout=VK_NULL_HANDLE;
    REQUIRE(vkCreateDescriptorSetLayout(device->device, &layoutInfo, nullptr, &setLayout) == VK_SUCCESS);

    VkDescriptorPoolSize poolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 };
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets=1;
    poolInfo.poolSizeCount=1;
    poolInfo.pPoolSizes=&poolSize;
    VkDescriptorPool pool=VK_NULL_HANDLE;
    REQUIRE(vkCreateDescriptorPool(device->device, &poolInfo, nullptr, &pool) == VK_SUCCESS);

    VkDescriptorSetAllocateInfo allocate{};
    allocate.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate.descriptorPool=pool;
    allocate.descriptorSetCount=1;
    allocate.pSetLayouts=&setLayout;
    VkDescriptorSet set=VK_NULL_HANDLE;
    REQUIRE(vkAllocateDescriptorSets(device->device, &allocate, &set) == VK_SUCCESS);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter=VK_FILTER_NEAREST;
    samplerInfo.minFilter=VK_FILTER_NEAREST;
    samplerInfo.addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VkSampler sampler=VK_NULL_HANDLE;
    REQUIRE(vkCreateSampler(device->device, &samplerInfo, nullptr, &sampler) == VK_SUCCESS);

    // the validation layer checks the image's usage here (SAMPLED for a sampler)
    VkDescriptorImageInfo imageInfo{ sampler, target.colorView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkWriteDescriptorSet write{};
    write.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet=set;
    write.dstBinding=0;
    write.descriptorCount=1;
    write.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo=&imageInfo;
    vkUpdateDescriptorSets(device->device, 1, &write, 0, nullptr);

    // the app's barriers: after gweni's submit, to be sampled, and back before
    // gweni's next frame
    submitAndWait(*device, [&](VkCommandBuffer commands)
        {
            colorBarrier(commands, target.colorImage(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
            colorBarrier(commands, target.colorImage(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
        });

    vkDestroySampler(device->device, sampler, nullptr);
    vkDestroyDescriptorPool(device->device, pool, nullptr);
    vkDestroyDescriptorSetLayout(device->device, setLayout, nullptr);

    // the round trip keeps what was drawn
    std::vector<uint8_t> pixels;

    gweni::test::readView(pixels);
    REQUIRE(pixels.size() == (size_t)viewSize.x*viewSize.y*4);
    CHECK(pixels[0] == 10);
    CHECK(pixels[1] == 20);
    CHECK(pixels[2] == 30);
    CHECK(pixels[3] == 255);
}

TEST_CASE("Vulkan device check: init() refuses a device volk is not loaded for, with one line, and draws nothing")
{
    const VulkanDevice *suite=gweni::test::vulkanDevice();

    if(!vulkanRenderer() || !suite)
    {
        MESSAGE("skipped: the renderer is not renderer::Vulkan");
        return;
    }

    const Point viewSize=gweni::test::viewSize();

    gweni::test::clearView(Color(40, 50, 60, 255));

    // a second device: createVulkanDevice loads volk for it
    VulkanDevice second;
    gweni::renderer::VulkanDeviceOptions options;
    std::string error;

    options.applicationName="gweni_unittests second device";

    const bool made=gweni::renderer::createVulkanDevice(second, options, error);

    if(!made)
        loadVolkFor(*suite);
    REQUIRE_MESSAGE(made, error);
    CHECK(volkGetLoadedDevice() == second.device);

    // a target made on the suite's device now is refused, before any Vulkan call
    {
        gweni::renderer::VulkanTarget target(*suite);
        StderrCapture capture;
        const bool created=target.create(16, 16);
        const std::string printed=capture.stop();

        INFO("printed: ", printed);
        CHECK_FALSE(created);
        CHECK(target.colorImage() == VK_NULL_HANDLE);
        CHECK(lineCount(printed) == 1);
        CHECK(printed.find("gweni: VulkanTarget: cannot use the device: volk is not loaded for this VkInstance")
            != std::string::npos);
    }

    // volk is another instance's
    const Attempt instance=attemptOn(*suite);

    INFO("printed: ", instance.printed);
    CHECK(instance.error);
    CHECK(lineCount(instance.printed) == 1);
    CHECK(instance.printed.find("volk is not loaded for this VkInstance") != std::string::npos);

    // volk is this instance's and another device's
    volkLoadInstanceOnly(suite->instance);

    const Attempt device=attemptOn(*suite);

    INFO("printed: ", device.printed);
    CHECK(device.error);
    CHECK(lineCount(device.printed) == 1);
    CHECK(device.printed.find("volk is not loaded for this VkDevice") != std::string::npos);

    // the second device goes with volk loaded for it, then volk is the suite's again
    loadVolkFor(second);
    gweni::renderer::destroyVulkanDevice(second);
    loadVolkFor(*suite);
    CHECK(volkGetLoadedDevice() == suite->device);

    // neither drew, cleared or read anything: the target is as the suite left it
    std::vector<uint8_t> pixels;

    gweni::test::readView(pixels);
    REQUIRE(pixels.size() == (size_t)viewSize.x*viewSize.y*4);
    size_t changed=0;

    for(size_t i=0; i<pixels.size(); i+=4)
        changed+=!(pixels[i] == 40 && pixels[i+1] == 50 && pixels[i+2] == 60 && pixels[i+3] == 255);
    CHECK(changed == 0);

    // and a renderer on the suite's device, with volk its own again, is accepted
    // without a word
    gweni::platform::CurrentPath paths("resources");
    StderrCapture capture;
    std::unique_ptr<Vulkan> accepted(new Vulkan(paths, Rect(Point(0, 0), viewSize), *suite,
        vulkanRenderer()->target()));

    accepted->init();

    const std::string printed=capture.stop();

    INFO("printed: ", printed);
    CHECK(printed.empty());
    CHECK_FALSE(accepted->deviceError());
    accepted.reset();
}

TEST_CASE("Vulkan device check: an instance below 1.3 on a 1.3 driver is refused by the target and init(), with one line each")
{
    const VulkanDevice *suite=gweni::test::vulkanDevice();

    if(!vulkanRenderer() || !suite)
    {
        MESSAGE("skipped: the renderer is not renderer::Vulkan");
        return;
    }

    // vk-bootstrap's default instance is 1.0: the driver (lavapipe, 1.3) has every
    // feature, but the device's Vulkan is the instance's
    for(const uint32_t apiVersion:{ VK_API_VERSION_1_2, VK_API_VERSION_1_0 })
    {
        CAPTURE(VK_API_VERSION_MINOR(apiVersion));

        VkApplicationInfo application{};
        application.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;
        application.pApplicationName="gweni_unittests old instance";
        application.apiVersion=apiVersion;
        VkInstanceCreateInfo instanceInfo{};
        instanceInfo.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceInfo.pApplicationInfo=&application;
        VulkanDevice old;

        REQUIRE(vkCreateInstance(&instanceInfo, nullptr, &old.instance) == VK_SUCCESS);
        volkLoadInstanceOnly(old.instance);

        uint32_t count=0;
        vkEnumeratePhysicalDevices(old.instance, &count, nullptr);
        std::vector<VkPhysicalDevice> physical(count);
        vkEnumeratePhysicalDevices(old.instance, &count, physical.data());

        VkPhysicalDeviceProperties properties{};
        if(count > 0)
        {
            old.physicalDevice=physical[0];
            vkGetPhysicalDeviceProperties(old.physicalDevice, &properties);
        }

        const float priority=1.0f;
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex=0;
        queueInfo.queueCount=1;
        queueInfo.pQueuePriorities=&priority;
        VkDeviceCreateInfo deviceInfo{};
        deviceInfo.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.queueCreateInfoCount=1;
        deviceInfo.pQueueCreateInfos=&queueInfo;

        const bool made=count > 0
            && vkCreateDevice(old.physicalDevice, &deviceInfo, nullptr, &old.device) == VK_SUCCESS;

        if(!made)
        {
            vkDestroyInstance(old.instance, nullptr);
            loadVolkFor(*suite);
        }
        REQUIRE(made);
        volkLoadDevice(old.device);
        vkGetDeviceQueue(old.device, 0, 0, &old.queue);

        // the driver is 1.3, so this is the instance's version alone
        CHECK(properties.apiVersion >= VK_API_VERSION_1_3);

        std::string targetPrinted;
        bool targetCreated=true;
        {
            gweni::renderer::VulkanTarget target(old);
            StderrCapture capture;

            targetCreated=target.create(16, 16);
            targetPrinted=capture.stop();
        }

        const Attempt attempt=attemptOn(old);

        vkDestroyDevice(old.device, nullptr);
        vkDestroyInstance(old.instance, nullptr);
        loadVolkFor(*suite);

        INFO("target printed: ", targetPrinted);
        INFO("init printed: ", attempt.printed);
        CHECK_FALSE(targetCreated);
        CHECK(lineCount(targetPrinted) == 1);
        CHECK(targetPrinted.find("gweni: VulkanTarget: cannot use the device: the device is not Vulkan 1.3")
            != std::string::npos);
        CHECK(attempt.error);
        CHECK(lineCount(attempt.printed) == 1);
        CHECK(attempt.printed.find("gweni: renderer::Vulkan: cannot use the device: the device is not Vulkan 1.3")
            != std::string::npos);
    }
    CHECK(volkGetLoadedDevice() == suite->device);
}

#else

TEST_CASE("Vulkan target: its colour view is sampled, into a combined-image-sampler descriptor and through SHADER_READ_ONLY_OPTIMAL")
{
    MESSAGE("skipped: built without Vulkan");
}

#endif
