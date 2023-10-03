#pragma once


#include <vector>

#include "jms/vulkan/vulkan.hpp"


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


void InitPipeline(const vk::raii::Device& device,
                         const vk::raii::RenderPass& render_pass,
                         const vk::Extent2D target_extent,
                         //const VertexDescription& vertex_desc,
                         const std::vector<vk::DescriptorSetLayoutBinding>& layout_bindings,
                         const std::vector<jms::vulkan::shader::Info>& shaders);

struct GraphicsPipelineConfig {
    vk::PipelineInputAssemblyStateCreateInfo input_assembly_info{
        .topology=vk::PrimitiveTopology::eTriangleList,
        .primitiveRestartEnable=false
    };



    std::vector<vk::PipelineShaderStageCreateInfo> shader_stages{};
    shader_stages.reserve(shaders.size());
    for (const auto& shader_info : shaders) {
        vk::raii::ShaderModule& module = shader_modules.emplace_back(device, vk::ShaderModuleCreateInfo{
            .codeSize=(shader_info.code.size() * sizeof(decltype(shader_info.code)::value_type)),
            .pCode=shader_info.code.data()
        });
        shader_stages.push_back(vk::PipelineShaderStageCreateInfo{
            .stage=shader_info.stage,
            .module=*module,
            .pName=shader_info.entry_point_name.c_str()
        });
    }


    std::vector<vk::DynamicState> dynamic_states = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};

    vk::PipelineDynamicStateCreateInfo dynamic_state_info{
        .dynamicStateCount=static_cast<uint32_t>(dynamic_states.size()),
        .pDynamicStates=dynamic_states.data()
    };

    // *********************************************************
    // Default viewport and scissor; stored in state for use when drawing with dynamic viewport/scissor
    viewports.push_back(vk::Viewport{
        .x=0.0f,
        .y=0.0f,
        .width=static_cast<float>(target_extent.width),
        .height=static_cast<float>(target_extent.height),
        .minDepth=0.0f,
        .maxDepth=1.0f
    });

    scissors.push_back(vk::Rect2D{
        .offset={0, 0},
        .extent=target_extent
    });
    // *********************************************************

    vk::PipelineViewportStateCreateInfo viewport_info{
        .viewportCount=1,
        .scissorCount=1,
    };
    // *********************************************************

    vk::PipelineRasterizationStateCreateInfo rasterizer_info{
        .depthClampEnable=VK_FALSE,
        .rasterizerDiscardEnable=VK_FALSE,
        .polygonMode=vk::PolygonMode::eFill,
        .cullMode=vk::CullModeFlagBits::eBack,
        .frontFace=vk::FrontFace::eCounterClockwise, //vk::FrontFace::eClockwise,   ---- review
        .depthBiasEnable=VK_FALSE,
        .depthBiasConstantFactor=0.0f,
        .depthBiasClamp=0.0f,
        .depthBiasSlopeFactor=0.0f,
        .lineWidth=1.0f
    };

    vk::PipelineMultisampleStateCreateInfo multisampling_info{
        .rasterizationSamples=vk::SampleCountFlagBits::e1,
        .sampleShadingEnable=VK_FALSE,
        .minSampleShading=1.0f,
        .pSampleMask=nullptr,
        .alphaToCoverageEnable=VK_FALSE,
        .alphaToOneEnable=VK_FALSE
    };

    vk::PipelineColorBlendAttachmentState color_blend_attachment{
        .blendEnable=VK_FALSE,
        .srcColorBlendFactor=vk::BlendFactor::eOne,
        .dstColorBlendFactor=vk::BlendFactor::eZero,
        .colorBlendOp=vk::BlendOp::eAdd,
        .srcAlphaBlendFactor=vk::BlendFactor::eOne,
        .dstAlphaBlendFactor=vk::BlendFactor::eZero,
        .alphaBlendOp=vk::BlendOp::eAdd,
        .colorWriteMask=vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
    };

    vk::PipelineColorBlendStateCreateInfo color_blend_info{
        .logicOpEnable=VK_FALSE,
        .logicOp=vk::LogicOp::eCopy,
        .attachmentCount=1,
        .pAttachments=&color_blend_attachment,
        .blendConstants=std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}
    };

    descriptor_set_layouts.push_back(device.createDescriptorSetLayout({
        .bindingCount=static_cast<uint32_t>(layout_bindings.size()),
        .pBindings=layout_bindings.data()
    }));
    std::vector<vk::DescriptorSetLayout> vk_descriptor_set_layouts{};
    std::ranges::transform(descriptor_set_layouts, std::back_inserter(vk_descriptor_set_layouts), [](auto& dsl) {
        return *dsl;
    });

    auto vertex_info = vertex_desc.GetInfo();
    pipeline_layouts.push_back(device.createPipelineLayout({
        .setLayoutCount=static_cast<uint32_t>(vk_descriptor_set_layouts.size()),
        .pSetLayouts=vk_descriptor_set_layouts.data(),
        .pushConstantRangeCount=0,
        .pPushConstantRanges=nullptr
    }));
    vk::PipelineLayout vk_pipeline_layout = *pipeline_layouts.back();
    vk::RenderPass vk_render_pass = *render_pass;
    uint32_t subpass_id = 0;
    pipelines.push_back(vk::raii::Pipeline(device, nullptr, vk::GraphicsPipelineCreateInfo{
        .stageCount=static_cast<uint32_t>(shader_stages.size()),
        .pStages=shader_stages.data(),
        .pVertexInputState=&vertex_info,
        .pInputAssemblyState=&input_assembly_info,
        .pViewportState=&viewport_info,
        .pRasterizationState=&rasterizer_info,
        .pMultisampleState=&multisampling_info,
        .pDepthStencilState=nullptr,
        .pColorBlendState=&color_blend_info,
        .pDynamicState=&dynamic_state_info,
        .layout=vk_pipeline_layout,
        .renderPass=vk_render_pass,
        .subpass=subpass_id,
        .basePipelineHandle=VK_NULL_HANDLE,
        .basePipelineIndex=-1
    }));
};




