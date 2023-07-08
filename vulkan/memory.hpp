#pragma once


#include <cstdint>
#include <ranges>
#include <vector>



#include <array>
#include <cstddef>
#include <list>
#include <stdexcept>


#include <algorithm>
#include <cstdlib>
#include <mutex>
#include <utility>


#include <bit>
#include <iterator>
#include <limits>
#include <span>
#include <string_view>
#include <tuple>



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



/***
 * Vulkan storage buffer allocator.
 *
 * 32KB  = 32768  <- 2^15
 * 64KB  = 65536  <- 2^16
 * 128KB = 131072 <- 2^17
 * 256KB = 262144 <- 2^18
 *
 * 256MB = 268435456 <- 2^28
 *
 * 1GB   = 1073741824  <- 2^30
 * 32GB  = 34359738368 <- 32 * 2^30 <- 2^35
 * 64GB  = 68719476736 <- 64 * 2^30 <- 2^36
 *
 * Allocation strategies-
 *   1. allocate next chunk when percentage filled exceeded: 50%, 75%, 100%
 *   2. allocate next chunk with size-
 *       a. relative to current chunk size: 100%, 200%
 *       b. fixed chunk size
 *
 * Storage provides allocations for a specific device.  Storage cannot change what device it is associated with.
 * Alternate strategies and migrations would be required to copy/move data from one Storage to another.  This
 * structure is not intended for multi-device support and device intermixing.
 *
 * *NOTE: Should be monitoring the maxMemoryAllocationCount feature.  How to ensure that allocations happen elsewhere?
 * *NOTE: Should be checking against the feature maxMemoryAllocationSize
 * *NOTE: there is a chance that allocation is larger than requested due to alignment manipulation behind the
 *        scenes.  I am not certain how to check or get that value.
 *
 *
 * storage_blocks = StorageBlocks{.device=device, .memory_type_index=memory_type_index, .chunk_size=65536};
 * SSBO<Vertex> ssbo_vertices{storage_blocks};
 * std::vector<Vertex> vertices{};
 * ssbo_vertices.copy(vertices);
 * ssbo_vertices.update(vertices, dirty_ranges); // what happens if includes ranges beyond last copy
 *
 * ssbo_vertices.bind(); // lets say it contains 2 64KB non-contiguous pages.  How to bind for shader?
 * struct Page {
 * };
 *
 *
 *
 * Types of buffers-
 * 1. Class instance - uniform buffer allocated from block for instance allocations
 * 2. Class instance array - same as above but as array; must verify size?  Or bump to storage buffer?
 * 3. Large buffers of the same type
 *     a. fixed size array
 * Dynamic allocations need to designate growth methods:
 * a. how much to grow
 * b. how to grow; i.e. extend with new block or allocate new chunk, copy data, and deallocate old memory
 * c. should growth allocate the entire (aligned) requested bytes?  Or should allocate entire growth split across
 *    chunks?
 * What should happen when starting with small amounts of data?  Then what should happen when it grows to a threshold?
 * At threshold should it migrate small allocations into unit chunk?  Then allocate all future with chunks? How can
 * one write a strategy that allows this flexibility?  Concern is overallocating for early data that either leads
 * to lots of waste when lots of types but smaller quanity. (maybe this doesn't matter and should be using GPU for
 * this small quantities?)  The other issue would be small allocations alternating within a chunk before it gets to
 * a good chunk size.  Should it then consolidate the small ones and then grow by chunk?  Or start out with the
 * proper good chunk size first?  How to easily allow this flexibility?
 *
 * When using sparse buffers to store data across chunks, do I need to worry about element proximity and caching?  If
 * sparse buffers are no performant could one use multiple render passes to render each chunk?  Again, is there a
 * potential issue with element locality and caching?
 *
 * Need to have multiple pipelines for different primitives.  How to reuse sparse data?
 */
struct StorageBlocks {
    static_assert(sizeof(uint64_t) <= sizeof(VkDeviceSize),
                  "StorageBlocks requires VkDeviceSize is at least the size of uint64_t.");

