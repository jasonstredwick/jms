#pragma once


#include <algorithm>
#include <format>
#include <map>
#include <memory>
#include <tuple>
#include <vector>

#include "jms/vulkan/vulkan.hpp"
#include "jms/vulkan/graphics_rendering_state.hpp"
#include "jms/vulkan/shader.hpp"
#include "jms/vulkan/utils.hpp"


namespace jms {
namespace vulkan {


struct GraphicsPass {
    GraphicsRenderingState rendering_state{};
    ShaderGroup shader_group{};
    std::vector<std::vector<vk::DescriptorPoolSize>> set_pool_sizes{};
    std::vector<vk::raii::DescriptorSetLayout> layouts{};
    vk::raii::PipelineLayout pipeline_layout{nullptr};
    std::vector<vk::raii::ShaderEXT> shaders{};

    GraphicsPass(vk::raii::Device& device,
                 const GraphicsRenderingState& graphics_rendering_state,
                 const ShaderGroup& shader_group_in,
                 std::optional<vk::AllocationCallbacks*> vk_allocation_callbacks = std::nullopt)
    : rendering_state{graphics_rendering_state}, shader_group{shader_group_in} {
        shader_group.Validate(shader_group.shader_infos);

        set_pool_sizes.reserve(shader_group.set_layout_bindings.size());
        std::ranges::transform(shader_group.set_layout_bindings, std::back_inserter(set_pool_sizes),
            [](const auto& layout_bindings) -> std::vector<vk::DescriptorPoolSize> {
                std::map<vk::DescriptorType, size_t> counts{};
                for (const auto& lb : layout_bindings) { counts[lb.descriptorType]++; }
                std::vector<vk::DescriptorPoolSize> pool_sizes{};
                pool_sizes.reserve(counts.size());
                std::ranges::transform(counts, std::back_inserter(pool_sizes), [](auto& i) -> vk::DescriptorPoolSize {
                    auto [descriptor_type, descriptor_count] = i;
                    return {.type=descriptor_type, .descriptorCount=static_cast<uint32_t>(descriptor_count)};
                });
                return pool_sizes;
            });

        layouts.reserve(shader_group.set_layout_bindings.size());
        std::ranges::transform(shader_group.set_layout_bindings, std::back_inserter(layouts),
            [&device, &vk_allocation_callbacks](const auto& layout_bindings) -> vk::raii::DescriptorSetLayout{
                return device.createDescriptorSetLayout({
                    .bindingCount=static_cast<uint32_t>(layout_bindings.size()),
                    .pBindings=VectorAsPtr(layout_bindings)
                }, vk_allocation_callbacks.value_or(nullptr));
            });

        std::vector<vk::DescriptorSetLayout> vk_layouts{};
        vk_layouts.reserve(layouts.size());
        std::ranges::transform(layouts, std::back_inserter(vk_layouts), [](auto& layout) { return *layout; });
        pipeline_layout = device.createPipelineLayout({
            .setLayoutCount=static_cast<uint32_t>(vk_layouts.size()),
            .pSetLayouts=VectorAsPtr(vk_layouts),
            .pushConstantRangeCount=static_cast<uint32_t>(shader_group.push_constant_ranges.size()),
            .pPushConstantRanges=VectorAsPtr(shader_group.push_constant_ranges)
        }, vk_allocation_callbacks.value_or(nullptr));

        shaders = shader_group.CreateShaders(device, layouts, vk_allocation_callbacks);
    }
    GraphicsPass(const GraphicsPass&) = delete;
    // default ok; only vk::raii, data only structs, std::vector and defaulted destructor
    GraphicsPass(GraphicsPass&&) noexcept = default;
    ~GraphicsPass() noexcept = default;
    GraphicsPass& operator=(const GraphicsPass&) = delete;
    GraphicsPass& operator=(GraphicsPass&&) noexcept = default;

    void BindShaders(vk::raii::CommandBuffer& command_buffer, const std::vector<size_t>& indices) {
        std::vector<vk::ShaderEXT> vk_shaders{};
        std::vector<vk::ShaderStageFlagBits> stage_bits{};
        // add stages bits/shaders and check for duplicate stages (error); invalid indices ... etc
        for (auto& index : indices) {
            vk_shaders.push_back(*shaders.at(index));
            stage_bits.push_back(shader_group.shader_infos.at(index).stage);
        }
        // check features for tesellationShader and geometryShader and disable stages if enabled and not used.
        command_buffer.bindShadersEXT(stage_bits, vk_shaders);
    }

    vk::raii::DescriptorPool CreateDescriptorPool(
        vk::raii::Device& device,
        size_t set_index,
        size_t max_sets,
        std::optional<vk::AllocationCallbacks*> vk_allocation_callbacks = std::nullopt)
    {
        auto& pool_sizes = set_pool_sizes.at(set_index);
        vk::raii::DescriptorPool pool = device.createDescriptorPool({
            .maxSets=static_cast<uint32_t>(max_sets),
            .poolSizeCount=static_cast<uint32_t>(pool_sizes.size()),
            .pPoolSizes=VectorAsPtr(pool_sizes)
        }, vk_allocation_callbacks.value_or(nullptr));
        return pool;
    }

