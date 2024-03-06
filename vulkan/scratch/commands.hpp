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


void CopyBufferToImage(const vk::raii::CommandBuffer& command_buffer,
                       const vk::raii::Buffer& src,
                       vk::Image dst,
                       uint32_t width,
                       uint32_t height) {
    vk::BufferImageCopy region{
        .bufferOffset=0,
        .bufferRowLength=0,
        .bufferImageHeight=0,
        .imageSubresource={
            .aspectMask=vk::ImageAspectFlagBits::eColor,
            .mipLevel=0,
            .baseArrayLayer=0,
            .layerCount=1
        },
        .imageOffset={0, 0, 0},
        .imageExtent={static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1}
    };
    command_buffer.copyBufferToImage(*src, dst, vk::ImageLayout::eTransferDstOptimal, {region});
};


void TransitionImageLayout(const vk::raii::CommandBuffer& command_buffer,
                           vk::Image image,
                           vk::ImageLayout old_layout,
                           vk::ImageLayout new_layout) {
    vk::ImageMemoryBarrier barrier{
        .oldLayout=vk::ImageLayout::eUndefined,
        .newLayout=vk::ImageLayout::eTransferDstOptimal,
        .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
        .image=image,
        .subresourceRange={
            .aspectMask=vk::ImageAspectFlagBits::eColor,
            .baseMipLevel=0,
            .levelCount=1,
            .baseArrayLayer=0,
            .layerCount=1
        }
    };
    vk::PipelineStageFlags src_stage{};
    vk::PipelineStageFlags dst_stage{};
    if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eNone;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        src_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        dst_stage = vk::PipelineStageFlagBits::eTransfer;
    } else if (old_layout == vk::ImageLayout::eTransferDstOptimal &&
               new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        src_stage = vk::PipelineStageFlagBits::eTransfer;
        dst_stage = vk::PipelineStageFlagBits::eFragmentShader;
    } else {
        throw std::runtime_error("Unsupported layout transition.");
    }
    command_buffer.pipelineBarrier(src_stage, dst_stage, vk::DependencyFlagBits::eByRegion, nullptr, nullptr, {barrier});
}


}
}
