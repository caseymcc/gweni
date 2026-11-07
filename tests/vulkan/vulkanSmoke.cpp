/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  vulkan_smoke: the Vulkan plumbing works before any renderer code uses it.
 *  It loads libvulkan.so.1 through volk, creates a Vulkan 1.3 instance with
 *  VK_LAYER_KHRONOS_validation and a debug-utils messenger that prints any
 *  validation warning or error and aborts, finds a device with apiVersion 1.3,
 *  dynamicRendering and dualSrcBlend, creates a device on it, and builds a
 *  dynamic-rendering pipeline with dual-source blending from the SPIR-V that
 *  gweni_add_spirv compiled (smoke.vert, smoke.frag). It draws nothing.
 *
 *  Each thing that can be missing fails with one line naming it: the loader
 *  (libvulkan1), the layer (its manifest on VK_LAYER_PATH, which
 *  gweni_vulkan_test_environment sets) and the device (mesa-vulkan-drivers'
 *  lavapipe). It never runs without the layer: no layer is a failure.
 */
#include <volk.h>

#include "smoke.frag.h"
#include "smoke.vert.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace
{

const char *const ValidationLayer="VK_LAYER_KHRONOS_validation";

int fail(const char *message)
{
    std::fprintf(stderr, "vulkan_smoke: %s\n", message);
    return EXIT_FAILURE;
}

int failResult(const char *what, VkResult result)
{
    std::fprintf(stderr, "vulkan_smoke: %s failed: VkResult %d\n", what, static_cast<int>(result));
    return EXIT_FAILURE;
}

VKAPI_ATTR VkBool32 VKAPI_CALL onMessage(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT /*types*/, const VkDebugUtilsMessengerCallbackDataEXT *data,
    void * /*user*/)
{
    const char *level=(severity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)?"error":"warning";

    std::fprintf(stderr, "vulkan_smoke: validation %s: %s\n", level,
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
    info.pfnUserCallback=onMessage;
    return info;
}

bool hasLayer(const char *name, uint32_t &specVersion)
{
    uint32_t count=0;

    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());
    for(const VkLayerProperties &layer:layers)
    {
        if(std::strcmp(layer.layerName, name) == 0)
        {
            specVersion=layer.specVersion;
            return true;
        }
    }
    return false;
}

//! A device that has what the renderer needs, and its graphics queue family.
struct Candidate
{
    VkPhysicalDevice device=VK_NULL_HANDLE;
    uint32_t queueFamily=0;
    VkPhysicalDeviceProperties properties{};
};

bool suitable(VkPhysicalDevice device, Candidate &candidate, bool print)
{
    VkPhysicalDeviceProperties properties;

    vkGetPhysicalDeviceProperties(device, &properties);

    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    VkPhysicalDeviceFeatures2 features{};
    features.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext=&features13;
    if(properties.apiVersion >= VK_API_VERSION_1_3)
        vkGetPhysicalDeviceFeatures2(device, &features);

    if(print)
        std::printf("vulkan_smoke: device %s, Vulkan %u.%u.%u, dynamicRendering %s, dualSrcBlend %s\n",
            properties.deviceName, VK_API_VERSION_MAJOR(properties.apiVersion),
            VK_API_VERSION_MINOR(properties.apiVersion), VK_API_VERSION_PATCH(properties.apiVersion),
            features13.dynamicRendering?"yes":"no", features.features.dualSrcBlend?"yes":"no");

    if(properties.apiVersion < VK_API_VERSION_1_3 || !features13.dynamicRendering ||
        !features.features.dualSrcBlend)
        return false;

    uint32_t count=0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());
    for(uint32_t i=0; i < count; ++i)
    {
        if(families[i].queueFlags&VK_QUEUE_GRAPHICS_BIT)
        {
            candidate.device=device;
            candidate.queueFamily=i;
            candidate.properties=properties;
            return true;
        }
    }
    return false;
}

