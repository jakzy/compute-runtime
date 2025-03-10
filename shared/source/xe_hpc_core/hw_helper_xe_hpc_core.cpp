/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/aub_mapper.h"
#include "shared/source/xe_hpc_core/hw_cmds.h"

using Family = NEO::XE_HPC_COREFamily;

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/flat_batch_buffer_helper_hw.inl"
#include "shared/source/helpers/hw_helper_base.inl"
#include "shared/source/helpers/hw_helper_dg2_and_later.inl"
#include "shared/source/helpers/hw_helper_tgllp_and_later.inl"
#include "shared/source/helpers/hw_helper_xehp_and_later.inl"

namespace NEO {

template <>
const AuxTranslationMode HwHelperHw<Family>::defaultAuxTranslationMode = AuxTranslationMode::Blit;

template <>
bool HwHelperHw<Family>::isCooperativeEngineSupported(const HardwareInfo &hwInfo) const {
    return (HwInfoConfig::get(hwInfo.platform.eProductFamily)->getSteppingFromHwRevId(hwInfo) >= REVISION_B);
}

template <>
const EngineInstancesContainer HwHelperHw<Family>::getGpgpuEngineInstances(const HardwareInfo &hwInfo) const {
    auto defaultEngine = getChosenEngineType(hwInfo);

    EngineInstancesContainer engines;

    if (hwInfo.featureTable.flags.ftrCCSNode) {
        for (uint32_t i = 0; i < hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled; i++) {
            engines.push_back({static_cast<aub_stream::EngineType>(i + aub_stream::ENGINE_CCS), EngineUsage::Regular});
            if (isCooperativeEngineSupported(hwInfo)) {
                engines.push_back({static_cast<aub_stream::EngineType>(i + aub_stream::ENGINE_CCS), EngineUsage::Cooperative});
            }
        }
    }

    if ((DebugManager.flags.NodeOrdinal.get() == static_cast<int32_t>(aub_stream::EngineType::ENGINE_CCCS)) ||
        hwInfo.featureTable.flags.ftrRcsNode) {
        engines.push_back({aub_stream::ENGINE_CCCS, EngineUsage::Regular});
    }

    engines.push_back({defaultEngine, EngineUsage::LowPriority});
    engines.push_back({defaultEngine, EngineUsage::Internal});

    if (hwInfo.capabilityTable.blitterOperationsSupported) {
        if (hwInfo.featureTable.ftrBcsInfo.test(0)) {
            engines.push_back({aub_stream::EngineType::ENGINE_BCS, EngineUsage::Regular});  // Main copy engine
            engines.push_back({aub_stream::EngineType::ENGINE_BCS, EngineUsage::Internal}); // internal usage
        }

        for (uint32_t i = 1; i < hwInfo.featureTable.ftrBcsInfo.size(); i++) {
            if (hwInfo.featureTable.ftrBcsInfo.test(i)) {
                auto engineType = static_cast<aub_stream::EngineType>((i - 1) + aub_stream::ENGINE_BCS1); // Link copy engine
                engines.push_back({engineType, EngineUsage::Regular});
            }
        }
    }

    return engines;
};

template <>
EngineGroupType HwHelperHw<Family>::getEngineGroupType(aub_stream::EngineType engineType, EngineUsage engineUsage, const HardwareInfo &hwInfo) const {
    if (engineType == aub_stream::ENGINE_CCCS) {
        return EngineGroupType::RenderCompute;
    }
    if (engineType >= aub_stream::ENGINE_CCS && engineType < (aub_stream::ENGINE_CCS + hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled)) {
        if (engineUsage == EngineUsage::Cooperative) {
            return EngineGroupType::CooperativeCompute;
        }
        return EngineGroupType::Compute;
    }
    if (engineType == aub_stream::ENGINE_BCS) {
        return EngineGroupType::Copy;
    }
    if (engineType >= aub_stream::ENGINE_BCS1 && engineType < aub_stream::ENGINE_BCS1 + hwInfo.featureTable.ftrBcsInfo.size() - 1) {
        return EngineGroupType::LinkedCopy;
    }
    UNRECOVERABLE_IF(true);
}

template <>
void HwHelperHw<Family>::adjustDefaultEngineType(HardwareInfo *pHwInfo) {
    if (!pHwInfo->featureTable.flags.ftrCCSNode) {
        pHwInfo->capabilityTable.defaultEngineType = aub_stream::EngineType::ENGINE_CCCS;
    }
}

template <>
bool HwHelperHw<Family>::tilingAllowed(bool isSharedContext, bool isImage1d, bool forceLinearStorage) {
    return false;
}

template <>
uint32_t HwHelperHw<Family>::getBarriersCountFromHasBarriers(uint32_t hasBarriers) {
    static constexpr uint32_t possibleBarriersCounts[] = {
        0u,  // 0
        1u,  // 1
        2u,  // 2
        4u,  // 3
        8u,  // 4
        16u, // 5
        24u, // 6
        32u, // 7
    };
    return possibleBarriersCounts[hasBarriers];
}

template <>
uint32_t HwHelperHw<Family>::calculateAvailableThreadCount(PRODUCT_FAMILY family, uint32_t grfCount, uint32_t euCount,
                                                           uint32_t threadsPerEu) {
    auto maxThreadsPerEuCount = 1024u / grfCount;
    return maxThreadsPerEuCount * euCount;
}

template <>
uint32_t HwHelperHw<Family>::getMetricsLibraryGenId() const {
    return static_cast<uint32_t>(MetricsLibraryApi::ClientGen::XeHPC);
}

template <>
uint32_t HwHelperHw<Family>::getMinimalSIMDSize() {
    return 16u;
}

template <>
bool HwHelperHw<Family>::isFenceAllocationRequired(const HardwareInfo &hwInfo) const {
    if ((DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.get() == 0) &&
        (DebugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.get() == 0) &&
        (DebugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.get() == 0)) {
        return false;
    }
    return true;
}

template <>
uint32_t HwHelperHw<Family>::getMocsIndex(const GmmHelper &gmmHelper, bool l3enabled, bool l1enabled) const {
    if (l3enabled) {
        return gmmHelper.getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER) >> 1;
    }
    return gmmHelper.getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED) >> 1;
}

