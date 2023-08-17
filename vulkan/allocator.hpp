#pragma once


#include <algorithm>
#include <functional>
#include <stdexcept>
#include <vector>

#include "jms/vulkan/memory_resource.hpp"
#include "jms/vulkan/vulkan.hpp"


using BufferAllocation = Allocation<VkBuffer_T>;
using ImageAllocation = Allocation<VkImage_T>;


struct Allocator {
    virtual ~Allocator() noexcept {}
    virtual BufferAllocation AllocateBuffer(size_t size,
                                            vk::BufferCreateFlags create_flags,
                                            vk::BufferUsageFlags usage_flags,
                                            const std::vector<uint32_t>& sharing_queue_family_indices) = 0;
    virtual void DeallocateBuffer(BufferAllocation allocation) = 0;
    virtual ImageAllocation AllocateImage() = 0;
    virtual void DeallocateImage(ImageAllocation allocation) = 0;
    virtual bool IsEqual(const Allocator& other) const noexcept { return std::addressof(other) == this; }
};


template <typename T>
concept AllocatorResource_c = requires (T t) {
    { t.AllocateBuffer() } -> std::same_as<BufferAllocation>;
    { t.DeallocateBuffer() };
    { t.AllocateImage() } -> std::same_as<ImageAllocation>;
    { t.DeallocateImage() };
    { t.IsEqual() } -> std::convertible_to<bool>;
};


class DirectAllocator : public Allocator {
    struct BufferData { MemoryAllocation allocation; VkBuffer_T* ptr; };
    struct ImageData { MemoryAllocation allocation; VkImage_T* ptr; };

    MemoryResource* memory_resource;
    vk::raii::Device* device;
    vk::AllocationCallbacks* vk_allocation_callbacks;
    size_t min_alignment;
    std::vector<BufferData> buffers{};
    std::vector<ImageData> images{};

public:
    DirectAllocator(MemoryResource& memory_resource,
                    vk::raii::Device& device,
                    vk::AllocationCallbacks& vk_allocation_callbacks,
                    size_t min_alignment)
    : memory_resource{std::addressof(memory_resource)},
      device{std::addressof(device)},
      vk_allocation_callbacks{std::addressof(vk_allocation_callbacks)},
      min_alignment{min_alignment}
    { if(!std::has_single_bit(min_alignment)) { throw std::runtime_error{"Allocator requires non-zero, power of two value."}; }}

    DirectAllocator(const DirectAllocator&) = delete;
    DirectAllocator& operator=(const DirectAllocator&) = delete;

    ~DirectAllocator() noexcept override { Clear(); }

    BufferAllocation AllocateBuffer(size_t size,
                                    vk::BufferCreateFlags create_flags,
                                    vk::BufferUsageFlags usage_flags,
                                    const std::vector<uint32_t>& sharing_queue_family_indices) override {
        size_t total_bytes = ((size / min_alignment) + static_cast<size_t>(size % min_alignment > 0)) * min_alignment;
        MemoryAllocation allocation = memory_resource->Allocate(total_bytes);
        vk::raii::Buffer buffer = device->createBuffer({
            .flags=create_flags,
            .size=static_cast<vk::DeviceSize>(size),
            .usage=usage_flags,
            .sharingMode=((sharing_queue_family_indices.size()) ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive),
            .queueFamilyIndexCount=static_cast<uint32_t>(sharing_queue_family_indices.size()),
            .pQueueFamilyIndices=sharing_queue_family_indices.data()
        }, *vk_allocation_callbacks);
        buffer.bindMemory(allocation.ptr, allocation.offset);
        VkBuffer_T* ptr = buffer.release();
        buffers.push_back({.allocation=allocation, .ptr=ptr});
        return {.ptr=ptr, .offset=0, .size=size};
    }

    void DeallocateBuffer(BufferAllocation allocation) override {
        auto it = std::ranges::find_if(buffers, [rhs=allocation.ptr](VkBuffer_T* lhs) { return lhs == rhs; }, &BufferData::ptr);
        if (it == buffers.end()) { throw std::runtime_error{"Unable to find allocated buffer for deallocation."}; }
        DestroyBuffer(*it);
        buffers.erase(it);
    }

