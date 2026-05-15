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
cmake_minimum_required(VERSION 3.3)

set(MODULE_NAME nflog_listener)
set(CHECK_NEIGHBOR_MODULE_NAME check_neighbor)
find_package(PkgConfig REQUIRED)

pkg_check_modules(LIBNETFILTER_LOG REQUIRED libnetfilter_log)
pkg_check_modules(LIBNFNETLINK     REQUIRED libnfnetlink)

add_executable(${MODULE_NAME} src/nflog.c)
add_executable(${CHECK_NEIGHBOR_MODULE_NAME} src/check_neighbor.c)

target_include_directories(${MODULE_NAME} PRIVATE
    ${LIBNETFILTER_LOG_INCLUDE_DIRS}
    ${LIBNFNETLINK_INCLUDE_DIRS}
)

target_link_libraries(${MODULE_NAME} PRIVATE
    ${LIBNETFILTER_LOG_LIBRARIES}
    ${LIBNFNETLINK_LIBRARIES}
)

install(TARGETS ${MODULE_NAME} ${CHECK_NEIGHBOR_MODULE_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
