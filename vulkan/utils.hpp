#pragma once


#include <algorithm>
#include <cassert>
#include <ranges>
#include <set>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

#include "jms/vulkan/vulkan.hpp"


namespace jms {
namespace vulkan {


template <typename Container_t>
constexpr const auto* VectorAsPtr(const Container_t& v) { return v.size() > 0 ? v.data() : nullptr; }

template <typename Container_t>
constexpr auto* VectorAsPtr(Container_t& v) { return v.size() > 0 ? v.data() : nullptr; }


}
}
