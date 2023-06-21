#pragma once


#include <cstdint>
#include <ranges>

#include "jms/vulkan/vulkan.hpp"


namespace jms {
namespace vulkan {


/**

What I am trying to do?  I am trying to figure out what different kinds of allocation, initialization, and manipulation
of data within Vulkan.  From these different scenarios, I would like to provide functionality for those common or
heavy scenarios.  For each of these types of scenarios I would also like to figure out how heap management works and
how it would work with these scenarios.  For example, my Nvidia card has large device_local and host_visible heaps but
with a small device_local|host_visible heap.  As recommended, most GPU optimal work should be on the device_local
heap and use staging buffers rather than the device_local|host_visible heap leaving that for GPU management stuff like
command buffers and small chunks of highly frequent data.  My other graphics card is an integrated Intel GPU which only
has devic_local|host_visible heaps.  That means all work is done there and thus little attention is needed to determine
which heap allocations should target and also removes the need for staging buffers.

The following can be used for reference material when looking at these scenarios and memory management:

https://asawicki.info/news_1740_vulkan_memory_types_on_pc_and_how_to_use_them

Other references:

https://github.com/philiptaylor/vulkan-sync
https://github.com/google/shaderc
https://www.reddit.com/r/vulkan/comments/squ4i8/writing_vulkan_spirv_shaders_in_c/


Scenario 1: Initialization and construction of memory resources prior to work beginning.

Thoughts: Allocation, data migration, and independent code execution related to initialization can all be run in
parallel.  Since it may need to complete prior to work beginning, it makes sense that the user imposes synchronization
barriers and not the allocation machinery.


Allocation 1: Static, persistant data

Thoughts: Allocate exact amount and initialize.  Initialization can be data migration or computed once.  Alternatively
to GPU compute approach is to allocate and initialize code in system memory and do a simple migration.
*All code executed must be associated with a shader?
*All shader code must be associated with a pipeline?


*All allocated memory needs to be associated with a descriptor in order to use it from a shader?  Should an allocator
provide a wrapper resource for allocation that also maintains the descriptor?



When suballocating, one must be mindful of VkDescriptorBufferInfo offset and range values.  They must meet the limits
set for physical devies.  Listed below are the values for storage and uniform buffers but there are equivalents for
other types like push constants.  Basically, when suballocating the beginning of a buffer must start on particular
aligned values; i.e. the starting offset may need to be aligned 16, 64, or up to 256 bytes boundaries.  The ranges are
likewise restricted to maximum amounts.

*StorageBuffers
    -VkPhysicalDeviceLimits::minStorageBufferOffsetAlignment
    -VkPhysicalDeviceLimits::maxStorageBufferRange
*UniformBuffers
    -VkPhysicalDeviceLimits::minUniformBufferOffsetAlignment
    -VkPhysicalDeviceLimits::maxUniformBufferRange

Alignment of arrays/structs use extended alignment if one of its members has an extended alignment.  All extended
alignments must be rounded to multiples of 16.  Not sure if it also requires alignment to be a power of 2.
*Also as noted in thte spec "The std430 layout in GLSL satisfies these rules for types using the base alignment. The std140 layout satisfies the rules for types using the extended alignment."
*Example document: https://github.com/KhronosGroup/Vulkan-Guide/blob/main/chapters/shader_memory_layout.adoc
 */


enum class AccessDirection {
    BOTH,
    DEVICE,
    HOST,
    UNDEFINED
};


enum class AccessFrequency {
    STATIC,
    ONCE_PER_FRAME,
    MULTIPLE_PER_FRAME,
    UNDEFINED
};


enum class MemoryType {
    DEVICE,
    HOST,
    HYBRID,
    SYSTEM,
    UNDEFINED,
};


enum class DescriptorFrequency {
    GLOBAL,
    PER_PASS,
    PER_MATERIAL,
    PER_OBJECT
};



#if 0
/**
 * Types of allocation?
 * 1.  optimal only
 * 2.  don't care
 * 3.  optimal, in - one time, infrequent, or frequent
 * 4.  optimal, out - one time, infrequent, or frequent
 * 5.  optimal, in/out - one time, infrequent, or frequent
*/
void ToOptimal() {
    vk::PhysicalDeviceType::eIntegratedGpu;
    // 1. Check for special case VkPhysicalDeviceProperties::deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU not DISCRETE_GP
    // 2. Check for first device_local supporting heap
    // 3. If budget check if there is room or go to #2
    // 4. If no device_local with room then is it possible to migrate some data from device_local to other memory?  If not then fail.
}


#include <algorithm>
#include <array>
#include <iterator>
#include <limits>
#include <span>
#include <string_view>
#include <tuple>
#include <vector>
struct BudgetInfo {
    vk::DeviceSize budget;
    vk::DeviceSize usage;
};


struct Memory {
    struct Heap {
        vk::DeviceSize size;
        vk::MemoryHeapFlags flags;
    };
    const std::vector<Heap> heaps{};
    std::array<std::vector<vk::raii::DeviceMemory>, VK_MAX_MEMORY_TYPES> device_memory{{}};

