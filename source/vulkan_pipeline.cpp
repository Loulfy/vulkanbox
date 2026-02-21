//
// Created by lorian on 15/02/2026.
//

#include "vulkan_device.hpp"

// #define SPIRV_REFLECT_HAS_VULKAN_H
#define SPIRV_REFLECT_USE_SYSTEM_SPIRV_H
#include <spirv_reflect.h>

Shader VulkanDevice::createShader(const ShaderModule& shaderModule)
{
    fs::path path = sys::CACHED_DIR / shaderModule.path;
    std::string filename;
    filename += path.stem().string();
    // filename += stageToExtension(shaderModule.stage());

    Shader shader;
    shader.stage = shaderModule.stage;
    shader.entryPoint = shaderModule.entryPoint;

    std::vector<uint8_t> fileBlob;
    // sys::FileSystemService::Get().readFile(filename.c_str(), fileBlob);

    std::ifstream f(path, std::ios::in | std::ios::binary);

    // Get the size of the file.
    std::error_code ec;
    const auto sz = static_cast<std::streamsize>(fs::file_size(path, ec));
    if (ec.value())
        log::fatalError("File Not Found: " + ec.message());

    // Create a buffer.
    fileBlob.resize(sz);

    // Read the whole file into the buffer.
    f.read(reinterpret_cast<char*>(fileBlob.data()), sz);
    f.close();

    vk::ShaderModuleCreateInfo shaderInfo;
    shaderInfo.setCodeSize(fileBlob.size());
    shaderInfo.setPCode(reinterpret_cast<const uint32_t*>(fileBlob.data()));
    shader.shaderModule = m_device->createShaderModuleUnique(shaderInfo);

    uint32_t count = 0;
    SpvReflectShaderModule module;
    SpvReflectResult result = spvReflectCreateShaderModule(fileBlob.size(), fileBlob.data(), &module);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    shader.entryPoint = module.entry_point_name;

    // Push Constants
    result = spvReflectEnumeratePushConstantBlocks(&module, &count, nullptr);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    std::vector<SpvReflectBlockVariable*> constants(count);
    result = spvReflectEnumeratePushConstantBlocks(&module, &count, constants.data());
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    for (SpvReflectBlockVariable* block : constants)
        shader.pushConstant = vk::PushConstantRange(shader.stage, block->offset, block->size);

    // Descriptor Set
    result = spvReflectEnumerateDescriptorSets(&module, &count, nullptr);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    std::vector<SpvReflectDescriptorSet*> sets(count);
    result = spvReflectEnumerateDescriptorSets(&module, &count, sets.data());
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    for (SpvReflectDescriptorSet* set : sets)
    {
        int useMutable = 0;
        for (size_t i = 0; i < set->binding_count; ++i)
        {
            const uint32_t binding = set->bindings[i]->binding;
            const uint32_t descriptorCount = set->bindings[i]->count;
            const auto descriptorType = static_cast<vk::DescriptorType>(set->bindings[i]->descriptor_type);
            log::debug("set = {}, binding = {}, count = {:02}, type = {}", set->set, binding, descriptorCount,
                       vk::to_string(descriptorType));
            if (binding == 0 && descriptorCount == 0)
                ++useMutable;
        }
        // log::debug("set = {}, mutable = {}", set->set, useMutable > 1);
    }

    spvReflectDestroyShaderModule(&module);
    return shader;
}

static void addShaderStage(std::vector<vk::PipelineShaderStageCreateInfo>& stages, const Shader& shader)
{
    stages.emplace_back(vk::PipelineShaderStageCreateFlags(), shader.stage, shader.shaderModule.get(),
                        shader.entryPoint.data(), nullptr);
}

