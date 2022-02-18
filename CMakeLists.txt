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
PROJECT(SharemindFacilityLogHard VERSION 0.4.1 LANGUAGES CXX)

INCLUDE("${CMAKE_CURRENT_SOURCE_DIR}/config.local" OPTIONAL)
INCLUDE("${CMAKE_CURRENT_BINARY_DIR}/config.local" OPTIONAL)

FIND_PACKAGE(SharemindCMakeHelpers 1.6 REQUIRED)

FIND_PACKAGE(LogHard 0.5.0 REQUIRED)
FIND_PACKAGE(SharemindCxxHeaders 0.9.0 REQUIRED)
FIND_PACKAGE(SharemindFacilityModuleApis 2.1.0 REQUIRED)


# The facility:
FILE(GLOB_RECURSE SharemindFacilityLogHard_HEADERS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
FILE(GLOB_RECURSE SharemindFacilityLogHard_SOURCES
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
SharemindAddSharedLibrary(FacilityLogHard
    MODULE
    OUTPUT_NAME "sharemind_facility_loghard"
    SOURCES
        ${SharemindFacilityLogHard_SOURCES}
        ${SharemindFacilityLogHard_HEADERS}
)
TARGET_COMPILE_DEFINITIONS(FacilityLogHard PRIVATE "_XOPEN_SOURCE=700")
TARGET_LINK_LIBRARIES(FacilityLogHard
    PRIVATE
        LogHard::LogHard
        Sharemind::CxxHeaders
        Sharemind::FacilityModuleApis
    )

# Configuration files:
INSTALL(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/packaging/configs/sharemind/"
        DESTINATION "/etc/sharemind/"
        COMPONENT "lib"
        EXCLUDE_FROM_ALL)

# Packaging:
SharemindSetupPackaging()
SharemindAddComponentPackage("lib"
    NAME "libsharemind-facility-loghard"
    DESCRIPTION "Sharemind LogHard facility"
    DEB_SECTION "libs"
    DEB_DEPENDS
        "libloghard (>= 0.5.0)"
        "libstdc++6 (>= 4.8.0)"
        "libc6 (>= 2.19)"
    DEB_EXTRA_CONTROL_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/debian/conffiles"
)
SharemindAddComponentPackage("debug"
    NAME "libsharemind-facility-loghard-dbg"
    DESCRIPTION "Sharemind LogHard facility debug symbols"
    DEB_SECTION "debug"
    DEB_DEPENDS
        "libsharemind-facility-loghard (= ${SharemindFacilityLogHard_DEB_lib_PACKAGE_VERSION})"
)
SharemindPackagingFinalize()
