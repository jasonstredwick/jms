#pragma once


#include <algorithm>
#include <exception>
#include <limits>

#include "include_config.hpp"
#include <vulkan/vulkan_raii.hpp>
#undef max


namespace jms {
namespace vulkan {


struct RenderInfo {
    vk::Format format = vk::Format::eUndefined;
    vk::ColorSpaceKHR color_space = vk::ColorSpaceKHR::ePassThroughEXT;
    vk::Extent2D extent{};
    uint32_t image_count = 0;
    vk::PresentModeKHR present_mode = vk::PresentModeKHR::eFifo;
    vk::SurfaceTransformFlagBitsKHR transform_bits = vk::SurfaceTransformFlagBitsKHR::eIdentity;
};


RenderInfo SurfaceInfo(const vk::raii::SurfaceKHR& surface, const vk::raii::PhysicalDevice& physical_device,
                       uint32_t client_width, uint32_t client_height) {
    auto surface_formats_vec = physical_device.getSurfaceFormatsKHR(*surface);
    if (surface_formats_vec.empty()) { throw std::runtime_error("No formats found for surface."); }

    vk::SurfaceFormatKHR surface_format{surface_formats_vec[0]};
    auto color_space_comp = [](auto& i) {
        return i.format == vk::Format::eB8G8R8A8Srgb && i.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
    };
    if (auto iter = std::ranges::find_if(surface_formats_vec, color_space_comp);
        iter != surface_formats_vec.end()) {
        surface_format = *iter;
    }

    auto surface_present_modes_vec = physical_device.getSurfacePresentModesKHR(*surface);
    auto present_mode = vk::PresentModeKHR::eFifo;
    if (auto iter = std::ranges::find_if(surface_present_modes_vec,
                                         [](auto &i) { return i == vk::PresentModeKHR::eMailbox; });
        iter != surface_present_modes_vec.end()) {
        present_mode = vk::PresentModeKHR::eMailbox;
    }

    vk::SurfaceCapabilitiesKHR surface_caps = physical_device.getSurfaceCapabilitiesKHR(*surface);
    vk::Extent2D extent = surface_caps.currentExtent;
    if (extent.width == std::numeric_limits<uint32_t>::max()) {
        if (client_width == 0) { throw std::runtime_error("Failed to get window dimensions."); }
        extent.width = std::clamp(static_cast<uint32_t>(client_width),
                                  surface_caps.minImageExtent.width,
                                  surface_caps.maxImageExtent.width);
        extent.height = std::clamp(static_cast<uint32_t>(client_height),
                                   surface_caps.minImageExtent.height,
                                   surface_caps.maxImageExtent.height);
    }

    uint32_t image_count = surface_caps.minImageCount + 1;
    if (surface_caps.maxImageCount > 0 && image_count > surface_caps.maxImageCount) {
        image_count = surface_caps.maxImageCount;
    }

    return RenderInfo{
        .format=surface_format.format,
        .color_space=surface_format.colorSpace,
        .extent=extent,
        .image_count=image_count,
        .present_mode=present_mode,
        .transform_bits=surface_caps.currentTransform
    };
}


}
}
