#pragma once


#include <algorithm>
#include <iterator>
#include <memory>
#include <mutex>
#include <new>
#include <span>
#include <stdexcept>
#include <utility>

#include "allocation.hpp"
#include "resources.hpp"


namespace jms {
namespace memory {


/***
 * Thoughs
 * 1. Perhaps add traits such as if it should clean up on dealloc or allow reorganization.
 */

// TODO (1): other STL options instead of std::vector?
// TOOD (2): use pmr with possibly pool allocations to manage STL container internal heap allocations.
// ChunkContainer: is_range, begin, end, clear, push_back
// SpaceContainer: is_range, begin, end, erase, empty, push_back, insert
template <typename Allocation_t,
          template <typename> typename ChunkContainer,
          template <typename> typename SpaceContainer,
          typename Mutex_t/*=jms::NoMutex*/>
class AdhocPool : public Resource<Allocation_t> {
    using pointer_type = Resource<Allocation_t>::allocation_type::pointer_type;
    using size_type = Resource<Allocation_t>::allocation_type::size_type;

    struct Space { size_type offset, size; };

    struct Chunk {
        pointer_type ptr;
        size_type size;
        SpaceContainer<Space> free_space{};
    };

    Resource<Allocation_t>* upstream{nullptr};
    size_type chunk_size{0};
    ChunkContainer<Chunk> chunks{};
    Mutex_t mutex{};

public:
    AdhocPool(Resource<Allocation_t>& upstream, size_type chunk_size)
    : upstream{std::addressof(upstream)}, chunk_size{chunk_size}
    {
        if (chunk_size < 1) { throw std::runtime_error{"Chunk size must be a positive value."}; }
    }
    AdhocPool(const AdhocPool&) = delete;
    AdhocPool(AdhocPool&& other) noexcept { *this = other; }
    ~AdhocPool() noexcept override { Clear(); }
    AdhocPool& operator=(const AdhocPool&) = delete;
    AdhocPool& operator=(AdhocPool&&) noexcept {
        std::scoped_lock lock{mutex, other.mutex};
        upstream = std::exchange(other.upstream, nullptr);
        chunk_size = other.chunk_size;
        chunks = std::move(other.chunks);
        return *this;
    }

    [[nodiscard]] Allocation_t Allocate(size_type size,
                                        size_type data_alignment,
                                        size_type pointer_alignment) override {
        if (size < 1) { throw std::bad_alloc{}; }
        auto IsEnough = [needed=size](auto available) -> bool { return available >= needed; };
        std::lock_guard<Mutex_t>{mutex};
        for (Chunk& chunk : chunks) {
            auto it = std::ranges::find_if(chunk.free_space, IsEnough, &Space::size);
            if (it != chunk.free_space.end()) {
                Space& space = *it;
                auto offset = std::exchange(space.offset, space.offset + size);
                if (space.size == size) { chunk.free_space.erase(it); }
                else { space.size -= size; }
                return {.ptr=chunk.ptr, .offset=offset, .size=size};
            }
        }
        auto total_size = ((size / chunk_size) + static_cast<size_type>(size % chunk_size > 0)) * chunk_size;
        auto result = upstream->Allocate(total_size);
        chunks.push_back({
            .ptr=result.ptr,
            .size=result.size,
            .free_space={{.offset=size, .size=(result.size - size)}}
        });
        return {.ptr=result.ptr, .offset=0, .size=size};
    }

    void Deallocate(Allocation_t allocation) override {
        std::lock_guard<Mutex_t>{mutex};

        auto chunk_it = std::ranges::find(chunks, allocation.ptr, &Chunk::ptr);
        if (chunk_it == chunks.end()) { throw std::runtime_error{"Deallocate cannot find chunk for suballocation."}; }
        Chunk& chunk = *chunk_it;

        auto offset = allocation.offset;
        auto size = allocation.size;
        auto IsFirstLess = [lhs=offset](auto rhs) { return lhs < rhs; };
        auto right_it = std::ranges::find_if(chunk.free_space, IsFirstLess, &Space::offset);
        if (chunk.free_space.empty()) {
            chunk.free_space.push_back({.offset=offset, .size=size});
        } else if (right_it == chunk.free_space.begin()) {
            Space& right = *right_it;
            if (right.offset < offset + size) { throw std::runtime_error{"Found overlapping suballocation."}; }
            else if (right.offset == offset + size) { right.offset = offset; right.size += size; }
            else { chunk.free_space.insert(right_it, {.offset=offset, .size=size}); }
        } else if (right_it == chunk.free_space.end()) {
            Space& left = *std::ranges::prev(right_it);
            if (offset < left.offset + left.size) { throw std::runtime_error{"Found overlapping suballocation."}; }
            else if (offset == left.offset + left.size) { left.size += size; }
            else { chunk.free_space.push_back({.offset=offset, .size=size}); }
        } else {
            Space& left = *std::ranges::prev(right_it);
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
                right.size += size;
            } else {
                chunk.free_space.push_back({.offset=offset, .size=size});
            }
        }
    }

