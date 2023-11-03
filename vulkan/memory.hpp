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
#include <functional>
#include <iterator>
#include <limits>
#include <span>
#include <string_view>
#include <tuple>


#include "jms/vulkan/memory_resource.hpp"
#include "jms/vulkan/vulkan.hpp"


namespace jms {
namespace vulkan {


/***
 * Should I handle or provide a mechanism to handle fallback allocation when a heap is full or passes a threshold?
 * Fallback could also be defined as wrapper memory_resource or allocator that contains an ordered list of memory resources, etc
 * that checks for certain exceptions raised and call the next in line.  Should be easy to create the list using base class pointers.
*/
template <template <typename> typename Container_t_, typename Mutex_t_/*=jms::NoMutex*/>
class MemoryHelper {
    template <typename T> using Container_t = Container_t_<T>;
    using Mutex_t = Mutex_t_;

    struct DeviceMemoryData{
        DeviceMemoryResource dmr;
        uint32_t memory_type_index;
    };

    const vk::raii::PhysicalDevice* physical_device{nullptr};
    vk::raii::Device* device{nullptr};
    std::optional<vk::AllocationCallbacks*> vk_allocation_callbacks{std::nullopt};
    std::vector<DeviceMemoryData> memory_resources_data{};

public:
    MemoryHelper() noexcept = default;
    MemoryHelper(const vk::raii::PhysicalDevice& physical_device,
                 vk::raii::Device& device,
                 const std::vector<vk::MemoryPropertyFlags>& memory_resource_infos,
                 std::optional<vk::AllocationCallbacks*> vk_allocation_callbacks = std::nullopt)
    : physical_device{std::addressof(physical_device)},
      device{std::addressof(device)},
      vk_allocation_callbacks{vk_allocation_callbacks}
    { Init(memory_resource_infos); }
    MemoryHelper(const MemoryHelper&) = delete;
    MemoryHelper(MemoryHelper&&) noexcept = default; // no destruction and std::vector
    ~MemoryHelper() noexcept = default;
    MemoryHelper& operator=(const MemoryHelper&) = delete;
    MemoryHelper& operator=(MemoryHelper&&) noexcept = default;

    uint32_t GetMemoryTypeIndex(size_t memory_resource_id) const {
        return memory_resources_data.at(memory_resource_id).memory_type_index;
    }
    DeviceMemoryResource& GetResource(size_t memory_resource_id) {
        return memory_resources_data.at(memory_resource_id).dmr;
    }

    DeviceMemoryResource CreateDirectMemoryResource(uint32_t memory_type_index) {
        // validate requested index ...
        //     { throw std::runtime_error{"CreateDirectMemoryResource requires valid memory_type_index."}; }
        return {*device, memory_type_index, vk_allocation_callbacks};
    }

    auto CreateDeviceMemoryResourceMapped(size_t memory_resource_id) {
        auto& data = memory_resources_data.at(memory_resource_id);
        auto props = physical_device->getMemoryProperties();
        auto flags = props.memoryTypes[data.memory_type_index].propertyFlags;
        if (!IsDeviceMemoryResourceMappedCapableMemoryType(flags)) {
            throw std::runtime_error{"CreateDeviceMemoryResourceMapped: invalid memory_type_index provided."};
        }
        return DeviceMemoryResourceMapped<Container_t, Mutex_t>{data.dmr};
    }

    auto CreateImageAllocator(size_t memory_resource_id) {
        auto& data = memory_resources_data.at(memory_resource_id);
        return ImageResourceAllocator<decltype(Container_t), Mutex_t>{data.dmr, data.memory_type_index, *device};
    }

    template <typename MemoryResource_t> // template <MemoryResource_c T>
    MemoryResource_t CreateMemoryResource(auto&&... args) { return {std::forward<decltype(args)>(args)...}; }

    template <typename Resource_t> // template <Resource_c T>
    Resource_t CreateResource(auto&&... args) { return {std::forward<decltype(args)>(args)...}; }

    uint32_t GetDeviceLocalMemoryType(bool should_throw = true) {
        auto props = physical_device->getMemoryProperties();
        for (auto i : std::views::iota(static_cast<uint32_t>(0), props.memoryTypeCount)) {
            auto flags = props.memoryTypes[i].propertyFlags;
            if (IsDeviceLocal(flags)) { return i; }
        }
        if (should_throw) { throw std::runtime_error{"Failed to find suitable device local memory type."}; }
        return std::numeric_limits<uint32_t>::max();
    }

    uint32_t GetDeviceMemoryResourceMappedCapableMemoryType(vk::MemoryPropertyFlags opt_flags = {},
                                                            bool should_throw = true) {
        auto props = physical_device->getMemoryProperties();
        for (auto i : std::views::iota(static_cast<uint32_t>(0), props.memoryTypeCount)) {
            auto flags = props.memoryTypes[i].propertyFlags;
            if (IsDeviceMemoryResourceMappedCapableMemoryType(flags) && (flags & opt_flags) == opt_flags) {
                return i;
            }
        }
        if (should_throw) { throw std::runtime_error{"Failed to find suitable mapped capable memory type."}; }
        return std::numeric_limits<uint32_t>::max();
    }

    bool IsDeviceLocal(vk::MemoryPropertyFlags flags) noexcept {
        return static_cast<bool>(flags & vk::MemoryPropertyFlagBits::eDeviceLocal);
    }

    bool IsDeviceMemoryResourceMappedCapableMemoryType(vk::MemoryPropertyFlags flags) noexcept {
        return static_cast<bool>(flags & vk::MemoryPropertyFlagBits::eHostVisible) &&
               static_cast<bool>(flags & vk::MemoryPropertyFlagBits::eHostCoherent) &&
               //!static_cast<bool>(flags & vk::MemoryPropertyFlagBits::eHostCached) &&
               !static_cast<bool>(flags & vk::MemoryPropertyFlagBits::eLazilyAllocated);
    }

    bool IsMemoryTypeIndexOutOfBounds(uint32_t i) { return i == std::numeric_limits<uint32_t>::max(); }

private:
    void Init(const std::vector<vk::MemoryPropertyFlags>& memory_resources_flags) {
        vk::PhysicalDeviceMemoryProperties props = physical_device->getMemoryProperties();
        std::ranges::transform(memory_resources_flags, std::back_inserter(memory_resources_data),
            [this, &props](auto& mr_flags) -> DeviceMemoryData {
                for (auto index : std::views::iota(static_cast<uint32_t>(0), props.memoryTypeCount)) {
                    auto index_flags = props.memoryTypes[index].propertyFlags;
                    if ((mr_flags & index_flags) == mr_flags) {
                        return {.dmr=this->CreateDirectMemoryResource(index), .memory_type_index=index};
                    }
                }
                throw std::runtime_error{"Failed to find suitable capable memory type."};
            });
    }
};


}
}
