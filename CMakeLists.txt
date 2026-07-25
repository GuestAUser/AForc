# AForc
# Author: GuestAUser
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.16)

project(
    AForc
    VERSION 0.1.0
    DESCRIPTION "An ASCII C game engine for 2D planes"
    LANGUAGES C
)

include(CTest)
include(CheckCCompilerFlag)
include(CheckCSourceCompiles)
include(CheckPIESupported)
include(CMakePushCheckState)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

option(AFORC_BUILD_EXAMPLES "Build AForc example programs" ON)
option(AFORC_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(AFORC_ENABLE_SANITIZERS "Enable address and undefined-behavior sanitizers" OFF)
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(AFORC_HARDENING_DEFAULT ON)
else()
    set(AFORC_HARDENING_DEFAULT OFF)
endif()
option(
    AFORC_ENABLE_HARDENING
    "Enable supported hardening for AForc-owned targets"
    "${AFORC_HARDENING_DEFAULT}"
)
unset(AFORC_HARDENING_DEFAULT)

if(NOT UNIX)
    message(FATAL_ERROR "AForc currently requires a POSIX terminal backend")
endif()

set(AFORC_HARDENING_ACTIVE OFF)
if(AFORC_ENABLE_HARDENING)
    if(AFORC_ENABLE_SANITIZERS)
        message(STATUS "AForc hardening is disabled for sanitizer builds")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux"
           AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        check_c_compiler_flag(
            "-fstack-protector-strong"
            AFORC_HAS_STACK_PROTECTOR_STRONG
        )
        check_c_compiler_flag(
            "-fvisibility=hidden"
            AFORC_HAS_HIDDEN_VISIBILITY
        )
        check_c_compiler_flag(
            "-Wl,-z,relro,-z,now"
            AFORC_HAS_FULL_RELRO
        )
        cmake_push_check_state(RESET)
        set(CMAKE_REQUIRED_FLAGS
            "-std=c17 -O2 -Werror -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3"
        )
        check_c_source_compiles([=[
#include <features.h>
_Static_assert(__USE_FORTIFY_LEVEL >= 3, "fortify level 3 required");
int main(void)
{
    return 0;
}
]=] AFORC_HAS_FORTIFY_SOURCE_3)
        cmake_pop_check_state()
        check_pie_supported(
            OUTPUT_VARIABLE AFORC_PIE_CHECK_OUTPUT
            LANGUAGES C
        )

        if(AFORC_HAS_STACK_PROTECTOR_STRONG
           AND AFORC_HAS_HIDDEN_VISIBILITY
           AND AFORC_HAS_FULL_RELRO
           AND AFORC_HAS_FORTIFY_SOURCE_3
           AND CMAKE_C_LINK_PIE_SUPPORTED)
            set(AFORC_HARDENING_ACTIVE ON)
            message(STATUS "AForc hardening is enabled for project targets")
        else()
            message(WARNING
                "AForc hardening was requested but the complete flag set is "
                "unsupported; building without hardening"
            )
        endif()
    else()
        message(STATUS
            "AForc hardening is unavailable for this platform/compiler"
        )
    endif()
endif()

function(aforc_enable_target_hardening target)
    if(NOT AFORC_HARDENING_ACTIVE)
        return()
    endif()

    target_compile_options(${target} PRIVATE
        -fstack-protector-strong
        "$<$<CONFIG:Release>:-U_FORTIFY_SOURCE>"
        "$<$<CONFIG:Release>:-D_FORTIFY_SOURCE=3>"
    )

    get_target_property(target_type ${target} TYPE)
    if(target_type STREQUAL "OBJECT_LIBRARY")
        set_target_properties(${target} PROPERTIES
            POSITION_INDEPENDENT_CODE ON
        )
    elseif(NOT target_type STREQUAL "STATIC_LIBRARY")
        target_link_options(${target} PRIVATE
            "LINKER:-z,relro"
            "LINKER:-z,now"
        )
    endif()
    if(target_type STREQUAL "EXECUTABLE")
        set_target_properties(${target} PROPERTIES
            POSITION_INDEPENDENT_CODE ON
        )
    endif()
endfunction()

function(aforc_enable_target_warnings target)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        return()
    endif()

    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Wstrict-prototypes
        -Wmissing-prototypes
    )
    if(AFORC_WARNINGS_AS_ERRORS)
        target_compile_options(${target} PRIVATE -Werror)
    endif()
