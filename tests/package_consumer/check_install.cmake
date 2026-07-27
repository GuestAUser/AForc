# AForc
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED AFORC_INSTALL_PREFIX)
    message(FATAL_ERROR "AFORC_INSTALL_PREFIX is required")
endif()
if(NOT DEFINED AFORC_INSTALL_LIBDIR)
    set(AFORC_INSTALL_LIBDIR lib)
endif()
if(NOT DEFINED AFORC_INSTALL_DATADIR)
    set(AFORC_INSTALL_DATADIR share)
endif()
set(AFORC_REQUIRED_INSTALL_PATHS
    include/aforc/aforc.h
    "${AFORC_INSTALL_LIBDIR}/cmake/aforc/aforcConfig.cmake"
    "${AFORC_INSTALL_LIBDIR}/cmake/aforc/aforcConfigVersion.cmake"
    "${AFORC_INSTALL_LIBDIR}/cmake/aforc/aforcTargets.cmake"
    "${AFORC_INSTALL_LIBDIR}/pkgconfig/aforc.pc"
    "${AFORC_INSTALL_DATADIR}/licenses/aforc/LICENSE"
    "${AFORC_INSTALL_DATADIR}/licenses/aforc/THIRD_PARTY_NOTICES.md"
)

foreach(relative_path IN LISTS AFORC_REQUIRED_INSTALL_PATHS)
    if(NOT EXISTS "${AFORC_INSTALL_PREFIX}/${relative_path}")
        message(FATAL_ERROR "Installed package is missing ${relative_path}")
    endif()
endforeach()

file(GLOB AFORC_INSTALLED_LIBRARIES
    "${AFORC_INSTALL_PREFIX}/${AFORC_INSTALL_LIBDIR}/libaforc.*"
)
if(NOT AFORC_INSTALLED_LIBRARIES)
    message(FATAL_ERROR "Installed package is missing the AForc library")
endif()

set(AFORC_PC_PATH
    "${AFORC_INSTALL_PREFIX}/${AFORC_INSTALL_LIBDIR}/pkgconfig/aforc.pc"
)
file(READ "${AFORC_PC_PATH}" AFORC_PC_CONTENT)
file(RELATIVE_PATH AFORC_EXPECTED_PC_PREFIX
    "${AFORC_INSTALL_PREFIX}/${AFORC_INSTALL_LIBDIR}/pkgconfig"
    "${AFORC_INSTALL_PREFIX}"
)
foreach(required_text IN ITEMS
        "prefix=\${pcfiledir}/${AFORC_EXPECTED_PC_PREFIX}"
        "Version: 0.1.0"
        "Libs: -L\${libdir} -laforc"
        "Cflags: -I\${includedir}"
)
    string(FIND "${AFORC_PC_CONTENT}" "${required_text}" text_index)
    if(text_index EQUAL -1)
        message(FATAL_ERROR "aforc.pc is missing: ${required_text}")
    endif()
endforeach()
string(FIND "${AFORC_PC_CONTENT}" "${AFORC_INSTALL_PREFIX}" prefix_index)
if(NOT prefix_index EQUAL -1)
    message(FATAL_ERROR "aforc.pc contains its staging prefix")
endif()

file(READ
    "${AFORC_INSTALL_PREFIX}/${AFORC_INSTALL_DATADIR}/licenses/aforc/LICENSE"
    AFORC_LICENSE_CONTENT
)
string(FIND "${AFORC_LICENSE_CONTENT}" "MIT License" license_index)
if(license_index EQUAL -1)
    message(FATAL_ERROR "Installed LICENSE is not the MIT License")
endif()

file(READ
    "${AFORC_INSTALL_PREFIX}/${AFORC_INSTALL_DATADIR}/licenses/aforc/THIRD_PARTY_NOTICES.md"
    AFORC_NOTICES_CONTENT
)
foreach(required_notice IN ITEMS
        "Melissa O'Neill"
        "Jean-loup Gailly"
        "Mark Adler"
)
    string(FIND "${AFORC_NOTICES_CONTENT}" "${required_notice}" notice_index)
    if(notice_index EQUAL -1)
        message(FATAL_ERROR "Installed notices are missing ${required_notice}")
    endif()
endforeach()
