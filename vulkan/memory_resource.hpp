#pragma once


#include <algorithm>
#include <bit>
#include <functional>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <new>
#include <stdexcept>
#include <vector>
#include <type_traits>

#include "jms/memory/allocation.hpp"
#include "jms/memory/resources.hpp"
#include "jms/vulkan/vulkan.hpp"


namespace jms {
namespace vulkan {


using BufferAllocation = jms::memory::Allocation<VkBuffer_T, vk::DeviceSize>;
using DeviceMemoryAllocation = jms::memory::Allocation<VkDeviceMemory_T, vk::DeviceSize>;
using ImageAllocation = jms::memory::Allocation<VkImage_T, vk::DeviceSize>;


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
                         uint32_t memory_type_index,
                         vk::AllocationCallbacks* vk_allocation_callbacks = nullptr) noexcept
    : device{std::addressof(device)},
      vk_allocation_callbacks{vk_allocation_callbacks},
      memory_type_index{memory_type_index}
    {}
    ~DeviceMemoryResource() noexcept override = default;

    [[nodiscard]] DeviceMemoryAllocation Allocate(size_type size) override {
        if (size < 1) { throw std::bad_alloc{}; }
        vk::raii::DeviceMemory dev_mem = device->allocateMemory({
            .allocationSize=size,
            .memoryTypeIndex=memory_type_index
        }, vk_allocation_callbacks);
        return {.ptr=static_cast<pointer_type>(dev_mem.release()), .offset=0, .size=size};
    }

    void Deallocate(DeviceMemoryAllocation allocation) override {
        // delete upon leaving scope
        vk::raii::DeviceMemory dev_mem{*device, allocation.ptr, vk_allocation_callbacks};
    }

    bool IsEqual(const jms::memory::Resource<DeviceMemoryAllocation>& other) const noexcept override {
        const DeviceMemoryResource* omr = dynamic_cast<const DeviceMemoryResource*>(std::addressof(other));
        return this->device                  == omr->device &&
               this->vk_allocation_callbacks == omr->vk_allocation_callbacks &&
               this->memory_type_index       == omr->memory_type_index;
    }

    vk::raii::Device& GetDevice() const noexcept { return *device; }
    vk::AllocationCallbacks* GetAllocationCallbacks() const noexcept { return vk_allocation_callbacks; }
    uint32_t GetMemoryTypeIndex() const noexcept { return memory_type_index; }
};


class DeviceMemoryResourceAligned : public DeviceMemoryResource {
    using pointer_type = jms::memory::Resource<DeviceMemoryAllocation>::allocation_type::pointer_type;
    using size_type = jms::memory::Resource<DeviceMemoryAllocation>::allocation_type::size_type;

    size_type alignment;

public:
    DeviceMemoryResourceAligned(vk::raii::Device& device,
                                uint32_t memory_type_index,
                                size_type alignment,
                                vk::AllocationCallbacks* vk_allocation_callbacks = nullptr) noexcept
    : DeviceMemoryResource{device, memory_type_index, vk_allocation_callbacks},
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


template <typename ResourceAllocation_t,
          typename RAII_t,
          typename MemoryAllocation_t,
          template <typename> typename Container_t,
          typename Mutex_t/*=jms::NoMutex*/>
class ResourceBase : jms::memory::Resource<ResourceAllocation_t> {
protected:
    using pointer_type = ResourceAllocation_t::pointer_type;
    using size_type = ResourceAllocation_t::size_type;

    struct Unit {
        MemoryAllocation_t mem;
        pointer_type res_ptr;
    };

    Container_t<Unit> units{};
    jms::memory::Resource<MemoryAllocation_t>* memory_resource;
    vk::raii::Device* device;
    vk::AllocationCallbacks* vk_allocation_callbacks;
    Mutex_t mutex{};

    void Clear() {
        std::lock_guard<Mutex_t> lock{mutex};
        for (Unit& unit : units) { DestroyUnit(unit); }
        units.clear();
    }

    void DestroyUnit(Unit& unit) {
        RAII_t resource{*device, unit.res_ptr, vk_allocation_callbacks};
        resource.clear();
        memory_resource->Deallocate(unit.mem);
    }

