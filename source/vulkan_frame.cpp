//
// Created by lorian on 14/02/2026.
//

#include "vulkan_device.hpp"

FrameContext::FrameContext(VulkanContext& context) : m_context(context)
{
    m_fence = context.device.createFenceUnique({});
    m_acquireSemaphore = context.device.createSemaphoreUnique({});
    m_presentSemaphore = context.device.createSemaphoreUnique({});
    m_queue = context.device.getQueue(context.graphicsQueueFamily, 0);

    vk::CommandPoolCreateInfo cmdPoolInfo;
    cmdPoolInfo.setQueueFamilyIndex(context.graphicsQueueFamily);
    cmdPoolInfo.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer |
                         vk::CommandPoolCreateFlagBits::eTransient);
    m_allocator = context.device.createCommandPoolUnique(cmdPoolInfo);

    // allocate command buffer
    vk::CommandBufferAllocateInfo allocInfo;
    allocInfo.setLevel(vk::CommandBufferLevel::ePrimary);
    allocInfo.setCommandPool(m_allocator.get());
    allocInfo.setCommandBufferCount(1);

    const vk::Result res = context.device.allocateCommandBuffers(&allocInfo, &m_cmdBuf);
    assert(res == vk::Result::eSuccess);
}

void FrameContext::begin()
{
    m_cmdBuf.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
    m_committed = false;
}

void FrameContext::commit()
{
    transitionImage(m_backBuffer, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::ImageLayout::ePresentSrcKHR);
    flushBarriers();

    m_committed = true;
    m_cmdBuf.end();
    m_waitFenceValue = m_fenceValue++;
    vk::CommandBuffer commandList = m_cmdBuf;

    vk::SubmitInfo submitInfo;
    submitInfo.setCommandBuffers(commandList);
    if (0) // if (m_queueType == QueueType::Transfer)
    {
        m_queue.submit(submitInfo, m_fence.get());
        m_cmdBuf = commandList;
    }
    else
    {
        static constexpr vk::PipelineStageFlags waitMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        submitInfo.setWaitDstStageMask(waitMask);
        submitInfo.setWaitSemaphoreCount(1);
        submitInfo.setPWaitSemaphores(&m_acquireSemaphore.get());
        submitInfo.setSignalSemaphoreCount(1);
        submitInfo.setPSignalSemaphores(&m_presentSemaphore.get());
        m_queue.submit(submitInfo, m_fence.get());
    }
}

void FrameContext::waitUntilCompleted()
{
    if (m_committed)
    {
        vk::Result res = m_context.device.waitForFences(m_fence.get(), true, std::numeric_limits<uint64_t>::max());
        assert(res == vk::Result::eSuccess);
        res = m_context.device.resetFences(1, &m_fence.get());
        assert(res == vk::Result::eSuccess);
        m_committed = false;
    }
}

static constexpr vk::AccessFlags2 getAccessFlagsFromLayout(vk::ImageLayout layout)
{
    switch (layout)
    {
    default:
    case vk::ImageLayout::eUndefined:
    case vk::ImageLayout::ePreinitialized:
        return vk::AccessFlagBits2::eNone;

    case vk::ImageLayout::eColorAttachmentOptimal:
        return vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite;

    case vk::ImageLayout::eDepthAttachmentOptimal:
        return vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite;

    case vk::ImageLayout::eDepthStencilReadOnlyOptimal:
    case vk::ImageLayout::eDepthReadOnlyStencilAttachmentOptimal:
    case vk::ImageLayout::eDepthAttachmentStencilReadOnlyOptimal:
        return vk::AccessFlagBits2::eDepthStencilAttachmentRead;

    case vk::ImageLayout::eShaderReadOnlyOptimal:
        return vk::AccessFlagBits2::eShaderRead;

    case vk::ImageLayout::eTransferSrcOptimal:
        return vk::AccessFlagBits2::eTransferRead;

    case vk::ImageLayout::eTransferDstOptimal:
        return vk::AccessFlagBits2::eTransferWrite;

    case vk::ImageLayout::ePresentSrcKHR:
        return vk::AccessFlagBits2::eMemoryRead;

    case vk::ImageLayout::eGeneral:
        return vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite;
    }
}

void FrameContext::transitionImage(const TexturePtr& image, vk::PipelineStageFlagBits2 newStage,
                                   vk::ImageLayout newLayout)
{
    if (image->m_currentLayout == newLayout)
        return;

    vk::ImageMemoryBarrier2KHR& barrier = m_imageBarriers.emplace_back();
    barrier.srcAccessMask = getAccessFlagsFromLayout(image->m_currentLayout);
    barrier.srcStageMask = image->m_lastStage;
    barrier.dstAccessMask = getAccessFlagsFromLayout(newLayout);
    barrier.dstStageMask = newStage;
    barrier.oldLayout = image->m_currentLayout;
    barrier.newLayout = newLayout;
    barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    barrier.setImage(image->m_handle);
    const vk::ImageAspectFlags aspect = VulkanDevice::guessImageAspectFlags(image->m_info.format, false);
    barrier.setSubresourceRange(
        vk::ImageSubresourceRange(aspect, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS));

    image->m_currentLayout = newLayout;
    image->m_lastStage = newStage;
}

void FrameContext::transitionBuffer(const BufferPtr& buffer, vk::PipelineStageFlagBits2 newStage,
                                    vk::AccessFlagBits2 newAccess)
{
    if (buffer->m_currentAccess & newAccess)
        return;

    vk::BufferMemoryBarrier2KHR& barrier = m_bufferBarriers.emplace_back();
    barrier.srcAccessMask = buffer->m_currentAccess;
    barrier.srcStageMask = buffer->m_lastStage;
    barrier.dstAccessMask = newAccess;
    barrier.dstStageMask = newStage;
    barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    barrier.buffer = buffer->m_handle;
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;

    buffer->m_currentAccess = newAccess;
    buffer->m_lastStage = newStage;
}

void FrameContext::flushBarriers()
{
    if (m_imageBarriers.empty() && m_bufferBarriers.empty())
        return;

    vk::DependencyInfoKHR dependencyInfo;
    dependencyInfo.imageMemoryBarrierCount = m_imageBarriers.size();
    dependencyInfo.pImageMemoryBarriers = m_imageBarriers.data();
    dependencyInfo.bufferMemoryBarrierCount = m_bufferBarriers.size();
    dependencyInfo.pBufferMemoryBarriers = m_bufferBarriers.data();
    m_cmdBuf.pipelineBarrier2(dependencyInfo);
    m_imageBarriers.clear();
    m_bufferBarriers.clear();
}