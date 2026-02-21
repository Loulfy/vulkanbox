//
// Created by lorian on 14/02/2026.
//

#include "vulkan_device.hpp"

vk::ImageAspectFlags VulkanDevice::guessImageAspectFlags(vk::Format format, bool stencil)
{
    switch (format)
    {
    case vk::Format::eD16Unorm:
    case vk::Format::eX8D24UnormPack32:
    case vk::Format::eD32Sfloat:
        return vk::ImageAspectFlagBits::eDepth;

    case vk::Format::eS8Uint:
        return vk::ImageAspectFlagBits::eStencil;

    case vk::Format::eD16UnormS8Uint:
    case vk::Format::eD24UnormS8Uint:
    case vk::Format::eD32SfloatS8Uint: {
        if (stencil)
            return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
        else
            return vk::ImageAspectFlagBits::eDepth;
    }

    default:
        return vk::ImageAspectFlagBits::eColor;
    }
}

void Texture::generateImageView(vk::ImageSubresourceRange subresource)
{
    subresource.setAspectMask(VulkanDevice::guessImageAspectFlags(m_info.format, false));

    vk::ImageViewCreateInfo createInfo;
    createInfo.setImage(m_handle);
    createInfo.setViewType(subresource.layerCount == 1 ? vk::ImageViewType::e2D : vk::ImageViewType::e2DArray);
    createInfo.setFormat(m_info.format);
    createInfo.setSubresourceRange(subresource);

    m_view = m_device.createImageViewUnique(createInfo);
}