cmake_minimum_required(VERSION 3.10)
set(UNIT_TEST "tests")

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
add_executable(${UNIT_TEST}
  Tests/unit_test/test_WiFiSignalStrengthMonitor.cpp
  WiFiSignalStrengthMonitor.cpp
  NetworkManagerLogger.cpp
  NetworkManagerConnectivity.cpp
)
set_target_properties(${UNIT_TEST} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
)
target_compile_options(${UNIT_TEST} PRIVATE -Wall -include ${CMAKE_SOURCE_DIR}/INetworkManager.h)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")

target_include_directories(${UNIT_TEST} PRIVATE
    ${GLIB_INCLUDE_DIRS}
    ${LIBNM_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}  
    ${CMAKE_CURRENT_SOURCE_DIR}
    Tests
    ${gtest_SOURCE_DIR}/include  
    ${gtest_SOURCE_DIR}/../googlemock/include
)

target_link_libraries(${UNIT_TEST} PRIVATE gmock_main ${NAMESPACE}Core::${NAMESPACE}Core ${GLIB_LIBRARIES} ${GIO_LIBRARIES} ${LIBNM_LIBRARIES} ${CURL_LIBRARIES})
target_include_directories(${UNIT_TEST} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}) 
install(TARGETS ${UNIT_TEST} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

