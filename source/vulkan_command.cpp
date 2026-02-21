//
// Created by lorian on 14/02/2026.
//

#include "vulkan_device.hpp"

void FrameContext::endDebugEvent() const
{
    m_cmdBuf.endDebugUtilsLabelEXT();
}

void FrameContext::beginDebugEvent(const char* label, const float* color) const
{
    vk::DebugUtilsLabelEXT markerInfo;
    memcpy(markerInfo.color, color, sizeof(float) * 4);
    markerInfo.pLabelName = label;
    m_cmdBuf.beginDebugUtilsLabelEXT(markerInfo);
}

void FrameContext::clearBackBufferColor(std::span<const float> color)
{
    transitionImage(m_backBuffer, vk::PipelineStageFlagBits2::eTransfer, vk::ImageLayout::eTransferDstOptimal);
    flushBarriers();
    vk::ClearColorValue value;
    memcpy(value.float32, color.data(), sizeof(float) * 4);
    constexpr vk::ImageSubresourceRange range(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
    m_cmdBuf.clearColorImage(m_backBuffer->m_handle, m_backBuffer->m_currentLayout, value, range);
}

void FrameContext::bindPipeline(const PipelinePtr& pipeline)
{
    m_pipeline = pipeline;
    m_cmdBuf.bindPipeline(m_pipeline->m_bindPoint, m_pipeline->m_handle.get());
}

void FrameContext::pushConstant(vk::ShaderStageFlags stage, const void* data, uint8_t size) const
{
    assert(size <= 128);
    m_cmdBuf.pushConstants(m_pipeline->m_pipelineLayout.get(), stage, 0, size, data);
}

void FrameContext::setScissor(const vk::Rect2D& rect) const
{
    m_cmdBuf.setScissor(0, 1, &rect);
}

void FrameContext::endRenderPass() const
{
    m_cmdBuf.endRendering();
}

void FrameContext::beginRenderPass(const RenderingInfo& info)
{
    constexpr float minDepth = 0.0f;
    constexpr float maxDepth = 1.0f;

    const vk::Extent2D surface = info.viewport.extent;
    const vk::Viewport renderViewport{
        0.0f, 0.0f, static_cast<float>(surface.width), static_cast<float>(surface.height), minDepth, maxDepth
    };

    vk::RenderingInfo renderInfo;
    vk::RenderingAttachmentInfo depthAttachment;

    const uint32_t numRenderTargets = info.colors.size();
    auto* rt =
        static_cast<vk::RenderingAttachmentInfo*>(alloca(sizeof(vk::RenderingAttachmentInfo) * numRenderTargets));

    for (size_t i = 0; i < numRenderTargets; ++i)
    {
        const TexturePtr& res = info.colors[i].m_renderTarget;
        transitionImage(res, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                        vk::ImageLayout::eColorAttachmentOptimal);
        std::construct_at(rt + i);
        rt[i].setImageView(res->m_view.get());
        rt[i].setImageLayout(vk::ImageLayout::eColorAttachmentOptimal);
        rt[i].setLoadOp(info.colors[i].m_loadOp);
        rt[i].setStoreOp(vk::AttachmentStoreOp::eStore);
        if (rt[i].loadOp == vk::AttachmentLoadOp::eClear)
            rt[i].setClearValue(info.colors[i].m_clearValue);
    }

    if (info.depth.m_renderTarget)
    {
        const TexturePtr& res = info.depth.m_renderTarget;
        transitionImage(res, vk::PipelineStageFlagBits2::eEarlyFragmentTests, vk::ImageLayout::eDepthAttachmentOptimal);
        depthAttachment.setImageView(res->m_view.get());
        depthAttachment.setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal);
        depthAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
        depthAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
        if (depthAttachment.loadOp == vk::AttachmentLoadOp::eClear)
            depthAttachment.setClearValue(vk::ClearDepthStencilValue(1.0f, 0));
        renderInfo.setPDepthAttachment(&depthAttachment);
    }

    renderInfo.setRenderArea(info.viewport);
    renderInfo.setLayerCount(1);
    renderInfo.setPColorAttachments(rt);
    renderInfo.setColorAttachmentCount(numRenderTargets);
    flushBarriers();
    m_cmdBuf.beginRendering(renderInfo);
    m_cmdBuf.setViewport(0, 1, &renderViewport);
}

void FrameContext::drawIndexedPrimitives(uint32_t indexCount, uint32_t firstIndex, int32_t firstVertex,
                                         uint32_t instanceId) const
{
    m_cmdBuf.drawIndexed(indexCount, 1, firstIndex, firstVertex, instanceId);
}

void FrameContext::drawPrimitives(uint32_t vertexCount, uint32_t firstVertex) const
{
    m_cmdBuf.draw(vertexCount, 1, firstVertex, 0);
}