//! The first suitable device of the instance, if any.
Candidate findDevice(VkInstance instance, bool print)
{
    Candidate candidate;
    uint32_t count=0;

    if(vkEnumeratePhysicalDevices(instance, &count, nullptr) != VK_SUCCESS)
        return candidate;
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());
    for(VkPhysicalDevice device:devices)
    {
        if(suitable(device, candidate, print))
            break;
    }
    return candidate;
}

//! Whether there is a device at all, asked of an instance with no layer and no
//! messenger: on an image without a driver the loader's own errors would reach
//! the messenger and abort before the line saying what is missing.
bool probeDevice()
{
    VkApplicationInfo app{};
    app.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.apiVersion=VK_API_VERSION_1_3;

    VkInstanceCreateInfo info{};
    info.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo=&app;

    VkInstance instance=VK_NULL_HANDLE;
    if(vkCreateInstance(&info, nullptr, &instance) != VK_SUCCESS)
        return false;
    volkLoadInstanceOnly(instance);

    const bool found=findDevice(instance, true).device != VK_NULL_HANDLE;

    vkDestroyInstance(instance, nullptr);
    return found;
}

VkShaderModule createShader(VkDevice device, const uint32_t *code, size_t bytes)
{
    VkShaderModuleCreateInfo info{};

    info.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize=bytes;
    info.pCode=code;

    VkShaderModule module=VK_NULL_HANDLE;
    if(vkCreateShaderModule(device, &info, nullptr, &module) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    return module;
}

//! A pipeline for dynamic rendering into one RGBA8 attachment, blending
//! SRC1_COLOR as the LCD text path will.
VkResult createPipeline(VkDevice device, VkShaderModule vert, VkShaderModule frag,
    VkPipelineLayout layout, VkPipeline &pipeline)
{
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module=vert;
    stages[0].pName="main";
    stages[1].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module=frag;
    stages[1].pName="main";

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo assembly{};
    assembly.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewport{};
    viewport.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount=1;
    viewport.scissorCount=1;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode=VK_POLYGON_MODE_FILL;
    raster.cullMode=VK_CULL_MODE_NONE;
    raster.lineWidth=1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.blendEnable=VK_TRUE;
    blendAttachment.srcColorBlendFactor=VK_BLEND_FACTOR_SRC1_COLOR;
    blendAttachment.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
    blendAttachment.colorBlendOp=VK_BLEND_OP_ADD;
    blendAttachment.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE;
    blendAttachment.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachment.alphaBlendOp=VK_BLEND_OP_ADD;
    blendAttachment.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|
        VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount=1;
    blend.pAttachments=&blendAttachment;

    const VkDynamicState dynamicStates[]={VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount=2;
    dynamic.pDynamicStates=dynamicStates;

    const VkFormat colorFormat=VK_FORMAT_R8G8B8A8_UNORM;
    VkPipelineRenderingCreateInfo rendering{};
    rendering.sType=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering.colorAttachmentCount=1;
    rendering.pColorAttachmentFormats=&colorFormat;

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
    info.pColorBlendState=&blend;
    info.pDynamicState=&dynamic;
    info.layout=layout;

    return vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);
}

}//namespace

