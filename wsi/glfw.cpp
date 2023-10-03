#include "glfw.hpp"

#include <format>
#include <stdexcept>
#include <string>
#include <vector>


namespace jms {
namespace wsi {
namespace glfw {


void Environment::EnableHIDPI() {
#ifdef VK_USE_PLATFORM_WIN32_KHR
    // Prevent Windows from auto stretching content; just want a raw rectangle of pixels
    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        auto error = GetLastError();
        if (error != ERROR_ACCESS_DENIED) { // ERROR_ACCESS_DENIED == already set; ignore error
            throw std::runtime_error(std::format("WIN32: Failed to set dpi awareness: {}\n", error));
        }
    }
#endif
}


std::vector<std::string> GetVulkanInstanceExtensions() {
    uint32_t count = 0;
    const char** exts = glfwGetRequiredInstanceExtensions(&count);
    if (exts == nullptr && count) {
        throw std::runtime_error("Failed to get Vulkan instance required extensions from GLFW.");
    }
    std::vector<std::string> out{};
    for (int i=0; i<count; ++i) {
        out.push_back(std::string{exts[i]});
    }
    return out;
}


vk::raii::SurfaceKHR CreateSurface(Window& window,
                                   const vk::raii::Instance& instance,
                                   const vk::AllocationCallbacks* allocator) {
    VkSurfaceKHR surface_raw = nullptr;
    const VkAllocationCallbacks* vk_allocator = nullptr;
    if (allocator) { vk_allocator = std::addressof(static_cast<const VkAllocationCallbacks&>(*allocator)); }
    if (glfwCreateWindowSurface(*instance, window.get(), vk_allocator, &surface_raw) != VK_SUCCESS) {
        throw std::runtime_error("GLFW failed to create a surface for the given window.");
    }
    return vk::raii::SurfaceKHR{instance, surface_raw, allocator};
}


}
}
}
