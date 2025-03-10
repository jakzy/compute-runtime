/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/os_interface/os_library.h"
#include "shared/source/sku_info/operations/sku_info_transfer.h"

#include <algorithm>

namespace NEO {

uint32_t GmmHelper::addressWidth = 48;

GmmClientContext *GmmHelper::getClientContext() const {
    return gmmClientContext.get();
}

const HardwareInfo *GmmHelper::getHardwareInfo() {
    return hwInfo;
}

uint32_t GmmHelper::getMOCS(uint32_t type) const {
    if (allResourcesUncached || (DebugManager.flags.ForceAllResourcesUncached.get() == true)) {
        type = GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED;
    }

    MEMORY_OBJECT_CONTROL_STATE mocs = gmmClientContext->cachePolicyGetMemoryObject(nullptr, static_cast<GMM_RESOURCE_USAGE_TYPE>(type));

    return static_cast<uint32_t>(mocs.DwordValue);
}

GmmHelper::GmmHelper(OSInterface *osInterface, const HardwareInfo *pHwInfo) : hwInfo(pHwInfo) {
    auto hwInfoAddressWidth = Math::log2(hwInfo->capabilityTable.gpuAddressSpace + 1);
    GmmHelper::addressWidth = std::max(hwInfoAddressWidth, static_cast<uint32_t>(48));

    gmmClientContext = GmmHelper::createGmmContextWrapperFunc(osInterface, const_cast<HardwareInfo *>(pHwInfo));
    UNRECOVERABLE_IF(!gmmClientContext);
}

bool GmmHelper::isValidCanonicalGpuAddress(uint64_t address) {
    auto decanonizedAddress = NEO::GmmHelper::decanonize(address);
    auto canonizedAddress = NEO::GmmHelper::canonize(decanonizedAddress);

    if (address == canonizedAddress) {
        return true;
    }
    return false;
}

GmmHelper::~GmmHelper() = default;

decltype(GmmHelper::createGmmContextWrapperFunc) GmmHelper::createGmmContextWrapperFunc = GmmClientContext::create<GmmClientContext>;
} // namespace NEO
