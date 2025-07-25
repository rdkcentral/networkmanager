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

find_package(PkgConfig)

find_library(T2_LIBRARIES NAMES telemetry_msgsender)
find_path(T2_INCLUDE_DIRS NAMES telemetry_busmessage_sender.h)

set(T2_LIBRARIES ${T2_LIBRARIES} CACHE PATH "Path to telemetry library")
set(T2_INCLUDE_DIRS ${T2_INCLUDE_DIRS} )
set(T2_INCLUDE_DIRS ${T2_INCLUDE_DIRS} CACHE PATH "Path to telemetry include")

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(T2 DEFAULT_MSG T2_INCLUDE_DIRS T2_LIBRARIES)

mark_as_advanced(
    T2_FOUND
    T2_INCLUDE_DIRS
    T2_LIBRARIES
    T2_LIBRARY_DIRS
    T2_FLAGS)
