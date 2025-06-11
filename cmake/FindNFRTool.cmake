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

# - Try to find IARMBus
# Once done this will define
#  NFRTOOL_FOUND - System has NFRTool
#  NFRTOOL_INCLUDE_DIRS - The NFRTool include directories
#  NFRTOOL_LIBRARIES - The libraries needed to use NFRTool
#  NFRTOOL_FLAGS - The flags needed to use NFRTool
#

find_package(PkgConfig)

find_library(NFRTOOL_LIBRARIES NAMES nfrtoolCore)
find_path(NFRTOOL_INCLUDE_DIRS NAMES NFRAPI.h PATH_SUFFIXES nfr)

set(NFRTOOL_LIBRARIES ${NFRTOOL_LIBRARIES} CACHE PATH "Path to NFRTool library")
set(NFRTOOL_INCLUDE_DIRS ${NFRTOOL_INCLUDE_DIRS})
set(NFRTOOL_INCLUDE_DIRS ${NFRTOOL_INCLUDE_DIRS} CACHE PATH "Path to NFRTool include")

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(NFRTOOL DEFAULT_MSG NFRTOOL_INCLUDE_DIRS NFRTOOL_LIBRARIES)

mark_as_advanced(
    NFRTOOL_FOUND
    NFRTOOL_INCLUDE_DIRS
    NFRTOOL_LIBRARIES
    NFRTOOL_LIBRARY_DIRS
    NFRTOOL_FLAGS)
