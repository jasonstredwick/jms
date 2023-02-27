#pragma once


#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>

#include "include_config.hpp"
#include <vulkan/vulkan_raii.hpp>

#include "commands.hpp"
#include "memory.hpp"


namespace jms {
namespace vulkan {


/***
 * TODO: Handle textures as part of a larger buffer instead of creating VkBuffer just for this texture.
*/


template <typename T>
struct Texture {
  using Data_t = T;

  uint32_t width;
  uint32_t height;
  vk::Format format;
  vk::raii::Buffer staging_buffer;
  vk::MemoryRequirements staging_memory_reqs;
  vk::raii::DeviceMemory staging_device_memory;
  vk::raii::Image image;
  vk::raii::ImageView image_view;
  vk::MemoryRequirements image_mem_reqs;
  vk::raii::DeviceMemory image_device_memory;

  Texture(const vk::raii::PhysicalDevice& physical_device,
          const vk::raii::Device& device,
          const std::array<uint32_t, 2>& dims,
          const vk::Format format);
  Texture(const Texture&) = delete;
  Texture(Texture&&) = default;
  ~Texture() = default;
  Texture& operator=(const Texture&) = delete;
  Texture& operator=(Texture&&) = default;

  void CopyBufferToImage(const vk::raii::CommandBuffer& command_buffer);
  void TransitionImageLayout(const vk::raii::CommandBuffer& command_buffer,
                             vk::ImageLayout old_layout,
                             vk::ImageLayout new_layout);
};


template <typename T>
Texture<T>::Texture(const vk::raii::PhysicalDevice& physical_device,
                    const vk::raii::Device& device,
                    const std::array<uint32_t, 2>& dims,
                    const vk::Format format)
: width(dims[0]),
  height(dims[1]),
  format(format),
  staging_buffer(vk::raii::Buffer{device, {
    .size=(width * height * sizeof(Data_t)),
    .usage=vk::BufferUsageFlagBits::eTransferSrc,
    .sharingMode=vk::SharingMode::eExclusive
  }}),
  staging_memory_reqs(staging_buffer.getMemoryRequirements()),
  staging_device_memory(device.allocateMemory({
    .allocationSize=staging_memory_reqs.size,
    .memoryTypeIndex=FindMemoryTypeIndex(
        physical_device,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        staging_memory_reqs.memoryTypeBits)
  })),
  image(device.createImage({
    .imageType=vk::ImageType::e2D,
    .format=format,
    .extent={
      .width=width,
      .height=height,
      .depth=1
    },
    .mipLevels=1,
    .arrayLayers=1,
    .samples=vk::SampleCountFlagBits::e1,
    .tiling=vk::ImageTiling::eOptimal,
    .usage=(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled),
    .sharingMode=vk::SharingMode::eExclusive,
    .initialLayout=vk::ImageLayout::eUndefined
  })),
  image_view(device.createImageView({
    .image=*image,
    .viewType=vk::ImageViewType::e2D,
    .format=format,
    .subresourceRange={
      .aspectMask=vk::ImageAspectFlagBits::eColor,
      .baseMipLevel=0,
      .levelCount=1,
      .baseArrayLayer=0,
      .layerCount=1
    }
  })),
  image_mem_reqs(image.getMemoryRequirements()),
  image_device_memory(device.allocateMemory({
    .allocationSize=image_mem_reqs.size,
    .memoryTypeIndex=FindMemoryTypeIndex(
        physical_device,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        image_mem_reqs.memoryTypeBits)
  }))
{
  staging_buffer.bindMemory(*staging_device_memory, 0);
  image.bindMemory(*image_device_memory, 0);
}


template <typename T>
void Texture<T>::CopyBufferToImage(const vk::raii::CommandBuffer& command_buffer) {
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
  command_buffer.copyBufferToImage(*staging_buffer, *image, vk::ImageLayout::eTransferDstOptimal, {region});
};


template <typename T>
void Texture<T>::TransitionImageLayout(const vk::raii::CommandBuffer& command_buffer,
                                       vk::ImageLayout old_layout,
                                       vk::ImageLayout new_layout) {
  vk::ImageMemoryBarrier barrier{
    .oldLayout=vk::ImageLayout::eUndefined,
    .newLayout=vk::ImageLayout::eTransferDstOptimal,
    .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
    .image=*image,
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















void F(const vk::raii::Device& device,
                         const vk::raii::CommandPool& command_pool,
                         const vk::raii::Queue& queue) {
  CommandsSingleTime cst{device, command_pool, queue, true};




  const size_t mem_index = 1 + vulkan_state.swapchain_framebuffers.size();
  const vk::raii::DeviceMemory& device_memory = vulkan_state.device_memory.at(mem_index);
  const vk::MemoryRequirements& buffers_mem_reqs = vulkan_state.buffers_mem_reqs.at(mem_index);


  void* tex_data = device_memory.mapMemory(0, buffers_mem_reqs.size);
  uint32_t* text_data_v = reinterpret_cast<uint32_t*>(tex_data);
  for (uint32_t color=0; color < DIM_X * DIM_Y; ++color) {
      *(text_data_v + static_cast<ptrdiff_t>(color)) = (color << 8) + 255;
  }
  //std::memcpy(tex_data, pixels, 4 * DIM_X * DIM_Y);
  device_memory.unmapMemory();



  CommandsSingleTime cst{device, command_pool, queue, true};
  texture.TransitionImageLayout(command_buffer,
                        vk::ImageLayout::eUndefined,
                        vk::ImageLayout::eTransferDstOptimal);
  texture.CopyBufferToImage(command_buffer);
  texture.TransitionImageLayout(command_buffer,
                        vk::ImageLayout::eTransferDstOptimal,
                        vk::ImageLayout::eShaderReadOnlyOptimal);
  for (size_t index=0; index<vulkan_state.swapchain_framebuffers.size(); ++index) {
      vk::DescriptorBufferInfo buffer_info{
          .buffer=*vulkan_state.buffers.at(1 + index),
          .offset=0,
          .range=sizeof(UniformBufferObject)
      };
      vk::DescriptorImageInfo image_info{
          .imageView=*image_view,
          .imageLayout=vk::ImageLayout::eShaderReadOnlyOptimal
      };
      vulkan_state.devices.at(0).updateDescriptorSets({{
          .dstSet=*vulkan_state.descriptor_sets.at(index),
          .dstBinding=0,
          .dstArrayElement=0,
          .descriptorCount=1,
          .descriptorType=vk::DescriptorType::eUniformBuffer,
          .pImageInfo=nullptr,
          .pBufferInfo=&buffer_info,
          .pTexelBufferView=nullptr
      }}, {});
  }
}


}
}
