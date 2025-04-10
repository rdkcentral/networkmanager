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
message("Build ${PROJECT_NAME} v${PROJECT_VERSION}")

set(PLUGIN_NAME NetworkManager)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})
set(MODULE_IMPL_NAME ${NAMESPACE}${PLUGIN_NAME}Impl)

find_package(${NAMESPACE}Core REQUIRED)
find_package(${NAMESPACE}Plugins REQUIRED)
find_package(CURL)

if (USE_RDK_LOGGER)
    find_package(rdklogger REQUIRED)
    add_definitions(-DUSE_RDK_LOGGER)

    include_directories(${RDKLOGGER_INCLUDE_DIRS})
endif (USE_RDK_LOGGER)



if(ENABLE_GNOME_NETWORKMANAGER)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(LIBNM REQUIRED libnm)
pkg_check_modules(GLIB REQUIRED gio-2.0)
else()
find_package(IARMBus REQUIRED)
endif ()

set(PLUGIN_NETWORKMANAGER_LOGLEVEL "5" CACHE STRING "To configure default loglevel NetworkManager plugin")
set(PLUGIN_NETWORKMANAGER_STARTUPORDER "25" CACHE STRING "To configure startup order of Unified NetworkManager plugin")

set(PLUGIN_BUILD_REFERENCE ${PROJECT_VERSION} CACHE STRING "To Set the Hash for the plugin")
add_definitions(-DPLUGIN_BUILD_REFERENCE=${PLUGIN_BUILD_REFERENCE})

include_directories(${PROJECT_SOURCE_DIR}/interface)
include_directories(${PROJECT_SOURCE_DIR}/plugin)

#Setting up source files
file(GLOB PROXY_STUB_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/../interface/generated/ProxyStubs*.cpp")

# Build the main plugin that runs inside the WPEFramework daemon
add_library(${MODULE_NAME} SHARED
                            NetworkManager.cpp
                            NetworkManagerJsonRpc.cpp
                            NetworkManagerLogger.cpp
                            Module.cpp
                            ${PROXY_STUB_SOURCES})

target_link_libraries(${MODULE_NAME}  PRIVATE
                                        ${NAMESPACE}Core::${NAMESPACE}Core
                                        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
                                        )

set_target_properties(${MODULE_NAME} PROPERTIES
                                     CXX_STANDARD 11
                                     CXX_STANDARD_REQUIRED YES
                                     FRAMEWORK FALSE)

add_library(${MODULE_IMPL_NAME} SHARED
                            NetworkManagerImplementation.cpp
                            NetworkManagerConnectivity.cpp
                            NetworkManagerStunClient.cpp
                            NetworkManagerLogger.cpp
                            Module.cpp
                            ${PROXY_STUB_SOURCES})

if(ENABLE_GNOME_NETWORKMANAGER)
    add_definitions(-DENABLE_GNOME_NETWORKMANAGER)
    if(ENABLE_GNOME_GDBUS)
        message("networkmanager building with gdbus")
        target_sources(${MODULE_IMPL_NAME} PRIVATE
            gnome/gdbus/NetworkManagerGdbusProxy.cpp
            gnome/gdbus/NetworkManagerGdbusClient.cpp
            gnome/gdbus/NetworkManagerGdbusEvent.cpp
            gnome/gdbus/NetworkManagerGdbusMgr.cpp
            gnome/gdbus/NetworkManagerGdbusUtils.cpp
            NetworkManagerSecretAgent.cpp)
            target_include_directories(${MODULE_IMPL_NAME} PRIVATE ${GLIB_INCLUDE_DIRS} ${GIO_INCLUDE_DIRS} ${LIBNM_INCLUDE_DIRS})
            target_link_libraries(${MODULE_IMPL_NAME} PRIVATE ${GLIB_LIBRARIES} ${GIO_LIBRARIES} uuid)
    else()
        message("networkmanager building with libnm")
        target_sources(${MODULE_IMPL_NAME} PRIVATE
            gnome/NetworkManagerGnomeProxy.cpp
            gnome/NetworkManagerGnomeWIFI.cpp
            gnome/NetworkManagerGnomeEvents.cpp
            gnome/NetworkManagerGnomeUtils.cpp
            NetworkManagerSecretAgent.cpp )
        target_include_directories(${MODULE_IMPL_NAME} PRIVATE ${GLIB_INCLUDE_DIRS} ${LIBNM_INCLUDE_DIRS})
        target_link_libraries(${MODULE_IMPL_NAME} PRIVATE ${LIBNM_LIBRARIES})
    endif()
else()
    message("networkmanager building with RDK netsrvmgr")
    target_sources(${MODULE_IMPL_NAME} PRIVATE rdk/NetworkManagerRDKProxy.cpp)
    target_include_directories(${MODULE_IMPL_NAME} PRIVATE ${IARMBUS_INCLUDE_DIRS})
    target_link_libraries(${MODULE_IMPL_NAME} PRIVATE ${IARMBUS_LIBRARIES})
endif()

set_target_properties(${MODULE_IMPL_NAME} PROPERTIES
                                          CXX_STANDARD 11
                                          CXX_STANDARD_REQUIRED YES
                                          FRAMEWORK FALSE)

target_link_libraries(${MODULE_IMPL_NAME}  PRIVATE
                                        ${NAMESPACE}Core::${NAMESPACE}Core
                                        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
                                        )

target_link_libraries(${MODULE_IMPL_NAME} PRIVATE ${CURL_LIBRARIES})
target_include_directories(${MODULE_IMPL_NAME} PRIVATE ${CURL_INCLUDE_DIRS})

if (USE_RDK_LOGGER)
    target_link_libraries(${MODULE_NAME} PRIVATE ${RDKLOGGER_LIBRARIES})
    target_link_libraries(${MODULE_IMPL_NAME} PRIVATE ${RDKLOGGER_LIBRARIES})
endif (USE_RDK_LOGGER)

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${STORAGE_DIRECTORY}/plugins)
install(TARGETS ${MODULE_IMPL_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${STORAGE_DIRECTORY}/plugins)

#Generate Plugin configuration file
write_config()
