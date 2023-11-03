#pragma once


#include <cmath>

#include "jms/external/glm.hpp"
#include <glm/gtc/matrix_transform.hpp>

#include "jms/vulkan/vulkan.hpp"

#include <glm/gtx/string_cast.hpp>
#include <iostream>


namespace jms {
namespace vulkan {


/***

TODO: Projection data structure that not only defines the explicit parameters but other bits (or derive them) to
denote things like [-1..1] vs [0..1], reversed z-order, and infinite z.

Camera should simply be a normal object, perhaps include a "lens" or projection attachment.

Camera should be fed into the graphics_pass and aspects define state parameters rather than they be explicitly set
separate and in addition to being set with the camera.

*/


struct Camera {
    // state
    glm::mat4 projection{1.0f};
    glm::vec3 position{0.0f};
    glm::vec3 rotation_angles{0.0f};

    Camera() noexcept = default;
    Camera(const glm::mat4& a) noexcept : projection{a} {}
    Camera(const glm::mat4& a, const glm::vec3& p, const glm::vec3& r) noexcept
    : projection{a}, position{p}, rotation_angles{r}
    {}
    Camera(const Camera&) = default;
    Camera(Camera&&) noexcept = default;
    ~Camera() = default;
    Camera& operator=(const Camera&) = default;
    Camera& operator=(Camera&&) noexcept = default;

    void SetPosition(const glm::vec3& a) { position = a; }
    void SetRotation(const glm::vec3& a) { rotation_angles = a; }

    void Update(const glm::vec3& delta_translation, const glm::vec3& delta_angles) {
        // Reference: https://thinkinginsideadifferentbox.wordpress.com/2020/09/22/rotation-matrices-and-looking-at-a-thing-the-easy-way/
        glm::mat4 rot{1.0f};
        rot = glm::rotate(rot, rotation_angles.z, glm::vec3(0.0f, 0.0f, 1.0f)); // yaw
        rot = glm::rotate(rot, rotation_angles.x, glm::vec3(1.0f, 0.0f, 0.0f)); // pitch
        rot = glm::rotate(rot, rotation_angles.y, glm::vec3(0.0f, 1.0f, 0.0f)); // roll
        position += glm::vec3(delta_translation.x * rot[0]) +
                    glm::vec3(delta_translation.y * rot[1]) +
                    glm::vec3(delta_translation.z * rot[2]);
        rotation_angles = glm::mod(rotation_angles + delta_angles,
                                   {glm::two_pi<float>(), glm::two_pi<float>(), glm::two_pi<float>()});
    }

    glm::mat4 View() const {
        glm::mat4 tra = glm::translate(glm::mat4{1.0f}, position);
        glm::mat4 rot{1.0f};
        rot = glm::rotate(rot, rotation_angles.z, glm::vec3(0.0f, 0.0f, 1.0f)); // yaw
        rot = glm::rotate(rot, rotation_angles.x, glm::vec3(1.0f, 0.0f, 0.0f)); // pitch
        rot = glm::rotate(rot, rotation_angles.y, glm::vec3(0.0f, 1.0f, 0.0f)); // roll
        glm::mat4 rot_origin = rot * tra;
        glm::mat4 rot_self = tra * rot;
        return projection * glm::inverse(rot_self);
    }
};


#if 0

    static constexpr vk::DescriptorSetLayoutBinding Binding(uint32_t binding) noexcept {
        return vk::DescriptorSetLayoutBinding{
            .binding=binding,
            .descriptorType=vk::DescriptorType::eUniformBuffer,
            .descriptorCount=1,
            .stageFlags=vk::ShaderStageFlagBits::eVertex,
            .pImmutableSamplers=nullptr
        };
    }

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
