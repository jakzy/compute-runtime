#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE_HPG_CORE)
  set(IGDRCL_SRCS_tests_xe_hpg_core_excludes
      ${CMAKE_CURRENT_SOURCE_DIR}/excludes_ocl_xe_hpg_core.cpp
  )
  set_property(GLOBAL APPEND PROPERTY IGDRCL_SRCS_tests_excludes ${IGDRCL_SRCS_tests_xe_hpg_core_excludes})

  set(IGDRCL_SRCS_tests_xe_hpg_core
      ${IGDRCL_SRCS_tests_xe_hpg_core_excludes}
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/cl_hw_helper_tests_xe_hpg_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_tests_xe_hpg_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_cmds_programming_xe_hpg_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/copy_engine_tests_xe_hpg_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_xe_hpg_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/image_tests_xe_hpg_core.cpp
  )

  get_property(NEO_CORE_TESTS_XE_HPG_CORE GLOBAL PROPERTY NEO_CORE_TESTS_XE_HPG_CORE)
  list(APPEND IGDRCL_SRCS_tests_xe_hpg_core ${NEO_CORE_TESTS_XE_HPG_CORE})

  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_xe_hpg_core})
  add_subdirectories()
endif()
