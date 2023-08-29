#pragma once


namespace jms {
namespace memory {


template <typename T, typename SizeType=size_t>
struct Allocation {
    using pointer_type = T*;
    using size_type = SizeType;

    pointer_type ptr{nullptr};
    size_type offset{0};
    size_type size{0};
};


} // namespace memory
} // namespace jms
