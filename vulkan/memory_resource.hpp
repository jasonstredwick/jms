#pragma once


#include <algorithm>
#include <bit>
#include <functional>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <new>
#include <optional>
#include <stdexcept>
#include <vector>
#include <type_traits>
#include <utility>

#include "jms/memory/allocation.hpp"
#include "jms/memory/resources.hpp"
#include "jms/vulkan/vulkan.hpp"
#include "jms/vulkan/info.hpp"
#include "jms/vulkan/utils.hpp"


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

    vk::raii::Device* device{nullptr};
    vk::AllocationCallbacks* vk_allocation_callbacks{nullptr};
    uint32_t memory_type_index{0};

public:
    DeviceMemoryResource(vk::raii::Device& device,
                         uint32_t memory_type_index,
                         std::optional<vk::AllocationCallbacks*> vk_allocation_callbacks = std::nullopt) noexcept
    : device{std::addressof(device)},
      vk_allocation_callbacks{vk_allocation_callbacks.value_or(nullptr)},
      memory_type_index{memory_type_index}
    {}
    DeviceMemoryResource(const DeviceMemoryResource&) = default;
    DeviceMemoryResource(DeviceMemoryResource&&) noexcept = default;
    ~DeviceMemoryResource() noexcept override = default;
    DeviceMemoryResource& operator=(const DeviceMemoryResource&) = default;
    DeviceMemoryResource& operator=(DeviceMemoryResource&&) = default;

    [[nodiscard]] DeviceMemoryAllocation Allocate(size_type size, 
                                                  [[maybe_unused]] size_type data_alignment,
                                                  [[maybe_unused]] size_type pointer_alignment) override {
        if (size < 1) { throw std::bad_alloc{}; }
        vk::raii::DeviceMemory dev_mem = device->allocateMemory({
            .allocationSize=size,
            .memoryTypeIndex=memory_type_index
        }, vk_allocation_callbacks);
        pointer_type ptr = static_cast<pointer_type>(dev_mem.release());
        return {.ptr=ptr, .offset=0, .size=size};
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
        DeviceMemoryResource* upstream{nullptr};
        DeviceMemoryAllocation allocation{};
        Result(DeviceMemoryResource* a, DeviceMemoryAllocation b) : upstream{a}, allocation{b} {}
        ~Result() { if (allocation.ptr) { upstream->Deallocate(allocation); } }
        void Reset() { allocation.ptr = nullptr; }
    };
    struct MappedData { DeviceMemoryAllocation allocation; void* mapped_ptr; };

    DeviceMemoryResource* upstream{nullptr};
    Container_t<MappedData> allocations{};
    Mutex_t mutex{};

public:
    DeviceMemoryResourceMapped(DeviceMemoryResource& upstream) noexcept : upstream{std::addressof(upstream)} {}
    DeviceMemoryResourceMapped(const DeviceMemoryResourceMapped&) = delete;
    DeviceMemoryResourceMapped(DeviceMemoryResourceMapped&& other) noexcept { *this = other; }
    ~DeviceMemoryResourceMapped() noexcept override { Clear(); }
    DeviceMemoryResourceMapped& operator=(const DeviceMemoryResourceMapped&) = delete;
    DeviceMemoryResourceMapped& operator=(DeviceMemoryResourceMapped&& other) noexcept {
        std::scoped_lock lock{mutex, other.mutex};
        upstream = std::exchange(other.upstream, nullptr);
        allocations = std::move(other.allocations);
        return *this;
    }

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
            .pQueueFamilyIndices=VectorAsPtr(sharing_queue_family_indices)
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

private:
    void* do_allocate(size_t bytes, size_t alignment) override {
        if (!bytes || !std::has_single_bit(alignment)) { throw std::bad_alloc{}; }
        bytes = ((bytes / alignment) + static_cast<size_t>(bytes % alignment > 0)) * alignment;
        auto vk_size = static_cast<size_type>(bytes);
        Result result{upstream, upstream->Allocate(vk_size, 1, alignment)};
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
};


template <typename ResourceAllocation_t,
          typename RAII_t,
          template <typename> typename Container_t_,
          typename Mutex_t_/*=jms::NoMutex*/>
class ResourceAllocator {
public:
    using pointer_type = ResourceAllocation_t::pointer_type;
    using size_type = ResourceAllocation_t::size_type;
    template <typename T> using Container_t = Container_t_<T>;
    using Mutex_t = Mutex_t_;

private:
    struct Unit {
        DeviceMemoryAllocation mem;
        pointer_type res_ptr;
    };

