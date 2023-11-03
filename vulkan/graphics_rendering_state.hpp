#pragma once


#include <array>
#include <optional>
#include <vector>

#include "jms/vulkan/vulkan.hpp"


namespace jms {
namespace vulkan {


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

    vk::PrimitiveTopology primitive_topology{vk::PrimitiveTopology::eTriangleList};
    bool primitive_restart_enabled{false};

    bool rasterization_discard_enabled{false};
    vk::PolygonMode rasterization_polygon_mode{vk::PolygonMode::eFill};
    vk::CullModeFlags rasterization_cull_mode{vk::CullModeFlagBits::eNone};//vk::CullModeFlagBits::eBack};
    vk::FrontFace rasterization_front_face{vk::FrontFace::eCounterClockwise}; //vk::FrontFace::eClockwise  ---- review
    float rasterization_line_width{1.0f};

    bool depth_test_enabled{false};
    bool depth_clamp_enabled{false};
    bool depth_bias_enabled{false};
    std::array<float, 3> depth_bias{0.0f, 0.0f, 0.0f};
    vk::CompareOp depth_compare_op = vk::CompareOp::eNever;
    bool depth_write_enabled{false};
};


}
}