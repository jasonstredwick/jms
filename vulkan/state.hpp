#pragma once


#include <algorithm>
#include <exception>
#include <format>
#include <iterator>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "jms/vulkan/vulkan.hpp"
#include "jms/vulkan/debug/default.hpp"
#include "jms/vulkan/info.hpp"
#include "jms/vulkan/memory.hpp"
#include "jms/vulkan/shader.hpp"
#include "jms/vulkan/types.hpp"
#include "jms/vulkan/variants.hpp"


namespace jms {
namespace vulkan {


struct alignas(16) InstanceConfig {
    uint32_t api_version{0};
    uint32_t app_version{0};
    uint32_t engine_version{0};
    std::string app_name{};
    std::string engine_name{};
    std::vector<std::string> layer_names{};
    std::vector<std::string> extension_names{};
    std::optional<jms::vulkan::DebugConfig> debug{};
};


struct alignas(16) DeviceConfig {
    std::vector<std::string> layer_names{};
    std::vector<std::string> extension_names{};
    vk::PhysicalDeviceFeatures features{};
    uint32_t queue_family_index{};
    std::vector<float> queue_priority{};
    std::vector<vk::DeviceQueueCreateInfo> queue_infos{};
    std::vector<DeviceCreateInfo2Variant> pnext_features{};
};


struct State {
    // Order matters; i.e. order of destruction
    vk::raii::Context context{};
    InstanceConfig instance_config{};
    vk::raii::Instance instance{nullptr};
    vk::raii::DebugUtilsMessengerEXT debug_messenger{nullptr};
    vk::raii::PhysicalDevices physical_devices{nullptr};
    std::vector<DeviceConfig> device_configs{};
    std::vector<vk::raii::Device> devices{};
    vk::raii::Queue graphics_queue{nullptr};
    vk::raii::Queue present_queue{nullptr};
    std::vector<vk::raii::CommandPool> command_pools{};
    std::vector<std::vector<vk::raii::CommandBuffer>> command_buffers{};
    std::vector<vk::raii::Semaphore> semaphores{};
    std::vector<vk::raii::Fence> fences{};
    vk::raii::SurfaceKHR surface{nullptr};
    vk::raii::SwapchainKHR swapchain{nullptr};
    MemoryHelper<std::vector, jms::NoMutex> memory_helper{};

    State() noexcept = default;
    State(const State&) = delete;
    State(State&&) noexcept = default;
    ~State() noexcept = default;
    State& operator=(const State&) = delete;
    State& operator=(State&&) noexcept = default;

