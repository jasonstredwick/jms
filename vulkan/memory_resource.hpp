#pragma once


#include <algorithm>
#include <memory> // std::addressof
#include <span>
#include <stdexcept>
#include <vector>

#include "jms/vulkan/no_mutex.hpp"
#include "jms/vulkan/vulkan.hpp"


template <typename T>
struct Allocation {
    using pointer_type = T*;
    pointer_type const ptr{nullptr};
    const size_t offset{0};
    const size_t num_units{0};
};
using MemoryAllocation = Allocation<VkDeviceMemory_T>;


struct MemoryResource {
    virtual ~MemoryResource() noexcept = default;
    [[nodiscard]] virtual MemoryAllocation Allocate(size_t num_units) = 0;
    virtual void Deallocate(MemoryAllocation allocation) = 0;
    virtual bool IsEqual(const MemoryResource& other) const noexcept { return std::addressof(other) == this; }
};


// TODO (1): other STL options instead of std::vector?
// TOOD (2): use pmr with possibly pool allocations to manage STL container internal heap allocations.
template <Mutex_c Mutex_t=NoMutex>
class AdhocPoolMemoryResource : public MemoryResource {
    struct Space { size_t offset, num_units; };

    struct Chunk {
        VkDeviceMemory_T* ptr;
        size_t num_units;
        std::vector<Space> free_space{};
    };

    MemoryResource* upstream;
    size_t chunk_num_units;
    std::vector<Chunk> chunks{};
    Mutex_t mutex{};

public:
    AdhocPoolMemoryResource(MemoryResource& upstream, size_t chunk_num_units)
    : upstream{std::addressof(upstream)}, chunk_num_units{chunk_num_units}
    {
        if (!chunk_num_units) { throw std::runtime_error{"Chunk num_units must be non-zero."}; }
    }
    AdhocPoolMemoryResource(const AdhocPoolMemoryResource&) = delete;
    AdhocPoolMemoryResource& operator=(const AdhocPoolMemoryResource&) = delete;
    ~AdhocPoolMemoryResource() noexcept override { Clear(); }

    [[nodiscard]] MemoryAllocation Allocate(size_t num_units) override {
        if (!num_units) { throw std::bad_alloc{}; }
        std::lock_guard<Mutex_t>{mutex};
        for (Chunk& chunk : chunks) {
            auto IsEnough = [needed=num_units](size_t available) -> bool { return available >= needed; };
            if (auto it = std::ranges::find_if(chunk.free_space, IsEnough, &Space::num_units); it != chunk.free_space.end()) {
                Space& space = *it;
                size_t offset = std::exchange(space.offset, space.offset + num_units);
                if (space.num_units == num_units) { chunk.free_space.erase(it); }
                else { space.num_units -= num_units; }
                return {.ptr=chunk.ptr, .offset=offset, .num_units=num_units};
            }
        }
        size_t total_units = ((num_units / chunk_num_units) + static_cast<size_t>(num_units % chunk_num_units > 0)) * chunk_num_units;
        MemoryAllocation result = upstream->Allocate(total_units);
        chunks.push_back({
            .ptr=result.ptr,
            .num_units=total_units,
            .free_space={{.offset=num_units, .num_units=(total_units - num_units)}}
        });
        return {.ptr=result.ptr, .offset=0, .num_units=num_units};
    }

    void Deallocate(MemoryAllocation allocation) override {
        std::lock_guard<Mutex_t>{mutex};

        auto chunk_it = std::ranges::find(chunks, allocation.ptr, &Chunk::ptr);
        if (chunk_it == chunks.end()) { throw std::runtime_error{"Deallocate cannot find chunk for suballocation."}; }
        Chunk& chunk = *chunk_it;

        size_t offset = allocation.offset;
        size_t num_units = allocation.num_units;
        auto IsFirstLess = [lhs=offset](size_t rhs) { return lhs < rhs; };
        auto right_it = std::ranges::find_if(chunk.free_space, IsFirstLess, &Space::offset);
        if (chunk.free_space.empty()) {
            chunk.free_space.push_back({.offset=offset, .num_units=num_units});
        } else if (right_it == chunk.free_space.begin()) {
            Space& right = *right_it;
            if (right.offset < offset + num_units) { throw std::runtime_error{"Found overlapping suballocation."}; }
            else if (right.offset == offset + num_units) { right.offset = offset; }
            else { chunk.free_space.insert(right_it, {.offset=offset, .num_units=num_units}); }
        } else if (right_it == chunk.free_space.end()) {
            Space& left = *std::prev(right_it);
            if (offset < left.offset + left.num_units) { throw std::runtime_error{"Found overlapping suballocation."}; }
            else if (offset == left.offset + left.num_units) { left.num_units += num_units; }
            else { chunk.free_space.push_back({.offset=offset, .num_units=num_units}); }
        } else {
            Space& left = *std::prev(right_it);
            Space& right = *right_it;
            if (offset < left.offset + left.num_units || right.offset < offset + num_units) {
                throw std::runtime_error{"Found overlapping suballocation."};
            }
            if (offset == left.offset + left.num_units) {
                left.num_units += num_units;
                if (right.offset == offset + num_units) {
                    left.num_units += right.num_units;
                    chunk.free_space.erase(right_it);
                }
            } else if (right.offset == offset + num_units) {
                right.offset = offset;
            } else {
                chunk.free_space.push_back({.offset=offset, .num_units=num_units});
            }
        }
    }

