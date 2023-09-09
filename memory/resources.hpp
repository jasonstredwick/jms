#pragma once


#include <memory>
#include <utility>


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


template <typename Allocation_t>
class UniqueResource {
    Resource<Allocation_t>* resource{nullptr};
    Allocation_t allocation{};

public:
    UniqueResource() noexcept = default;
    UniqueResource(Resource<Allocation_t>& r, Allocation_t a) noexcept : resource{std::addressof(r)}, allocation{a} {}
    UniqueResource(Resource<Allocation_t>& r, auto&&... args)
    : resource{std::addressof(r)}, allocation{r.Allocate(std::forward<decltype(args)>(args)...)}
    {}
    UniqueResource(const UniqueResource&) = delete;
    ~UniqueResource() noexcept { if (resource) { resource->Deallocate(allocation); } }
    UniqueResource& operator=(const UniqueResource&) = delete;

    explicit operator bool() const noexcept { return resource != nullptr; }
    void Clear() noexcept { resource = nullptr; allocation = {}; }
    const Allocation_t& Get() const noexcept { return allocation; }
    const Allocation_t& operator*() const noexcept { return allocation; }
    const Allocation_t* operator->() const noexcept { return std::addressof(allocation); }
};


} // namespace memory
} // namespace jms
