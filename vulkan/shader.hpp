#pragma once


#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <vector>

#include "jms/vulkan/vulkan.hpp"


namespace jms {
namespace vulkan {


struct ShaderGroup {
    struct ShaderInfo {
        uint32_t subgroup_size{0};
        vk::ShaderCreateFlagsEXT flags{};
        vk::ShaderStageFlagBits stage{};
        vk::ShaderStageFlags next_stage{};
        vk::ShaderCodeTypeEXT code_type{vk::ShaderCodeTypeEXT::eSpirv};
        std::vector<uint32_t> code{};
        std::string entry_point_name{};
        std::vector<size_t> set_info_indices{};
        std::vector<size_t> push_constant_ranges_indices{};
        std::optional<vk::SpecializationInfo> specialization_info{};
    };

    std::vector<vk::VertexInputAttributeDescription2EXT> vertex_attribute_desc{};
    std::vector<vk::VertexInputBindingDescription2EXT> vertex_binding_desc{};
    std::vector<vk::PushConstantRange> push_constant_ranges{};
    std::vector<std::vector<vk::DescriptorSetLayoutBinding>> set_layout_bindings{};
    std::vector<ShaderInfo> shader_infos{};

    // May want to switch to C api to take advantage of failure handles for retry.  Wait to see raii failures first.
    std::vector<vk::raii::ShaderEXT> CreateShaders(
        vk::raii::Device& device,
        const std::vector<vk::raii::DescriptorSetLayout>& layouts,
        std::optional<vk::AllocationCallbacks*> vk_allocation_callbacks = std::nullopt)
    {
        std::vector<vk::PipelineShaderStageRequiredSubgroupSizeCreateInfo> pnexts{};
        pnexts.reserve(shader_infos.size());
        std::ranges::transform(shader_infos, std::back_inserter(pnexts), [](auto& info) {
            return vk::PipelineShaderStageRequiredSubgroupSizeCreateInfo{.requiredSubgroupSize=info.subgroup_size};
        });

        std::vector<std::vector<vk::DescriptorSetLayout>> info_vk_layouts{};
        info_vk_layouts.reserve(shader_infos.size());
        std::ranges::transform(shader_infos, std::back_inserter(info_vk_layouts),
            [&layouts](auto& info) {
                std::vector<vk::DescriptorSetLayout> vk_layouts{};
                vk_layouts.reserve(info.set_info_indices.size());
                for (size_t index : info.set_info_indices) { vk_layouts.push_back(*layouts.at(index)); }
                return vk_layouts;
            });

        std::vector<std::vector<vk::PushConstantRange>> info_pcrs{};
        info_pcrs.reserve(shader_infos.size());
        std::ranges::transform(shader_infos, std::back_inserter(info_pcrs),
            [&pcr=push_constant_ranges](auto& info) {
                std::vector<vk::PushConstantRange> pcrs{};
                pcrs.reserve(info.push_constant_ranges_indices.size());
                for (size_t index : info.push_constant_ranges_indices) { pcrs.push_back(pcr.at(index)); }
                return pcrs;
            });

        std::vector<vk::ShaderCreateInfoEXT> create_infos{};
        create_infos.reserve(shader_infos.size());
        std::ranges::transform(
            shader_infos,
            std::views::zip(pnexts, info_vk_layouts, info_pcrs),
            std::back_inserter(create_infos),
            [](auto& info, auto&& tup) -> vk::ShaderCreateInfoEXT {
                const auto& [pnext, vk_layouts, pcrs] = tup;
                return vk::ShaderCreateInfoEXT{
                    .pNext=(info.subgroup_size > 0 ? std::addressof(pnext) : nullptr),
                    .flags=info.flags,
                    .stage=info.stage,
                    .nextStage=info.next_stage,
                    .codeType=info.code_type,
                    .codeSize=(info.code.size() * sizeof(typename decltype(info.code)::value_type)),
                    .pCode=info.code.data(),
                    .pName=info.entry_point_name.c_str(),
                    .setLayoutCount=static_cast<uint32_t>(vk_layouts.size()),
                    .pSetLayouts=VectorAsPtr(vk_layouts),
                    .pushConstantRangeCount=static_cast<uint32_t>(pcrs.size()),
                    .pPushConstantRanges=VectorAsPtr(pcrs),
                    .pSpecializationInfo=(info.specialization_info.has_value() ?
                                          std::addressof(info.specialization_info.value()) : nullptr)
                };
            });

        return device.createShadersEXT(create_infos, vk_allocation_callbacks.value_or(nullptr));
    }

    // Consider an uber validation function with custom exceptions vs current approach with individual validations.
    void Validate(const std::vector<ShaderInfo>& shader_infos) {
        if (auto it = std::ranges::find_if(shader_infos, ShaderGroup::IsUnlinkable); it != shader_infos.end()) {
            throw std::runtime_error{"Shader info cannot be linked; invalid stage provided."};
        }
        if (auto it = std::ranges::find_if(shader_infos, ShaderGroup::IsBadFragment); it != shader_infos.end()) {
            throw std::runtime_error{"ShaderExtInfo has bad fragment related flags."};
        }
        if (auto it = std::ranges::find_if(shader_infos, ShaderGroup::IsBadSubgroupSize, &ShaderInfo::subgroup_size);
            it != shader_infos.end()) {
            throw std::runtime_error{
                "vk::PipelineShaderStageRequiredSubgroupSizeCreateInfo requires power of two size."};
        }
        // Do more of the rest of the required validation here ...
    }

private:
    static constexpr vk::ShaderStageFlags UNLINKABLE_STAGES = ~(vk::ShaderStageFlagBits::eAllGraphics |
                                                                vk::ShaderStageFlagBits::eTaskEXT |
                                                                vk::ShaderStageFlagBits::eMeshEXT);

    static constexpr auto IsUnlinkable = [](const ShaderInfo& v) -> bool {
        return static_cast<bool>(v.flags & vk::ShaderCreateFlagBitsEXT::eLinkStage) &&
               static_cast<bool>(v.stage & UNLINKABLE_STAGES);
    };

    static constexpr auto IsBadFragment = [](const ShaderInfo& v) -> bool {
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
};


std::vector<uint32_t> Load(const std::filesystem::path& path) {
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
