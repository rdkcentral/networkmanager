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
message("Generate ProxyStub for INetworkManager.h")

find_package(${NAMESPACE}Core REQUIRED)
find_package(${NAMESPACE}Plugins REQUIRED)
find_package(CompileSettingsDebug CONFIG REQUIRED)
find_package(ProxyStubGenerator REQUIRED)

set(ProxyStubGenerator_DIR ${CMAKE_SYSROOT}${CMAKE_INSTALL_PREFIX}/tools/cmake ${ProxyStubGenerator_DIR})

if(NOT GENERATOR_SEARCH_PATH)
    set(GENERATOR_SEARCH_PATH ${CMAKE_SYSROOT}${CMAKE_INSTALL_PREFIX}/include/${NAMESPACE})
endif()

file(GLOB INTERFACE_HEADER ${CMAKE_CURRENT_SOURCE_DIR})

ProxyStubGenerator(INPUT "${INTERFACE_HEADER}" OUTDIR "${CMAKE_CURRENT_BINARY_DIR}/generated" INCLUDE_PATH ${GENERATOR_SEARCH_PATH})

install(FILES INetworkManager.h  DESTINATION include/${NAMESPACE}/interfaces)
