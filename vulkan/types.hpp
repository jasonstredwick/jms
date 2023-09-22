#pragma once


#if 0
#include <algorithm>
#include <bit>
#include <iterator>
#include <limits>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <new>
#include <memory_resource>
#include <span>
#include <vector>
#endif
#include <variant>

//#include "jms/vulkan/memory_resource.hpp"
#include "jms/vulkan/vulkan.hpp"


namespace jms {
namespace vulkan {


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





#if 0
constexpr const size_t Size_32KB = 32768;
constexpr const size_t Size_64KB = 65536;
constexpr const size_t Size_128KB = 131072;
constexpr const size_t Size_256KB = 262144;



/***
 * Vulkan storage buffer allocator.
 *
 * 2KB   = 2048   <- 2^11
 * 32KB  = 32768  <- 2^15
 * 64KB  = 65536  <- 2^16
 * 128KB = 131072 <- 2^17
 * 256KB = 262144 <- 2^18
 *
 * 32MB  = 33554432  <- 2^25
 * 256MB = 268435456 <- 2^28
 *
 * 1GB   = 1073741824  <- 2^30
 * 32GB  = 34359738368 <- 32 * 2^30 <- 2^35
 * 64GB  = 68719476736 <- 64 * 2^30 <- 2^36
 *
 * 25 + 11 = 36 ---> 32MB chunks over 2048 allocations
 * Allocation strategies-
 *   1. allocate next chunk when percentage filled exceeded: 50%, 75%, 100%
 *   2. allocate next chunk with size-
 *       a. relative to current chunk size: 100%, 200%
 *       b. fixed chunk size
 *
 * Storage provides allocations for a specific device.  Storage cannot change what device it is associated with.
 * Alternate strategies and migrations would be required to copy/move data from one Storage to another.  This
 * structure is not intended for multi-device support and device intermixing.
 *
 * *NOTE: Should be monitoring the maxMemoryAllocationCount feature.  How to ensure that allocations happen elsewhere?
 * *NOTE: Should be checking against the feature maxMemoryAllocationSize
 * *NOTE: there is a chance that allocation is larger than requested due to alignment manipulation behind the
 *        scenes.  I am not certain how to check or get that value.
 *
 *
 * storage_blocks = StorageBlocks{.device=device, .memory_type_index=memory_type_index, .chunk_size=65536};
 * SSBO<Vertex> ssbo_vertices{storage_blocks};
 * std::vector<Vertex> vertices{};
 * ssbo_vertices.copy(vertices);
 * ssbo_vertices.update(vertices, dirty_ranges); // what happens if includes ranges beyond last copy
 *
 * ssbo_vertices.bind(); // lets say it contains 2 64KB non-contiguous pages.  How to bind for shader?
 * struct Page {
 * };
 *
 *
 *
 * Types of buffers-
 * 1. Class instance - uniform buffer allocated from block for instance allocations
 * 2. Class instance array - same as above but as array; must verify size?  Or bump to storage buffer?
 * 3. Large buffers of the same type
 *     a. fixed size array
 * Dynamic allocations need to designate growth methods:
 * a. how much to grow
 * b. how to grow; i.e. extend with new block or allocate new chunk, copy data, and deallocate old memory
 * c. should growth allocate the entire (aligned) requested bytes?  Or should allocate entire growth split across
 *    chunks?
 * What should happen when starting with small amounts of data?  Then what should happen when it grows to a threshold?
 * At threshold should it migrate small allocations into unit chunk?  Then allocate all future with chunks? How can
 * one write a strategy that allows this flexibility?  Concern is overallocating for early data that either leads
 * to lots of waste when lots of types but smaller quanity. (maybe this doesn't matter and should be using GPU for
 * this small quantities?)  The other issue would be small allocations alternating within a chunk before it gets to
 * a good chunk size.  Should it then consolidate the small ones and then grow by chunk?  Or start out with the
 * proper good chunk size first?  How to easily allow this flexibility?
 *
 * When using sparse buffers to store data across chunks, do I need to worry about element proximity and caching?  If
 * sparse buffers are no performant could one use multiple render passes to render each chunk?  Again, is there a
 * potential issue with element locality and caching?
 *
 * Need to have multiple pipelines for different primitives.  How to reuse sparse data?
 */
// TODO: This will need to be broken up to separate physical device/device and memory_type_index in order to track device only restrictions.
// Alternatively, the use of a mutex and shared tracking data could be used.
// Max alignment size, max allocations per device, 
//     VkPhysicalDeviceLimits::maxMemoryAllocationCount
//     pAllocateInfo->allocationSize must be less than or equal to VkPhysicalDeviceMemoryProperties::memoryHeaps[memindex].size where memindex = VkPhysicalDeviceMemoryProperties::memoryTypes[pAllocateInfo->memoryTypeIndex].heapIndex as returned by vkGetPhysicalDeviceMemoryProperties for the VkPhysicalDevice that device was created from
/***
 * vk::raii::Device::allocateMemory is thread safe and has it's own minimal alignment it enforces.
*/



template <typename Allocator_t>
class Buffer {
    Allocator_t* allocator;
    BufferAllocation allocation;
    std::unique_ptr<int> x{nullptr};

public:
    Buffer(Allocator_t& a, BufferAllocation b) : allocator{std::addressof(a)}, allocation{b} {}
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    ~Buffer() noexcept { allocator->Deallocate(allocation); }
};










struct Camera {
    std::array<float, 16> projection{};
    std::array<float, 16> view{};
};
void Sample(vk::raii::Device& device, vk::AllocationCallbacks& vac, uint32_t memory_type_index) {
    DirectMemoryResource direct_memory_resource{device, vac, memory_type_index};
    AdhocPoolMemoryResource object_memory_resource{direct_memory_resource, Size_64KB};
    DirectAllocator object_allocator{object_memory_resource, device, vac, 128};

    Object<Camera> camera1{object_allocator, vk::BufferUsageFlagBits::eUniformBuffer};
    Array<Camera> cameras{3, object_allocator, vk::BufferUsageFlagBits::eUniformBuffer};
}
#endif


/***
 * Data has two uses
 * 1. intangible inter-pipeline variables
 * 2. border pipeline variables exposed to user
 *
 * The first use case interface is the creation and assignment of variables to descriptor sets.
 *
 * The second use case interface also creates and assigns variables to descriptor sets.  It also provides mechanisms
 * for users to read/modify data in these variables.  Mechanisms may include direct access through a pointer, direct copy/read,
 * and staging buffer that uses the first two methods.
*/



#if 0
// let type aware allocators apply alignment

// allocation strategy
struct Contiguous {};
struct NonContiguous {};

// backing method
struct UniformBuffer {};
struct UniformTexelBuffer {};
//struct StorageBuffer {};
struct StorageTexelBuffer {};
struct DynamicUniformBuffer {};
struct DynamicStorageBuffer {};

struct alignas(16) Vertex { float data[3]; };
void F() {
    Array<Vertex> vertices{32};
    constexpr auto x = alignof(Vertex);
    constexpr auto y = sizeof(Vertex);
}


template <typename T>
class Object {
    Allocator* allocator;
    BufferAllocation allocation{};
    vk::BufferUsageFlags usage_flags{};
    vk::BufferCreateFlags create_flags{};

public:
    using value_type = T;

