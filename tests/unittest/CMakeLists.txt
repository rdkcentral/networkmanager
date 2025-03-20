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
message ("Unittest binary is built here")

set(NM_CLASS_UT "common_class_tests")
set(NM_RDKPROXY_UT "rdkproxy_tests")
set(NM_LEGACY_WIFI_UT "legacywifi_tests")
set(NM_LEGACY_NETWORK_UT "legacynetwork_tests")
set(NM_ROUTER_DISCOVERY_UT "routerDiscovery_tests")

find_package(${NAMESPACE}Core REQUIRED)
find_package(${NAMESPACE}Plugins REQUIRED)
find_package(CURL)
find_package(PkgConfig REQUIRED)
#pkg_check_modules(GLIB REQUIRED glib-2.0)
#pkg_check_modules(GIO REQUIRED gio-2.0)
#pkg_check_modules(LIBNM REQUIRED libnm)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)

FetchContent_MakeAvailable(googletest)

add_executable(${NM_CLASS_UT}
    ${CMAKE_SOURCE_DIR}/tests/unittest/test_NetworkManagerConnectivity.cpp
    ${CMAKE_SOURCE_DIR}/tests/unittest/test_NetworkManagerStunClient.cpp
    ${CMAKE_SOURCE_DIR}/plugin/NetworkManagerLogger.cpp
    ${CMAKE_SOURCE_DIR}/plugin/NetworkManagerConnectivity.cpp
    ${CMAKE_SOURCE_DIR}/plugin/NetworkManagerStunClient.cpp
)

add_executable(${NM_RDKPROXY_UT}
    ${CMAKE_SOURCE_DIR}/tests/unittest/test_NetworkManager.cpp
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

add_executable(${NM_LEGACY_WIFI_UT}
    ${CMAKE_SOURCE_DIR}/tests/unittest/test_LegacyPlugin_WiFiManagerAPIs.cpp
    ${CMAKE_SOURCE_DIR}/tests/mocks/thunder/Module.cpp
    ${CMAKE_SOURCE_DIR}/plugin/NetworkManagerLogger.cpp
    ${CMAKE_SOURCE_DIR}/legacy/LegacyWiFiManagerAPIs.cpp
)

add_executable(${NM_LEGACY_NETWORK_UT}
    ${CMAKE_SOURCE_DIR}/tests/unittest/test_LegacyPlugin_NetworkAPIs.cpp
    ${CMAKE_SOURCE_DIR}/tests/mocks/thunder/Module.cpp
    ${CMAKE_SOURCE_DIR}/plugin/NetworkManagerLogger.cpp
    ${CMAKE_SOURCE_DIR}/legacy/LegacyNetworkAPIs.cpp
)

add_executable(${NM_ROUTER_DISCOVERY_UT}
    ${CMAKE_SOURCE_DIR}/tests/unittest/test_UpnpDiscoveryManager.cpp
)

set_target_properties(${NM_CLASS_UT} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
)

set_target_properties(${NM_RDKPROXY_UT} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
)

set_target_properties(${NM_LEGACY_WIFI_UT} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
)
set_target_properties(${NM_LEGACY_NETWORK_UT} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
)
set_target_properties(${NM_ROUTER_DISCOVERY_UT} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
)
target_compile_options(${NM_CLASS_UT} PRIVATE -Wall -include ${CMAKE_SOURCE_DIR}/interface/INetworkManager.h)
target_compile_options(${NM_RDKPROXY_UT} PRIVATE -Wall -include ${CMAKE_SOURCE_DIR}/interface/INetworkManager.h)
target_compile_options(${NM_LEGACY_WIFI_UT} PRIVATE -Wall -include ${CMAKE_SOURCE_DIR}/interface/INetworkManager.h)
target_compile_options(${NM_LEGACY_NETWORK_UT} PRIVATE -Wall -include ${CMAKE_SOURCE_DIR}/interface/INetworkManager.h)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")

include_directories(${PROJECT_SOURCE_DIR}/interface)
include_directories(${PROJECT_SOURCE_DIR}/plugin)
include_directories(${PROJECT_SOURCE_DIR}/legacy)
include_directories(${PROJECT_SOURCE_DIR}/tests/mocks)
include_directories(${PROJECT_SOURCE_DIR}/tests/mocks/thunder)

target_include_directories(${NM_CLASS_UT} PRIVATE
    ${GLIB_INCLUDE_DIRS}
    ${LIBNM_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${gtest_SOURCE_DIR}/include
    ${gtest_SOURCE_DIR}/../googlemock/include
)

target_include_directories(${NM_RDKPROXY_UT} PRIVATE
    ${CURL_INCLUDE_DIRS}
    ${IARMBUS_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}
    ${gtest_SOURCE_DIR}/include
    ${gtest_SOURCE_DIR}/../googlemock/include
    ${CMAKE_CURRENT_SOURCE_DIR}/install/usr/include/rdk/iarmbus
)
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

target_link_libraries(${NM_CLASS_UT} PRIVATE
    gmock_main
    ${NAMESPACE}Core::${NAMESPACE}Core
    ${GLIB_LIBRARIES}
    ${GIO_LIBRARIES}
    ${LIBNM_LIBRARIES}
    ${CURL_LIBRARIES}
)
target_include_directories(${NM_ROUTER_DISCOVERY_UT} PRIVATE
    ${PROJECT_SOURCE_DIR}/tools/upnp
    ${GUPNP_INCLUDE_DIRS}
)

target_link_libraries(${NM_RDKPROXY_UT} PRIVATE
    gmock_main
    ${NAMESPACE}Core::${NAMESPACE}Core
    ${NAMESPACE}Plugins::${NAMESPACE}Plugins
    ${CURL_LIBRARIES}
    ${IARMBUS_LIBRARIES}
)
target_link_libraries(${NM_LEGACY_WIFI_UT} PRIVATE
     gmock_main 
      ${NAMESPACE}Core::${NAMESPACE}Core
      ${NAMESPACE}Plugins::${NAMESPACE}Plugins
      ${NAMESPACE}Core::${NAMESPACE}Core 
      ${CURL_LIBRARIES}
)
target_link_libraries(${NM_LEGACY_NETWORK_UT} PRIVATE
     gmock_main 
      ${NAMESPACE}Core::${NAMESPACE}Core
      ${NAMESPACE}Plugins::${NAMESPACE}Plugins
      ${NAMESPACE}Core::${NAMESPACE}Core 
      ${CURL_LIBRARIES}
)

link_directories(${PROJECT_SOURCE_DIR}/../build/RDKNetworkmanager)

target_link_libraries(${NM_ROUTER_DISCOVERY_UT} PRIVATE
      gmock_main
      ${GLIB_LIBRARIES}
      ${ROUTER_DISCOVERY_LIB}
)

install(TARGETS ${NM_CLASS_UT} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(TARGETS ${NM_RDKPROXY_UT} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(TARGETS ${NM_LEGACY_WIFI_UT} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(TARGETS ${NM_LEGACY_NETWORK_UT} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
