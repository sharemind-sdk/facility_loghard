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

CMAKE_MINIMUM_REQUIRED(VERSION 3.0)
PROJECT(SharemindFacilityLogHard VERSION 0.3.0 LANGUAGES CXX)

INCLUDE("${CMAKE_CURRENT_SOURCE_DIR}/config.local" OPTIONAL)
INCLUDE("${CMAKE_CURRENT_BINARY_DIR}/config.local" OPTIONAL)

FIND_PACKAGE(SharemindCMakeHelpers REQUIRED)

SharemindSetProjectVersion()


FIND_PACKAGE(LogHard 0.3.0 REQUIRED)
FIND_PACKAGE(SharemindCxxHeaders 0.4.0 REQUIRED)
FIND_PACKAGE(SharemindFacilityModuleApis 2.0.0 REQUIRED)


# The facility:
SharemindSetCxx11CompileOptions()
FILE(GLOB_RECURSE SharemindFacilityLogHard_HEADERS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
FILE(GLOB_RECURSE SharemindFacilityLogHard_SOURCES
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
SharemindAddSharedLibrary("sharemind_facility_loghard"
    MODULE
    OUTPUT_NAME "sharemind_facility_loghard"
    SOURCES
        ${SharemindFacilityLogHard_SOURCES}
        ${SharemindFacilityLogHard_HEADERS}
    INCLUDE_DIRECTORIES
        ${LogHard_INCLUDE_DIRS}
        ${SharemindCxxHeaders_INCLUDE_DIRS}
        ${SharemindFacilityModuleApis_INCLUDE_DIRS}
    COMPILE_DEFINITIONS
        "_XOPEN_SOURCE=700"
    LEGACY_DEFINITIONS
        ${LogHard_DEFINITIONS}
        ${SharemindCxxHeaders_DEFINITIONS}
        ${SharemindFacilityModuleApis_DEFINITIONS}
    LINK_LIBRARIES
        ${LogHard_LIBRARIES}
        ${SharemindCxxHeaders_LIBRARIES}
        ${SharemindFacilityModuleApis_LIBRARIES}
)


# Packaging:
SharemindSetupPackaging()
SharemindAddComponentPackage("lib"
    NAME "libsharemind-facility-loghard"
    DESCRIPTION "Sharemind LogHard facility"
    DEB_SECTION "libs"
    DEB_DEPENDS
        "libloghard (>= 0.3.0)"
        "libstdc++6 (>= 4.8.0)"
        "libc6 (>= 2.19)"
)
SharemindAddComponentPackage("debug"
    NAME "libsharemind-facility-loghard-dbg"
    DESCRIPTION "Sharemind LogHard facility debug symbols"
    DEB_SECTION "debug"
    DEB_DEPENDS
        "libsharemind-facility-loghard (= ${SharemindFacilityLogHard_DEB_lib_PACKAGE_VERSION})"
)
SharemindPackagingFinalize()
