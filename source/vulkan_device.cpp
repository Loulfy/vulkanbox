//
// Created by lorian on 10/02/2026.
//

#define VMA_IMPLEMENTATION
#include "vulkan_device.hpp"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

VulkanDevice::~VulkanDevice()
{
    vmaDestroyAllocator(m_allocator);
}

// clang-format off
using PropertiesChain = vk::StructureChain<vk::PhysicalDeviceProperties2,
                    vk::PhysicalDeviceSubgroupProperties,
                    vk::PhysicalDeviceExternalMemoryHostPropertiesEXT,
                    vk::PhysicalDeviceDescriptorBufferPropertiesEXT>;

using FeaturesChain = vk::StructureChain<vk::PhysicalDeviceFeatures2,
                    vk::PhysicalDeviceVulkan11Features,
                    vk::PhysicalDeviceVulkan12Features,
                    vk::PhysicalDeviceVulkan13Features,
                    vk::PhysicalDeviceSynchronization2Features,
                    vk::PhysicalDeviceDynamicRenderingFeatures,
                    vk::PhysicalDeviceMutableDescriptorTypeFeaturesEXT,
                    vk::PhysicalDeviceDescriptorBufferFeaturesEXT,
                    vk::PhysicalDeviceRayQueryFeaturesKHR,
                    vk::PhysicalDeviceRayTracingPipelineFeaturesKHR,
                    vk::PhysicalDeviceAccelerationStructureFeaturesKHR>;

using InfoChain = vk::StructureChain<vk::DeviceCreateInfo,
                    vk::PhysicalDeviceVulkan11Features,
                    vk::PhysicalDeviceVulkan12Features,
                    vk::PhysicalDeviceVulkan13Features,
                    vk::PhysicalDeviceSynchronization2Features,
                    vk::PhysicalDeviceDynamicRenderingFeatures,
                    vk::PhysicalDeviceMutableDescriptorTypeFeaturesEXT,
                    vk::PhysicalDeviceDescriptorBufferFeaturesEXT,
                    vk::PhysicalDeviceRayQueryFeaturesKHR,
                    vk::PhysicalDeviceRayTracingPipelineFeaturesKHR,
                    vk::PhysicalDeviceAccelerationStructureFeaturesKHR>;
// clang-format on

VulkanFeatures getSupportedFeatures(const std::set<std::string>& supportedExtensionSet, const PropertiesChain& p,
                                    const FeaturesChain& f)
{
    VulkanFeatures features;
    features.apiVersion = p.get<vk::PhysicalDeviceProperties2>().properties.apiVersion;
    uint64_t nonCoherentAtomSize = p.get<vk::PhysicalDeviceProperties2>().properties.limits.nonCoherentAtomSize;
    features.drawIndirectCount = f.get<vk::PhysicalDeviceVulkan12Features>().drawIndirectCount;
    features.multiDrawIndirect = f.get<vk::PhysicalDeviceFeatures2>().features.multiDrawIndirect;

    if (supportedExtensionSet.contains(VK_KHR_RAY_QUERY_EXTENSION_NAME))
        features.rayTracing = true;
    if (supportedExtensionSet.contains(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME))
        features.hostBuffer = true;
    if (supportedExtensionSet.contains(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME))
        features.memoryPriority = true;
    if (supportedExtensionSet.contains(VK_KHR_PIPELINE_BINARY_EXTENSION_NAME))
        features.binaryPipeline = true;
    if (supportedExtensionSet.contains(VK_EXT_MUTABLE_DESCRIPTOR_TYPE_EXTENSION_NAME))
        features.mutableDescriptor = true;
    if (supportedExtensionSet.contains(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME))
        features.descriptorBuffer = true;

    return features;
}

