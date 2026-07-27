# AForc subproject-default consumer
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.16)

project(AForcSubprojectConsumer LANGUAGES C)

if(NOT DEFINED AFORC_SOURCE_DIR)
    message(FATAL_ERROR "AFORC_SOURCE_DIR is required")
endif()
if(DEFINED BUILD_TESTING)
    message(FATAL_ERROR "Host BUILD_TESTING must begin undefined")
endif()

add_subdirectory("${AFORC_SOURCE_DIR}" aforc)

if(DEFINED BUILD_TESTING)
    message(FATAL_ERROR "AForc changed the host BUILD_TESTING variable")
endif()
if(AFORC_BUILD_EXAMPLES)
    message(FATAL_ERROR "AForc examples defaulted on as a subproject")
endif()
if(AFORC_BUILD_TESTS)
    message(FATAL_ERROR "AForc tests defaulted on as a subproject")
endif()
foreach(unexpected_target IN ITEMS
        aforc_roguelike
        aforc_surf_man
        aforc_core_lifecycle_test
)
    if(TARGET "${unexpected_target}")
        message(FATAL_ERROR "Unexpected subproject target: ${unexpected_target}")
    endif()
endforeach()
if(NOT TARGET aforc::aforc)
    message(FATAL_ERROR "AForc library target is unavailable")
endif()
