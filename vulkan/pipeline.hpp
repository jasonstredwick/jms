#pragma once


#include <vector>

#include "vulkan.hpp"


struct PipelineState {
  //???
  std::vector<vk::PipelineShaderStageCreateInfo> shader_stages{};
  vk::PipelineInputAssemblyStateCreateInfo input_assembly_info{};
  std::vector<vk::DynamicState> dynamic_states{};
  std::vector<vk::Viewport> static_viewports{};
  std::vector<vk::Rect2D> static_scissors{};
  vk::PipelineRasterizationStateCreateInfo rasterization_info{};
  vk::PipelineMultisampleStateCreateInfo multisampling_info{};
  vk::PipelineColorBlendAttachmentState color_blend_attachment{};
  vk::PipelineColorBlendStateCreateInfo color_blend_info{};
};