#if 0
    vulkan_state.InitPipeline(vulkan_state.devices.at(0),
                              vulkan_state.render_passes.at(0),
                              vulkan_state.render_info.extent,
                              jms::vulkan::VertexDescription::Create<Vertex>(0),
                              std::vector<vk::DescriptorSetLayoutBinding>{
                                  vk::DescriptorSetLayoutBinding{
                                      .binding=0,
                                      .descriptorType=vk::DescriptorType::eStorageBuffer,
                                      .descriptorCount=1,
                                      .stageFlags=vk::ShaderStageFlagBits::eVertex,
                                      .pImmutableSamplers=nullptr
                                  },
                                  vk::DescriptorSetLayoutBinding{
                                      .binding=1,
                                      .descriptorType=vk::DescriptorType::eStorageBuffer,
                                      .descriptorCount=1,
                                      .stageFlags=vk::ShaderStageFlagBits::eVertex,
                                      .pImmutableSamplers=nullptr
                                  },
                                  vk::DescriptorSetLayoutBinding{
                                      .binding=2,
                                      .descriptorType=vk::DescriptorType::eStorageBuffer,
                                      .descriptorCount=1,
                                      .stageFlags=vk::ShaderStageFlagBits::eVertex,
                                      .pImmutableSamplers=nullptr
                                  }
                              },
                              LoadShaders(vulkan_state.devices.at(0)));

