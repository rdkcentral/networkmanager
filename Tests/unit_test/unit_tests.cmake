#############################################################################
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2023 RDK Management
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
set(UNIT_TEST "unit_tests")

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
  Tests/unit_test/test_NetworkManagerConnectivity.cpp
  Tests/unit_test/test_NetworkManagerStunClient.cpp
  NetworkManagerLogger.cpp
  NetworkManagerConnectivity.cpp
  NetworkManagerStunClient.cpp
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
