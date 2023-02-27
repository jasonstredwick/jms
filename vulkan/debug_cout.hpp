#pragma once


#include <format>
#include <iostream>
#include <ranges>

#include "include_config.hpp"
#include <vulkan/vulkan_raii.hpp>


namespace jms {
namespace vulkan {


VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessage_cout(VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity,
                                                 VkDebugUtilsMessageTypeFlagsEXT msg_type,
                                                 VkDebugUtilsMessengerCallbackDataEXT const* callback_data,
                                                 void* user_data) {
    auto& out = std::cout;
    out << std::format("{} ({}): {} {}\n{}\n",
        vk::to_string(static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(msg_severity)),
        vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(msg_type)),
        callback_data->pMessageIdName,
        callback_data->messageIdNumber,
        callback_data->pMessage);

    if (callback_data->queueLabelCount > 0) {
        out << std::format("\nQueue Labels:\n");
        for (size_t i : std::views::iota(static_cast<size_t>(0), static_cast<size_t>(callback_data->queueLabelCount))) {
            out << std::format("\tName: {}\n", callback_data->pQueueLabels[i].pLabelName);
        }
    }

    if (callback_data->cmdBufLabelCount > 0) {
        out << std::format("\nCommandBuffer Labels:\n");
        for (size_t i : std::views::iota(static_cast<size_t>(0), static_cast<size_t>(callback_data->cmdBufLabelCount))) {
            out << std::format("\tName: {}\n", callback_data->pCmdBufLabels[i].pLabelName);
        }
    }

    if (callback_data->objectCount > 0) {
        out << std::format("\nObjects:\n");
        for (size_t i : std::views::iota(static_cast<size_t>(0), static_cast<size_t>(callback_data->objectCount))) {
            std::string obj_name = (callback_data->pObjects[i].pObjectName) ?
                std::format("\t{}", std::string_view{callback_data->pObjects[i].pObjectName}) : "";
            out << std::format("\t{}\t{}\t{}{}\n",
                i, callback_data->pObjects[i].objectType, callback_data->pObjects[i].objectHandle, obj_name);
        }
    }

    return VK_FALSE;
}


}
}
