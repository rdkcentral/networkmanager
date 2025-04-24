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
message("Building plugin CLI tools")

# Required packages
find_package(PkgConfig REQUIRED)
find_package(${NAMESPACE}Core REQUIRED)

pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GIO REQUIRED gio-2.0)
pkg_check_modules(LIBNM REQUIRED libnm)

set(COMMON_COMPILE_OPTIONS
    -g
    -Wall
    -Wno-deprecated-declarations
    -include ${CMAKE_SOURCE_DIR}/interface/INetworkManager.h
)

set(COMMON_SOURCES
    ${CMAKE_SOURCE_DIR}/plugin/NetworkManagerLogger.cpp
    ${CMAKE_SOURCE_DIR}/plugin/NetworkManagerSecretAgent.cpp
    ${CMAKE_SOURCE_DIR}/plugin/gnome/NetworkManagerGnomeUtils.cpp
)

set(COMMON_INCLUDES
    ${GLIB_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
    ${LIBNM_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/plugin
    ${CMAKE_SOURCE_DIR}/interface
    ${CMAKE_SOURCE_DIR}/plugin/gnome
    ${CMAKE_SOURCE_DIR}/plugin/gnome/gdbus
)

set(COMMON_LIBS
    ${NAMESPACE}Core::${NAMESPACE}Core
    ${GLIB_LIBRARIES}
    ${GIO_LIBRARIES}
)

if(ENABLE_GNOME_GDBUS)

    message("Building plugin GDBUS CLI tool...")

    set(PLUGIN_GDBUS_CLI "nm-cli-gdbus")
    set(GDBUS_SOURCES
        ${CMAKE_SOURCE_DIR}/plugin/gnome/gdbus/NetworkManagerGdbusClient.cpp
        ${CMAKE_SOURCE_DIR}/plugin/gnome/gdbus/NetworkManagerGdbusMgr.cpp
        ${CMAKE_SOURCE_DIR}/plugin/gnome/gdbus/NetworkManagerGdbusUtils.cpp
        ${CMAKE_SOURCE_DIR}/plugin/gnome/gdbus/NetworkManagerGdbusEvent.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/NetworkManagerGdbusTest.cpp
    )

    add_executable(${PLUGIN_GDBUS_CLI}
        ${COMMON_SOURCES}
        ${GDBUS_SOURCES}
    )

    set_target_properties(${PLUGIN_GDBUS_CLI} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES
    )

    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/NetworkManagerGdbusTest.cpp
        PROPERTIES COMPILE_FLAGS "-w"
    )

    target_compile_options(${PLUGIN_GDBUS_CLI} PRIVATE ${COMMON_COMPILE_OPTIONS})
    target_include_directories(${PLUGIN_GDBUS_CLI} PRIVATE ${COMMON_INCLUDES})
    target_link_libraries(${PLUGIN_GDBUS_CLI} PRIVATE ${COMMON_LIBS} uuid)

    install(TARGETS ${PLUGIN_GDBUS_CLI} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

else()

    message("Building plugin Libnm CLI tool...")

    set(PLUGIN_LIBNM_CLI "nm-cli-libnm")
    set(LIBNM_SOURCES
        ${CMAKE_SOURCE_DIR}/plugin/gnome/NetworkManagerGnomeEvents.cpp
        ${CMAKE_SOURCE_DIR}/plugin/gnome/NetworkManagerGnomeWIFI.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/NetworkManagerLibnmTest.cpp
    )

    add_executable(${PLUGIN_LIBNM_CLI}
        ${COMMON_SOURCES}
        ${LIBNM_SOURCES}
    )

    set_target_properties(${PLUGIN_LIBNM_CLI} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES
    )

    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/NetworkManagerLibnmTest.cpp
        PROPERTIES COMPILE_FLAGS "-w"
    )

    target_compile_options(${PLUGIN_LIBNM_CLI} PRIVATE ${COMMON_COMPILE_OPTIONS})
    target_include_directories(${PLUGIN_LIBNM_CLI} PRIVATE ${COMMON_INCLUDES})
    target_link_libraries(${PLUGIN_LIBNM_CLI} PRIVATE ${COMMON_LIBS} ${LIBNM_LIBRARIES})

    install(TARGETS ${PLUGIN_LIBNM_CLI} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

endif()
