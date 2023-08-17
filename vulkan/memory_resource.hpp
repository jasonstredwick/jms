#pragma once


#include <algorithm>
#include <concepts>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

#include "jms/vulkan/no_mutex.hpp"
#include "jms/vulkan/vulkan.hpp"


template <typename T>
struct Allocation {
    using pointer_type = T*;

    pointer_type ptr{nullptr};
    size_t offset{0};
    size_t size{0};
};
using MemoryAllocation = Allocation<VkDeviceMemory_T>;


#if 0
template <template <bool Owner> typename T, bool Owner>
concept AllocationResult_c = requires(T<Owner> t) {
    requires std::same_as<T<true>, vk::raii::DeviceMemory> || std::same_as<T<false>, vk::raii::DeviceMemory*>;
    { *t } -> std::same_as<vk::raii::DeviceMemory&>;
    { t.Bytes() } -> std::convertible_to<size_t>;
    { t.Offset() } -> std::convertible_to<size_t>;
    { t.FinalBytes() } -> std::convertible_to<size_t>;
    { t.FinalAlignment() } -> std::convertible_to<size_t>;
    requires std::is_constructible_v<T<Owner>, vk::raii::DeviceMemory&&, size_t, size_t, size_t, size_t>;
    requires !std::is_copy_constructible_v<T<Owner>> && !std::is_copy_assignable_v<T<Owner>>;
    requires std::is_move_constructible_v<T<Owner>> && std::is_move_assignable_v<T<Owner>>;
};
#endif


struct MemoryResource {
    virtual ~MemoryResource() noexcept = default;
    [[nodiscard]] virtual MemoryAllocation Allocate(size_t size) = 0;
    virtual void Deallocate(MemoryAllocation allocation) = 0;
    virtual bool IsEqual(const MemoryResource& other) const noexcept { return std::addressof(other) == this; }
};


template <typename T>
concept MemoryResource_c = requires (T t) {
    { t.Allocate() } -> std::same_as<MemoryAllocation>;
    { t.Deallocate() };
    { t.IsEqual() } -> std::convertible_to<bool>;
};


// TODO (1): other STL options instead of std::vector?
// TOOD (2): use pmr with possibly pool allocations to manage STL container internal heap allocations.
template <Mutex_c Mutex_t=NoMutex>
class AdhocPoolMemoryResource : public MemoryResource {
    struct Space { size_t offset, size; };

    struct Chunk {
        VkDeviceMemory_T* ptr;
        size_t size;
        std::vector<Space> free_space{};
    };

    MemoryResource* upstream;
    size_t chunk_size;
    std::vector<Chunk> chunks{};
    Mutex_t mutex{};

public:
    AdhocPoolMemoryResource(MemoryResource& upstream, size_t chunk_size)
    : upstream{std::addressof(upstream)}, chunk_size{chunk_size}
    {
        if (!chunk_size) { throw std::runtime_error{"Chunk size must be non-zero."}; }
    }
    AdhocPoolMemoryResource(const AdhocPoolMemoryResource&) = delete;
    AdhocPoolMemoryResource& operator=(const AdhocPoolMemoryResource&) = delete;
    ~AdhocPoolMemoryResource() noexcept override { Clear(); }

    [[nodiscard]] MemoryAllocation Allocate(size_t size) override {
        if (!size) { throw std::bad_alloc{}; }
        std::lock_guard<Mutex_t>{mutex};
        for (Chunk& chunk : chunks) {
            auto IsEnough = [needed=size](size_t available) -> bool { return available >= needed; };
            if (auto it = std::ranges::find_if(chunk.free_space, IsEnough, &Space::size); it != chunk.free_space.end()) {
                Space& space = *it;
                size_t offset = std::exchange(space.offset, space.offset + size);
                if (space.size == size) { chunk.free_space.erase(it); }
                else { space.size -= size; }
                return {.ptr=chunk.ptr, .offset=offset, .size=size};
            }
        }
        size_t total_units = ((size / chunk_size) + static_cast<size_t>(size % chunk_size > 0)) * chunk_size;
        MemoryAllocation result = upstream->Allocate(total_units);
        chunks.push_back({
            .ptr=result.ptr,
            .size=total_units,
            .free_space={{.offset=size, .size=(total_units - size)}}
        });
        return {.ptr=result.ptr, .offset=0, .size=size};
    }