    static const int64_t ERROR_INVALID_CHUNK_SIZE = -1;             // power of 2; greater than 512
    static const int64_t ERROR_INVALID_MAX_CHUNK_SIZE = -2;         // power of 2; greater than 512
    static const int64_t ERROR_INVALID_REQUESTED_CHUNK_SIZE = -3;   // power of 2; greater than 512
    static const int64_t ERROR_TOO_MANY_OBJECTS = -4;               // VK_ERROR_TOO_MANY_OBJECTS
    static const int64_t ERROR_OUT_OF_MEMORY = -5;                  // VK_ERROR_OUT_OF_DEVICE_MEMORY
    static const int64_t ERROR_INVALID_EXTERNAL_HANDLE = -6;        // VK_ERROR_INVALID_EXTERNAL_HANDLE
    static const int64_t ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS = -7; // VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS_KHR
    static const int64_t ERROR_UNKNOWN_VULKAN_ERROR = -8;           // unknown/unexpected Vulkan error
    static const int64_t ERROR_REQUEST_EXCEEDS_MAX = -9;            // requested memory too large

    VkDevice_T* const device;
    VkAllocationCallbacks vk_allocation_callbacks{};
    alignas(sizeof(int64_t)) const uint32_t memory_type_index;

    int64_t chunk_size = 32768;
    int64_t max_chunk_size = 268435456;
    std::vector<VkDeviceMemory_T*> blocks{};

    StorageBlocks() = default;
    StorageBlocks(const StorageBlocks&) = delete;
    StorageBlocks(StorageBlocks&&) = default;
    ~StorageBlocks() { Clear(); }
    StorageBlocks& operator=(const StorageBlocks&) = delete;
    StorageBlocks& operator=(StorageBlocks&&) = default;

    [[nodiscard]] void* allocate(size_t bytes, size_t alignment=alignof(std::max_align_t));
    void deallocate(void* p, size_t bytes, size_t alignment=alignof(std::max_align_t));
    struct memory_resource{}; // define temporarily for is_equal
    bool is_equal(const memory_resource& other) const noexcept;

    bool do_is_equal();

    /**
     * Must take into account-
     * 1. protected bit set for memory type and whether or not protected memory feature is enabled
     * 2. Do I need to care about MemoryOpaqueCapture?
     * 3. Do I need to care about AllocateDeviceAddress?
     * 4. VkDedicatedAllocationMemoryAllocateInfoNV
     * 5. VkMemoryAllocateFlagsInfo,
     * 6. VkMemoryDedicatedAllocateInfo,
     * 7. VkMemoryPriorityAllocateInfoEXT
    */
    int64_t BlockAllocate(int64_t size_in_bytes) {
        if ((max_chunk_size < 512) || (max_chunk_size & (max_chunk_size - 1) != 0)) {
            return ERROR_INVALID_MAX_CHUNK_SIZE;
        }
        if ((size_in_bytes < 512) || (size_in_bytes & (size_in_bytes - 1) != 0) || (size_in_bytes > max_chunk_size)) {
            return ERROR_INVALID_MAX_CHUNK_SIZE;
        }
        VkDeviceMemory_T* mem = nullptr;
        VkMemoryAllocateInfo info{
            .sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext=nullptr,
            // non-negative values are castable to uint64_t unmodified
            .allocationSize=static_cast<VkDeviceSize>(size_in_bytes),
            .memoryTypeIndex=memory_type_index
        };
        VkResult result = vkAllocateMemory(device, &info, &vk_allocation_callbacks, &mem);
        switch (result) {
        case VK_SUCCESS:
            break;
        case VK_ERROR_TOO_MANY_OBJECTS:
            // exceeded maxMemoryAllocationCount (can be very small for some types such as protected resources)
            return ERROR_TOO_MANY_OBJECTS;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            // This can also occur if the requested size is larger than maxMemoryAllocationSize
            return ERROR_OUT_OF_MEMORY;
        case VK_ERROR_INVALID_EXTERNAL_HANDLE:
            return ERROR_INVALID_EXTERNAL_HANDLE;
        case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS_KHR:
            return ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS;
        default:
            return ERROR_UNKNOWN_VULKAN_ERROR;
        }
        blocks.push_back(mem);
        return size_in_bytes;
    }
    void BlockFree(VkDeviceMemory_T* ptr) {
        vkFreeMemory(device, ptr, &vk_allocation_callbacks);
    }

    void Clear() {
        for (VkDeviceMemory_T* ptr : blocks) {
            BlockFree(ptr);
        }
        blocks.clear();
    }
};


