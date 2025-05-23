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
cmake_minimum_required(VERSION 3.3)

set(MODULE_NAME routerDiscovery)
find_package(PkgConfig REQUIRED)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(LIBSOUP REQUIRED libsoup-2.4)
pkg_check_modules(GSSDP REQUIRED gssdp-1.2)
pkg_check_modules(GUPNP gupnp-1.2)

if (NOT GUPNP_FOUND)
    pkg_check_modules(GUPNP REQUIRED gupnp-1.0)
    add_definitions(-DGUPNP_1_0)
endif ()

if (USE_TELEMETRY)
    find_package(T2 REQUIRED)
    add_definitions(-DUSE_TELEMETRY)
    include_directories(${TELEMETRY_INCLUDE_DIRS})
endif (USE_TELEMETRY)

add_definitions(-DENABLE_ROUTER_DISCOVERY_MAIN)

add_executable(${MODULE_NAME} UpnpDiscoveryManager.cpp)
target_include_directories(${MODULE_NAME} PRIVATE  ${CMAKE_CURRENT_SOURCE_DIR} ${GLIB_INCLUDE_DIRS} ${LIBSOUP_INCLUDE_DIRS} ${GUPNP_INCLUDE_DIRS} ${GSSDP_INCLUDE_DIRS})
target_link_libraries(${MODULE_NAME} PRIVATE ${GLIB_LIBRARIES} ${LIBSOUP_LIBRARIES} ${GUPNP_LIBRARIES} ${GSSDP_LIBRARIES})

if (USE_TELEMETRY)
    target_link_libraries(${MODULE_NAME} PRIVATE ${T2_LIBRARIES})
endif (USE_TELEMETRY)

install(TARGETS ${MODULE_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(ENABLE_GNOME_NETWORKMANAGER)
install(PROGRAMS     scripts/getRouterInfo-NMdispatcher.sh
        DESTINATION  ${CMAKE_INSTALL_PREFIX}/../lib/rdk
        RENAME       getRouterInfo.sh)
else ()
install(PROGRAMS     scripts/getRouterInfo-nlmon.sh
        DESTINATION  ${CMAKE_INSTALL_PREFIX}/../lib/rdk
        RENAME       getRouterInfo.sh)
endif ()

install(PROGRAMS
        scripts/readyToGetRouterInfo.sh
        DESTINATION
        ${CMAKE_INSTALL_PREFIX}/../lib/rdk)

install(FILES
        scripts/routerDiscovery@.service
        DESTINATION
        ${CMAKE_INSTALL_PREFIX}/../lib/systemd/system)