    void Clear() {
        std::lock_guard<Mutex_t>{mutex};
        std::ranges::for_each(chunks, [upstream](Chunk& chunk) {
            upstream->Deallocate({.ptr=chunk.ptr, .offset=0, .num_units=chunk.num_units});
        });
        chunks.clear();
    }
};


template <Mutex_c Mutex_t=NoMutex>
class BlockPoolMemoryResource : public MemoryResource {
    struct Block {
        VkDeviceMemory_T* ptr;
        size_t offset;
    };

    MemoryResource* upstream;
    size_t block_num_units;
    size_t chunk_num_units;
    std::vector<VkDeviceMemory_T*> chunks{};
    std::vector<Block> blocks{};
    decltype(blocks)::iterator free_block_it{blocks.end()};
    Mutex_t mutex{};

public:
    BlockPoolMemoryResource(MemoryResource& upstream, size_t block_num_units, size_t chunk_num_units)
    : upstream{std::addressof(upstream)}, block_num_units{block_num_units}, chunk_num_units{chunk_num_units}
    {
        if (!chunk_num_units || !block_num_units) { throw std::runtime_error{"Num chunk units and block units must be non-zero."}; }
        if (chunk_num_units % block_num_units > 0) { throw std::runtime_error{"Num Block units must be multiple of num chunk units."}; }
    }
    BlockPoolMemoryResource(const BlockPoolMemoryResource&) = delete;
    BlockPoolMemoryResource& operator=(const BlockPoolMemoryResource&) = delete;
    ~BlockPoolMemoryResource() noexcept override { Clear(); }

    [[nodiscard]] MemoryAllocation Allocate([[maybe_unused]] size_t num_units) override {
        std::lock_guard<Mutex_t>{mutex};
        if (free_block_it == blocks.end()) {
            MemoryAllocation result = upstream->Allocate(chunk_num_units);
            chunks.push_back(result.ptr);
            size_t num_blocks = chunk_num_units / block_num_units;
            blocks.reserve(blocks.capacity() + num_blocks);
            for (size_t block=0; block<num_blocks; ++block) {
                blocks.push_back({.ptr=result.ptr, .offset=(block * block_num_units)});
            }
            free_block_it = blocks.end() - num_blocks;
        }
        Block& block = *free_block_it;
        free_block_it = std::next(free_block_it);
        return {.ptr=block.ptr, .offset=block.offset, .num_units=block_num_units};
    }   

    void Deallocate(MemoryAllocation allocation) override {
        std::lock_guard<Mutex_t>{mutex};
        if (blocks.empty() || free_block_it == blocks.begin()) { throw std::runtime_error{"Deallocate cannot find allocated block to free."}; }
        std::span allocated_blocks(blocks.begin(), free_block_it);
        auto block_it = std::ranges::find_if(allocated_blocks, [allocation](const Block& block) {
            return block.ptr == allocation.ptr && block.offset == allocation.offset;
        });
        if (block_it == allocated_blocks.end()) { throw std::runtime_error{"Deallocate cannot find allocated block to free."}; }
        std::swap(*block_it, allocated_blocks.back());
        free_block_it = std::prev(free_block_it);
    }

    void Clear() {
        std::lock_guard<Mutex_t>{mutex};
        std::ranges::for_each(chunks, [upstream, chunk_num_units](auto ptr) {
            upstream->Deallocate({.ptr=ptr, .offset=0, .num_units=chunk_num_units});
        });
        chunks.clear();
        blocks.clear();
        free_block_it = blocks.end();
    }
};


// device.allocateMemory is thread safe : https://stackoverflow.com/questions/51528553/can-i-use-vkdevice-from-multiple-threads-concurrently
// device.allocateMemory implicitly includes a minimum alignment set by the driver applied in allocateMemory
// TODO: Use device props to determine what this minimum alignment value might be.
class DirectMemoryResource : public MemoryResource {
    static_assert(sizeof(vk::DeviceSize) <= sizeof(size_t),
                  "DirectMemoryResource requires vk::DeviceSize must be less than or equal to the size of size_t.");

