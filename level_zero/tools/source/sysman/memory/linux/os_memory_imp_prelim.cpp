/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/memory/linux/os_memory_imp_prelim.h"

#include "shared/source/os_interface/linux/system_info.h"

#include "level_zero/tools/source/sysman/sysman_const.h"

#include "drm/intel_hwconfig_types.h"
#include "igfxfmid.h"
#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

const std::string LinuxMemoryImp::deviceMemoryHealth("device_memory_health");

void LinuxMemoryImp::init() {
    if (isSubdevice) {
        const std::string baseDir = "gt/gt" + std::to_string(subdeviceId) + "/";
        physicalSizeFile = baseDir + "addr_range";
    }
}

LinuxMemoryImp::LinuxMemoryImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) : isSubdevice(onSubdevice), subdeviceId(subdeviceId) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pDrm = &pLinuxSysmanImp->getDrm();
    pDevice = pLinuxSysmanImp->getDeviceHandle();
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    pPmt = pLinuxSysmanImp->getPlatformMonitoringTechAccess(subdeviceId);
    init();
}

bool LinuxMemoryImp::isMemoryModuleSupported() {
    return pDevice->getDriverHandle()->getMemoryManager()->isLocalMemorySupported(pDevice->getRootDeviceIndex());
}

ze_result_t LinuxMemoryImp::getProperties(zes_mem_properties_t *pProperties) {
    pProperties->type = ZES_MEM_TYPE_DDR;
    pProperties->numChannels = -1;
    if (pDrm->querySystemInfo()) {
        auto memSystemInfo = pDrm->getSystemInfo();
        if (memSystemInfo != nullptr) {
            pProperties->numChannels = memSystemInfo->getMaxMemoryChannels();
            auto memType = memSystemInfo->getMemoryType();
            switch (memType) {
            case INTEL_HWCONFIG_MEMORY_TYPE_HBM2e:
            case INTEL_HWCONFIG_MEMORY_TYPE_HBM2:
                pProperties->type = ZES_MEM_TYPE_HBM;
                break;
            case INTEL_HWCONFIG_MEMORY_TYPE_LPDDR4:
                pProperties->type = ZES_MEM_TYPE_LPDDR4;
                break;
            case INTEL_HWCONFIG_MEMORY_TYPE_LPDDR5:
                pProperties->type = ZES_MEM_TYPE_LPDDR5;
                break;
            default:
                pProperties->type = ZES_MEM_TYPE_DDR;
                break;
            }
        }
    }
    pProperties->location = ZES_MEM_LOC_DEVICE;
    pProperties->onSubdevice = isSubdevice;
    pProperties->subdeviceId = subdeviceId;
    pProperties->busWidth = memoryBusWidth; // Hardcode

    pProperties->physicalSize = 0;
    if (isSubdevice) {
        std::string memval;
        ze_result_t result = pSysfsAccess->read(physicalSizeFile, memval);
        uint64_t intval = strtoull(memval.c_str(), nullptr, 16);
        if (ZE_RESULT_SUCCESS != result) {
            pProperties->physicalSize = 0u;
        } else {
            pProperties->physicalSize = intval;
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxMemoryImp::getVFIDString(std::string &vfID) {
    uint32_t vf0VfIdVal = 0;
    std::string key = "VF0_VFID";
    auto result = pPmt->readValue(key, vf0VfIdVal);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    uint32_t vf1VfIdVal = 0;
    key = "VF1_VFID";
    result = pPmt->readValue(key, vf1VfIdVal);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    // At any point of time only one VF(virtual function) could be active and thus would
    // read greater than zero val. If both VF0 and VF1 are reading 0 or both are reading
    // greater than 0, then we would be confused in taking the decision of correct VF.
    // Lets assume and report this as a error condition
    if (((vf0VfIdVal == 0) && (vf1VfIdVal == 0)) ||
        ((vf0VfIdVal > 0) && (vf1VfIdVal > 0))) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (vf0VfIdVal > 0) {
        vfID = "VF0";
    }
    if (vf1VfIdVal > 0) {
        vfID = "VF1";
    }
    return result;
}

void LinuxMemoryImp::getHbmFrequency(PRODUCT_FAMILY productFamily, unsigned short stepping, uint64_t &hbmFrequency) {
    hbmFrequency = 0;
    if (productFamily == IGFX_XE_HP_SDV) {
        // For IGFX_XE_HP HBM frequency would be 2.8 GT/s = 2.8 * 1000 * 1000 * 1000 T/s = 2800000000 T/s
        hbmFrequency = 2.8 * gigaUnitTransferToUnitTransfer;
    } else if (productFamily == IGFX_PVC) {
        if (stepping == REVISION_B) {
            const std::string baseDir = "gt/gt" + std::to_string(subdeviceId) + "/";
            // Calculating bandwidth based on HBM max frequency
            const std::string hbmRP0FreqFile = baseDir + "hbm_RP0_freq_mhz";
            uint64_t hbmFreqValue = 0;
            ze_result_t result = pSysfsAccess->read(hbmRP0FreqFile, hbmFreqValue);
            if (ZE_RESULT_SUCCESS == result) {
                hbmFrequency = hbmFreqValue * 1000 * 1000; // Converting MHz value to Hz
                return;
            }
        } else if (stepping == REVISION_A0) {
            // For IGFX_PVC REV A0 HBM frequency would be 3.2 GT/s = 3.2 * 1000 * 1000 * 1000 T/s = 3200000000 T/s
            hbmFrequency = 3.2 * gigaUnitTransferToUnitTransfer;
        }
    }
}

ze_result_t LinuxMemoryImp::getBandwidth(zes_mem_bandwidth_t *pBandwidth) {
    if (pPmt == nullptr) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    std::string vfId = "";
    auto result = getVFIDString(vfId);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    uint32_t numHbmModules = 0u;
    auto &hwInfo = pDevice->getNEODevice()->getHardwareInfo();
    auto productFamily = hwInfo.platform.eProductFamily;
    auto stepping = NEO::HwInfoConfig::get(productFamily)->getSteppingFromHwRevId(hwInfo);
    if (productFamily == IGFX_XE_HP_SDV) {
        numHbmModules = 2u;
    } else if (productFamily == IGFX_PVC) {
        numHbmModules = 4u;
    }

    pBandwidth->readCounter = 0;
    pBandwidth->writeCounter = 0;
    pBandwidth->timestamp = 0;
    pBandwidth->maxBandwidth = 0;
    for (auto hbmModuleIndex = 0u; hbmModuleIndex < numHbmModules; hbmModuleIndex++) {
        uint32_t counterValue = 0;
        // To read counters from VFID 0 and HBM module 0, key would be: VF0_HBM0_READ
        std::string readCounterKey = vfId + "_HBM" + std::to_string(hbmModuleIndex) + "_READ";
        result = pPmt->readValue(readCounterKey, counterValue);
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }
        pBandwidth->readCounter += counterValue;

        counterValue = 0;
        // To write counters to VFID 0 and HBM module 0, key would be: VF0_HBM0_Write
        std::string writeCounterKey = vfId + "_HBM" + std::to_string(hbmModuleIndex) + "_WRITE";
        result = pPmt->readValue(writeCounterKey, counterValue);
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }
        pBandwidth->writeCounter += counterValue;
    }

    uint32_t timeStampL = 0;
    std::string timeStamp = vfId + "_TIMESTAMP_L";
    result = pPmt->readValue(timeStamp, timeStampL);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    uint32_t timeStampH = 0;
    timeStamp = vfId + "_TIMESTAMP_H";
    result = pPmt->readValue(timeStamp, timeStampH);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    pBandwidth->timestamp |= timeStampH;
    pBandwidth->timestamp = (pBandwidth->timestamp << 32) | timeStampL;

    uint64_t hbmFrequency = 0;
    getHbmFrequency(productFamily, stepping, hbmFrequency);

    pBandwidth->maxBandwidth = memoryBusWidth * hbmFrequency * numHbmModules;
    pBandwidth->maxBandwidth /= 8; // Divide by 8 to get bandwidth in bytes/sec

    return result;
}

ze_result_t LinuxMemoryImp::getState(zes_mem_state_t *pState) {
    std::string memHealth;
    ze_result_t result = pSysfsAccess->read(deviceMemoryHealth, memHealth);
    if (ZE_RESULT_SUCCESS != result) {
        pState->health = ZES_MEM_HEALTH_UNKNOWN;
    } else {
        auto health = i915ToL0MemHealth.find(memHealth);
        if (health == i915ToL0MemHealth.end()) {
            pState->health = ZES_MEM_HEALTH_UNKNOWN;
        } else {
            pState->health = i915ToL0MemHealth.at(memHealth);
        }
    }

    std::vector<NEO::MemoryRegion> deviceRegions;
    auto memRegions = pDrm->getMemoryRegions();
    if (memRegions.empty()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    auto regions = pDrm->getIoctlHelper()->translateToMemoryRegions(memRegions);
    for (auto region : regions) {
        if (region.region.memoryClass == I915_MEMORY_CLASS_DEVICE) {
            deviceRegions.push_back(region);
        }
    }

    UNRECOVERABLE_IF(deviceRegions.size() <= subdeviceId);

    pState->free = deviceRegions[subdeviceId].unallocatedSize;
    pState->size = deviceRegions[subdeviceId].probedSize;

    return ZE_RESULT_SUCCESS;
}

OsMemory *OsMemory::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    LinuxMemoryImp *pLinuxMemoryImp = new LinuxMemoryImp(pOsSysman, onSubdevice, subdeviceId);
    return static_cast<OsMemory *>(pLinuxMemoryImp);
}

} // namespace L0
