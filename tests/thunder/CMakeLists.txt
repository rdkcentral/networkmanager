# ============================================================================
# FetchContent: Thunder (built as static libraries for unit testing)
# ============================================================================

set(THUNDER_VERSION "R5.1" CACHE STRING "Thunder git tag/branch/commit to use")

include(FetchContent)
FetchContent_Declare(Thunder
    GIT_REPOSITORY https://github.com/rdkcentral/Thunder.git
    GIT_TAG ${THUNDER_VERSION}
)

# Thunder as static libs — no executables, no daemons
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# Build only the components needed for plugin testing
set(CORE ON CACHE BOOL "" FORCE)
set(COM ON CACHE BOOL "" FORCE)
set(MESSAGING ON CACHE BOOL "" FORCE)
set(PLUGINS ON CACHE BOOL "" FORCE)
set(WEBSOCKET ON CACHE BOOL "" FORCE)
set(CRYPTALGO ON CACHE BOOL "" FORCE)

# Disable components not needed for testing
set(EXECUTABLE OFF CACHE BOOL "" FORCE)
set(PROCESS OFF CACHE BOOL "" FORCE)
set(SECURE_SOCKET OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(Thunder)

# Make Thunder's cmake modules (CmakeHelperFunctions, generators, etc.)
# available to the rest of the build
set(CMAKE_MODULE_PATH
    "${thunder_SOURCE_DIR}/cmake/common"
    "${thunder_SOURCE_DIR}/cmake/modules"
    ${CMAKE_MODULE_PATH}
    CACHE PATH "" FORCE
)

# ============================================================================
# FetchContent: GoogleTest
# ============================================================================

set(GOOGLE_TEST_VERSION "v1.15.2" CACHE STRING "GoogleTest git tag/branch/commit to use")

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG ${GOOGLE_TEST_VERSION}
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_library(GTest::GTest ALIAS gtest_main)