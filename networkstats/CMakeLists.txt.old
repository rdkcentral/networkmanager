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
message("Building NetworkConnectionStats")

# Required packages
find_package(PkgConfig REQUIRED)

# Find JSON-RPC CPP library
pkg_check_modules(JSONRPCCPP REQUIRED libjsonrpccpp-client libjsonrpccpp-common)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

if (USE_TELEMETRY)
    find_package(T2 REQUIRED)
    add_definitions(-DUSE_TELEMETRY)
    include_directories(${TELEMETRY_INCLUDE_DIRS})
endif (USE_TELEMETRY)

set(MODULE_NAME networkconnectionstats)

set(STATS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/NetworkConnectionStats.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ThunderJsonRPCProvider.cpp
)

add_executable(${MODULE_NAME}
    ${STATS_SOURCES}
)

set_target_properties(${MODULE_NAME} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
)

# Do NOT define TEST_MAIN - NetworkConnectionStats.cpp has its own main()

target_compile_options(${MODULE_NAME} PRIVATE
    -g
    -Wall
    -Wno-deprecated-declarations
)

target_include_directories(${MODULE_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${JSONRPCCPP_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
)

target_link_libraries(${MODULE_NAME} PRIVATE
    ${JSONRPCCPP_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    pthread
)

if (USE_TELEMETRY)
    target_link_libraries(${MODULE_NAME} PRIVATE ${T2_LIBRARIES})
endif (USE_TELEMETRY)

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