/*
Sparse block size- VkMemoryRequirements::alignment

Storage is owner of all allocation/data
Allocator represents a sequence of allocations managed together.  Storage can provide many allocators.

Fixed allocators can allocate a single instance or array of instances.  It can also allow multiple instances and/or
arrays to be within the same units of backing memory.  What is allocated from this allocator is considered to fixed
size and non-changing over time.

DynamicAllocators can allocate units of backing memory over time allowing for containers with dynamic sizing. 
*/
struct Page {
    VkDeviceMemory_T* raw_byes = nullptr;
    const int64_t num_bytes = 0;
    int64_t free_index = 0;
    int64_t remaining = 0;
};
struct PageReference { int64_t page_id=-1, index=-1; };


struct ContiguousAllocator {
    const int64_t unit_alignment;
    const int64_t page_size;
    StorageBlocks& storage_blocks;
    std::vector<VkDeviceMemory_T*> memory{};
    std::vector<int64_t> free_index{};
    std::vector<int64_t> remaining{};
    std::mutex mutex{};

    PageReference Allocate(const int64_t num_bytes) {
        int64_t actual_bytes = num_bytes + static_cast<int64_t>(num_bytes % unit_alignment > 0) * unit_alignment;
        PageReference pr = AllocFirstContiguous(actual_bytes);
        if (pr.page_id >= 0) { return pr; }
        return AllocNew(actual_bytes);
    };
    void deallocate() {};

    PageReference AllocFirstContiguous(int64_t N) {
        std::lock_guard<std::mutex> lock{mutex};
        auto it = std::ranges::find_if(remaining, [N](auto x) { return x >= N; });
        if (it == remaining.end()) { return {}; }
        size_t page_id = static_cast<size_t>(it - remaining.begin());
        int64_t mem_index = free_index[page_id];
        *it -= N;
        free_index[page_id] += N;
        return {.page_id=static_cast<int64_t>(page_id), .index=mem_index};
    }

    PageReference AllocNew(int64_t N) {
        int64_t num_page_blocks = N / page_size + static_cast<int64_t>(N % page_size > 0);
        int64_t total_block_bytes = num_page_blocks * page_size;
        VkDeviceMemory_T* mem = storage_blocks.BlockAllocate(total_blocks_bytes);
        if (!mem) { return {}; }
        std::lock_guard<std::mutex> lock{mutex};
        memory.push_back(mem);
        free_index.push_back(N);
        remaining.push_back(total_block_bytes - N);
        return {.page_id=static_cast<int64_t>(memory.size() - 1), .index=0};
    }
};
struct SparseAllocator{
    PageReference AllocNew(int64_t N) {
        auto [num_page_blocks, extra_page_bytes] = std::div(N, page_size);
        auto num_pages = num_page_blocks + static_cast<int64_t>(extra_page_bytes > 0);
        std::vector<VkDeviceMemory_T*> local_alloc{};
        local_alloc.reserve(static_cast<size_t>(num_pages));
        for (auto _ : std::views::iota(0, num_pages)) {
            local_alloc.push_back(storage_blocks.BlockAllocate());
        }
        return {};
    }
};
template <typename T, typename Allocator>
struct Instance{
    Allocator* allocator{nullptr};
    int64_t id{-1};
    Instance(FixedAllocator& allocator_) : allocator{&allocator_}, id(allocator_.allocate(sizeof(T))) {};
    Instance(const Instance&) = delete;
    ~Instance() { allocator->deallocate(id); }
    Instance& operator=(const Instance&) = delete;
    void Set(const T& value);
};
struct Array{};
struct ArraySparse{};
struct VectorSparse{};

struct Vertex{};
StorageBlocks storage_blocks{};
FixedAllocator fixed_allocator{};
Instance<Vertex> instance{fixed_allocator};


/*
Hierarchical allocators.  StorageBlock -> Allocator then new Allocators can be created using other backing allocators.  The lowest level
allocators allocate actual memory.  Higher tier allocators get blocks of data from the lower tier allocator and allocate within that.
Do you have to do anything special due to the layering?
*/




}
}




#include <bit>
#include <cstddef>
#include <memory_resource>


class DeviceMemoryResource : std::pmr::memory_resource {
};
class HostMemoryResource : std::pmr::memory_resource {
};


struct Allocator {
    [[nodiscard]] void* allocate(std::size_t bytes, std::size_t alignment=alignof(std::max_align_t));
    void deallocate(void* src);
};
class Memory {};