    Container_t<Unit> units{};
    jms::memory::Resource<DeviceMemoryAllocation>* memory_resource{nullptr};
    uint32_t memory_resource_type_index_bit{0};
    vk::raii::Device* device{nullptr};
    vk::AllocationCallbacks* vk_allocation_callbacks{nullptr};
    Mutex_t mutex{};

public:
    ResourceAllocator(jms::memory::Resource<DeviceMemoryAllocation>& memory_resource,
                      uint32_t memory_resource_type_index,
                      vk::raii::Device& device,
                      std::optional<vk::AllocationCallbacks*> vk_allocation_callbacks = std::nullopt)
    : memory_resource{std::addressof(memory_resource)},
      memory_resource_type_index_bit{static_cast<uint32_t>(1) << memory_resource_type_index},
      device{std::addressof(device)},
      vk_allocation_callbacks{vk_allocation_callbacks.value_or(nullptr)}
    {}
    ResourceAllocator(const ResourceAllocator&) = delete;
    ResourceAllocator(ResourceAllocator&& other) noexcept { *this = other; }
    ~ResourceAllocator() noexcept { Clear(); }
    ResourceAllocator& operator=(const ResourceAllocator&) = delete;
    ResourceAllocator& operator=(ResourceAllocator&& other) noexcept {
        std::scoped_lock lock{mutex, other.mutex};
        units = std::move(units);
        memory_resource = std::exchange(other.memory_resource, nullptr);
        memory_resource_type_index_bit = other.memory_resource_type_index_bit;
        device = std::exchange(other.device, nullptr);
        vk_allocation_callbacks = std::exchange(other.vk_allocation_callbacks, nullptr);
        return *this;
    }

    [[nodiscard]] ResourceAllocation_t Allocate(const auto& info) {
        RAII_t resource = RAII_t{*device, info.ToCreateInfo(), vk_allocation_callbacks};
        vk::MemoryRequirements reqs = resource.getMemoryRequirements();
        if (!static_cast<bool>(reqs.memoryTypeBits & memory_resource_type_index_bit)) {
            throw std::runtime_error{"Cannot allocate resource with the given allocated device memory."};
        }
        // TODO: Handle deallocation of device memory and resource in failure scenarios.
        auto allocation = memory_resource->Allocate(reqs.size, 1, reqs.alignment);
        resource.bindMemory(allocation.ptr, allocation.offset);
        auto ptr = resource.release();
        std::lock_guard<Mutex_t> lock{mutex};
        units.push_back({.mem=allocation, .res_ptr=ptr});
        return {.ptr=ptr, .offset=0, .size=reqs.size};
    }

    void Clear() {
        std::lock_guard<Mutex_t> lock{mutex};
        for (Unit& unit : units) { DestroyUnit(unit); }
        units.clear();
    }

    void Deallocate(ResourceAllocation_t allocation) {
        std::lock_guard<Mutex_t> lock{mutex};
        auto it = std::ranges::find_if(units, [rhs=allocation.ptr](auto lhs) { return lhs == rhs; },
                                       &Unit::res_ptr);
        if (it == units.end()) { throw std::runtime_error{"Unable to find allocated resource for deallocation."}; }
        DestroyUnit(*it);
        units.erase(it);
    }

    std::optional<vk::AllocationCallbacks*> GetAllocationCallbacks() noexcept { return vk_allocation_callbacks; }

    vk::raii::Device& GetDevice() noexcept { return *device; }

    bool IsEqual(const ResourceAllocator& other) const noexcept { return this == std::addressof(other); }

private:
    void DestroyUnit(Unit& unit) {
        RAII_t resource{*device, unit.res_ptr, vk_allocation_callbacks};
        resource.clear();
        memory_resource->Deallocate(unit.mem);
    }
};


template <template <typename> typename Container_t, typename Mutex_t/*=jms::NoMutex*/>
using BufferResourceAllocator = ResourceAllocator<BufferAllocation, vk::raii::Buffer, Container_t, Mutex_t>;


template < template <typename> typename Container_t, typename Mutex_t/*=jms::NoMutex*/>
using ImageResourceAllocator = ResourceAllocator<ImageAllocation, vk::raii::Image, Container_t, Mutex_t>;








template <template <typename> typename Container_t, typename Mutex_t/*=jms::NoMutex*/>
class Buffer {
public:
    using Allocator_t = BufferResourceAllocator<Container_t, Mutex_t>;

private:
    Allocator_t* allocator{nullptr};
    VkBuffer_T* ptr{nullptr};
    vk::DeviceSize size{0};

public:
    Buffer() noexcept = default;
    Buffer(Allocator_t& allocator_in, const BufferInfo& info) : allocator{std::addressof(allocator_in)} {
        BufferAllocation allocation = allocator->Allocate(info);
        ptr = allocation.ptr;
        size = allocation.size;
    }
    Buffer(const Buffer&) = delete;
    Buffer(Buffer&& other) noexcept { *this = other; }
    ~Buffer() noexcept { if (ptr) { allocator->Deallocate({.ptr=ptr, .size=size}); } }
    Buffer& operator=(const Buffer&) = delete;
    Buffer& operator=(Buffer&& other) noexcept {
        allocator = std::exchange(other.allocator, nullptr);
        ptr = std::exchange(other.ptr, nullptr);
        size = std::exchange(other.size, 0);
        return *this;
    }