template <>
const StackVec<size_t, 3> HwHelperHw<Family>::getDeviceSubGroupSizes() const {
    return {16, 32};
}

template <>
const StackVec<uint32_t, 6> HwHelperHw<Family>::getThreadsPerEUConfigs() const {
    return {4, 8};
}

template <>
uint32_t HwHelperHw<Family>::getMaxNumSamplers() const {
    return 0;
}

template <>
size_t HwHelperHw<Family>::getPaddingForISAAllocation() const {
    return 3584;
}

template <>
size_t MemorySynchronizationCommands<Family>::getSizeForSingleAdditionalSynchronization(const HardwareInfo &hwInfo) {
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    auto programGlobalFenceAsMiMemFenceCommandInCommandStream = hwInfoConfig.isGlobalFenceAsMiMemFenceCommandInCommandStreamRequired(hwInfo);
    if (DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.get() != -1) {
        programGlobalFenceAsMiMemFenceCommandInCommandStream = !!DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.get();
    }

    if (programGlobalFenceAsMiMemFenceCommandInCommandStream) {
        return sizeof(Family::MI_MEM_FENCE);
    } else {
        return EncodeSempahore<Family>::getSizeMiSemaphoreWait();
    }
}

template <>
void MemorySynchronizationCommands<Family>::setAdditionalSynchronization(void *&commandsBuffer, uint64_t gpuAddress, const HardwareInfo &hwInfo) {
    using MI_MEM_FENCE = typename Family::MI_MEM_FENCE;
    using MI_SEMAPHORE_WAIT = typename Family::MI_SEMAPHORE_WAIT;

    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    auto programGlobalFenceAsMiMemFenceCommandInCommandStream = hwInfoConfig.isGlobalFenceAsMiMemFenceCommandInCommandStreamRequired(hwInfo);
    if (DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.get() != -1) {
        programGlobalFenceAsMiMemFenceCommandInCommandStream = !!DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.get();
    }
    if (programGlobalFenceAsMiMemFenceCommandInCommandStream) {
        MI_MEM_FENCE miMemFence = Family::cmdInitMemFence;
        miMemFence.setFenceType(Family::MI_MEM_FENCE::FENCE_TYPE::FENCE_TYPE_RELEASE);
        *reinterpret_cast<MI_MEM_FENCE *>(commandsBuffer) = miMemFence;
        commandsBuffer = ptrOffset(commandsBuffer, sizeof(MI_MEM_FENCE));
    } else {
        EncodeSempahore<Family>::programMiSemaphoreWait(reinterpret_cast<MI_SEMAPHORE_WAIT *>(commandsBuffer),
                                                        gpuAddress,
                                                        EncodeSempahore<Family>::invalidHardwareTag,
                                                        MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD,
                                                        false);
        commandsBuffer = ptrOffset(commandsBuffer, EncodeSempahore<Family>::getSizeMiSemaphoreWait());
    }
}