    Object(Allocator& allocator,
           vk::BufferUsageFlags user_usage_flags,
           vk::BufferCreateFlags user_create_flags={},
           const std::vector<uint32_t>& sharing_queue_family_indices={})
    : allocator{std::addressof(allocator)}
    { Construct(sizeof(value_type), user_usage_flags, user_create_flags, sharing_queue_family_indices); }

    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;

    virtual ~Object() noexcept { allocator->DeallocateBuffer(allocation); }

    vk::DescriptorType GetDescriptorType(bool dynamic=false) const noexcept {
        if (usage_flags & vk::BufferUsageFlagBits::eUniformBuffer) {
            if (dynamic) { return vk::DescriptorType::eUniformBufferDynamic; }
            return vk::DescriptorType::eUniformBuffer;
        } else if (dynamic) { return vk::DescriptorType::eStorageBufferDynamic; }
        return vk::DescriptorType::eStorageBuffer;
    }

protected:
    void Construct(size_t size,
                   vk::BufferUsageFlags user_usage_flags,
                   vk::BufferCreateFlags user_create_flags,
                   const std::vector<uint32_t>& sharing_queue_family_indices) {
        Validate(user_usage_flags);
        usage_flags = user_usage_flags;
        create_flags = user_create_flags;
        allocation = allocator->AllocateBuffer(size, create_flags, usage_flags, sharing_queue_family_indices);
    }