    // Vulkan does not like DescriptorSets to be cleaned up without a special flag.  Going to return the wrapper
    // reference rather than raii variant instead.
    std::vector<vk::DescriptorSet> CreateDescriptorSets(vk::raii::Device& device,
                                                        vk::raii::DescriptorPool& pool,
                                                        std::vector<size_t> set_indices) {
        std::vector<vk::DescriptorSetLayout> vk_layouts{};
        vk_layouts.reserve(set_indices.size());
        std::ranges::transform(set_indices, std::back_inserter(vk_layouts), [&layouts=layouts](size_t index) {
            return *layouts.at(index);
        });
        auto raii_sets = device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
            .descriptorPool=*pool,
            .descriptorSetCount=static_cast<uint32_t>(vk_layouts.size()),
            .pSetLayouts=VectorAsPtr(vk_layouts)
        });
        std::vector<vk::DescriptorSet> vk_descriptor_sets{};
        vk_descriptor_sets.reserve(raii_sets.size());
        std::ranges::transform(raii_sets, std::back_inserter(vk_descriptor_sets), [](auto& i) { return i.release(); });
        return vk_descriptor_sets;
    }

    void ToCommands(vk::raii::CommandBuffer& command_buffer,
                    const std::vector<vk::ImageView>& color_attachment_targets,
                    const std::vector<vk::ImageView>& color_attachment_resolve_targets,
                    const vk::ImageView& depth_image_view,
                    const std::vector<vk::DescriptorSet>& vk_descriptor_sets,
                    const std::vector<uint32_t>& descriptor_set_dynamic_offsets,
                    auto&&... DrawCommands) {
        if (color_attachment_targets.size() != rendering_state.color_attachments.size()) {
            throw std::runtime_error{
                std::format("WriteRenderingCommands: Incorrect number of color attachment targets: {} / {}\n",
                            color_attachment_targets.size(), rendering_state.color_attachments.size())};
        }
        if (color_attachment_resolve_targets.size() > 0 &&
            color_attachment_resolve_targets.size() != rendering_state.color_attachments.size()) {
            throw std::runtime_error{
                std::format("WriteRenderingCommands: Incorrect number of color attachment resolve targets: {} / {}\n",
                            color_attachment_resolve_targets.size(), rendering_state.color_attachments.size())};
        }

        std::vector<vk::RenderingAttachmentInfo> color_attachments{};
        color_attachments.reserve(rendering_state.color_attachments.size());
        std::ranges::transform(rendering_state.color_attachments,
                               color_attachment_targets,
                               std::back_inserter(color_attachments),
                               [](auto copy, auto& target) { copy.imageView = target; return copy; });
        std::ranges::for_each(std::views::zip(color_attachments, color_attachment_resolve_targets),
                              [](auto&& tup) { std::get<0>(tup).resolveImageView = std::get<1>(tup); });

        vk::RenderingAttachmentInfo depth_attachment{};
        if (rendering_state.depth_attachment.has_value()) {
            depth_attachment = rendering_state.depth_attachment.value();
            depth_attachment.imageView = depth_image_view;
        }

        command_buffer.beginRendering({
            .flags=rendering_state.flags,
            .renderArea=rendering_state.render_area,
            .layerCount=rendering_state.layer_count,
            .viewMask=rendering_state.view_mask,
            .colorAttachmentCount=static_cast<uint32_t>(color_attachments.size()),
            .pColorAttachments=VectorAsPtr(color_attachments),
            .pDepthAttachment=(rendering_state.depth_attachment.has_value() ?
                               std::addressof(depth_attachment) : nullptr),
            .pStencilAttachment=(rendering_state.stencil_attachment.has_value() ?
                                 std::addressof(rendering_state.stencil_attachment.value()) : nullptr)
        });

        command_buffer.setViewportWithCountEXT(rendering_state.viewports);
        command_buffer.setScissorWithCountEXT(rendering_state.scissors);
        command_buffer.setPrimitiveTopologyEXT(rendering_state.primitive_topology);
        command_buffer.setPrimitiveRestartEnableEXT(rendering_state.primitive_restart_enabled);

        // multisampling
        //command_buffer.setRasterizationSamplesEXT(vk::SampleCountFlagBits::e1);
        //command_buffer.setAlphaToCoverageEnableEXT(false);
        //command_buffer.setAlphaToOneEnableEXT(false);
        //command_buffer.setSampleMaskEXT(vk::SampleCountFlagBits::e1, {});
        //    ...others?  seems like it is missing some settings like minSampleShading

        // rasterization
        command_buffer.setRasterizerDiscardEnableEXT(rendering_state.rasterization_discard_enabled);
        command_buffer.setPolygonModeEXT(rendering_state.rasterization_polygon_mode);
        command_buffer.setCullModeEXT(rendering_state.rasterization_cull_mode);
        command_buffer.setFrontFaceEXT(rendering_state.rasterization_front_face); //vk::FrontFace::eClockwise  ---- review
        command_buffer.setLineWidth(rendering_state.rasterization_line_width);

        // DepthState
        command_buffer.setDepthTestEnable(rendering_state.depth_test_enabled);
        command_buffer.setDepthClampEnableEXT(rendering_state.depth_clamp_enabled);
        command_buffer.setDepthCompareOp(rendering_state.depth_compare_op);
        command_buffer.setDepthWriteEnable(rendering_state.depth_write_enabled);



        command_buffer.setDepthClipEnableEXT(false); // if not provided then VkPipelineRasterizationDepthClipStateCreateInfoEXT::depthClipEnable or if VkPipelineRasterizationDepthClipStateCreateInfoEXT is not provided then the inverse of setDepthClampEnableEXT
        command_buffer.setDepthClipNegativeOneToOneEXT(false);
        command_buffer.setDepthBoundsTestEnable(false); // VkPipelineDepthStencilStateCreateInfo::depthBoundsTestEnable
        //command_buffer.setDepthBounds(0.0f, 1.0f); // VkPipelineDepthStencilStateCreateInfo::minDepthBounds/maxDepthBounds
        command_buffer.setDepthBiasEnable(rendering_state.depth_bias_enabled);
        //command_buffer.setDepthBias(rendering_state.depth_bias[0],
        //                            rendering_state.depth_bias[1],
        //                            rendering_state.depth_bias[2]);

        // Stencil stuff
        //command_buffer.setStencilTestEnableEXT(false);
        //command_buffer.setStencilOpEXT({}, {}, {}, {}, {});
        //command_buffer.setStencilCompareMask({}, {});
        //command_buffer.setStencilWriteMask({}, {});
        //command_buffer.setStencilReference({}, {});

        //command_buffer.setFragmentShadingRateKHR({}, {});

        //command_buffer.setLogicOpEnableEXT(false);
        //command_buffer.setLogicOpEXT(vk::LogicOp::eCopy);
        /*
        command_buffer.setColorWriteMaskEXT(0, {
            {
                vk::ColorComponentFlagBits::eR |
                vk::ColorComponentFlagBits::eG |
                vk::ColorComponentFlagBits::eB |
                vk::ColorComponentFlagBits::eA
            }
        });
        */

        //std::vector<vk::DescriptorSet> vk_descriptor_sets{};
        //vk_descriptor_sets.reserve(descriptor_sets.size());
        //std::range::transform(descriptor_sets, std::back_inserter(vk_descriptor_sets), [](auto& i) { return *i; });

        std::vector<std::tuple<vk::DescriptorSet, int>> descriptor_sets{};
        command_buffer.setVertexInputEXT(shader_group.vertex_binding_desc, shader_group.vertex_attribute_desc);
        command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipeline_layout, 0, 
                                          vk_descriptor_sets, descriptor_set_dynamic_offsets);

