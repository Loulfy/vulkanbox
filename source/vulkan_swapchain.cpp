//
// Created by lorian on 14/02/2026.
//

#include "vulkan_device.hpp"

#define GLFW_INCLUDE_NONE // Do not include any OpenGL/Vulkan headers
#include <GLFW/glfw3.h>

static vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes,
                                                bool vSync)
{
    for (const auto& availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == vk::PresentModeKHR::eFifo && vSync)
            return availablePresentMode;
        if (availablePresentMode == vk::PresentModeKHR::eMailbox && !vSync)
            return availablePresentMode;
    }
    return vk::PresentModeKHR::eImmediate;
}

static vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
{
    if (availableFormats.size() == 1 && availableFormats[0].format == vk::Format::eUndefined)
    {
        return { vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };
    }

    for (const auto& format : availableFormats)
    {
        if ((format.format == vk::Format::eR8G8B8A8Unorm || format.format == vk::Format::eB8G8R8A8Unorm) &&
            format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            return format;
    }

    log::error("found no suitable surface format");
    return {};
}

static vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height)
{
    if (capabilities.currentExtent.width == UINT32_MAX)
    {
        vk::Extent2D extent(width, height);
        const vk::Extent2D minExtent = capabilities.minImageExtent;
        const vk::Extent2D maxExtent = capabilities.maxImageExtent;
        extent.width = std::clamp(extent.width, minExtent.width, maxExtent.width);
        extent.height = std::clamp(extent.height, minExtent.height, maxExtent.height);
        return extent;
    }
    return capabilities.currentExtent;
}

void VulkanDevice::createSwapChain(GLFWwindow* window)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    // Init surface
    VkSurfaceKHR glfwSurface;
    const VkResult res = glfwCreateWindowSurface(m_instance.get(), window, nullptr, &glfwSurface);
    if (res != VK_SUCCESS)
        log::fatalError("Failed to create window surface");

    m_surface = vk::UniqueSurfaceKHR(vk::SurfaceKHR(glfwSurface), { m_instance.get() });

    // Create swapChain Info
    using vkIU = vk::ImageUsageFlagBits;
    vk::SwapchainCreateInfoKHR& createInfo = m_swapChainCreateInfo;
    createInfo.setSurface(m_surface.get());
    createInfo.setImageArrayLayers(1);
    createInfo.setImageUsage(vkIU::eColorAttachment | vkIU::eTransferDst);
    createInfo.setImageSharingMode(vk::SharingMode::eExclusive);
    createInfo.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque);
    createInfo.setClipped(true);

    resizeSwapChain(width, height);
}

void VulkanDevice::resizeSwapChain(uint32_t width, uint32_t height)
{
    // we can not create minimized swap chain
    if (width == 0 || height == 0)
        return;

    // we can not destroy swap chain with pending frames
    waitIdle();

    // Setup viewports, vSync
    const std::vector<vk::SurfaceFormatKHR> surfaceFormats = m_physicalDevice.getSurfaceFormatsKHR(m_surface.get());
    const vk::SurfaceCapabilitiesKHR surfaceCapabilities = m_physicalDevice.getSurfaceCapabilitiesKHR(m_surface.get());
    const std::vector<vk::PresentModeKHR> surfacePresentModes =
        m_physicalDevice.getSurfacePresentModesKHR(m_surface.get());

    const vk::Extent2D extent = chooseSwapExtent(surfaceCapabilities, width, height);
    const vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(surfaceFormats);
    const vk::PresentModeKHR presentMode = chooseSwapPresentMode(surfacePresentModes, true);

    uint32_t backBufferCount = std::clamp(surfaceCapabilities.minImageCount, 1u, kMaxPendingFrames);

    // Update & Create SwapChain
    vk::SwapchainCreateInfoKHR& createInfo = m_swapChainCreateInfo;
    createInfo.setPreTransform(surfaceCapabilities.currentTransform);
    createInfo.setImageExtent({ width, height });
    createInfo.setImageColorSpace(surfaceFormat.colorSpace);
    createInfo.setMinImageCount(backBufferCount);
    createInfo.setImageFormat(surfaceFormat.format);
    createInfo.setPresentMode(presentMode);
    createInfo.setOldSwapchain(m_swapChain.get());
    m_swapChain = m_device->createSwapchainKHRUnique(createInfo);

    const std::vector<vk::Image> swapChainImages = m_device->getSwapchainImagesKHR(m_swapChain.get());
    for (size_t i = 0; i < swapChainImages.size(); ++i)
    {
        //TexturePtr& texture = m_frameImages[i];

        TexturePtr texture = std::make_shared<Texture>();
        texture->m_info = vk::ImageCreateInfo();
        texture->m_info.setImageType(vk::ImageType::e2D);
        texture->m_info.setExtent(vk::Extent3D(extent.width, extent.height, 1));
        texture->m_info.setSamples(vk::SampleCountFlagBits::e1);
        texture->m_info.setFormat(createInfo.imageFormat);
        texture->m_info.setArrayLayers(1);

        texture->m_handle = swapChainImages[i];
        texture->m_device = m_device.get();
        texture->m_allocation = nullptr;
        // const std::string debugName = fmt::format("backBuffer{}", i);
        texture->setDebugName("backBuffer" + std::to_string(i));
        texture->generateImageView();

        m_frameImages[i] = std::move(texture);
    }

    log::info("SwapChain: Images({}), Extent({}x{}), Format({}), PresentMode({})", backBufferCount, extent.width,
              extent.height, vk::to_string(surfaceFormat.format), vk::to_string(presentMode));
}

FrameContext& VulkanDevice::acquireNextFrame()
{
    FrameContext& frame = m_frameContext[m_currentFrameIndex];

    frame.waitUntilCompleted();

    // Acquire next frame
    const vk::Result result = m_device->acquireNextImageKHR(
        m_swapChain.get(), UINT64_MAX, frame.m_acquireSemaphore.get(), vk::Fence(), &m_swapChainIndex);
    assert(result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR);

    frame.m_backBuffer = m_frameImages[m_swapChainIndex];
    frame.begin();

    return frame;
}

void VulkanDevice::present(FrameContext& frame)
{
    frame.commit();

    // Present
    vk::PresentInfoKHR presentInfo;
    presentInfo.setWaitSemaphoreCount(1);
    presentInfo.setPWaitSemaphores(&frame.m_presentSemaphore.get());
    presentInfo.setSwapchainCount(1);
    presentInfo.setPSwapchains(&m_swapChain.get());
    presentInfo.setPImageIndices(&m_swapChainIndex);

    const vk::Result result = m_graphicsQueue.presentKHR(&presentInfo);
    assert(result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR);
    m_currentFrameIndex = (m_currentFrameIndex + 1) % m_maxFramesInFlight;
}