    [[nodiscard]] ResourceAllocation_t DoAllocate(size_type size, RAII_t&& resource) {
        std::lock_guard<Mutex_t> lock{mutex};
        auto allocation = memory_resource->Allocate(size);
        resource.bindMemory(allocation.ptr, allocation.offset);
        auto ptr = resource.release();
        units.push_back({.mem=allocation, .res_ptr=ptr});
        return {.ptr=ptr, .offset=0, .size=size};
    }

    void DoDeallocate(ResourceAllocation_t allocation) {
        std::lock_guard<Mutex_t> lock{mutex};
        auto it = std::ranges::find_if(units, [rhs=allocation.res_ptr](auto lhs) { return lhs == rhs; },
                                       &Unit::res_ptr);
        if (it == units.end()) { throw std::runtime_error{"Unable to find allocated resource for deallocation."}; }
        DestroyUnit(*it);
        units.erase(it);
    }

public:
    ResourceBase(jms::memory::Resource<MemoryAllocation_t>& memory_resource,
                 vk::raii::Device& device,
                 vk::AllocationCallbacks* vk_allocation_callbacks)
    : memory_resource{std::addressof(memory_resource)},
      device{std::addressof(device)},
      vk_allocation_callbacks{vk_allocation_callbacks}
    {}
    ResourceBase(const ResourceBase&) = delete;
    ResourceBase& operator=(const ResourceBase&) = delete;
    ~ResourceBase() noexcept override { Clear(); }
};


template <typename MemoryAllocation_t, template <typename> typename Container_t, typename Mutex_t/*=jms::NoMutex*/>
class BufferResource : public ResourceBase<BufferAllocation, vk::raii::Buffer, MemoryAllocation_t, Container_t, Mutex_t> {
    using size_type = BufferAllocation::size_type;

    vk::BufferCreateFlags create_flags;
    vk::BufferUsageFlags usage_flags;
    vk::SharingMode sharing_mode;
    const std::vector<uint32_t>& sharing_queue_family_indices;

public:
    BufferResource(jms::memory::Resource<MemoryAllocation_t>& memory_resource,
                   vk::raii::Device& device,
                   vk::BufferUsageFlags usage_flags,
                   vk::BufferCreateFlags create_flags = {},
                   const std::vector<uint32_t>& sharing_queue_family_indices = {},
                   vk::AllocationCallbacks* vk_allocation_callbacks = nullptr)
    : ResourceBase{memory_resource, device, vk_allocation_callbacks},
      create_flags{create_flags},
      usage_flags{usage_flags},
      sharing_mode{sharing_queue_family_indices.empty() ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent},
      sharing_queue_family_indices{sharing_queue_family_indices}
    {}
    BufferResource(const BufferResource&) = delete;
    BufferResource& operator=(const BufferResource&) = delete;
    ~BufferResource() noexcept override = default;

    [[nodiscard]] BufferAllocation Allocate(size_type size) override {
        if (size < 1) { throw std::bad_alloc{}; }
        return DoAllocate(size, this->device->createBuffer({
            .flags=create_flags,
            .size=size,
            .usage=usage_flags,
            .sharingMode=sharing_mode,
            .queueFamilyIndexCount=static_cast<uint32_t>(sharing_queue_family_indices.size()),
            .pQueueFamilyIndices=sharing_queue_family_indices.data()
        }, this->vk_allocation_callbacks));
    }

    void Deallocate(BufferAllocation allocation) override { this->DoDeallocate(allocation); }

