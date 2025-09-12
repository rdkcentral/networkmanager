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
message ("building l1 test")

find_package(CURL)
find_package(PkgConfig REQUIRED)
find_package(${NAMESPACE}Core REQUIRED)
find_package(${NAMESPACE}Plugins REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/interface)
include_directories(${PROJECT_SOURCE_DIR}/plugin/rdk)
include_directories(${PROJECT_SOURCE_DIR}/plugin)

if(ENABLE_ROUTER_DISCOVERY_TOOL)
    set(NM_ROUTER_DISCOVERY_L1_TEST "upnpcli_l1_test")

    add_executable(${NM_ROUTER_DISCOVERY_L1_TEST}
        ${CMAKE_SOURCE_DIR}/tests/l1Test/l1_test_routediscovery.cpp
        ${CMAKE_SOURCE_DIR}/tools/upnp/UpnpDiscoveryManager.cpp
    )

    target_link_libraries(${NM_ROUTER_DISCOVERY_L1_TEST} PRIVATE
        gmock_main
        ${GLIB_LIBRARIES}
        ${LIBSOUP_LIBRARIES} 
        ${GUPNP_LIBRARIES} 
        ${GSSDP_LIBRARIES}
    )

    set_target_properties(${NM_ROUTER_DISCOVERY_L1_TEST} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES
    )

    target_include_directories(${NM_ROUTER_DISCOVERY_L1_TEST} PRIVATE
        ${gtest_SOURCE_DIR}/include  
        ${gtest_SOURCE_DIR}/../googlemock/include
        ${PROJECT_SOURCE_DIR}/tools/upnp
        ${GUPNP_INCLUDE_DIRS}
        ${GLIB_INCLUDE_DIRS} 
        ${LIBSOUP_INCLUDE_DIRS}
        ${GSSDP_INCLUDE_DIRS}
    )

    install(TARGETS ${NM_ROUTER_DISCOVERY_L1_TEST} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
endif()

set(NM_CLASS_L1_TEST "class_l1_test")

add_executable(${NM_CLASS_L1_TEST}
    ${CMAKE_SOURCE_DIR}/tests/l1Test/l1_test_stunclient.cpp
    ${CMAKE_SOURCE_DIR}/tests/l1Test/l1_test_connectivity.cpp
    ${CMAKE_SOURCE_DIR}/plugin/NetworkManagerLogger.cpp
    ${CMAKE_SOURCE_DIR}/plugin/NetworkManagerConnectivity.cpp
    ${CMAKE_SOURCE_DIR}/plugin/NetworkManagerStunClient.cpp
)

target_link_libraries(${NM_CLASS_L1_TEST} PRIVATE
    gmock_main
    ${CURL_LIBRARIES}
    ${NAMESPACE}Core::${NAMESPACE}Core
)

set_target_properties(${NM_CLASS_L1_TEST} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
)

target_include_directories(${NM_CLASS_L1_TEST} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${gtest_SOURCE_DIR}/include  
    ${gtest_SOURCE_DIR}/../googlemock/include
)

install(TARGETS ${NM_CLASS_L1_TEST} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
