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
find_package(WPEFramework)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
set(VERSION_MAJOR 1)
set(VERSION_MINOR 7)
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


add_subdirectory(interface)
add_subdirectory(definition)
add_subdirectory(plugin)

if (ENABLE_LEGACY_PLUGINS)
    add_subdirectory(legacy)
endif (ENABLE_LEGACY_PLUGINS)

add_subdirectory(tools)

if(ENABLE_UNIT_TESTING)
    add_subdirectory(tests/l1Test)
    add_subdirectory(tests/l2Test)
endif(ENABLE_UNIT_TESTING)

