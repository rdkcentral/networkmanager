#############################################################################
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2024 RDK Management
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
message("Build Legacy Plugins")

find_package(${NAMESPACE}Core REQUIRED)
find_package(${NAMESPACE}Plugins REQUIRED)

set(PLUGIN_LEGACY_NETWORK ${NAMESPACE}Network)
set(PLUGIN_LEGACY_WIFI    ${NAMESPACE}WiFiManager)

set(PLUGIN_LEGACY_NW_STARTUPORDER "35" CACHE STRING "To configure startup order of Legacy Network plugin")
set(PLUGIN_LEGACY_WIFI_STARTUPORDER "35" CACHE STRING "To configure startup order of Legacy WiFi plugin")

set(PLUGIN_BUILD_REFERENCE ${PROJECT_VERSION} CACHE STRING "To Set the Hash for the plugin")
add_definitions(-DPLUGIN_BUILD_REFERENCE=${PLUGIN_BUILD_REFERENCE})

if (USE_RDK_LOGGER)
    find_package(rdklogger REQUIRED)
    add_definitions(-DUSE_RDK_LOGGER)

    include_directories(${RDKLOGGER_INCLUDE_DIRS})
endif (USE_RDK_LOGGER)

find_package(NFRTool REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/interface)
include_directories(${PROJECT_SOURCE_DIR}/plugin)

add_library(${PLUGIN_LEGACY_NETWORK} SHARED
        LegacyNetworkAPIs.cpp
        ../plugin/NetworkManagerLogger.cpp
        ../plugin/Module.cpp
)

target_link_libraries(${PLUGIN_LEGACY_NETWORK}  PRIVATE ${NAMESPACE}Core::${NAMESPACE}Core ${NAMESPACE}Plugins::${NAMESPACE}Plugins)

if (USE_RDK_LOGGER)
    target_link_libraries(${PLUGIN_LEGACY_NETWORK} PRIVATE ${RDKLOGGER_LIBRARIES})
endif (USE_RDK_LOGGER)

set_target_properties(${PLUGIN_LEGACY_NETWORK} PROPERTIES
                                        CXX_STANDARD 11
                                        CXX_STANDARD_REQUIRED YES)

install(TARGETS ${PLUGIN_LEGACY_NETWORK} DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${STORAGE_DIRECTORY}/plugins)

add_library(${PLUGIN_LEGACY_WIFI} SHARED
        LegacyWiFiManagerAPIs.cpp
        ../plugin/NetworkManagerLogger.cpp
        ../plugin/Module.cpp
)

target_link_libraries(${PLUGIN_LEGACY_WIFI}  PRIVATE ${NAMESPACE}Core::${NAMESPACE}Core ${NAMESPACE}Plugins::${NAMESPACE}Plugins)

if (USE_RDK_LOGGER)
    target_link_libraries(${PLUGIN_LEGACY_WIFI} PRIVATE ${RDKLOGGER_LIBRARIES})
endif (USE_RDK_LOGGER)

target_include_directories(${PLUGIN_LEGACY_WIFI} PRIVATE ${NFRTOOL_INCLUDE_DIRS})
target_link_libraries(${PLUGIN_LEGACY_WIFI} PRIVATE ${NFRTOOL_LIBRARIES})

set_target_properties(${PLUGIN_LEGACY_WIFI} PROPERTIES
                                        CXX_STANDARD 11
                                        CXX_STANDARD_REQUIRED YES)

install(TARGETS ${PLUGIN_LEGACY_WIFI} DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${STORAGE_DIRECTORY}/plugins)

write_config(PLUGINS LegacyNetworkAPIs CLASSNAME Network LOCATOR lib${PLUGIN_LEGACY_NETWORK}.so)
write_config(PLUGINS LegacyWiFiManagerAPIs CLASSNAME WiFiManager LOCATOR lib${PLUGIN_LEGACY_WIFI}.so)
