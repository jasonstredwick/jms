#include <algorithm>
#include <array>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <vector>

#include "jms/vulkan/vulkan.hpp"


constexpr const size_t WIDTH = 1024;
constexpr const size_t HEIGHT = 1024;


struct GraphicsRenderingState {
    vk::RenderingFlags flags{};
    uint32_t layer_count{1};
    uint32_t view_mask{0};
    vk::Rect2D render_area{.offset{0, 0}, .extent{0, 0}};
    std::vector<vk::RenderingAttachmentInfo> color_attachments{
        {
            .imageLayout=vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp=vk::AttachmentLoadOp::eClear,
            .storeOp=vk::AttachmentStoreOp::eStore,
            .clearValue=vk::ClearValue{.color={std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}}}
        }
    };
    std::optional<vk::RenderingAttachmentInfo> depth_attachment{};
    std::optional<vk::RenderingAttachmentInfo> stencil_attachment{};

    std::vector<vk::Viewport> viewports{
        {
            .x=0.0f,
            .y=0.0f,
            .width=0,
            .height=0,
            .minDepth=0.0f,
            .maxDepth=1.0f
        }
    };
    std::vector<vk::Rect2D> scissors{{.offset={0, 0}, .extent={0, 0}}};

    vk::PrimitiveTopology primitive_topology = vk::PrimitiveTopology::eTriangleList;
    bool primitive_restart_enabled = false;

    bool rasterization_discard_enabled = false;
    vk::PolygonMode rasterization_polygon_mode = vk::PolygonMode::eFill;
    vk::CullModeFlags rasterization_cull_mode = vk::CullModeFlagBits::eBack;
    vk::FrontFace rasterization_front_face = vk::FrontFace::eCounterClockwise; //vk::FrontFace::eClockwise  ---- review
    float rasterization_line_width = 1.0f;
    bool rasterization_depth_clamp_enabled = false;
    bool rasterization_depth_bias_enabled = false;
    std::array<float, 3> rasterization_depth_bias{0.0f, 0.0f, 0.0f};
};





void Convert(
    vk::raii::ImageView& target_image_view,
    vk::Format pixel_format,
    const GraphicsRenderingState& graphics_rendering_info
) {
    std::vector<vk::AttachmentDescription> vk_color_attachments{};
    vk_color_attachments.reserve(graphics_rendering_info.color_attachments.size());
    auto ConvertColorAttachment = [&pixel_format](const vk::RenderingAttachmentInfo& v) {
        return vk::AttachmentDescription{
            .format=pixel_format,
            .samples=vk::SampleCountFlagBits::e1,
            .loadOp=v.loadOp,
            .storeOp=v.storeOp,
            .stencilLoadOp=vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp=vk::AttachmentStoreOp::eDontCare,
            .initialLayout=vk::ImageLayout::eUndefined,
            .finalLayout=vk::ImageLayout::ePresentSrcKHR
        };
    };
    std::ranges::transform(graphics_rendering_info.color_attachments, std::back_inserter(vk_color_attachments),
                           ConvertColorAttachment);

    std::vector<vk::AttachmentReference> vk_color_attachment_references{};
    vk_color_attachment_references.reserve(graphics_rendering_info.color_attachments.size());
    auto ConvertColorAttachmentReference = [&pixel_format](const vk::RenderingAttachmentInfo& v, int32_t index) {
        return vk::AttachmentReference{
            .attachment=static_cast<uint32_t>(index),
            .layout=v.imageLayout
        };
    };
    std::ranges::transform(graphics_rendering_info.color_attachments, std::views::iota(0),
                           std::back_inserter(vk_color_attachment_references),
                           ConvertColorAttachmentReference);

    vk::SubpassDescription subpass_desc{
        .pipelineBindPoint=vk::PipelineBindPoint::eGraphics,
    };
}


