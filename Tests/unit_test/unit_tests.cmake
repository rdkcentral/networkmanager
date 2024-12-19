cmake_minimum_required(VERSION 3.10)
set(UNIT_TEST "unit_tests")

find_package(PkgConfig REQUIRED)

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)

FetchContent_MakeAvailable(googletest)
add_executable(${UNIT_TEST}
  Tests/unit_test/test_WiFiSignalStrengthMonitor.cpp
  Tests/unit_test/test_NetworkManagerConnectivity.cpp
  Tests/unit_test/test_NetworkManagerStunClient.cpp
  Tests/unit_test/test_LegacyPlugin_WiFiManagerAPIs.cpp
  Tests/unit_test/test_LegacyPlugin_NetworkAPIs.cpp
  Tests/mocks/thunder/Module.cpp
  WiFiSignalStrengthMonitor.cpp
  NetworkManagerLogger.cpp
  NetworkManagerConnectivity.cpp
  NetworkManagerStunClient.cpp
  LegacyPlugin_WiFiManagerAPIs.cpp
  LegacyPlugin_NetworkAPIs.cpp
)
set_target_properties(${UNIT_TEST} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
)
target_compile_options(${UNIT_TEST} PRIVATE -Wall -include ${CMAKE_SOURCE_DIR}/INetworkManager.h)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")

target_include_directories(${UNIT_TEST} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${gtest_SOURCE_DIR}/include  
    ${gtest_SOURCE_DIR}/../googlemock/include
    Tests
    Tests/mocks
    Tests/mocks/thunder
)
target_link_libraries(${UNIT_TEST} PRIVATE 
      gmock_main 
      ${NAMESPACE}Core::${NAMESPACE}Core
      ${NAMESPACE}Plugins::${NAMESPACE}Plugins
      ${NAMESPACE}Core::${NAMESPACE}Core 
      ${CURL_LIBRARIES}
      )

install(TARGETS ${UNIT_TEST} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

