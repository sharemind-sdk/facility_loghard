#
# Copyright (C) 2015 Cybernetica
#
# Research/Commercial License Usage
# Licensees holding a valid Research License or Commercial License
# for the Software may use this file according to the written
# agreement between you and Cybernetica.
#
# GNU General Public License Usage
# Alternatively, this file may be used under the terms of the GNU
# General Public License version 3.0 as published by the Free Software
# Foundation and appearing in the file LICENSE.GPL included in the
# packaging of this file.  Please review the following information to
# ensure the GNU General Public License version 3.0 requirements will be
# met: http://www.gnu.org/copyleft/gpl-3.0.html.
#
# For further information, please contact us at sharemind@cyber.ee.
#

CMAKE_MINIMUM_REQUIRED(VERSION "2.8.12")
PROJECT("SHAREMIND_FACILITY_LOGHARD" "CXX")
SET(SHAREMIND_FACILITY_LOGHARD_VERSION "0.1.0.0")
SET(SHAREMIND_FACILITY_LOGHARD_COMPAT_VERSION "0.1.0.0")

INCLUDE("${CMAKE_CURRENT_SOURCE_DIR}/config.local" OPTIONAL)
INCLUDE("${CMAKE_CURRENT_BINARY_DIR}/config.local" OPTIONAL)

FIND_PACKAGE(LogHard 0.1.0 REQUIRED)
FIND_PACKAGE(SharemindCxxHeaders REQUIRED)
FIND_PACKAGE(SharemindLibfmodapi 0.1.0 REQUIRED)

ADD_COMPILE_OPTIONS(
  "-std=c++11" "-Wall" "-Wextra" "-O2"
  "$<$<NOT:$<STREQUAL:$<CONFIGURATION>,Release>>:-ggdb>"
  "$<$<NOT:$<STREQUAL:$<CONFIGURATION>,Release>>:-fno-omit-frame-pointer>"
  "$<$<STREQUAL:$<CONFIGURATION>,Release>:-DNDEBUG>"
  "$<$<STREQUAL:$<CONFIGURATION>,Release>:-fomit-frame-pointer>"
)

INCLUDE(CheckCXXCompilerFlag)
FUNCTION(CHECK_ADD_COMPILER_FLAG flag)
    STRING(SUBSTRING "${flag}" 1 -1 FlagName)
    STRING(REPLACE "+" "X" FlagName "${FlagName}")
    CHECK_CXX_COMPILER_FLAG("${flag}" C_COMPILER_HAS_${FlagName}_FLAG)
    IF(C_COMPILER_HAS_${FlagName}_FLAG)
        ADD_COMPILE_OPTIONS("${flag}")
    ENDIF()
ENDFUNCTION()
CHECK_ADD_COMPILER_FLAG("-Weverything")
CHECK_ADD_COMPILER_FLAG("-Wlogical-op")
CHECK_ADD_COMPILER_FLAG("-Wno-covered-switch-default")
CHECK_ADD_COMPILER_FLAG("-Wno-padded")
CHECK_ADD_COMPILER_FLAG("-Wno-c++98-compat")
CHECK_ADD_COMPILER_FLAG("-Wno-c++98-compat-pedantic")
CHECK_ADD_COMPILER_FLAG("-Wno-gnu-case-range")
CHECK_ADD_COMPILER_FLAG("-Wno-weak-vtables")

ADD_DEFINITIONS(
    ${LogHard_DEFINITIONS}
    ${SharemindCxxHeaders_DEFINITIONS}
    ${SharemindLibfmodapi_DEFINITIONS}
    "-D__STDC_LIMIT_MACROS"
    "-D_XOPEN_SOURCE=700"
)

INCLUDE_DIRECTORIES(
    ${LogHard_INCLUDE_DIRS}
    ${SharemindCxxHeaders_INCLUDE_DIRS}
    ${SharemindLibfmodapi_INCLUDE_DIRS}
)



################################################################################
# libLogHardFacility
################################################################################

FILE(GLOB_RECURSE SHAREMIND_FACILITY_LOGHARD_HEADERS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
FILE(GLOB_RECURSE SHAREMIND_FACILITY_LOGHARD_SOURCES
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
IF(APPLE)
    # OS X bundle does not accept VERSION numbers, so build regular .dylib instead
    # Newer OS X can also dynamically load shared libraries similarly to Linux
    ADD_LIBRARY("sharemind_facility_loghard" SHARED
                ${SHAREMIND_FACILITY_LOGHARD_SOURCES}
                ${SHAREMIND_FACILITY_LOGHARD_HEADERS}
    )
ELSE()
    ADD_LIBRARY("sharemind_facility_loghard" MODULE
                ${SHAREMIND_FACILITY_LOGHARD_SOURCES}
                ${SHAREMIND_FACILITY_LOGHARD_HEADERS}
    )
ENDIF()
STRING(REGEX REPLACE "(([^\\.]+\\.[^\\.]+)\\.).*" "\\2"
       SHAREMIND_FACILITY_LOGHARD_SOVERSION "${SHAREMIND_FACILITY_LOGHARD_VERSION}")
SET_TARGET_PROPERTIES("sharemind_facility_loghard" PROPERTIES
                      OUTPUT_NAME "sharemind_facility_loghard"
                      VERSION "${SHAREMIND_FACILITY_LOGHARD_VERSION}"
                      SOVERSION "${SHAREMIND_FACILITY_LOGHARD_SOVERSION}")
TARGET_LINK_LIBRARIES("sharemind_facility_loghard" ${LogHard_LIBRARIES})
INSTALL(TARGETS "sharemind_facility_loghard"
        LIBRARY DESTINATION "lib")
