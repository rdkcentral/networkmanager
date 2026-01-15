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
message("Build NetworkConnectionStats v${PROJECT_VERSION}")

set(PLUGIN_NAME NetworkConnectionStats)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})
set(MODULE_IMPL_NAME ${NAMESPACE}${PLUGIN_NAME}Implementation)

find_package(${NAMESPACE}Core REQUIRED)
find_package(${NAMESPACE}Plugins REQUIRED)
find_package(${NAMESPACE}Definitions REQUIRED)
find_package(CompileSettingsDebug CONFIG REQUIRED)

# Optional Telemetry support
if (USE_TELEMETRY)
    find_package(T2 REQUIRED)
    add_definitions(-DUSE_TELEMETRY)
    message("Telemetry support enabled")
endif()

set(PLUGIN_BUILD_REFERENCE ${PROJECT_VERSION} CACHE STRING "To Set the Hash for the plugin")
add_definitions(-DPLUGIN_BUILD_REFERENCE=${PLUGIN_BUILD_REFERENCE})

# Include directories - use CMAKE_CURRENT_SOURCE_DIR for relative paths
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../interface)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Main Plugin Library (in-process)
add_library(${MODULE_NAME} SHARED
    Module.cpp
    NetworkConnectionStats.cpp
    NetworkConnectionStatsLogger.cpp
)

target_link_libraries(${MODULE_NAME}
    PRIVATE
        CompileSettingsDebug::CompileSettingsDebug
        ${NAMESPACE}Core::${NAMESPACE}Core
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}Definitions::${NAMESPACE}Definitions
)

target_include_directories(${MODULE_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../interface
)

set_target_properties(${MODULE_NAME} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    FRAMEWORK FALSE
)

# Out-of-process Implementation Library
add_library(${MODULE_IMPL_NAME} SHARED
    Module.cpp
    NetworkConnectionStatsImplementation.cpp
    NetworkConnectionStatsLogger.cpp
    ThunderJsonRPCProvider.cpp
)

target_link_libraries(${MODULE_IMPL_NAME}
    PRIVATE
        CompileSettingsDebug::CompileSettingsDebug
        ${NAMESPACE}Core::${NAMESPACE}Core
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}Definitions::${NAMESPACE}Definitions
        pthread
)

if (USE_TELEMETRY)
    target_link_libraries(${MODULE_IMPL_NAME} PRIVATE ${T2_LIBRARIES})
endif()

target_include_directories(${MODULE_IMPL_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../interface
)

set_target_properties(${MODULE_IMPL_NAME} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    FRAMEWORK FALSE
    OUTPUT_NAME ${MODULE_NAME}Implementation
)

# Install plugin libraries
install(
    TARGETS ${MODULE_NAME} ${MODULE_IMPL_NAME}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${STORAGE_DIRECTORY}/plugins
    COMPONENT ${NAMESPACE}_Runtime
)

# Configuration variables for plugin
set(PLUGIN_NETWORKCONNECTIONSTATS_AUTOSTART "false" CACHE STRING "Automatically start the plugin")
set(PLUGIN_NETWORKCONNECTIONSTATS_OUTOFPROCESS "true" CACHE STRING "Run plugin out-of-process")
set(PLUGIN_NETWORKCONNECTIONSTATS_STARTUPORDER "50" CACHE STRING "Plugin startup order")
set(PLUGIN_NETWORKCONNECTIONSTATS_REPORTING_INTERVAL "600" CACHE STRING "Reporting interval in seconds (default: 10 minutes)")

# Install configuration
write_config(PLUGINS NetworkConnectionStats)