template <>
bool MemorySynchronizationCommands<Family>::isPipeControlWArequired(const HardwareInfo &hwInfo) {
    if (DebugManager.flags.DisablePipeControlPrecedingPostSyncCommand.get() == 1) {
        return true;
    }
    return false;
}

template <>
size_t MemorySynchronizationCommands<Family>::getSizeForAdditonalSynchronization(const HardwareInfo &hwInfo) {
    return (DebugManager.flags.DisablePipeControlPrecedingPostSyncCommand.get() == 1 ? 2 : 1) * getSizeForSingleAdditionalSynchronization(hwInfo);
}

template <>
void HwHelperHw<Family>::setL1CachePolicy(bool useL1Cache, typename Family::RENDER_SURFACE_STATE *surfaceState, const HardwareInfo *hwInfo) {
    if (useL1Cache) {
        surfaceState->setL1CachePolicyL1CacheControl(Family::RENDER_SURFACE_STATE::L1_CACHE_POLICY_WB);
        if (DebugManager.flags.OverrideL1CacheControlInSurfaceStateForScratchSpace.get() != -1) {
            surfaceState->setL1CachePolicyL1CacheControl(static_cast<typename Family::RENDER_SURFACE_STATE::L1_CACHE_POLICY>(DebugManager.flags.OverrideL1CacheControlInSurfaceStateForScratchSpace.get()));
        }
    }
}

template <>
void HwHelperHw<Family>::setExtraAllocationData(AllocationData &allocationData, const AllocationProperties &properties, const HardwareInfo &hwInfo) const {
    if (properties.allocationType == AllocationType::TIMESTAMP_PACKET_TAG_BUFFER || properties.allocationType == AllocationType::COMMAND_BUFFER) {
        allocationData.flags.useSystemMemory = false;
    }

    allocationData.cacheRegion = properties.cacheRegion;

    if (allocationData.flags.requiresCpuAccess && !allocationData.flags.useSystemMemory &&
        (allocationData.storageInfo.getMemoryBanks() > 1)) {

        bool bdA0 = ((hwInfo.platform.usRevId & Family::pvcBaseDieRevMask) == Family::pvcBaseDieA0Masked);
        bool applyWa = ((DebugManager.flags.ForceTile0PlacementForTile1ResourcesWaActive.get() == 1) || bdA0);
        applyWa &= (DebugManager.flags.ForceTile0PlacementForTile1ResourcesWaActive.get() != 0);

        if (applyWa) {
            allocationData.storageInfo.memoryBanks = 1; // force Tile0
        }
    }
}

template <>
uint32_t HwHelperHw<Family>::getNumCacheRegions() const {
    if (DebugManager.flags.ClosEnabled.get() == 0) {
        return 0;
    }

    constexpr uint32_t numSharedCacheRegions = 1;
    constexpr uint32_t numReservedCacheRegions = 2;
    constexpr uint32_t numTotalCacheRegions = numSharedCacheRegions + numReservedCacheRegions;
    return numTotalCacheRegions;
}

template <>
std::string HwHelperHw<Family>::getExtensions() const {
    std::string extensions;

    extensions += "cl_intel_create_buffer_with_properties ";
    extensions += "cl_intel_dot_accumulate ";
    extensions += "cl_intel_subgroup_local_block_io ";
    extensions += "cl_intel_subgroup_matrix_multiply_accumulate_for_PVC ";
    extensions += "cl_khr_subgroup_named_barrier ";
    extensions += "cl_intel_subgroup_extended_block_read ";
    extensions += "cl_intel_subgroup_matrix_multiply_accumulate ";
    extensions += "cl_intel_subgroup_split_matrix_multiply_accumulate ";

    return extensions;
}

template <>
uint32_t HwHelperHw<Family>::alignSlmSize(uint32_t slmSize) {
    const uint32_t alignedSlmSizes[] = {
        0u,
        1u * KB,
        2u * KB,
        4u * KB,
        8u * KB,
        16u * KB,
        24u * KB,
        32u * KB,
        48u * KB,
        64u * KB,
        96u * KB,
        128u * KB,
    };

    for (auto &alignedSlmSize : alignedSlmSizes) {
        if (slmSize <= alignedSlmSize) {
            return alignedSlmSize;
        }
    }

    UNRECOVERABLE_IF(true);
    return 0;
}

