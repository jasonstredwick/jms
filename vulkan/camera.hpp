#pragma once


#include <cmath>

#include "jms/vulkan/glm.hpp"
#include <glm/gtc/matrix_transform.hpp>

#include "jms/vulkan/vulkan.hpp"


namespace jms {
namespace vulkan {


struct Camera {
    glm::mat4 projection{1.0f};
    glm::mat4 view{1.0f};

    /***
     * Camera position/orientation
     *
     *    Z
     *   /
     *  .--X
     * /|
     *  Y
    */
    Camera()
    : view(glm::lookAt(glm::vec3{0.0f, 0.0f, 0.0f}, glm::vec3{0.0f, 0.0f, 1.0f}, glm::vec3{0.0f, -1.0f, 0.0f}))
    {}

    Camera(float aspect_ratio)
    : projection(glm::infinitePerspective(glm::radians(60.0f), aspect_ratio, 0.1f)),
      view(glm::lookAt(glm::vec3{0.0f}, glm::vec3{0.0f, 0.0f, 1.0f}, glm::vec3{0.0f, -1.0f, 0.0f}))
    {}

    Camera(float fov_y_rads, float aspect_ratio, float near)
    : projection(glm::infinitePerspective(fov_y_rads, aspect_ratio, near)),
      view(glm::lookAt(glm::vec3{0.0f}, glm::vec3{0.0f, 0.0f, 1.0f}, glm::vec3{0.0f, -1.0f, 0.0f}))
    {}

    Camera(const glm::mat4& projection, const glm::mat4& view)
    : projection(projection), view(view)
    {}

    Camera(const Camera&) = default;
    Camera(Camera&&) = default;
    ~Camera() = default;
    Camera& operator=(const Camera&) = default;
    Camera& operator=(Camera&&) = default;

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


#if 0
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
#endif


}
}
