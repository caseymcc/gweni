/*
 *  Gweni - the Vulkan backend's interactive window: a GLFW window with no client
 *  API, its Vulkan surface and a FIFO swapchain.
 *
 *  renderer::Vulkan draws into a VulkanTarget, as it does in a capture, and a
 *  frame is shown by blitting the target into an acquired swapchain image and
 *  presenting it. The target's rows are bottom-up (GL's window coordinates,
 *  vulkan.h), so the blit turns it over; the swapchain image is then top-down,
 *  as the window shows it and a PNG stores it, and a capture's readPixels of the
 *  same frame gives the same bytes.
 *
 *  Two frames are in flight, each with its command buffer, its fence and the
 *  semaphore its acquire signals; the semaphore a submit signals for the present
 *  to wait on is the swapchain image's, so it is free again once the image is
 *  acquired again. The swapchain is made again, after the device has finished
 *  everything sent to it, when the framebuffer changes size (the size callback
 *  has already fitted the canvas and the renderer's view to it, capture.h) and
 *  when an acquire or a present says it is out of date or suboptimal. While the
 *  window is minimised (iconified, or a 0x0 framebuffer, as Windows makes it)
 *  nothing is drawn, acquired or presented: the loop waits for events. The loop
 *  is input::GlfwLoop's, as GL's: idle, it draws only when the canvas has work,
 *  and a frame that was not shown marks the canvas for another.
 *
 *  See license in Gweni.h
 */

#ifdef GWENI_HAS_VULKAN
// before GLFW's header (through capture.h), which declares its Vulkan
// functions (glfwCreateWindowSurface) only once Vulkan's are
#include <gweni/platforms/renderers/vulkan.h>
#endif

#include "vulkanWindow.h"

#ifdef GWENI_HAS_VULKAN

#include <gweni/controls/canvas.h>
#include <gweni/platforms/input/glfwLoop.h>
#include <gweni/platforms/platform.h>

#include <algorithm>
#include <cstring>
#include <iostream>

