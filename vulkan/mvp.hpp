#pragma once


#include <glm/glm.hpp>

#include "include_config.hpp"
#include "vulkan/vulkan_raii.hpp"


namespace jms {
namespace vulkan {


struct UniformBufferObject {
    glm::mat4 model{};
    glm::mat4 view{};
    glm::mat4 proj{};

    static constexpr vk::DescriptorSetLayoutBinding Binding(uint32_t binding) noexcept {
        return vk::DescriptorSetLayoutBinding{
            .binding=binding,
            .descriptorType=vk::DescriptorType::eUniformBuffer,
            .descriptorCount=1,
            .stageFlags=vk::ShaderStageFlagBits::eVertex,
            .pImmutableSamplers=nullptr
        };
    }
};


}
}
