#pragma once


#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <fmt/core.h>
#include "include_config.hpp"
#include <vulkan/vulkan_raii.hpp>


namespace jms {
namespace vulkan {
namespace shader {


vk::raii::ShaderModule Load(const std::filesystem::path& path, const vk::raii::Device& device) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error(fmt::format("Shader file does not exist: {}\n", path.string()));
    }
    std::ifstream file{path, std::ios::ate | std::ios::binary};
    if (!file.is_open()) {
        throw std::runtime_error(fmt::format("Filed to open shader file: {}\n", path.string()));
    }
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    size_t num_bytes = static_cast<size_t>(file.tellg());
    size_t num_uint32 = (num_bytes / sizeof(uint32_t)) + ((num_bytes % sizeof(uint32_t) ? 1 : 0));
    size_t total_bytes = num_uint32 * sizeof(uint32_t);
    std::vector<uint32_t> buffer(num_uint32, 0);
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), num_bytes);
    if (buffer.empty()) {
        throw std::runtime_error(fmt::format("Shader has no code; i.e. empty: {}\n", path.string()));
    }
    return vk::raii::ShaderModule(device, vk::ShaderModuleCreateInfo{
        .codeSize=total_bytes,
        .pCode=buffer.data()
    });
}


}
}
}
