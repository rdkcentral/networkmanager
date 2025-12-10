#############################################################################
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2025 RDK Management
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

set(PLUGIN_NAME NetworkConnectionStats)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

message("Building ${PLUGIN_NAME} Thunder Plugin (Internal-only, no external APIs)")

# Find required packages
find_package(${NAMESPACE}Plugins REQUIRED)
find_package(${NAMESPACE}Definitions REQUIRED)
find_package(CompileSettingsDebug CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)

# JSON-RPC CPP and JSON libraries for network provider
pkg_check_modules(JSONRPCCPP REQUIRED libjsonrpccpp-client libjsonrpccpp-common)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

# Optional Telemetry support
if (USE_TELEMETRY)
    find_package(T2 REQUIRED)
    add_definitions(-DUSE_TELEMETRY)
    message("Telemetry support enabled")
endif()

# Interface library - minimal interface (no external APIs)
add_library(${MODULE_NAME}Interface INTERFACE)
target_link_libraries(${MODULE_NAME}Interface INTERFACE ${NAMESPACE}Plugins::${NAMESPACE}Plugins)
target_compile_definitions(${MODULE_NAME}Interface INTERFACE ID_NETWORKCONNECTIONSTATS=0x80000001)
target_compile_definitions(${MODULE_NAME}Interface INTERFACE ID_NETWORKCONNECTIONSTATS_NOTIFICATION=0x80000002)

install(
    FILES INetworkConnectionStats.h
    DESTINATION include/${NAMESPACE}/interfaces
    COMPONENT ${NAMESPACE}_Development
)

# Main Plugin Library (in-process)
add_library(${MODULE_NAME} SHARED
    Module.cpp
    NetworkConnectionStats.cpp
)

target_link_libraries(${MODULE_NAME}
    PRIVATE
        CompileSettingsDebug::CompileSettingsDebug
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}Definitions::${NAMESPACE}Definitions
        ${MODULE_NAME}Interface
)

target_include_directories(${MODULE_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(${MODULE_NAME} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    FRAMEWORK FALSE
    VERSION ${PROJECT_VERSION}
)

# Out-of-process Implementation Library
add_library(${MODULE_NAME}Implementation SHARED
    Module.cpp
    NetworkConnectionStatsImplementation.cpp
    ThunderJsonRPCProvider.cpp
)

target_link_libraries(${MODULE_NAME}Implementation
    PRIVATE
        CompileSettingsDebug::CompileSettingsDebug
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}Definitions::${NAMESPACE}Definitions
        ${MODULE_NAME}Interface
        ${JSONRPCCPP_LIBRARIES}
        ${JSONCPP_LIBRARIES}
        pthread
)

if (USE_TELEMETRY)
    target_link_libraries(${MODULE_NAME}Implementation PRIVATE ${T2_LIBRARIES})
endif()

target_include_directories(${MODULE_NAME}Implementation
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${JSONRPCCPP_INCLUDE_DIRS}
        ${JSONCPP_INCLUDE_DIRS}
)

set_target_properties(${MODULE_NAME}Implementation PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    FRAMEWORK FALSE
    VERSION ${PROJECT_VERSION}
    OUTPUT_NAME ${MODULE_NAME}Implementation
)

# Install plugin libraries
install(
    TARGETS ${MODULE_NAME} ${MODULE_NAME}Implementation ${MODULE_NAME}Interface
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${STORAGE_DIRECTORY}/plugins
    COMPONENT ${NAMESPACE}_Runtime
)

# Configuration variables for plugin
set(PLUGIN_NETWORKCONNECTIONSTATS_AUTOSTART "true" CACHE STRING "Automatically start the plugin")
set(PLUGIN_NETWORKCONNECTIONSTATS_OUTOFPROCESS "true" CACHE STRING "Run plugin out-of-process")
set(PLUGIN_NETWORKCONNECTIONSTATS_STARTUPORDER "50" CACHE STRING "Plugin startup order")
set(PLUGIN_NETWORKCONNECTIONSTATS_REPORTING_INTERVAL "600" CACHE STRING "Reporting interval in seconds (default: 10 minutes)")

set(PLUGIN_BUILD_REFERENCE ${PROJECT_VERSION} CACHE STRING "To Set the Hash for the plugin")
add_definitions(-DBUILD_REFERENCE=${PLUGIN_BUILD_REFERENCE})

# Install configuration
write_config(${PLUGIN_NAME})
