/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */
#include <gweni/platforms/renderers/vulkan.h>

#include "primitive.frag.h"
#include "primitive.vert.h"
#include "text.frag.h"
#include "text.vert.h"
#include "textSingle.frag.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace gweni
{
namespace renderer
{

namespace
{

const char *const ValidationLayer="VK_LAYER_KHRONOS_validation";

VKAPI_ATTR VkBool32 VKAPI_CALL onValidationMessage(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT /*types*/, const VkDebugUtilsMessengerCallbackDataEXT *data,
    void * /*user*/)
{
    const char *level=(severity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)?"error":"warning";

    std::fprintf(stderr, "gweni: Vulkan validation %s: %s\n", level,
        (data && data->pMessage)?data->pMessage:"(no message)");
    std::fflush(stderr);
    std::abort();
}

VkDebugUtilsMessengerCreateInfoEXT messengerInfo()
{
    VkDebugUtilsMessengerCreateInfoEXT info{};

    info.sType=VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback=onValidationMessage;
    return info;
}

bool hasLayer(const char *name)
{
    uint32_t count=0;

    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());
    for(const VkLayerProperties &layer:layers)
    {
        if(std::strcmp(layer.layerName, name) == 0)
            return true;
    }
    return false;
}

//! A device of the instance with Vulkan 1.3, dynamicRendering and a graphics queue.
bool findDevice(VkInstance instance, VkPhysicalDevice &found, uint32_t &queueFamily, bool &dualSource)
{
    uint32_t count=0;

    if(vkEnumeratePhysicalDevices(instance, &count, nullptr) != VK_SUCCESS)
        return false;

    std::vector<VkPhysicalDevice> devices(count);

    vkEnumeratePhysicalDevices(instance, &count, devices.data());
    for(VkPhysicalDevice device:devices)
    {
        VkPhysicalDeviceProperties properties;

        vkGetPhysicalDeviceProperties(device, &properties);
        if(properties.apiVersion < VK_API_VERSION_1_3)
            continue;

        VkPhysicalDeviceVulkan13Features features13{};
        features13.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        VkPhysicalDeviceFeatures2 features{};
        features.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features.pNext=&features13;
        vkGetPhysicalDeviceFeatures2(device, &features);
        // glslang compiles a Vulkan 1.3 shader's discard to a demote (a helper
        // invocation still computes derivatives), which the feature allows
        if(!features13.dynamicRendering || !features13.shaderDemoteToHelperInvocation)
            continue;

        uint32_t familyCount=0;

        vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(familyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families.data());
        for(uint32_t i=0; i<familyCount; ++i)
        {
            if(families[i].queueFlags&VK_QUEUE_GRAPHICS_BIT)
            {
                found=device;
                queueFamily=i;
                dualSource=(features.features.dualSrcBlend == VK_TRUE);
                return true;
            }
        }
    }
    return false;
}

VkImageAspectFlags aspectOf(VkFormat format)
{
    switch(format)
    {
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    case VK_FORMAT_D24_UNORM_S8_UINT:
        return VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT;
    default:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

//! A layout transition of a whole image (every mip level), with the stages and
//! accesses on each side.
void imageBarrier(VkCommandBuffer commands, VkImage image, VkImageAspectFlags aspect,
    VkImageLayout from, VkImageLayout to,
    VkPipelineStageFlags srcStages, VkAccessFlags srcAccess,
    VkPipelineStageFlags dstStages, VkAccessFlags dstAccess,
    uint32_t firstLevel=0, uint32_t levels=VK_REMAINING_MIP_LEVELS,
    uint32_t firstLayer=0, uint32_t layers=VK_REMAINING_ARRAY_LAYERS)
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
    barrier.subresourceRange.aspectMask=aspect;
    barrier.subresourceRange.baseMipLevel=firstLevel;
    barrier.subresourceRange.levelCount=levels;
    barrier.subresourceRange.baseArrayLayer=firstLayer;
    barrier.subresourceRange.layerCount=layers;
    vkCmdPipelineBarrier(commands, srcStages, dstStages, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

//! Everything a frame's draws into the target do to it, as a barrier's stages
const VkPipelineStageFlags TargetStages=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|
    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
const VkAccessFlags TargetAccess=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

//! What reads the slots on the device: the vertex input, and the shaders the
//! clip boxes and textures are sampled in
const VkPipelineStageFlags DrawReadStages=VK_PIPELINE_STAGE_VERTEX_INPUT_BIT|
    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
const VkAccessFlags DrawReadAccess=VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT|VK_ACCESS_INDEX_READ_BIT|
    VK_ACCESS_SHADER_READ_BIT;

//! The target before and after a command buffer that uses it: every earlier
//! submission's use of it (a frame of any renderer, a clear) is done first.
void targetBarrier(VkCommandBuffer commands, VkPipelineStageFlags dstStages, VkAccessFlags dstAccess)
{
    VkMemoryBarrier barrier{};

    barrier.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask=TargetAccess|VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask=dstAccess;
    vkCmdPipelineBarrier(commands, TargetStages|VK_PIPELINE_STAGE_TRANSFER_BIT, dstStages,
        0, 1, &barrier, 0, nullptr, 0, nullptr);
}

//! The view's projection: OpenGLCore's matrix (device px, y down, and z, a z
//! index, from 0 at depth 1.0 to kMaxZIndex at depth 0), into GL's clip space,
//! after the renderer's view transform when it has one; primitive.vert turns that
//! into Vulkan's.
glm::mat4 viewProjection(const SlotRenderer &renderer)
{
    const gweni::Rect &viewRect=renderer.viewRect();
    const glm::mat4 ortho=glm::ortho(
        static_cast<float>(viewRect.x),
        static_cast<float>(viewRect.x + viewRect.w),
        static_cast<float>(viewRect.y + viewRect.h),
        static_cast<float>(viewRect.y),
        -static_cast<float>(SlotRenderer::kMaxZIndex),
        0.0f
    );

    if(!renderer.hasViewTransform())
        return ortho;
    return ortho*glm::make_mat4(renderer.viewTransform());
}

}//namespace

//-------------------------------------------------------------------------------
// The device

bool createVulkanDevice(VulkanDevice &device, const VulkanDeviceOptions &options, std::string &error)
{
    device=VulkanDevice();

    if(volkInitialize() != VK_SUCCESS)
    {
        error="cannot load the Vulkan loader (libvulkan.so.1)";
        return false;
    }
    if(volkGetInstanceVersion() < VK_API_VERSION_1_3)
    {
        error="the Vulkan loader is older than 1.3";
        return false;
    }
    if(options.validation && !hasLayer(ValidationLayer))
    {
        const char *path=std::getenv("VK_LAYER_PATH");

        error=std::string(ValidationLayer)+" not found (VK_LAYER_PATH="+(path?path:"unset")+")";
        return false;
    }

    VkApplicationInfo app{};
    app.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName=options.applicationName.c_str();
    app.pEngineName="gweni";
    app.apiVersion=VK_API_VERSION_1_3;

    std::vector<const char *> instanceExtensions=options.instanceExtensions;
    // chained to the instance's creation, so it is validated too
    VkDebugUtilsMessengerCreateInfoEXT messenger=messengerInfo();

    if(options.validation)
        instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pNext=options.validation?&messenger:nullptr;
    instanceInfo.pApplicationInfo=&app;
    instanceInfo.enabledLayerCount=options.validation?1:0;
    instanceInfo.ppEnabledLayerNames=&ValidationLayer;
    instanceInfo.enabledExtensionCount=(uint32_t)instanceExtensions.size();
    instanceInfo.ppEnabledExtensionNames=instanceExtensions.data();

    VkResult result=vkCreateInstance(&instanceInfo, nullptr, &device.instance);

    if(result != VK_SUCCESS)
    {
        error="vkCreateInstance failed: VkResult "+std::to_string((int)result);
        device.instance=VK_NULL_HANDLE;
        return false;
    }
    volkLoadInstanceOnly(device.instance);

    if(options.validation)
    {
        result=vkCreateDebugUtilsMessengerEXT(device.instance, &messenger, nullptr, &device.messenger);
        if(result != VK_SUCCESS)
        {
            error="vkCreateDebugUtilsMessengerEXT failed: VkResult "+std::to_string((int)result);
            destroyVulkanDevice(device);
            return false;
        }
    }

    if(!findDevice(device.instance, device.physicalDevice, device.queueFamily, device.dualSourceBlend))
    {
        error="no Vulkan 1.3 device with dynamicRendering, shaderDemoteToHelperInvocation and a graphics queue";
        destroyVulkanDevice(device);
        return false;
    }

    const float priority=1.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex=device.queueFamily;
    queueInfo.queueCount=1;
    queueInfo.pQueuePriorities=&priority;

    VkPhysicalDeviceVulkan13Features enable13{};
    enable13.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    enable13.dynamicRendering=VK_TRUE;
    enable13.shaderDemoteToHelperInvocation=VK_TRUE;
    VkPhysicalDeviceFeatures2 enable{};
    enable.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    enable.pNext=&enable13;
    enable.features.dualSrcBlend=device.dualSourceBlend?VK_TRUE:VK_FALSE;

    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext=&enable;
    deviceInfo.queueCreateInfoCount=1;
    deviceInfo.pQueueCreateInfos=&queueInfo;
    deviceInfo.enabledExtensionCount=(uint32_t)options.deviceExtensions.size();
    deviceInfo.ppEnabledExtensionNames=options.deviceExtensions.data();

    result=vkCreateDevice(device.physicalDevice, &deviceInfo, nullptr, &device.device);
    if(result != VK_SUCCESS)
    {
        error="vkCreateDevice failed: VkResult "+std::to_string((int)result);
        device.device=VK_NULL_HANDLE;
        destroyVulkanDevice(device);
        return false;
    }
    volkLoadDevice(device.device);
    vkGetDeviceQueue(device.device, device.queueFamily, 0, &device.queue);
    return true;
}

void destroyVulkanDevice(VulkanDevice &device)
{
    if(device.device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(device.device);
        vkDestroyDevice(device.device, nullptr);
    }
    if(device.messenger != VK_NULL_HANDLE)
        vkDestroyDebugUtilsMessengerEXT(device.instance, device.messenger, nullptr);
    if(device.instance != VK_NULL_HANDLE)
        vkDestroyInstance(device.instance, nullptr);
    device=VulkanDevice();
}

//-------------------------------------------------------------------------------
// The target

VulkanTarget::VulkanTarget(const VulkanDevice &device)
    : m_device(device)
{}

VulkanTarget::~VulkanTarget()
{
    destroy();
}

void VulkanTarget::destroy()
{
    if(m_device.device == VK_NULL_HANDLE)
        return;
    if(m_color.image != VK_NULL_HANDLE || m_depth.image != VK_NULL_HANDLE)
        vkQueueWaitIdle(m_device.queue);
    for(Image *image:{ &m_color, &m_depth })
    {
        if(image->view != VK_NULL_HANDLE)
            vkDestroyImageView(m_device.device, image->view, nullptr);
        if(image->image != VK_NULL_HANDLE)
            vkDestroyImage(m_device.device, image->image, nullptr);
        if(image->memory != VK_NULL_HANDLE)
            vkFreeMemory(m_device.device, image->memory, nullptr);
        *image=Image();
    }
    m_width=0;
    m_height=0;
}

bool VulkanTarget::create(int width, int height, VkFormat depthFormat)
{
    destroy();

    // the depth format nearest GL's 24-bit depth buffer that the device renders to,
    // or the one asked for
    m_depthFormat=VK_FORMAT_UNDEFINED;
    for(VkFormat format:{ VK_FORMAT_X8_D24_UNORM_PACK32, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT })
    {
        VkFormatProperties properties;

        if(depthFormat != VK_FORMAT_UNDEFINED && format != depthFormat)
            continue;
        vkGetPhysicalDeviceFormatProperties(m_device.physicalDevice, format, &properties);
        if(properties.optimalTilingFeatures&VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            m_depthFormat=format;
            break;
        }
    }
    if(m_depthFormat == VK_FORMAT_UNDEFINED)
        return false;

    VkPhysicalDeviceMemoryProperties memoryProperties;

    vkGetPhysicalDeviceMemoryProperties(m_device.physicalDevice, &memoryProperties);

    auto make=[&](Image &image, VkFormat format, VkImageUsageFlags usage)
    {
        VkImageCreateInfo info{};
        info.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.imageType=VK_IMAGE_TYPE_2D;
        info.format=format;
        info.extent={ (uint32_t)width, (uint32_t)height, 1 };
        info.mipLevels=1;
        info.arrayLayers=1;
        info.samples=VK_SAMPLE_COUNT_1_BIT;
        info.tiling=VK_IMAGE_TILING_OPTIMAL;
        info.usage=usage;
        info.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        if(vkCreateImage(m_device.device, &info, nullptr, &image.image) != VK_SUCCESS)
            return false;

        VkMemoryRequirements requirements;
        vkGetImageMemoryRequirements(m_device.device, image.image, &requirements);

        VkMemoryAllocateInfo allocate{};
        allocate.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate.allocationSize=requirements.size;
        allocate.memoryTypeIndex=UINT32_MAX;
        for(uint32_t i=0; i<memoryProperties.memoryTypeCount; ++i)
        {
            if((requirements.memoryTypeBits&(1u << i)) &&
                (memoryProperties.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
            {
                allocate.memoryTypeIndex=i;
                break;
            }
        }
        if(allocate.memoryTypeIndex == UINT32_MAX ||
            vkAllocateMemory(m_device.device, &allocate, nullptr, &image.memory) != VK_SUCCESS)
            return false;
        vkBindImageMemory(m_device.device, image.image, image.memory, 0);

        VkImageViewCreateInfo view{};
        view.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.image=image.image;
        view.viewType=VK_IMAGE_VIEW_TYPE_2D;
        view.format=format;
        view.subresourceRange.aspectMask=aspectOf(format)&~VK_IMAGE_ASPECT_STENCIL_BIT;
        view.subresourceRange.levelCount=1;
        view.subresourceRange.layerCount=1;
        return vkCreateImageView(m_device.device, &view, nullptr, &image.view) == VK_SUCCESS;
    };

    if(!make(m_color, m_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|
            VK_IMAGE_USAGE_TRANSFER_DST_BIT) ||
        !make(m_depth, m_depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT))
    {
        destroy();
        return false;
    }
    m_width=width;
    m_height=height;

    // into the layouts every renderer keeps them in between frames, cleared
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags=VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex=m_device.queueFamily;

    VkCommandPool pool=VK_NULL_HANDLE;
    vkCreateCommandPool(m_device.device, &poolInfo, nullptr, &pool);

    VkCommandBufferAllocateInfo allocate{};
    allocate.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate.commandPool=pool;
    allocate.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate.commandBufferCount=1;

    VkCommandBuffer commands=VK_NULL_HANDLE;
    vkAllocateCommandBuffers(m_device.device, &allocate, &commands);

    VkCommandBufferBeginInfo begin{};
    begin.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commands, &begin);

    imageBarrier(commands, m_color.image, VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, TargetStages, TargetAccess);
    imageBarrier(commands, m_depth.image, aspectOf(m_depthFormat),
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, TargetStages, TargetAccess);

    VkRenderingAttachmentInfo color{};
    color.sType=VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color.imageView=m_color.view;
    color.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    VkRenderingAttachmentInfo depth=color;
    depth.imageView=m_depth.view;
    depth.imageLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth.clearValue.depthStencil={ 1.0f, 0 };

    VkRenderingInfo rendering{};
    rendering.sType=VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering.renderArea.extent={ (uint32_t)width, (uint32_t)height };
    rendering.layerCount=1;
    rendering.colorAttachmentCount=1;
    rendering.pColorAttachments=&color;
    rendering.pDepthAttachment=&depth;
    vkCmdBeginRendering(commands, &rendering);
    vkCmdEndRendering(commands);
    vkEndCommandBuffer(commands);

    VkSubmitInfo submit{};
    submit.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount=1;
    submit.pCommandBuffers=&commands;
    vkQueueSubmit(m_device.queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_device.queue);
    vkDestroyCommandPool(m_device.device, pool, nullptr);
    return true;
}

//-------------------------------------------------------------------------------
// The renderer

Vulkan::Vulkan(ResourcePaths &paths, const Rect &viewRect, const VulkanDevice &device, VulkanTarget &target)
    : SlotRenderer(paths, viewRect)
    , m_device(device)
    , m_target(&target)
{}

Vulkan::~Vulkan()
{
    if(m_commandPool == VK_NULL_HANDLE)
    {
        // never init()'d: only the CPU side to free
        releaseResources();
        return;
    }

    // the last frames' commands may still use what follows
    vkQueueWaitIdle(m_device.queue);
    m_recording=false;

    // each font's atlas, then each image, through the hooks
    releaseResources();
    for(Frame &frame:m_frames)
    {
        for(Texture &retired:frame.retired)
            destroyTexture(retired);
        frame.retired.clear();
    }

    for(TextureHandle handle:{ m_clipTexture, m_textClipTexture, m_blackTexture })
        destroyHandle(handle);
    for(auto &entry:m_textures)
        destroyTexture(entry.second);
    m_textures.clear();

    for(Buffer *buffer:{ &m_quadIndexBuffer, &m_textQuadIndexBuffer, &m_vertexBuffer, &m_textVertexBuffer })
        destroyBuffer(*buffer);
    for(Frame &frame:m_frames)
    {
        for(Buffer &staging:frame.staging)
            destroyBuffer(staging);
        frame.staging.clear();
        if(frame.fence != VK_NULL_HANDLE)
            vkDestroyFence(device(), frame.fence, nullptr);
        for(VkDescriptorPool pool:frame.textPools)
            vkDestroyDescriptorPool(device(), pool, nullptr);
        frame.textPools.clear();
    }

    destroyPipelines();
    vkDestroySampler(device(), m_imageSampler, nullptr);
    vkDestroySampler(device(), m_atlasSampler, nullptr);
    vkDestroySampler(device(), m_clipSampler, nullptr);
    vkDestroyDescriptorPool(device(), m_descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device(), m_primitiveSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device(), m_textSetLayout, nullptr);
    // frees the frames' command buffers
    vkDestroyCommandPool(device(), m_commandPool, nullptr);
}

bool Vulkan::check(VkResult result, const char *what)
{
    if(result >= 0)
        return true;
    std::fprintf(stderr, "gweni: renderer::Vulkan: %s failed: VkResult %d\n", what, (int)result);
    m_error=true;
    return false;
}

bool Vulkan::deviceError()
{
    const bool error=m_error;

    m_error=false;
    return error;
}

const char *Vulkan::lcdTextUnavailable() const
{
    if(m_device.dualSourceBlend)
        return nullptr;
    return "LCD text needs dual-source blending (the dualSrcBlend feature), which this device does not have";
}

uint32_t Vulkan::memoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) const
{
    for(uint32_t i=0; i<m_memoryProperties.memoryTypeCount; ++i)
    {
        if((typeBits&(1u << i)) && (m_memoryProperties.memoryTypes[i].propertyFlags&properties) == properties)
            return i;
    }
    return UINT32_MAX;
}

bool Vulkan::createBuffer(Buffer &buffer, VkDeviceSize size, VkBufferUsageFlags usage, bool hostVisible)
{
    buffer=Buffer();

    VkBufferCreateInfo info{};
    info.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size=std::max<VkDeviceSize>(size, 4);
    info.usage=usage;
    info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    if(!check(vkCreateBuffer(device(), &info, nullptr, &buffer.buffer), "vkCreateBuffer"))
        return false;

    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(device(), buffer.buffer, &requirements);

    VkMemoryAllocateInfo allocate{};
    allocate.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate.allocationSize=requirements.size;
    allocate.memoryTypeIndex=memoryType(requirements.memoryTypeBits, hostVisible?
        (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT):
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if(allocate.memoryTypeIndex == UINT32_MAX ||
        !check(vkAllocateMemory(device(), &allocate, nullptr, &buffer.memory), "vkAllocateMemory"))
    {
        destroyBuffer(buffer);
        return false;
    }
    vkBindBufferMemory(device(), buffer.buffer, buffer.memory, 0);
    if(hostVisible && !check(vkMapMemory(device(), buffer.memory, 0, VK_WHOLE_SIZE, 0, &buffer.mapped), "vkMapMemory"))
    {
        destroyBuffer(buffer);
        return false;
    }
    buffer.size=info.size;
    return true;
}

void Vulkan::destroyBuffer(Buffer &buffer)
{
    if(buffer.buffer != VK_NULL_HANDLE)
        vkDestroyBuffer(device(), buffer.buffer, nullptr);
    if(buffer.memory != VK_NULL_HANDLE)
        vkFreeMemory(device(), buffer.memory, nullptr);
    buffer=Buffer();
}

bool Vulkan::createTexture(Texture &texture, uint32_t width, uint32_t height, VkFormat format,
    uint32_t mipLevels, VkImageUsageFlags usage, uint32_t layers)
{
    texture.format=format;
    texture.width=width;
    texture.height=height;
    texture.mipLevels=mipLevels;
    texture.layers=std::max(layers, 1u);
    texture.layout=VK_IMAGE_LAYOUT_UNDEFINED;

    VkImageCreateInfo info{};
    info.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType=VK_IMAGE_TYPE_2D;
    info.format=format;
    info.extent={ std::max(width, 1u), std::max(height, 1u), 1 };
    info.mipLevels=mipLevels;
    info.arrayLayers=texture.layers;
    info.samples=VK_SAMPLE_COUNT_1_BIT;
    info.tiling=VK_IMAGE_TILING_OPTIMAL;
    info.usage=usage|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    info.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    if(!check(vkCreateImage(device(), &info, nullptr, &texture.image), "vkCreateImage"))
        return false;

    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(device(), texture.image, &requirements);

    VkMemoryAllocateInfo allocate{};
    allocate.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate.allocationSize=requirements.size;
    allocate.memoryTypeIndex=memoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if(allocate.memoryTypeIndex == UINT32_MAX ||
        !check(vkAllocateMemory(device(), &allocate, nullptr, &texture.memory), "vkAllocateMemory"))
    {
        destroyTexture(texture);
        return false;
    }
    vkBindImageMemory(device(), texture.image, texture.memory, 0);

    VkImageViewCreateInfo view{};
    view.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.image=texture.image;
    view.viewType=(layers > 0)?VK_IMAGE_VIEW_TYPE_2D_ARRAY:VK_IMAGE_VIEW_TYPE_2D;
    view.format=format;
    view.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    view.subresourceRange.levelCount=mipLevels;
    view.subresourceRange.layerCount=texture.layers;
    if(!check(vkCreateImageView(device(), &view, nullptr, &texture.view), "vkCreateImageView"))
    {
        destroyTexture(texture);
        return false;
    }
    return true;
}

void Vulkan::destroyTexture(Texture &texture)
{
    if(texture.view != VK_NULL_HANDLE)
        vkDestroyImageView(device(), texture.view, nullptr);
    if(texture.image != VK_NULL_HANDLE)
        vkDestroyImage(device(), texture.image, nullptr);
    if(texture.memory != VK_NULL_HANDLE)
        vkFreeMemory(device(), texture.memory, nullptr);
    texture=Texture();
}

VkShaderModule Vulkan::createShader(const uint32_t *code, size_t bytes)
{
    VkShaderModuleCreateInfo info{};

    info.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize=bytes;
    info.pCode=code;

    VkShaderModule module=VK_NULL_HANDLE;
    check(vkCreateShaderModule(device(), &info, nullptr, &module), "vkCreateShaderModule");
    return module;
}

template<typename Record>
void Vulkan::immediate(Record &&record)
{
    VkCommandBufferAllocateInfo allocate{};
    allocate.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate.commandPool=m_commandPool;
    allocate.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate.commandBufferCount=1;

    VkCommandBuffer commands=VK_NULL_HANDLE;
    if(!check(vkAllocateCommandBuffers(device(), &allocate, &commands), "vkAllocateCommandBuffers"))
        return;

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
    check(vkQueueSubmit(m_device.queue, 1, &submit, VK_NULL_HANDLE), "vkQueueSubmit");
    check(vkQueueWaitIdle(m_device.queue), "vkQueueWaitIdle");
    vkFreeCommandBuffers(device(), m_commandPool, 1, &commands);
}

Vulkan::Texture *Vulkan::texture(TextureHandle handle)
{
    auto it=m_textures.find(handle);

    return (it != m_textures.end())?&it->second:nullptr;
}

const Vulkan::Texture *Vulkan::texture(TextureHandle handle) const
{
    auto it=m_textures.find(handle);

    return (it != m_textures.end())?&it->second:nullptr;
}

TextureHandle Vulkan::addTexture(Texture &&texture)
{
    const TextureHandle handle=m_nextHandle++;

    m_textures.emplace(handle, std::move(texture));
    return handle;
}

void Vulkan::destroyHandle(TextureHandle handle)
{
    auto it=m_textures.find(handle);

    if(it == m_textures.end())
        return;
    destroyTexture(it->second);
    m_textures.erase(it);
    m_writtenTextures.erase(std::remove(m_writtenTextures.begin(), m_writtenTextures.end(), handle),
        m_writtenTextures.end());
}

void Vulkan::retireHandle(TextureHandle handle)
{
    auto it=m_textures.find(handle);

    if(it == m_textures.end())
        return;
    m_writtenTextures.erase(std::remove(m_writtenTextures.begin(), m_writtenTextures.end(), handle),
        m_writtenTextures.end());

    // The queue runs the frames in order, so once the newest one that may sample
    // it is done, every earlier one is: the frame being recorded, else the one
    // submitted last (its fence is the next waited of the two before its slot is
    // used again). With neither, nothing on the device uses it.
    Frame &newest=m_recording?m_frames[m_frameIndex]:
        m_frames[(m_frameIndex+FramesInFlight-1)%FramesInFlight];

    if(m_recording || newest.submitted)
        newest.retired.push_back(std::move(it->second));
    else
        destroyTexture(it->second);
    m_textures.erase(it);
}

VkDescriptorSet Vulkan::allocateTextSet()
{
    Frame &current=frame();
    const size_t pool=current.textSets/TextSetsPerPool;

    // a pool more when the frame's are full; they stay, reset as the frame begins
    if(pool >= current.textPools.size())
    {
        VkDescriptorPoolSize size{};
        size.type=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        size.descriptorCount=(uint32_t)(2*TextSetsPerPool);

        VkDescriptorPoolCreateInfo info{};
        info.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        info.maxSets=(uint32_t)TextSetsPerPool;
        info.poolSizeCount=1;
        info.pPoolSizes=&size;

        VkDescriptorPool made=VK_NULL_HANDLE;

        if(!check(vkCreateDescriptorPool(device(), &info, nullptr, &made), "vkCreateDescriptorPool"))
            return VK_NULL_HANDLE;
        current.textPools.push_back(made);
    }

    VkDescriptorSetAllocateInfo allocate{};
    allocate.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate.descriptorPool=current.textPools[pool];
    allocate.descriptorSetCount=1;
    allocate.pSetLayouts=&m_textSetLayout;

    VkDescriptorSet set=VK_NULL_HANDLE;

    if(!check(vkAllocateDescriptorSets(device(), &allocate, &set), "vkAllocateDescriptorSets"))
        return VK_NULL_HANDLE;
    ++current.textSets;
    return set;
}

void Vulkan::init()
{
    // nothing of the mirror is on the buffers below yet, and the view is the clip region
    SlotRenderer::init();

    vkGetPhysicalDeviceProperties(m_device.physicalDevice, &m_properties);
    vkGetPhysicalDeviceMemoryProperties(m_device.physicalDevice, &m_memoryProperties);
    // GWENI_TEXT_AA=rgb|bgr falls back to grayscale here without dual-source blending
    applyLcdTextSupport();

    // x, y and z as three 16-bit ints where the device reads them as a vertex
    // attribute; otherwise four, the shader taking the first three
    VkFormatProperties position;
    vkGetPhysicalDeviceFormatProperties(m_device.physicalDevice, VK_FORMAT_R16G16B16_SINT, &position);
    m_positionFormat=(position.bufferFeatures&VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT)?
        VK_FORMAT_R16G16B16_SINT:VK_FORMAT_R16G16B16A16_SINT;

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex=m_device.queueFamily;
    check(vkCreateCommandPool(device(), &poolInfo, nullptr, &m_commandPool), "vkCreateCommandPool");

    // set 0: the clip boxes and the image pages
    VkDescriptorSetLayoutBinding bindings[2]{};
    bindings[0].binding=0;
    bindings[0].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount=1;
    bindings[0].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].binding=1;
    bindings[1].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount=(uint32_t)ImageClasses;
    bindings[1].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo setLayout{};
    setLayout.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setLayout.bindingCount=2;
    setLayout.pBindings=bindings;
    check(vkCreateDescriptorSetLayout(device(), &setLayout, nullptr, &m_primitiveSetLayout),
        "vkCreateDescriptorSetLayout");

    // the text draws' set 0: the atlas being drawn and the glyph slots' clip boxes
    bindings[1].descriptorCount=1;
    check(vkCreateDescriptorSetLayout(device(), &setLayout, nullptr, &m_textSetLayout),
        "vkCreateDescriptorSetLayout");

    VkDescriptorPoolSize poolSize{};
    poolSize.type=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount=(uint32_t)((1+ImageClasses)*FramesInFlight);

    VkDescriptorPoolCreateInfo descriptorPool{};
    descriptorPool.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPool.maxSets=(uint32_t)FramesInFlight;
    descriptorPool.poolSizeCount=1;
    descriptorPool.pPoolSizes=&poolSize;
    check(vkCreateDescriptorPool(device(), &descriptorPool, nullptr, &m_descriptorPool), "vkCreateDescriptorPool");

    for(Frame &frame:m_frames)
    {
        VkCommandBufferAllocateInfo allocate{};
        allocate.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate.commandPool=m_commandPool;
        allocate.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount=2;

        VkCommandBuffer commands[2];
        check(vkAllocateCommandBuffers(device(), &allocate, commands), "vkAllocateCommandBuffers");
        frame.transfer=commands[0];
        frame.draw=commands[1];

        VkFenceCreateInfo fence{};
        fence.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        check(vkCreateFence(device(), &fence, nullptr, &frame.fence), "vkCreateFence");
        frame.submitted=false;

        VkDescriptorSetAllocateInfo set{};
        set.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        set.descriptorPool=m_descriptorPool;
        set.descriptorSetCount=1;
        set.pSetLayouts=&m_primitiveSetLayout;
        check(vkAllocateDescriptorSets(device(), &set, &frame.primitiveSet), "vkAllocateDescriptorSets");
    }

    // the images as OpenGLCore's: every level, nearest, clamped (the shader reads
    // texel centres and filters them itself, imageTexel); the atlases linear, the
    // clip boxes (integer texels) nearest
    VkSamplerCreateInfo sampler{};
    sampler.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.magFilter=VK_FILTER_NEAREST;
    sampler.minFilter=VK_FILTER_NEAREST;
    sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler.addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.maxLod=VK_LOD_CLAMP_NONE;
    check(vkCreateSampler(device(), &sampler, nullptr, &m_imageSampler), "vkCreateSampler");
    sampler.magFilter=VK_FILTER_LINEAR;
    sampler.minFilter=VK_FILTER_LINEAR;
    sampler.maxLod=0.0f;
    check(vkCreateSampler(device(), &sampler, nullptr, &m_atlasSampler), "vkCreateSampler");
    sampler.magFilter=VK_FILTER_NEAREST;
    sampler.minFilter=VK_FILTER_NEAREST;
    check(vkCreateSampler(device(), &sampler, nullptr, &m_clipSampler), "vkCreateSampler");

    // Every chunk draws its slots from vertex 0 of its own vertex buffer binding, so
    // one pattern of kSlotsPerChunk quads per path serves every chunk and never
    // changes: the primitives' zigzag, and freetype-gl's glyph order (OpenGLCore::init).
    {
        static const uint16_t primitiveQuad[6]={ 0, 1, 2, 1, 3, 2 };
        static const uint16_t glyphQuad[6]={ 0, 1, 2, 0, 2, 3 };
        std::vector<uint16_t> quads(kSlotsPerChunk*6);

        for(const auto &pattern:{ std::make_pair(&m_quadIndexBuffer, primitiveQuad),
            std::make_pair(&m_textQuadIndexBuffer, glyphQuad) })
        {
            for(size_t slot=0; slot<kSlotsPerChunk; ++slot)
            {
                const uint16_t v=static_cast<uint16_t>(slot*4);

                for(size_t j=0; j<6; ++j)
                    quads[slot*6+j]=v+pattern.second[j];
            }

            const VkDeviceSize bytes=quads.size()*sizeof(uint16_t);
            Buffer staging;

            createBuffer(*pattern.first, bytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT, false);
            if(!createBuffer(staging, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true))
                continue;
            memcpy(staging.mapped, quads.data(), bytes);
            immediate([&](VkCommandBuffer commands)
            {
                VkBufferCopy copy{ 0, 0, bytes };
                VkMemoryBarrier barrier{};

                vkCmdCopyBuffer(commands, staging.buffer, pattern.first->buffer, 1, &copy);
                barrier.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask=VK_ACCESS_INDEX_READ_BIT;
                vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                    0, 1, &barrier, 0, nullptr, 0, nullptr);
            });
            destroyBuffer(staging);
        }
    }

    // what a class with no page binds: GL's texture 0, (0, 0, 0, 1)
    {
        Texture black;

        if(createTexture(black, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, 1, 0, 1))
        {
            Buffer staging;

            if(createBuffer(staging, 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true))
            {
                const uint8_t texel[4]={ 0, 0, 0, 255 };

                memcpy(staging.mapped, texel, 4);
                immediate([&](VkCommandBuffer commands)
                {
                    VkBufferImageCopy copy{};
                    copy.imageSubresource={ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
                    copy.imageExtent={ 1, 1, 1 };
                    imageBarrier(commands, black.image, VK_IMAGE_ASPECT_COLOR_BIT,
                        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_ACCESS_TRANSFER_WRITE_BIT);
                    vkCmdCopyBufferToImage(commands, staging.buffer, black.image,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
                    imageBarrier(commands, black.image, VK_IMAGE_ASPECT_COLOR_BIT,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
                });
                destroyBuffer(staging);
            }
            black.layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            m_blackTexture=addTexture(std::move(black));
        }
    }

    createPipelines();

    m_push=PrimitivePush();
    setViewRect(viewRect());
}

void Vulkan::setViewRect(const gweni::Rect &viewRect)
{
    SlotRenderer::setViewRect(viewRect);

    viewTransformChanged();
}

void Vulkan::viewTransformChanged()
{
    const glm::mat4 projection=viewProjection(*this);

    memcpy(m_push.projection, glm::value_ptr(projection), sizeof(m_push.projection));
}

void Vulkan::setTarget(VulkanTarget &target)
{
    if(m_recording)
        submit();
    m_target=&target;
    if(m_commandPool != VK_NULL_HANDLE &&
        (target.colorFormat() != m_pipelineColorFormat || target.depthFormat() != m_pipelineDepthFormat))
    {
        vkQueueWaitIdle(m_device.queue);
        destroyPipelines();
        createPipelines();
    }
}

bool Vulkan::createPipelines()
{
    // text.frag's SDF_SPREAD
    static_assert(kSdfSpread == 6, "Vulkan/textFragment.glsl hard-codes SDF_SPREAD 6.0");

    m_pipelineColorFormat=m_target->colorFormat();
    m_pipelineDepthFormat=m_target->depthFormat();

    VkPushConstantRange push{};
    push.stageFlags=VK_SHADER_STAGE_VERTEX_BIT;
    push.size=sizeof(PrimitivePush);

    VkPipelineLayoutCreateInfo layout{};
    layout.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout.setLayoutCount=1;
    layout.pSetLayouts=&m_primitiveSetLayout;
    layout.pushConstantRangeCount=1;
    layout.pPushConstantRanges=&push;
    if(!check(vkCreatePipelineLayout(device(), &layout, nullptr, &m_primitiveLayout), "vkCreatePipelineLayout"))
        return false;

    // the text draws read their push constants in both stages
    push.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;
    push.size=sizeof(TextPush);
    layout.pSetLayouts=&m_textSetLayout;
    if(!check(vkCreatePipelineLayout(device(), &layout, nullptr, &m_textLayout), "vkCreatePipelineLayout"))
        return false;

    VkPipelineInputAssemblyStateCreateInfo assembly{};
    assembly.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewport{};
    viewport.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount=1;
    viewport.scissorCount=1;

    // the y flip in the projection turns every triangle over, and nothing is culled
    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode=VK_POLYGON_MODE_FILL;
    raster.cullMode=VK_CULL_MODE_NONE;
    raster.lineWidth=1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;

    // GL_LESS, writing depth (the primitive shader's gl_FragDepth, the text's own)
    VkPipelineDepthStencilStateCreateInfo depth{};
    depth.sType=VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth.depthTestEnable=VK_TRUE;
    depth.depthWriteEnable=VK_TRUE;
    depth.depthCompareOp=VK_COMPARE_OP_LESS;

    // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA), colour and alpha alike
    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.blendEnable=VK_TRUE;
    blendAttachment.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttachment.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachment.colorBlendOp=VK_BLEND_OP_ADD;
    blendAttachment.srcAlphaBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttachment.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachment.alphaBlendOp=VK_BLEND_OP_ADD;
    blendAttachment.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|
        VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount=1;
    blend.pAttachments=&blendAttachment;

    const VkDynamicState dynamicStates[]={ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount=2;
    dynamic.pDynamicStates=dynamicStates;

    VkPipelineRenderingCreateInfo rendering{};
    rendering.sType=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering.colorAttachmentCount=1;
    rendering.pColorAttachmentFormats=&m_pipelineColorFormat;
    rendering.depthAttachmentFormat=m_pipelineDepthFormat;

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].pName="main";
    stages[1].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].pName="main";

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount=1;

    VkGraphicsPipelineCreateInfo info{};
    info.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.pNext=&rendering;
    info.stageCount=2;
    info.pStages=stages;
    info.pVertexInputState=&vertexInput;
    info.pInputAssemblyState=&assembly;
    info.pViewportState=&viewport;
    info.pRasterizationState=&raster;
    info.pMultisampleState=&multisample;
    info.pDepthStencilState=&depth;
    info.pColorBlendState=&blend;
    info.pDynamicState=&dynamic;

    bool made=true;

    // The primitive slots: the Vertex layout, attribute for attribute as
    // OpenGLCore's vertex arrays read it
    {
        VkVertexInputBindingDescription binding{};
        binding.binding=0;
        binding.stride=sizeof(Vertex);
        binding.inputRate=VK_VERTEX_INPUT_RATE_VERTEX;

        const VkVertexInputAttributeDescription attributes[]=
        {
            { 0, 0, VK_FORMAT_R16_UINT, (uint32_t)offsetof(Vertex, style) },
            { 1, 0, m_positionFormat, (uint32_t)offsetof(Vertex, x) },
            { 2, 0, VK_FORMAT_R16G16_UINT, (uint32_t)offsetof(Vertex, tx) },
            { 3, 0, VK_FORMAT_R16G16_SINT, (uint32_t)offsetof(Vertex, lx) },
            { 4, 0, VK_FORMAT_R16G16_UINT, (uint32_t)offsetof(Vertex, radius) },
            { 5, 0, VK_FORMAT_R16G16_UINT, (uint32_t)offsetof(Vertex, cx) },
            { 6, 0, VK_FORMAT_R16G16_UINT, (uint32_t)offsetof(Vertex, bx) },
            { 7, 0, VK_FORMAT_R16G16_UINT, (uint32_t)offsetof(Vertex, dx) },
        };

        vertexInput.pVertexBindingDescriptions=&binding;
        vertexInput.vertexAttributeDescriptionCount=(uint32_t)(sizeof(attributes)/sizeof(attributes[0]));
        vertexInput.pVertexAttributeDescriptions=attributes;

        VkShaderModule vert=createShader(primitive_vert, sizeof(primitive_vert));
        VkShaderModule frag=createShader(primitive_frag, sizeof(primitive_frag));

        stages[0].module=vert;
        stages[1].module=frag;
        info.layout=m_primitiveLayout;
        made=check(vkCreateGraphicsPipelines(device(), VK_NULL_HANDLE, 1, &info, nullptr,
            &m_primitivePipeline), "vkCreateGraphicsPipelines") && made;
        vkDestroyShaderModule(device(), vert, nullptr);
        vkDestroyShaderModule(device(), frag, nullptr);
    }

    // The glyph slots: freetype-gl's glyph_vertex_t, as OpenGLCore's text vertex
    // arrays read it (position, texture coordinates, colour, gamma)
    {
        VkVertexInputBindingDescription binding{};
        binding.binding=0;
        binding.stride=sizeof(ftgl::glyph_vertex_t);
        binding.inputRate=VK_VERTEX_INPUT_RATE_VERTEX;

        const VkVertexInputAttributeDescription attributes[]=
        {
            { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, (uint32_t)offsetof(ftgl::glyph_vertex_t, x) },
            { 1, 0, VK_FORMAT_R32G32_SFLOAT, (uint32_t)offsetof(ftgl::glyph_vertex_t, u) },
            { 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, (uint32_t)offsetof(ftgl::glyph_vertex_t, r) },
            { 3, 0, VK_FORMAT_R32_SFLOAT, (uint32_t)offsetof(ftgl::glyph_vertex_t, gamma) },
        };

        vertexInput.pVertexBindingDescriptions=&binding;
        vertexInput.vertexAttributeDescriptionCount=(uint32_t)(sizeof(attributes)/sizeof(attributes[0]));
        vertexInput.pVertexAttributeDescriptions=attributes;

        // the fragment shader with a second output wherever dual-source blending
        // is there, as OpenGLCore compiles its text program, so the two pipelines
        // run the same code
        VkShaderModule vert=createShader(text_vert, sizeof(text_vert));
        VkShaderModule frag=m_device.dualSourceBlend?createShader(text_frag, sizeof(text_frag)):
            createShader(textSingle_frag, sizeof(textSingle_frag));

        stages[0].module=vert;
        stages[1].module=frag;
        info.layout=m_textLayout;
        made=check(vkCreateGraphicsPipelines(device(), VK_NULL_HANDLE, 1, &info, nullptr,
            &m_textPipeline), "vkCreateGraphicsPipelines") && made;

        // an LCD atlas's runs: glBlendFunc(GL_SRC1_COLOR, GL_ONE_MINUS_SRC1_COLOR),
        // each channel by its own coverage, the alpha by the second output's
        if(m_device.dualSourceBlend)
        {
            blendAttachment.srcColorBlendFactor=VK_BLEND_FACTOR_SRC1_COLOR;
            blendAttachment.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
            blendAttachment.srcAlphaBlendFactor=VK_BLEND_FACTOR_SRC1_COLOR;
            blendAttachment.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
            made=check(vkCreateGraphicsPipelines(device(), VK_NULL_HANDLE, 1, &info, nullptr,
                &m_lcdTextPipeline), "vkCreateGraphicsPipelines") && made;
        }
        vkDestroyShaderModule(device(), vert, nullptr);
        vkDestroyShaderModule(device(), frag, nullptr);
    }
    return made;
}

void Vulkan::destroyPipelines()
{
    for(VkPipeline *pipeline:{ &m_primitivePipeline, &m_textPipeline, &m_lcdTextPipeline })
    {
        if(*pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device(), *pipeline, nullptr);
        *pipeline=VK_NULL_HANDLE;
    }
    for(VkPipelineLayout *layout:{ &m_primitiveLayout, &m_textLayout })
    {
        if(*layout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device(), *layout, nullptr);
        *layout=VK_NULL_HANDLE;
    }
}

//-------------------------------------------------------------------------------
// Recording a frame

Vulkan::Frame &Vulkan::frame()
{
    Frame &frame=m_frames[m_frameIndex];

    if(m_recording)
        return frame;

    // the frame that used these buffers last, FramesInFlight frames ago
    if(frame.submitted)
    {
        check(vkWaitForFences(device(), 1, &frame.fence, VK_TRUE, UINT64_MAX), "vkWaitForFences");
        check(vkResetFences(device(), 1, &frame.fence), "vkResetFences");
        frame.submitted=false;
    }
    // nothing samples what was freed while that frame was in flight any more
    for(Texture &retired:frame.retired)
        destroyTexture(retired);
    frame.retired.clear();
    for(VkDescriptorPool pool:frame.textPools)
        vkResetDescriptorPool(device(), pool, 0);
    frame.textSets=0;

    // what the last use spread over several staging blocks fits in one from now on
    if(frame.staging.size() > 1)
    {
        VkDeviceSize total=0;

        for(Buffer &staging:frame.staging)
        {
            total+=staging.size;
            destroyBuffer(staging);
        }
        frame.staging.clear();
        frame.staging.emplace_back();
        createBuffer(frame.staging.back(), total, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true);
    }
    frame.stagingBlock=0;
    frame.stagingUsed=0;

    vkResetCommandBuffer(frame.transfer, 0);
    vkResetCommandBuffer(frame.draw, 0);

    VkCommandBufferBeginInfo begin{};
    begin.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(frame.transfer, &begin);
    vkBeginCommandBuffer(frame.draw, &begin);

    // The uploads overwrite slots the frames before this one may still be drawing
    // from: their reads finish first. The draws then come after every earlier use
    // of the target (another renderer's frame, a clear).
    VkMemoryBarrier barrier{};
    barrier.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask=0;
    barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(frame.transfer, DrawReadStages|VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    targetBarrier(frame.draw, TargetStages, TargetAccess);

    m_recording=true;
    m_rendering=false;
    return frame;
}

std::pair<VkBuffer, VkDeviceSize> Vulkan::stage(const void *data, size_t bytes, size_t alignment)
{
    Frame &current=frame();

    VkDeviceSize offset=(current.stagingUsed+alignment-1)/alignment*alignment;

    if(current.stagingBlock >= current.staging.size() ||
        offset+bytes > current.staging[current.stagingBlock].size)
    {
        // the block in use is full, or there is none: the next one, or a new one of
        // at least twice the last
        if(current.stagingBlock < current.staging.size())
            ++current.stagingBlock;
        offset=0;
        if(current.stagingBlock >= current.staging.size() || bytes > current.staging[current.stagingBlock].size)
        {
            VkDeviceSize size=std::max<VkDeviceSize>(bytes, 64*1024);

            if(!current.staging.empty())
                size=std::max(size, current.staging.back().size*2);

            Buffer block;

            createBuffer(block, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true);
            current.staging.insert(current.staging.begin()+(std::ptrdiff_t)current.stagingBlock, block);
        }
    }

    Buffer &block=current.staging[current.stagingBlock];

    memcpy(static_cast<char *>(block.mapped)+offset, data, bytes);
    current.stagingUsed=offset+bytes;
    return { block.buffer, offset };
}

void Vulkan::beginTextureWrite(Texture &target)
{
    Frame &current=frame();

    if(target.layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        return;

    imageBarrier(current.transfer, target.image, VK_IMAGE_ASPECT_COLOR_BIT,
        target.layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    target.layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    for(auto &entry:m_textures)
    {
        if(&entry.second == &target)
        {
            m_writtenTextures.push_back(entry.first);
            break;
        }
    }
}

void Vulkan::beginRendering()
{
    Frame &current=frame();

    if(m_rendering)
        return;

    VkRenderingAttachmentInfo color{};
    color.sType=VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color.imageView=m_target->colorView();
    color.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;
    color.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    VkRenderingAttachmentInfo depth=color;
    depth.imageView=m_target->depthView();
    depth.imageLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkRenderingInfo rendering{};
    rendering.sType=VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering.renderArea.extent={ (uint32_t)m_target->width(), (uint32_t)m_target->height() };
    rendering.layerCount=1;
    rendering.colorAttachmentCount=1;
    rendering.pColorAttachments=&color;
    rendering.pDepthAttachment=&depth;
    vkCmdBeginRendering(current.draw, &rendering);

    // glViewport(view): the view's device px from the target's first row, the
    // bottom one in GL's terms
    const Rect &view=viewRect();
    VkViewport viewport{};
    viewport.x=(float)view.x;
    viewport.y=(float)view.y;
    viewport.width=(float)std::max(view.w, 1);
    viewport.height=(float)std::max(view.h, 1);
    viewport.minDepth=0.0f;
    viewport.maxDepth=1.0f;
    vkCmdSetViewport(current.draw, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent=rendering.renderArea.extent;
    vkCmdSetScissor(current.draw, 0, 1, &scissor);

    m_rendering=true;
}

void Vulkan::endRendering()
{
    if(!m_rendering)
        return;
    vkCmdEndRendering(m_frames[m_frameIndex].draw);
    m_rendering=false;
}

void Vulkan::submit()
{
    if(!m_recording)
        return;

    Frame &current=m_frames[m_frameIndex];

    endRendering();

    // what this frame's uploads wrote is sampled by its draws, recorded after them
    for(TextureHandle handle:m_writtenTextures)
    {
        Texture *written=texture(handle);

        if(!written || written->layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
            continue;
        imageBarrier(current.transfer, written->image, VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_TRANSFER_READ_BIT);
        written->layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    m_writtenTextures.clear();

    // the vertex and index data copied this frame, for its draws
    VkMemoryBarrier barrier{};
    barrier.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask=DrawReadAccess|VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(current.transfer, VK_PIPELINE_STAGE_TRANSFER_BIT,
        DrawReadStages|VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);

    vkEndCommandBuffer(current.transfer);
    vkEndCommandBuffer(current.draw);

    const VkCommandBuffer commands[2]={ current.transfer, current.draw };
    VkSubmitInfo info{};
    info.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.commandBufferCount=2;
    info.pCommandBuffers=commands;
    check(vkQueueSubmit(m_device.queue, 1, &info, current.fence), "vkQueueSubmit");
    current.submitted=true;

    m_recording=false;
    m_frameIndex=(m_frameIndex+1)%FramesInFlight;
}

//-------------------------------------------------------------------------------
// The target, from outside a frame

void Vulkan::clearTarget(const gweni::Color &color)
{
    // at once, in the queue's order: another renderer drawing into the target
    // after this sees it cleared, and so do this one's next frame's draws
    if(m_recording)
        submit();

    immediate([&](VkCommandBuffer commands)
    {
        targetBarrier(commands, TargetStages, TargetAccess);

        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType=VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView=m_target->colorView();
        colorAttachment.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color={ { color.r/255.0f, color.g/255.0f, color.b/255.0f, color.a/255.0f } };
        VkRenderingAttachmentInfo depthAttachment=colorAttachment;
        depthAttachment.imageView=m_target->depthView();
        depthAttachment.imageLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.clearValue.depthStencil={ 1.0f, 0 };

        VkRenderingInfo rendering{};
        rendering.sType=VK_STRUCTURE_TYPE_RENDERING_INFO;
        rendering.renderArea.extent={ (uint32_t)m_target->width(), (uint32_t)m_target->height() };
        rendering.layerCount=1;
        rendering.colorAttachmentCount=1;
        rendering.pColorAttachments=&colorAttachment;
        rendering.pDepthAttachment=&depthAttachment;
        vkCmdBeginRendering(commands, &rendering);
        vkCmdEndRendering(commands);
    });
}

void Vulkan::readPixels(int width, int height, std::vector<uint8_t> &pixels)
{
    if(m_recording)
        submit();

    width=std::min(width, m_target->width());
    height=std::min(height, m_target->height());
    pixels.assign(static_cast<size_t>(std::max(width, 0))*std::max(height, 0)*4, 0);
    if(width <= 0 || height <= 0)
        return;

    Buffer readback;

    if(!createBuffer(readback, pixels.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT, true))
        return;

    immediate([&](VkCommandBuffer commands)
    {
        imageBarrier(commands, m_target->colorImage(), VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            TargetStages|VK_PIPELINE_STAGE_TRANSFER_BIT, TargetAccess|VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);

        // the target's bottom rows, which hold the view's top-left corner (its rows
        // are bottom-up, as glReadPixels reads them from the bottom left)
        VkBufferImageCopy copy{};
        copy.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.layerCount=1;
        copy.imageExtent={ (uint32_t)width, (uint32_t)height, 1 };
        vkCmdCopyImageToBuffer(commands, m_target->colorImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            readback.buffer, 1, &copy);

        imageBarrier(commands, m_target->colorImage(), VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, TargetStages, TargetAccess);

        VkMemoryBarrier host{};
        host.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
        host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
            0, 1, &host, 0, nullptr, 0, nullptr);
    });

    // bottom-up rows to top-down, as OpenGLCore::readPixels turns glReadPixels' over
    const size_t rowBytes=(size_t)width*4;
    const uint8_t *rows=static_cast<const uint8_t *>(readback.mapped);

    for(int y=0; y<height; ++y)
        memcpy(&pixels[rowBytes*(size_t)y], rows+rowBytes*(size_t)(height-1-y), rowBytes);
    destroyBuffer(readback);
}

bool Vulkan::textureExists(TextureHandle handle) const
{
    return (handle != 0) && (texture(handle) != nullptr);
}

bool Vulkan::textureChannelSizes(TextureHandle handle, int bits[4]) const
{
    const Texture *found=texture(handle);

    if(!found)
        return false;

    // what the atlas holds, as GL reports its R8 and RGB8 formats; an LCD atlas is
    // RGBA8 on the device only because no RGB8 format is guaranteed to be sampled
    const int depth=(int)found->sourceDepth;

    for(int i=0; i<4; ++i)
        bits[i]=(i < depth)?8:0;
    if(depth == 4)
        bits[3]=8;
    return true;
}

//-------------------------------------------------------------------------------
// SlotRenderer's hooks

void Vulkan::beginFrame()
{
    // the blend and depth test are the pipelines'; the frame is begun as its
    // first upload or draw is recorded
}

void Vulkan::allocateVertexBuffer(SlotPath path, size_t bytes)
{
    Buffer &buffer=(path == SlotPath::Primitives)?m_vertexBuffer:m_textVertexBuffer;

    // the frames in flight may still draw from the old one; this frame has
    // recorded nothing from it yet (a path is allocated before its uploads and draws)
    if(buffer.buffer != VK_NULL_HANDLE)
    {
        vkQueueWaitIdle(m_device.queue);
        destroyBuffer(buffer);
    }
    createBuffer(buffer, bytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT, false);
}

void Vulkan::uploadVertexRange(SlotPath path, size_t offset, size_t bytes, const void *data)
{
    Buffer &buffer=(path == SlotPath::Primitives)?m_vertexBuffer:m_textVertexBuffer;

    if(buffer.buffer == VK_NULL_HANDLE || bytes == 0)
        return;

    const std::pair<VkBuffer, VkDeviceSize> source=stage(data, bytes);
    VkBufferCopy copy{ source.second, offset, bytes };

    vkCmdCopyBuffer(frame().transfer, source.first, buffer.buffer, 1, &copy);
}

size_t Vulkan::maxClipBoxRows() const
{
    return m_properties.limits.maxImageDimension2D;
}

void Vulkan::allocateClipBoxes(SlotPath path, size_t rows)
{
    // primitive.frag's CLIP_COLUMNS and CLIP_COLUMNS_LOG2
    static_assert(kClipColumns == 2048 && kClipColumnsLog2 == 11, "primitive.frag hard-codes 2048 clip boxes a row");

    TextureHandle &handle=(path == SlotPath::Primitives)?m_clipTexture:m_textClipTexture;

    if(handle != 0)
    {
        vkQueueWaitIdle(m_device.queue);
        destroyHandle(handle);
        handle=0;
    }

    Texture table;

    if(!createTexture(table, (uint32_t)kClipColumns, (uint32_t)rows, VK_FORMAT_R16G16B16A16_UINT, 1, 0))
        return;
    table.sourceDepth=sizeof(Box);
    table.deviceDepth=sizeof(Box);
    handle=addTexture(std::move(table));
}

void Vulkan::uploadClipBoxRows(SlotPath path, size_t x, size_t y, size_t width, size_t height,
    const Box *boxes)
{
    Texture *table=texture((path == SlotPath::Primitives)?m_clipTexture:m_textClipTexture);

    if(!table || width == 0 || height == 0)
        return;

    // the rows are kClipColumns boxes apart in the mirror, and go up as they are
    const size_t bytes=((height-1)*kClipColumns+width)*sizeof(Box);
    const std::pair<VkBuffer, VkDeviceSize> source=stage(boxes, bytes);

    beginTextureWrite(*table);

    VkBufferImageCopy copy{};
    copy.bufferOffset=source.second;
    copy.bufferRowLength=(uint32_t)kClipColumns;
    copy.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount=1;
    copy.imageOffset={ (int32_t)x, (int32_t)y, 0 };
    copy.imageExtent={ (uint32_t)width, (uint32_t)height, 1 };
    vkCmdCopyBufferToImage(frame().transfer, source.first, table->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
}

TextureHandle Vulkan::createAtlasTexture(const texture_atlas_t &atlas)
{
    // R8 for coverage and distance alike; an LCD atlas's RGB as RGBA8, which every
    // device samples (R8G8B8 need not be), the alpha unused
    const bool lcd=(atlas.depth == 3);
    Texture texture;

    if(!createTexture(texture, (uint32_t)atlas.width, (uint32_t)atlas.height,
        lcd?VK_FORMAT_R8G8B8A8_UNORM:VK_FORMAT_R8_UNORM, 1, 0))
        return 0;
    texture.sourceDepth=(uint32_t)atlas.depth;
    texture.deviceDepth=lcd?4:1;

    // contents undefined until uploadAtlasRows; sampled-ready (zero) from now on
    immediate([&](VkCommandBuffer commands)
    {
        VkClearColorValue zero{};
        VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        imageBarrier(commands, texture.image, VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        vkCmdClearColorImage(commands, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &zero, 1, &range);
        imageBarrier(commands, texture.image, VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
    });
    texture.layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    return addTexture(std::move(texture));
}

void Vulkan::uploadAtlasRows(TextureHandle handle, const texture_atlas_t &atlas, size_t firstRow, size_t rows)
{
    Texture *atlasTexture=texture(handle);

    if(!atlasTexture || rows == 0)
        return;

    const unsigned char *source=atlas.data+firstRow*atlas.width*atlas.depth;
    std::pair<VkBuffer, VkDeviceSize> staged;

    if(atlas.depth == 3)
    {
        std::vector<uint8_t> rgba(rows*atlas.width*4);

        for(size_t i=0; i<rows*atlas.width; ++i)
        {
            rgba[i*4+0]=source[i*3+0];
            rgba[i*4+1]=source[i*3+1];
            rgba[i*4+2]=source[i*3+2];
            rgba[i*4+3]=255;
        }
        staged=stage(rgba.data(), rgba.size());
    }
    else
        staged=stage(source, rows*atlas.width*atlas.depth);

    beginTextureWrite(*atlasTexture);

    VkBufferImageCopy copy{};
    copy.bufferOffset=staged.second;
    copy.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount=1;
    copy.imageOffset={ 0, (int32_t)firstRow, 0 };
    copy.imageExtent={ (uint32_t)atlas.width, (uint32_t)rows, 1 };
    vkCmdCopyBufferToImage(frame().transfer, staged.first, atlasTexture->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
}

void Vulkan::destroyAtlasTexture(TextureHandle handle)
{
    // a frame in flight may still sample it
    retireHandle(handle);
}

size_t Vulkan::maxImageLayers() const
{
    return std::max<size_t>(m_properties.limits.maxImageArrayLayers, 1);
}

bool Vulkan::allocateImagePage(size_t imageClass, size_t layers, size_t keptLayers)
{
    const uint32_t side=(uint32_t)imageClassSide(imageClass);
    const uint32_t levels=(uint32_t)imageClassLevels(imageClass);
    Texture page;

    // RGBA8, every level, sampled nearest and clamped (m_imageSampler), as
    // OpenGLCore's pages: the shader filters it itself (imageTexel)
    if(!createTexture(page, side, side, VK_FORMAT_R8G8B8A8_UNORM, levels, 0, (uint32_t)layers))
        return false;

    Texture *old=texture(m_imagePages[imageClass]);
    const uint32_t kept=old?(uint32_t)std::min<size_t>(keptLayers, old->layers):0;

    immediate([&](VkCommandBuffer commands)
    {
        imageBarrier(commands, page.image, VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        if(kept > 0)
        {
            imageBarrier(commands, old->image, VK_IMAGE_ASPECT_COLOR_BIT,
                old->layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);

            // every level of the kept layers, as GL's blits
            std::vector<VkImageCopy> copies(levels);

            for(uint32_t level=0; level<levels; ++level)
            {
                const uint32_t size=std::max(side >> level, 1u);

                copies[level].srcSubresource={ VK_IMAGE_ASPECT_COLOR_BIT, level, 0, kept };
                copies[level].dstSubresource={ VK_IMAGE_ASPECT_COLOR_BIT, level, 0, kept };
                copies[level].extent={ size, size, 1 };
            }
            vkCmdCopyImage(commands, old->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                page.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (uint32_t)copies.size(), copies.data());
            imageBarrier(commands, old->image, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, old->layout,
                VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
        }
        imageBarrier(commands, page.image, VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
    });

    page.layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    page.sourceDepth=4;
    page.deviceDepth=4;
    // a frame in flight may still sample the old page
    retireHandle(m_imagePages[imageClass]);
    m_imagePages[imageClass]=addTexture(std::move(page));
    return true;
}

void Vulkan::uploadImageLayer(size_t imageClass, size_t layer, int width, int height, const uint8_t *rgba,
    const ImageMipLevel *levels, size_t levelCount)
{
    Texture *page=texture(m_imagePages[imageClass]);

    if(!page || width <= 0 || height <= 0)
        return;

    size_t stagingBytes=(size_t)width*height*4;

    for(size_t i=0; i<levelCount; ++i)
        stagingBytes+=levels[i].texels.size();

    Buffer staging;

    if(!createBuffer(staging, stagingBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true))
        return;

    uint8_t *mapped=static_cast<uint8_t *>(staging.mapped);
    std::vector<VkBufferImageCopy> copies(levelCount+1);
    size_t offset=(size_t)width*height*4;

    memcpy(mapped, rgba, offset);
    copies[0].imageSubresource={ VK_IMAGE_ASPECT_COLOR_BIT, 0, (uint32_t)layer, 1 };
    copies[0].imageExtent={ (uint32_t)width, (uint32_t)height, 1 };
    for(size_t i=0; i<levelCount; ++i)
    {
        memcpy(mapped+offset, levels[i].texels.data(), levels[i].texels.size());
        copies[i+1].bufferOffset=offset;
        copies[i+1].imageSubresource={ VK_IMAGE_ASPECT_COLOR_BIT, (uint32_t)(i+1), (uint32_t)layer, 1 };
        copies[i+1].imageExtent={ (uint32_t)levels[i].width, (uint32_t)levels[i].height, 1 };
        offset+=levels[i].texels.size();
    }

    immediate([&](VkCommandBuffer commands)
    {
        imageBarrier(commands, page->image, VK_IMAGE_ASPECT_COLOR_BIT,
            page->layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            0, VK_REMAINING_MIP_LEVELS, (uint32_t)layer, 1);
        vkCmdCopyBufferToImage(commands, staging.buffer, page->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            (uint32_t)copies.size(), copies.data());
        imageBarrier(commands, page->image, VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, page->layout,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
            0, VK_REMAINING_MIP_LEVELS, (uint32_t)layer, 1);
    });
    destroyBuffer(staging);
}

bool Vulkan::readImageLayer(size_t imageClass, size_t layer, int width, int height, std::vector<uint8_t> &rgba)
{
    rgba.clear();

    Texture *page=texture(m_imagePages[imageClass]);

    if(!page || width <= 0 || height <= 0 || (uint32_t)width > page->width || (uint32_t)height > page->height ||
        layer >= page->layers)
        return false;
    if(m_recording)
        submit();

    const size_t bytes=(size_t)width*height*4;
    Buffer readback;

    if(!createBuffer(readback, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true))
        return false;

    immediate([&](VkCommandBuffer commands)
    {
        imageBarrier(commands, page->image, VK_IMAGE_ASPECT_COLOR_BIT,
            page->layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, 0, 1, (uint32_t)layer, 1);

        VkBufferImageCopy copy{};
        copy.imageSubresource={ VK_IMAGE_ASPECT_COLOR_BIT, 0, (uint32_t)layer, 1 };
        copy.imageExtent={ (uint32_t)width, (uint32_t)height, 1 };
        vkCmdCopyImageToBuffer(commands, page->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            readback.buffer, 1, &copy);

        imageBarrier(commands, page->image, VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, page->layout,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_TRANSFER_WRITE_BIT, 0, 1, (uint32_t)layer, 1);

        VkMemoryBarrier host{};
        host.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
        host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
            0, 1, &host, 0, nullptr, 0, nullptr);
    });

    rgba.resize(bytes);
    memcpy(rgba.data(), readback.mapped, bytes);
    destroyBuffer(readback);
    return true;
}

TextureHandle Vulkan::imagePageHandle(size_t imageClass) const
{
    return m_imagePages[imageClass];
}

void Vulkan::destroyImagePages()
{
    // a frame in flight may still sample them
    for(TextureHandle &page:m_imagePages)
    {
        retireHandle(page);
        page=0;
    }
}

bool Vulkan::readTextureRgba(TextureHandle handle, int width, int height, std::vector<uint8_t> &rgba)
{
    rgba.clear();

    Texture *source=texture(handle);

    if(!source || width <= 0 || height <= 0 || (uint32_t)width > source->width || (uint32_t)height > source->height)
        return false;
    if(m_recording)
        submit();

    const size_t texels=(size_t)width*height;
    Buffer readback;

    if(!createBuffer(readback, texels*source->deviceDepth, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true))
        return false;

    immediate([&](VkCommandBuffer commands)
    {
        imageBarrier(commands, source->image, VK_IMAGE_ASPECT_COLOR_BIT,
            source->layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, 0, 1);

        VkBufferImageCopy copy{};
        copy.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.layerCount=1;
        copy.imageExtent={ (uint32_t)width, (uint32_t)height, 1 };
        vkCmdCopyImageToBuffer(commands, source->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            readback.buffer, 1, &copy);

        imageBarrier(commands, source->image, VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, source->layout,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_TRANSFER_WRITE_BIT, 0, 1);

        VkMemoryBarrier host{};
        host.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
        host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
            0, 1, &host, 0, nullptr, 0, nullptr);
    });

    // as GL reads an R8 texture through a framebuffer: (r, 0, 0, 255)
    const uint8_t *texel=static_cast<const uint8_t *>(readback.mapped);

    rgba.resize(texels*4);
    if(source->deviceDepth == 4)
        memcpy(rgba.data(), texel, texels*4);
    else
    {
        for(size_t i=0; i<texels; ++i)
        {
            rgba[i*4+0]=texel[i];
            rgba[i*4+1]=0;
            rgba[i*4+2]=0;
            rgba[i*4+3]=255;
        }
    }
    destroyBuffer(readback);
    return true;
}

void Vulkan::beginPrimitiveDraws()
{
    Frame &current=frame();

    // this frame's set (the frame that last used it is done): the clip-box table
    // and each class's image page, GL's texture 0 for a class with none
    const Texture *clip=texture(m_clipTexture);
    const Texture *black=texture(m_blackTexture);
    VkDescriptorImageInfo clipInfo{ m_clipSampler, clip?clip->view:VK_NULL_HANDLE,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkDescriptorImageInfo imageInfo[ImageClasses];

    for(size_t i=0; i<ImageClasses; ++i)
    {
        const Texture *page=texture(m_imagePages[i]);

        if(!page)
            page=black;
        imageInfo[i]={ m_imageSampler, page->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    }

    VkWriteDescriptorSet writes[2]{};
    writes[0].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet=current.primitiveSet;
    writes[0].dstBinding=0;
    writes[0].descriptorCount=1;
    writes[0].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].pImageInfo=&clipInfo;
    writes[1]=writes[0];
    writes[1].dstBinding=1;
    writes[1].descriptorCount=(uint32_t)ImageClasses;
    writes[1].pImageInfo=imageInfo;
    vkUpdateDescriptorSets(device(), 2, writes, 0, nullptr);

    beginRendering();
    vkCmdBindPipeline(current.draw, VK_PIPELINE_BIND_POINT_GRAPHICS, m_primitivePipeline);
    vkCmdBindDescriptorSets(current.draw, VK_PIPELINE_BIND_POINT_GRAPHICS, m_primitiveLayout,
        0, 1, &current.primitiveSet, 0, nullptr);
    vkCmdBindIndexBuffer(current.draw, m_quadIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT16);
}

void Vulkan::drawPrimitiveChunk(const ChunkDraw &draw)
{
    Frame &current=frame();
    const size_t firstSlot=draw.chunk*indexChunkSlots();
    // the chunk's vertexes from its first slot's, as its GL vertex array points
    const VkDeviceSize offset=firstSlot*4*sizeof(Vertex);

    m_push.slotBase=(int32_t)firstSlot;
    vkCmdBindVertexBuffers(current.draw, 0, 1, &m_vertexBuffer.buffer, &offset);
    vkCmdPushConstants(current.draw, m_primitiveLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
        sizeof(PrimitivePush), &m_push);
    // glMultiDrawElements' ranges, one draw each; the offsets are bytes of 16-bit indexes
    for(size_t range=0; range<draw.counts.size(); ++range)
        vkCmdDrawIndexed(current.draw, (uint32_t)draw.counts[range], 1,
            (uint32_t)(draw.offsets[range]/sizeof(uint16_t)), 0, 0);
}

void Vulkan::endPrimitiveDraws()
{
}

void Vulkan::beginTextDraws()
{
    Frame &current=frame();

    beginRendering();
    vkCmdBindIndexBuffer(current.draw, m_textQuadIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT16);
    m_boundTextPipeline=VK_NULL_HANDLE;
    m_drawingAtlas=false;

    memcpy(m_textPush.projection, m_push.projection, sizeof(m_textPush.projection));
    m_textPush.textContrast=textContrast();
}

void Vulkan::beginTextAtlas(const FontData &fontData)
{
    Frame &current=frame();
    const bool lcd=(fontData.mode == TextMode::LcdRGB) || (fontData.mode == TextMode::LcdBGR);
    // without dual-source blending an LCD atlas would be blended by its mean
    // coverage, as on GL; lcdTextUnavailable() keeps the LCD modes from being used
    const VkPipeline pipeline=(lcd && (m_lcdTextPipeline != VK_NULL_HANDLE))?m_lcdTextPipeline:m_textPipeline;
    const Texture *atlas=texture(fontData.atlas?(TextureHandle)fontData.atlas->id:0);
    const Texture *clip=texture(m_textClipTexture);

    m_drawingAtlas=(atlas != nullptr) && (clip != nullptr) && (m_textVertexBuffer.buffer != VK_NULL_HANDLE);
    if(!m_drawingAtlas)
        return;

    const VkDescriptorSet set=allocateTextSet();

    if(set == VK_NULL_HANDLE)
    {
        m_drawingAtlas=false;
        return;
    }

    // the atlas linear and clamped, as OpenGLCore's GL_LINEAR atlases; the clip
    // boxes nearest
    const VkDescriptorImageInfo images[2]=
    {
        { m_atlasSampler, atlas->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { m_clipSampler, clip->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
    };
    VkWriteDescriptorSet writes[2]{};

    for(uint32_t i=0; i<2; ++i)
    {
        writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet=set;
        writes[i].dstBinding=i;
        writes[i].descriptorCount=1;
        writes[i].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].pImageInfo=&images[i];
    }
    vkUpdateDescriptorSets(device(), 2, writes, 0, nullptr);

    if(pipeline != m_boundTextPipeline)
    {
        vkCmdBindPipeline(current.draw, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        m_boundTextPipeline=pipeline;
    }
    vkCmdBindDescriptorSets(current.draw, VK_PIPELINE_BIND_POINT_GRAPHICS, m_textLayout,
        0, 1, &set, 0, nullptr);
    m_textPush.textMode=(int32_t)fontData.mode;
}

void Vulkan::drawTextChunk(const ChunkDraw &draw)
{
    if(!m_drawingAtlas)
        return;

    Frame &current=frame();
    const size_t firstSlot=draw.chunk*indexChunkSlots();
    // the chunk's glyph vertexes from its first slot's, as its GL vertex array points
    const VkDeviceSize offset=firstSlot*4*sizeof(ftgl::glyph_vertex_t);

    m_textPush.slotBase=(int32_t)firstSlot;
    vkCmdBindVertexBuffers(current.draw, 0, 1, &m_textVertexBuffer.buffer, &offset);
    vkCmdPushConstants(current.draw, m_textLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(TextPush), &m_textPush);
    // glMultiDrawElements' ranges, one draw each; the offsets are bytes of 16-bit indexes
    for(size_t range=0; range<draw.counts.size(); ++range)
        vkCmdDrawIndexed(current.draw, (uint32_t)draw.counts[range], 1,
            (uint32_t)(draw.offsets[range]/sizeof(uint16_t)), 0, 0);
}

void Vulkan::endTextDraws()
{
    m_boundTextPipeline=VK_NULL_HANDLE;
    m_drawingAtlas=false;
}

void Vulkan::submitFrame()
{
    submit();
}

void Vulkan::indexChunkSlotsChanged()
{
    // nothing is made per chunk: each draw binds its own first slot
}

}//namespace renderer
}//namespace gweni
