#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_TESTS_TOOLS_SYSMAN_ENGINE_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}test_zes_engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}mock_engine.h
)

if((NEO_ENABLE_i915_PRELIM_DETECTION) AND ("${BRANCH_TYPE}" STREQUAL ""))
  list(REMOVE_ITEM L0_TESTS_TOOLS_SYSMAN_ENGINE_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_engine.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_engine.h
  )
endif()

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_TESTS_TOOLS_SYSMAN_ENGINE_LINUX}
  )
endif()