endfunction()

function(aforc_enable_target_sanitizers target)
    if(NOT AFORC_ENABLE_SANITIZERS OR
       NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        return()
    endif()

    target_compile_options(${target} PRIVATE
        -fno-omit-frame-pointer
        -fsanitize=address,undefined
    )
    get_target_property(target_type ${target} TYPE)
    if(NOT target_type STREQUAL "OBJECT_LIBRARY" AND
       NOT target_type STREQUAL "STATIC_LIBRARY")
        target_link_options(${target} PRIVATE
            -fsanitize=address,undefined
        )
    endif()
endfunction()

function(aforc_configure_owned_target target)
    set_target_properties(${target} PROPERTIES
        C_EXTENSIONS OFF
    )
    target_compile_features(${target} PRIVATE c_std_17)
    aforc_enable_target_warnings(${target})
    aforc_enable_target_sanitizers(${target})
    aforc_enable_target_hardening(${target})
endfunction()

add_library(aforc
    src/assets/assets.c
    src/assets/config_parser.c
    src/assets/path_io.c
    src/assets/pcg.c
    src/assets/save_container.c
    src/core/common.c
    src/core/engine.c
    src/core/engine_commands.c
    src/core/scene.c
    src/ecs/component.c
    src/ecs/ecs.c
    src/ecs/view.c
    src/effects/animation.c
    src/effects/particles.c
    src/effects/sprite.c
    src/effects/tween.c
    src/input/input.c
    src/input/input_parser.c
    src/input/input_protocol.c
    src/input/input_queue.c
    src/input/input_state.c
    src/platform/terminal_io.c
    src/platform/terminal_posix.c
    src/platform/terminal_signals.c
    src/render/renderer.c
    src/render/renderer_ansi.c
    src/render/renderer_draw.c
    src/ui/canvas.c
    src/ui/focus.c
    src/ui/layout.c
    src/ui/widgets.c
    src/world/astar.c
    src/world/camera.c
    src/world/collision.c
    src/world/fov.c
    src/world/raycast.c
    src/world/tilemap.c
    src/world/world.c
    src/world/world_private.c
)
add_library(aforc::aforc ALIAS aforc)

target_compile_features(aforc PUBLIC c_std_17)
set_target_properties(aforc PROPERTIES
    C_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    VERSION "${PROJECT_VERSION}"
    SOVERSION "${PROJECT_VERSION_MAJOR}"
)
if(AFORC_HARDENING_ACTIVE AND BUILD_SHARED_LIBS)
    set_target_properties(aforc PROPERTIES
        C_VISIBILITY_PRESET hidden
    )
endif()
aforc_enable_target_hardening(aforc)

target_include_directories(aforc
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

find_package(Threads REQUIRED)
target_link_libraries(aforc PUBLIC Threads::Threads m)

if(BUILD_SHARED_LIBS)
    target_compile_definitions(aforc
        PUBLIC AFORC_SHARED
        PRIVATE AFORC_BUILDING_LIBRARY
    )
endif()

aforc_enable_target_warnings(aforc)
aforc_enable_target_sanitizers(aforc)

if(AFORC_BUILD_EXAMPLES)
    set(AFORC_ROGUELIKE_SOURCES
        examples/roguelike/main.c
        examples/roguelike/game.c
        examples/roguelike/generation.c
        examples/roguelike/persistence.c
        examples/roguelike/render.c
        examples/roguelike/runtime.c
        examples/roguelike/turns.c
    )
    add_executable(aforc_roguelike ${AFORC_ROGUELIKE_SOURCES})
    set_target_properties(aforc_roguelike PROPERTIES
        OUTPUT_NAME "aforc-roguelike"
    )
    aforc_configure_owned_target(aforc_roguelike)
    target_link_libraries(aforc_roguelike PRIVATE aforc::aforc)

    if(BUILD_TESTING)
        add_test(NAME roguelike_smoke COMMAND aforc_roguelike --smoke)
        set_tests_properties(roguelike_smoke PROPERTIES TIMEOUT 15)
    endif()
endif()

if(BUILD_TESTING)
    function(aforc_register_regression target test_name)
        aforc_configure_owned_target(${target})
        add_test(NAME ${test_name} COMMAND ${target})
        set_tests_properties(${test_name} PROPERTIES TIMEOUT 30)
    endfunction()

    function(aforc_add_library_regression test_name)
        set(target "aforc_${test_name}_test")
        add_executable(${target} ${ARGN})
        target_link_libraries(${target} PRIVATE aforc::aforc)
        aforc_register_regression(${target} ${test_name})
    endfunction()

    aforc_add_library_regression(
        scene_reentrancy
        tests/scene_reentrancy.c
    )
    aforc_add_library_regression(
        save_crc
        tests/save_crc.c
    )
    aforc_add_library_regression(
        astar_workspace
        tests/astar_workspace.c
        tests/astar_workspace_support.c
    )

    add_library(aforc_config_parser_test_subject OBJECT
        src/assets/config_parser.c
    )
    target_include_directories(aforc_config_parser_test_subject PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
    target_compile_options(aforc_config_parser_test_subject PRIVATE
        -include
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/config_parser_scale_support.h"
    )
    target_compile_definitions(aforc_config_parser_test_subject PRIVATE
        malloc=aforc_config_test_malloc
        realloc=aforc_config_test_realloc
        free=aforc_config_test_free
    )
    aforc_configure_owned_target(aforc_config_parser_test_subject)

    add_executable(aforc_config_parser_scale_test
        tests/config_parser_scale.c
        tests/config_parser_scale_support.c
        src/core/common.c
        $<TARGET_OBJECTS:aforc_config_parser_test_subject>
    )
    target_include_directories(aforc_config_parser_scale_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
    target_link_libraries(aforc_config_parser_scale_test PRIVATE
        Threads::Threads
        m
    )
    aforc_register_regression(
        aforc_config_parser_scale_test
        config_parser_scale
    )

    add_library(aforc_renderer_ansi_test_subject OBJECT
        src/render/renderer_ansi.c
    )
    target_include_directories(aforc_renderer_ansi_test_subject PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
    aforc_configure_owned_target(aforc_renderer_ansi_test_subject)

    add_executable(aforc_renderer_diff_test
        tests/renderer_diff.c
        tests/renderer_diff_cases.c
        tests/renderer_diff_model.c
        src/core/common.c
        $<TARGET_OBJECTS:aforc_renderer_ansi_test_subject>
    )
    target_link_libraries(aforc_renderer_diff_test PRIVATE aforc::aforc)
    aforc_register_regression(aforc_renderer_diff_test renderer_diff)

    add_executable(aforc_ecs_optimization_test
        tests/ecs_optimization.c
        tests/ecs_optimization_support.c
        src/core/common.c
        src/ecs/component.c
        src/ecs/ecs.c
        src/ecs/view.c
    )
    target_include_directories(aforc_ecs_optimization_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
    target_link_libraries(aforc_ecs_optimization_test PRIVATE
        Threads::Threads
        m
    )
    aforc_register_regression(aforc_ecs_optimization_test ecs_optimization)
endif()

install(
    TARGETS aforc
    EXPORT aforcTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
install(
    DIRECTORY include/aforc
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.h"
)
install(
    EXPORT aforcTargets
    FILE aforcTargets.cmake
    NAMESPACE aforc::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/aforc"
)

configure_package_config_file(
    cmake/aforcConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/aforcConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/aforc"
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/aforcConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)
install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/aforcConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/aforcConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/aforc"
)
