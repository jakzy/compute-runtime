/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using HwInfoConfigTest = Test<DeviceFixture>;

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenIsAdjustProgrammableIdPreferredSlmSizeRequiredThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isAdjustProgrammableIdPreferredSlmSizeRequired(*defaultHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenIsComputeDispatchAllWalkerEnableInCfeStateRequiredThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isComputeDispatchAllWalkerEnableInCfeStateRequired(*defaultHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenIsComputeDispatchAllWalkerEnableInComputeWalkerRequiredThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isComputeDispatchAllWalkerEnableInComputeWalkerRequired(*defaultHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenIsGlobalFenceAsMiMemFenceCommandInCommandStreamRequiredThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isGlobalFenceAsMiMemFenceCommandInCommandStreamRequired(*defaultHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenIsGlobalFenceAsPostSyncOperationInComputeWalkerRequiredThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isGlobalFenceAsPostSyncOperationInComputeWalkerRequired(*defaultHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenIsSpecialPipelineSelectModeChangedThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isSpecialPipelineSelectModeChanged(*defaultHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenIsSystolicModeConfigurabledThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isSystolicModeConfigurable(*defaultHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenIsThreaEuRatio16ForScratchRequiredThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isThreadEuRatio16ForScratchRequired(*defaultHwInfo));
}