    static Memory From(const vk::raii::PhysicalDevice& device);
};


std::vector<BudgetInfo> ExtractBudgets(const vk::raii::PhysicalDevice& physical_device);
bool HasBudget(const std::vector<vk::ExtensionProperties>& ext_props);


// https://asawicki.info/news_1740_vulkan_memory_types_on_pc_and_how_to_use_them
void F() {
    if (VkPhysicalDeviceProperties::deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {}
}
Memory Memory::From(const vk::raii::PhysicalDevice& physical_device) {
    const vk::PhysicalDeviceMemoryProperties mem_props = physical_device.getMemoryProperties();
    std::span<const vk::MemoryHeap> vk_heaps{mem_props.memoryHeaps.begin(), mem_props.memoryHeapCount};
    std::span<const vk::MemoryType> vk_types{mem_props.memoryTypes.begin(), mem_props.memoryTypeCount};
    std::vector<Memory::Heap> heaps{};
    heaps.reserve(mem_props.memoryHeapCount);
    std::ranges::transform(vk_heaps, std::back_inserter(heaps), [](auto& heap_info) {
        return Memory::Heap{
            .size=heap_info.size,
            .flags=heap_info.flags
        };
    });
    return {.heaps=std::move(heaps)};
}


/***
 * Assumptions:
 * 1. HasBudget has already been checked and is true.
*/
std::vector<BudgetInfo> ExtractBudgets(const vk::raii::PhysicalDevice& physical_device) {
    auto mem_props2 = physical_device.getMemoryProperties2<vk::PhysicalDeviceMemoryProperties2,
                                                           vk::PhysicalDeviceMemoryBudgetPropertiesEXT>();
    const auto& mem_props = mem_props2.get<vk::PhysicalDeviceMemoryProperties2>().memoryProperties;
    const auto& mem_budget = mem_props2.get<vk::PhysicalDeviceMemoryBudgetPropertiesEXT>();
    const size_t num_heaps = mem_props.memoryHeapCount;
    std::vector<BudgetInfo> budget_info{};
    std::ranges::transform(std::span<const vk::DeviceSize>{mem_budget.heapBudget.begin(), num_heaps},
                           std::span<const vk::DeviceSize>{mem_budget.heapUsage.begin(), num_heaps},
                           std::back_inserter(budget_info),
                           [](auto b, auto u) { return BudgetInfo{b, u}; });
    return budget_info;
}


bool HasBudget(const std::vector<vk::ExtensionProperties>& ext_props) {
    return ext_props.end() == std::ranges::find_if(
        ext_props, [test=std::string_view{VK_EXT_MEMORY_BUDGET_EXTENSION_NAME}](const auto& ext) {
            return std::string_view{ext.extensionName} == test;
        });
}
#endif


/***
 * PhysicalDevice memory types are sorted within a heap such that you can take the first one that matches the
 * requested properties.  Not sure about heap order.  Within heap ordering also goes from fewest to most props.
 *
 * If heap type is zero or memoryType[i] is zero then I think it means that it is system memory.
 *
 * memory_type_bits-
 *     a uint32_t whose bits set to 1 correspond to a physcial device's memory type array indices that support the
 *     given type being allocated for.  Also, bits are offset by 1 so 0x1 would be index 0.  I assume this is to
 *     prohibit a given value of 0.  Also, this value works because the maximum number of memory type combinations
 *     i.e. memoryTypeCount has a maximum value of 32.  I assume that if this value increases beyond 32 then
 *     memory_type_bits type would change to uint64_t.  See VK_MAX_MEMORY_TYPES
 *
 * Current version returns VK_MAX_MEMORY_TYPES if a valid type was not found.  This allows calling functions to
 * call again with alternative or fallback types.
 */
std::vector<uint32_t> FindMemoryTypeIndices(const vk::PhysicalDeviceMemoryProperties& mem_props,
                                            const vk::MemoryPropertyFlags desired_bits,
                                            uint32_t memory_type_indices_supported) {
    std::vector<uint32_t> indices{};
    for (auto i : std::views::iota(static_cast<uint32_t>(0), mem_props.memoryTypeCount)) {
        if ((1 << i) & memory_type_indices_supported) {
            auto prop_check = mem_props.memoryTypes[i].propertyFlags & desired_bits;
            if (prop_check == desired_bits) { indices.push_back(i); }
        }
    }
    return indices;
}


std::vector<uint32_t> FindMemoryTypeIndices(const vk::raii::PhysicalDevice& physical_device,
                                            const vk::MemoryPropertyFlags desired_bits,
                                            uint32_t memory_type_indices_supported) {
    return FindMemoryTypeIndices(physical_device.getMemoryProperties(), desired_bits, memory_type_indices_supported);
}


std::vector<uint32_t> FindOptimalIndices(const vk::raii::PhysicalDevice& physical_device) {
    std::array<uint32_t, VK_MAX_MEMORY_TYPES> found{};
    std::ranges::for_each(FindMemoryTypeIndices(physical_device, vk::MemoryPropertyFlagBits::eHostVisible, std::numeric_limits<uint32_t>::max()),
                          [&found](uint32_t i) mutable { found[i] = 1; });
    std::ranges::for_each(FindMemoryTypeIndices(physical_device, vk::MemoryPropertyFlagBits::eDeviceLocal, std::numeric_limits<uint32_t>::max()),
                          [&found](uint32_t i) mutable { found[i] = 1; });
    std::vector<uint32_t> indices{};
    for (size_t i : std::views::iota(static_cast<size_t>(0), found.size())) {
        if (found[i]) { indices.push_back(i); }
    }
    return indices;
}


std::vector<uint32_t> RestrictMemoryTypes(const vk::raii::PhysicalDevice& physical_device,
                                          const std::vector<uint32_t>& all_indices,
                                          const uint32_t supported_indices,
                                          const vk::MemoryPropertyFlags prohibited_flags) {
    std::vector<uint32_t> indices{};
    const vk::PhysicalDeviceMemoryProperties mem_props = physical_device.getMemoryProperties();
    for (uint32_t index : all_indices) {
        if ((1 << index) & supported_indices == 0) { continue; }
        if (mem_props.memoryTypes[index].propertyFlags & prohibited_flags) { continue; }
        indices.push_back(index);
    }
    return indices;
}


#if 0
struct MemoryBuffer {
    vk::raii::Buffer buffer;
    vk::raii::DeviceMemory memory;
    vk::MemoryRequirements reqs;
    uint32_t type_index;
};
void InitMemory(const vk::raii::PhysicalDevice& physical_device,
                const vk::raii::Device& device,
                const size_t size_in_bytes,
                const vk::Flags<vk::BufferUsageFlagBits> buffer_usage_bits,
                const vk::Flags<vk::MemoryPropertyFlagBits> desired_bits) {
    vk::raii::Buffer buffer = device.createBuffer({
        .size=size_in_bytes,
        .usage=buffer_usage_bits,
        .sharingMode=vk::SharingMode::eExclusive
    });
    vk::MemoryRequirements mem_reqs = buffer.getMemoryRequirements();
    uint32_t memory_type_index = FindMemoryTypeIndex(physical_device, desired_bits, mem_reqs.memoryTypeBits);
    vk::raii::DeviceMemory device_memory = device.allocateMemory({
        .allocationSize=mem_reqs.size,
        .memoryTypeIndex=memory_type_index
    });
    buffer.bindMemory(*(device_memory), 0);
}
#endif


template <typename T>
struct StorageBuffer {
    using value_type_t = T;

    size_t num_elements{0};
    vk::DeviceSize buffer_size{0};
    vk::raii::Buffer buffer{nullptr};
    vk::raii::DeviceMemory device_memory{nullptr};

    StorageBuffer(const vk::raii::Device& device,
                  const vk::raii::PhysicalDevice& physical_device,
                  const std::vector<uint32_t>& optimal_indices,
                  size_t num_elements,
                  vk::BufferUsageFlags usage,
                  vk::MemoryPropertyFlags prohibited_flags)
    : num_elements(num_elements),
      buffer_size(static_cast<vk::DeviceSize>(sizeof(T) * num_elements)),
      buffer(device.createBuffer({.size=buffer_size, .usage=usage, .sharingMode=vk::SharingMode::eExclusive}))
    {
        vk::MemoryRequirements mem_reqs = buffer.getMemoryRequirements();
        std::vector<uint32_t> restricted_indices = jms::vulkan::RestrictMemoryTypes(physical_device,
                                                                                    optimal_indices,
                                                                                    mem_reqs.memoryTypeBits,
                                                                                    prohibited_flags);
        if (restricted_indices.empty()) {
            throw std::runtime_error("Failed to create vertex buffer ... no compatible memory type found.");
        }
        device_memory = device.allocateMemory({
            .allocationSize= mem_reqs.size,
            .memoryTypeIndex=restricted_indices[0]
        });
        buffer.bindMemory(*device_memory, 0);
    }
};


template <typename T>
struct GPUStorageBuffer : StorageBuffer<T> {
    GPUStorageBuffer(const vk::raii::Device& device,
                     const vk::raii::PhysicalDevice& physical_device,
                     const std::vector<uint32_t>& optimal_indices,
                     size_t num_elements,
                     vk::BufferUsageFlags usage,
                     vk::MemoryPropertyFlags prohibited_flags={})
    : StorageBuffer<T>(device, physical_device, optimal_indices, num_elements, usage, prohibited_flags) {}
};


template <typename T>
struct HostStorageBuffer : StorageBuffer<T> {
    HostStorageBuffer(const vk::raii::Device& device,
                      const vk::raii::PhysicalDevice& physical_device,
                      const std::vector<uint32_t>& optimal_indices,
                      size_t num_elements,
                      vk::BufferUsageFlags usage,
                      vk::MemoryPropertyFlags prohibited_flags=vk::MemoryPropertyFlagBits::eDeviceLocal)
    : StorageBuffer<T>(device, physical_device, optimal_indices, num_elements, usage, prohibited_flags) {}

    void Zero() {
        auto& self = *this;
        void* dst = self.device_memory.mapMemory(0, self.buffer_size);
        std::memset(dst, 0, self.buffer_size);
        self.device_memory.unmapMemory();
    }

    void Copy(std::span<const T> src) {
        auto& self = *this;
        void* dst = self.device_memory.mapMemory(0, self.buffer_size);
        std::memcpy(dst, src.data(), self.buffer_size);
        self.device_memory.unmapMemory();
    }
};


}
}
