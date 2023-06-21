#pragma once


#include <cassert>
#include <type_traits>

#include "jms/vulkan/vulkan.hpp"


namespace jms {
namespace vulkan {


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
            throw std::runtime_error(fmt::format("Unknown PhysicalDeviceMemory extension: {}\n",
                                                    vk::to_string(jms::vulkan::ExtractSType(props2.pNext))));
        }
    }
*/

}
}