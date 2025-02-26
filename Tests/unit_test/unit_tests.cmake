cmake_minimum_required(VERSION 3.10)

set(NM_CLASS_UT "common_class_tests")
set(NM_RDKPROXY_UT "rdkproxy_tests")
set(NM_LEGACY_WIFI_UT "legacywifi_tests")
set(NM_LEGACY_NETWORK_UT "legacynetwork_tests")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GIO REQUIRED gio-2.0)
pkg_check_modules(LIBNM REQUIRED libnm)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)

FetchContent_MakeAvailable(googletest)

add_executable(${NM_CLASS_UT}
    Tests/unit_test/test_NetworkManagerConnectivity.cpp
    Tests/unit_test/test_NetworkManagerStunClient.cpp
    NetworkManagerLogger.cpp
    NetworkManagerConnectivity.cpp
    NetworkManagerStunClient.cpp
)

add_executable(${NM_RDKPROXY_UT}
    Tests/unit_test/test_NetworkManager.cpp
    Tests/mocks/thunder/Module.cpp
    Tests/mocks/Iarm.cpp
    NetworkManager.cpp
    NetworkManagerLogger.cpp
    NetworkManagerJsonRpc.cpp
    NetworkManagerImplementation.cpp
    NetworkManagerConnectivity.cpp
    NetworkManagerStunClient.cpp
    NetworkManagerRDKProxy.cpp
    ${PROXY_STUB_SOURCES}
)

add_executable(${NM_LEGACY_WIFI_UT}
  Tests/unit_test/test_LegacyPlugin_WiFiManagerAPIs.cpp
  Tests/mocks/thunder/Module.cpp
  NetworkManagerLogger.cpp
  LegacyPlugin_WiFiManagerAPIs.cpp
)

add_executable(${NM_LEGACY_NETWORK_UT}
  Tests/unit_test/test_LegacyPlugin_NetworkAPIs.cpp
  Tests/mocks/thunder/Module.cpp
  NetworkManagerLogger.cpp
  LegacyPlugin_NetworkAPIs.cpp
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

target_compile_options(${NM_CLASS_UT} PRIVATE -Wall -include ${CMAKE_SOURCE_DIR}/INetworkManager.h)
target_compile_options(${NM_RDKPROXY_UT} PRIVATE -Wall -include ${CMAKE_SOURCE_DIR}/INetworkManager.h)
target_compile_options(${NM_LEGACY_WIFI_UT} PRIVATE -Wall -include ${CMAKE_SOURCE_DIR}/INetworkManager.h)
target_compile_options(${NM_LEGACY_NETWORK_UT} PRIVATE -Wall -include ${CMAKE_SOURCE_DIR}/INetworkManager.h)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")

target_include_directories(${NM_CLASS_UT} PRIVATE
    ${GLIB_INCLUDE_DIRS}
    ${LIBNM_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    Tests
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
    Tests
    Tests/mocks
    Tests/mocks/thunder
)
target_include_directories(${NM_LEGACY_WIFI_UT} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${gtest_SOURCE_DIR}/include  
    ${gtest_SOURCE_DIR}/../googlemock/include
    Tests
    Tests/mocks
    Tests/mocks/thunder
)

target_include_directories(${NM_LEGACY_NETWORK_UT} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${gtest_SOURCE_DIR}/include  
    ${gtest_SOURCE_DIR}/../googlemock/include
    Tests
    Tests/mocks
    Tests/mocks/thunder
)

target_link_libraries(${NM_CLASS_UT} PRIVATE
    gmock_main
    ${NAMESPACE}Core::${NAMESPACE}Core
    ${GLIB_LIBRARIES}
    ${GIO_LIBRARIES}
    ${LIBNM_LIBRARIES}
    ${CURL_LIBRARIES}
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

install(TARGETS ${NM_CLASS_UT} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(TARGETS ${NM_RDKPROXY_UT} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(TARGETS ${NM_LEGACY_WIFI_UT} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(TARGETS ${NM_LEGACY_NETWORK_UT} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