    void Deallocate(MemoryAllocation allocation) override {
        std::lock_guard<Mutex_t>{mutex};

        auto chunk_it = std::ranges::find(chunks, allocation.ptr, &Chunk::ptr);
        if (chunk_it == chunks.end()) { throw std::runtime_error{"Deallocate cannot find chunk for suballocation."}; }
        Chunk& chunk = *chunk_it;

        size_t offset = allocation.offset;
        size_t size = allocation.size;
        auto IsFirstLess = [lhs=offset](size_t rhs) { return lhs < rhs; };
        auto right_it = std::ranges::find_if(chunk.free_space, IsFirstLess, &Space::offset);
        if (chunk.free_space.empty()) {
            chunk.free_space.push_back({.offset=offset, .size=size});
        } else if (right_it == chunk.free_space.begin()) {
            Space& right = *right_it;
            if (right.offset < offset + size) { throw std::runtime_error{"Found overlapping suballocation."}; }
            else if (right.offset == offset + size) { right.offset = offset; }
            else { chunk.free_space.insert(right_it, {.offset=offset, .size=size}); }
        } else if (right_it == chunk.free_space.end()) {
            Space& left = *std::prev(right_it);
            if (offset < left.offset + left.size) { throw std::runtime_error{"Found overlapping suballocation."}; }
            else if (offset == left.offset + left.size) { left.size += size; }
            else { chunk.free_space.push_back({.offset=offset, .size=size}); }
        } else {
            Space& left = *std::prev(right_it);
            Space& right = *right_it;
            if (offset < left.offset + left.size || right.offset < offset + size) {
                throw std::runtime_error{"Found overlapping suballocation."};
            }
            if (offset == left.offset + left.size) {
                left.size += size;
                if (right.offset == offset + size) {
                    left.size += right.size;
                    chunk.free_space.erase(right_it);
                }
            } else if (right.offset == offset + size) {
                right.offset = offset;
            } else {
                chunk.free_space.push_back({.offset=offset, .size=size});
            }
        }
    }

