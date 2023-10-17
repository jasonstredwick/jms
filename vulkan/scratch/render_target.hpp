#pragma once


#if 0
class RenderTargets {
    vk::raii::SwapchainKHR& swapchain;
    std::vector<vk::raii::ImageView>& targets;
    std::vector<vk::raii::ImageView>& swapchain_targets;
    std::vector<vk::raii::ImageView>::iterator target;
    std::vector<vk::raii::ImageView>::iterator sc_target;
    std::mutex mutex{};

public:
    RenderTargets(vk::raii::SwapchainKHR& swapchain_in,
                  std::vector<vk::raii::ImageView>& targets_in,
                  std::vector<vk::raii::ImageView>& swapchain_targets)
    : swapchain{swapchain_in},
      targets{targets_in},
      swapchain_targets{swapchain_targets},
      target{targets.end()},
      sc_target{targets.end()}
    {
        if (targets.size() < 2) { std::runtime_error{"RenderTargets require minimum of two targets."}; }
    }
    vk::raii::ImageView& NextTarget() {
        std::lock_guard lock{mutex};
        if (target != targets.end()) {
            target = std::ranges::next(target);
            if (target == targets.end()) { target = targets.begin(); }
        } else {
            target = targets.begin();
        }
        if (target == sc_target) {
            target = std::ranges::next(target);
            if (target == targets.end()) { target = targets.begin(); }
        }
        return *target;
    }
};


    jms::vulkan::ImageInfo render_target_info{
        .flags={},
        .image_type=vk::ImageType::e2D,
        .format=vk::Format::eR8G8B8A8Unorm,
        .extent={.width=WINDOW_WIDTH, .height=WINDOW_HEIGHT, .depth=1},
        .mip_levels=1,
        .array_layers=1,
        .samples=vk::SampleCountFlagBits::e1,
        .tiling=vk::ImageTiling::eOptimal,
        .usage=(vk::ImageUsageFlagBits::eColorAttachment |
                vk::ImageUsageFlagBits::eSampled |
                vk::ImageUsageFlagBits::eTransferSrc),
        .initial_layout=vk::ImageLayout::eUndefined
    };

    jms::vulkan::ImageViewInfo render_target_view_info{
        .flags={},
        .view_type=vk::ImageViewType::e2D,
        .format=vk::Format::eR8G8B8A8Unorm,
        .components={
            .r = vk::ComponentSwizzle::eIdentity,
            .g = vk::ComponentSwizzle::eIdentity,
            .b = vk::ComponentSwizzle::eIdentity,
            .a = vk::ComponentSwizzle::eIdentity
        },
        .subresource={
            .aspectMask=vk::ImageAspectFlagBits::eColor,
            .baseMipLevel=0,
            .levelCount=1,
            .baseArrayLayer=0,
            .layerCount=1
        }
    };

        using ImageResourceAllocator = jms::vulkan::ImageResourceAllocator<std::vector, jms::NoMutex>;
        using Image = jms::vulkan::Image<std::vector, jms::NoMutex>;

        auto image_allocator = vulkan_state.memory_helper.CreateImageAllocator(0);
        std::vector<Image> images{};
        images.reserve(3);
        std::ranges::generate_n(std::back_inserter(images), 3, [&a=image_allocator, &b=app_state.render_target_info]() {
            template <typename T> using Container_t = typename std::remove_cvref_t<decltype(a)>::Container_t<T>;
            using Mutex_t = typename std::remove_cvref_t<decltype(a)>::Mutex_t;
            return jms::vulkan::Image<Container_t, Mutex_t>{a, b};
        });

        std::vector<vk::raii::ImageView> image_views{};
        image_views.reserve(3);
        std::ranges::transform(images, std::back_inserter(image_views),
            [&info=app_state.render_target_view_info](const auto& image) { return image.CreateView(info); });


    command_buffer_1.reset();
    command_buffer_1.begin({.pInheritanceInfo=nullptr});
    command_buffer_1.copyImage(target_image, vk::ImageLayout::eColorAttachmentOptimal,
                               swapchain_image, vk::ImageLayout::eColorAttachmentOptimal, {});
    command_buffer_1.end();


    graphics_queue.submit(std::array<vk::SubmitInfo, 1>{vk::SubmitInfo{
        .waitSemaphoreCount=static_cast<uint32_t>(render_semaphores.size()),
        .pWaitSemaphores=jms::vulkan::VectorAsPtr(render_semaphores),
        .pWaitDstStageMask=jms::vulkan::VectorAsPtr(dst_stage_mask),
        .commandBufferCount=static_cast<uint32_t>(command_buffers_0.size()),
        .pCommandBuffers=jms::vulkan::VectorAsPtr(command_buffers_0),
        .signalSemaphoreCount=static_cast<uint32_t>(present_semaphores.size()),
        .pSignalSemaphores=jms::vulkan::VectorAsPtr(present_semaphores)
    }}, *in_flight_fence);

#endif
