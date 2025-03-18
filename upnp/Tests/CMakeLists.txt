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
find_package(PkgConfig REQUIRED)

set(MODULE_NAME testrouterDiscovery)

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)
FetchContent_MakeAvailable(googletest)

set(TEST_SRC_FILES
  test_UpnpDiscoveryManager.cpp
)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")

# Create the test executable
add_executable(${MODULE_NAME} ${TEST_SRC_FILES})

# Include directories for the test executable
target_include_directories(${MODULE_NAME} PRIVATE ${PROJECT_SOURCE_DIR} ${GUPNP_INCLUDE_DIRS})

# Link the test executable with necessary libraries
target_link_libraries(${MODULE_NAME} PRIVATE
    gtest
    gtest_main
    gmock
    glib-2.0
    routerDiscovery-lib
)
# Add test to CTest
add_test(NAME ${MODULE_NAME} COMMAND runTests)
install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