    vk::raii::Device* device;
    vk::AllocationCallbacks* vk_allocation_callbacks;
    uint32_t memory_type_index;

public:
    DirectMemoryResource(vk::raii::Device& device, vk::AllocationCallbacks& vk_allocation_callbacks, uint32_t memory_type_index) noexcept
    : device{std::addressof(device)}, vk_allocation_callbacks{std::addressof(vk_allocation_callbacks)}, memory_type_index{memory_type_index}
    {}
    ~DirectMemoryResource() noexcept override = default;

    vk::raii::Device& GetDevice() const noexcept { return *device; }
    vk::AllocationCallbacks& GetAllocationCallbacks() const noexcept { return *vk_allocation_callbacks; }
    uint32_t GetMemoryType() const noexcept { return memory_type_index; }

    [[nodiscard]] MemoryAllocation Allocate(size_t num_units) override {
        if (!num_units) { throw std::bad_alloc{}; }
        vk::raii::DeviceMemory dev_mem = device->allocateMemory({
            .allocationSize=static_cast<vk::DeviceSize>(num_units),
            .memoryTypeIndex=memory_type_index
        }, *vk_allocation_callbacks);
        return {.ptr=static_cast<VkDeviceMemory>(dev_mem.release()), .offset=0, .num_units=num_units};
    }

    void Deallocate(MemoryAllocation allocation) override {
        vk::raii::DeviceMemory dev_mem{*device, allocation.ptr, *vk_allocation_callbacks}; // delete upon leaving scope
    }

    bool IsEqual(const MemoryResource& other) const noexcept override {
        const DirectMemoryResource* omr = dynamic_cast<const DirectMemoryResource*>(std::addressof(other));
        return omr != nullptr &&
               omr->device == device &&
               omr->vk_allocation_callbacks == vk_allocation_callbacks &&
               omr->memory_type_index == memory_type_index;
    }
};


template <Mutex_c Mutex_t=NoMutex>
class MonotonicMemoryResource : public MemoryResource {
    struct alignas(8) Options {
        size_t start_num_units{65536};
        double multiple{2.0};
        bool allocate_initial_chunk{false};
    };

    struct Chunk { VkDeviceMemory_T* ptr; size_t chunk_size; size_t offset; };

    MemoryResource* upstream;
    Options options{};
    size_t next_num_units{0};
    std::vector<Chunk> chunks{};
    decltype(chunks)::iterator chunk_it = chunks.end();
    Mutex_t mutex{};

public:
    MonotonicMemoryResource(MemoryResource& upstream) : upstream{std::addressof(upstream)} {}
    MonotonicMemoryResource(MemoryResource& upstream, Options options)
    : upstream{std::addressof(upstream)}, options{options}
    {
        if (!options.start_num_units) { throw std::runtime_error{"Monotonic memory resource must have non-zero starting units."}; }
        if (options.multiple < 0) { throw std::runtime_error{"Monotonic memory resource must have a positive multiple."}; }
        if (options.allocate_initial_chunk) { AllocateNextChunk(0); chunk_it = chunks.begin(); }
    }
    MonotonicMemoryResource(const MonotonicMemoryResource&) = delete;
    MonotonicMemoryResource& operator=(const MonotonicMemoryResource&) = delete;
    ~MonotonicMemoryResource() noexcept override { Clear(); }

    [[nodiscard]] MemoryAllocation Allocate(size_t num_units) override {
        if (!num_units) { throw std::bad_alloc{}; }
        std::lock_guard<Mutex_t>{mutex};
        while (chunk_it != chunks.end() && chunk_it->offset + num_units >= chunk_it->chunk_size) { chunk_it = std::next(chunk_it); }
        if (chunk_it == chunks.end()) { AllocateNextChunk(num_units); chunk_it = std::prev(chunks.end()); }
        size_t offset = std::exchange(chunk_it->offset, chunk_it->offset + num_units);
        return {.ptr=chunk_it->ptr, .offset=offset, .num_units=num_units};
    }

    void Deallocate([[maybe_unused]] MemoryAllocation allocation) override {}

    void Clear() {
        std::lock_guard<Mutex_t>{mutex};
        for (Chunk& chunk : chunks) {
            upstream->Deallocate({.ptr=chunk.ptr, .num_units=chunk.chunk_size, .offset=0});
        }
        chunks.clear();
        next_num_units = 0;
        chunk_it = chunks.end();
    }

private:
    void AllocateNextChunk(size_t num_units) {
        if (!next_num_units) { next_num_units = options.start_num_units; }
        else {
            double new_units = options.multiple * next_num_units;
            if (new_units < 1) { throw std::bad_alloc{}; }
            next_num_units = static_cast<size_t>(new_units);
        }
        size_t total_units = next_num_units;
        if (num_units > total_units) {
            total_units = ((num_units / total_units) + static_cast<size_t>(num_units % total_units)) * next_num_units;
        }
        MemoryAllocation allocation = upstream->Allocate(total_units);
        chunks.push_back({.ptr=allocation.ptr, .chunk_size=allocation.num_units});
    }
};
