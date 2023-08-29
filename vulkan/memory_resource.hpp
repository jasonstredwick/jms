#pragma once


#include <algorithm>
#include <bit>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <new>
#include <stdexcept>
#include <type_traits>

#include "jms/memory/allocation.hpp"
#include "jms/memory/resources.hpp"
#include "jms/vulkan/vulkan.hpp"


namespace jms {
namespace vulkan {


using DeviceMemoryAllocation = jms::memory::Allocation<VkDeviceMemory_T, vk::DeviceSize>;


// device.allocateMemory is thread safe : https://stackoverflow.com/questions/51528553/can-i-use-vkdevice-from-multiple-threads-concurrently
// device.allocateMemory implicitly includes a minimum alignment set by the driver applied in allocateMemory
// TODO: Use device props to determine what this minimum alignment value might be.
class DeviceMemoryResource : public jms::memory::Resource<DeviceMemoryAllocation> {
    using pointer_type = jms::memory::Resource<DeviceMemoryAllocation>::allocation_type::pointer_type;
    using size_type = jms::memory::Resource<DeviceMemoryAllocation>::allocation_type::size_type;

    vk::raii::Device* device;
    vk::AllocationCallbacks* vk_allocation_callbacks;
    uint32_t memory_type_index;

public:
    DeviceMemoryResource(vk::raii::Device& device,
                         vk::AllocationCallbacks& vk_allocation_callbacks,
                         uint32_t memory_type_index) noexcept
    : device{std::addressof(device)},
      vk_allocation_callbacks{std::addressof(vk_allocation_callbacks)},
      memory_type_index{memory_type_index}
    {}
    ~DeviceMemoryResource() noexcept override = default;

    [[nodiscard]] DeviceMemoryAllocation Allocate(size_type size) override {
        if (size < 1) { throw std::bad_alloc{}; }
        vk::raii::DeviceMemory dev_mem = device->allocateMemory({
            .allocationSize=size,
            .memoryTypeIndex=memory_type_index
        }, *vk_allocation_callbacks);
        return {.ptr=static_cast<pointer_type>(dev_mem.release()), .offset=0, .size=size};
    }

    void Deallocate(DeviceMemoryAllocation allocation) override {
        // delete upon leaving scope
        vk::raii::DeviceMemory dev_mem{*device, allocation.ptr, *vk_allocation_callbacks};
    }

    bool IsEqual(const jms::memory::Resource<DeviceMemoryAllocation>& other) const noexcept override {
        const DeviceMemoryResource* omr = dynamic_cast<const DeviceMemoryResource*>(std::addressof(other));
        return this->device                  == omr->device &&
               this->vk_allocation_callbacks == omr->vk_allocation_callbacks &&
               this->memory_type_index       == omr->memory_type_index;
    }

    vk::raii::Device& GetDevice() const noexcept { return *device; }
    vk::AllocationCallbacks& GetAllocationCallbacks() const noexcept { return *vk_allocation_callbacks; }
    uint32_t GetMemoryType() const noexcept { return memory_type_index; }
};


class DeviceMemoryResourceAligned : public DeviceMemoryResource {
    using pointer_type = jms::memory::Resource<DeviceMemoryAllocation>::allocation_type::pointer_type;
    using size_type = jms::memory::Resource<DeviceMemoryAllocation>::allocation_type::size_type;

    size_type alignment;

public:
    DeviceMemoryResourceAligned(vk::raii::Device& device,
                                vk::AllocationCallbacks& vk_allocation_callbacks,
                                uint32_t memory_type_index,
                                size_type alignment) noexcept
    : DeviceMemoryResource{device, vk_allocation_callbacks, memory_type_index},
      alignment{alignment}
    {}
    DeviceMemoryResourceAligned(const DeviceMemoryResourceAligned&) = delete;
    DeviceMemoryResourceAligned& operator=(const DeviceMemoryResourceAligned&) = delete;
    ~DeviceMemoryResourceAligned() noexcept override = default;

    [[nodiscard]] DeviceMemoryAllocation Allocate(size_type size) override {
        if (size < 1) { throw std::bad_alloc{}; }
        size = ((size / alignment) + static_cast<size_type>(size % alignment > 0)) * alignment;
        return DeviceMemoryResource::Allocate(size);
    }

    void Deallocate(DeviceMemoryAllocation allocation) override { DeviceMemoryResource::Deallocate(allocation); }

