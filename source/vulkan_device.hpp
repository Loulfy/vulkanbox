//
// Created by lorian on 10/02/2026.
//

#pragma once

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <set>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "logger.hpp"
#include "system.hpp"

using namespace cominou;

struct Color
{
    static constexpr std::array<float, 4> Black = { 0.f, 0.f, 0.f, 1.f };
    static constexpr std::array<float, 4> White = { 1.f, 1.f, 1.f, 1.f };
    static constexpr std::array<float, 4> Red = { 1.f, 0.f, 0.f, 1.f };
    static constexpr std::array<float, 4> Green = { 0.f, 1.f, 0.f, 1.f };
    static constexpr std::array<float, 4> Blue = { 0.f, 0.f, 1.f, 1.f };
    static constexpr std::array<float, 4> Gray = { 0.45f, 0.55f, 0.6f, 1.f };
    static constexpr std::array<float, 4> Magenta = { 1.f, 0.f, 1.f, 1.f };
    static constexpr std::array<std::array<float, 4>, 10> Palette = { { { 0.620f, 0.004f, 0.259f, 1.f },
                                                                        { 0.835f, 0.243f, 0.310f, 1.f },
                                                                        { 0.957f, 0.427f, 0.263f, 1.f },
                                                                        { 0.992f, 0.682f, 0.380f, 1.f },
                                                                        { 0.996f, 0.878f, 0.545f, 1.f },
                                                                        { 0.902f, 0.961f, 0.596f, 1.f },
                                                                        { 0.671f, 0.867f, 0.643f, 1.f },
                                                                        { 0.400f, 0.761f, 0.647f, 1.f },
                                                                        { 0.196f, 0.533f, 0.741f, 1.f },
                                                                        { 0.369f, 0.310f, 0.635f, 1.f } } };
};

struct ShaderModule
{
    fs::path path;
    std::string entryPoint = "main";
    vk::ShaderStageFlagBits stage = vk::ShaderStageFlagBits::eVertex;
};

struct GLFWwindow;
struct DeviceConfig
{
    bool m_debug = true;
    GLFWwindow* m_window = nullptr;
    std::vector<const char*> m_extensions;
};

/// @brief Summary of important vulkan features
struct VulkanFeatures
{
    uint32_t apiVersion = VK_API_VERSION_1_3;
    bool hostBuffer = false;
    bool rayTracing = false;
    bool memoryPriority = false;
    bool binaryPipeline = false;
    bool descriptorBuffer = false;
    bool mutableDescriptor = false;
    bool multiDrawIndirect = false;
    bool drawIndirectCount = false;
};

struct VulkanContext
{
    vk::Device device;
    vk::PhysicalDevice physicalDevice;
    VmaAllocator allocator = nullptr;
    vk::PipelineCache pipelineCache;
    vk::DescriptorSetLayout bindlessLayout;
    vk::DescriptorSetLayout constantLayout;
    uint32_t graphicsQueueFamily = UINT32_MAX;
    uint32_t transferQueueFamily = UINT32_MAX;
    VulkanFeatures features;
    vk::PhysicalDeviceDescriptorBufferPropertiesEXT descBufferProperties;
    //PipelineRegistry pipelineRegistry;
};

template<typename HandleType>
struct ResourceState
{
    HandleType m_handle;
    VmaAllocationInfo m_hostInfo = {};
    VmaAllocation m_allocation = nullptr;
    vk::PipelineStageFlags2 m_lastStage = vk::PipelineStageFlagBits2::eTopOfPipe;
    vk::AccessFlags2 m_currentAccess = vk::AccessFlagBits2::eNone;
    uint16_t m_srvIndex = UINT16_MAX;
    uint16_t m_uavIndex = UINT16_MAX;
    vk::Device m_device;

    void setDebugName(std::string_view debugName)
    {
        vk::DebugUtilsObjectNameInfoEXT nameInfo;
        nameInfo.setObjectType(m_handle.objectType);
        nameInfo.setPObjectName(debugName.data());
        nameInfo.setObjectHandle(std::bit_cast<uint64_t>(m_handle));
        m_device.setDebugUtilsObjectNameEXT(nameInfo);
    }
};

struct Texture : ResourceState<vk::Image>
{
    //vk::Image m_handle;
    vk::ImageCreateInfo m_info;
    vk::ImageLayout m_currentLayout = vk::ImageLayout::eUndefined;
    vk::UniqueImageView m_view;

    static constexpr vk::ImageSubresourceRange DefaultSub{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
    void generateImageView(vk::ImageSubresourceRange subresource = DefaultSub);
};

using TexturePtr = std::shared_ptr<Texture>;

struct Buffer : ResourceState<vk::Buffer>
{
    //vk::Buffer m_handle;
    vk::BufferCreateInfo m_info;
};

using BufferPtr = std::shared_ptr<Buffer>;

struct Pipeline
{
    vk::UniquePipeline m_handle;
    vk::UniquePipelineLayout m_pipelineLayout;
    vk::PipelineBindPoint m_bindPoint = vk::PipelineBindPoint::eGraphics;
};

using PipelinePtr = std::shared_ptr<Pipeline>;

struct Shader
{
    vk::ShaderStageFlagBits stage = {};
    vk::UniqueShaderModule shaderModule;
    vk::PushConstantRange pushConstant;
    std::string entryPoint;
};

struct GraphicsPipelineInfo
{
    void fillGraphicsPsoDesc();
    void fillShaderStages(const std::span<Shader>& shaders);
    void fillLayout(const vk::PipelineLayout& layout);
    void tryLoadPipelineBinaries(const std::string& name);
    vk::GraphicsPipelineCreateInfo& getCreateInfo()
    {
        return m_chain.get<vk::GraphicsPipelineCreateInfo>();
    }

