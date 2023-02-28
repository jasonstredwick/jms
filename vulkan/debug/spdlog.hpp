#pragma once


#include <format>
#include <ranges>
#include <sstream>

#include <spdlog/spdlog.h>
#include "include_config.hpp"
#include <vulkan/vulkan_raii.hpp>


namespace jms {
namespace vulkan {


VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessage_spdlog(VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity,
                                                   VkDebugUtilsMessageTypeFlagsEXT msg_type,
                                                   VkDebugUtilsMessengerCallbackDataEXT const* callback_data,
                                                   void* user_data) {
    std::ostringstream ss{};
    ss << std::format("({}): {} {}\n{}\n",
        vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(msg_type)),
        callback_data->pMessageIdName,
        callback_data->messageIdNumber,
        callback_data->pMessage);

    if (callback_data->queueLabelCount > 0) {
        ss << std::format("\nQueue Labels:\n");
        for (size_t i : std::views::iota(static_cast<size_t>(0), static_cast<size_t>(callback_data->queueLabelCount))) {
            ss << std::format("\tName: {}\n", callback_data->pQueueLabels[i].pLabelName);
        }
    }

    if (callback_data->cmdBufLabelCount > 0) {
        ss << std::format("\nCommandBuffer Labels:\n");
        for (size_t i : std::views::iota(static_cast<size_t>(0), static_cast<size_t>(callback_data->cmdBufLabelCount))) {
            ss << std::format("\tName: {}\n", callback_data->pCmdBufLabels[i].pLabelName);
        }
    }

    if (callback_data->objectCount > 0) {
        ss << std::format("\nObjects:\n");
        for (size_t i : std::views::iota(static_cast<size_t>(0), static_cast<size_t>(callback_data->objectCount))) {
            std::string obj_name = (callback_data->pObjects[i].pObjectName) ?
                std::format("\t{}", std::string_view{callback_data->pObjects[i].pObjectName}) : "";
            ss << std::format("\t{}\t{}\t{}{}\n",
                i, callback_data->pObjects[i].objectType, callback_data->pObjects[i].objectHandle, obj_name);
        }
    }

    switch (msg_severity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        spdlog::warn(ss.str());
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        spdlog::error(ss.str());
        break;
    default:
        spdlog::info(ss.str());
        break;
    }

    return VK_FALSE;
}


}
}