template <>
uint32_t HwHelperHw<Family>::computeSlmValues(const HardwareInfo &hwInfo, uint32_t slmSize) {
    using SHARED_LOCAL_MEMORY_SIZE = typename Family::INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE;
    if (slmSize == 0u) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_ENCODES_0K;
    }

    UNRECOVERABLE_IF(slmSize > 128u * KB);

    if (slmSize > 96u * KB) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_ENCODES_128K;
    }
    if (slmSize > 64u * KB) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_ENCODES_96K;
    }
    if (slmSize > 48u * KB) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_ENCODES_64K;
    }
    if (slmSize > 32u * KB) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_ENCODES_48K;
    }
    if (slmSize > 24u * KB) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_ENCODES_32K;
    }
    if (slmSize > 16u * KB) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_ENCODES_24K;
    }
    if (slmSize > 8u * KB) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_ENCODES_16K;
    }
    if (slmSize > 4u * KB) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_ENCODES_8K;
    }
    if (slmSize > 2u * KB) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_ENCODES_4K;
    }
    if (slmSize > 1u * KB) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_ENCODES_2K;
    }
    return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_ENCODES_1K;
}

template <>
int32_t HwHelperHw<Family>::getDefaultThreadArbitrationPolicy() const {
    return ThreadArbitrationPolicy::RoundRobinAfterDependency;
}

template <>
bool HwHelperHw<Family>::isCopyOnlyEngineType(EngineGroupType type) const {
    return (EngineGroupType::Copy == type || EngineGroupType::LinkedCopy == type);
}

template <>
bool HwHelperHw<Family>::isSubDeviceEngineSupported(const HardwareInfo &hwInfo, const DeviceBitfield &deviceBitfield, aub_stream::EngineType engineType) const {
    constexpr uint64_t tile1Bitfield = 0b10;

    bool isBaseDieA0 = (hwInfo.platform.usRevId & Family::pvcBaseDieRevMask) == Family::pvcBaseDieA0Masked;
    bool affectedEngine = (deviceBitfield.to_ulong() == tile1Bitfield) &&
                          (aub_stream::ENGINE_BCS == engineType ||
                           aub_stream::ENGINE_BCS1 == engineType ||
                           aub_stream::ENGINE_BCS3 == engineType);

    if (affectedEngine) {
        if (DebugManager.flags.DoNotReportTile1BscWaActive.get() != -1) {
            return !DebugManager.flags.DoNotReportTile1BscWaActive.get();
        }

        return !isBaseDieA0;
    }

    return true;
}

template <>
inline bool HwHelperHw<Family>::isBlitCopyRequiredForLocalMemory(const HardwareInfo &hwInfo, const GraphicsAllocation &allocation) const {
    if (!allocation.isAllocatedInLocalMemoryPool()) {
        return false;
    }

    if (HwInfoConfig::get(hwInfo.platform.eProductFamily)->getLocalMemoryAccessMode(hwInfo) == LocalMemoryAccessMode::CpuAccessDisallowed) {
        // Regular L3 WA
        return true;
    }

    if (!allocation.isAllocationLockable()) {
        return true;
    }

    bool isBaseDieA0 = (hwInfo.platform.usRevId & Family::pvcBaseDieRevMask) == Family::pvcBaseDieA0Masked;
    bool isOtherTileThan0Accessed = allocation.storageInfo.memoryBanks.to_ulong() > 1u;
    if (isBaseDieA0 && isOtherTileThan0Accessed) {
        // Tile1 CPU access
        return true;
    }

    return false;
}

template <>
uint32_t HwHelperHw<Family>::getComputeUnitsUsedForScratch(const HardwareInfo *pHwInfo) const {
    if (DebugManager.flags.OverrideNumComputeUnitsForScratch.get() != -1) {
        return static_cast<uint32_t>(DebugManager.flags.OverrideNumComputeUnitsForScratch.get());
    }

    const auto &hwInfoConfig = *HwInfoConfig::get(pHwInfo->platform.eProductFamily);
    auto revId = pHwInfo->platform.usRevId & Family::pvcSteppingBits;
    uint32_t threadEuRatio = ((0x3 <= revId) && hwInfoConfig.isThreadEuRatio16ForScratchRequired(*pHwInfo)) ? 16 : 8;

    return pHwInfo->gtSystemInfo.MaxSubSlicesSupported * pHwInfo->gtSystemInfo.MaxEuPerSubSlice * threadEuRatio;
}

template <>
bool HwHelperHw<Family>::isRevisionSpecificBinaryBuiltinRequired() const {
    return true;
}

} // namespace NEO

#include "shared/source/helpers/hw_helper_pvc_and_later.inl"

namespace NEO {
template class HwHelperHw<Family>;
template class FlatBatchBufferHelperHw<Family>;
template struct MemorySynchronizationCommands<Family>;
template struct LriHelper<Family>;
} // namespace NEO
