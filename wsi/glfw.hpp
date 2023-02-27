#pragma once


#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include <GLFW/glfw3.h>


namespace vk {
    struct AllocationCallbacks;
    namespace raii {
        struct Instance;
        struct SurfaceKHR;
    }
}


namespace jms {
namespace wsi {
namespace glfw {


struct Environment {
    Environment() { if (!glfwInit()) { throw std::runtime_error("Failed to initialize GLFW."); } }
    Environment(const Environment&) = delete;
    Environment(Environment&&) = default;
    ~Environment() { glfwTerminate(); }
    Environment& operator=(const Environment&) = delete;
    Environment& operator=(Environment&&) = default;

    void EnableHIDPI();
};

struct Window {
    std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> window{nullptr, &glfwDestroyWindow};

    Window(int width, int height,
           std::string title=std::string{}, GLFWmonitor* monitor=nullptr, GLFWwindow* share=nullptr) {
        window.reset(glfwCreateWindow(width, height, title.c_str(), monitor, share));
        if (!window) { throw std::runtime_error("GLFW failed to create a window."); }
    }
    Window(const Window&) = delete;
    Window(Window&&) = default;
    ~Window() = default;
    Window& operator=(const Window&) = delete;
    Window& operator=(Window&&) = default;

    GLFWwindow* operator->() noexcept { return window.get(); }
    GLFWwindow* get() noexcept { return window.get(); }

    std::tuple<int, int> DimsPixel() const noexcept {
        int w=0, h=0;
        glfwGetFramebufferSize(window.get(), &w, &h);
        return {w, h};
    }

    std::tuple<int, int> DimsScreen() const noexcept {
        int w=0, h=0;
        glfwGetWindowSize(window.get(), &w, &h);
        return {w, h};
    }

    static Window DefaultCreate(int width, int height, int pos_x=0, int pos_y=0) {
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
        Window window(width, height);
        glfwSetWindowPos(window.get(), pos_x, pos_y);
        glfwShowWindow(window.get());
        glfwFocusWindow(window.get());
        return window;
    }

    static Window DefaultCreateFullscreen(GLFWmonitor* monitor=nullptr) {
        if (!monitor) {
            monitor = glfwGetPrimaryMonitor();
            if (!monitor) { throw std::runtime_error("Failed to get primary GLFW monitor."); }
        }
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if (!mode) { throw std::runtime_error("Failed to get GLFW monitor video mode."); }
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_RED_BITS, mode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
        glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);
        Window window(mode->width, mode->height, std::string{}, monitor);
        return window;
    }
};


std::vector<std::string> GetVulkanInstanceExtensions();
vk::raii::SurfaceKHR CreateSurface(Window& window, const vk::raii::Instance& instance, const vk::AllocationCallbacks* allocator=nullptr);


}
}
}