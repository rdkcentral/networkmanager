# ============================================================================
# FetchContent setup
# ============================================================================

set(THUNDER_VERSION "R5.1" CACHE STRING "Thunder git tag/branch/commit to use")

include(FetchContent)

# ============================================================================
# 1. ThunderTools (source only — needed BEFORE Thunder for generators + cmake)
# ============================================================================

FetchContent_Declare(ThunderTools
    GIT_REPOSITORY https://github.com/rdkcentral/ThunderTools.git
    GIT_TAG ${THUNDER_VERSION}
)
FetchContent_Populate(ThunderTools)

# Point the generators at the fetched source tree
set(PROXYSTUB_GENERATOR "${thundertools_SOURCE_DIR}/ProxyStubGenerator/StubGenerator.py" CACHE STRING "" FORCE)
set(JSON_GENERATOR "${thundertools_SOURCE_DIR}/JsonGenerator/JsonGenerator.py" CACHE STRING "" FORCE)

# The .cmake.in templates use @ONLY substitution for these options (default OFF)
set(PROXYSTUB_GENERATOR_ENABLE_SECURITY  OFF)
set(PROXYSTUB_GENERATOR_ENABLE_COHERENCY OFF)
set(JSON_GENERATOR_ENABLE_STATS          OFF)

# Generate the Find*.cmake modules from templates so find_package() works
configure_file("${thundertools_SOURCE_DIR}/cmake/FindProxyStubGenerator.cmake.in"
               "${thundertools_BINARY_DIR}/FindProxyStubGenerator.cmake" @ONLY)
configure_file("${thundertools_SOURCE_DIR}/cmake/FindJsonGenerator.cmake.in"
               "${thundertools_BINARY_DIR}/FindJsonGenerator.cmake" @ONLY)
configure_file("${thundertools_SOURCE_DIR}/cmake/FindConfigGenerator.cmake.in"
               "${thundertools_BINARY_DIR}/FindConfigGenerator.cmake" @ONLY)

# ============================================================================
# 2. Thunder (built as static libraries for unit testing)
# ============================================================================

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

# Populate Thunder source WITHOUT building — we need to set CMAKE_MODULE_PATH
# first so that Thunder's CMakeLists.txt can find platform, CmakeHelperFunctions,
# and ProxyStubGenerator when it runs.
FetchContent_Populate(Thunder)

set(CMAKE_MODULE_PATH
    "${thundertools_BINARY_DIR}"
    "${thunder_SOURCE_DIR}/cmake"
    "${thunder_SOURCE_DIR}/cmake/common"
    "${thunder_SOURCE_DIR}/cmake/modules"
    ${CMAKE_MODULE_PATH}
    CACHE PATH "" FORCE
)

# Now build Thunder (its CMakeLists.txt will find everything it needs)
add_subdirectory(${thunder_SOURCE_DIR} ${thunder_BINARY_DIR})

# GoogleTest is fetched in the root CMakeLists.txt when ENABLE_UNIT_TESTING is ON