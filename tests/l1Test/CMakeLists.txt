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

set(NM_RDK_PROXY_L1_TEST "rdkproxy_l1_test")

find_package(CURL)
find_package(PkgConfig REQUIRED)
find_package(${NAMESPACE}Core REQUIRED)
find_package(${NAMESPACE}Plugins REQUIRED)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)

FetchContent_MakeAvailable(googletest)

add_executable(${NM_RDK_PROXY_L1_TEST}
    ${CMAKE_SOURCE_DIR}/tests/l1Test/l1_test_rdkproxy.cpp
    ${CMAKE_SOURCE_DIR}/tests/l1Test/l1_test_rdkproxyEvent.cpp
    ${CMAKE_SOURCE_DIR}/tests/l1Test/l1_test_rdkproxyImpl.cpp
    ${CMAKE_SOURCE_DIR}/tests/mocks/thunder/Module.cpp
    ${CMAKE_SOURCE_DIR}/tests/mocks/Iarm.cpp
    ${CMAKE_SOURCE_DIR}/plugin/NetworkManager.cpp
    ${CMAKE_SOURCE_DIR}/plugin/NetworkManagerLogger.cpp
    ${CMAKE_SOURCE_DIR}/plugin/NetworkManagerJsonRpc.cpp
    ${CMAKE_SOURCE_DIR}/plugin/NetworkManagerImplementation.cpp
    ${CMAKE_SOURCE_DIR}/plugin/NetworkManagerConnectivity.cpp
    ${CMAKE_SOURCE_DIR}/plugin/NetworkManagerStunClient.cpp
    ${CMAKE_SOURCE_DIR}/plugin/rdk/NetworkManagerRDKProxy.cpp
    ${PROXY_STUB_SOURCES}
)

set_target_properties(${NM_RDK_PROXY_L1_TEST} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
)

target_compile_options(${NM_RDK_PROXY_L1_TEST} PRIVATE -Wall -include ${CMAKE_SOURCE_DIR}/interface/INetworkManager.h)

include_directories(${PROJECT_SOURCE_DIR}/interface)
include_directories(${PROJECT_SOURCE_DIR}/plugin/rdk)
include_directories(${PROJECT_SOURCE_DIR}/plugin)
include_directories(${PROJECT_SOURCE_DIR}/tests/mocks)
include_directories(${PROJECT_SOURCE_DIR}/tests/mocks/thunder)
include_directories(${PROJECT_SOURCE_DIR}/../install/usr/include/rdk/iarmbus/)
include_directories(${PROJECT_SOURCE_DIR}/tools/upnp/)

target_include_directories(${NM_RDK_PROXY_L1_TEST} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${gtest_SOURCE_DIR}/include  
    ${gtest_SOURCE_DIR}/../googlemock/include
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")

target_link_libraries(${NM_RDK_PROXY_L1_TEST} PRIVATE
    gmock_main
    ${NAMESPACE}Core::${NAMESPACE}Core
    ${NAMESPACE}Plugins::${NAMESPACE}Plugins
    ${CURL_LIBRARIES}
)

install(TARGETS ${NM_RDK_PROXY_L1_TEST} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

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