    bool IsEqual(const jms::memory::Resource<BufferAllocation>& other) const noexcept override {
        const BufferResource* res = dynamic_cast<const BufferResource*>(std::addressof(other));
        return this->memory_resource              == res->memory_resource &&
               this->device                       == res->device &&
               this->vk_allocation_callbacks      == res->vk_allocation_callbacks &&
               this->create_flags                 == res->create_flags &&
               this->usage_flags                  == res->usage_flags &&
               this->sharing_mode                 == res->sharing_mode &&
               this->sharing_queue_family_indices == res->sharing_queue_family_indices;
    }
};


/*
template <typename MemoryAllocation_t, template <typename> typename Container_t, typename Mutex_t>
class ImageResource : public ResourceBase<ImageAllocation, vk::raii::Image, MemoryAllocation_t, Container_t, Mutex_t> {
    using size_type = ImageAllocation::size_type;

    vk::BufferCreateFlags create_flags;
    vk::BufferUsageFlags usage_flags;
    vk::SharingMode sharing_mode;
    const std::vector<uint32_t>& sharing_queue_family_indices;

public:
    ImageResource(jms::memory::Resource<MemoryAllocation_t>& memory_resource,
                  vk::raii::Device& device,
                  vk::BufferUsageFlags usage_flags,
                  vk::BufferCreateFlags create_flags = {},
                  const std::vector<uint32_t>& sharing_queue_family_indices = {},
                  vk::AllocationCallbacks* vk_allocation_callbacks = nullptr)
    : ResourceBase{memory_resource, device, vk_allocation_callbacks},
      create_flags{create_flags},
      usage_flags{usage_flags},
      sharing_mode{sharing_queue_family_indices.empty() ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent},
      sharing_queue_family_indices{sharing_queue_family_indices}
    {}
    ImageResource(const ImageResource&) = delete;
    ImageResource& operator=(const ImageResource&) = delete;
    ~ImageResource() noexcept override = default;

    [[nodiscard]] ImageAllocation Allocate(size_type size) override {
        if (size < 1) { throw std::bad_alloc{}; }
        return DoAllocate(size, device->createImage({
#if 0
            .flags=create_flags,
            .size=size,
            .usage=usage_flags,
            .sharingMode=sharing_mode,
            .queueFamilyIndexCount=static_cast<uint32_t>(sharing_queue_family_indices.size()),
            .pQueueFamilyIndices=sharing_queue_family_indices.data()
#endif
        }, vk_allocation_callbacks));
    }

    void Deallocate(ImageAllocation allocation) override { DoDeallocate(allocation); }

    bool IsEqual(const jms::memory::Resource<BufferAllocation>& other) const noexcept override {
        const ImageResource* res = dynamic_cast<const ImageResource*>(std::addressof(other));
        return this->memory_resource              == res->memory_resource &&
               this->device                       == res->device &&
               this->vk_allocation_callbacks      == res->vk_allocation_callbacks &&
               this->create_flags                 == res->create_flags &&
               this->usage_flags                  == res->usage_flags &&
               this->sharing_mode                 == res->sharing_mode &&
               this->sharing_queue_family_indices == res->sharing_queue_family_indices;
    }
};
*/


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
template <template <typename> typename Container_t, typename Mutex_t/*=jms::NoMutex*/>
class DeviceMemoryResourceMapped : public std::pmr::memory_resource {
    using pointer_type = jms::memory::Resource<DeviceMemoryAllocation>::allocation_type::pointer_type;
    using size_type = jms::memory::Resource<DeviceMemoryAllocation>::allocation_type::size_type;
    static_assert(std::is_convertible_v<size_t, size_type>,
                  "DeviceMemoryResourceMapped::size_type is not convertible from size_t");

    struct Result {
        DeviceMemoryResource* upstream;
        DeviceMemoryAllocation allocation;
        Result(DeviceMemoryResource* a, DeviceMemoryAllocation b) : upstream{a}, allocation{b} {}
        ~Result() { if (allocation.ptr) { upstream->Deallocate(allocation); } }
        void Reset() { allocation.ptr = nullptr; }
    };
    struct MappedData { DeviceMemoryAllocation allocation; void* mapped_ptr; };

    DeviceMemoryResource* upstream;
    Container_t<MappedData> allocations{};
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
    DeviceMemoryResourceMapped(DeviceMemoryResource& upstream) noexcept : upstream{std::addressof(upstream)} {}
    DeviceMemoryResourceMapped(const DeviceMemoryResourceMapped&) = delete;
    DeviceMemoryResourceMapped& operator=(const DeviceMemoryResourceMapped&) = delete;
    ~DeviceMemoryResourceMapped() noexcept override { Clear(); }

    void Clear() {
        std::lock_guard lock{mutex};
        for (MappedData& data : allocations) {
            vkUnmapMemory(*upstream->GetDevice(), data.allocation.ptr);
            upstream->Deallocate(data.allocation);
        }
        allocations.clear();
    }