#if 0
    std::vector<vk::RenderingAttachmentInfo> color_attachments{
        {
            .imageLayout=vk::ImageLayout::eColorAttachmentOptimal,
            .resolveMode={},
            .resolveImageView={},
            .resolveImageLayout={},
            .clearValue=clear_value
        }
    };


void DrawFrame(DrawState& draw_state) {
    auto& vulkan_state = draw_state.vulkan_state;
    auto& image_available_semaphore = vulkan_state.semaphores.at(0);
    auto& render_finished_semaphore = vulkan_state.semaphores.at(1);
    auto& in_flight_fence = vulkan_state.fences.at(0);
    auto& device = vulkan_state.devices.at(0);
    auto& swapchain = vulkan_state.swapchain;
    auto& swapchain_framebuffers = vulkan_state.swapchain_framebuffers;
    auto& vs_command_buffers_0 = vulkan_state.command_buffers.at(0);
    auto& command_buffer = vs_command_buffers_0.at(0);
    auto& render_pass = vulkan_state.render_passes.at(0);
    auto& swapchain_extent = vulkan_state.render_info.extent;
    auto& pipeline = vulkan_state.pipelines.at(0);
    auto& viewport = vulkan_state.viewports.front();
    auto& scissor = vulkan_state.scissors.front();
    auto& graphics_queue = vulkan_state.graphics_queue;
    auto& present_queue = vulkan_state.present_queue;
    auto& pipeline_layout = vulkan_state.pipeline_layouts.at(0);

    vk::Result result = device.waitForFences({*in_flight_fence}, VK_TRUE, std::numeric_limits<uint64_t>::max());
    device.resetFences({*in_flight_fence});
    uint32_t image_index = 0;
    std::tie(result, image_index) = swapchain.acquireNextImage(std::numeric_limits<uint64_t>::max(), *image_available_semaphore);
    assert(result == vk::Result::eSuccess);
    assert(image_index < swapchain_framebuffers.size());




    vk::raii::ImageView& target_view = vulkan_state.swapchain_image_views[image_index];
    vk::Extent2D target_extent = vulkan_state.render_info.extent;



    auto NeedsLineWidth = [](vk::PolygonMode polygon_mode, bool has_stage_vertex, vk::PrimitiveTopology primitive_topology) -> bool {
        return (polygon_mode == vk::PolygonMode::eLine) ||
               (has_stage_vertex && (primitive_topology == vk::PrimitiveTopology::eLineList ||
                                     primitive_topology == vk::PrimitiveTopology::eLineListWithAdjacency ||
                                     primitive_topology == vk::PrimitiveTopology::eLineStrip ||
                                     primitive_topology == vk::PrimitiveTopology::eLineStripWithAdjacency)) ||
               // if a shader which outputs line primitives is bound to the VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT or VK_SHADER_STAGE_GEOMETRY_BIT stage
               (false);
    };

    bool has_stage_vertex = true;
    bool has_stage_fragment = true;
    bool has_stage_tessellation_evaluation = false;

    bool feature_alpha_to_one_enabled = true;
    bool feature_depth_bounds_enabled = false;
    bool feature_depth_clamp_enabled = false;
    bool feature_pipeline_fragment_shading_rate_enabled = true;
    bool rasterizer_discard_enable = true;
    bool primitive_restart_enable = false;
    bool need_line_width = false;
    bool stencil_test_enabled = false;
    bool fragment_requires_front_face = false; // ??? if the fragment shader uses a variable decorated with FrontFacing
    bool depth_test_enabled = false;
    bool depth_bounds_test_enabled = false;
    bool depth_bias_enable = false;
    vk::PrimitiveTopology primitive_topology = vk::PrimitiveTopology::eTriangleList;
    vk::PolygonMode polygon_mode = vk::PolygonMode::eFill;
    vk::CullModeFlags cull_mode = vk::CullModeFlagBits::eBack;
    vk::FrontFace front_face = vk::FrontFace::eCounterClockwise; //vk::FrontFace::eClockwise,   ---- review
    vk::ClearValue clear_value{.color={std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}}};
    std::vector<vk::Viewport> viewports{
        {
            .x=0.0f,
            .y=0.0f,
            .width=static_cast<float>(target_extent.width),
            .height=static_cast<float>(target_extent.height),
            .minDepth=0.0f,
            .maxDepth=1.0f
        }
    };
    std::vector<vk::Rect2D> scissors{{.offset={0, 0}, .extent=target_extent}};

    std::vector<vk::RenderingAttachmentInfo> color_attachments{
        {
            .imageView=*target_view,
            .imageLayout=vk::ImageLayout::eColorAttachmentOptimal,
            .resolveMode={},
            .resolveImageView={},
            .resolveImageLayout={},
            .loadOp=vk::AttachmentLoadOp::eClear,
            .storeOp=vk::AttachmentStoreOp::eStore,
            .clearValue=clear_value
        }
    };

    vk::RenderingInfo rendering_info{
        .flags={},
        .renderArea={
            .offset={0, 0},
            .extent=target_extent
        },
        .layerCount=1,
        .viewMask=0,
        .colorAttachmentCount=static_cast<uint32_t>(color_attachments.size()),
        .pColorAttachments=color_attachments.data(),
        .pDepthAttachment=nullptr,
        .pStencilAttachment=nullptr
    };

    command_buffer.reset();
    command_buffer.begin({.pInheritanceInfo=nullptr});
    command_buffer.beginRendering(rendering_info);

    // BEGIN required
    command_buffer.setViewportWithCountEXT(viewports);
    command_buffer.setScissorWithCountEXT(scissors);
    //command_buffer.setRasterizationDiscardEnableEXT(); // do not exist
    // END   required

    // BEGIN vertex shader commands
    command_buffer.setVertexInputEXT({});
    command_buffer.setPrimitiveTopologyEXT({});
    if (primitive_topology == vk::PrimitiveTopology::ePatchList) {
        //command_buffer.setPatchControlPointsEXT({});
    }
    command_buffer.setPrimitiveRestartEnableEXT({});
    // END   vertex shader commands

    // BEGIN
    if (!rasterizer_discard_enable) {
        command_buffer.setRasterizationSamplesEXT({});
        command_buffer.setSampleMaskEXT({});
        command_buffer.setAlphaToCoverageEnableEXT({});
        if (feature_alpha_to_one_enabled) {
            command_buffer.setAlphaToOneEnableEXT({});
        }
        command_buffer.setPolygonModeEXT({});
        if (need_line_width) {
            command_buffer.setLineWidth({});
        }
        command_buffer.setCullModeEXT({});
        if (!static_cast<bool>(cull_mode & vk::CullModeFlagBits::eNone) || stencil_test_enabled || fragment_requires_front_face) {
            command_buffer.setFrontFaceEXT({});
        }
        command_buffer.setDepthTestEnableEXT({});
        command_buffer.setDepthWriteEnableEXT({});
        if (depth_test_enabled) {
            command_buffer.setDepthCompareOpEXT({});
        }
        if (feature_depth_bounds_enabled) {
            command_buffer.setDepthBoundsTestEnableEXT({});
        }
        if (depth_bounds_test_enabled) {
            command_buffer.setDepthBounds({});
        }
        command_buffer.setDepthBiasEnableEXT({});
        if (depth_bias_enable) {
            command_buffer.setDepthBias({});
        }
        if (feature_depth_clamp_enabled) {
            command_buffer.setDepthClampEnableEXT({});
        }
        command_buffer.setStencilTestEnableEXT({});
        if (stencil_test_enabled) {
            command_buffer.setStencilOpEXT({});
            command_buffer.setStencilCompareMask({});
            command_buffer.setStencilWriteMask({});
            command_buffer.setStencilReference({});
        }
    }
    // END

    // BEGIN fragment shader commands
    bool feature_logic_op_enabled = false;
    bool logic_op_enabled = false;
    if (!rasterizer_discard_enable) {
        if (feature_logic_op_enabled) {
            command_buffer.setLogicOpEnableEXT({});
        }
        if (logic_op_enabled) {
            command_buffer.setLogicOpEXT({});
        }
        //for each color attachment in this draw buffer: command_buffer.setColorBlendEnableEXT({});
        //for each attachment whose index in pColorBlendEnables is a pointer to VK_TRUE: command_buffer.setColorBlendEnablEXT({});
        /*
        vkCmdSetBlendConstants, if any index in pColorBlendEnables is VK_TRUE, and the same index in pColorBlendEquations is a VkColorBlendEquationEXT structure with any VkBlendFactor member with a value of VK_BLEND_FACTOR_CONSTANT_COLOR, VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR, VK_BLEND_FACTOR_CONSTANT_ALPHA, or VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA
        */
        command_buffer.setColorWriteMaskEXT({});
    }
    // END   fragment shader commands

    // BEGIN shading rate
    if (feature_pipeline_fragment_shading_rate_enabled && !rasterizer_discard_enable) {
        //command_buffer.setFragmentShadingRateKHR();
    }
    // END   shading rate

    /*
     * If the depthClipEnable feature is enabled on the device, the following command must have been called in the command buffer prior to drawing:
            bool feature_depth_clip_enable_enabled = false;
            if (feature_depth_clip_enable_enabled) {
                command_buffer.setDepthClipEnableEXT({});
            }
     */


    command_buffer.beginRenderPass({
        .renderPass=*render_pass,
        .framebuffer=*swapchain_framebuffers[image_index],
        .renderArea={
            .offset={0, 0},
            .extent=swapchain_extent
        },
        .clearValueCount=1,
        .pClearValues=&clear_color // count + values can be array
    }, vk::SubpassContents::eInline);
    command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
    command_buffer.setViewport(0, viewport);
    command_buffer.setScissor(0, scissor);
    command_buffer.bindVertexBuffers(0, {draw_state.vertex_buffer}, {0});
    command_buffer.bindIndexBuffer(draw_state.index_buffer, 0, vk::IndexType::eUint32);
    BindShaders(command_buffer, draw_state.shader_group);
    //command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipeline_layout, 0, {*vulkan_state.descriptor_sets[0]}, {});
    command_buffer.drawIndexed(draw_state.num_indices, 1, 0, 0, 0);
    command_buffer.endRenderPass();
    command_buffer.end();

    std::vector<vk::Semaphore> wait_semaphores{*image_available_semaphore};
    std::vector<vk::Semaphore> signal_semaphores{*render_finished_semaphore};
    std::vector<vk::PipelineStageFlags> dst_stage_mask{vk::PipelineStageFlagBits::eColorAttachmentOutput};
    std::vector<vk::CommandBuffer> command_buffers{*command_buffer};
    graphics_queue.submit(std::array<vk::SubmitInfo, 1>{vk::SubmitInfo{
        .waitSemaphoreCount=static_cast<uint32_t>(wait_semaphores.size()),
        .pWaitSemaphores=wait_semaphores.data(),
        .pWaitDstStageMask=dst_stage_mask.data(),
        .commandBufferCount=static_cast<uint32_t>(command_buffers.size()),
        .pCommandBuffers=command_buffers.data(),
        .signalSemaphoreCount=static_cast<uint32_t>(signal_semaphores.size()),
        .pSignalSemaphores=signal_semaphores.data()
    }}, *in_flight_fence);
    std::vector<vk::SwapchainKHR> swapchains{*swapchain};
    result = present_queue.presentKHR({
        .waitSemaphoreCount=static_cast<uint32_t>(signal_semaphores.size()),
        .pWaitSemaphores=signal_semaphores.data(),
        .swapchainCount=static_cast<uint32_t>(swapchains.size()),
        .pSwapchains=swapchains.data(),
        .pImageIndices=&image_index,
        .pResults=nullptr
    });
}

#endif