    void Clear() {
        std::lock_guard<Mutex_t> lock{mutex};
        std::ranges::for_each(chunks, [upstream=upstream](Chunk& chunk) {
            upstream->Deallocate({.ptr=chunk.ptr, .offset=0, .size=chunk.size});
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
    size_t block_size;
    size_t chunk_size;
    std::vector<VkDeviceMemory_T*> chunks{};
    std::vector<Block> blocks{};
    decltype(blocks)::iterator free_block_it{blocks.end()};
    Mutex_t mutex{};

public:
    BlockPoolMemoryResource(MemoryResource& upstream, size_t block_size, size_t chunk_size)
    : upstream{std::addressof(upstream)}, block_size{block_size}, chunk_size{chunk_size}
    {
        if (!chunk_size || !block_size) { throw std::runtime_error{"Num chunk units and block units must be non-zero."}; }
        if (chunk_size % block_size > 0) { throw std::runtime_error{"Num Block units must be multiple of num chunk units."}; }
    }
    BlockPoolMemoryResource(const BlockPoolMemoryResource&) = delete;
    BlockPoolMemoryResource& operator=(const BlockPoolMemoryResource&) = delete;
    ~BlockPoolMemoryResource() noexcept override { Clear(); }

    [[nodiscard]] MemoryAllocation Allocate([[maybe_unused]] size_t size) override {
        std::lock_guard<Mutex_t>{mutex};
        if (free_block_it == blocks.end()) {
            MemoryAllocation result = upstream->Allocate(chunk_size);
            chunks.push_back(result.ptr);
            size_t num_blocks = chunk_size / block_size;
            blocks.reserve(blocks.capacity() + num_blocks);
            for (size_t block=0; block<num_blocks; ++block) {
                blocks.push_back({.ptr=result.ptr, .offset=(block * block_size)});
            }
            free_block_it = blocks.end() - num_blocks;
        }
        Block& block = *free_block_it;
        free_block_it = std::next(free_block_it);
        return {.ptr=block.ptr, .offset=block.offset, .size=block_size};
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
        std::ranges::for_each(chunks, [upstream=upstream, chunk_size=chunk_size](auto ptr) {
            upstream->Deallocate({.ptr=ptr, .offset=0, .size=chunk_size});
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

    [[nodiscard]] MemoryAllocation Allocate(size_t size) override {
        if (!size) { throw std::bad_alloc{}; }
        vk::raii::DeviceMemory dev_mem = device->allocateMemory({
            .allocationSize=static_cast<vk::DeviceSize>(size),
            .memoryTypeIndex=memory_type_index
        }, *vk_allocation_callbacks);
        return {.ptr=static_cast<VkDeviceMemory>(dev_mem.release()), .offset=0, .size=size};
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
        size_t start_size{65536};
        double multiple{2.0};
        bool allocate_initial_chunk{false};
    };

    struct Chunk { VkDeviceMemory_T* ptr; size_t chunk_size; size_t offset; };

    MemoryResource* upstream;
    Options options{};
    size_t next_size{0};
    std::vector<Chunk> chunks{};
    decltype(chunks)::iterator chunk_it = chunks.end();
    Mutex_t mutex{};

public:
    MonotonicMemoryResource(MemoryResource& upstream) : upstream{std::addressof(upstream)} {}
    MonotonicMemoryResource(MemoryResource& upstream, Options options)
    : upstream{std::addressof(upstream)}, options{options}
    {
        if (!options.start_size) { throw std::runtime_error{"Monotonic memory resource must have non-zero starting units."}; }
        if (options.multiple < 0) { throw std::runtime_error{"Monotonic memory resource must have a positive multiple."}; }
        if (options.allocate_initial_chunk) { AllocateNextChunk(0); chunk_it = chunks.begin(); }
    }
    MonotonicMemoryResource(const MonotonicMemoryResource&) = delete;
    MonotonicMemoryResource& operator=(const MonotonicMemoryResource&) = delete;
    ~MonotonicMemoryResource() noexcept override { Clear(); }

    [[nodiscard]] MemoryAllocation Allocate(size_t size) override {
        if (!size) { throw std::bad_alloc{}; }
        std::lock_guard<Mutex_t>{mutex};
        while (chunk_it != chunks.end() && chunk_it->offset + size >= chunk_it->chunk_size) { chunk_it = std::next(chunk_it); }
        if (chunk_it == chunks.end()) { AllocateNextChunk(size); chunk_it = std::prev(chunks.end()); }
        size_t offset = std::exchange(chunk_it->offset, chunk_it->offset + size);
        return {.ptr=chunk_it->ptr, .offset=offset, .size=size};
    }

    void Deallocate([[maybe_unused]] MemoryAllocation allocation) override {}

    void Clear() {
        std::lock_guard<Mutex_t>{mutex};
        for (Chunk& chunk : chunks) {
            upstream->Deallocate({.ptr=chunk.ptr, .size=chunk.chunk_size, .offset=0});
        }
        chunks.clear();
        next_size = 0;
        chunk_it = chunks.end();
    }

private:
    void AllocateNextChunk(size_t size) {
        if (!next_size) { next_size = options.start_size; }
        else {
            double new_units = options.multiple * next_size;
            if (new_units < 1) { throw std::bad_alloc{}; }
            next_size = static_cast<size_t>(new_units);
        }
        size_t total_units = next_size;
        if (size > total_units) {
            total_units = ((size / total_units) + static_cast<size_t>(size % total_units)) * next_size;
        }
        MemoryAllocation allocation = upstream->Allocate(total_units);
        chunks.push_back({.ptr=allocation.ptr, .chunk_size=allocation.size});
    }
};
