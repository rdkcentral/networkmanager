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

# ============================================================================
# FetchContent: ThunderTools (source only — for ProxyStubGenerator / JsonGenerator)
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

# Make Thunder's cmake modules (CmakeHelperFunctions, generators, etc.)
# available to the rest of the build
set(CMAKE_MODULE_PATH
    "${thundertools_BINARY_DIR}"
    "${thunder_SOURCE_DIR}/cmake/common"
    "${thunder_SOURCE_DIR}/cmake/modules"
    ${CMAKE_MODULE_PATH}
    CACHE PATH "" FORCE
)

# GoogleTest is fetched in the root CMakeLists.txt when ENABLE_UNIT_TESTING is ON