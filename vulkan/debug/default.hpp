#pragma once


#include <functional>

#include "include_config.hpp"
#include <vulkan/vulkan_raii.hpp>
//#include <vulkan/vk_enum_string_helper.h>


namespace jms {
namespace vulkan {


struct DebugConfig {
    static constexpr PFN_vkDebugUtilsMessengerCallbackEXT DefaultDebugFn = +[](
        VkDebugUtilsMessageSeverityFlagBitsEXT,
        VkDebugUtilsMessageTypeFlagsEXT,
        const VkDebugUtilsMessengerCallbackDataEXT *,
        void*) -> VkBool32 { return VK_FALSE; };

    vk::DebugUtilsMessageSeverityFlagsEXT severity_flags{
        //vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
        //vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
    };
    vk::DebugUtilsMessageTypeFlagsEXT msg_type_flags{
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
    };
    PFN_vkDebugUtilsMessengerCallbackEXT debug_fn = DebugConfig::DefaultDebugFn;
};


}
}