    ImageAllocation AllocateImage() override { return {}; }

    void DeallocateImage(ImageAllocation allocation) override {
        auto it = std::ranges::find_if(images, [rhs=allocation.ptr](VkImage_T* lhs) { return lhs == rhs; }, &ImageData::ptr);
        if (it == images.end()) { throw std::runtime_error{"Unable to find allocated image for deallocation."}; }
        DestroyImage(*it);
        images.erase(it);
    }

    void Clear() {
        for (BufferData& data : buffers) { DestroyBuffer(data); }
        buffers.clear();
        for (ImageData& data : images) { DestroyImage(data); }
        images.clear();
    }

private:
    void DestroyBuffer(BufferData& data) {
        vk::raii::Buffer buffer{*device, data.ptr, *vk_allocation_callbacks};
        buffer.clear();
        memory_resource->Deallocate(data.allocation);
    }

    void DestroyImage(ImageData& data) {
        vk::raii::Image image{*device, data.ptr, *vk_allocation_callbacks};
        image.clear();
        memory_resource->Deallocate(data.allocation);
    }
};


#if 0
class AdhocAllocator : public Allocator {
    template <typename T>
    struct Data {
        Allocation allocation;
        T* buffer;
    };

    MemoryResource* memory_resource;
    vk::raii::Device* device;
    vk::AllocationCallbacks* vk_allocation_callbacks;
    size_t min_align;
    std::vector<Data<VkBuffer_T>> buffer_data{};
    std::vector<Data<VkImage_T>> image_data{};

public:
    DirectAllocator(MemoryResource& memory_resource,
                    vk::raii::Device& device,
                    vk::AllocationCallbacks& vk_allocation_callbacks)
    : memory_resource{std::addressof(memory_resource)},
      device{std::addressof(device)},
      vk_allocation_callbacks{std::addressof(vk_allocation_callbacks)}
    {}

    Allocation_t<VkBuffer_T> AllocateBuffer(size_t num_units,
                                            vk::BufferUsageFlags usage_flags=vk::BufferUsageFlagBits::eUniformBuffer,
                                            vk::BufferCreateFlags create_flags={},
                                            const std::vector<uint32_t>& sharing_queue_family_indices={}) override {
        size_t total_bytes = ((num_units / min_align) + static_cast<size_t>(num_units % min_align > 0)) * min_align;
        Allocation allocation = memory_resource->Allocate(total_bytes);
        vk::SharingMode sharing_mode = (sharing_queue_family_indices.size()) ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive;
        vk::raii::Buffer buffer = device->createBuffer({
            .flags=create_flags,
            .size=static_cast<vk::DeviceSize>(num_units),
            .usage=usage_flags,
            .sharingMode=sharing_mode,
            .queueFamilyIndexCount=static_cast<uint32_t>(sharing_queue_family_indices.size()),
            .pQueueFamilyIndices=sharing_queue_family_indices.data()
        }, *vk_allocation_callbacks);
        buffer.bindMemory(allocation.ptr, allocation.offset);
        VkBuffer_T* raw_buffer = buffer.release();
        buffer_data.push_back({.allocation=allocation, .buffer=raw_buffer});
        return {.buffer=raw_buffer, .offset=0, .num_units=num_units};
    }

    void DeallocateBuffer(Allocation_t<VkBuffer_T> allocation) override {}

    Allocation_t<VkImage_T> AllocateImage() override {}
    void DeallocateImage(Allocation_t<VkImage_T> allocation) override {}
};


    vk::DescriptorSetLayoutBinding Bind(uint32_t binding, vk::ShaderStageFlags stage_flags) const noexcept {
        vk::DescriptorBufferInfo camera_buffer_info{
            .buffer=*camera_buffer.buffer,
            .offset=0,
            .range=camera_buffer.buffer_size
        };
        return vk::DescriptorSetLayoutBinding{
            .binding=binding,
            .descriptorType=vk::DescriptorType::eUniformBuffer,
            .descriptorCount=1,
            .stageFlags=stage_flags,
            .pImmutableSamplers=nullptr
        };
    }
#endif