    vk::raii::Device& InitDevice(vk::raii::PhysicalDevice& physical_device,
                                 DeviceConfig&& cfg,
                                 std::optional<vk::AllocationCallbacks*> vk_allocation_callbacks = std::nullopt);
    void InitInstance(InstanceConfig&& cfg,
                      std::optional<vk::AllocationCallbacks*> vk_allocation_callbacks = std::nullopt);
    void InitQueues(size_t device_index,
                    std::optional<vk::AllocationCallbacks*> vk_allocation_callbacks = std::nullopt);
    void InitSwapchain(vk::raii::Device& device,
                       vk::raii::SurfaceKHR& surface,
                       const jms::vulkan::RenderInfo& render_info,
                       //std::optional<vk::RenderPass> render_pass = std::nullopt,
                       std::optional<vk::AllocationCallbacks*> vk_allocation_callbacks = std::nullopt);
};


vk::raii::Device& State::InitDevice(vk::raii::PhysicalDevice& physical_device,
                                    DeviceConfig&& cfg,
                                    std::optional<vk::AllocationCallbacks*> vk_allocation_callbacks) {
    DeviceConfig& device_config = device_configs.emplace_back(std::move(cfg));

    auto TransFn = [](auto& i) { return i.c_str(); };
    std::vector<const char*> vk_layers_name{};
    std::ranges::transform(device_config.layer_names, std::back_inserter(vk_layers_name), TransFn);
    std::vector<const char*> vk_extensions_name{};
    std::ranges::transform(device_config.extension_names, std::back_inserter(vk_extensions_name), TransFn);
    std::vector<DeviceCreateInfo2Variant> pnext_copy = ChainPNext(device_config.pnext_features);

    const void* pnext = nullptr;
    const vk::PhysicalDeviceFeatures* features = &device_config.features;
    vk::PhysicalDeviceFeatures2 features2{};
    if (pnext_copy.size()) {
        features2.pNext = &pnext_copy[0];
        features2.features=device_config.features;
        features = nullptr;
        pnext = static_cast<const void*>(&features2);
    }

    device_config.queue_infos = std::vector<vk::DeviceQueueCreateInfo>{
        // graphics queue + presentation queue
        {
            .queueFamilyIndex=device_config.queue_family_index,
            .queueCount=static_cast<uint32_t>(device_config.queue_priority.size()),
            .pQueuePriorities=device_config.queue_priority.data()
        }
    };

    return devices.emplace_back(physical_device, vk::DeviceCreateInfo{
        .pNext=pnext,
        .queueCreateInfoCount=static_cast<uint32_t>(device_config.queue_infos.size()),
        .pQueueCreateInfos=device_config.queue_infos.data(),
        .enabledLayerCount=static_cast<uint32_t>(vk_layers_name.size()),
        .ppEnabledLayerNames=vk_layers_name.data(),
        .enabledExtensionCount=static_cast<uint32_t>(vk_extensions_name.size()),
        .ppEnabledExtensionNames=vk_extensions_name.data(),
        .pEnabledFeatures=features
    }, vk_allocation_callbacks.value_or(nullptr));
}


void State::InitInstance(InstanceConfig&& cfg, std::optional<vk::AllocationCallbacks*> vk_allocation_callbacks) {
    instance_config = std::move(cfg);

    vk::ApplicationInfo application_info{
        .pApplicationName=instance_config.app_name.c_str(),
        .applicationVersion=instance_config.app_version,
        .pEngineName=instance_config.engine_name.c_str(),
        .engineVersion=instance_config.engine_version,
        .apiVersion=(instance_config.api_version) ? instance_config.api_version : context.enumerateInstanceVersion()
    };

    // dedup and check requested layer is available
    std::set<std::string> layer_names_available{};
    std::ranges::transform(context.enumerateInstanceLayerProperties(),
                           std::inserter(layer_names_available, layer_names_available.begin()),
                           [](auto& i) -> std::string { return i.layerName; });
    std::set<std::string> layer_names{instance_config.layer_names.begin(), instance_config.layer_names.end()};
    if (instance_config.debug.has_value()) { layer_names.insert(std::string{"VK_LAYER_KHRONOS_validation"}); }
    for (auto& i : layer_names) {
        if (!layer_names_available.contains(i)) {
            throw std::runtime_error(std::format("Requested layer \"{}\" not available.", i));
        }
    }

    // dedup and check requested extension is available
    std::set<std::string> extension_names_available{};
    std::ranges::transform(context.enumerateInstanceExtensionProperties(),
                           std::inserter(extension_names_available, extension_names_available.begin()),
                           [](auto& i) -> std::string { return i.extensionName; });
    std::set<std::string> extension_names{instance_config.extension_names.begin(),
                                          instance_config.extension_names.end()};
    for (auto& i : extension_names) {
        if (!extension_names_available.contains(i)) {
            throw std::runtime_error(std::format("Requested instance extension \"{}\" not available.", i));
        }
    }

    auto TransFn = [](auto& i) { return i.c_str(); };
    std::vector<const char*> vk_layer_names{};
    std::ranges::transform(layer_names, std::back_inserter(vk_layer_names), TransFn);
    std::vector<const char*> vk_extension_names{};
    std::ranges::transform(extension_names, std::back_inserter(vk_extension_names), TransFn);

    instance = context.createInstance(vk::InstanceCreateInfo{
        .flags=vk::InstanceCreateFlags(),
        .pApplicationInfo=&application_info,
        .enabledLayerCount=static_cast<uint32_t>(vk_layer_names.size()),
        .ppEnabledLayerNames=vk_layer_names.data(),
        .enabledExtensionCount=static_cast<uint32_t>(vk_extension_names.size()),
        .ppEnabledExtensionNames=vk_extension_names.data()
    }, vk_allocation_callbacks.value_or(nullptr));

    if (instance_config.debug.has_value()) {
        debug_messenger = vk::raii::DebugUtilsMessengerEXT{
            instance,
            vk::DebugUtilsMessengerCreateInfoEXT{
                .flags=vk::DebugUtilsMessengerCreateFlagsEXT(),
                .messageSeverity=instance_config.debug.value().severity_flags,
                .messageType=instance_config.debug.value().msg_type_flags,
                .pfnUserCallback=instance_config.debug.value().debug_fn
            },
            vk_allocation_callbacks.value_or(nullptr)
        };
    }

    physical_devices = vk::raii::PhysicalDevices{instance};
}


void State::InitQueues(size_t device_index, std::optional<vk::AllocationCallbacks*> vk_allocation_callbacks) {
    vk::raii::Device& device = devices.at(device_index);
    uint32_t queue_family_index = device_configs.at(device_index).queue_family_index;

    graphics_queue = vk::raii::Queue{device, 0, 0};
    present_queue = vk::raii::Queue{device, 0, 1};

    command_pools.push_back(device.createCommandPool({
        .flags=vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex=queue_family_index
    }, vk_allocation_callbacks.value_or(nullptr)));
}


void State::InitSwapchain(vk::raii::Device& device,
                          vk::raii::SurfaceKHR& surface,
                          const jms::vulkan::RenderInfo& render_info,
                          //std::optional<vk::RenderPass> render_pass,
                          std::optional<vk::AllocationCallbacks*> vk_allocation_callbacks) {
    swapchain = device.createSwapchainKHR({
        .flags=vk::SwapchainCreateFlagsKHR(),
        .surface=*surface,
        .minImageCount=render_info.image_count,
        .imageFormat=render_info.format,
        .imageColorSpace=render_info.color_space,
        .imageExtent=render_info.extent,
        .imageArrayLayers=1, // mono
        .imageUsage=vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode=vk::SharingMode::eExclusive,
        .queueFamilyIndexCount=0,
        .pQueueFamilyIndices=nullptr,
        .preTransform=render_info.transform_bits,
        .compositeAlpha=vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode=render_info.present_mode,
        .clipped=VK_TRUE,
        .oldSwapchain=VK_NULL_HANDLE
    }, vk_allocation_callbacks.value_or(nullptr));

#if 0
    ImageViewInfo iv_info{.format=render_info.format};
    std::vector<vk::Image> swapchain_images = swapchain.getImages();
    for (auto& image : swapchain_images) {
        swapchain_image_views.push_back(
            device.createImageView(iv_info.ToCreateInfo(image),
                                   vk_allocation_callbacks.value_or(nullptr)));
    }

    if (!render_pass.has_value()) { return; }
    for (auto& image_view : swapchain_image_views) {
        std::array<vk::ImageView, 1> attachments = {*image_view};
        swapchain_framebuffers.push_back(
            device.createFramebuffer({
                .renderPass=*render_pass,
                .attachmentCount=static_cast<uint32_t>(attachments.size()),
                .pAttachments=attachments.data(),
                .width=render_info.extent.width,
                .height=render_info.extent.height,
                .layers=1
            }, vk_allocation_callbacks.value_or(nullptr)));
    }
#endif
}


}
}
