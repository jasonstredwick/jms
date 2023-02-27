#pragma once


#include <ranges>

#include "include_config.hpp"
#include <vulkan/vulkan_raii.hpp>


constexpr uint32_t FindMemoryTypeIndex(const vk::raii::PhysicalDevice& physical_device,
                                       const vk::Flags<vk::MemoryPropertyFlagBits> memory_prop_bits,
                                       uint32_t vertex_mem_type_filter) {
    vk::PhysicalDeviceMemoryProperties physical_device_mem_props = physical_device.getMemoryProperties();
    uint32_t memory_type_index = physical_device_mem_props.memoryTypeCount;
    for (auto i : std::views::iota(static_cast<uint32_t>(0), physical_device_mem_props.memoryTypeCount)) {
        auto prop_check = physical_device_mem_props.memoryTypes[i].propertyFlags & memory_prop_bits;
        if ((1 << i) & vertex_mem_type_filter && prop_check == memory_prop_bits) { memory_type_index = i; }
    }
    if (memory_type_index == physical_device_mem_props.memoryTypeCount) {
        throw std::runtime_error(fmt::format("Failed to find the appropriate memory type on physical device.\n"));
    }
    return memory_type_index;
}