static void populateRequiredDeviceExtensions(const VulkanFeatures& features, std::vector<const char*>& deviceExtensions)
{
    // Enable Dynamic Rendering and Barrier2 for Vulkan 1.2
    if (VK_VERSION_MINOR(features.apiVersion) == 2)
    {
        deviceExtensions.emplace_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
        deviceExtensions.emplace_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    }

    // Enable RayTracing
    if (features.rayTracing)
    {
        deviceExtensions.emplace_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        deviceExtensions.emplace_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        deviceExtensions.emplace_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
        deviceExtensions.emplace_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
    }

    // Enable Pipeline library
    if (features.binaryPipeline)
    {
        deviceExtensions.emplace_back(VK_KHR_MAINTENANCE_5_EXTENSION_NAME);
        deviceExtensions.emplace_back(VK_KHR_PIPELINE_BINARY_EXTENSION_NAME);
    }

    // Enable VMA priority
    if (features.memoryPriority)
        deviceExtensions.emplace_back(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME);

    // Malloc staging buffer
    if (features.hostBuffer)
    {
        deviceExtensions.emplace_back(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME);
        deviceExtensions.emplace_back(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
    }

    // Enable Mutable Descriptor
    if (features.mutableDescriptor)
        deviceExtensions.emplace_back(VK_EXT_MUTABLE_DESCRIPTOR_TYPE_EXTENSION_NAME);

    // Enable Descriptor Buffer
    if (features.descriptorBuffer)
        deviceExtensions.emplace_back(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME);
}

static void printVulkanFeatures(const VulkanFeatures& features, const PropertiesChain& p, const FeaturesChain& f)
{
    uint32_t major = VK_VERSION_MAJOR(features.apiVersion);
    uint32_t minor = VK_VERSION_MINOR(features.apiVersion);
    uint32_t patch = VK_VERSION_PATCH(features.apiVersion);

    log::info("API: {}.{}.{}", major, minor, patch);
    auto subgroupProps = p.get<vk::PhysicalDeviceSubgroupProperties>();
    auto memoryProps = p.get<vk::PhysicalDeviceExternalMemoryHostPropertiesEXT>();
    log::info("RayTracing: {}", features.rayTracing);
    log::info("HostBuffer: {}", features.hostBuffer);
    log::info("PipelineBinary: {}", features.binaryPipeline);
    log::info("DescriptorBuffer: {}", features.descriptorBuffer);
    log::info("MutableDescriptor: {}", features.mutableDescriptor);
    log::info("MultiDrawIndirect: {}", features.multiDrawIndirect);
    log::info("DrawIndirectCount: {}", features.drawIndirectCount);
    log::info("SubgroupSize: {}", subgroupProps.subgroupSize);
    log::info("MinImportedHostPointerAlignment: {}", memoryProps.minImportedHostPointerAlignment);
}

static void unlinkUnsupportedFeatures(InfoChain& createInfoChain, const VulkanFeatures& features)
{
    const uint32_t minor = VK_VERSION_MINOR(features.apiVersion);

    if (minor == 2)
        createInfoChain.unlink<vk::PhysicalDeviceVulkan13Features>();
    if (minor == 3 || minor == 4)
    {
        createInfoChain.unlink<vk::PhysicalDeviceSynchronization2Features>();
        createInfoChain.unlink<vk::PhysicalDeviceDynamicRenderingFeatures>();
    }

    if (!features.mutableDescriptor)
        createInfoChain.unlink<vk::PhysicalDeviceMutableDescriptorTypeFeaturesEXT>();
    if (!features.descriptorBuffer)
        createInfoChain.unlink<vk::PhysicalDeviceDescriptorBufferFeaturesEXT>();
    if (!features.rayTracing)
    {
        createInfoChain.unlink<vk::PhysicalDeviceRayQueryFeaturesKHR>();
        createInfoChain.unlink<vk::PhysicalDeviceRayTracingPipelineFeaturesKHR>();
        createInfoChain.unlink<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>();
    }
}

static void printAnalyzeMemoryHeaps(const vk::PhysicalDevice& physicalDevice)
{
    vk::PhysicalDeviceMemoryProperties memProps = physicalDevice.getMemoryProperties();
    for (uint32_t h = 0; h < memProps.memoryHeapCount; h++)
    {
        const VkMemoryHeap heap = memProps.memoryHeaps[h];

        vk::MemoryPropertyFlags flags;
        for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
        {
            const vk::MemoryType type = memProps.memoryTypes[i];
            if (type.heapIndex == h)
                flags = std::max(flags, type.propertyFlags);
        }

        if ((flags & vk::MemoryPropertyFlagBits::eHostVisible) && (flags & vk::MemoryPropertyFlagBits::eDeviceLocal))
            log::info("Heap {}: {:5} MB (BAR) -> {}", h, heap.size / 1024 / 1024, vk::to_string(flags));
        else if (flags & vk::MemoryPropertyFlagBits::eDeviceLocal)
            log::info("Heap {}: {:5} MB (GPU) -> {}", h, heap.size / 1024 / 1024, vk::to_string(flags));
        else
            log::info("Heap {}: {:5} MB (CPU) -> {}", h, heap.size / 1024 / 1024, vk::to_string(flags));
    }
}

VulkanDevice::VulkanDevice(const DeviceConfig& config)
{
    static const vk::detail::DynamicLoader dl;
    const auto vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    vk::ResultValueType<uint32_t>::type sdkVersion = vk::enumerateInstanceVersion();
    uint32_t major = VK_VERSION_MAJOR(sdkVersion);
    uint32_t minor = VK_VERSION_MINOR(sdkVersion);
    uint32_t patch = VK_VERSION_PATCH(sdkVersion);

    log::info("Vulkan SDK {}.{}.{}", major, minor, patch);

    static constexpr std::initializer_list layers = { "VK_LAYER_KHRONOS_validation" };

    vk::InstanceCreateInfo instInfo;

    std::vector<const char*> extensions;
    extensions.reserve(14);
    if (config.m_debug)
    {
        instInfo.setPEnabledLayerNames(layers);
        extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    extensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    extensions.insert(extensions.end(), config.m_extensions.begin(), config.m_extensions.end());

    vk::ApplicationInfo appInfo;
    appInfo.apiVersion = VK_API_VERSION_1_3;
    appInfo.pEngineName = "Cominou";

    instInfo.pApplicationInfo = &appInfo;
    instInfo.setPEnabledExtensionNames(extensions);
    m_instance = vk::createInstanceUnique(instInfo);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(m_instance.get());

    // Pick Discrete GPU
    std::vector<vk::PhysicalDevice> physicalDeviceList = m_instance->enumeratePhysicalDevices();

    VkPhysicalDeviceProperties properties;
    for (const vk::PhysicalDevice& phyDev : physicalDeviceList)
    {
        vkGetPhysicalDeviceProperties(phyDev, &properties);
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            m_physicalDevice = phyDev;
            break;
        }
    }

    if (m_physicalDevice == nullptr)
        log::fatalError("No discrete GPU available");

    log::info("GPU: {}", properties.deviceName);

    std::vector<const char*> deviceExtensions = {
        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
        // VULKAN MEMORY ALLOCATOR
        VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
        VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
        // SWAP CHAIN
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    uint32_t extPropertiesCount = 0;
    vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extPropertiesCount, nullptr);
    std::vector<VkExtensionProperties> extensionPropertyList(extPropertiesCount);
    vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extPropertiesCount, extensionPropertyList.data());

    std::set<std::string> supportedExtensionSet;
    for (const VkExtensionProperties& devExt : extensionPropertyList)
        supportedExtensionSet.insert(devExt.extensionName);

    for (const char* devExt : deviceExtensions)
    {
        if (!supportedExtensionSet.contains(devExt))
        {
            std::string fatal("Mandatory Device Extension Not Supported: ");
            fatal += devExt;
            log::fatalError(fatal);
        }
    }

    // Device Properties
    PropertiesChain pp;
    m_physicalDevice.getProperties2(&pp.get<vk::PhysicalDeviceProperties2>());

    // Device Features
    FeaturesChain ff;
    m_physicalDevice.getFeatures2(&ff.get<vk::PhysicalDeviceFeatures2>());
    const vk::PhysicalDeviceFeatures& features = ff.get<vk::PhysicalDeviceFeatures2>().features;

    VulkanFeatures vulkanFeatures = getSupportedFeatures(supportedExtensionSet, pp, ff);
    vulkanFeatures.hostBuffer = false;
    vulkanFeatures.binaryPipeline = false;
    populateRequiredDeviceExtensions(vulkanFeatures, deviceExtensions);
    printAnalyzeMemoryHeaps(m_physicalDevice);
    printVulkanFeatures(vulkanFeatures, pp, ff);

    // Find Graphics Queue
    const std::vector<vk::QueueFamilyProperties> queueFamilies = m_physicalDevice.getQueueFamilyProperties();
    for (uint32_t queueIndex = 0; queueIndex < queueFamilies.size(); ++queueIndex)
    {
        const vk::QueueFamilyProperties& queueFamily = queueFamilies[queueIndex];
        if (queueFamily.queueCount > 0 && queueFamily.queueFlags & vk::QueueFlagBits::eGraphics)
        {
            m_graphicsQueueFamily = queueIndex;
            break;
        }
    }

    // Find Transfer Queue (for parallel command)
    for (uint32_t queueIndex = 0; queueIndex < queueFamilies.size(); ++queueIndex)
    {
        const vk::QueueFamilyProperties& queueFamily = queueFamilies[queueIndex];
        if (queueFamily.queueCount > 0 && queueFamily.queueFlags & vk::QueueFlagBits::eTransfer &&
            m_graphicsQueueFamily != queueIndex)
        {
            m_transferQueueFamily = queueIndex;
            break;
        }
    }

    if (m_transferQueueFamily == UINT32_MAX)
        log::fatalError("No transfer queue available");

    // Create queues
    float queuePriority = 1.0f;
    std::initializer_list<vk::DeviceQueueCreateInfo> queueCreateInfos = {
        { {}, m_graphicsQueueFamily, 1, &queuePriority },
        { {}, m_transferQueueFamily, 1, &queuePriority },
    };

    for (const vk::DeviceQueueCreateInfo& q : queueCreateInfos)
        log::info("Queue Family {}: {}", q.queueFamilyIndex,
                  vk::to_string(queueFamilies[q.queueFamilyIndex].queueFlags));

    // Create Device
    vk::DeviceCreateInfo deviceInfo;
    deviceInfo.setQueueCreateInfos(queueCreateInfos);
    deviceInfo.setPEnabledExtensionNames(deviceExtensions);
    deviceInfo.setPEnabledFeatures(&features);

    vk::PhysicalDeviceMutableDescriptorTypeFeaturesEXT mutableFeature;
    mutableFeature.setMutableDescriptorType(true);

    vk::PhysicalDeviceDescriptorBufferFeaturesEXT descriptorFeature;
    descriptorFeature.setDescriptorBuffer(true);

    vk::PhysicalDevicePipelineBinaryFeaturesKHR pipelineFeature;
    pipelineFeature.setPipelineBinaries(true);

    vk::PhysicalDeviceSynchronization2Features sync2Features;
    sync2Features.setSynchronization2(true);

    vk::PhysicalDeviceDynamicRenderingFeatures dynRenderFeature;
    dynRenderFeature.setDynamicRendering(true);

    vk::PhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures;
    rayQueryFeatures.setRayQuery(true);

    vk::PhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures;
    rayTracingPipelineFeatures.setRayTracingPipeline(true);

    vk::PhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures;
    accelerationStructureFeatures.setAccelerationStructure(true);

    // clang-format off
    InfoChain createInfoChain(deviceInfo,
        ff.get<vk::PhysicalDeviceVulkan11Features>(),
        ff.get<vk::PhysicalDeviceVulkan12Features>(),
        ff.get<vk::PhysicalDeviceVulkan13Features>(),
        sync2Features,
        dynRenderFeature,
        mutableFeature,
        descriptorFeature,
        rayQueryFeatures,
        rayTracingPipelineFeatures,
        accelerationStructureFeatures);
    // clang-format on

    unlinkUnsupportedFeatures(createInfoChain, vulkanFeatures);

    m_device = m_physicalDevice.createDeviceUnique(createInfoChain.get<vk::DeviceCreateInfo>());
    VULKAN_HPP_DEFAULT_DISPATCHER.init(m_device.get());

    m_pipelineCache = m_device->createPipelineCacheUnique({});

    // Create Memory Allocator
    VmaAllocatorCreateInfo allocatorCreateInfo = {};
    allocatorCreateInfo.vulkanApiVersion = appInfo.apiVersion;
    allocatorCreateInfo.device = m_device.get();
    allocatorCreateInfo.instance = m_instance.get();
    allocatorCreateInfo.physicalDevice = m_physicalDevice;
    allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;
    allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;

    vmaCreateAllocator(&allocatorCreateInfo, &m_allocator);

    m_graphicsQueue = m_device->getQueue(m_graphicsQueueFamily, 0);
    m_transferQueue = m_device->getQueue(m_transferQueueFamily, 0);

    m_context.device = m_device.get();
    m_context.descBufferProperties = pp.get<vk::PhysicalDeviceDescriptorBufferPropertiesEXT>();
    // m_context.bindlessLayout = m_bindlessLayout.get();
    // m_context.constantLayout = m_constantLayout.get();
    m_context.graphicsQueueFamily = m_graphicsQueueFamily;
    m_context.transferQueueFamily = m_transferQueueFamily;
    m_context.allocator = m_allocator;

    m_frameContext.reserve(kMaxPendingFrames);
    for (int i = 0; i < kMaxPendingFrames; ++i)
        m_frameContext.emplace_back(m_context);

    createSwapChain(config.m_window);
}

void VulkanDevice::waitQueueIdle(vk::Queue& queue)
{
    queue.waitIdle();
}

void VulkanDevice::waitIdle() const
{
    /*for (const vk::Queue& q : context.m_queues)
        if (q)
            q.waitIdle();*/

    m_graphicsQueue.waitIdle();
    m_transferQueue.waitIdle();
}