#############################################################################
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2023 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#############################################################################
cmake_minimum_required(VERSION 3.3)

project(NetworkManager)

# When THUNDER_VERSION is explicitly set, always use FetchContent
# (skip find_package — avoids conflicts with stale cached installs)
if(DEFINED THUNDER_VERSION)
    message(STATUS "THUNDER_VERSION set — using FetchContent (${THUNDER_VERSION})")
    add_subdirectory(tests/thunder)
else()
    # Try WPEFramework first
    find_package(WPEFramework QUIET CONFIG)

    # If WPEFramework isn't found, try Thunder
    if (NOT WPEFramework_FOUND)
        find_package(Thunder QUIET CONFIG)
    endif()

    if (NOT WPEFramework_FOUND AND NOT Thunder_FOUND)
        if(ENABLE_UNIT_TESTING)
            message(STATUS "Thunder not found via find_package — using FetchContent for test builds")
            add_subdirectory(tests/thunder)
        else()
            message(FATAL_ERROR
                "Neither WPEFramework nor Thunder could be found.\n"
                "Install one of them and ensure its CMake package is discoverable via CMAKE_PREFIX_PATH or default locations.\n"
                "Alternatively, pass -DTHUNDER_VERSION=<tag> to use FetchContent.")
        endif()
    endif()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
set(VERSION_MAJOR 3)
set(VERSION_MINOR 0)
set(VERSION_PATCH 0)

add_compile_definitions(NETWORKMANAGER_MAJOR_VERSION=${VERSION_MAJOR})
add_compile_definitions(NETWORKMANAGER_MINOR_VERSION=${VERSION_MINOR})
add_compile_definitions(NETWORKMANAGER_PATCH_VERSION=${VERSION_PATCH})

set(PROJECT_VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})

include(CmakeHelperFunctions)

string(TOLOWER ${NAMESPACE} STORAGE_DIRECTORY)
get_directory_property(SEVICES_DEFINES COMPILE_DEFINITIONS)

option(ENABLE_LEGACY_PLUGINS "Enable Legacy Plugins" ON)
option(USE_RDK_LOGGER "Enable RDK Logger for logging" OFF )
option(ENABLE_UNIT_TESTING "Enable unit tests" OFF)
option(USE_TELEMETRY "Enable Telemetry T2 support" OFF)

if (USE_TELEMETRY)
    find_package(T2 REQUIRED)
    add_compile_definitions(USE_TELEMETRY=1)
    message("Telemetry support enabled")
endif(USE_TELEMETRY)

add_subdirectory(interface)
add_subdirectory(definition)
add_subdirectory(plugin)

if (ENABLE_LEGACY_PLUGINS)
    add_subdirectory(legacy)
endif (ENABLE_LEGACY_PLUGINS)

add_subdirectory(tools)

if(ENABLE_UNIT_TESTING)
    # Fetch GoogleTest for all test targets
    include(FetchContent)
    set(GOOGLE_TEST_VERSION "v1.15.2" CACHE STRING "GoogleTest git tag/branch/commit to use")
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG ${GOOGLE_TEST_VERSION}
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_subdirectory(tests/l1Test)
    add_subdirectory(tests/l2Test)
endif(ENABLE_UNIT_TESTING)