int main()
{
    if(volkInitialize() != VK_SUCCESS)
        return fail("cannot load the Vulkan loader (libvulkan.so.1): is libvulkan1 installed? rebuild the image");

    if(!probeDevice())
        return fail("no Vulkan 1.3 device with dynamicRendering and dualSrcBlend: is "
            "mesa-vulkan-drivers installed? rebuild the image");

    uint32_t layerVersion=0;
    if(!hasLayer(ValidationLayer, layerVersion))
    {
        const char *path=std::getenv("VK_LAYER_PATH");
        std::fprintf(stderr, "vulkan_smoke: %s not found (VK_LAYER_PATH=%s): is the vcpkg feature "
            "\"vulkan\" installed? run it through ctest, which sets the layer path\n",
            ValidationLayer, path?path:"unset");
        return EXIT_FAILURE;
    }
    std::printf("vulkan_smoke: %s %u.%u.%u\n", ValidationLayer, VK_API_VERSION_MAJOR(layerVersion),
        VK_API_VERSION_MINOR(layerVersion), VK_API_VERSION_PATCH(layerVersion));

    VkApplicationInfo app{};
    app.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName="vulkan_smoke";
    app.pEngineName="gweni";
    app.apiVersion=VK_API_VERSION_1_3;

    const char *extensions[]={VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
    // chained, so creating and destroying the instance is validated too
    VkDebugUtilsMessengerCreateInfoEXT messenger=messengerInfo();

    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pNext=&messenger;
    instanceInfo.pApplicationInfo=&app;
    instanceInfo.enabledLayerCount=1;
    instanceInfo.ppEnabledLayerNames=&ValidationLayer;
    instanceInfo.enabledExtensionCount=1;
    instanceInfo.ppEnabledExtensionNames=extensions;

    VkInstance instance=VK_NULL_HANDLE;
    VkResult result=vkCreateInstance(&instanceInfo, nullptr, &instance);
    if(result != VK_SUCCESS)
        return failResult("vkCreateInstance with the validation layer", result);
    volkLoadInstanceOnly(instance);

    VkDebugUtilsMessengerEXT debugMessenger=VK_NULL_HANDLE;
    result=vkCreateDebugUtilsMessengerEXT(instance, &messenger, nullptr, &debugMessenger);
    if(result != VK_SUCCESS)
        return failResult("vkCreateDebugUtilsMessengerEXT", result);

    Candidate candidate=findDevice(instance, false);
    if(candidate.device == VK_NULL_HANDLE)
        return fail("the device found without the validation layer is not there with it");

    const float priority=1.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex=candidate.queueFamily;
    queueInfo.queueCount=1;
    queueInfo.pQueuePriorities=&priority;

    VkPhysicalDeviceVulkan13Features enable13{};
    enable13.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    enable13.dynamicRendering=VK_TRUE;
    VkPhysicalDeviceFeatures2 enable{};
    enable.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    enable.pNext=&enable13;
    enable.features.dualSrcBlend=VK_TRUE;

    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext=&enable;
    deviceInfo.queueCreateInfoCount=1;
    deviceInfo.pQueueCreateInfos=&queueInfo;

    VkDevice device=VK_NULL_HANDLE;
    result=vkCreateDevice(candidate.device, &deviceInfo, nullptr, &device);
    if(result != VK_SUCCESS)
        return failResult("vkCreateDevice", result);
    volkLoadDevice(device);

    VkShaderModule vert=createShader(device, smoke_vert, sizeof(smoke_vert));
    VkShaderModule frag=createShader(device, smoke_frag, sizeof(smoke_frag));
    if(vert == VK_NULL_HANDLE || frag == VK_NULL_HANDLE)
        return fail("vkCreateShaderModule failed for the embedded SPIR-V");

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    VkPipelineLayout layout=VK_NULL_HANDLE;
    result=vkCreatePipelineLayout(device, &layoutInfo, nullptr, &layout);
    if(result != VK_SUCCESS)
        return failResult("vkCreatePipelineLayout", result);

    VkPipeline pipeline=VK_NULL_HANDLE;
    result=createPipeline(device, vert, frag, layout, pipeline);
    if(result != VK_SUCCESS)
        return failResult("vkCreateGraphicsPipelines (dynamic rendering, dual-source blend)", result);

    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, layout, nullptr);
    vkDestroyShaderModule(device, frag, nullptr);
    vkDestroyShaderModule(device, vert, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    vkDestroyInstance(instance, nullptr);

    std::printf("vulkan_smoke: ok on %s\n", candidate.properties.deviceName);
    return EXIT_SUCCESS;
}
