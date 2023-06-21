#pragma once


#include <concepts>
#include <cstdint>
#include <type_traits>
#include <vector>

#include "jms/vulkan/vulkan.hpp"


namespace jms {
namespace vulkan {


template <typename T>
concept Vertex_c = requires(T, uint32_t binding) {
  { T::GetBindingDesc(binding) } -> std::convertible_to<std::vector<vk::VertexInputBindingDescription>>;
  { T::GetAttributeDesc(binding) } -> std::convertible_to<std::vector<vk::VertexInputAttributeDescription>>;
};


struct VertexDescription {
    std::vector<vk::VertexInputBindingDescription> binding_description{};
    std::vector<vk::VertexInputAttributeDescription> attribute_description{};

    template <Vertex_c T>
    static constexpr VertexDescription Create(uint32_t binding) {
        return {.binding_description=T::GetBindingDesc(binding), .attribute_description=T::GetAttributeDesc(binding)};
    }

    constexpr vk::PipelineVertexInputStateCreateInfo GetInfo() const {
        return vk::PipelineVertexInputStateCreateInfo{
            .vertexBindingDescriptionCount=static_cast<uint32_t>(binding_description.size()),
            .pVertexBindingDescriptions=binding_description.data(),
            .vertexAttributeDescriptionCount=static_cast<uint32_t>(attribute_description.size()),
            .pVertexAttributeDescriptions=attribute_description.data()
        };
    }
};


}
}