namespace gweni
{
namespace capture
{

namespace
{

//! Everything a frame's draws into the target do to it, as a barrier's stages
//! (vulkan.cpp's TargetStages): the next frame's draws come after the blit's read
const VkPipelineStageFlags TargetStages=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|
    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
const VkAccessFlags TargetAccess=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

void imageBarrier(VkCommandBuffer commands, VkImage image, VkImageLayout from, VkImageLayout to,
    VkPipelineStageFlags srcStages, VkAccessFlags srcAccess,
    VkPipelineStageFlags dstStages, VkAccessFlags dstAccess)
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
    vkCmdPipelineBarrier(commands, srcStages, dstStages, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

//! The window, its surface, the swapchain and the target renderer::Vulkan draws
//! into, which a frame's present blits into a swapchain image.
class VulkanWindowDevice: public Device
{
public:
    ~VulkanWindowDevice() override;

    Backend backend() const override { return Backend::Vulkan; }

    renderer::SlotRenderer *newRenderer(ResourcePaths &paths, const Rect &viewRect) override
    {
        return new renderer::Vulkan(paths, viewRect, m_device, *m_target);
    }

    bool resizeTarget(const Point &size) override
    {
        if(m_target->create(size.x, size.y))
            return true;
        std::cerr<<toolName()<<": could not make a "<<size.x<<"x"<<size.y<<" Vulkan target\n";
        return false;
    }

    void finish() override
    {
        vkQueueWaitIdle(m_device.queue);
    }

    std::string deviceName() const override
    {
        VkPhysicalDeviceProperties properties{};

        vkGetPhysicalDeviceProperties(m_device.physicalDevice, &properties);
        return properties.deviceName;
    }

    renderer::VulkanDevice *vulkanDevice() override { return &m_device; }
    renderer::VulkanTarget *vulkanTarget() override { return m_target.get(); }

    void runWindowLoop(controls::Canvas *canvas, renderer::Base *renderer,
        const platform::LoopOptions &options, std::function<void()> frame) override;
    bool beginWindowFrame() override;
    bool presentWindowFrame(std::vector<uint8_t> *pixels, Point *size) override;
    const WindowStats *windowStats() const override { return &m_stats; }

    bool open(const DeviceOptions &options);

private:
    struct Frame
    {
        VkCommandBuffer commands=VK_NULL_HANDLE;
        VkFence fence=VK_NULL_HANDLE;
        //! signalled by this frame's acquire, waited on by its submit
        VkSemaphore acquired=VK_NULL_HANDLE;
        bool submitted=false;
    };

    static constexpr size_t FramesInFlight=2;

    VkDevice device() const { return m_device.device; }
    //! Whether nothing should be drawn or shown: the window is iconified, or its
    //! framebuffer 0x0 (\a framebuffer is its size)
    bool minimised(Point &framebuffer) const;
    //! The swapchain made (again) at the surface's size, after everything sent
    //! to the device is done; the target remade at \a framebuffer first if it is
    //! another size. False, leaving it to the next frame, if the surface is 0x0.
    bool recreate(const Point &framebuffer);
    //! A host-visible buffer of at least \a bytes the swapchain image is copied into
    bool readbackBuffer(VkDeviceSize bytes);
    void destroyReadback();
    //! Waits for \a frame's last submit, if it has one in flight.
    void waitFrame(Frame &frame);

    renderer::VulkanDevice m_device;
    std::unique_ptr<renderer::VulkanTarget> m_target;
    VkSurfaceKHR m_surface=VK_NULL_HANDLE;

    VkSwapchainKHR m_swapchain=VK_NULL_HANDLE;
    VkSurfaceFormatKHR m_format{};
    VkExtent2D m_extent{};
    std::vector<VkImage> m_images;
    //! one per swapchain image, signalled by the submit that blits into it and
    //! waited on by its present; kept across recreates, grown as needed
    std::vector<VkSemaphore> m_rendered;
    //! made again before the next frame: a new size, or an acquire or present
    //! that said the swapchain no longer matches the surface
    bool m_recreate=false;

    VkCommandPool m_commandPool=VK_NULL_HANDLE;
    Frame m_frames[FramesInFlight];
    size_t m_frameIndex=0;

    VkBuffer m_readback=VK_NULL_HANDLE;
    VkDeviceMemory m_readbackMemory=VK_NULL_HANDLE;
    VkDeviceSize m_readbackSize=0;
    void *m_readbackMapped=nullptr;

    WindowStats m_stats;
};

VulkanWindowDevice::~VulkanWindowDevice()
{
    if(device() != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(device());
        if(m_swapchain != VK_NULL_HANDLE)
            vkDestroySwapchainKHR(device(), m_swapchain, nullptr);
        for(VkSemaphore semaphore:m_rendered)
            vkDestroySemaphore(device(), semaphore, nullptr);
        for(Frame &frame:m_frames)
        {
            if(frame.fence != VK_NULL_HANDLE)
                vkDestroyFence(device(), frame.fence, nullptr);
            if(frame.acquired != VK_NULL_HANDLE)
                vkDestroySemaphore(device(), frame.acquired, nullptr);
        }
        if(m_commandPool != VK_NULL_HANDLE)
            vkDestroyCommandPool(device(), m_commandPool, nullptr);
        destroyReadback();
    }
    m_target.reset();
    if(m_surface != VK_NULL_HANDLE)
        vkDestroySurfaceKHR(m_device.instance, m_surface, nullptr);
    renderer::destroyVulkanDevice(m_device);
    destroyContext(m_context);
}

bool VulkanWindowDevice::open(const DeviceOptions &options)
{
    const std::string &tool=toolName();

    if(!glfwInit())
    {
        std::cerr<<tool<<": GLFW could not open a window; interactive mode needs a display\n";
        return false;
    }
    if(!glfwVulkanSupported())
    {
        std::cerr<<tool<<": GLFW found no Vulkan loader for a window (libvulkan.so.1)\n";
        glfwTerminate();
        return false;
    }

    // a window of no client API, drawn into through its Vulkan surface; scaled
    // to its monitor as a GL window is (initGlfwContext)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
    m_context.window=glfwCreateWindow(options.size.x, options.size.y, options.title.c_str(), nullptr, nullptr);
    if(!m_context.window)
    {
        std::cerr<<tool<<": GLFW could not open a window; interactive mode needs a display\n";
        glfwTerminate();
        return false;
    }
    // undone by destroyContext, as for a GL window
    if(options.systemClipboard)
        input::GLFW3::useSystemClipboard(m_context.window);

    renderer::VulkanDeviceOptions deviceOptions;
    uint32_t extensionCount=0;
    const char **extensions=glfwGetRequiredInstanceExtensions(&extensionCount);
    std::string error;

    if(!extensions)
    {
        std::cerr<<tool<<": GLFW knows no Vulkan surface extensions for this window system\n";
        return false;
    }
    deviceOptions.applicationName=tool;
    deviceOptions.validation=vulkanValidation();
    deviceOptions.instanceExtensions.assign(extensions, extensions+extensionCount);
    deviceOptions.deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    if(!renderer::createVulkanDevice(m_device, deviceOptions, error))
    {
        std::cerr<<tool<<": "<<error<<"\n";
        return false;
    }

    VkResult result=glfwCreateWindowSurface(m_device.instance, m_context.window, nullptr, &m_surface);

    if(result != VK_SUCCESS)
    {
        std::cerr<<tool<<": glfwCreateWindowSurface failed: VkResult "<<(int)result<<"\n";
        m_surface=VK_NULL_HANDLE;
        return false;
    }

    VkBool32 presents=VK_FALSE;

    vkGetPhysicalDeviceSurfaceSupportKHR(m_device.physicalDevice, m_device.queueFamily, m_surface, &presents);
    if(!presents)
    {
        std::cerr<<tool<<": the Vulkan device's graphics queue cannot present to the window\n";
        return false;
    }

    // An 8-bit UNORM format, as a GL window's default framebuffer is (no sRGB
    // encoding on the way out), into which the RGBA8 target blits.
    uint32_t formatCount=0;

    vkGetPhysicalDeviceSurfaceFormatsKHR(m_device.physicalDevice, m_surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_device.physicalDevice, m_surface, &formatCount, formats.data());

    m_format.format=VK_FORMAT_UNDEFINED;
    for(VkFormat wanted:{ VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM })
    {
        for(const VkSurfaceFormatKHR &format:formats)
        {
            if(format.format == wanted && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                m_format=format;
                break;
            }
        }
        if(m_format.format != VK_FORMAT_UNDEFINED)
            break;
    }
    if(m_format.format == VK_FORMAT_UNDEFINED)
    {
        std::cerr<<tool<<": the window's surface has no B8G8R8A8_UNORM or R8G8B8A8_UNORM format\n";
        return false;
    }

    VkFormatProperties targetFeatures, swapchainFeatures;

    vkGetPhysicalDeviceFormatProperties(m_device.physicalDevice, VK_FORMAT_R8G8B8A8_UNORM, &targetFeatures);
    vkGetPhysicalDeviceFormatProperties(m_device.physicalDevice, m_format.format, &swapchainFeatures);
    if(!(targetFeatures.optimalTilingFeatures&VK_FORMAT_FEATURE_BLIT_SRC_BIT) ||
        !(swapchainFeatures.optimalTilingFeatures&VK_FORMAT_FEATURE_BLIT_DST_BIT))
    {
        std::cerr<<tool<<": the Vulkan device cannot blit its target into the window's images\n";
        return false;
    }

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex=m_device.queueFamily;
    if(vkCreateCommandPool(device(), &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS)
    {
        std::cerr<<tool<<": vkCreateCommandPool failed\n";
        return false;
    }
    for(Frame &frame:m_frames)
    {
        VkCommandBufferAllocateInfo allocate{};
        allocate.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate.commandPool=m_commandPool;
        allocate.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount=1;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        if(vkAllocateCommandBuffers(device(), &allocate, &frame.commands) != VK_SUCCESS ||
            vkCreateFence(device(), &fenceInfo, nullptr, &frame.fence) != VK_SUCCESS ||
            vkCreateSemaphore(device(), &semaphoreInfo, nullptr, &frame.acquired) != VK_SUCCESS)
        {
            std::cerr<<tool<<": could not make the window's frames\n";
            return false;
        }
    }

    // The target at the framebuffer's size (GLFW_SCALE_TO_MONITOR may have made
    // it other than options.size; the caller's fitCanvasToWindow fits the
    // renderer's view and the canvas to it), the swapchain with the first frame.
    Point framebuffer;

    glfwGetFramebufferSize(m_context.window, &framebuffer.x, &framebuffer.y);
    if(framebuffer.x <= 0 || framebuffer.y <= 0)
        framebuffer=options.size;
    m_target.reset(new renderer::VulkanTarget(m_device));
    if(!resizeTarget(framebuffer))
        return false;
    m_recreate=true;
    return true;
}

void VulkanWindowDevice::destroyReadback()
{
    if(m_readback != VK_NULL_HANDLE)
        vkDestroyBuffer(device(), m_readback, nullptr);
    if(m_readbackMemory != VK_NULL_HANDLE)
        vkFreeMemory(device(), m_readbackMemory, nullptr);
    m_readback=VK_NULL_HANDLE;
    m_readbackMemory=VK_NULL_HANDLE;
    m_readbackSize=0;
    m_readbackMapped=nullptr;
}

bool VulkanWindowDevice::readbackBuffer(VkDeviceSize bytes)
{
    if(m_readback != VK_NULL_HANDLE && m_readbackSize >= bytes)
        return true;
    destroyReadback();

    VkBufferCreateInfo info{};
    info.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size=bytes;
    info.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    if(vkCreateBuffer(device(), &info, nullptr, &m_readback) != VK_SUCCESS)
        return false;

    VkMemoryRequirements requirements;
    VkPhysicalDeviceMemoryProperties memory;
    const VkMemoryPropertyFlags wanted=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    vkGetBufferMemoryRequirements(device(), m_readback, &requirements);
    vkGetPhysicalDeviceMemoryProperties(m_device.physicalDevice, &memory);

    VkMemoryAllocateInfo allocate{};
    allocate.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate.allocationSize=requirements.size;
    allocate.memoryTypeIndex=UINT32_MAX;
    for(uint32_t i=0; i<memory.memoryTypeCount; ++i)
    {
        if((requirements.memoryTypeBits&(1u << i)) && (memory.memoryTypes[i].propertyFlags&wanted) == wanted)
        {
            allocate.memoryTypeIndex=i;
            break;
        }
    }
    if(allocate.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(device(), &allocate, nullptr, &m_readbackMemory) != VK_SUCCESS)
    {
        destroyReadback();
        return false;
    }
    vkBindBufferMemory(device(), m_readback, m_readbackMemory, 0);
    vkMapMemory(device(), m_readbackMemory, 0, VK_WHOLE_SIZE, 0, &m_readbackMapped);
    m_readbackSize=bytes;
    return true;
}

bool VulkanWindowDevice::minimised(Point &framebuffer) const
{
    glfwGetFramebufferSize(m_context.window, &framebuffer.x, &framebuffer.y);
    return framebuffer.x <= 0 || framebuffer.y <= 0 ||
        glfwGetWindowAttrib(m_context.window, GLFW_ICONIFIED) == GLFW_TRUE;
}

void VulkanWindowDevice::waitFrame(Frame &frame)
{
    if(!frame.submitted)
        return;
    vkWaitForFences(device(), 1, &frame.fence, VK_TRUE, UINT64_MAX);
    vkResetFences(device(), 1, &frame.fence);
    frame.submitted=false;
}

bool VulkanWindowDevice::recreate(const Point &framebuffer)
{
    // nothing sent to the device still uses the old images, the target's or
    // the swapchain's, or a frame's semaphores
    vkDeviceWaitIdle(device());
    for(Frame &frame:m_frames)
        waitFrame(frame);

    const bool resized=framebuffer.x != m_target->width() || framebuffer.y != m_target->height();

    // the canvas and the renderer's view were fitted to it by the size callback
    if(resized && !resizeTarget(framebuffer))
        return false;

    VkSurfaceCapabilitiesKHR capabilities;

    if(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_device.physicalDevice, m_surface, &capabilities) != VK_SUCCESS)
        return false;

    VkExtent2D extent=capabilities.currentExtent;

    // the surface lets the swapchain choose: the framebuffer's size
    if(extent.width == UINT32_MAX)
    {
        extent.width=std::min(std::max((uint32_t)framebuffer.x, capabilities.minImageExtent.width),
            capabilities.maxImageExtent.width);
        extent.height=std::min(std::max((uint32_t)framebuffer.y, capabilities.minImageExtent.height),
            capabilities.maxImageExtent.height);
    }
    // minimised as far as the surface is concerned: no swapchain at 0x0
    if(extent.width == 0 || extent.height == 0)
        return false;

    const VkImageUsageFlags usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    if((capabilities.supportedUsageFlags&usage) != usage)
    {
        std::cerr<<toolName()<<": the window's swapchain images cannot be blitted into and read back\n";
        return false;
    }

    uint32_t imageCount=capabilities.minImageCount+1;

    if(capabilities.maxImageCount > 0)
        imageCount=std::min(imageCount, capabilities.maxImageCount);

    VkSwapchainCreateInfoKHR info{};
    info.sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface=m_surface;
    info.minImageCount=imageCount;
    info.imageFormat=m_format.format;
    info.imageColorSpace=m_format.colorSpace;
    info.imageExtent=extent;
    info.imageArrayLayers=1;
    info.imageUsage=usage;
    info.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform=capabilities.currentTransform;
    info.compositeAlpha=(capabilities.supportedCompositeAlpha&VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) ?
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    // every implementation has FIFO: one image per vertical blank, never torn
    info.presentMode=VK_PRESENT_MODE_FIFO_KHR;
    info.clipped=VK_TRUE;
    info.oldSwapchain=m_swapchain;

    VkSwapchainKHR swapchain=VK_NULL_HANDLE;
    const VkResult result=vkCreateSwapchainKHR(device(), &info, nullptr, &swapchain);

    if(m_swapchain != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(device(), m_swapchain, nullptr);
    m_swapchain=VK_NULL_HANDLE;
    m_images.clear();
    if(result != VK_SUCCESS)
    {
        std::cerr<<toolName()<<": vkCreateSwapchainKHR failed: VkResult "<<(int)result<<"\n";
        return false;
    }
    m_swapchain=swapchain;
    m_extent=extent;

    uint32_t count=0;

    vkGetSwapchainImagesKHR(device(), m_swapchain, &count, nullptr);
    m_images.resize(count);
    vkGetSwapchainImagesKHR(device(), m_swapchain, &count, m_images.data());
    while(m_rendered.size() < m_images.size())
    {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkSemaphore semaphore=VK_NULL_HANDLE;
        vkCreateSemaphore(device(), &semaphoreInfo, nullptr, &semaphore);
        m_rendered.push_back(semaphore);
    }

    if(m_stats.swapchains > 0 && resized)
        ++m_stats.resizeRecreates;
    ++m_stats.swapchains;
    m_stats.extent=Point((int)extent.width, (int)extent.height);
    m_recreate=false;
    return true;
}

bool VulkanWindowDevice::beginWindowFrame()
{
    Point framebuffer;

    if(minimised(framebuffer))
    {
        ++m_stats.skipped;
        return false;
    }
    if(m_recreate || m_swapchain == VK_NULL_HANDLE ||
        framebuffer.x != m_target->width() || framebuffer.y != m_target->height())
    {
        if(!recreate(framebuffer))
        {
            m_recreate=true;
            ++m_stats.skipped;
            return false;
        }
    }
    ++m_stats.frames;
    return true;
}

bool VulkanWindowDevice::presentWindowFrame(std::vector<uint8_t> *pixels, Point *size)
{
    if(m_swapchain == VK_NULL_HANDLE)
        return false;

    Point framebuffer;

    // minimised since beginWindowFrame: the frame is not shown, as none is
    // while the window is
    if(minimised(framebuffer))
    {
        ++m_stats.dropped;
        return false;
    }

    Frame &frame=m_frames[m_frameIndex];

    // its acquire semaphore and command buffer are free again
    waitFrame(frame);

    uint32_t index=0;
    VkResult result=vkAcquireNextImageKHR(device(), m_swapchain, UINT64_MAX, frame.acquired,
        VK_NULL_HANDLE, &index);

    if(result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        // nothing was acquired, so nothing waits on the semaphore: made again
        // before the next frame, which draws again
        ++m_stats.outOfDate;
        m_recreate=true;
        return false;
    }
    if(result == VK_SUBOPTIMAL_KHR)
    {
        // acquired, and shown, but not at the surface's size any more
        ++m_stats.suboptimal;
        m_recreate=true;
    }
    else if(result != VK_SUCCESS)
    {
        std::cerr<<toolName()<<": vkAcquireNextImageKHR failed: VkResult "<<(int)result<<"\n";
        return false;
    }

    const VkImage image=m_images[index];
    const int targetWidth=m_target->width();
    const int targetHeight=m_target->height();
    // the same size, unless the surface changed since the swapchain was made
    const int width=std::min(targetWidth, (int)m_extent.width);
    const int height=std::min(targetHeight, (int)m_extent.height);
    const VkDeviceSize readbackBytes=(VkDeviceSize)m_extent.width*m_extent.height*4;

    if(width != (int)m_extent.width || height != (int)m_extent.height)
        m_recreate=true;
    if(pixels && !readbackBuffer(readbackBytes))
    {
        std::cerr<<toolName()<<": no host-visible memory to read the window's image back into\n";
        pixels=nullptr;
    }

    vkResetCommandBuffer(frame.commands, 0);

    VkCommandBufferBeginInfo begin{};
    begin.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(frame.commands, &begin);

    // the target, once the renderer's draws (and its clear) are done with it;
    // the swapchain image from whatever it held, after the acquire's semaphore
    imageBarrier(frame.commands, m_target->colorImage(),
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        TargetStages|VK_PIPELINE_STAGE_TRANSFER_BIT, TargetAccess|VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    imageBarrier(frame.commands, image,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);

    if(width < (int)m_extent.width || height < (int)m_extent.height)
    {
        // a frame between a surface's change and its recreate: the part the
        // target does not reach is black, not what the image held before
        VkClearColorValue black{};
        VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        vkCmdClearColorImage(frame.commands, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &black, 1, &range);
        imageBarrier(frame.commands, image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    }

    // The view's top row is the target's last (its rows are bottom-up, GL's), so
    // the target's top `height` rows are turned over into the image's first:
    // one to one, each pixel copied as it is (RGBA8 to the image's 8-bit
    // format, BGRA swizzled), nothing filtered.
    VkImageBlit blit{};
    blit.srcSubresource={ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    blit.srcOffsets[0]={ 0, targetHeight-height, 0 };
    blit.srcOffsets[1]={ width, targetHeight, 1 };
    blit.dstSubresource={ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    blit.dstOffsets[0]={ 0, height, 0 };
    blit.dstOffsets[1]={ width, 0, 1 };
    vkCmdBlitImage(frame.commands, m_target->colorImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);

    if(pixels)
    {
        // what is presented, read back from the image itself
        imageBarrier(frame.commands, image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);

        VkBufferImageCopy copy{};
        copy.imageSubresource={ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        copy.imageExtent={ m_extent.width, m_extent.height, 1 };
        vkCmdCopyImageToBuffer(frame.commands, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            m_readback, 1, &copy);

        VkMemoryBarrier host{};
        host.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
        host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(frame.commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
            0, 1, &host, 0, nullptr, 0, nullptr);

        imageBarrier(frame.commands, image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0);
    }
    else
    {
        imageBarrier(frame.commands, image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0);
    }

    // back to the layout the renderers keep it in, before the next frame draws
    imageBarrier(frame.commands, m_target->colorImage(),
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
        TargetStages, TargetAccess);
    vkEndCommandBuffer(frame.commands);

    const VkPipelineStageFlags waitStage=VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submit{};
    submit.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount=1;
    submit.pWaitSemaphores=&frame.acquired;
    submit.pWaitDstStageMask=&waitStage;
    submit.commandBufferCount=1;
    submit.pCommandBuffers=&frame.commands;
    submit.signalSemaphoreCount=1;
    submit.pSignalSemaphores=&m_rendered[index];
    result=vkQueueSubmit(m_device.queue, 1, &submit, frame.fence);
    if(result != VK_SUCCESS)
    {
        std::cerr<<toolName()<<": vkQueueSubmit failed: VkResult "<<(int)result<<"\n";
        return false;
    }
    frame.submitted=true;
    m_frameIndex=(m_frameIndex+1)%FramesInFlight;

    VkPresentInfoKHR present{};
    present.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount=1;
    present.pWaitSemaphores=&m_rendered[index];
    present.swapchainCount=1;
    present.pSwapchains=&m_swapchain;
    present.pImageIndices=&index;
    result=vkQueuePresentKHR(m_device.queue, &present);
    if(result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        ++m_stats.outOfDate;
        m_recreate=true;
    }
    else if(result == VK_SUBOPTIMAL_KHR)
    {
        ++m_stats.suboptimal;
        m_recreate=true;
    }
    else if(result != VK_SUCCESS)
    {
        std::cerr<<toolName()<<": vkQueuePresentKHR failed: VkResult "<<(int)result<<"\n";
        return false;
    }
    ++m_stats.presents;

    if(pixels)
    {
        // the copy is done once the submit's fence has signalled
        vkWaitForFences(device(), 1, &frame.fence, VK_TRUE, UINT64_MAX);

        const size_t texels=(size_t)m_extent.width*m_extent.height;
        const uint8_t *source=static_cast<const uint8_t *>(m_readbackMapped);
        const bool bgra=m_format.format == VK_FORMAT_B8G8R8A8_UNORM;

        pixels->resize(texels*4);
        for(size_t i=0; i<texels; ++i)
        {
            uint8_t *out=&(*pixels)[i*4];

            out[0]=source[i*4+(bgra ? 2 : 0)];
            out[1]=source[i*4+1];
            out[2]=source[i*4+(bgra ? 0 : 2)];
            out[3]=source[i*4+3];
        }
        if(size)
            *size=Point((int)m_extent.width, (int)m_extent.height);
    }
    return true;
}

void VulkanWindowDevice::runWindowLoop(controls::Canvas *canvas, renderer::Base *renderer,
    const platform::LoopOptions &options, std::function<void()> frame)
{
    // a beginWindowFrame that found nothing to draw into (minimised since the
    // loop asked, or a remake that failed): the next iteration waits for an
    // event rather than spinning on the canvas's pending work
    bool beginFailed=false;
    input::GlfwLoop::Callbacks callbacks;

    callbacks.drawFrame=[this, canvas, &beginFailed, frame]() {
        if(!beginWindowFrame())
        {
            beginFailed=true;
            return;
        }
        frame();
        // not shown (an out-of-date acquire, or minimised since the begin):
        // the canvas has nothing left to draw, so it is marked for the next
        if(!presentWindowFrame(nullptr, nullptr))
            canvas->redraw();
    };
    callbacks.canDraw=[this, &beginFailed]() {
        Point framebuffer;
        const bool failed=beginFailed;

        beginFailed=false;
        // minimised: nothing to show until an event (the restore) comes
        return !failed && !minimised(framebuffer);
    };

    input::GlfwLoop loop(m_context.window, canvas, renderer, options, std::move(callbacks));

    loop.run();
    vkDeviceWaitIdle(device());
}

}//namespace

std::unique_ptr<Device> openVulkanWindowDevice(const DeviceOptions &options)
{
    std::unique_ptr<VulkanWindowDevice> device(new VulkanWindowDevice);

    if(!device->open(options))
        return nullptr;
    return std::move(device);
}

}//namespace capture
}//namespace gweni

#endif//GWENI_HAS_VULKAN
