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

set(PLUGIN_LEGACY_NW_AUTOSTART "false" CACHE STRING "The autostart of Legacy Network plugin")
set(PLUGIN_LEGACY_WIFI_AUTOSTART "false" CACHE STRING "The autostart of Legacy WiFi plugin")

include_directories(${PROJECT_SOURCE_DIR}/interface)
include_directories(${PROJECT_SOURCE_DIR}/plugin)

add_library(${PLUGIN_LEGACY_NETWORK} SHARED
        LegacyNetworkAPIs.cpp
        ../plugin/Module.cpp
)

target_link_libraries(${PLUGIN_LEGACY_NETWORK}  PRIVATE ${NAMESPACE}Core::${NAMESPACE}Core ${NAMESPACE}Plugins::${NAMESPACE}Plugins)

set_target_properties(${PLUGIN_LEGACY_NETWORK} PROPERTIES
                                        CXX_STANDARD 17
                                        CXX_STANDARD_REQUIRED YES)

install(TARGETS ${PLUGIN_LEGACY_NETWORK} DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${STORAGE_DIRECTORY}/plugins)

add_library(${PLUGIN_LEGACY_WIFI} SHARED
        LegacyWiFiManagerAPIs.cpp
        ../plugin/Module.cpp
)

target_link_libraries(${PLUGIN_LEGACY_WIFI}  PRIVATE ${NAMESPACE}Core::${NAMESPACE}Core ${NAMESPACE}Plugins::${NAMESPACE}Plugins)

set_target_properties(${PLUGIN_LEGACY_WIFI} PROPERTIES
                                        CXX_STANDARD 17
                                        CXX_STANDARD_REQUIRED YES)

install(TARGETS ${PLUGIN_LEGACY_WIFI} DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${STORAGE_DIRECTORY}/plugins)

write_config(PLUGINS LegacyNetworkAPIs CLASSNAME Network LOCATOR lib${PLUGIN_LEGACY_NETWORK}.so)
write_config(PLUGINS LegacyWiFiManagerAPIs CLASSNAME WiFiManager LOCATOR lib${PLUGIN_LEGACY_WIFI}.so)
