#pragma once


#include <cstdint>
#include <vector>

#include "jms/vulkan/vulkan.hpp"
#include "jms/vulkan/utils.hpp"


namespace jms {
namespace vulkan {


struct BufferInfo {
    vk::DeviceSize size{};
    vk::BufferCreateFlags flags{};
    vk::BufferUsageFlags usage{};
    std::vector<uint32_t> queue_family_indices{};

    vk::BufferCreateInfo ToCreateInfo() {
        return vk::BufferCreateInfo{
            .flags=flags,
            .size=size,
            .usage=usage,
            .sharingMode=(queue_family_indices.size() > 1 ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive),
            .queueFamilyIndexCount=static_cast<uint32_t>(queue_family_indices.size()),
            .pQueueFamilyIndices=VectorAsPtr(queue_family_indices)
        };
    }
};


struct ImageInfo {
    vk::ImageCreateFlags flags{};
    vk::ImageType image_type{vk::ImageType::e2D};
    vk::Format format{vk::Format::eUndefined};
    vk::Extent3D extent{.width{0}, .height{0}, .depth{1}};
    uint32_t mip_levels{1};
    uint32_t array_layers{1};
    vk::SampleCountFlagBits samples{vk::SampleCountFlagBits::e1};
    vk::ImageTiling tiling{vk::ImageTiling::eOptimal};
    vk::ImageUsageFlags usage{};
    vk::SharingMode sharing_mode{vk::SharingMode::eExclusive};
    vk::ImageLayout initial_layout{vk::ImageLayout::eUndefined};
    VkImageAspectFlagBits aspect_flag{};
    std::vector<uint32_t> queue_family_indices{};

    vk::ImageCreateInfo ToCreateInfo() const {
        return vk::ImageCreateInfo{
            .flags=flags,
            .imageType=image_type,
            .format=format,
            .extent=extent,
            .mipLevels=mip_levels,
            .arrayLayers=array_layers,
            .samples=samples,
            .tiling=tiling,
            .usage=usage,
            .sharingMode=(queue_family_indices.size() > 1 ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive),
            .queueFamilyIndexCount=static_cast<uint32_t>(queue_family_indices.size()),
            .pQueueFamilyIndices=VectorAsPtr(queue_family_indices),
            .initialLayout=initial_layout
        };
    }
};


struct ImageViewInfo {
    vk::ImageViewCreateFlags flags{};
    vk::ImageViewType view_type{vk::ImageViewType::e2D};
    vk::Format format{vk::Format::eUndefined};
    vk::ComponentMapping components{
        .r{vk::ComponentSwizzle::eIdentity},
        .g{vk::ComponentSwizzle::eIdentity},
        .b{vk::ComponentSwizzle::eIdentity},
        .a{vk::ComponentSwizzle::eIdentity}
    };
    vk::ImageSubresourceRange subresource{
        .aspectMask{vk::ImageAspectFlagBits::eColor},
        .baseMipLevel{0},
        .levelCount{1},
        .baseArrayLayer{0},
        .layerCount{1}
    };

    vk::ImageViewCreateInfo ToCreateInfo(vk::Image image) const {
        return {
            .flags=flags,
            .image=image,
            .viewType=view_type,
            .format=format,
            .components=components,
            .subresourceRange=subresource
        };
    }
};


struct RenderInfo {
    vk::Format format{vk::Format::eUndefined};
    vk::ColorSpaceKHR color_space{vk::ColorSpaceKHR::ePassThroughEXT};
    vk::Extent2D extent{};
    uint32_t image_count{0};
    vk::PresentModeKHR present_mode{vk::PresentModeKHR::eFifo};
    vk::SurfaceTransformFlagBitsKHR transform_bits{vk::SurfaceTransformFlagBitsKHR::eIdentity};
};


}
}
