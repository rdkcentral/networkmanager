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
message ("building l2 test")

set(NM_RDK_PROXY_L2_TEST "rdkproxy_l2_test")
set(NM_LEGACY_WIFI_UT "legacywifi_tests")
set(NM_LEGACY_NETWORK_UT "legacynetwork_tests")

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

add_executable(${NM_RDK_PROXY_L2_TEST}
    ${CMAKE_SOURCE_DIR}/tests/l2Test/l2_test_rdkproxy.cpp
    ${CMAKE_SOURCE_DIR}/tests/l2Test/l2_test_rdkproxyEvent.cpp
    ${CMAKE_SOURCE_DIR}/tests/l2Test/l2_test_rdkproxyImpl.cpp
    ${CMAKE_SOURCE_DIR}/tests/mocks/thunder/Module.cpp
    ${CMAKE_SOURCE_DIR}/tests/mocks/Iarm.cpp
    ${CMAKE_SOURCE_DIR}/tests/mocks/Wraps.cpp
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
    ${CMAKE_SOURCE_DIR}/tests/l2Test/l2_test_LegacyPlugin_WiFiManagerAPIs.cpp
    ${CMAKE_SOURCE_DIR}/tests/mocks/thunder/Module.cpp
    ${CMAKE_SOURCE_DIR}/plugin/NetworkManagerLogger.cpp
    ${CMAKE_SOURCE_DIR}/legacy/LegacyWiFiManagerAPIs.cpp
)

add_executable(${NM_LEGACY_NETWORK_UT}
    ${CMAKE_SOURCE_DIR}/tests/l2Test/l2_test_LegacyPlugin_NetworkAPIs.cpp
    ${CMAKE_SOURCE_DIR}/tests/mocks/thunder/Module.cpp
    ${CMAKE_SOURCE_DIR}/plugin/NetworkManagerLogger.cpp
    ${CMAKE_SOURCE_DIR}/legacy/LegacyNetworkAPIs.cpp
)

set_target_properties(${NM_RDK_PROXY_L2_TEST} PROPERTIES
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

target_compile_options(${NM_RDK_PROXY_L2_TEST} PRIVATE -Wall -include ${CMAKE_SOURCE_DIR}/interface/INetworkManager.h)
target_compile_options(${NM_LEGACY_WIFI_UT} PRIVATE -Wall -include ${CMAKE_SOURCE_DIR}/interface/INetworkManager.h)
target_compile_options(${NM_LEGACY_NETWORK_UT} PRIVATE -Wall -include ${CMAKE_SOURCE_DIR}/interface/INetworkManager.h)

include_directories(${PROJECT_SOURCE_DIR}/interface)
include_directories(${PROJECT_SOURCE_DIR}/plugin/rdk)
include_directories(${PROJECT_SOURCE_DIR}/plugin)
include_directories(${PROJECT_SOURCE_DIR}/legacy)
include_directories(${PROJECT_SOURCE_DIR}/tests/mocks)
include_directories(${PROJECT_SOURCE_DIR}/tests/mocks/thunder)
include_directories(${PROJECT_SOURCE_DIR}/../install/usr/include/rdk/iarmbus/)
include_directories(${PROJECT_SOURCE_DIR}/tools/upnp/)

target_include_directories(${NM_RDK_PROXY_L2_TEST} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${gtest_SOURCE_DIR}/include
    ${gtest_SOURCE_DIR}/../googlemock/include
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

target_link_options(${NM_RDK_PROXY_L2_TEST} PRIVATE -Wl,-wrap,system -Wl,-wrap,popen -Wl,-wrap,syslog -Wl,-wrap,pclose -Wl,-wrap,getmntent -Wl,-wrap,setmntent -Wl,-wrap,v_secure_popen -Wl,-wrap,v_secure_pclose -Wl,-wrap,v_secure_system)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")

target_link_libraries(${NM_RDK_PROXY_L2_TEST} PRIVATE
    gmock_main
    ${NAMESPACE}Core::${NAMESPACE}Core
    ${NAMESPACE}Plugins::${NAMESPACE}Plugins
    ${CURL_LIBRARIES}
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

install(TARGETS ${NM_RDK_PROXY_L2_TEST} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(TARGETS ${NM_LEGACY_WIFI_UT} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(TARGETS ${NM_LEGACY_NETWORK_UT} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
