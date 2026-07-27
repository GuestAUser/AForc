# AForc
# Author: GuestAUser
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.16)

project(AForcPackageConsumer LANGUAGES C)

set(AFORC_REQUIRED_VERSION "0.1" CACHE STRING
    "AForc version required by the package consumer")

find_package(aforc ${AFORC_REQUIRED_VERSION} CONFIG REQUIRED)

add_executable(aforc_package_consumer main.c)
target_compile_features(aforc_package_consumer PRIVATE c_std_17)
set_target_properties(aforc_package_consumer PROPERTIES C_EXTENSIONS OFF)
target_link_libraries(aforc_package_consumer PRIVATE aforc::aforc)

enable_testing()
add_test(NAME installed_package_smoke COMMAND aforc_package_consumer)