void State::InitPipeline(const vk::raii::Device& device,
                         const vk::raii::RenderPass& render_pass,
                         const vk::Extent2D target_extent,
                         const VertexDescription& vertex_desc,
                         const std::vector<vk::DescriptorSetLayoutBinding>& layout_bindings,
                         const std::vector<jms::vulkan::shader::Info>& shaders) {
    std::vector<vk::PipelineShaderStageCreateInfo> shader_stages{};
    shader_stages.reserve(shaders.size());
    for (const auto& shader_info : shaders) {
        vk::raii::ShaderModule& module = shader_modules.emplace_back(device, vk::ShaderModuleCreateInfo{
            .codeSize=(shader_info.code.size() * sizeof(decltype(shader_info.code)::value_type)),
            .pCode=shader_info.code.data()
        });
        shader_stages.push_back(vk::PipelineShaderStageCreateInfo{
            .stage=shader_info.stage,
            .module=*module,
            .pName=shader_info.entry_point_name.c_str()
        });
    }

    vk::PipelineInputAssemblyStateCreateInfo input_assembly_info{
        .topology=vk::PrimitiveTopology::eTriangleList,
        .primitiveRestartEnable=VK_FALSE
    };

    std::vector<vk::DynamicState> dynamic_states = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};

    vk::PipelineDynamicStateCreateInfo dynamic_state_info{
        .dynamicStateCount=static_cast<uint32_t>(dynamic_states.size()),
        .pDynamicStates=dynamic_states.data()
    };

    // *********************************************************
    // Default viewport and scissor; stored in state for use when drawing with dynamic viewport/scissor
    viewports.push_back(vk::Viewport{
        .x=0.0f,
        .y=0.0f,
        .width=static_cast<float>(target_extent.width),
        .height=static_cast<float>(target_extent.height),
        .minDepth=0.0f,
        .maxDepth=1.0f
    });

    scissors.push_back(vk::Rect2D{
        .offset={0, 0},
        .extent=target_extent
    });
    // *********************************************************

    vk::PipelineViewportStateCreateInfo viewport_info{
        .viewportCount=1,
        .scissorCount=1,
    };
    // *********************************************************

    vk::PipelineRasterizationStateCreateInfo rasterizer_info{
        .depthClampEnable=VK_FALSE,
        .rasterizerDiscardEnable=VK_FALSE,
        .polygonMode=vk::PolygonMode::eFill,
        .cullMode=vk::CullModeFlagBits::eBack,
        .frontFace=vk::FrontFace::eCounterClockwise, //vk::FrontFace::eClockwise,   ---- review
        .depthBiasEnable=VK_FALSE,
        .depthBiasConstantFactor=0.0f,
        .depthBiasClamp=0.0f,
        .depthBiasSlopeFactor=0.0f,
        .lineWidth=1.0f
    };

    vk::PipelineMultisampleStateCreateInfo multisampling_info{
        .rasterizationSamples=vk::SampleCountFlagBits::e1,
        .sampleShadingEnable=VK_FALSE,
        .minSampleShading=1.0f,
        .pSampleMask=nullptr,
        .alphaToCoverageEnable=VK_FALSE,
        .alphaToOneEnable=VK_FALSE
    };

    vk::PipelineColorBlendAttachmentState color_blend_attachment{
        .blendEnable=VK_FALSE,
        .srcColorBlendFactor=vk::BlendFactor::eOne,
        .dstColorBlendFactor=vk::BlendFactor::eZero,
        .colorBlendOp=vk::BlendOp::eAdd,
        .srcAlphaBlendFactor=vk::BlendFactor::eOne,
        .dstAlphaBlendFactor=vk::BlendFactor::eZero,
        .alphaBlendOp=vk::BlendOp::eAdd,
        .colorWriteMask=vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
    };

    vk::PipelineColorBlendStateCreateInfo color_blend_info{
        .logicOpEnable=VK_FALSE,
        .logicOp=vk::LogicOp::eCopy,
        .attachmentCount=1,
        .pAttachments=&color_blend_attachment,
        .blendConstants=std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}
    };

    descriptor_set_layouts.push_back(device.createDescriptorSetLayout({
        .bindingCount=static_cast<uint32_t>(layout_bindings.size()),
        .pBindings=layout_bindings.data()
    }));
    std::vector<vk::DescriptorSetLayout> vk_descriptor_set_layouts{};
    std::ranges::transform(descriptor_set_layouts, std::back_inserter(vk_descriptor_set_layouts), [](auto& dsl) {
        return *dsl;
    });

    auto vertex_info = vertex_desc.GetInfo();
    pipeline_layouts.push_back(device.createPipelineLayout({
        .setLayoutCount=static_cast<uint32_t>(vk_descriptor_set_layouts.size()),
        .pSetLayouts=vk_descriptor_set_layouts.data(),
        .pushConstantRangeCount=0,
        .pPushConstantRanges=nullptr
    }));
    vk::PipelineLayout vk_pipeline_layout = *pipeline_layouts.back();
    vk::RenderPass vk_render_pass = *render_pass;
    uint32_t subpass_id = 0;
    pipelines.push_back(vk::raii::Pipeline(device, nullptr, vk::GraphicsPipelineCreateInfo{
        .stageCount=static_cast<uint32_t>(shader_stages.size()),
        .pStages=shader_stages.data(),
        .pVertexInputState=&vertex_info,
        .pInputAssemblyState=&input_assembly_info,
        .pViewportState=&viewport_info,
        .pRasterizationState=&rasterizer_info,
        .pMultisampleState=&multisampling_info,
        .pDepthStencilState=nullptr,
        .pColorBlendState=&color_blend_info,
        .pDynamicState=&dynamic_state_info,
        .layout=vk_pipeline_layout,
        .renderPass=vk_render_pass,
        .subpass=subpass_id,
        .basePipelineHandle=VK_NULL_HANDLE,
        .basePipelineIndex=-1
    }));
}