    vk::DescriptorBufferInfo AsDescriptorInfo() const noexcept { return {.buffer=ptr, .offset=0, .range=size}; }
};


template <template <typename> typename Container_t, typename Mutex_t/*=jms::NoMutex*/>
class Image {
public:
    using Allocator_t = ImageResourceAllocator<Container_t, Mutex_t>;

private:
    Allocator_t* allocator{nullptr};
    VkImage_T* ptr{nullptr};
    vk::DeviceSize size{0};

public:
    Image() noexcept = default;
    Image(Allocator_t& allocator_in, const ImageInfo& info) : allocator{std::addressof(allocator_in)} {
        ImageAllocation allocation = allocator->Allocate(info);
        ptr = allocation.ptr;
        size = allocation.size;
    }
    Image(const Image&) = delete;
    Image(Image&& other) noexcept { *this = std::move(other); }
    ~Image() noexcept { if (ptr) { allocator->Deallocate({.ptr=ptr, .size=size}); } }
    Image& operator=(const Image&) = delete;
    Image& operator=(Image&& other) noexcept {
        allocator = std::exchange(other.allocator, nullptr);
        ptr = std::exchange(other.ptr, nullptr);
        size = std::exchange(other.size, 0);
        return *this;
    }

    vk::Image AsVkImage() { return vk::Image{ptr}; }

    vk::raii::ImageView CreateView(const ImageViewInfo& info) const {
        vk::raii::Device& device = allocator->GetDevice();
        std::optional<vk::AllocationCallbacks*> vk_allocation_callbacks = allocator->GetAllocationCallbacks();
        vk::ImageViewCreateInfo create_info = info.ToCreateInfo(vk::Image{ptr});
        vk::raii::ImageView iv = device.createImageView(create_info, vk_allocation_callbacks.value_or(nullptr));
        return iv;
    }

    vk::DescriptorImageInfo AsDescriptorInfo() const noexcept {
        return vk::DescriptorImageInfo{
            .sampler={},
            .imageView={},
            .imageLayout={}
        };
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




#if 0
class Allocator {
    jms::memory::Resource<DeviceMemoryAllocation>* upstream;
    vk::raii::Device* device;
    vk::AllocationCallbacks* vk_allocation_callbacks;

public:
    using size_type = DeviceMemoryAllocation::size_type;

    VkBuffer_T* RawBuffer(size_type size, const BufferInfo& info) { size_type total_size = size; return nullptr; }
    VkImage_T* Image(const ImageInfo& info) { return nullptr; }
    template <typename T> VkBuffer_T* Object(const BufferInfo& info);
    template <typename T> VkBuffer_T* Object(size_type data_over_alignment, const BufferInfo& info);
    template <typename T> VkBuffer_T* Array(size_type num_T, const BufferInfo& info);
    template <typename T> VkBuffer_T* Array(size_type num_T, size_type data_over_alignment, size_type array_over_alignment, const BufferInfo& info);
};


template <typename MemoryAllocation_t, template <typename> typename Container_t, typename Mutex_t/*=jms::NoMutex*/>
class ImageResourceAllocator
: public ResourceAllocatorBase<ImageAllocation, vk::raii::Image, MemoryAllocation_t, Container_t, Mutex_t>
{
public:
    using size_type = ImageAllocation::size_type;
    using ResourceAllocatorBase_t = ResourceAllocatorBase<ImageAllocation, vk::raii::Image, MemoryAllocation_t,
                                                          Container_t, Mutex_t>;

    ImageResourceAllocator(jms::memory::Resource<MemoryAllocation_t>& memory_resource,
                           uint32_t memory_resource_type_index,
                           vk::raii::Device& device,
                           std::optional<vk::AllocationCallbacks*> vk_allocation_callbacks = std::nullopt)
    : ResourceAllocatorBase_t{memory_resource, memory_resource_type_index, device, vk_allocation_callbacks}
    {}
    ImageResourceAllocator(const ImageResourceAllocator&) = delete;
    ImageResourceAllocator& operator=(const ImageResourceAllocator&) = delete;
    ~ImageResourceAllocator() noexcept override = default;

    [[nodiscard]] ImageAllocation Allocate(const ImageInfo& info) { return DoAllocate(info.To()); }
    void Deallocate(ImageAllocation allocation) { DoDeallocate(allocation); }
    bool IsEqual(const BufferResourceAllocator& other) const noexcept { return DoIsEqual(other); }
};
#endif