    void Clear() {
        std::lock_guard<Mutex_t> lock{mutex};
        std::ranges::for_each(chunks, [dealloc_func=dealloc_func](Chunk& chunk) {
            upstream->Deallocate({.ptr=chunk.ptr, .offset=0, .size=chunk.size});
        });
        chunks.clear();
    }
};


// ChunkContainer: is_range, begin, end, clear, push_back
// BlockContainer: is_range, begin, end, iterator, erase, empty, push_back, insert, capacity, reserve, size
template <typename Allocation_t,
          template <typename> typename ChunkContainer,
          template <typename> typename BlockContainer,
          typename Mutex_t/*=jms::NoMutex*/>
class BlockPool : public Resource<Allocation_t> {
    using pointer_type = Resource<Allocation_t>::allocation_type::pointer_type;
    using size_type = Resource<Allocation_t>::allocation_type::size_type;

    struct Block { pointer_type ptr; size_type offset; };

    Resource<Allocation_t>* upstream;
    size_type block_size;
    size_type chunk_size;
    ChunkContainer<pointer_type> chunks{};
    BlockContainer<Block> blocks{};
    typename decltype(blocks)::iterator free_block_it{blocks.end()};
    Mutex_t mutex{};

public:
    BlockPool(Resource<Allocation_t>& upstream, size_type block_size, size_type chunk_size)
    : upstream{std::addressof(upstream)}, block_size{block_size}, chunk_size{chunk_size}
    {
        if (chunk_size < 1) { throw std::runtime_error{"Chunk size must be a positive value."}; }
        if (block_size < 1) { throw std::runtime_error{"Block size must be a positive value."}; }
        if (chunk_size % block_size > 0) { throw std::runtime_error{"Chunk size must be multiple of block size."}; }
    }
    BlockPool(const BlockPool&) = delete;
    BlockPool(BlockPool&& other) noexcept { *this = other; }
    ~BlockPool() noexcept override { Clear(); }
    BlockPool& operator=(const BlockPool&) = delete;
    BlockPool& operator=(BlockPool&&) noexcept {
        std::scoped_lock lock{mutex, other.mutex};
        upstream = std::exchange(other.upstream, nullptr);
        block_size = other.block_size;
        chunk_size = other.chunk_size;
        chunks = std::move(other.chunks);
        blocks = std::move(other.blocks);
        free_block_it = std::move(free_block_it);
        return *this;
    }

    [[nodiscard]] Allocation_t Allocate([[maybe_unused]] size_type size,
                                        size_type data_alignment,
                                        size_type pointer_alignment) override {
        std::lock_guard<Mutex_t>{mutex};
        if (free_block_it == blocks.end()) {
            Allocation_t result = upstream->Allocate(chunk_size);
            chunks.push_back(result.ptr);
            auto num_blocks = chunk_size / block_size;
            if (blocks.size() + num_blocks > blocks.capacity()) { blocks.reserve(blocks.capacity() + num_blocks); }
            for (size_t block=0; block<num_blocks; ++block) {
                blocks.push_back({.ptr=result.ptr, .offset=(block * block_size)});
            }
            free_block_it = blocks.end() - num_blocks;
        }
        Block& block = *free_block_it;
        free_block_it = std::ranges::next(free_block_it);
        return {.ptr=block.ptr, .offset=block.offset, .size=block_size};
    }   

    void Deallocate(Allocation_t allocation) override {
        std::lock_guard<Mutex_t>{mutex};
        if (blocks.empty() || free_block_it == blocks.begin()) {
            throw std::runtime_error{"Deallocate cannot find allocated block to free."};
        }
        std::span allocated_blocks(blocks.begin(), free_block_it);
        auto block_it = std::ranges::find_if(allocated_blocks, [&allocation](const Block& block) {
            return block.ptr == allocation.ptr && block.offset == allocation.offset;
        });
        if (block_it == allocated_blocks.end()) {
            throw std::runtime_error{"Deallocate cannot find allocated block to free."};
        }
        std::swap(*block_it, allocated_blocks.back());
        free_block_it = std::ranges::prev(free_block_it);
    }