void State::InitRenderPass(const vk::raii::Device& device, const vk::Format pixel_format, const vk::Extent2D target_extent) {
    vk::AttachmentDescription color_attachment{
        .format=pixel_format,
        .samples=vk::SampleCountFlagBits::e1,
        .loadOp=vk::AttachmentLoadOp::eClear,
        .storeOp=vk::AttachmentStoreOp::eStore,
        .stencilLoadOp=vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp=vk::AttachmentStoreOp::eDontCare,
        .initialLayout=vk::ImageLayout::eUndefined,
        .finalLayout=vk::ImageLayout::ePresentSrcKHR
    };

    vk::AttachmentReference color_attachment_reference{
        .attachment=0,
        .layout=vk::ImageLayout::eColorAttachmentOptimal
    };

    vk::SubpassDescription subpass_desc{
        .pipelineBindPoint=vk::PipelineBindPoint::eGraphics,
        .inputAttachmentCount=0,
        .pInputAttachments=nullptr,
        .colorAttachmentCount=1,
        .pColorAttachments=&color_attachment_reference,
        .pResolveAttachments=nullptr,
        .pDepthStencilAttachment=nullptr,
        .preserveAttachmentCount=0,
        .pPreserveAttachments=nullptr
    };

    std::vector<vk::SubpassDependency> subpass_deps{
        {
            .srcSubpass=VK_SUBPASS_EXTERNAL,// vk::SubpassDependency::eE...?
            .dstSubpass=0,
            .srcStageMask=vk::PipelineStageFlagBits::eColorAttachmentOutput,
            .dstStageMask=vk::PipelineStageFlagBits::eColorAttachmentOutput,
            .srcAccessMask=vk::AccessFlagBits::eNone,
            .dstAccessMask=vk::AccessFlagBits::eColorAttachmentWrite
        }
    };

    render_passes.push_back(vk::raii::RenderPass{device, {
        .attachmentCount=1,
        .pAttachments=&color_attachment,
        .subpassCount=1,
        .pSubpasses=&subpass_desc,
        .dependencyCount=static_cast<uint32_t>(subpass_deps.size()),
        .pDependencies=subpass_deps.data()
    }});
}


#endif