    vk::raii::Buffer AsBuffer(void* p,
                              size_t size_in_bytes,
                              vk::BufferUsageFlags usage_flags,
                              vk::BufferCreateFlags create_flags = {},
                              const std::vector<uint32_t>& sharing_queue_family_indices = {}) {
        vk::raii::Buffer buffer = upstream->GetDevice().createBuffer({
            .flags=create_flags,
            .size=static_cast<vk::DeviceSize>(size_in_bytes),
            .usage=usage_flags,
            .sharingMode=(sharing_queue_family_indices.empty() ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent),
            .queueFamilyIndexCount=static_cast<uint32_t>(sharing_queue_family_indices.size()),
            .pQueueFamilyIndices=sharing_queue_family_indices.data()
        }, upstream->GetAllocationCallbacks());
        auto reqs = buffer.getMemoryRequirements();

        std::lock_guard lock{mutex};
        auto it = std::ranges::find_if(allocations, [rhs=p](void* lhs) { return lhs == rhs; }, &MappedData::mapped_ptr);
        if (it == allocations.end()) { throw std::runtime_error{"Unable to find allocation to generate a buffer."}; }

        // verify mapped memory can be used for this buffer or throw exception
        if (!(1 << upstream->GetMemoryTypeIndex()) & reqs.memoryTypeBits) {
            throw std::runtime_error{"DeviceMemoryResourceMapped not compatible with VkBuffer memory types."};
        } else if (size_in_bytes != reqs.size) {
            throw std::runtime_error{"DeviceMemoryResourceMapped size mismatch with VkBuffer."};
        }
        // alignment should always work since each allocation is direct from DeviceMemory and offset is zero.

        buffer.bindMemory(it->allocation.ptr, 0);

        return buffer;
    }
};


template <typename T>
class UniqueMappedResource {
public:
    using allocator_type = std::pmr::polymorphic_allocator<>;
    using element_type = T;
    using pointer = T*;

private:
    allocator_type* resource{nullptr};
    pointer ptr{nullptr};

public:
    UniqueMappedResource() noexcept = default;
    UniqueMappedResource(allocator_type& r, pointer p)
    : resource{std::addressof(r)}, ptr{p}
    { if (ptr == nullptr) { throw std::runtime_error{"Invalid pointer provided to UniqueMappedResource"}; } }
    UniqueMappedResource(allocator_type& r, auto&&... args)
    : resource{std::addressof(r)}, ptr{r.new_object(std::forward<decltype(args)>(args)...)}
    { if (ptr == nullptr) { throw std::bad_alloc{}; } }
    UniqueMappedResource(const UniqueMappedResource&) = delete;
    ~UniqueMappedResource() noexcept { if (resource && ptr) { resource->delete_object(ptr); } }
    UniqueMappedResource& operator=(const UniqueMappedResource&) = delete;

    pointer get() const noexcept { return ptr; }

    allocator_type& get_allocator() const {
        if (resource) { return *resource; }
        throw std::runtime_error{"Attempting to dereference null UniqueMappedResource."};
    }

    typename std::add_lvalue_reference<T>::type operator*() const noexcept(noexcept(*std::declval<pointer>())) {
        if (ptr) { return *ptr; }
        throw std::runtime_error{"Attempting to dereference null UniqueMappedResource."};
    }

    pointer operator->() const noexcept { return ptr; }

    explicit operator bool() const noexcept { return resource != nullptr && ptr != nullptr; }

    pointer release() noexcept { return std::exchange(ptr, nullptr); }

    void reset(allocator_type& r, pointer p) noexcept {
        resource = std::addressof(r);
        ptr = p;
        if (ptr == nullptr) { throw std::runtime_error{"Invalid pointer provided to UniqueMappedResource"}; }
    }

    void reset(allocator_type& r, auto&&... args) {
        resource = std::addressof(r);
        ptr = resource->new_object(std::forward<decltype(args)>(args)...);
        if (ptr == nullptr) { throw std::bad_alloc{}; }
    }

    void reset(auto&&... args) {
        ptr = resource->new_object(std::forward<decltype(args)>(args)...);
        if (ptr == nullptr) { throw std::bad_alloc{}; }
    }

    void swap(UniqueMappedResource& other) noexcept {
        std::swap(resource, other.resource);
        std::swap(ptr, other.ptr);
    }
};


} // namespace vulkan
} // namespace jms
