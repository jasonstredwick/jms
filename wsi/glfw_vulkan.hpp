#pragma once


#include <algorithm>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "jms/vulkan/vulkan.hpp"
#define GLFW_INCLUDE_NONE
#include "jms/wsi/glfw.hpp"


namespace jms::wsi::glfw::vulkan {


std::vector<std::string> GetInstanceExtensions() {
    uint32_t count = 0;
    const char** exts = glfwGetRequiredInstanceExtensions(&count);
    if (!count) { return {}; }
    if (!exts) { throw std::runtime_error("Failed to get Vulkan instance required extensions from GLFW."); }
    std::vector<std::string> out{};
    std::ranges::transform(std::span{exts, count}, std::back_inserter(out), [](auto ext) { return std::string{ext}; });
    return out;
}


vk::raii::SurfaceKHR CreateSurface(Window& window,
                                   const vk::raii::Instance& instance,
                                   const vk::AllocationCallbacks* allocator=nullptr) {
    VkSurfaceKHR surface_raw = nullptr;
    const VkAllocationCallbacks* vk_allocator = nullptr;
    if (allocator) { vk_allocator = std::addressof(static_cast<const VkAllocationCallbacks&>(*allocator)); }
    if (glfwCreateWindowSurface(*instance, window.get(), vk_allocator, &surface_raw) != VK_SUCCESS) {
        throw std::runtime_error("GLFW failed to create a surface for the given window.");
    }
    return vk::raii::SurfaceKHR{instance, surface_raw, allocator};
}


}