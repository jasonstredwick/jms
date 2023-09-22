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


template <typename... Ts>
auto ChainPNext(std::vector<std::variant<Ts...>> v) {
    using Tuple_t = std::tuple<std::variant<Ts...>&, std::variant<Ts...>&>;

    // Validate
    std::set<vk::StructureType> kinds{};
    std::ranges::transform(v, std::inserter(kinds, kinds.begin()), [](auto& i) {
        return std::visit([](auto&& x) { return x.sType; }, i); });
    if (kinds.size() != v.size()) {
        throw std::runtime_error{"DeviceCreateInfo does not allow duplicate pNext structures."};
    }

    std::ranges::for_each(
        std::views::zip(v, v | std::views::drop(1)), [](Tuple_t&& tup) {
            void* ptr = &(std::get<1>(tup));
            std::visit([&ptr](auto&& x) { x.pNext = ptr; }, std::get<0>(tup));
        });
    return v;
}


template <typename T>
requires requires(T) { T::structureType; } && std::is_scoped_enum_v<decltype(T::structureType)>
T* Convert(void* next_ptr) {
    if (!next_ptr) { return nullptr; }
    vk::BaseOutStructure* base_ptr = reinterpret_cast<vk::BaseOutStructure*>(next_ptr);
    if (base_ptr->sType == T::structureType) { return reinterpret_cast<T*>(next_ptr); }
    return nullptr;
}


vk::StructureType ExtractSType(void* next_ptr) {
    assert(next_ptr);
    return reinterpret_cast<vk::BaseOutStructure*>(next_ptr)->sType;
}


/*

    vk::PhysicalDeviceMemoryProperties2 props2 = physical_device.getMemoryProperties2();
    vk::PhysicalDeviceMemoryProperties physical_device_mem_props = props2.memoryProperties;
    vk::PhysicalDeviceMemoryBudgetPropertiesEXT* budget_props = nullptr;
    if (props2.pNext) {
        if (auto* ptr = jms::vulkan::Convert<vk::PhysicalDeviceMemoryBudgetPropertiesEXT>(props2.pNext); ptr) {
            budget_props = ptr;
        } else {
            throw std::runtime_error(std::format("Unknown PhysicalDeviceMemory extension: {}\n",
                                                    vk::to_string(jms::vulkan::ExtractSType(props2.pNext))));
        }
    }
*/


}
}