    void Clear() {
        std::lock_guard<Mutex_t>{mutex};
        std::ranges::for_each(chunks, [dealloc_func=dealloc_func, chunk_size=chunk_size](auto ptr) {
            upstream->Deallocate({.ptr=ptr, .offset=0, .size=chunk_size});
        });
        chunks.clear();
        blocks.clear();
        free_block_it = blocks.end();
    }
};


// ChunkContainer: is_range, begin, end, clear, push_back
// BlockContainer: is_range, begin, end, iterator, erase, empty, push_back, insert, capacity, reserve, size
template <typename Allocation_t,
          template <typename> typename ChunkContainer,
          typename Mutex_t/*=jms::NoMutex*/>
class Monotonic : public Resource<Allocation_t> {
public:
    using pointer_type = Resource<Allocation_t>::allocation_type::pointer_type;
    using size_type = Resource<Allocation_t>::allocation_type::size_type;

    struct alignas(8) Options {
        size_type start_size{65536};
        double multiple{2.0};
        bool allocate_initial_chunk{false};
    };

private:
    struct Chunk { pointer_type ptr; size_type chunk_size, offset; };

    Resource<Allocation_t>* upstream;
    Options options{};
    size_type next_size{0};
    ChunkContainer<Chunk> chunks{};
    ChunkContainer<Chunk>::iterator chunk_it = chunks.end();
    Mutex_t mutex{};

public:
    Monotonic(Resource<Allocation_t>& upstream) : upstream{std::addressof(upstream)} {}
    Monotonic(Resource<Allocation_t>& upstream, Options options) : upstream{std::addressof(upstream)}, options{options}
    {
        if (options.start_size < 1) { throw std::runtime_error{"Monotonic resource must have a positive size."}; }
        if (options.multiple < 0) { throw std::runtime_error{"Monotonic resource must have a non-negative multiple."}; }
        if (options.allocate_initial_chunk) { AllocateNextChunk(0); chunk_it = chunks.begin(); }
    }
    Monotonic(const Monotonic&) = delete;
    Monotonic(Monotonic&& other) noexcept { *this = other; }
    ~Monotonic() noexcept override { Clear(); }
    Monotonic& operator=(const Monotonic&) = delete;
    Monotonic& operator=(Monotonic&& other) noexcept {
        std::scoped_lock lock{mutex, other.mutex};
        upstream = std::exchange(other.upstream, nullptr);
        options = other.options;
        next_size = other.next_size;
        chunks = std::move(other.chunks);
        chunk_it = std::move(chunk_it);
        return *this;
    }

    [[nodiscard]] Allocation_t Allocate(size_type size,
                                        size_type data_alignment,
                                        size_type pointer_alignment) override {
        if (size < 1) { throw std::bad_alloc{}; }
        std::lock_guard<Mutex_t>{mutex};
        while (chunk_it != chunks.end() && chunk_it->offset + size >= chunk_it->chunk_size) {
            chunk_it = std::ranges::next(chunk_it);
        }
        if (chunk_it == chunks.end()) { AllocateNextChunk(size); chunk_it = std::ranges::prev(chunks.end()); }
        size_type offset = std::exchange(chunk_it->offset, chunk_it->offset + size);
        return {.ptr=chunk_it->ptr, .offset=offset, .size=size};
    }

    void Deallocate([[maybe_unused]] Allocation_t allocation) override {}

    void Clear() {
        std::lock_guard<Mutex_t>{mutex};
        for (Chunk& chunk : chunks) { upstream->Deallocate({.ptr=chunk.ptr, .size=chunk.chunk_size, .offset=0}); }
        chunks.clear();
        next_size = 0;
        chunk_it = chunks.end();
    }

private:
    void AllocateNextChunk(size_type size) {
        if (!next_size) { next_size = options.start_size; }
        else {
            double new_size = options.multiple * next_size;
            if (new_size < 1) { throw std::bad_alloc{}; }
            next_size = static_cast<size_type>(new_size);
        }
        auto total_size = next_size;
        if (size > total_size) {
            total_size = ((size / total_size) + static_cast<size_type>(size % total_size)) * next_size;
        }
        auto allocation = upstream->Allocate(total_size);
        chunks.push_back({.ptr=allocation.ptr, .chunk_size=allocation.size, .offset=0});
    }
};


} // namespace memory
} // namespace jms
