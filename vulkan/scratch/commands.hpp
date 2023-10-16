#pragma once


#include <vector>

#include "jms/vulkan/vulkan.hpp"


namespace jms {
namespace vulkan {


class CommandsSingleTime {
    std::vector<vk::raii::CommandBuffer> buffers;
    const vk::raii::Queue& queue;
    bool should_wait;

public:
    CommandsSingleTime(const vk::raii::Device& device,
                       const vk::raii::CommandPool& command_pool,
                       const vk::raii::Queue& queue,
                       bool should_wait=false)
    : buffers(Alloc(device, command_pool)), queue(queue), should_wait(should_wait) {
        buffers[0].begin({.flags=vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    }
    CommandsSingleTime(const CommandsSingleTime&) = delete;
    CommandsSingleTime(CommandsSingleTime&&) = default;
    ~CommandsSingleTime() {
        buffers[0].end();
        queue.submit({
            vk::SubmitInfo{
                .commandBufferCount=1,
                .pCommandBuffers=&(*buffers[0])
            }
        });
        if (should_wait) { queue.waitIdle(); }
    }
    CommandsSingleTime& operator=(const CommandsSingleTime&) = delete;
    CommandsSingleTime& operator=(CommandsSingleTime&&) = default;

    const vk::raii::CommandBuffer& CommandBuffer() const { return buffers[0]; }

private:
    inline std::vector<vk::raii::CommandBuffer> Alloc(const vk::raii::Device& device,
                                                      const vk::raii::CommandPool& command_pool) {
        return device.allocateCommandBuffers({
            .commandPool=*command_pool,
            .level=vk::CommandBufferLevel::ePrimary,
            .commandBufferCount=1
        });
    }
};


}
}