        (DrawCommands(command_buffer), ...);

        command_buffer.endRendering();
    }

    // Need to add support for inline descriptors
    void UpdateDescriptorSets(vk::raii::Device& device,
                              vk::DescriptorSet& descriptor_set,
                              size_t set_index,
                              const std::vector<std::tuple<size_t, vk::DescriptorBufferInfo>>& buffer_data,
                              const std::vector<std::tuple<size_t, vk::DescriptorImageInfo>>& image_data,
                              const std::vector<std::tuple<size_t, vk::BufferView>>& texel_data) {
        std::vector<vk::WriteDescriptorSet> write_data{};
        write_data.reserve(buffer_data.size() + image_data.size() + texel_data.size());

        auto SetBufferF = [](vk::WriteDescriptorSet& data, const vk::DescriptorBufferInfo& value) {
            data.pBufferInfo = std::addressof(value);
        };
        auto SetImageF = [](vk::WriteDescriptorSet& data, const vk::DescriptorImageInfo& value) {
            data.pImageInfo = std::addressof(value);
        };
        auto SetTexelF = [](vk::WriteDescriptorSet& data, const vk::BufferView& value) {
            data.pTexelBufferView = std::addressof(value);
        };
        auto CreateF = [&descriptor_set, &set_index, &set_layout_bindings=shader_group.set_layout_bindings](size_t i) {
                auto& layout = set_layout_bindings.at(set_index).at(i);
                return vk::WriteDescriptorSet{
                    .dstSet=descriptor_set,
                    .dstBinding=layout.binding,
                    .dstArrayElement=0,
                    .descriptorCount=1,
                    .descriptorType=layout.descriptorType,
                    .pImageInfo=nullptr,
                    .pBufferInfo=nullptr,
                    .pTexelBufferView=nullptr
                };
            };
        auto F = [](auto&& CreateF, auto&& SetF) { return [&CreateF, &SetF](auto& data) {
            const auto& [layout_index, info] = data;
            vk::WriteDescriptorSet out = CreateF(layout_index);
            SetF(out, info);
            return out;
        }; };

        std::ranges::transform(buffer_data, std::back_inserter(write_data), F(CreateF, SetBufferF));
        std::ranges::transform(image_data, std::back_inserter(write_data), F(CreateF, SetImageF));
        std::ranges::transform(texel_data, std::back_inserter(write_data), F(CreateF, SetTexelF));

        device.updateDescriptorSets(write_data, {});
    }
};


}
}