void GraphicsPipelineInfo::fillGraphicsPsoDesc()
{
    // COLOR RENDER TARGET STATE
    m_colorBlendAttachments.resize(1);
    m_colorFormatAttachments.resize(1);
    m_colorFormatAttachments[0] = vk::Format::eB8G8R8A8Unorm;
    m_colorBlendAttachments[0].setBlendEnable(false);
    m_colorBlendAttachments[0].setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                                 vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    /*for (size_t i = 0; i < m_colorFormatAttachments.size(); ++i)
    {
        const RenderColorTarget* rt = desc->color_rt()->Get(i);
        m_colorFormatAttachments[i] = VKGraphicsContextImpl::convertFormat(rt->format());

        vk::PipelineColorBlendAttachmentState& pcb = m_colorBlendAttachments[i];
        pcb.blendEnable = rt->blend_enable();
        pcb.setSrcColorBlendFactor(convertBlendFactor(rt->src_blend_color()));
        pcb.setDstColorBlendFactor(convertBlendFactor(rt->dst_blend_color()));
        pcb.setColorBlendOp(convertBlendOp(rt->op_blend_color()));
        pcb.setSrcAlphaBlendFactor(convertBlendFactor(rt->src_blend_alpha()));
        pcb.setDstAlphaBlendFactor(convertBlendFactor(rt->dst_blend_alpha()));
        pcb.setAlphaBlendOp(convertBlendOp(rt->op_blend_alpha()));
        pcb.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    }*/

    auto& renderingCreateInfo = m_chain.get<vk::PipelineRenderingCreateInfo>();
    renderingCreateInfo.setColorAttachmentFormats(m_colorFormatAttachments);

    // TOPOLOGY STATE
    m_pia.setTopology(vk::PrimitiveTopology::eTriangleStrip);

    // Multi Sampling STATE
    m_pm.setRasterizationSamples(vk::SampleCountFlagBits::e1);

    // POLYGON STATE
    m_pr.setDepthClampEnable(VK_TRUE);
    m_pr.setRasterizerDiscardEnable(VK_FALSE);
    // if (desc->fill_mode() == RasterFillMode_Solid)
    m_pr.setPolygonMode(vk::PolygonMode::eFill);
    /*else
        m_pr.setPolygonMode(vk::PolygonMode::eLine);*/
    m_pr.setFrontFace(vk::FrontFace::eCounterClockwise);
    m_pr.setCullMode(vk::CullModeFlagBits::eNone);
    m_pr.setDepthBiasEnable(VK_FALSE);
    m_pr.setDepthBiasConstantFactor(0.f);
    m_pr.setDepthBiasClamp(0.f);
    m_pr.setDepthBiasSlopeFactor(0.f);
    m_pr.setLineWidth(1);

    // VIEWPORT STATES
    m_pv.viewportCount = 1;
    m_pv.scissorCount = 1;

    // DEPTH & STENCIL STATE
    /*const RenderDepthStencil* depthStencil = desc->depth_rt();
    if (depthStencil->format().has_value())
        renderingCreateInfo.setDepthAttachmentFormat(
            VKGraphicsContextImpl::convertFormat(depthStencil->format().value()));*/
    m_pds.setDepthTestEnable(VK_FALSE);
    m_pds.setDepthWriteEnable(VK_FALSE);
    m_pds.setDepthCompareOp(vk::CompareOp::eLessOrEqual);
    m_pds.setDepthBoundsTestEnable(VK_FALSE);
    m_pds.setStencilTestEnable(VK_FALSE);
    m_pds.setFront(vk::StencilOpState());
    m_pds.setBack(vk::StencilOpState());
    m_pds.setMinDepthBounds(0.f);
    m_pds.setMaxDepthBounds(1.f);

    // BLEND STATE
    m_pbs.setLogicOpEnable(VK_FALSE);
    m_pbs.setLogicOp(vk::LogicOp::eClear);
    m_pbs.setAttachments(m_colorBlendAttachments);

    // DYNAMIC STATE
    m_pdy.setDynamicStates(m_dynamicStates);

    // SHADER REFLECT
    /*std::set<uint32_t> bindingUsed;
    for (const VertexInputLayout* vi : *desc->vertex_input())
    {
        vk::VertexInputAttributeDescription& attribute = m_attributes.emplace_back();
        attribute.format = VKGraphicsContextImpl::convertFormat(vi->format());
        attribute.location = vi->location();
        attribute.binding = vi->stream();
        bindingUsed.insert(vi->stream());
    }

    for (const uint32_t binding : bindingUsed)
        m_bindingDesc.emplace_back(binding, 0, vk::VertexInputRate::eVertex);

    // Compute final offsets of each attribute, and total vertex stride.
    for (auto& attribute : m_attributes)
    {
        const uint32_t binding = attribute.binding;
        const uint32_t formatSize = VKGraphicsContextImpl::formatByteSize(static_cast<VkFormat>(attribute.format));
        attribute.offset = m_bindingDesc[binding].stride;
        m_bindingDesc[binding].stride += formatSize;
    }*/
    m_pvi.setVertexBindingDescriptions(m_bindingDesc);
    m_pvi.setVertexAttributeDescriptions(m_attributes);

    m_flags.setFlags(vk::PipelineCreateFlagBits2KHR::eCaptureDataKHR);

    auto& createInfo = m_chain.get<vk::GraphicsPipelineCreateInfo>();
    createInfo.setRenderPass(nullptr);
    // pipelineInfo.setFlags(vk::PipelineCreateFlagBits::eDescriptorBufferEXT);
    createInfo.setPVertexInputState(&m_pvi);
    createInfo.setPInputAssemblyState(&m_pia);
    createInfo.setPViewportState(&m_pv);
    createInfo.setPRasterizationState(&m_pr);
    createInfo.setPMultisampleState(&m_pm);
    createInfo.setPDepthStencilState(&m_pds);
    createInfo.setPColorBlendState(&m_pbs);
    createInfo.setPDynamicState(&m_pdy);

    m_chain.assign(m_flags);
    m_chain.unlink<vk::PipelineCreateFlags2CreateInfoKHR>();
    m_chain.unlink<vk::PipelineBinaryInfoKHR>();
}

