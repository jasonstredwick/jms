#pragma once


#include <algorithm>
#include <exception>
#include <limits>

#include "jms/vulkan/vulkan.hpp"


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


}
}
