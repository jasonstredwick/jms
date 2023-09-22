#pragma once


#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "jms/vulkan/vulkan.hpp"


namespace jms {
namespace vulkan {
namespace shader {


struct Info {
    uint32_t subgroup_size{0};
    vk::ShaderCreateFlagsEXT flags{};
    vk::ShaderStageFlagBits stage{};
    vk::ShaderStageFlags next_stage{};
    vk::ShaderCodeTypeEXT code_type;
    std::vector<uint32_t> code{};
    std::vector<vk::DescriptorSetLayoutBinding> layout_bindings{};
    std::vector<vk::PushConstantRange> push_constant_ranges{};
    vk::SpecializationInfo specialization_info{};
    std::string entry_point_name{};
};


struct ShaderGroup {
    vk::raii::Device* device;
    vk::AllocationCallbacks* vk_allocation_callbacks;
    std::vector<Info> shader_info{};
    std::vector<vk::raii::DescriptorSetLayout> layouts{};
    std::vector<vk::raii::ShaderEXT> shaders{};

    ShaderGroup(vk::raii::Device& device,
                std::vector<Info>&& shader_info_input,
                vk::AllocationCallbacks* vk_allocation_callbacks = nullptr)
    : device{std::addressof(device)},
      vk_allocation_callbacks{vk_allocation_callbacks},
      shader_info{std::move(shader_info_input)}
    {
        Validate(shader_info);

        std::vector<vk::PipelineShaderStageRequiredSubgroupSizeCreateInfo> pnexts{};
        std::vector<vk::ShaderCreateInfoEXT> create_infos{};
        for (auto& info : shader_info) {
            pnexts.push_back({.requiredSubgroupSize=info.subgroup_size});
            auto& pnext = pnexts.back();
            auto& layout = layouts.emplace_back(device.createDescriptorSetLayout({
                .bindingCount=static_cast<uint32_t>(info.layout_bindings.size()),
                .pBindings=info.layout_bindings.data()
            }));
            std::vector<vk::DescriptorSetLayout> vk_descriptor_set_layouts{*layout};
            vk::ShaderCreateInfoEXT create_info{
                .pNext=(info.subgroup_size > 0 ? std::addressof(pnext) : nullptr),
                .flags=info.flags,
                .stage=info.stage,
                .nextStage=info.next_stage,
                .codeType=info.code_type,
                .codeSize=(info.code.size() * sizeof(decltype(info.code)::value_type)),
                .pCode=info.code.data(),
                .pName=info.entry_point_name.c_str(),
                .setLayoutCount=static_cast<uint32_t>(vk_descriptor_set_layouts.size()),
                .pSetLayouts=vk_descriptor_set_layouts.data(),
                .pushConstantRangeCount=static_cast<uint32_t>(info.push_constant_ranges.size()),
                .pPushConstantRanges=info.push_constant_ranges.data(),
                .pSpecializationInfo=std::addressof(info.specialization_info)
            };
            create_infos.push_back(create_info);
        }
        shaders = this->device->createShadersEXT(create_infos, vk_allocation_callbacks);
    }
    ShaderGroup(const ShaderGroup&) = delete;
    ShaderGroup& operator=(const ShaderGroup&) = delete;

    void Bind(vk::raii::CommandBuffer& command_buffer,
              const std::vector<size_t> indices,
              const std::vector<vk::ShaderStageFlagBits> stage_bits) {
        std::vector<vk::ShaderEXT> vk_shaders{};
        // add stages bits/shaders and check for duplicate stages (error); invalid indices ... etc
        for (auto& index : indices) {
            vk_shaders.push_back(*shaders[index]);
        }
        // check features for tesellationShader and geometryShader and disable stages if enabled and not used.
        command_buffer.bindShadersEXT(stage_bits, vk_shaders);
    }

    vk::raii::Device& GetDevice() const noexcept { return *device; }
    vk::AllocationCallbacks* GetAllocationCallbacks() const noexcept { return vk_allocation_callbacks; }
    const std::vector<Info>& GetShaderInfo() const noexcept { return shader_info; }

private:
    static constexpr vk::ShaderStageFlags UNLINKABLE_STAGES = ~(vk::ShaderStageFlagBits::eAllGraphics |
                                                                vk::ShaderStageFlagBits::eTaskEXT |
                                                                vk::ShaderStageFlagBits::eMeshEXT);

    static constexpr auto IsUnlinkable = [](const Info& v) -> bool {
        return static_cast<bool>(v.flags & vk::ShaderCreateFlagBitsEXT::eLinkStage) &&
               static_cast<bool>(v.stage & UNLINKABLE_STAGES);
    };

    static constexpr auto IsBadFragment = [](const Info& v) -> bool {
        bool has_fragment_stage = static_cast<bool>(v.stage & vk::ShaderStageFlagBits::eFragment);
        if (has_fragment_stage) {
            // Also need to check for feature attachmentFragmentShadingRate ???
            if (v.flags & vk::ShaderCreateFlagBitsEXT::eFragmentShadingRateAttachment) { return true; }
            // Also need to check for feature fragmentDensityMap ???
            if (v.flags & vk::ShaderCreateFlagBitsEXT::eFragmentDensityMapAttachment) { return true; }
        }
        return false;
    };

    static constexpr auto IsBadSubgroupSize(uint32_t size) -> bool {
        if (size > 0) {
            if (!std::has_single_bit(size)) { return true; }
            // Check that size >= minSubgroupSize and size <= maxSubgroupSize
        }
        return false;
    }

    // Consider an uber validation function with custom exceptions vs current approach with individual validations.
    void Validate(const std::vector<Info>& shader_info) {
        if (auto it = std::ranges::find_if(shader_info, ShaderGroup::IsUnlinkable); it != shader_info.end()) {
            throw std::runtime_error{"Info cannot be linked; invalid stage provided."};
        }
        if (auto it = std::ranges::find_if(shader_info, ShaderGroup::IsBadFragment); it != shader_info.end()) {
            throw std::runtime_error{"ShaderExtInfo has bad fragment related flags."};
        }
        if (auto it = std::ranges::find_if(shader_info, ShaderGroup::IsBadSubgroupSize, &Info::subgroup_size);
            it != shader_info.end()) {
            throw std::runtime_error{
                "vk::PipelineShaderStageRequiredSubgroupSizeCreateInfo requires power of two size."};
        }
        // Do more of the rest of the required validation here ...
    }
};


std::vector<uint32_t> Load(const std::filesystem::path& path, const vk::raii::Device& device) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error(std::format("Shader file does not exist: {}\n", path.string()));
    }
    std::ifstream file{path, std::ios::ate | std::ios::binary};
    if (!file.is_open()) {
        throw std::runtime_error(std::format("Filed to open shader file: {}\n", path.string()));
    }
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    size_t num_bytes = static_cast<size_t>(file.tellg());
    size_t num_uint32 = (num_bytes / sizeof(uint32_t)) + ((num_bytes % sizeof(uint32_t) ? 1 : 0));
    size_t total_bytes = num_uint32 * sizeof(uint32_t);
    std::vector<uint32_t> buffer(num_uint32, 0);
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), num_bytes);
    if (buffer.empty()) {
        throw std::runtime_error(std::format("Shader has no code; i.e. empty: {}\n", path.string()));
    }
    return buffer;
}


}
}
}