    using PipelineChainInfo = vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo,
                                                 vk::PipelineCreateFlags2CreateInfoKHR, vk::PipelineBinaryInfoKHR>;
    PipelineChainInfo m_chain;
    vk::PipelineCreateFlags2CreateInfoKHR m_flags;
    std::vector<vk::Format> m_colorFormatAttachments;
    std::vector<vk::PipelineColorBlendAttachmentState> m_colorBlendAttachments;
    vk::PipelineInputAssemblyStateCreateInfo m_pia;
    std::vector<vk::VertexInputAttributeDescription> m_attributes;
    std::vector<vk::VertexInputBindingDescription> m_bindingDesc;
    std::vector<vk::PipelineShaderStageCreateInfo> m_shaderStages;
    vk::PipelineDepthStencilStateCreateInfo m_pds;
    vk::PipelineColorBlendStateCreateInfo m_pbs;
    vk::PipelineDynamicStateCreateInfo m_pdy;
    vk::PipelineVertexInputStateCreateInfo m_pvi;
    vk::PipelineMultisampleStateCreateInfo m_pm;
    vk::PipelineRasterizationStateCreateInfo m_pr;
    vk::PipelineViewportStateCreateInfo m_pv;
    std::vector<vk::PipelineBinaryKHR> m_pipelineBinaries;

    static constexpr std::initializer_list m_dynamicStates = { vk::DynamicState::eViewport,
                                                               vk::DynamicState::eScissor };
};

struct AttachmentOutput
{
    TexturePtr m_renderTarget;
    vk::AttachmentLoadOp m_loadOp = vk::AttachmentLoadOp::eDontCare;
    vk::ClearValue m_clearValue;
};

struct RenderingInfo
{
    std::vector<AttachmentOutput> colors;
    AttachmentOutput depth;
    vk::Rect2D viewport;
};

struct FrameContext
{
  public:
    friend class VulkanDevice;
    explicit FrameContext(VulkanContext& context);

    void transitionImage(const TexturePtr& res, vk::PipelineStageFlagBits2 newStage, vk::ImageLayout newLayout);
    void transitionBuffer(const BufferPtr& res, vk::PipelineStageFlagBits2 newStage, vk::AccessFlagBits2 newAccess);
    // void memoryBarrier(const VKResource& res);
    void flushBarriers();

    void begin();
    void commit();
    void waitUntilCompleted();

    // Commands
    void endDebugEvent() const;
    void beginDebugEvent(const char* label, const float* color) const;
    void clearBackBufferColor(std::span<const float> color);

    void bindPipeline(const PipelinePtr& pipeline);
    void pushConstant(vk::ShaderStageFlags stage, const void* data, uint8_t size) const;
    void setScissor(const vk::Rect2D& rect) const;
    void endRenderPass() const;
    void beginRenderPass(const RenderingInfo& info);
    void drawPrimitives(uint32_t vertexCount, uint32_t firstVertex) const;
    void drawIndexedPrimitives(uint32_t indexCount, uint32_t firstIndex, int32_t firstVertex,
                               uint32_t instanceId) const;

    [[nodiscard]] TexturePtr getBackBuffer() const
    {
        return m_backBuffer;
    }

  private:
    const VulkanContext& m_context;
    vk::UniqueCommandPool m_allocator;
    vk::CommandBuffer m_cmdBuf;
    vk::Queue m_queue;
    vk::UniqueFence m_fence;
    vk::UniqueSemaphore m_presentSemaphore;
    vk::UniqueSemaphore m_acquireSemaphore;
    uint64_t m_fenceValue = 0;
    uint64_t m_waitFenceValue = 0;
    bool m_committed = false;

    TexturePtr m_backBuffer;
    std::vector<vk::ImageMemoryBarrier2KHR> m_imageBarriers;
    std::vector<vk::BufferMemoryBarrier2KHR> m_bufferBarriers;

    PipelinePtr m_pipeline;
};

class VulkanDevice
{
  public:
    ~VulkanDevice();
    explicit VulkanDevice(const DeviceConfig& config);

    Buffer createBuffer();

    void createSwapChain(GLFWwindow* window);
    void resizeSwapChain(uint32_t width, uint32_t height);

    FrameContext& acquireNextFrame();
    void present(FrameContext& frame);

    void waitQueueIdle(vk::Queue& queue);
    void waitIdle() const;

    PipelinePtr createGraphicsPipeline();
    Shader createShader(const ShaderModule& shaderModule);

    static vk::ImageAspectFlags guessImageAspectFlags(vk::Format format, bool stencil);
    static void shaderAutoCompile();

  private:
    static constexpr uint32_t kMaxPendingFrames = 3;
    vk::UniqueInstance m_instance;
    vk::PhysicalDevice m_physicalDevice;
    uint32_t m_graphicsQueueFamily = UINT32_MAX;
    uint32_t m_transferQueueFamily = UINT32_MAX;
    vk::UniqueDevice m_device;
    vk::UniquePipelineCache m_pipelineCache;
    VmaAllocator m_allocator = nullptr;
    VulkanContext m_context;

    vk::Queue m_graphicsQueue;
    vk::Queue m_transferQueue;

    std::array<TexturePtr, kMaxPendingFrames> m_frameImages;
    std::vector<FrameContext> m_frameContext;
    vk::SwapchainCreateInfoKHR m_swapChainCreateInfo;
    vk::UniqueSurfaceKHR m_surface;
    vk::UniqueSwapchainKHR m_swapChain;
    uint32_t m_swapChainIndex = 0;

    uint32_t m_maxFramesInFlight = kMaxPendingFrames;
    uint32_t m_currentFrameIndex = 0;
};