    virtual vk::BufferUsageFlags ValidFlags() const noexcept {
        return vk::BufferUsageFlagBits::eUniformBuffer |
               vk::BufferUsageFlagBits::eStorageBuffer |
               vk::BufferUsageFlagBits::eTransferSrc |
               vk::BufferUsageFlagBits::eTransferDst;
    }

    void Validate(vk::BufferUsageFlags flags) {
        if (flags & ~ValidFlags()) { throw std::runtime_error{"Invalid buffer usage flags."}; }
        flags &= ~vk::BufferUsageFlagBits::eTransferDst;
        flags &= ~vk::BufferUsageFlagBits::eTransferSrc;
        if (!std::has_single_bit(flags)) { throw std::runtime_error{"Buffer can only have a single, non-transfer usage type."}; }
    }
};


template <typename T>
class Array : public Object<T> {
    size_t num_elements;

public:
    Array(size_t num_elements,
          Allocator& allocator,
          vk::BufferUsageFlags user_usage_flags,
          vk::BufferCreateFlags create_flags={},
          const std::vector<uint32_t>& sharing_queue_family_indices={})
    : allocator{std::addressof(allocator)}, num_elements{num_elements}
    { Construct(sizeof(value_type) * num_elements, user_usage_flags, user_create_flags, sharing_queue_family_indices); }

    Array(const Array&) = delete;
    Array& operator=(const Array&) = delete;

    ~Array() noexcept override = default;

protected:
    vk::BufferUsageFlags ValidFlags() const noexcept override {
        return vk::BufferUsageFlagBits::eUniformBuffer |
               vk::BufferUsageFlagBits::eStorageBuffer |
               vk::BufferUsageFlagBits::eUniformTexelBuffer |
               vk::BufferUsageFlagBits::eStorageTexelBuffer |
               vk::BufferUsageFlagBits::eIndexBuffer |
               vk::BufferUsageFlagBits::eVertexBuffer |
               vk::BufferUsageFlagBits::eIndirectBuffer |
               vk::BufferUsageFlagBits::eTransferSrc |
               vk::BufferUsageFlagBits::eTransferDst;
    }
};


// Vulkan Sparse Data
template <typename T>
class NonContiguousArray : Array<T> {
    NonContiguousArray(size_t num_elements,
                       Allocator& allocator,
                       vk::BufferUsageFlags user_usage_flags,
                       vk::BufferCreateFlags create_flags={},
                       const std::vector<uint32_t>& sharing_queue_family_indices={})
    : allocator{std::addressof(allocator)}, num_elements{num_elements}
    { Construct(sizeof(value_type) * num_elements, user_usage_flags, user_create_flags, sharing_queue_family_indices); }

    NonContiguousArray(const NonContiguousArray&) = delete;
    NonContiguousArray& operator=(const NonContiguousArray&) = delete;

    ~NonContiguousArray() noexcept override = default;
};





template <typename T>
struct Alias {};
/*
Sparse block size- VkMemoryRequirements::alignment

Storage is owner of all allocation/data
Allocator represents a sequence of allocations managed together.  Storage can provide many allocators.

Fixed allocators can allocate a single instance or array of instances.  It can also allow multiple instances and/or
arrays to be within the same units of backing memory.  What is allocated from this allocator is considered to fixed
size and non-changing over time.

DynamicAllocators can allocate units of backing memory over time allowing for containers with dynamic sizing. 
*/


/*
Hierarchical allocators.  StorageBlock -> Allocator then new Allocators can be created using other backing allocators.  The lowest level
allocators allocate actual memory.  Higher tier allocators get blocks of data from the lower tier allocator and allocate within that.
Do you have to do anything special due to the layering?
*/

#endif


}
}






