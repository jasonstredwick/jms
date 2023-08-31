#pragma once


#include <algorithm>
#include <functional>
#include <stdexcept>
#include <vector>

#include "jms/memory/allocation.hpp"
#include "jms/memory/resources.hpp"
#include "jms/utils/no_mutex.hpp"
#include "jms/vulkan/vulkan.hpp"


namespace jms {
namespace vulkan {


using BufferAllocation = jms::memory::Allocation<VkBuffer_T, vk::DeviceSize>;
using ImageAllocation = jms::memory::Allocation<VkImage_T, vk::DeviceSize>;


template <typename ResourceAllocation_t,
          typename RAII_t,
          typename MemoryAllocation_t,
          template <typename> typename Container>
class ResourceBase : jms::memory::Resource<ResourceAllocation_t> {
protected:
    using pointer_type = ResourceAllocation_t::pointer_type;
    using size_type = ResourceAllocation_t::size_type;

    struct Unit {
        MemoryAllocation_t mem;
        pointer_type res_ptr;
    };

    Container<Unit> units{};
    jms::memory::Resource<MemoryAllocation_t>* memory_resource;
    vk::raii::Device* device;
    vk::AllocationCallbacks* vk_allocation_callbacks;

    void Clear() {
        for (Unit& unit : units) { DestroyUnit(unit); }
        units.clear();
    }

    void DestroyUnit(Unit& unit) {
        RAII_t resource{*device, unit.res_ptr, *vk_allocation_callbacks};
        resource.clear();
        memory_resource->Deallocate(unit.mem);
    }

    [[nodiscard]] ResourceAllocation_t DoAllocate(size_type size, RAII_t&& resource) {
        auto allocation = memory_resource->Allocate(size);
        resource.bindMemory(allocation.ptr, allocation.offset);
        auto ptr = resource.release();
        units.push_back({.mem=allocation, .res_ptr=ptr});
        return {.ptr=ptr, .offset=0, .size=size};
    }

    void DoDeallocate(ResourceAllocation_t allocation) {
        auto it = std::ranges::find_if(units, [rhs=allocation.res_ptr](auto lhs) { return lhs == rhs; },
                                       &Unit::res_ptr);
        if (it == units.end()) { throw std::runtime_error{"Unable to find allocated resource for deallocation."}; }
        DestroyUnit(*it);
        units.erase(it);
    }

public:
    ResourceBase(jms::memory::Resource<MemoryAllocation_t>& memory_resource,
                 vk::raii::Device& device,
                 vk::AllocationCallbacks& vk_allocation_callbacks)
    : memory_resource{std::addressof(memory_resource)},
      device{std::addressof(device)},
      vk_allocation_callbacks{std::addressof(vk_allocation_callbacks)}
    {}
    ResourceBase(const ResourceBase&) = delete;
    ResourceBase& operator=(const ResourceBase&) = delete;
    ~ResourceBase() noexcept override { Clear(); }
};


template <typename MemoryAllocation_t, template <typename> typename Container>
class BufferResource : public ResourceBase<BufferAllocation, vk::raii::Buffer, MemoryAllocation_t, Container> {
    using size_type = BufferAllocation::size_type;

    vk::BufferCreateFlags create_flags;
    vk::BufferUsageFlags usage_flags;
    vk::SharingMode sharing_mode;
    const std::vector<uint32_t>& sharing_queue_family_indices;

public:
    BufferResource(jms::memory::Resource<MemoryAllocation_t>& memory_resource,
                   vk::raii::Device& device,
                   vk::AllocationCallbacks& vk_allocation_callbacks,
                   vk::BufferCreateFlags create_flags,
                   vk::BufferUsageFlags usage_flags,
                   const std::vector<uint32_t>& sharing_queue_family_indices)
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
        }, *this->vk_allocation_callbacks));
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
template <typename MemoryAllocation_t, template <typename> typename Container>
class ImageResource : public ResourceBase<ImageAllocation, vk::raii::Image, MemoryAllocation_t, Container> {
    using size_type = ImageAllocation::size_type;

    vk::BufferCreateFlags create_flags;
    vk::BufferUsageFlags usage_flags;
    vk::SharingMode sharing_mode;
    const std::vector<uint32_t>& sharing_queue_family_indices;

public:
    ImageResource(jms::memory::Resource<MemoryAllocation_t>& memory_resource,
                  vk::raii::Device& device,
                  vk::AllocationCallbacks& vk_allocation_callbacks,
                  vk::BufferCreateFlags create_flags,
                  vk::BufferUsageFlags usage_flags,
                  const std::vector<uint32_t>& sharing_queue_family_indices)
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
        }, *vk_allocation_callbacks));
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


} // namespace vulkan
} // namespace jms
