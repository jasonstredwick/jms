#pragma once


#include <memory>


namespace jms {
namespace memory {


template <typename Allocation_t>
struct Resource {
    using allocation_type = Allocation_t;
    virtual ~Resource() noexcept = default;
    [[nodiscard]] virtual allocation_type Allocate(allocation_type::size_type size) = 0;
    virtual void Deallocate(allocation_type allocation) = 0;
    virtual bool IsEqual(const Resource& other) const noexcept { return std::addressof(other) == this; }
};


} // namespace memory
} // namespace jms
