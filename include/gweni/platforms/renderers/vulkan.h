/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */
#ifndef _gweni_renders_vulkan_h_
#define _gweni_renders_vulkan_h_


#include <gweni/platforms/renderers/slotRenderer.h>

#include <volk.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace gweni
{
namespace renderer
{

//! The Vulkan objects a renderer::Vulkan draws with, made by the application
//! (createVulkanDevice, or its own) and outliving every renderer and target on it.
struct VulkanDevice
{
    VkInstance instance=VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice=VK_NULL_HANDLE;
    VkDevice device=VK_NULL_HANDLE;
    //! a graphics queue of queueFamily, which every renderer on the device submits to
    VkQueue queue=VK_NULL_HANDLE;
    uint32_t queueFamily=0;
    //! the device was created with the dualSrcBlend feature, which LCD text needs
    bool dualSourceBlend=false;
    //! createVulkanDevice's validation messenger, VK_NULL_HANDLE if none
    VkDebugUtilsMessengerEXT messenger=VK_NULL_HANDLE;
};

struct VulkanDeviceOptions
{
    //! the application name the instance is created with
    std::string applicationName="gweni";
    //! Enables VK_LAYER_KHRONOS_validation and a debug-utils messenger that prints
    //! every validation warning and error on stderr and aborts, as ASAN's
    //! abort_on_error does. Without the layer createVulkanDevice fails.
    bool validation=false;
    //! instance and device extensions the caller needs beyond these (a window's
    //! surface and swapchain)
    std::vector<const char *> instanceExtensions;
    std::vector<const char *> deviceExtensions;
};

//! Loads the Vulkan loader (volk), makes an instance (Vulkan 1.3), picks the first
//! device with Vulkan 1.3, dynamicRendering, shaderDemoteToHelperInvocation (a
//! discard in the shaders) and a graphics queue, and makes a device on it with
//! those (and dualSrcBlend where it has it). False,
//! with error saying what is missing, if it cannot; device then holds nothing.
GWENI_EXPORT bool createVulkanDevice(VulkanDevice &device, const VulkanDeviceOptions &options,
    std::string &error);
//! Destroys what createVulkanDevice made, once everything made on it is gone.
GWENI_EXPORT void destroyVulkanDevice(VulkanDevice &device);

//! An image a renderer::Vulkan draws into: a colour image and its depth buffer
//! of one size. It is the application's, as an EGL context's framebuffer is: any
//! renderer on the device may draw into it, one after another, and read it back.
//! Between frames the colour image is in COLOR_ATTACHMENT_OPTIMAL and the depth
//! in DEPTH_STENCIL_ATTACHMENT_OPTIMAL; every renderer keeps them so.
//!
//! Its rows are bottom-up, as a GL framebuffer's are: the view's top row is the
//! image's last. The renderers draw in GL's window coordinates (no y flip), so
//! every triangle covers, interpolates and differentiates as on GL, and
//! readPixels turns the rows over as OpenGLCore's does. Shown in a window it
//! needs a flip (a blit), not a renderer that draws the other way up.
class GWENI_EXPORT VulkanTarget
{
public:
    VulkanTarget(const VulkanDevice &device);
    //! Waits for the queue, then destroys the images.
    ~VulkanTarget();

    VulkanTarget(const VulkanTarget &)=delete;
    VulkanTarget &operator=(const VulkanTarget &)=delete;

    //! (Re)makes the images at width x height (RGBA8, and a 24- or 32-bit depth),
    //! the colour cleared to transparent black and the depth to the far plane.
    bool create(int width, int height);

    int width() const { return m_width; }
    int height() const { return m_height; }
    VkFormat colorFormat() const { return m_colorFormat; }
    VkFormat depthFormat() const { return m_depthFormat; }
    VkImage colorImage() const { return m_color.image; }
    VkImageView colorView() const { return m_color.view; }
    VkImage depthImage() const { return m_depth.image; }
    VkImageView depthView() const { return m_depth.view; }

private:
    struct Image
    {
        VkImage image=VK_NULL_HANDLE;
        VkDeviceMemory memory=VK_NULL_HANDLE;
        VkImageView view=VK_NULL_HANDLE;
    };

    void destroy();

    const VulkanDevice &m_device;
    int m_width=0;
    int m_height=0;
    VkFormat m_colorFormat=VK_FORMAT_R8G8B8A8_UNORM;
    VkFormat m_depthFormat=VK_FORMAT_UNDEFINED;
    Image m_color;
    Image m_depth;
};

//
//! Renderer for [Vulkan](https://www.vulkan.org/) 1.3: the Vulkan backend of
//! SlotRenderer, drawing with dynamic rendering into a VulkanTarget.
//
//! The slots are copied from SlotRenderer's mirrors into device-local vertex
//! buffers through a host-visible staging buffer per frame in flight, and drawn
//! one chunk at a time through two static 16-bit quad index buffers, each chunk
//! with its vertex buffer bound at its first slot. The clip boxes are 2D
//! R16G16B16A16_UINT images, kClipColumns boxes a row, as on GL; the image units
//! one array of 8 combined image samplers; the projection and slot base push
//! constants. The shader bodies are OpenGLCore's (shaders/*.glsl), compiled to
//! SPIR-V at build time behind a Vulkan prologue.
//
//! Text is not drawn yet: the atlases are made, uploaded and read back, the text
//! draws record nothing (#372).
//
class GWENI_EXPORT Vulkan: public SlotRenderer
{
public:

    //! Draws on device into target, which both outlive it.
    Vulkan(ResourcePaths &paths, const gweni::Rect &viewRect, const VulkanDevice &device,
        VulkanTarget &target);
    //! Waits for the queue, then destroys everything it made.
    virtual ~Vulkan();

    void init() override;
    void setViewRect(const gweni::Rect &viewRect) override;

    //! Draws into another target of the same formats from the next frame on.
    void setTarget(VulkanTarget &target);
    VulkanTarget &target() const { return *m_target; }

    void clearTarget(const gweni::Color &color) override;
    void readPixels(int width, int height, std::vector<uint8_t> &pixels) override;
    bool textureExists(TextureHandle handle) const override;
    bool textureChannelSizes(TextureHandle handle, int bits[4]) const override;
    bool deviceError() override;

    const char *lcdTextUnavailable() const override;

    //! Frames whose commands may be on the device at once: a frame waits for the
    //! one this many before it, and reuses its staging and command buffers.
    static constexpr size_t FramesInFlight=2;

protected:

    // SlotRenderer's hooks
    void beginFrame() override;
    void allocateVertexBuffer(SlotPath path, size_t bytes) override;
    void uploadVertexRange(SlotPath path, size_t offset, size_t bytes, const void *data) override;
    size_t maxClipBoxRows() const override;
    void allocateClipBoxes(SlotPath path, size_t rows) override;
    void uploadClipBoxRows(SlotPath path, size_t x, size_t y, size_t width, size_t height,
        const Box *boxes) override;
    TextureHandle createAtlasTexture(const texture_atlas_t &atlas) override;
    void uploadAtlasRows(TextureHandle texture, const texture_atlas_t &atlas, size_t firstRow, size_t rows) override;
    void destroyAtlasTexture(TextureHandle texture) override;
    TextureHandle createImageTexture(const SlotTextureData &image, bool rgba) override;
    void destroyImageTexture(TextureHandle texture) override;
    bool readTextureRgba(TextureHandle texture, int width, int height, std::vector<uint8_t> &rgba) override;
    void beginPrimitiveDraws() override;
    void drawPrimitiveChunk(size_t chunk, size_t firstSlot, size_t slots) override;
    void endPrimitiveDraws() override;
    void beginTextDraws() override;
    void beginTextAtlas(const FontData &font) override;
    void drawTextChunk(const TextChunkDraw &draw) override;
    void endTextDraws() override;
    void submitFrame() override;
    void indexChunkSlotsChanged() override;

private:

    struct Buffer
    {
        VkBuffer buffer=VK_NULL_HANDLE;
        VkDeviceMemory memory=VK_NULL_HANDLE;
        VkDeviceSize size=0;
        void *mapped=nullptr; //!< host-visible ones, mapped for their life
    };

    //! An atlas, image or clip-box table: a sampled 2D image and where it is.
    struct Texture
    {
        VkImage image=VK_NULL_HANDLE;
        VkDeviceMemory memory=VK_NULL_HANDLE;
        VkImageView view=VK_NULL_HANDLE;
        VkFormat format=VK_FORMAT_UNDEFINED;
        uint32_t width=0;
        uint32_t height=0;
        uint32_t mipLevels=1;
        //! bytes an upload's texel takes in the mirror (an atlas's depth) and on the device
        uint32_t sourceDepth=4;
        uint32_t deviceDepth=4;
        VkImageLayout layout=VK_IMAGE_LAYOUT_UNDEFINED;
        //! sampled in the draws (SHADER_READ_ONLY_OPTIMAL between frames)
        bool sampled=true;
    };

    //! What one frame in flight owns: its command buffers (the uploads, then the
    //! draws, submitted together), the fence they signal and the staging memory
    //! the uploads are copied from.
    struct Frame
    {
        VkCommandBuffer transfer=VK_NULL_HANDLE;
        VkCommandBuffer draw=VK_NULL_HANDLE;
        VkFence fence=VK_NULL_HANDLE;
        bool submitted=false;
        //! host-visible blocks, used in order; all but the largest freed once the
        //! frame is done with them
        std::vector<Buffer> staging;
        size_t stagingBlock=0;
        VkDeviceSize stagingUsed=0;
        VkDescriptorSet primitiveSet=VK_NULL_HANDLE;
    };

    struct PrimitivePush
    {
        float projection[16];
        int32_t slotBase;
    };

    VkDevice device() const { return m_device.device; }
    //! Records a VkResult that is an error (deviceError()), and returns whether it succeeded.
    bool check(VkResult result, const char *what);

    uint32_t memoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) const;
    bool createBuffer(Buffer &buffer, VkDeviceSize size, VkBufferUsageFlags usage, bool hostVisible);
    void destroyBuffer(Buffer &buffer);
    bool createTexture(Texture &texture, uint32_t width, uint32_t height, VkFormat format,
        uint32_t mipLevels, VkImageUsageFlags usage);
    void destroyTexture(Texture &texture);
    VkShaderModule createShader(const uint32_t *code, size_t bytes);

    //! Records into a command buffer of its own, submits it and waits for it.
    template<typename Record>
    void immediate(Record &&record);

    //! The frame being recorded, begun (its fence waited, its buffers reset) if none is.
    Frame &frame();
    //! Copies bytes into the frame's staging memory; the buffer and offset to copy from.
    std::pair<VkBuffer, VkDeviceSize> stage(const void *data, size_t bytes, size_t alignment=16);
    //! A Texture this frame's uploads write: TRANSFER_DST_OPTIMAL from its first one,
    //! made SHADER_READ_ONLY_OPTIMAL again when the frame is submitted.
    void beginTextureWrite(Texture &texture);
    //! Begins the target's rendering in the frame's draw commands, if it is not.
    void beginRendering();
    void endRendering();
    //! Submits what the frame recorded, if anything.
    void submit();

    //! The primitive slots' pipeline and its layout, for the target's formats.
    bool createPipelines();
    void destroyPipelines();

    Texture *texture(TextureHandle handle);
    const Texture *texture(TextureHandle handle) const;
    TextureHandle addTexture(Texture &&texture);
    void destroyHandle(TextureHandle handle);

    const VulkanDevice &m_device;
    VulkanTarget *m_target;

    //! the device's properties and the formats init() chose
    VkPhysicalDeviceProperties m_properties{};
    VkPhysicalDeviceMemoryProperties m_memoryProperties{};
    VkFormat m_positionFormat=VK_FORMAT_R16G16B16_SINT;

    VkCommandPool m_commandPool=VK_NULL_HANDLE;
    Frame m_frames[FramesInFlight];
    size_t m_frameIndex=0;
    bool m_recording=false;
    bool m_rendering=false;
    //! the Textures this frame's uploads wrote, made sampled again in submit()
    std::vector<TextureHandle> m_writtenTextures;

    VkDescriptorSetLayout m_primitiveSetLayout=VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool=VK_NULL_HANDLE;
    VkPipelineLayout m_primitiveLayout=VK_NULL_HANDLE;
    VkPipeline m_primitivePipeline=VK_NULL_HANDLE;
    VkFormat m_pipelineColorFormat=VK_FORMAT_UNDEFINED;
    VkFormat m_pipelineDepthFormat=VK_FORMAT_UNDEFINED;

    VkSampler m_imageSampler=VK_NULL_HANDLE;   //!< linear, mipmapped, clamped: the images
    VkSampler m_atlasSampler=VK_NULL_HANDLE;   //!< linear, clamped: the font atlases
    VkSampler m_clipSampler=VK_NULL_HANDLE;    //!< nearest: the clip boxes (integer texels)

    //! kSlotsPerChunk quads of indexes each, as OpenGLCore's: the primitives' zigzag
    //! and freetype-gl's glyph order
    Buffer m_quadIndexBuffer;
    Buffer m_textQuadIndexBuffer;
    Buffer m_vertexBuffer;
    Buffer m_textVertexBuffer;
    //! the clip-box tables, handles into m_textures
    TextureHandle m_clipTexture=0;
    TextureHandle m_textClipTexture=0;
    //! what an image unit with no texture samples: one opaque black texel, as GL
    //! samples texture 0
    TextureHandle m_blackTexture=0;

    //! every Texture by handle; 0 is none
    std::unordered_map<TextureHandle, Texture> m_textures;
    TextureHandle m_nextHandle=1;

    PrimitivePush m_push{};
    bool m_error=false;
    bool m_warnedText=false;
};


}//namespace renderer
}//namespace gweni

#endif //_gweni_renders_vulkan_h_
