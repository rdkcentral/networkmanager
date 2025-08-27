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
message ("building l2 legacy plugin test")

set(NM_LEGACY_WIFI_UT "legacywifi_tests")
set(NM_LEGACY_NETWORK_UT "legacynetwork_tests")

find_package(PkgConfig REQUIRED)
find_package(${NAMESPACE}Core REQUIRED)
find_package(${NAMESPACE}Plugins REQUIRED)

add_executable(${NM_LEGACY_WIFI_UT}
    ${CMAKE_SOURCE_DIR}/tests/l2Test/legacy/l2_test_LegacyPlugin_WiFiManagerAPIs.cpp
    ${CMAKE_SOURCE_DIR}/tests/mocks/thunder/Module.cpp
    ${CMAKE_SOURCE_DIR}/plugin/NetworkManagerLogger.cpp
    ${CMAKE_SOURCE_DIR}/legacy/LegacyWiFiManagerAPIs.cpp
)

add_executable(${NM_LEGACY_NETWORK_UT}
    ${CMAKE_SOURCE_DIR}/tests/l2Test/legacy/l2_test_LegacyPlugin_NetworkAPIs.cpp
    ${CMAKE_SOURCE_DIR}/tests/mocks/thunder/Module.cpp
    ${CMAKE_SOURCE_DIR}/plugin/NetworkManagerLogger.cpp
    ${CMAKE_SOURCE_DIR}/legacy/LegacyNetworkAPIs.cpp
)

set_target_properties(${NM_LEGACY_WIFI_UT} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
)
set_target_properties(${NM_LEGACY_NETWORK_UT} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
)

target_compile_options(${NM_LEGACY_WIFI_UT} PRIVATE -Wall -include ${CMAKE_SOURCE_DIR}/interface/INetworkManager.h)
target_compile_options(${NM_LEGACY_NETWORK_UT} PRIVATE -Wall -include ${CMAKE_SOURCE_DIR}/interface/INetworkManager.h)

include_directories(${PROJECT_SOURCE_DIR}/interface)
include_directories(${PROJECT_SOURCE_DIR}/plugin)
include_directories(${PROJECT_SOURCE_DIR}/legacy)
include_directories(${PROJECT_SOURCE_DIR}/tests/mocks)
include_directories(${PROJECT_SOURCE_DIR}/tests/mocks/thunder)

target_include_directories(${NM_LEGACY_WIFI_UT} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${gtest_SOURCE_DIR}/include
    ${gtest_SOURCE_DIR}/../googlemock/include
)

target_include_directories(${NM_LEGACY_NETWORK_UT} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${gtest_SOURCE_DIR}/include
    ${gtest_SOURCE_DIR}/../googlemock/include
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")

target_link_libraries(${NM_LEGACY_WIFI_UT} PRIVATE
    gmock_main
    ${NAMESPACE}Core::${NAMESPACE}Core
    ${NAMESPACE}Plugins::${NAMESPACE}Plugins
    ${NAMESPACE}Core::${NAMESPACE}Core
)

target_link_libraries(${NM_LEGACY_NETWORK_UT} PRIVATE
    gmock_main
    ${NAMESPACE}Core::${NAMESPACE}Core
    ${NAMESPACE}Plugins::${NAMESPACE}Plugins
    ${NAMESPACE}Core::${NAMESPACE}Core
)

install(TARGETS ${NM_LEGACY_WIFI_UT} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(TARGETS ${NM_LEGACY_NETWORK_UT} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