    bool IsEqual(const jms::memory::Resource<DeviceMemoryAllocation>& other) const noexcept override {
        const DeviceMemoryResourceAligned* o = dynamic_cast<const DeviceMemoryResourceAligned*>(std::addressof(other));
        return DeviceMemoryResource::IsEqual(other) && this->alignment == o->alignment;
    }
};


/***
 * This class allows for direct GPU device memory as normal system memory for things such as std::vector.  Due to
 * the method of allocation by c++, this cannot provide suballocation of device memory.  Instead each allocation
 * is mapped 1:1 DeviceMemory and do_allocate.  That is why I am using DeviceMemoryResource directly rather than
 * allowing an interface to an alternate.  In order to provide suballocation, this will require pooling and other
 * algorithms to work on top of this class such as std::pmr::monotonic_buffer_resource or jms/memory/strategies.
 *
 * *TODO: Use this class with different allocation strategies and resource types to determine the class should
 *        be constructed with a minimum Vulkan alignment.  If it does then use DeviceMemoryResourceAligned instead
 *        of DeviceMemoryResource to enforce a fixed alignment on every allocation.  This can help keep arrays
 *        within suballocated DeviceMemory separated using page/block/chunk units.
 */
template <template <typename> typename Container, typename Mutex_t/*=jms::NoMutex*/>
class HostVisibleDeviceMemory : public std::pmr::memory_resource {
    using pointer_type = Resource<DeviceMemoryAllocation>::allocation_type::pointer_type;
    using size_type = Resource<DeviceMemoryAllocation>::allocation_type::size_type;
    static_assert(std::is_convertible_v<size_t, size_type>,
                  "HostVisibleDeviceMemory::size_type is not convertible from size_t");

    struct Result {
        DeviceMemoryResource* upstream;
        DeviceMemoryAllocation allocation;
        Result(DeviceMemoryResource* a, DeviceMemoryAllocation b) : upstream{a}, allocation{b} {}
        ~Result() { if (allocation.ptr) { upstream->Deallocate(allocation); } }
        DeviceMemoryAllocation Reset() { allocation.ptr = nullptr; }
    };
    struct MappedData { DeviceMemoryAllocation allocation; void* mapped_ptr; };

    DeviceMemoryResource* upstream;
    Container<MappedData> allocations{};
    Mutex_t mutex{};

    void* do_allocate(size_t bytes, size_t alignment) override {
        if (!bytes || !std::has_single_bit(alignment)) { throw std::bad_alloc{}; }
        bytes = ((bytes / alignment) + static_cast<size_t>(bytes % alignment > 0)) * alignment;
        auto vk_size = static_cast<size_type>(bytes);
        Result result{upstream, upstream->Allocate(vk_size)};
        void* ptr = nullptr;
        if (vkMapMemory(*upstream->GetDevice(),
                        result.allocation.ptr , result.allocation.offset, result.allocation.size,
                        {}, &ptr) != VK_SUCCESS) {
            // `result` should deallocate allocation upon destruction.
            throw std::bad_alloc{};
        }
        std::lock_guard lock{mutex};
        allocations.push_back({.allocation=result.allocation, .mapped_ptr=ptr});
        result.Reset();
        return ptr;
    }

    void do_deallocate(void* p, [[maybe_unused]] size_t bytes, [[maybe_unused]] size_t alignment) override {
        std::lock_guard lock{mutex};
        auto it = std::ranges::find_if(allocations, [rhs=p](void* lhs) { return lhs == rhs; }, &MappedData::mapped_ptr);
        if (it == allocations.end()) {
            return;
            //throw std::runtime_error{"Unable to find allocation to destroy from mapped pointer."};
        }
        vkUnmapMemory(*upstream->GetDevice(), it->allocation.ptr);
        upstream->Deallocate(it->allocation);
        allocations.erase(it);
    }

    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == std::addressof(other);
    }

public:
    HostVisibleDeviceMemory(DeviceMemoryResource& upstream, size_t min_alignment=alignof(std::max_align_t)) noexcept
    : upstream{std::addressof(upstream)}, min_alignment{min_alignment}
    {}
    HostVisibleDeviceMemory(const HostVisibleDeviceMemory&) = delete;
    HostVisibleDeviceMemory& operator=(const HostVisibleDeviceMemory&) = delete;
    ~HostVisibleDeviceMemory() noexcept override { Clear(); }

    void Clear() {
        std::lock_guard lock{mutex};
        for (MappedData& data : allocations) {
            vkUnmapMemory(*upstream->GetDevice(), data.allocation.ptr);
            upstream->Deallocate(data.allocation);
        }
        allocations.clear();
    }
};


} // namespace vulkan
} // namespace jms
