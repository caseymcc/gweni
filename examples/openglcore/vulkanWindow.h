/*
 *  Gweni - the Vulkan backend's interactive window for the examples: a GLFW
 *  window with no client API, a Vulkan surface on it and a FIFO swapchain that
 *  shows what renderer::Vulkan drew into its VulkanTarget (capture::Device,
 *  capture.h). Only in a tree built with GWENI_RENDER_VULKAN.
 *
 *  See license in Gweni.h
 */
#ifndef _gweni_example_vulkanWindow_h_
#define _gweni_example_vulkanWindow_h_

#include "capture.h"

#ifdef GWENI_HAS_VULKAN

namespace gweni
{
namespace capture
{

//! What openDevice gives an interactive Vulkan run: the window of
//! \a options.size (GLFW_SCALE_TO_MONITOR, as a GL window is), a device made
//! with the window system's surface extensions and VK_KHR_swapchain, the
//! target and the swapchain at the framebuffer's size. \return null, after one
//! line, if there is no display or no device that can present to it.
std::unique_ptr<Device> openVulkanWindowDevice(const DeviceOptions &options);

}//namespace capture
}//namespace gweni

#endif//GWENI_HAS_VULKAN

#endif//_gweni_example_vulkanWindow_h_