void GraphicsPipelineInfo::fillShaderStages(const std::span<Shader>& shaders)
{
    for (const Shader& shader : shaders)
        addShaderStage(m_shaderStages, shader);
    m_chain.get<vk::GraphicsPipelineCreateInfo>().setStages(m_shaderStages);
}

void GraphicsPipelineInfo::fillLayout(const vk::PipelineLayout& layout)
{
    m_chain.get<vk::GraphicsPipelineCreateInfo>().setLayout(layout);
}

static vk::UniquePipelineLayout installPipelineLayout(const VulkanContext& context, const std::span<Shader>& shaders)
{
    vk::PipelineLayoutCreateInfo layoutInfo;
    std::vector<vk::PushConstantRange> pushConstants;
    for (const auto& shader : shaders)
    {
        if (shader.pushConstant.size == 0)
            continue;
        pushConstants.emplace_back(shader.pushConstant);
    }
    layoutInfo.setPushConstantRanges(pushConstants);

    // std::array<vk::DescriptorSetLayout, 1> descriptorSetLayout = { context.bindlessLayout };
    // layoutInfo.setSetLayouts(descriptorSetLayout);
    return context.device.createPipelineLayoutUnique(layoutInfo);
}

PipelinePtr VulkanDevice::createGraphicsPipeline()
{
    std::array<ShaderModule, 2> shaderModules;
    shaderModules[0].path = "quad.vert.spv";
    shaderModules[0].stage = vk::ShaderStageFlagBits::eVertex;
    shaderModules[0].entryPoint = "VSMain";

    shaderModules[1].path = "sponge.frag.spv";
    shaderModules[1].stage = vk::ShaderStageFlagBits::eFragment;
    shaderModules[1].entryPoint = "PSMain";

    std::vector<Shader> shaders;
    for (const ShaderModule& shaderModule : shaderModules)
        shaders.emplace_back(createShader(shaderModule));

    PipelinePtr pipeline = std::make_shared<Pipeline>();
    pipeline->m_pipelineLayout = installPipelineLayout(m_context, shaders);

    GraphicsPipelineInfo pipelineInfo;
    pipelineInfo.fillShaderStages(shaders);
    pipelineInfo.fillGraphicsPsoDesc();
    pipelineInfo.fillLayout(pipeline->m_pipelineLayout.get());

    vk::ResultValue<vk::UniquePipeline> res =
        m_context.device.createGraphicsPipelineUnique(nullptr, pipelineInfo.getCreateInfo());

    assert(res.result == vk::Result::eSuccess);
    pipeline->m_bindPoint = vk::PipelineBindPoint::eGraphics;
    pipeline->m_handle = std::move(res.value);
    return pipeline;
}