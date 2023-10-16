#pragma once


#include <algorithm>
#include <cassert>
#include <ranges>
#include <set>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

#include "jms/vulkan/vulkan.hpp"


namespace jms {
namespace vulkan {


template <typename... Ts>
auto ChainPNext(std::vector<std::variant<Ts...>> v) {
    using Tuple_t = std::tuple<std::variant<Ts...>&, std::variant<Ts...>&>;

    // Validate
    std::set<vk::StructureType> kinds{};
    std::ranges::transform(v, std::inserter(kinds, kinds.begin()), [](auto& i) {
        return std::visit([](auto&& x) { return x.sType; }, i); });
    if (kinds.size() != v.size()) {
        throw std::runtime_error{"DeviceCreateInfo does not allow duplicate pNext structures."};
    }

    std::ranges::for_each(
        std::views::zip(v, v | std::views::drop(1)), [](Tuple_t&& tup) {
            void* ptr = &(std::get<1>(tup));
            std::visit([&ptr](auto&& x) { x.pNext = ptr; }, std::get<0>(tup));
        });
    return v;
}


template <typename T>
requires requires(T) { T::structureType; } && std::is_scoped_enum_v<decltype(T::structureType)>
T* Convert(void* next_ptr) {
    if (!next_ptr) { return nullptr; }
    vk::BaseOutStructure* base_ptr = reinterpret_cast<vk::BaseOutStructure*>(next_ptr);
    if (base_ptr->sType == T::structureType) { return reinterpret_cast<T*>(next_ptr); }
    return nullptr;
}


vk::StructureType ExtractSType(void* next_ptr) {
    assert(next_ptr);
    return reinterpret_cast<vk::BaseOutStructure*>(next_ptr)->sType;
}


using DeviceCreateInfo2Variant = std::variant<
    vk::DeviceDeviceMemoryReportCreateInfoEXT,
    vk::DeviceDiagnosticsConfigCreateInfoNV,
    vk::DeviceGroupDeviceCreateInfo,
    vk::DeviceMemoryOverallocationCreateInfoAMD,
    vk::DevicePrivateDataCreateInfo,
    vk::PhysicalDevice16BitStorageFeatures,
    vk::PhysicalDevice4444FormatsFeaturesEXT,
    vk::PhysicalDevice8BitStorageFeatures,
    vk::PhysicalDeviceASTCDecodeFeaturesEXT,
    vk::PhysicalDeviceAccelerationStructureFeaturesKHR,
    vk::PhysicalDeviceAddressBindingReportFeaturesEXT,
    vk::PhysicalDeviceAmigoProfilingFeaturesSEC,
    vk::PhysicalDeviceAttachmentFeedbackLoopDynamicStateFeaturesEXT,
    vk::PhysicalDeviceAttachmentFeedbackLoopLayoutFeaturesEXT,
    vk::PhysicalDeviceBlendOperationAdvancedFeaturesEXT,
    vk::PhysicalDeviceBorderColorSwizzleFeaturesEXT,
    vk::PhysicalDeviceBufferDeviceAddressFeatures,
    vk::PhysicalDeviceBufferDeviceAddressFeaturesEXT,
    vk::PhysicalDeviceClusterCullingShaderFeaturesHUAWEI,
    vk::PhysicalDeviceCoherentMemoryFeaturesAMD,
    vk::PhysicalDeviceColorWriteEnableFeaturesEXT,
    vk::PhysicalDeviceComputeShaderDerivativesFeaturesNV,
    vk::PhysicalDeviceConditionalRenderingFeaturesEXT,
    //vk::PhysicalDeviceCooperativeMatrixFeaturesKHR,
    vk::PhysicalDeviceCooperativeMatrixFeaturesNV,
    vk::PhysicalDeviceCopyMemoryIndirectFeaturesNV,
    vk::PhysicalDeviceCornerSampledImageFeaturesNV,
    vk::PhysicalDeviceCoverageReductionModeFeaturesNV,
    //vk::PhysicalDeviceCubicClampFeaturesQCOM,
    //vk::PhysicalDeviceCubicWeightsFeaturesQCOM,
    vk::PhysicalDeviceCustomBorderColorFeaturesEXT,
    vk::PhysicalDeviceDedicatedAllocationImageAliasingFeaturesNV,
    //vk::PhysicalDeviceDepthBiasControlFeaturesEXT,
    vk::PhysicalDeviceDepthClampZeroOneFeaturesEXT,
    vk::PhysicalDeviceDepthClipControlFeaturesEXT,
    vk::PhysicalDeviceDepthClipEnableFeaturesEXT,
    vk::PhysicalDeviceDescriptorBufferFeaturesEXT,
    vk::PhysicalDeviceDescriptorIndexingFeatures,
    //vk::PhysicalDeviceDescriptorPoolOverallocationFeaturesNV,
    vk::PhysicalDeviceDescriptorSetHostMappingFeaturesVALVE,
    //vk::PhysicalDeviceDeviceGeneratedCommandsComputeFeaturesNV,
    vk::PhysicalDeviceDeviceGeneratedCommandsFeaturesNV,
    vk::PhysicalDeviceDeviceMemoryReportFeaturesEXT,
    vk::PhysicalDeviceDiagnosticsConfigFeaturesNV,
    //vk::PhysicalDeviceDisplacementMicromapFeaturesNV,
    vk::PhysicalDeviceDynamicRenderingFeatures,
    //vk::PhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT,
    vk::PhysicalDeviceExclusiveScissorFeaturesNV,
    vk::PhysicalDeviceExtendedDynamicState2FeaturesEXT,
    vk::PhysicalDeviceExtendedDynamicState3FeaturesEXT,
    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT,
    vk::PhysicalDeviceExternalMemoryRDMAFeaturesNV,
    //vk::PhysicalDeviceExternalMemoryScreenBufferFeaturesQNX,
    vk::PhysicalDeviceFaultFeaturesEXT,
    vk::PhysicalDeviceFeatures2,
    vk::PhysicalDeviceFragmentDensityMap2FeaturesEXT,
    vk::PhysicalDeviceFragmentDensityMapFeaturesEXT,
    vk::PhysicalDeviceFragmentDensityMapOffsetFeaturesQCOM,
    vk::PhysicalDeviceFragmentShaderBarycentricFeaturesKHR,
    vk::PhysicalDeviceFragmentShaderInterlockFeaturesEXT,
    vk::PhysicalDeviceFragmentShadingRateEnumsFeaturesNV,
    vk::PhysicalDeviceFragmentShadingRateFeaturesKHR,
    //vk::PhysicalDeviceFrameBoundaryFeaturesEXT,
    vk::PhysicalDeviceGlobalPriorityQueryFeaturesKHR,
    vk::PhysicalDeviceGraphicsPipelineLibraryFeaturesEXT,
    //vk::PhysicalDeviceHostImageCopyFeaturesEXT,
    vk::PhysicalDeviceHostQueryResetFeatures,
    vk::PhysicalDeviceImage2DViewOf3DFeaturesEXT,
    vk::PhysicalDeviceImageCompressionControlFeaturesEXT,
    vk::PhysicalDeviceImageCompressionControlSwapchainFeaturesEXT,
    //vk::PhysicalDeviceImageProcessing2FeaturesQCOM,
    vk::PhysicalDeviceImageProcessingFeaturesQCOM,
    vk::PhysicalDeviceImageRobustnessFeatures,
    vk::PhysicalDeviceImageSlicedViewOf3DFeaturesEXT,
    vk::PhysicalDeviceImageViewMinLodFeaturesEXT,
    vk::PhysicalDeviceImagelessFramebufferFeatures,
    vk::PhysicalDeviceIndexTypeUint8FeaturesEXT,
    vk::PhysicalDeviceInheritedViewportScissorFeaturesNV,
    vk::PhysicalDeviceInlineUniformBlockFeatures,
    vk::PhysicalDeviceInvocationMaskFeaturesHUAWEI,
    vk::PhysicalDeviceLegacyDitheringFeaturesEXT,
    vk::PhysicalDeviceLineRasterizationFeaturesEXT,
    vk::PhysicalDeviceLinearColorAttachmentFeaturesNV,
    vk::PhysicalDeviceMaintenance4Features,
    //vk::PhysicalDeviceMaintenance5FeaturesKHR,
    vk::PhysicalDeviceMemoryDecompressionFeaturesNV,
    vk::PhysicalDeviceMemoryPriorityFeaturesEXT,
    vk::PhysicalDeviceMeshShaderFeaturesEXT,
    vk::PhysicalDeviceMeshShaderFeaturesNV,
    vk::PhysicalDeviceMultiDrawFeaturesEXT,
    vk::PhysicalDeviceMultisampledRenderToSingleSampledFeaturesEXT,
    vk::PhysicalDeviceMultiviewFeatures,
    vk::PhysicalDeviceMultiviewPerViewRenderAreasFeaturesQCOM,
    vk::PhysicalDeviceMultiviewPerViewViewportsFeaturesQCOM,
    vk::PhysicalDeviceMutableDescriptorTypeFeaturesEXT,
    vk::PhysicalDeviceNonSeamlessCubeMapFeaturesEXT,
    vk::PhysicalDeviceOpacityMicromapFeaturesEXT,
    vk::PhysicalDeviceOpticalFlowFeaturesNV,
    vk::PhysicalDevicePageableDeviceLocalMemoryFeaturesEXT,
    vk::PhysicalDevicePerformanceQueryFeaturesKHR,
    vk::PhysicalDevicePipelineCreationCacheControlFeatures,
    vk::PhysicalDevicePipelineExecutablePropertiesFeaturesKHR,
    vk::PhysicalDevicePipelineLibraryGroupHandlesFeaturesEXT,
    vk::PhysicalDevicePipelinePropertiesFeaturesEXT,
    vk::PhysicalDevicePipelineProtectedAccessFeaturesEXT,
    vk::PhysicalDevicePipelineRobustnessFeaturesEXT,
    //vk::PhysicalDevicePortabilitySubsetFeaturesKHR,
    vk::PhysicalDevicePresentBarrierFeaturesNV,
    vk::PhysicalDevicePresentIdFeaturesKHR,
    vk::PhysicalDevicePresentWaitFeaturesKHR,
    vk::PhysicalDevicePrimitiveTopologyListRestartFeaturesEXT,
    vk::PhysicalDevicePrimitivesGeneratedQueryFeaturesEXT,
    vk::PhysicalDevicePrivateDataFeatures,
    vk::PhysicalDeviceProtectedMemoryFeatures,
    vk::PhysicalDeviceProvokingVertexFeaturesEXT,
    vk::PhysicalDeviceRGBA10X6FormatsFeaturesEXT,
    vk::PhysicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT,
    vk::PhysicalDeviceRayQueryFeaturesKHR,
    vk::PhysicalDeviceRayTracingInvocationReorderFeaturesNV,
    vk::PhysicalDeviceRayTracingMaintenance1FeaturesKHR,
    vk::PhysicalDeviceRayTracingMotionBlurFeaturesNV,
    vk::PhysicalDeviceRayTracingPipelineFeaturesKHR,
    vk::PhysicalDeviceRayTracingPositionFetchFeaturesKHR,
    vk::PhysicalDeviceRepresentativeFragmentTestFeaturesNV,
    vk::PhysicalDeviceRobustness2FeaturesEXT,
    vk::PhysicalDeviceSamplerYcbcrConversionFeatures,
    vk::PhysicalDeviceScalarBlockLayoutFeatures,
    vk::PhysicalDeviceSeparateDepthStencilLayoutsFeatures,
    vk::PhysicalDeviceShaderAtomicFloat2FeaturesEXT,
    vk::PhysicalDeviceShaderAtomicFloatFeaturesEXT,
    vk::PhysicalDeviceShaderAtomicInt64Features,
    vk::PhysicalDeviceShaderClockFeaturesKHR,
    vk::PhysicalDeviceShaderCoreBuiltinsFeaturesARM,
    vk::PhysicalDeviceShaderDemoteToHelperInvocationFeatures,
    vk::PhysicalDeviceShaderDrawParametersFeatures,
    vk::PhysicalDeviceShaderEarlyAndLateFragmentTestsFeaturesAMD,
    //vk::PhysicalDeviceShaderEnqueueFeaturesAMDX,
    vk::PhysicalDeviceShaderFloat16Int8Features,
    vk::PhysicalDeviceShaderImageAtomicInt64FeaturesEXT,
    vk::PhysicalDeviceShaderImageFootprintFeaturesNV,
    vk::PhysicalDeviceShaderIntegerDotProductFeatures,
    vk::PhysicalDeviceShaderIntegerFunctions2FeaturesINTEL,
    vk::PhysicalDeviceShaderModuleIdentifierFeaturesEXT,
    vk::PhysicalDeviceShaderObjectFeaturesEXT,
    vk::PhysicalDeviceShaderSMBuiltinsFeaturesNV,
    vk::PhysicalDeviceShaderSubgroupExtendedTypesFeatures,
    vk::PhysicalDeviceShaderSubgroupUniformControlFlowFeaturesKHR,
    vk::PhysicalDeviceShaderTerminateInvocationFeatures,
    vk::PhysicalDeviceShaderTileImageFeaturesEXT,
    vk::PhysicalDeviceShadingRateImageFeaturesNV,
    vk::PhysicalDeviceSubgroupSizeControlFeatures,
    vk::PhysicalDeviceSubpassMergeFeedbackFeaturesEXT,
    vk::PhysicalDeviceSubpassShadingFeaturesHUAWEI,
    vk::PhysicalDeviceSwapchainMaintenance1FeaturesEXT,
    vk::PhysicalDeviceSynchronization2Features,
    vk::PhysicalDeviceTexelBufferAlignmentFeaturesEXT,
    vk::PhysicalDeviceTextureCompressionASTCHDRFeatures,
    vk::PhysicalDeviceTilePropertiesFeaturesQCOM,
    vk::PhysicalDeviceTimelineSemaphoreFeatures,
    vk::PhysicalDeviceTransformFeedbackFeaturesEXT,
    vk::PhysicalDeviceUniformBufferStandardLayoutFeatures,
    vk::PhysicalDeviceVariablePointersFeatures,
    vk::PhysicalDeviceVertexAttributeDivisorFeaturesEXT,
    vk::PhysicalDeviceVertexInputDynamicStateFeaturesEXT,
    vk::PhysicalDeviceVulkanMemoryModelFeatures,
    vk::PhysicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR,
    vk::PhysicalDeviceYcbcr2Plane444FormatsFeaturesEXT,
    //vk::PhysicalDeviceYcbcrDegammaFeaturesQCOM,
    vk::PhysicalDeviceYcbcrImageArraysFeaturesEXT,
    vk::PhysicalDeviceZeroInitializeWorkgroupMemoryFeatures
>;


}
}